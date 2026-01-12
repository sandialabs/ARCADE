#include "utils.h"
#include "Sem_Stop.h"
#include "UDP_Server.h"

bool strtobool(char *string){
    char *true_string = "true";
    
    if (strcasecmp(string,true_string) == 0){
        return true;
    }
    else{
        return false;
    }
}

void sem_wait_safe(void* arg, int wait_limit){

    sem_t* semaphore = (sem_t*)arg; // Cast the argument to a semaphore pointer
    int wait_count = 0;
    int STOP = 0;
    // Safely wait for the semaphore
    while (1) {
        if (wait_count > wait_limit && wait_limit > 0) {
            Set_Stop();
            UDP_Stop();
            printf("Semaphore wait time exceeded, exit due to stall...\n");
            break;
        }
        if (sem_wait(semaphore) == 0) {
            wait_count = 0;
            break;
        } else {
            perror("Semaphore wait failed");
            ++wait_count;
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
        }
    }
}

void sem_wait_value(void* arg, int sem_value){

    sem_t* semaphore = (sem_t*)arg; // Cast the argument to a semaphore pointer
    int sem_count, sem_int;
    int STOP = 0;
    // Safely wait for the semaphore
    while (1) { 
        sem_getvalue(semaphore, &sem_count);
		sem_int = (int)sem_count;

        if (sem_int >= sem_value) {
            break;
        } else {
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
        }
    }
}

void sem_decrement(void* arg, int sem_value){

    sem_t* semaphore = (sem_t*)arg; // Cast the argument to a semaphore pointer
    int sem_count, sem_int;
    int STOP = 0;
    // Safely wait for the semaphore
    while (1)
	{
		sem_getvalue(semaphore, &sem_count);
		sem_int = (int)sem_count;
		if (sem_int > sem_value)
		{
			sem_trywait(semaphore);
		}
		if (sem_int == sem_value || sem_int == 0)
		{
			break;
		}
	}
}