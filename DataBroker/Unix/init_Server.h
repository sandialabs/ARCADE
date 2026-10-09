#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Load the configuration. Returns zero on success or -1 on error.
int Load_Config(void);

/// Clean up the configuration after workers have exited.
void Cleanup_Config(void);

/// Initialize endpoints. Returns zero on success or -1 on error.
int init_Server(void);

/// Helper for opening a file and memory-mapping it.
///
/// The address of the mapping is returned, and the length of the file is stored
/// in `len`.
///
/// The caller is responsible for calling `munmap(2)` to deallocate the memory
/// used for the file. Returns NULL on failure with `errno` set. `len` is then
/// zero.
char *OpenFile(const char *filename, size_t *len);

/// Get the executable of the simulator from the configuration.
///
/// Returned strings are owned by the caller and must be freed.
char *SimName(void);

/// Read a flag from the configuration.
bool Read_flags(const char *category, const char *flagname, bool fallback);

/// Read a variable from the configuration.
///
/// Returned strings are owned by the caller and must be freed.
/// Returns NULL on error or if no value or fallback is available.
char *Read_Vars(const char *category, const char *varname, const char *fallback);
