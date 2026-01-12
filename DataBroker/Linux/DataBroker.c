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

#include "DataBroker.h"
#include "Sem_Interface.h"
#include "Shm_Interface.h"
#include "PLC_Interface.h"
#include "Sim_Control.h"
#include "init_Server.h"
#include "Data_Aggregator.h"
#include "ZMQ_Client.h"

pthread_mutex_t FLAG_Mutx;

Special_Flags FLAGS;
Configs CONF;

int main()
{
	void *status;
	pthread_t Sim_Int;
	pthread_t Sim_Con;
	pthread_t PLC_Con;
	pthread_t DA_Thr;
	pthread_t ZMQ_Thr;
	Sem_Interface();
	printf("Semaphores Initialized\n");
	

	// Open the Mutexes
    if (pthread_mutex_init(&DATA_Mutx, NULL) != 0) {
        perror("Mutex initialization failed");
        return -1;
    }
	if (pthread_mutex_init(&FLAG_Mutx, NULL) != 0) {
        perror("Mutex initialization failed");
        return -1;
    }
	
	// initialization server start
	init_Server();

	// Setup special flags
    Configs CONF_FLAGS;
    pthread_mutex_lock(&FLAG_Mutx);
    CONF_FLAGS = CONF;
    pthread_mutex_unlock(&FLAG_Mutx);

	// Start threads
	pthread_create(&Sim_Con, NULL, Sim_Control, (void *)0);
	pthread_create(&Sim_Int, NULL, Shm_Interface, (void *)0);
	pthread_create(&PLC_Con, NULL, PLC_Interface, (void *)0);
	pthread_create(&DA_Thr, NULL, Data_Aggregator, (void *)0);

	if (CONF_FLAGS.Co_Sim_Enable){
		printf("Co-Simulation Enabled\n");
		pthread_create(&ZMQ_Thr, NULL, ZMQ_Client, (void *)0);
	}
	else {
		printf("Co-Simulation Disabled\n");
	}

	// Join threads
	pthread_join(Sim_Con, &status);
	pthread_join(Sim_Int, &status);
	pthread_join(PLC_Con, &status);
	pthread_join(DA_Thr, &status);
	// Join co-sim thread if started
	if (CONF_FLAGS.Co_Sim_Enable){
		pthread_join(ZMQ_Thr, &status);
	}
	
	// Destory mutexes
	pthread_mutex_destroy(&DATA_Mutx);
	pthread_mutex_destroy(&FLAG_Mutx);

	return 0;
}
