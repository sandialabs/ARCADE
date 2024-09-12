#include "UDP_Server.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"

DWORD WINAPI Shm_Interface(LPVOID arg)
{
	// Setup names for shm
	TCHAR init_shm_name[] = TEXT("Global\\DB_init_shm");
	TCHAR pub_shm_name[] = TEXT("Global\\DB_PUB_shm");
	TCHAR up_shm_name[] = TEXT("Global\\DB_UP_shm");
	TCHAR pass_init_shm_name[] = TEXT("Local\\DA_init_shm");

	HANDLE semaphore_Write, semaphore_Done;

	//Setup semaphores
	HANDLE up, pub, msg_sem;
	up = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\up_DB_sem");
	if (up == NULL) {
		fprintf(stderr, "Failed to open update semaphore.");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	pub = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\pub_DB_sem");
	if (pub == NULL) {
		fprintf(stderr, "Failed to open publish semaphore.");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	msg_sem = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\msg_DB_sem");
	if (msg_sem == NULL) {
		fprintf(stderr, "Failed to open message semaphore.");
		Set_Stop();
		UDP_Stop();
		return -1;
	}

	int wait_mod = 0;
	//Semaphore closed by Data_Aggregator upon completion of data exchange
	semaphore_Write = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Local\\SemaphoreWrite");
	if (semaphore_Write == NULL) {
		while (1) {
			semaphore_Write = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Local\\SemaphoreWrite");
			if (semaphore_Write != NULL) {
				break;
			}
			else {
				if (wait_mod % 100 == 0) {
					printf("Waiting for DA semaphores");
				}
				//printf("Semaphore still waiting\n");
				//Check stop semaphore
				int STOP = Sem_Stop();
				if (STOP > 0)
				{
					break;
				}
				wait_mod += 1;
			}
		}
		/*
		fprintf(stderr, "Failed to open write semaphore.");
		Set_Stop();
		return -1;
		*/
	}
	//Semaphore closed by Data_Aggregator upon completion of data exchange
	semaphore_Done = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Local\\SemaphoreDone");
	if (semaphore_Done == NULL) {
		fprintf(stderr, "Failed to open done semaphore");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	
	// Open the Mutex
	DATA_Mutx = OpenMutex(SYNCHRONIZE, FALSE, "Global\\DB_DATA_Mutx");
	if (DATA_Mutx == NULL) {
		fprintf(stderr, "Failed to get DATA Mutex.");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	DWORD DB_WaitResult;

	//Gather number of update and publish points from simulink
	//And get timestep size
	printf("Wait for Semaphore\n");
	DWORD Sem_check;
	DWORD wait_time = 10000;
	int STOP;
	while (1) {
		Sem_check = WaitForSingleObject(msg_sem, wait_time);
		if (Sem_check == WAIT_OBJECT_0) {
			printf("Semaphore captured\n");
			break;
		}
		else if(Sem_check == WAIT_FAILED) {
			printf("Semaphore failed\n");
		}
		else {
			printf("Semaphore still waiting\n");
			//Check stop semaphore
			STOP = Sem_Stop();
			if (STOP > 0)
			{
				break;
			}
		}
	}
	
	printf("Semaphore captured\n");
	int N_UP, N_PUB;
	double DT;
	HANDLE Init_MapFile;
	Init_MapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, init_shm_name);
	if (Init_MapFile == NULL) {
		fprintf(stderr, "Failed to open Init mapping file.\n");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	MSG_DATA *MSG_DB = (MSG_DATA *) MapViewOfFile(Init_MapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MSG_DATA));
	if (MSG_DB == NULL) {
		fprintf(stderr, "Failed to map MSG_DB shared memory.\n");
		Set_Stop();
		UDP_Stop();
		CloseHandle(Init_MapFile);
		return -1;
	}
	else {
		//extract values from initalization SHM
		N_UP = MSG_DB->UP; //number of update points
		N_PUB = MSG_DB->PUB; //number of publish points
		DT = MSG_DB->TimeStep; //timestep size 
		
		// close shared memory
		UnmapViewOfFile(MSG_DB);
		CloseHandle(Init_MapFile);

		printf("Update Points: %i\nPublish Points: %i\nTimestep Size %f\n", N_UP, N_PUB, DT);
		
	} 

	// Signal that read is done
	
	if (!ReleaseSemaphore( msg_sem, 1, NULL)) {
		fprintf(stderr, "ReleaseSemaphore error: %d\n", GetLastError());
		Set_Stop();
		UDP_Stop();
		return -1;
	}


	//Wait for Data_Aggregator to setup shared memory for Shm_Interface
	while (1) {
		Sem_check = WaitForSingleObject(semaphore_Write, wait_time);
		if (Sem_check == WAIT_OBJECT_0) {
			printf("DA Semaphore captured\n");
			break;
		}
		else if (Sem_check == WAIT_FAILED) {
			printf("DA Semaphore failed\n");
		}
		else {
			printf("DA Semaphore still waiting\n");
			//Check stop semaphore
			STOP = Sem_Stop();
			if (STOP > 0)
			{
				break;
			}
		}
	}

	HANDLE pass_Init_Values_Map_File;
	MSG_DATA* init_Values;

	pass_Init_Values_Map_File = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, pass_init_shm_name);
	if (pass_Init_Values_Map_File == NULL) {
		fprintf(stderr, "Failed to open mapping file to pass init values.");
		Set_Stop();
		return -1;
	}
	init_Values = (MSG_DATA*)MapViewOfFile(pass_Init_Values_Map_File, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MSG_DATA));
	if (init_Values == NULL) {
		fprintf(stderr, "Failed to map shared memory to pass Init Values.");
		Set_Stop();
		CloseHandle(pass_Init_Values_Map_File);
		return -1;
	}
	else {
		//send values from initalization SHM

		init_Values->UP = N_UP;
		init_Values->PUB = N_PUB;
		init_Values->TimeStep = DT;
		printf("Init data written to shared memory\n");

		//Allow Data_Aggregator to continue
		ReleaseSemaphore(semaphore_Done, 1, NULL);
		UnmapViewOfFile(init_Values);
	}


	Sleep(500L);

	while (1) {
		Sem_check = WaitForSingleObject(msg_sem, wait_time);
		if (Sem_check == WAIT_OBJECT_0) {
			printf("Semaphore captured\n");
			break;
		}
		else if (Sem_check == WAIT_FAILED) {
			printf("Semaphore failed\n");
		}
		else {
			printf("Semaphore still waiting\n");
			//Check stop semaphore
			STOP = Sem_Stop();
			if (STOP > 0)
			{
				break;
			}
		}
	}
	// Setup the shared memory locations
	HANDLE PUB_MapFile, UP_MapFile;
	PUB_MapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, pub_shm_name);
	UP_MapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, up_shm_name);
	if (PUB_MapFile == NULL || UP_MapFile == NULL) {
		fprintf(stderr, "Failed to open SHM mapping files.");
		Set_Stop();
		UDP_Stop();
		return -1;
	}
	DATA* PUB_DB = (DATA*)MapViewOfFile(PUB_MapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DATA) * N_PUB);
	DATA* UP_DB = (DATA*)MapViewOfFile(UP_MapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DATA) * N_UP);
	if (PUB_DB == NULL || UP_DB == NULL) {
		fprintf(stderr, "Failed to map DB shared memory.");
		Set_Stop();
		UDP_Stop();
		CloseHandle(PUB_MapFile);
		CloseHandle(UP_MapFile);
		return -1;
	}
	else {
		
		DB_WaitResult = WaitForSingleObject(DATA_Mutx, 5000L);

		// If mutex is captured
		if (DB_WaitResult == WAIT_OBJECT_0) {
			int n;
			for (n = 0; n < N_PUB; n++)
			{
				PUB_DATA[n] = PUB_DB[n];
				enqueue(PUB_DATA_QUEUE, PUB_DB[n]);
			}
			for (n = 0; n < N_UP; n++)
			{
				UP_DATA[n] = UP_DB[n];
				enqueue(UP_DATA_QUEUE, UP_DB[n]);
			}
			//Release mutex
			if (!ReleaseMutex(DATA_Mutx)) {
				fprintf(stderr, "Failed to release DATA Mutex.");
			}

			for (n = 0; n < N_PUB; n++)
			{
				printf("%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
			}
			for (n = 0; n < N_UP; n++)
			{
				printf("%s %s %f %f sec \n", UP_DATA[n].Name, UP_DATA[n].Type, UP_DATA[n].Value, UP_DATA[n].Time);
			}
			// now the spagetti is ready
		}
		else {
			fprintf(stderr, "Failed to get Data Mutex.");
		}
	}
	
	// Begin the data exchange loop
	DWORD Pub_check;
	DWORD Pub_wait_time = 1000;
	int wait_count = 0;
	int wait_limit = 5;
	char msg[256 * 100];
	int n;
	
	//timing vars
	LARGE_INTEGER StartingTime, EndingTime, T_INTERVAL, Frequency;
	LARGE_INTEGER DT_LI;
	QueryPerformanceFrequency(&Frequency);
	DT_LI.QuadPart = (int)(DT * Frequency.QuadPart);
	T_INTERVAL.QuadPart = 0;
	QueryPerformanceCounter(&StartingTime);

	//Start with a semaphore triggered
	if (!ReleaseSemaphore(up, 1, NULL)) {
		fprintf(stderr, "ReleaseSemaphore error: %d\n", GetLastError());
		Set_Stop();
		UDP_Stop();
		return -1;
	}

	while (1) {
		// check the stop semaphore
		STOP = Sem_Stop();
		if (STOP > 0)
		{
			break;
		}

		// safely wait for the semaphore
		while (1) {
			if (wait_count > wait_limit) {
				Set_Stop();
				UDP_Stop();
				printf("Semaphore wait time exceeded, exit due to stall...\n");
				break;
			}
			Sem_check = WaitForSingleObject(pub, Pub_wait_time);
			if (Sem_check == WAIT_OBJECT_0) {
				wait_count = 0;
				break;
			}
			else if (Sem_check == WAIT_FAILED) {
				printf("Semaphore wait failed\n");
				++wait_count;
			}
			else {
				//Check stop semaphore
				STOP = Sem_Stop();
				if (STOP > 0)
				{
					break;
				}
			}
		}
		
		// capture mutex
		DB_WaitResult = WaitForSingleObject(DATA_Mutx, 500L);

		// If mutex is captured
		if (DB_WaitResult == WAIT_OBJECT_0) {
			
			// capture updates
			memset(msg, 0, sizeof(msg));
			for (n = 0; n < N_PUB; n++)
			{
				PUB_DATA[n] = PUB_DB[n];
				if (n == 0) {
					sprintf(msg, "%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
				}
				else {
					sprintf(msg + strlen(msg), "%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
				}
				enqueue(PUB_DATA_QUEUE, PUB_DB[n]);
				printf("%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
			}
			/* TESTING PURPOSES */
			for (n = 0; n < N_UP; n++)
			{
				UP_DATA[n].Time = PUB_DATA[0].Time;
			}
			/* /TESTING PURPOSES */

			for (n = 0; n < N_UP; n++)
			{
				UP_DB[n] = UP_DATA[n];
				enqueue(UP_DATA_QUEUE, UP_DB[n]);
				printf("%s %s %f %f sec \n", UP_DATA[n].Name, UP_DATA[n].Type, UP_DATA[n].Value, UP_DATA[n].Time);
			}
			
			if (!ReleaseMutex(DATA_Mutx)) {
				fprintf(stderr, "Failed to release DATA Mutex.");
			}
			// message out escape key
			printf("\n***Press X then Enter to stop simulation***\n");

			// broadcast the publish points
			UDP_Server(msg);

			// wait timestep
			// Time control
			
			while (T_INTERVAL.QuadPart < DT_LI.QuadPart)
			{
				QueryPerformanceCounter(&EndingTime);
				T_INTERVAL.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
				//printf("Time interval: %i \n", T_INTERVAL.QuadPart);
				//printf("Time dT: %i \n", DT_LI.QuadPart);
			}
			QueryPerformanceCounter(&StartingTime);
			T_INTERVAL.QuadPart = 0;
			

			if (!ReleaseSemaphore(up, 1, NULL)) {
				fprintf(stderr, "ReleaseSemaphore error: %d\n", GetLastError());
				Set_Stop();
				UDP_Stop();
				return -1;
			}
		}
		else {
			printf("Failed to capture MUTX\n");
		}
	}
	
	UDP_Stop();
	memset(msg, 0, sizeof(msg));
	sprintf(msg, "STOP\nSTOP\n");

	UnmapViewOfFile(PUB_DB);
	UnmapViewOfFile(UP_DB);
	CloseHandle(PUB_MapFile);
	CloseHandle(UP_MapFile);
	clearQueue(UP_DATA_QUEUE);
	clearQueue(PUB_DATA_QUEUE);
	Set_Stop();
	
	return 0;
}