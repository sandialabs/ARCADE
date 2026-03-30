#include "Sem_Stop.h"
#include "atomicSet.h"

// Static handle to avoid opening repeatedly
static HANDLE persistent_stop_handle = NULL;

// Initialize the stop semaphore handle once
void Init_Stop_Semaphore(void) {
    if (win_check != 0) {
        persistent_stop_handle = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\stop_DB_sem");
        if (persistent_stop_handle == NULL) {
            fprintf(stderr, "Failed to open stop semaphore during initialization.\n");
            DWORD dwError = GetLastError();
            printf("OpenSemaphore failed with error %lu\n", dwError);
        }
        else {
            printf("Stop semaphore opened successfully\n");
        }
    }
}

// Cleanup the stop semaphore handle
void Cleanup_Stop_Semaphore(void) {
    if (persistent_stop_handle != NULL) {
        CloseHandle(persistent_stop_handle);
        persistent_stop_handle = NULL;
    }
}

//utility to check the simulations stop flag semaphore
int Sem_Stop(void)
{
    if (win_check == 0) {
        return 0; // Linux path not implemented
    }
    else {
        int value = 0;

        // Use persistent handle instead of opening every time
        if (persistent_stop_handle == NULL) {
            fprintf(stderr, "Stop semaphore not initialized. Call Init_Stop_Semaphore() first.\n");
            return 0;
        }

        DWORD Sem_check;
        int release_check = 0;
        Sem_check = WaitForSingleObject(persistent_stop_handle, 0L);
        if (Sem_check == WAIT_OBJECT_0) {
            value = 1;
            while (!ReleaseSemaphore(persistent_stop_handle, 1, NULL)) {
                if (++release_check > 5) {
                    fprintf(stderr, "Failed to release stop semaphore.");
                    break;
                }
            }
        }
        return value;
    }
}

void Set_Stop(void) {
    printf("Set Stop Called");
    setErrorFlag();

    if (win_check == 0) {
        return;
    }

    // Use persistent handle instead of opening
    if (persistent_stop_handle == NULL) {
        fprintf(stderr, "Stop semaphore not initialized.\n");
        return;
    }

    int release_check = 0;
    while (!ReleaseSemaphore(persistent_stop_handle, 5, NULL)) {
        if (++release_check > 5) {
            fprintf(stderr, "Failed to release stop semaphore.");
            break;
        }
    }
}