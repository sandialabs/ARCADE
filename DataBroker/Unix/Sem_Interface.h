#pragma once

/// Acquire the per-user broker process lock before IPC access.
///
/// Returns 0 on success, -1 otherwise.
int Lock_Interface(void);

/// Release the broker process lock after IPC cleanup.
void Unlock_Interface(void);

/// Reset semaphores and prepare them to be used.
int Sem_Interface(void);

/// Clean up semaphores.
void Cleanup_Interface(void);
