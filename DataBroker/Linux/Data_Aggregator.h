#ifndef DATA_AGG_H
#define DATA_AGG_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Shm_Interface.h"  // Include to get Queue and DATA definitions

#define MSG_BUFFER 256

void* Data_Aggregator(void* arg);
void enqueue(Queue* q, DATA value);
DATA dequeue(Queue* q);
int isEmpty(Queue* q);
void clearQueue(Queue* q);
Queue* createQueue();

#endif // DATA_AGG_H