#ifndef dataB
#define dataB

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
	#include <Windows.h>
	#define win_check 1
	DWORD Sim_Int;
	DWORD Sim_Con;
	DWORD PLC_Con;
	DWORD Data_Agg;
	#define THREADCOUNT 4
#elif __linux__
	#define win_check 0
	#include <pthread.h>
	pthread_t Sim_Int;
	pthread_t Sim_Con;
	pthread_t PLC_Con;
	pthread_t Data_Agg;
	pthread_mutex_init(&DATA_Mutx, NULL);
#endif // _WIN32

#endif

