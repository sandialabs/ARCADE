#ifndef shm_int
#define shm_int
#ifdef _WIN32
    #include <Windows.h>
    #include <io.h>
    #include <tchar.h>
    HANDLE DATA_Mutx;
#elif __linux__
    #include <sys/ipc.h>
    #include <sys/shm.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <semaphore.h>
    pthread_mutex_t DATA_Mutx;
#endif // _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
// S-function Defines
#define _BSD_SOURCE
#define MAX_IO 1000
DWORD WINAPI Shm_Interface(LPVOID arg);
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
    struct node* next;
} Node;
typedef struct Queue {
    Node* front;  // Pointer to the front of the queue
    Node* rear;   // Pointer to the rear of the queue
    CRITICAL_SECTION lock;
} Queue;
DATA UP_DATA[MAX_IO]; // Upper limit of IO = 1000
DATA PUB_DATA[MAX_IO];
extern Queue* UP_DATA_QUEUE;
extern Queue* PUB_DATA_QUEUE;
extern void enqueue(Queue* q, DATA value);
#endif