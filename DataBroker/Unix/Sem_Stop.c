#include "Sem_Stop.h"

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

sem_t *stop;
static atomic_int signal_stop;
static atomic_bool local_stop = false;

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "A signal flag must not require a lock");

static void stop_signal(int signal_number)
{
    (void)signal_number;
    atomic_store(&signal_stop, 1);
}

int Init_Stop_Signals(void)
{
    struct sigaction action = {0};
    action.sa_handler = stop_signal;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL) ? -1 : 0;
}

void Init_Stop_Semaphore(void)
{
    stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (stop == SEM_FAILED) {
        perror("sem_open /stop failed");
        stop = NULL;
    }
}

void Cleanup_Stop_Semaphore(void)
{
    if (stop != NULL && stop != SEM_FAILED) {
        sem_close(stop);
        sem_unlink("/stop");
        stop = NULL;
    }
}

int Sem_Stop(void)
{
    if (atomic_load(&signal_stop)) {
        Set_Stop();
    }
    if (atomic_load(&local_stop)) {
        return 1;
    }
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
        return 1; // Stop is signaled
    } else {
        // sem_trywait failed - either EAGAIN (sem is 0) or error
        return 0; // Not stopped
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

void Set_Stop(void)
{
    if (atomic_exchange(&local_stop, true)) {
        return;
    }
    if (stop == NULL) {
        fprintf(stderr, "Stop semaphore not initialized.\n");
        return;
    }
    if (sem_post(stop) == -1) {
        perror("sem_post");
    }
}
