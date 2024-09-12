#ifndef sem_int
#define sem_int
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
	#include <Windows.h>
	#define win_check 1
	HANDLE up;
	HANDLE pub;
	HANDLE stop;
	HANDLE msg_sem;
#elif __linux__
	#include <semaphore.h>
	#define win_check 0
	sem_t* up;
	sem_t* pub;
	sem_t* stop;
	sem_t* msg_sem;
#endif // _WIN32
#include <fcntl.h>
void Sem_Interface(void);
void Sem_win_setval(HANDLE*);
#endif