#ifndef plc_int
#define plc_int
#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#define MSG_BUFFER 256
void *PLC_Interface();
#endif