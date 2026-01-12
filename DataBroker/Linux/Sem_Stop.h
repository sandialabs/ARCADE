#ifndef SEM_STOP_H
#define SEM_STOP_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>

extern sem_t* stop;

int Sem_Stop(void);
void Set_Stop(void);

#endif // SEM_STOP_H
