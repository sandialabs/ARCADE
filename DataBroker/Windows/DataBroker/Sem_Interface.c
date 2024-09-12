#include "Sem_Interface.h"

//setup semaphores and bring them to 0
void Sem_Interface(void)
{
	/*
	if (win_check == 0) {
		// initialization

		pub = sem_open("/pp_sem", O_CREAT, 0644, 0);
		up = sem_open("/up_sem", O_CREAT, 0644, 0);
		stop = sem_open("/stop", O_CREAT, 0644, 0);
		msg_sem = sem_open("/msg", O_CREAT, 0644, 0);

		// Loop over semaphore values unitl they equal zero
		Sem_posix_setval(pub);
		Sem_posix_setval(up);
		Sem_posix_setval(stop);
		Sem_posix_setval(msg_sem);
	}
	else {
		*/
	// Create windows semaphores
	up = CreateSemaphore(NULL, 0, 1, "Global\\up_DB_sem");
	pub = CreateSemaphore(NULL, 0, 1, "Global\\pub_DB_sem");
	stop = CreateSemaphore(NULL, 0, 10, "Global\\stop_DB_sem");
	msg_sem = CreateSemaphore(NULL, 0, 1, "Global\\msg_DB_sem");

	// Set them to 0 if not already
	Sem_win_setval(up);
	Sem_win_setval(pub);
	Sem_win_setval(stop);
	Sem_win_setval(msg_sem);

	
}

void Sem_win_setval(HANDLE *sem) {
	DWORD Sem_check;
	while (1) {
		Sem_check = WaitForSingleObject(sem, 0L);
		if (Sem_check == WAIT_TIMEOUT) {
			break;
		}	
	}
}

/*
void Sem_posix_setval(void* sem) {
	int ST;
	int flag;
	
	while (1) {
		sem_getvalue(sem, &ST);
		flag = (int)ST;
		if (flag > 0) {
			sem_trywait(sem);
		}
		if (flag == 0) {
			break;
		}
	}

}
*/
