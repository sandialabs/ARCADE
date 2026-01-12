#ifndef ZMQ_CLIENT_H
#define ZMQ_CLIENT_H

#include <pthread.h>
#include <stdbool.h>
#include <zmq.h>
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Shared sync flags and condition
extern bool ZMQ_SEND_TRIGGERED;      // Set by Shm_Interface
extern bool ZMQ_RESPONSE_READY;      // Set by ZMQ_Client
extern pthread_mutex_t ZMQ_LOCK;
extern pthread_cond_t ZMQ_COND;

typedef struct {
    char tag[64];
    int index;
} ZMQ_TagMap;

#define MAX_ZMQ_VARS 512
#define ZMQ_BUFFER 1024

extern ZMQ_TagMap ZMQ_Inputs[MAX_ZMQ_VARS];
extern ZMQ_TagMap ZMQ_Outputs[MAX_ZMQ_VARS];

void* ZMQ_Client(void* args);
void CoSim_init();

#endif // ZMQ_CLIENT_H
