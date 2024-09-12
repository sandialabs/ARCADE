/* Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
certain rights in this software.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "Shm_Interface.h"
#include "Sem_Interface.h"
#include "PLC_Interface.h"
#include "Sim_Control.h"
#include "init_Server.h"
#include "Data_Aggregator.h"
#include "DataBroker.h"

int main()
{
	void *status;
	
	int startFlag = 1;
	Sem_Interface();
	printf("Semaphores Initialized\n");
	init_Server(&startFlag); //comment out for testing

	/* Start Windows Only DataBroker*/
	DATA_Mutx = CreateMutex(NULL, FALSE, "Global\\DB_DATA_Mutx");
	if (DATA_Mutx == NULL) {
		fprintf(stderr, "Failed to get DATA Mutex.");
	}
		
	HANDLE aThreads[THREADCOUNT];

	aThreads[0] = CreateThread(NULL, 0, Shm_Interface, NULL, 0, &Sim_Int);
	aThreads[1] = CreateThread(NULL, 0, Sim_Control, NULL, 0, &Sim_Con);
	aThreads[2] = CreateThread(NULL, 0, PLC_Interface, NULL, 0, &PLC_Con);
	aThreads[3] = CreateThread(NULL, 0, Data_Aggregator, NULL, 0, &Data_Agg);

	WaitForMultipleObjects(THREADCOUNT, aThreads, TRUE, INFINITE);

	int i;
	for (i = 0; i < THREADCOUNT; i++) {
		CloseHandle(aThreads[i]);
	} 
	
	CloseHandle(DATA_Mutx); 
	
	return 0;
}
