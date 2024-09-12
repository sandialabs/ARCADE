#ifndef atomicSet
#define atomicSet

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

void setErrorFlag(void);
bool checkErrorFlag(void);
void setErrorFlagFalse(void);

extern atomic_flag errAtomicFlag;

#endif // !atomicSet
