#pragma once

#include <pthread.h>
#include "../ipc.h"
#include <stdbool.h>

/// Mutex guarding `PUB_DATA`,`UP_DATA`, `CO_SIM_OWNED`, and `PUB_TIME`.
extern pthread_mutex_t DATA_Mutx;

/// Initialize the shared memory thread.
void *Shm_Interface(void *);

extern DATA UP_DATA[MAX_IO];
extern DATA PUB_DATA[MAX_IO];

/// Whether each input is owned by a co-simulation.
extern bool CO_SIM_OWNED[MAX_IO];

/// Time of the latest publication.
extern double PUB_TIME;
