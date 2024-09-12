#ifndef sem_stop
#define sem_stop
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef _WIN32
	#define win_check 1
	#include <Windows.h>
	HANDLE stop;
#elif __linux__
	#define win_check 0
	#include <semaphore.h>
	sem_t* stop;
#endif // _WIN32
int Sem_Stop(void);
void Set_Stop(void);
#endif