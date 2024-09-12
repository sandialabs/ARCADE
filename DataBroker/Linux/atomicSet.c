#include "atomicSet.h"
#include <stdatomic.h>

atomic_flag errAtomicFlag = ATOMIC_FLAG_INIT;

void setErrorFlag(void) {
    atomic_flag_test_and_set(&errAtomicFlag);
    printf("Atomic Set to True\n");
}

void setErrorFlagFalse(void) {
    printf("I was called\n");
    atomic_flag_clear(&errAtomicFlag);
}

bool checkErrorFlag(void) {
    bool was_set = atomic_flag_test_and_set(&errAtomicFlag);
    if (!was_set) {
        atomic_flag_clear(&errAtomicFlag);
    }
    return was_set;
}
