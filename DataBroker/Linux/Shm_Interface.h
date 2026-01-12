#ifndef SHM_INT
#define SHM_INT

#define _POSIX_C_SOURCE 200809L
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// Data mutex
extern pthread_mutex_t DATA_Mutx;

// S-function Defines
#define _BSD_SOURCE
#define MAX_IO 1000

#ifdef _WIN32
DWORD WINAPI Shm_Interface(LPVOID arg);
#else
void* Shm_Interface(void* arg);
#endif

// Shm Variables
typedef struct {
    char Name[128];
    char Type[50];
    double Value;
    double Time;
} DATA;

typedef struct {
    int PUB;
    int UP;
    double TimeStep;
} MSG_DATA;

typedef struct Node {
    DATA data;
    struct timespec realTime;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* front;  // Pointer to the front of the queue
    Node* rear;   // Pointer to the rear of the queue
    #ifdef _WIN32
    CRITICAL_SECTION lock;
    #else
    pthread_mutex_t lock;
    #endif
} Queue;

extern DATA UP_DATA[MAX_IO]; // Upper limit of IO = 1000
extern DATA PUB_DATA[MAX_IO];

extern Queue* UP_DATA_QUEUE;
extern Queue* PUB_DATA_QUEUE;

void enqueue(Queue* q, DATA value);
void clearQueue(Queue* q);

#endif // SHM_INT