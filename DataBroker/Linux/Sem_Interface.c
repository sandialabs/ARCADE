#include "Sem_Interface.h"

//setup semaphores and bring them to 0
void Sem_Interface(void)
{
	// initialization
	int publish, update;
	int ST;
	int flag;
	sem_t *up;
	sem_t *pub;
	sem_t *stop;
	sem_t *msg_sem;

	pub = sem_open("/pp_sem", O_CREAT, 0644, 0);
	up = sem_open("/up_sem", O_CREAT, 0644, 0);
	stop = sem_open("/stop", O_CREAT, 0644, 0);
	msg_sem = sem_open("/msg", O_CREAT, 0644, 0);

	// Loop over semaphore values unitl they equal zero
	while (1)
	{
		sem_getvalue(pub, &publish);
		flag = (int)publish;
		if (flag > 0)
		{
			sem_trywait(pub);
		}
		if (flag == 0)
		{
			break;
		}
	}
	while (1)
	{
		sem_getvalue(up, &update);
		flag = (int)update;
		if (flag > 0)
		{
			sem_trywait(up);
		}
		if (flag == 0)
		{
			break;
		}
	}
	while (1)
	{
		sem_getvalue(stop, &ST);
		flag = (int)ST;
		if (flag > 0)
		{
			sem_trywait(stop);
		}
		if (flag == 0)
		{
			break;
		}
	}
	while (1)
	{
		sem_getvalue(msg_sem, &ST);
		flag = (int)ST;
		if (flag > 0)
		{
			sem_trywait(msg_sem);
		}
		if (flag == 0)
		{
			break;
		}
	}
}