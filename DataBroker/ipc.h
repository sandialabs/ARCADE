#pragma once

/// Data received over shared memory.
typedef struct {
    /// Name of the value.
    char Name[128];

    /// Type of the value.
    char Type[50];

    /// Value.
    double Value;

    /// Simulation time at which the value was recorded.
    double Time;
} DATA;

/// Simulation metadata received over shared memory.
typedef struct {
    /// Number of publish points.
    int PUB;

    /// Number of update points.
    int UP;

    /// Duration between timesteps in the simulation.
    double TimeStep;
} MSG_DATA;

/// Upper limit of data that may be recorded in memory at once.
#define MAX_IO 1000
