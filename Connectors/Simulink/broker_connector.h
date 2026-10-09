#pragma once

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>

#include "../../DataBroker/ipc.h"

/// Initial value of update points.
#define BROKER_INITIAL_VALUE -100000000000000.0

/// Connection to the Data Broker.
typedef struct {
    /// Publish semaphore.
    sem_t *pub;

    /// Update semaphore.
    sem_t *up;

    /// Metadata semaphore.
    sem_t *msg;

    /// Stop semaphore.
    sem_t *stop;

    /// Publish points.
    DATA *published;

    /// Update points.
    DATA *updated;

    /// Simulation metadata.
    MSG_DATA *metadata;

    /// Number of publish points.
    int n_pub;

    /// Number of update points.
    int n_up;
} Broker_Connector;

/// Count and validate tags.
///
/// Returns -1 on error.
static inline int Broker_Tag_Count(const char *tags)
{
    if (!tags) {
        errno = EINVAL;
        return -1;
    }
    if (!*tags) {
        return 0;
    }
    int count = 0;
    const char *start = tags;
    for (;;) {
        const char *end = strchr(start, ';');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        if (!len || len >= sizeof(((DATA *)0)->Name) || ++count > MAX_IO) {
            errno = EINVAL;
            return -1;
        }
        for (size_t i = 0; i < len; ++i) {
            if (isspace((unsigned char)start[i]) || start[i] == ',' || start[i] == ':') {
                errno = EINVAL;
                return -1;
            }
        }
        if (!end) {
            return count;
        }
        start = end + 1;
    }
}

/// Initialize data records.
///
/// Returns zero on success or -1 on error.
static inline int Broker_Set_Tags(DATA *data, int count, const char *tags, bool updated)
{
    if (count < 0 || Broker_Tag_Count(tags) != count || (count && !data)) {
        errno = EINVAL;
        return -1;
    }
    if (!count) {
        return 0;
    }
    memset(data, 0, (size_t)count * sizeof(DATA));
    const char *start = tags;
    for (int i = 0; i < count; ++i) {
        const char *end = strchr(start, ';');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        memcpy(data[i].Name, start, len);
        strcpy(data[i].Type, "DOUBLE");
        data[i].Value = updated ? BROKER_INITIAL_VALUE : 0;
        for (int j = 0; j < i; ++j) {
            if (!strcmp(data[j].Name, data[i].Name)) {
                errno = EINVAL;
                return -1;
            }
        }
        start = end ? end + 1 : start + len;
    }
    return 0;
}

/// Detach memory and close semaphore handles.
static inline void Broker_Close(Broker_Connector *connection)
{
    if (connection->published) {
        shmdt(connection->published);
        connection->published = NULL;
    }
    if (connection->updated) {
        shmdt(connection->updated);
        connection->updated = NULL;
    }
    if (connection->metadata) {
        shmdt(connection->metadata);
        connection->metadata = NULL;
    }
    sem_t **handles[] = {&connection->pub, &connection->up, &connection->msg, &connection->stop};
    for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); ++i) {
        if (*handles[i] != SEM_FAILED) {
            sem_close(*handles[i]);
            *handles[i] = SEM_FAILED;
        }
    }
}

/// Attach a shared memory segment. Returns NULL on error.
static inline void *Broker_Attach(key_t key, size_t size)
{
    int id = shmget(key, size, IPC_CREAT | 0600);
    if (id < 0) {
        return NULL;
    }
    void *data = shmat(id, NULL, 0);
    return data == (void *)-1 ? NULL : data;
}

/// Open the connection. Returns zero on success or -1 on error.
///
/// Call Broker_Close after use. Initialization is announced separately.
static inline int Broker_Open(Broker_Connector *connection, const char *pub_tags,
                              const char *up_tags, double dt)
{
    memset(connection, 0, sizeof(*connection));
    connection->pub = connection->up = connection->msg = connection->stop = SEM_FAILED;
    connection->n_pub = Broker_Tag_Count(pub_tags);
    connection->n_up = Broker_Tag_Count(up_tags);
    if (connection->n_pub < 0 || connection->n_up < 0 || !isfinite(dt) || dt < 0) {
        errno = EINVAL;
        return -1;
    }
    connection->pub = sem_open("/pp_sem", O_CREAT, 0644, 0);
    connection->up = sem_open("/up_sem", O_CREAT, 0644, 0);
    connection->msg = sem_open("/msg", O_CREAT, 0644, 0);
    connection->stop = sem_open("/stop", O_CREAT, 0644, 0);
    if (connection->pub == SEM_FAILED || connection->up == SEM_FAILED ||
        connection->msg == SEM_FAILED || connection->stop == SEM_FAILED) {
        goto error;
    }
    if (connection->n_pub) {
        connection->published = Broker_Attach(10618, (size_t)connection->n_pub * sizeof(DATA));
        if (!connection->published || Broker_Set_Tags(connection->published, connection->n_pub, pub_tags, false)) {
            goto error;
        }
    }
    if (connection->n_up) {
        connection->updated = Broker_Attach(10619, (size_t)connection->n_up * sizeof(DATA));
        if (!connection->updated || Broker_Set_Tags(connection->updated, connection->n_up, up_tags, true)) {
            goto error;
        }
    }
    connection->metadata = Broker_Attach(10620, sizeof(MSG_DATA));
    if (!connection->metadata) {
        goto error;
    }
    *connection->metadata = (MSG_DATA){connection->n_pub, connection->n_up, dt};
    return 0;
error: {
    int saved = errno;
    Broker_Close(connection);
    errno = saved;
}
    return -1;
}

/// Check the stop semaphore.
///
/// Returns one if stopped or on error.
static inline int Broker_Stopped(Broker_Connector *connection)
{
    if (connection->stop == SEM_FAILED) {
        return 1;
    }
    int rc;
    do {
        rc = sem_trywait(connection->stop);
    } while (rc < 0 && errno == EINTR);
    if (!rc) {
        sem_post(connection->stop);
        return 1;
    }
    return errno == EAGAIN ? 0 : 1;
}

/// Wait for an update.
///
/// Returns zero, one if stopped, or -1 on error.
static inline int Broker_Wait(Broker_Connector *connection)
{
    struct timespec delay = {0, 10000000L};
    for (;;) {
        if (Broker_Stopped(connection)) {
            return 1;
        }
        if (!sem_trywait(connection->up)) {
            return Broker_Stopped(connection) ? 1 : 0;
        }
        if (errno != EAGAIN && errno != EINTR) {
            return -1;
        }
        nanosleep(&delay, NULL);
    }
}

/// Initialize work values and outputs.
static inline void Broker_Initial_Values(double *work, double *output, int count)
{
    for (int i = 0; i < count; ++i) {
        work[i] = output[i] = BROKER_INITIAL_VALUE;
    }
}
