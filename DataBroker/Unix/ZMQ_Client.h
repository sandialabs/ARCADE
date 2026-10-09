#pragma once

/// Maximum number of outputs.
#define MAX_ZMQ_VARS 512

/// Mapping of string names to output indices.
typedef struct {
    /// Name of output.
    char tag[128];

    /// Index of output inside the `PUB_DATA` structure.
    int index;
} ZMQ_TagMap;

/// Function to run the ZeroMQ thread.
void *ZMQ_Client(void *);

/// Reserve configured inputs. Returns zero on success or -1 on error.
int CoSim_ReserveInputs(int count);
