#include "Sem_Stop.h"
sem_t* stop;

//utility to check the simulations stop flag semaphore
int Sem_Stop(void)
{
    int ST;
	stop = sem_open("/stop", O_CREAT, 0644, 0);
	sem_getvalue(stop, &ST);
	int value = (int)ST;
	return value;
}

void Set_Stop(void) {
    // Post (increment) the semaphore
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (sem_post(stop) == -1) {
        perror("sem_post");
    }
}

// Don't forget to close and unlink the semaphore when done
void Cleanup_Stop(void) {
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (sem_close(stop) == -1) {
        perror("sem_close");
    }
    if (sem_unlink("/stop") == -1) {
        perror("sem_unlink");
    }
}
