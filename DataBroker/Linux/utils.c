#include "utils.h"
#include "Sem_Stop.h"
#include "UDP_Server.h"
#include <errno.h>
#include <time.h>

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
    int STOP = 0;
    int decremented_count = 0;
    
    // Wait until we can decrement sem_value times (meaning sem >= sem_value)
    while (1) {
#ifdef __APPLE__
        // macOS: Try to decrement sem_value times without blocking
        // First, try to grab sem_value decrements
        int grabbed = 0;
        while (grabbed < sem_value) {
            if (sem_trywait(semaphore) == 0) {
                grabbed++;
            } else {
                // Couldn't grab enough, put back what we took and wait
                for (int i = 0; i < grabbed; i++) {
                    sem_post(semaphore);
                }
                grabbed = 0;
                
                // Small sleep to avoid busy waiting
                struct timespec ts = {0, 10000000}; // 10ms
                nanosleep(&ts, NULL);
                
                // Check stop semaphore
                STOP = Sem_Stop();
                if (STOP > 0) {
                    return;
                }
                break; // Break inner loop and retry
            }
        }
        if (grabbed >= sem_value) {
            // Put them back - we were just checking the value
            for (int i = 0; i < grabbed; i++) {
                sem_post(semaphore);
            }
            break; // Success - semaphore has at least sem_value
        }
#else
        // Linux: use sem_getvalue
        int sem_count;
        sem_getvalue(semaphore, &sem_count);
        int sem_int = (int)sem_count;

        if (sem_int >= sem_value) {
            break;
        } else {
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
            // Small sleep to avoid busy waiting
            struct timespec ts = {0, 10000000}; // 10ms
            nanosleep(&ts, NULL);
        }
#endif
    }
}

void sem_decrement(void* arg, int sem_value){

    sem_t* semaphore = (sem_t*)arg; // Cast the argument to a semaphore pointer
    int STOP = 0;
    
    // Decrement semaphore down to sem_value (or 0, whichever is higher)
    while (1)
    {
#ifdef __APPLE__
        // macOS: just try to decrement, stop when we can't
        if (sem_trywait(semaphore) == 0) {
            // Successfully decremented, check if we should continue
            // We need to peek at the new value, but we can't on macOS
            // So we use a counter approach - keep decrementing until we fail
            continue;
        } else {
            // Can't decrement anymore (sem is at 0) or error
            break;
        }
#else
        // Linux: use sem_getvalue to check current value
        int sem_count;
        sem_getvalue(semaphore, &sem_count);
        int sem_int = (int)sem_count;
        if (sem_int > sem_value)
        {
            sem_trywait(semaphore);
        }
        if (sem_int <= sem_value || sem_int == 0)
        {
            break;
        }
#endif
    }
}
