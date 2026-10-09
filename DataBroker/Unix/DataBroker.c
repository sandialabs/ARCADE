// Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
// Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains
// certain rights in this software.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "Sem_Interface.h"
#include "Sem_Stop.h"
#include "Shm_Interface.h"
#include "PLC_Interface.h"
#include "Sim_Control.h"
#include "init_Server.h"
#include "Data_Aggregator.h"
#include "ZMQ_Client.h"
#include "atomicSet.h"
#include "utils.h"

int main(void)
{
    int result = 0;
    if (Init_Stop_Signals() || Lock_Interface()) {
        perror("Acquiring broker ownership");
        return 1;
    }
    if (Sem_Interface() != 0) {
        perror("Initializing semaphores");
        Cleanup_Interface();
        Unlock_Interface();
        return 1;
    }
    Init_Stop_Semaphore();
    if (!stop) {
        Cleanup_Interface();
        Unlock_Interface();
        return 1;
    }
    if (pthread_mutex_init(&DATA_Mutx, NULL) != 0) {
        Cleanup_Stop_Semaphore();
        Cleanup_Interface();
        Unlock_Interface();
        return 1;
    }
    if (pthread_mutex_init(&FLAG_Mutx, NULL) != 0) {
        pthread_mutex_destroy(&DATA_Mutx);
        Cleanup_Stop_Semaphore();
        Cleanup_Interface();
        Unlock_Interface();
        return 1;
    }

    UP_DATA_QUEUE = createQueue();
    PUB_DATA_QUEUE = createQueue();
    if (!UP_DATA_QUEUE || !PUB_DATA_QUEUE) {
        result = 1;
        goto cleanup;
    }

    if (Load_Config() != 0) {
        result = 1;
        goto cleanup;
    }
    if (init_Server() != 0) {
        if (!Sem_Stop()) {
            perror("Initializing endpoints");
            result = 1;
        }
        goto cleanup;
    }

    if (Sem_Stop()) {
        goto cleanup;
    }

    pthread_t threads[5];
    void *(*workers[])(void *) = {
        Sim_Control, Shm_Interface, PLC_Interface, Data_Aggregator, ZMQ_Client};
    int count = CONF.Co_Sim_Enable ? 5 : 4;

    int started = 0;
    for (; started < count; ++started) {
        int rc = pthread_create(&threads[started], NULL, workers[started], NULL);
        if (rc) {
            errno = rc;
            perror("Starting worker");
            Set_Stop();
            result = 1;
            break;
        }
    }

    const int order[] = {0, 1, 2, 4, 3};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        int index = order[i];

        if (index == 3) {
            Finish_Logging();
        }
        if (index >= started) {
            continue;
        }

        void *status;
        int rc = pthread_join(threads[index], &status);
        if (rc || status != NULL) {
            result = 1;
        }
    }

    if (checkErrorFlag()) {
        result = 1;
    }

cleanup:
    Cleanup_Config();
    clearQueue(UP_DATA_QUEUE);
    clearQueue(PUB_DATA_QUEUE);
    Cleanup_Logging();
    pthread_mutex_destroy(&DATA_Mutx);
    pthread_mutex_destroy(&FLAG_Mutx);
    Cleanup_Stop_Semaphore();
    Cleanup_Interface();
    Unlock_Interface();
    printf("Exiting.\n");
    return result;
}
