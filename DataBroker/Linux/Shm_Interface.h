#ifndef SHM_INT
#define SHM_INT

#ifdef _WIN32
    #include <Windows.h>
    #include <io.h>
    #include <tchar.h>
    HANDLE DATA_Mutx;
#elif __linux__
    #define _POSIX_C_SOURCE 200809L
    #include <sys/ipc.h>
    #include <sys/shm.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <semaphore.h>
    #include <sys/mman.h>
    pthread_mutex_t DATA_Mutx;
    pthread_mutex_t FLAG_Mutx;
#endif // _WIN32

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

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

//setup special functions
typedef struct {
    bool Hold_Time_Flag;
    bool Start_Time_Flag;
    bool Reset_Sim_Flag;
} Special_Flags;

extern DATA UP_DATA[MAX_IO]; // Upper limit of IO = 1000
extern DATA PUB_DATA[MAX_IO];

extern Queue* UP_DATA_QUEUE;
extern Queue* PUB_DATA_QUEUE;

extern Special_Flags FLAGS;

void enqueue(Queue* q, DATA value);
void clearQueue(Queue* q);

#endif // SHM_INT