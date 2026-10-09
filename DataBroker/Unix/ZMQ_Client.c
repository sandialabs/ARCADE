#include "ZMQ_Client.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "init_Server.h"
#include "utils.h"

static bool configured_inputs;
static bool permitted[MAX_IO];

static char *trim(char *tag)
{
    while (isspace((unsigned char)*tag)) {
        ++tag;
    }
    size_t len = strlen(tag);
    while (len && isspace((unsigned char)tag[len - 1])) {
        tag[--len] = '\0';
    }
    return tag;
}

static char *next_tag(char **remaining)
{
    char *tag = *remaining;
    if (!tag) {
        return NULL;
    }
    char *end = strchr(tag, ',');
    if (end) {
        *end = '\0';
        *remaining = end + 1;
    } else {
        *remaining = NULL;
    }
    return trim(tag);
}

int CoSim_ReserveInputs(int count)
{
    char *tags = Read_Vars("cosim", "inputs", "");
    if (!tags) {
        errno = ENOMEM;
        return -1;
    }
    configured_inputs = *tags != '\0';
    bool reserved[MAX_IO] = {false};
    bool valid = true;
    int found = 0;
    char *remaining = configured_inputs ? tags : NULL;
    pthread_mutex_lock(&DATA_Mutx);
    for (char *tag = next_tag(&remaining); tag; tag = next_tag(&remaining)) {
        int index = -1;
        for (int i = 0; i < count; ++i) {
            if (!strcmp(UP_DATA[i].Name, tag)) {
                index = i;
                break;
            }
        }
        if (index < 0 || reserved[index]) {
            fprintf(stderr, "Invalid co-simulation input: %s\n", tag);
            valid = false;
            break;
        }
        reserved[index] = true;
        ++found;
    }
    if (configured_inputs && !found) {
        valid = false;
    }
    if (valid) {
        for (int i = 0; i < count; ++i) {
            permitted[i] = !configured_inputs || reserved[i];
            CO_SIM_OWNED[i] = reserved[i];
        }
    }
    pthread_mutex_unlock(&DATA_Mutx);
    free(tags);
    if (!valid) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static cJSON *parse_reply(const void *data, size_t len)
{
    const char *text = data, *end = NULL;
    if (len && text[len - 1] == '\0') {
        --len;
    }
    cJSON *reply = cJSON_ParseWithLengthOpts(text, len, &end, false);
    size_t used = end ? (size_t)(end - text) : 0;
    while (used < len && isspace((unsigned char)text[used])) {
        ++used;
    }
    if (!cJSON_IsObject(reply) || used != len) {
        cJSON_Delete(reply);
        errno = EPROTO;
        return NULL;
    }
    return reply;
}

static int apply_reply(cJSON *reply, int count, double time)
{
    bool seen[MAX_IO] = {false};
    double values[MAX_IO];
    bool valid = true;
    int found = 0;
    cJSON *item;
    pthread_mutex_lock(&DATA_Mutx);
    cJSON_ArrayForEach(item, reply)
    {
        int index = -1;
        for (int i = 0; i < count; ++i) {
            if (permitted[i] && !strcmp(UP_DATA[i].Name, item->string)) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            fprintf(stderr, "Ignoring unknown co-simulation input: %s\n", item->string);
            continue;
        }
        if (seen[index] || !cJSON_IsNumber(item) || !isfinite(item->valuedouble)) {
            fprintf(stderr, "Invalid co-simulation value: %s\n", item->string);
            valid = false;
            break;
        }
        seen[index] = true;
        values[index] = item->valuedouble;
        ++found;
    }
    if (count && !found) {
        valid = false;
    }
    if (valid && !Sem_Stop()) {
        for (int i = 0; i < count; ++i) {
            if (seen[i]) {
                UP_DATA[i].Value = values[i];
                UP_DATA[i].Time = time;
                CO_SIM_OWNED[i] = true;
            }
        }
    }
    pthread_mutex_unlock(&DATA_Mutx);
    if (!valid) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

void *ZMQ_Client(void *_)
{
    int result = WAIT_ERROR;
    void *context = NULL, *socket = NULL;
    sem_t *trigger = SEM_FAILED, *done = SEM_FAILED;
    ZMQ_TagMap outputs[MAX_ZMQ_VARS];
    int count = 0;
    char *tags = Read_Vars("cosim", "outputs", "");
    char *address = Read_Vars("cosim", "address", "tcp://localhost:5556");
    if (!tags || !address) {
        errno = ENOMEM;
        goto cleanup;
    }
    char *remaining = *tags ? tags : NULL;
    for (char *tag = next_tag(&remaining); tag; tag = next_tag(&remaining)) {
        if (!*tag || count == MAX_ZMQ_VARS || strlen(tag) >= sizeof(outputs[count].tag)) {
            errno = EINVAL;
            goto cleanup;
        }
        for (int i = 0; i < count; ++i) {
            if (!strcmp(outputs[i].tag, tag)) {
                errno = EINVAL;
                goto cleanup;
            }
        }
        strcpy(outputs[count++].tag, tag);
    }
    trigger = sem_open("/co_sim", 0);
    done = sem_open("/co_sim_2", 0);
    if (trigger == SEM_FAILED || done == SEM_FAILED) {
        goto cleanup;
    }
    result = sem_wait_safe(trigger, 0);
    if (result != WAIT_OK) {
        goto cleanup;
    }
    pthread_mutex_lock(&FLAG_Mutx);
    Configs config = CONF;
    pthread_mutex_unlock(&FLAG_Mutx);
    result = WAIT_ERROR;
    bool missing = false;
    pthread_mutex_lock(&DATA_Mutx);
    for (int i = 0; i < count; ++i) {
        outputs[i].index = -1;
        for (int j = 0; j < config.PUB_N; ++j) {
            if (!strcmp(PUB_DATA[j].Name, outputs[i].tag)) {
                outputs[i].index = j;
                break;
            }
        }
        if (outputs[i].index < 0) {
            fprintf(stderr, "Unknown co-simulation output: %s\n", outputs[i].tag);
            missing = true;
        }
    }
    pthread_mutex_unlock(&DATA_Mutx);
    if (missing) {
        errno = EINVAL;
        goto cleanup;
    }
    context = zmq_ctx_new();
    if (!context) {
        goto cleanup;
    }
    socket = zmq_socket(context, ZMQ_REQ);
    if (!socket || zmq_configure(socket) || zmq_connect(socket, address)) {
        goto cleanup;
    }
    if (config.Co_Sim_Sync_Enable && sem_post(done)) {
        goto cleanup;
    }
    while (!Sem_Stop()) {
        result = sem_wait_safe(trigger, 0);
        if (result != WAIT_OK) {
            goto cleanup;
        }
        if (!config.Co_Sim_Sync_Enable) {
            int rc;
            do {
                rc = sem_trywait(trigger);
            } while (rc == 0 || errno == EINTR);
            if (errno != EAGAIN) {
                result = WAIT_ERROR;
                goto cleanup;
            }
        }
        result = WAIT_ERROR;
        cJSON *root = cJSON_CreateObject();
        if (!root) {
            errno = ENOMEM;
            goto cleanup;
        }
        bool allocated = true, finite = true;
        pthread_mutex_lock(&DATA_Mutx);
        double time = PUB_TIME;
        for (int i = 0; i < count; ++i) {
            double value = PUB_DATA[outputs[i].index].Value;
            if (!isfinite(value)) {
                finite = false;
            } else if (!cJSON_AddNumberToObject(root, outputs[i].tag, value)) {
                allocated = false;
            }
        }
        pthread_mutex_unlock(&DATA_Mutx);
        char *request = allocated && finite ? cJSON_PrintUnformatted(root) : NULL;
        cJSON_Delete(root);
        if (!request) {
            errno = finite ? ENOMEM : EDOM;
            goto cleanup;
        }
        zmq_msg_t response;
        if (zmq_msg_init(&response)) {
            free(request);
            goto cleanup;
        }
        result = zmq_exchange(socket, request, &response, config.Exchange_Timeout);
        free(request);
        cJSON *reply = result == WAIT_OK ? parse_reply(zmq_msg_data(&response), zmq_msg_size(&response)) : NULL;
        int saved = errno;
        zmq_msg_close(&response);
        errno = saved;
        if (result != WAIT_OK) {
            goto cleanup;
        }
        result = WAIT_ERROR;
        if (!reply) {
            goto cleanup;
        }
        int rc = apply_reply(reply, config.UP_N, time);
        cJSON_Delete(reply);
        if (rc) {
            goto cleanup;
        }
        if (config.Co_Sim_Sync_Enable && sem_post(done)) {
            goto cleanup;
        }
    }
    result = WAIT_STOPPED;
cleanup:
    if (result == WAIT_ERROR) {
        perror("Co-simulation interface");
    }
    Set_Stop();
    free(tags);
    free(address);
    if (trigger != SEM_FAILED) {
        sem_close(trigger);
    }
    if (done != SEM_FAILED) {
        sem_close(done);
    }
    if (socket) {
        zmq_close(socket);
    }
    if (context) {
        zmq_ctx_destroy(context);
    }
    return result == WAIT_ERROR ? (void *)-1 : NULL;
}
