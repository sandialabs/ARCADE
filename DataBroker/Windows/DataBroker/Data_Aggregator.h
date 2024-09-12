#ifndef data_agg
#define data_agg
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <Windows.h>
#include <io.h>
#define MSG_BUFFER 256
DWORD WINAPI Data_Aggregator(LPVOID arg);
void enqueue(Queue* q, DATA value);
DATA dequeue(Queue* q);
int isEmpty(Queue* q);
void clearQueue(Queue* q);
Queue* createQueue();
#endif