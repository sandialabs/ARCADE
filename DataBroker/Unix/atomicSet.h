#pragma once

#include <stdbool.h>

/// Set the error flag atomically.
void setErrorFlag(void);

/// Check the error flag atomically.
bool checkErrorFlag(void);

/// Unset the error flag atomically.
void setErrorFlagFalse(void);
