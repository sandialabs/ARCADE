#pragma once

#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <zmq.h>
#include <semaphore.h>

/// Retry `expr` on `EINTR`, placing the result into `result`.
#define RETRY_EINTR(result, expr) \
    do {                          \
        (result) = (expr);        \
    } while ((result) == -1 && errno == EINTR)

/// Sleep with a delay in milliseconds.
int sleep_ms(long ms);

/// Convert a string to a boolean.
bool strtobool(char *string);

/// Return values for `sem_wait_safe` and `zmq_exchange`.
enum { WAIT_ERROR = -1,
       WAIT_OK = 0,
       WAIT_STOPPED = 1 };

/// Wait on a semaphore.
///
/// `wait_limit` is specified in seconds, with zero disabling the deadline. One
/// of `WAIT_ERROR`, `WAIT_OK`, or `WAIT_STOPPED` will be returned.
int sem_wait_safe(sem_t *sem, int wait_limit);

/// Configure a ZeroMQ socket. Returns zero on success or -1 on error.
int zmq_configure(void *socket);

/// Exchange a message over a ZeroMQ socket and wait for a reply.
///
/// `timeout` is specified in seconds, with zero disabling it.
/// One of `WAIT_ERROR`, `WAIT_OK`, or `WAIT_STOPPED` will be returned. The
/// caller is responsible for initializing and closing `reply`. Close the socket
/// on error.
int zmq_exchange(void *socket, const char *request, zmq_msg_t *reply, int timeout);

/// Flags controlling simulation disposition.
typedef struct {
    /// Whether or not the simulation is to be paused.
    bool Hold_Time_Flag;
} Special_Flags;

/// Global containing the simulation disposition.
extern Special_Flags FLAGS;

// Simulation configuration.
typedef struct {
    /// Whether co-simulation using ZMQ is enabled.
    bool Co_Sim_Enable;

    /// Whether to synchronize co-simulation.
    bool Co_Sim_Sync_Enable;

    /// Whether to log the time at which data is received.
    bool Realtime_Timestep;

    /// Number of update points.
    int UP_N;

    /// Number of publish points.
    int PUB_N;

    /// Duration between timesteps in the simulation.
    double TimeStep;

    /// Whether or not the configuration has been captured.
    bool config_captured;

    /// Endpoint timeout in seconds.
    ///
    /// Zero disables the deadline.
    int Endpoint_Timeout;

    /// Publication timeout in seconds.
    ///
    /// Zero disables the deadline.
    int Publish_Timeout;

    /// Co-simulation timeout in seconds.
    ///
    /// Zero disables the deadline.
    int Exchange_Timeout;
} Configs;

/// Global containing the configuration.
extern Configs CONF;

/// Mutex guarding `FLAGS` and `CONF`.
extern pthread_mutex_t FLAG_Mutx;
