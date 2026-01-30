#include "Sem_Stop.h"
#include <errno.h>

sem_t* stop;

//utility to check the simulations stop flag semaphore
int Sem_Stop(void)
{
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (stop == SEM_FAILED) {
        return 0;  // Can't open semaphore, assume not stopped
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
    // Post (increment) the semaphore
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (stop == SEM_FAILED) {
        perror("sem_open /stop failed");
        return;
    }
    if (sem_post(stop) == -1) {
        perror("sem_post");
    }
}

// Don't forget to close and unlink the semaphore when done
void Cleanup_Stop(void) {
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (stop == SEM_FAILED) {
        return;
    }
    if (sem_close(stop) == -1) {
        perror("sem_close");
    }
    if (sem_unlink("/stop") == -1) {
        perror("sem_unlink");
    }
}
