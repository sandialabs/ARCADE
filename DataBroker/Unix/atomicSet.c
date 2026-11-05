#include "atomicSet.h"

#include <stdatomic.h>

static atomic_bool error_flag = false;

void setErrorFlag(void)
{
    atomic_store(&error_flag, true);
}

void setErrorFlagFalse(void)
{
    atomic_store(&error_flag, false);
}

bool checkErrorFlag(void)
{
    return atomic_load(&error_flag);
}
