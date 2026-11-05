#include "init_Server.h"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "cJSON.h"
#include "utils.h"

static cJSON *snapshot;

static cJSON *field(const char *category, const char *name)
{
    cJSON *array = cJSON_GetObjectItem(snapshot, category);
    return cJSON_GetObjectItem(cJSON_GetArrayItem(array, 0), name);
}

static int invalid(const char *name)
{
    fprintf(stderr, "Invalid input.json: %s\n", name);
    errno = EINVAL;
    return -1;
}

static int timeout_field(const char *category, const char *name, int *value)
{
    cJSON *item = field(category, name);
    *value = 0;
    if (item) {
        if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
            item->valuedouble < 0 || item->valuedouble > INT_MAX ||
            item->valuedouble != (int)item->valuedouble) {
            return invalid(name);
        }
        *value = (int)item->valuedouble;
    }
    return 0;
}

int Load_Config(void)
{
    size_t len;
    char *data = OpenFile("input.json", &len);
    if (!data) {
        perror("Opening input.json");
        return -1;
    }
    const char *end = NULL;
    snapshot = cJSON_ParseWithLengthOpts(data, len, &end, false);
    size_t consumed = end ? (size_t)(end - data) : 0;
    if (snapshot) {
        while (consumed < len && (data[consumed] == ' ' || data[consumed] == '\t' ||
                                  data[consumed] == '\r' || data[consumed] == '\n')) {
            ++consumed;
        }
    }
    munmap(data, len);
    if (!cJSON_IsObject(snapshot) || consumed != len) {
        fprintf(stderr, "Invalid input.json near byte %zu\n", consumed);
        errno = EINVAL;
        return -1;
    }
    cJSON *sim = cJSON_GetObjectItem(snapshot, "simulator");
    cJSON *cosim = cJSON_GetObjectItem(snapshot, "cosim");
    if (!cJSON_IsArray(sim) || cJSON_GetArraySize(sim) != 1 ||
        !cJSON_IsObject(cJSON_GetArrayItem(sim, 0)) ||
        (cosim && (!cJSON_IsArray(cosim) || cJSON_GetArraySize(cosim) != 1 ||
                   !cJSON_IsObject(cJSON_GetArrayItem(cosim, 0))))) {
        return invalid("simulator/cosim must contain exactly one object");
    }
    if (field("simulator", "hold_for_dante")) {
        return invalid("hold_for_dante was renamed to hold. replace the old key");
    }
    cJSON *name = field("simulator", "executableName");
    if (!cJSON_IsString(name) || !name->valuestring[0]) {
        return invalid("simulator.executableName must be a nonempty string");
    }
    const char *categories[] = {"simulator", "simulator", "simulator", "cosim"};
    const char *flags[] = {"hold", "co_sim_enable", "realtime_timestep", "sync_enable"};
    for (size_t i = 0; i < 4; ++i) {
        cJSON *item = field(categories[i], flags[i]);
        if (item && !cJSON_IsBool(item) &&
            !(cJSON_IsString(item) && (!strcasecmp(item->valuestring, "true") ||
                                       !strcasecmp(item->valuestring, "false")))) {
            return invalid(flags[i]);
        }
    }
    const char *strings[] = {"address", "inputs", "outputs"};
    for (size_t i = 0; i < 3; ++i) {
        cJSON *item = field("cosim", strings[i]);
        if (item && (!cJSON_IsString(item) || (i == 0 && !item->valuestring[0]))) {
            return invalid(strings[i]);
        }
    }
    if (timeout_field("simulator", "endpoint_timeout_s", &CONF.Endpoint_Timeout) ||
        timeout_field("simulator", "publish_timeout_s", &CONF.Publish_Timeout) ||
        timeout_field("cosim", "exchange_timeout_s", &CONF.Exchange_Timeout)) {
        return -1;
    }
    FLAGS.Hold_Time_Flag = Read_flags("simulator", "hold", false);
    CONF.Co_Sim_Enable = Read_flags("simulator", "co_sim_enable", false);
    CONF.Co_Sim_Sync_Enable = Read_flags("cosim", "sync_enable", false);
    CONF.Realtime_Timestep = Read_flags("simulator", "realtime_timestep", false);

    cJSON *endpoints = cJSON_GetObjectItem(snapshot, "endpoints");
    if (endpoints && !cJSON_IsArray(endpoints)) {
        return invalid("endpoints must be an array");
    }
    cJSON *endpoint;
    cJSON_ArrayForEach(endpoint, endpoints)
    {
        cJSON *host = cJSON_GetObjectItem(endpoint, "IP_Host");
        if (!cJSON_IsObject(endpoint) || !cJSON_IsString(host) || !host->valuestring[0]) {
            return invalid("endpoints.IP_Host must be a nonempty string");
        }
        const char *groups[] = {"PLCS", "Server"};
        for (size_t i = 0; i < 2; ++i) {
            cJSON *group = cJSON_GetObjectItem(endpoint, groups[i]);
            if (group && !cJSON_IsArray(group)) {
                return invalid(groups[i]);
            }
            cJSON *item;
            cJSON_ArrayForEach(item, group)
            {
                if (!cJSON_IsObject(item)) {
                    return invalid("endpoint entries must be objects");
                }
            }
        }
    }
    return 0;
}

void Cleanup_Config(void)
{
    cJSON_Delete(snapshot);
    snapshot = NULL;
}

static int send_config(void *socket, const char *text, bool allow_skip)
{
    for (int attempt = 0; attempt < 5; ++attempt) {
        zmq_msg_t reply;
        if (zmq_msg_init(&reply)) {
            return -1;
        }
        int rc = zmq_exchange(socket, text, &reply, CONF.Endpoint_Timeout);
        if (rc != WAIT_OK) {
            int saved = errno;
            zmq_msg_close(&reply);
            errno = saved;
            return -1;
        }
        size_t n = zmq_msg_size(&reply);
        if (n && ((char *)zmq_msg_data(&reply))[n - 1] == '\0') {
            --n;
        }
        bool valid = n == 5 && memcmp(zmq_msg_data(&reply), "VALID", 5) == 0;
        bool skip = allow_skip && n == 4 && memcmp(zmq_msg_data(&reply), "SKIP", 4) == 0;
        zmq_msg_close(&reply);
        if (valid || skip) {
            return 0;
        }
    }
    errno = EPROTO;
    return -1;
}

int init_Server(void)
{
    int result = -1;
    void *context = zmq_ctx_new(), *requester = NULL;
    if (!context) {
        return -1;
    }
    cJSON *endpoint;
    cJSON_ArrayForEach(endpoint, cJSON_GetObjectItem(snapshot, "endpoints"))
    {
        cJSON *host = cJSON_GetObjectItem(endpoint, "IP_Host");
        char address[256];
        int n = snprintf(address, sizeof(address), "tcp://%s:6666", host->valuestring);
        if (n < 0 || (size_t)n >= sizeof(address)) {
            errno = EINVAL;
            goto cleanup;
        }
        requester = zmq_socket(context, ZMQ_REQ);
        if (!requester || zmq_configure(requester) || zmq_connect(requester, address)) {
            goto cleanup;
        }
        const char *groups[] = {"PLCS", "Server"};
        const char *types[] = {"plc", "server"};
        for (int i = 0; i < 2; ++i) {
            cJSON *item;
            cJSON_ArrayForEach(item, cJSON_GetObjectItem(endpoint, groups[i]))
            {
                cJSON *copy = cJSON_Duplicate(item, true);
                if (!copy) {
                    errno = ENOMEM;
                    goto cleanup;
                }
                bool allocated = cJSON_GetObjectItem(copy, "Type") ||
                                 cJSON_AddStringToObject(copy, "Type", types[i]);
                char *text = allocated ? cJSON_PrintUnformatted(copy) : NULL;
                cJSON_Delete(copy);
                if (!text) {
                    errno = ENOMEM;
                    goto cleanup;
                }
                int rc = send_config(requester, text, true);
                free(text);
                if (rc) {
                    goto cleanup;
                }
            }
        }
        if (send_config(requester, "END_MESSAGE", false)) {
            goto cleanup;
        }
        zmq_close(requester);
        requester = NULL;
    }
    result = 0;
    printf("Endpoint Initialization Complete\n");
cleanup: {
    int saved = errno;
    if (requester) {
        zmq_close(requester);
    }
    zmq_ctx_destroy(context);
    errno = saved;
}
    return result;
}

char *Read_Vars(const char *category, const char *varname, const char *fallback)
{
    cJSON *item = field(category, varname);
    const char *value = cJSON_IsString(item) ? item->valuestring : fallback;
    return value ? strdup(value) : NULL;
}

char *SimName(void)
{
    return Read_Vars("simulator", "executableName", NULL);
}

bool Read_flags(const char *category, const char *flagname, bool fallback)
{
    cJSON *item = field(category, flagname);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return cJSON_IsString(item) ? strtobool(item->valuestring) : fallback;
}

char *OpenFile(const char *filename, size_t *len)
{
    *len = 0;
    int file;
    RETRY_EINTR(file, open(filename, O_RDONLY | O_CLOEXEC));
    if (file == -1) {
        return NULL;
    }
    struct stat info;
    char *data = NULL;
    if (fstat(file, &info)) {
        goto cleanup;
    }
    if (!S_ISREG(info.st_mode) || info.st_size <= 0) {
        errno = EINVAL;
        goto cleanup;
    }
    data = mmap(NULL, (size_t)info.st_size, PROT_READ, MAP_PRIVATE, file, 0);
    if (data == MAP_FAILED) {
        data = NULL;
    } else {
        *len = (size_t)info.st_size;
    }
cleanup: {
    int saved = errno;
    close(file);
    errno = saved;
}
    return data;
}
