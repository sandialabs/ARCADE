#include "UDP_Server.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"

DATA UP_DATA[MAX_IO];
DATA PUB_DATA[MAX_IO];

Special_Flags FLAGS;

void* Shm_Interface(void* arg)
{
    printf("Starting Shm_Interface\n");

    // Setup keys for shm
    key_t msg_key = 10620; // shared memory DB key
    key_t SHM_PUB = 10618; // shared memory publish key
    key_t SHM_UP = 10619; // shared memory update key
    key_t pass_init_key = 10621; // shared memory pass init key

    sem_t* semaphore_Write;
    sem_t* semaphore_Done;

    // Setup semaphores
    sem_t* up = sem_open("/up_sem", O_CREAT, 0644, 0);
    if (up == SEM_FAILED) {
        perror("Failed to open update semaphore");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }

    sem_t* pub = sem_open("/pp_sem", O_CREAT, 0644, 0);
    if (pub == SEM_FAILED) {
        perror("Failed to open publish semaphore");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }

    sem_t* msg_sem = sem_open("/msg", O_CREAT, 0644, 0);
    if (msg_sem == SEM_FAILED) {
        perror("Failed to open message semaphore");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }
    printf("waiting for DA *********************\n");
    int wait_mod = 0;
    // Semaphore closed by Data_Aggregator upon completion of data exchange

    // Setup special flags
    Special_Flags SHM_FLAGS;
    pthread_mutex_lock(&FLAG_Mutx);
    SHM_FLAGS = FLAGS;
    pthread_mutex_unlock(&FLAG_Mutx);

    //special flag timer
    struct timespec remaining, request = { 0, 1000000 };


    // Attempt to open the semaphore until it exists
    while (1) {
        semaphore_Write = sem_open("/SemaphoreWrite", 0);
        if (semaphore_Write != SEM_FAILED) {
            break;
        } else {
            if (errno == ENOENT) {
                if (wait_mod % 100 == 0) {
                    printf("Waiting for DA semaphores to be created\n");
                }
                // Check stop semaphore
                int STOP = Sem_Stop();
                if (STOP > 0) {
                    printf("Stop condition detected, exiting loop\n");
                    return (void*)-1;
                }
                wait_mod += 1;
                usleep(10000); // Sleep for 10ms to avoid busy waiting
            } else {
                perror("sem_open failed");
                return (void*)-1;
            }
        }
    }
    printf("Done waiting for DA\n");

    // Open the Mutex
    if (pthread_mutex_init(&DATA_Mutx, NULL) != 0) {
        perror("Mutex initialization failed");
        return (void*)-1;
    }

    // Gather number of update and publish points from simulink
    // And get timestep size
    printf("Wait for Semaphore\n");
    //printf("Hello");
    int STOP;
    int i;
    while (1) {
        printf("Entering loop\n");
        if (sem_wait(msg_sem) == 0) {
            printf("Semaphore captured\n");
            break;
        } else {
            perror("Semaphore wait failed");
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
        }
        printf("Still waiting for Simulink\n");
    }

    printf("Semaphore captured\n");
    int N_UP, N_PUB;
    double DT;
    int shmdb = shmget(msg_key, sizeof(MSG_DATA), 0600 | IPC_CREAT);
    if (shmdb == -1) {
        perror("Failed to open Init mapping file");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }

    MSG_DATA* MSG_DB = (MSG_DATA*) shmat(shmdb, NULL, 0);
    if (MSG_DB == (void*) -1) {
        perror("Failed to attach MSG_DB shared memory");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    } else {
        // Extract values from initialization SHM
        N_UP = MSG_DB->UP; // Number update points
        N_PUB = MSG_DB->PUB; // Number publish points
        DT = MSG_DB->TimeStep; // Timestep size

        // Detach shared memory
        shmdt(MSG_DB);

        printf("Update Points: %i\nPublish Points: %i\nTimestep Size %f\n", N_UP, N_PUB, DT);
    }

    // Signal that read is done
    sem_post(msg_sem);

    // Wait for Data_Aggregator to setup shared memory for Shm_Interface
    while (1) {
        if (sem_wait(semaphore_Write) == 0) {
            printf("DA Semaphore captured\n");
            break;
        } else {
            perror("DA Semaphore wait failed");
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
        }
    }

    int pass_shmid = shmget(pass_init_key, sizeof(MSG_DATA), 0600 | IPC_CREAT);
    if (pass_shmid == -1) {
        perror("Failed to open mapping file to pass init values");
        Set_Stop();
        return (void*)-1;
    }

    MSG_DATA* init_Values = (MSG_DATA*) shmat(pass_shmid, NULL, 0);
    if (init_Values == (void*) -1) {
        perror("Failed to attach shared memory to pass Init Values");
        Set_Stop();
        return (void*)-1;
    } else {
        // Send values from initialization SHM
        init_Values->UP = N_UP;
        init_Values->PUB = N_PUB;
        init_Values->TimeStep = DT;
        printf("Init data written to shared memory\n");

        // Initialize semaphore_Done
        semaphore_Done = sem_open("/SemaphoreDone", O_CREAT, 0644, 0);
        if (semaphore_Done == SEM_FAILED) {
            perror("Failed to open done semaphore");
            Set_Stop();
            shmdt(init_Values);
            return (void*)-1;
        }

        // Allow Data_Aggregator to continue
        sem_post(semaphore_Done);
        shmdt(init_Values);
    }

    usleep(500000);

    while (1) {
        if (sem_wait(msg_sem) == 0) {
            printf("Semaphore captured\n");
            break;
        } else {
            perror("Semaphore wait failed");
            // Check stop semaphore
            STOP = Sem_Stop();
            if (STOP > 0) {
                break;
            }
        }
    }

    // Setup the shared memory locations
    int shmidpub = shmget(SHM_PUB, N_PUB * sizeof(DATA), 0600 | IPC_CREAT);
    int shmidup = shmget(SHM_UP, N_UP * sizeof(DATA), 0600 | IPC_CREAT);
    if (shmidpub == -1 || shmidup == -1) {
        perror("Failed to open SHM mapping files");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }

    DATA* PUB_DB = (DATA*) shmat(shmidpub, NULL, 0);
    DATA* UP_DB = (DATA*) shmat(shmidup, NULL, 0);
    if (PUB_DB == (void*) -1 || UP_DB == (void*) -1) {
        perror("Failed to attach DB shared memory");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    } else {
        pthread_mutex_lock(&DATA_Mutx);

        int n;
        for (n = 0; n < N_PUB; n++) {
            PUB_DATA[n] = PUB_DB[n];
            enqueue(PUB_DATA_QUEUE, PUB_DB[n]);
        }
        for (n = 0; n < N_UP; n++) {
            UP_DATA[n] = UP_DB[n];
            enqueue(UP_DATA_QUEUE, UP_DB[n]);
        }

        pthread_mutex_unlock(&DATA_Mutx);

        for (n = 0; n < N_PUB; n++) {
            printf("%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
        }
        for (n = 0; n < N_UP; n++) {
            printf("%s %s %f %f sec \n", UP_DATA[n].Name, UP_DATA[n].Type, UP_DATA[n].Value, UP_DATA[n].Time);
        }
    }

    // Begin the data exchange loop
    int wait_count = 0;
    int wait_limit = 5;
    char msg[256 * 100];
    int n;

    // Timing vars
    struct timespec StartingTime, EndingTime;
    double T_INTERVAL = 0;
    clock_gettime(CLOCK_MONOTONIC, &StartingTime);

    // Start with a semaphore triggered
    sem_post(up);

    while (1) {
        // Check the stop semaphore
        STOP = Sem_Stop();
        if (STOP > 0) {
            break;
        }

        // Safely wait for the semaphore
        while (1) {
            if (wait_count > wait_limit) {
                Set_Stop();
                UDP_Stop();
                printf("Semaphore wait time exceeded, exit due to stall...\n");
                break;
            }
            if (sem_wait(pub) == 0) {
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

        // Capture mutex
        pthread_mutex_lock(&DATA_Mutx);

        // Capture updates
        memset(msg, 0, sizeof(msg));
        for (n = 0; n < N_PUB; n++) {
            PUB_DATA[n] = PUB_DB[n];
            if (n == 0) {
                sprintf(msg, "%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
            } else {
                sprintf(msg + strlen(msg), "%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
            }
            enqueue(PUB_DATA_QUEUE, PUB_DB[n]);
            printf("%s %s %f %f sec \n", PUB_DATA[n].Name, PUB_DATA[n].Type, PUB_DATA[n].Value, PUB_DATA[n].Time);
        }

        /* TESTING PURPOSES */
        for (n = 0; n < N_UP; n++) {
            UP_DATA[n].Time = PUB_DATA[0].Time;
        }
        /* /TESTING PURPOSES */

        for (n = 0; n < N_UP; n++) {
            UP_DB[n] = UP_DATA[n];
            enqueue(UP_DATA_QUEUE, UP_DB[n]);
            printf("%s %s %f %f sec \n", UP_DATA[n].Name, UP_DATA[n].Type, UP_DATA[n].Value, UP_DATA[n].Time);
        }

        pthread_mutex_unlock(&DATA_Mutx);

        // Message out escape key
        printf("\n***Press X then Enter to stop simulation***\n");

        // Broadcast the publish points
        UDP_Server(msg);

        // Time control
        while (T_INTERVAL < DT) {
            clock_gettime(CLOCK_MONOTONIC, &EndingTime);
            T_INTERVAL = (EndingTime.tv_sec - StartingTime.tv_sec) + (EndingTime.tv_nsec - StartingTime.tv_nsec) / 1e9;
        }
        clock_gettime(CLOCK_MONOTONIC, &StartingTime);
        T_INTERVAL = 0;

        // Hold time system
        pthread_mutex_lock(&FLAG_Mutx);
        SHM_FLAGS = FLAGS;
        pthread_mutex_unlock(&FLAG_Mutx);
        if (SHM_FLAGS.Hold_Time_Flag){
            while (SHM_FLAGS.Hold_Time_Flag){
                pthread_mutex_lock(&FLAG_Mutx);
                SHM_FLAGS = FLAGS;
                pthread_mutex_unlock(&FLAG_Mutx);

                if (nanosleep(&request, &remaining) == -1){
                    printf("nanosleep failure\n");
                }

                STOP = Sem_Stop();
                if (STOP > 0) {
                    break;
                }
            }
        }

        sem_post(up);
    }

    UDP_Stop();
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "STOP\nSTOP\n");

    shmdt(PUB_DB);
    shmdt(UP_DB);
    shmctl(shmidpub, IPC_RMID, NULL);
    shmctl(shmidup, IPC_RMID, NULL);
    clearQueue(UP_DATA_QUEUE);
    clearQueue(PUB_DATA_QUEUE);
    Set_Stop();

    return 0;
}