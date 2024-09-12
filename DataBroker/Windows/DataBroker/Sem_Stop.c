#include "Sem_Stop.h"
#include "atomicSet.h"

//utility to check the simulations stop flag semaphore
int Sem_Stop(void)
{
	if (win_check == 0) {
		/*int ST;
		stop = sem_open("/stop", O_CREAT, 0644, 0);
		sem_getvalue(stop, &ST);
		int value = (int)ST;
		return value;*/
	}
	else {
		int value = 0;
		stop = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\stop_DB_sem");
		if (stop == NULL) {
			fprintf(stderr, "Failed to open stop semaphore.\n");
			DWORD dwError = GetLastError();
			printf("OpenSemaphore failed with error %lu\n", dwError);
		}
		else {
			DWORD Sem_check;
			int release_check = 0;
			Sem_check = WaitForSingleObject(stop, 0L);
			if (Sem_check == WAIT_OBJECT_0) {
				value = 1;
				while (!ReleaseSemaphore(stop, 1, NULL)) {
					if (++release_check > 5) {
						fprintf(stderr, "Failed to release stop semaphore.");
						break;
					}
				}
			}
		}
		return value;
	}
}

void Set_Stop(void) {
	printf("Set Stop Called");
	setErrorFlag();
	stop = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, "Global\\stop_DB_sem");
	if (stop == NULL) {
		fprintf(stderr, "Failed to open stop semaphore.\n");
	}
	else {
		int release_check = 0;
		while (!ReleaseSemaphore(stop, 5, NULL)) {
			if (++release_check > 5) {
				fprintf(stderr, "Failed to release stop semaphore.");
				break;
			}
		}
	}
}