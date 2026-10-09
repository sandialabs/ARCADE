#include "Data_Aggregator.h"

#include <stdatomic.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "atomicSet.h"
#include "utils.h"

static atomic_bool PRODUCER_DONE = false;
static pthread_mutex_t LOG_LOCK = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t LOG_READY = PTHREAD_COND_INITIALIZER;

DATA UP_DATA_OUT[MAX_IO];

Queue *UP_DATA_QUEUE;
Queue *PUB_DATA_QUEUE;

static void notify_logger(void)
{
    pthread_mutex_lock(&LOG_LOCK);
    pthread_cond_signal(&LOG_READY);
    pthread_mutex_unlock(&LOG_LOCK);
}

void Finish_Logging(void)
{
    pthread_mutex_lock(&LOG_LOCK);
    atomic_store(&PRODUCER_DONE, true);
    pthread_cond_signal(&LOG_READY);
    pthread_mutex_unlock(&LOG_LOCK);
}

void Cleanup_Logging(void)
{
    pthread_cond_destroy(&LOG_READY);
    pthread_mutex_destroy(&LOG_LOCK);
}

static void log_queue(FILE *file, Queue *queue)
{
    if (isEmpty(queue)) {
        return;
    }

    Timestamped_Data log = dequeue(queue);
    if (CONF.Realtime_Timestep) {
        fprintf(file, "%s,%f,%f,%ld\n", log.data.Name, log.data.Value,
                log.data.Time, log.realTime.tv_sec);
    } else {
        fprintf(file, "%s,%f,%f\n", log.data.Name, log.data.Value, log.data.Time);
    }
}

void *Data_Aggregator(void *_)
{
    FILE *up = fopen("up_data.csv", "w");
    FILE *pub = fopen("pub_data.csv", "w");

    if (!up || !pub) {
        perror("Opening data logs");
        if (up) {
            fclose(up);
        }
        if (pub) {
            fclose(pub);
        }
        Set_Stop();
        return (void *)-1;
    }
    const char *header = CONF.Realtime_Timestep ? "Name,Value,Time,RealTime\n"
                                                : "Name,Value,Time\n";

    // disable buffering to avoid masking disk errors
    setvbuf(up, NULL, _IOLBF, 0);
    setvbuf(pub, NULL, _IOLBF, 0);
    fputs(header, up);
    fputs(header, pub);

    int failed = 0;

    while (!atomic_load(&PRODUCER_DONE) || !isEmpty(PUB_DATA_QUEUE) ||
           !isEmpty(UP_DATA_QUEUE)) {
        log_queue(pub, PUB_DATA_QUEUE);
        log_queue(up, UP_DATA_QUEUE);
        if (ferror(up) || ferror(pub)) {
            perror("Writing data logs");
            Set_Stop();
            failed = 1;
            break;
        }
        if (checkErrorFlag()) {
            Set_Stop();
        }

        pthread_mutex_lock(&LOG_LOCK);
        while (!atomic_load(&PRODUCER_DONE) && isEmpty(PUB_DATA_QUEUE) &&
               isEmpty(UP_DATA_QUEUE)) {
            pthread_cond_wait(&LOG_READY, &LOG_LOCK);
        }
        pthread_mutex_unlock(&LOG_LOCK);
    }
    if (fclose(up) != 0) {
        failed = 1;
    }
    if (fclose(pub) != 0) {
        failed = 1;
    }
    printf("Data Aggregator exited.\n");
    return failed ? (void *)-1 : NULL;
}

Queue *createQueue(void)
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    if (q == NULL) {
        return NULL;
    }
    q->front = q->rear = NULL;
    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        free(q);
        return NULL;
    }
    return q;
}

void enqueue(Queue *q, DATA value)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        setErrorFlag();
        Set_Stop();
        return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    newNode->realTime = ts;
    newNode->data = value;
    newNode->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }

    pthread_mutex_unlock(&q->lock);
    notify_logger();
}

Timestamped_Data dequeue(Queue *q)
{
    pthread_mutex_lock(&q->lock);

    Timestamped_Data TSData;

    if (q->front == NULL) {
        fprintf(stderr, "Queue is empty\n");
        pthread_mutex_unlock(&q->lock);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        TSData.realTime = ts;
        DATA empty_data = {
            .Name = "",
            .Type = "",
            .Value = 0.0,
            .Time = 0.0,
        };
        TSData.data = empty_data;
        return TSData;
    }

    Node *temp = q->front;
    TSData.realTime = temp->realTime;
    TSData.data = temp->data;
    q->front = q->front->next;

    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    pthread_mutex_unlock(&q->lock);
    return TSData;
}

int isEmpty(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    int empty = (q->front == NULL);
    pthread_mutex_unlock(&q->lock);
    return empty;
}

void clearQueue(Queue *q)
{
    if (!q) {
        return;
    }

    Node *current = q->front;
    Node *next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    q->front = NULL;
    q->rear = NULL;
    pthread_mutex_destroy(&q->lock);
    free(q);
}
