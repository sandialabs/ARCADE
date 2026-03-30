#ifndef SEM_STOP_H
#define SEM_STOP_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef _WIN32
#define win_check 1
#include <Windows.h>
extern HANDLE stop;  // Legacy - not used in fixed version
#elif __linux__
#define win_check 0
#include <semaphore.h>
extern sem_t* stop;
#endif

// Initialize stop semaphore handle (call once at startup)
void Init_Stop_Semaphore(void);

// Cleanup stop semaphore handle (call at shutdown)
void Cleanup_Stop_Semaphore(void);

// Check if stop has been signaled
int Sem_Stop(void);

// Signal stop to all threads
void Set_Stop(void);

#endif