#include "Sem_Stop.h"
#include <errno.h>

sem_t* stop;

// Initialize the stop semaphore handle once
void Init_Stop_Semaphore(void) {
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (stop == SEM_FAILED) {
        perror("sem_open /stop failed");
        stop = NULL;
    }
}

// Cleanup the stop semaphore handle
void Cleanup_Stop_Semaphore(void) {
    if (stop != NULL && stop != SEM_FAILED) {
        sem_close(stop);
        sem_unlink("/stop");
        stop = NULL;
    }
}

//utility to check the simulations stop flag semaphore
int Sem_Stop(void)
{
    if (stop == NULL) {
        return 0;
    }
    
#ifdef __APPLE__
    // macOS doesn't support sem_getvalue, use sem_trywait instead
    // Try to decrement - if successful, stop was signaled
    if (sem_trywait(stop) == 0) {
        // Successfully decremented, meaning stop was > 0
        // Post it back so other threads also see the stop signal
        sem_post(stop);
        return 1;  // Stop is signaled
    } else {
        // sem_trywait failed - either EAGAIN (sem is 0) or error
        return 0;  // Not stopped
    }
#else
    // Linux - use sem_getvalue
    int ST = 0;
    if (sem_getvalue(stop, &ST) == 0) {
        return ST;
    }
    return 0;
#endif
}

void Set_Stop(void) {
    if (stop == NULL) {
        fprintf(stderr, "Stop semaphore not initialized.\n");
        return;
    }
    if (sem_post(stop) == -1) {
        perror("sem_post");
    }
}


