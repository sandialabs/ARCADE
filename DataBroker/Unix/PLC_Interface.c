#include "PLC_Interface.h"

#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "utils.h"

void *PLC_Interface(void *_)
{
    int result = 0;
    void *context = zmq_ctx_new();
    void *socket = context ? zmq_socket(context, ZMQ_PULL) : NULL;
    if (!socket || zmq_configure(socket) || zmq_bind(socket, "tcp://*:5555")) {
        result = -1;
        goto cleanup;
    }
    while (!Sem_Stop()) {
        char buffer[MSG_BUFFER];
        int n = zmq_recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }

        if ((size_t)n >= sizeof(buffer)) {
            continue;
        }

        buffer[n] = '\0';

        char *name = buffer;
        char *value = strchr(buffer, ':');
        if (!value) {
            continue;
        }
        *value++ = '\0';
        if (!*name || !*value || strchr(value, ':')) {
            continue;
        }

        if (strcmp(name, "End_Sim") == 0) {
            Set_Stop();
            break;
        }

        if (strcmp(name, "Hold_Time") == 0) {
            pthread_mutex_lock(&FLAG_Mutx);
            FLAGS.Hold_Time_Flag = strtobool(value);
            pthread_mutex_unlock(&FLAG_Mutx);
            continue;
        } else if (strcmp(name, "Start_Sim") == 0) {
            pthread_mutex_lock(&FLAG_Mutx);
            FLAGS.Hold_Time_Flag = !strtobool(value);
            pthread_mutex_unlock(&FLAG_Mutx);
            continue;
        }

        char *end;
        errno = 0;
        double number = strtod(value, &end);
        bool converted = end != value;
        while (isspace((unsigned char)*end)) {
            ++end;
        }
        if (!converted || *end || errno == ERANGE || !isfinite(number)) {
            fprintf(stderr, "Ignoring invalid PLC value for %s\n", name);
            continue;
        }
        pthread_mutex_lock(&FLAG_Mutx);
        bool ready = CONF.config_captured;
        pthread_mutex_unlock(&FLAG_Mutx);
        if (!ready) {
            continue;
        }
        pthread_mutex_lock(&DATA_Mutx);
        for (int i = 0; i < MAX_IO; ++i) {
            if (strcmp(UP_DATA[i].Name, name) == 0) {
                if (!CO_SIM_OWNED[i]) {
                    UP_DATA[i].Value = number;
                    UP_DATA[i].Time = PUB_TIME;
                }
                break;
            }
        }
        pthread_mutex_unlock(&DATA_Mutx);
    }
cleanup:
    if (result) {
        perror("PLC interface");
        Set_Stop();
    }
    if (socket) {
        zmq_close(socket);
    }
    if (context) {
        zmq_ctx_destroy(context);
    }
    printf("ZMQ Update Server Closed Successfully\n");
    return result ? (void *)-1 : NULL;
}
