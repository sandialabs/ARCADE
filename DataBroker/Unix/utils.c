#include "utils.h"

#include <strings.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <ctype.h>

#include "Sem_Stop.h"

Special_Flags FLAGS;
Configs CONF;

pthread_mutex_t FLAG_Mutx;

int sleep_ms(long ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    int r;
    RETRY_EINTR(r, nanosleep(&ts, &ts));
    return r;
}

bool strtobool(char *string)
{
    return string && strcasecmp(string, "true") == 0;
}

static double elapsed(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec - start.tv_sec + (now.tv_nsec - start.tv_nsec) / 1e9;
}

int sem_wait_safe(sem_t *sem, int wait_limit)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        if (Sem_Stop()) {
            return WAIT_STOPPED;
        }
        if (sem_trywait(sem) == 0) {
            return Sem_Stop() ? WAIT_STOPPED : WAIT_OK;
        }
        if (errno != EAGAIN && errno != EINTR) {
            return WAIT_ERROR;
        }
        if (wait_limit > 0 && elapsed(start) >= wait_limit) {
            errno = ETIMEDOUT;
            return WAIT_ERROR;
        }
        sleep_ms(10);
    }
}

int zmq_configure(void *socket)
{
    int timeout = 100, linger = 0;
    if (zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout)) ||
        zmq_setsockopt(socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout)) ||
        zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger))) {
        return -1;
    }
    return 0;
}

int zmq_exchange(void *socket, const char *request, zmq_msg_t *reply, int timeout)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int sent = 0;
    for (;;) {
        if (Sem_Stop()) {
            return WAIT_STOPPED;
        }
        int n = sent ? zmq_msg_recv(reply, socket, 0)
                     : zmq_send(socket, request, strlen(request), 0);
        if (n >= 0) {
            if (sent) {
                return Sem_Stop() ? WAIT_STOPPED : WAIT_OK;
            }
            sent = 1;
        } else if (errno != EAGAIN && errno != EINTR) {
            return WAIT_ERROR;
        }
        if (timeout > 0 && elapsed(start) >= timeout) {
            errno = ETIMEDOUT;
            return WAIT_ERROR;
        }
    }
}
