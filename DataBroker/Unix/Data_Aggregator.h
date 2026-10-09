#pragma once

#include "Shm_Interface.h"

#define MSG_BUFFER 256

/// Data with a timestamp attached.
typedef struct {
    /// Data enqueued.
    DATA data;

    /// Time the data was acquired at.
    struct timespec realTime;
} Timestamped_Data;

/// Queue element.
typedef struct Node {
    /// Data recorded.
    DATA data;

    /// Real time at which the data was recorded.
    struct timespec realTime;

    /// Next element of the queue.
    struct Node *next;
} Node;

/// Queue data structure.
typedef struct Queue {
    /// Front of the queue.
    Node *front;

    /// Rear of the queue.
    Node *rear;

    /// Lock used to control access to the queue.
    pthread_mutex_t lock;
} Queue;

/// Start the data aggregation thread.
void *Data_Aggregator(void *);

/// Enqueue a data object.
void enqueue(Queue *q, DATA value);

/// Dequeue a data object.
Timestamped_Data dequeue(Queue *q);

/// Check if the queue is empty.
int isEmpty(Queue *q);

/// Free the queue and all remaining elements.
void clearQueue(Queue *q);

/// Allocate a new queue.
///
/// The caller is responsible for freeing the memory using
/// `clearQueue`. Returns NULL on failure.
Queue *createQueue(void);

/// Signal the data aggregation thread that data is done being produced.
void Finish_Logging(void);

/// Clean up after the data aggregation thread.
void Cleanup_Logging(void);

extern Queue *UP_DATA_QUEUE;
extern Queue *PUB_DATA_QUEUE;
