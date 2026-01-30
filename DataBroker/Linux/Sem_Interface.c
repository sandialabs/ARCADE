#include "Sem_Interface.h"
#include <errno.h>

// Helper function to drain a semaphore to 0 (works on both Linux and macOS)
static void drain_semaphore(sem_t *sem) {
    if (sem == SEM_FAILED || sem == NULL) return;
    
    // Try to decrement until we can't anymore (semaphore is at 0)
    // sem_trywait returns 0 on success, -1 with EAGAIN when sem is 0
    while (sem_trywait(sem) == 0) {
        // Successfully decremented, keep going
    }
    // When we get here, semaphore is at 0 (or error)
}

//setup semaphores and bring them to 0
void Sem_Interface(void)
{
 sem_t *up;
 sem_t *pub;
 sem_t *stop;
 sem_t *msg_sem;
 sem_t *co_sim;
 sem_t *co_sim_2;

 // Open or create semaphores
 pub = sem_open("/pp_sem", O_CREAT, 0644, 0);
 up = sem_open("/up_sem", O_CREAT, 0644, 0);
 stop = sem_open("/stop", O_CREAT, 0644, 0);
 msg_sem = sem_open("/msg", O_CREAT, 0644, 0);
 co_sim = sem_open("/co_sim", O_CREAT, 0644, 0);
 co_sim_2 = sem_open("/co_sim_2", O_CREAT, 0644, 0);

 // Check for errors
 if (pub == SEM_FAILED) perror("sem_open /pp_sem failed");
 if (up == SEM_FAILED) perror("sem_open /up_sem failed");
 if (stop == SEM_FAILED) perror("sem_open /stop failed");
 if (msg_sem == SEM_FAILED) perror("sem_open /msg failed");
 if (co_sim == SEM_FAILED) perror("sem_open /co_sim failed");
 if (co_sim_2 == SEM_FAILED) perror("sem_open /co_sim_2 failed");

 // Drain all semaphores to 0 (works on both Linux and macOS)
 drain_semaphore(pub);
 drain_semaphore(up);
 drain_semaphore(stop);
 drain_semaphore(msg_sem);
 drain_semaphore(co_sim);
 drain_semaphore(co_sim_2);
}
