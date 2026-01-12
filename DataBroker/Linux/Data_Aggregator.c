#include "Data_Aggregator.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "atomicSet.h"
#include "utils.h"

int STOP = 0;
DATA UP_DATA_OUT[MAX_IO];

sem_t* semaphore_Write;
sem_t* semaphore_Done;
MSG_DATA* init_Values;

Queue* UP_DATA_QUEUE;
Queue* PUB_DATA_QUEUE;

void* Data_Aggregator(void* arg) {
    key_t pass_init_key = 10621; // shared memory pass init key
    UP_DATA_QUEUE = createQueue();
    PUB_DATA_QUEUE = createQueue();

    int pass_shmid = shmget(pass_init_key, sizeof(MSG_DATA), 0666 | IPC_CREAT);
    if (pass_shmid == -1) {
        perror("shmget");
        Set_Stop();
        setErrorFlag();
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return (void*)-1;
    }

    init_Values = (MSG_DATA*)shmat(pass_shmid, NULL, 0);
    if (init_Values == (void*)-1) {
        perror("shmat");
        Set_Stop();
        setErrorFlag();
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return (void*)-1;
    }

    semaphore_Write = sem_open("/SemaphoreWrite", O_CREAT, 0644, 0);
    semaphore_Done = sem_open("/SemaphoreDone", O_CREAT, 0644, 0);
    printf("Semaphores created by Data_Aggregator\n");

    if (semaphore_Write == SEM_FAILED || semaphore_Done == SEM_FAILED) {
        perror("sem_open");
        Set_Stop();
        setErrorFlag();
        shmdt(init_Values);
        shmctl(pass_shmid, IPC_RMID, NULL);
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return (void*)-1;
    }

    init_Values->PUB = 0;
    init_Values->UP = 0;
    init_Values->TimeStep = 0.0;
    sem_post(semaphore_Write);
    printf("DA WAITING ON SHM");
    sem_wait(semaphore_Done);

    int PUB = init_Values->PUB;
    int UP = init_Values->UP;
    double TimeStep = init_Values->TimeStep;

    printf("Received from Shm_Interface: PUB = %d, UP = %d, TimeStep = %f\n", init_Values->PUB, init_Values->UP, init_Values->TimeStep);

    shmdt(init_Values);
    shmctl(pass_shmid, IPC_RMID, NULL);
    sem_close(semaphore_Done);
    sem_close(semaphore_Write);

    FILE* up_file = fopen("up_data.csv", "w");
    if (up_file == NULL) {
        perror("fopen");
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        setErrorFlag();
        return (void*)-1;
    }

    FILE* pub_file = fopen("pub_data.csv", "w");
    if (pub_file == NULL) {
        perror("fopen");
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        fclose(up_file);
        setErrorFlag();
        return (void*)-1;
    }

    if (CONF.Realtime_Timestep) {
        fprintf(up_file, "Name,Value,Time,RealTime\n");
        fprintf(pub_file, "Name,Value,Time,RealTime\n");
    }
    else {
        fprintf(up_file, "Name,Value,Time\n");
        fprintf(pub_file, "Name,Value,Time\n");
    }

    while (1) {
        if (!isEmpty(PUB_DATA_QUEUE)) {
            Timestamped_Data new_log = dequeue(PUB_DATA_QUEUE);
            if (CONF.Realtime_Timestep) {
                fprintf(pub_file, "%s,%f,%f,%ld\n", new_log.data.Name, new_log.data.Value, new_log.data.Time, new_log.realTime.tv_sec);
            }
            else {
                fprintf(pub_file, "%s,%f,%f\n", new_log.data.Name, new_log.data.Value, new_log.data.Time);
            }
        }

        if (!isEmpty(UP_DATA_QUEUE)) {
            Timestamped_Data new_log = dequeue(UP_DATA_QUEUE);
            if (CONF.Realtime_Timestep) {
                fprintf(up_file, "%s,%f,%f,%ld\n", new_log.data.Name, new_log.data.Value, new_log.data.Time, new_log.realTime.tv_sec);
            }
            else {
                fprintf(up_file, "%s,%f,%f\n", new_log.data.Name, new_log.data.Value, new_log.data.Time);
            }
        }

        if (checkErrorFlag()) {
            printf("Error Flag set to True checked");
            Set_Stop();
        }
        STOP = Sem_Stop();
        if (STOP > 0) {
            break;
        }
    }

    while (!isEmpty(PUB_DATA_QUEUE) || !isEmpty(UP_DATA_QUEUE)) {
        if (!isEmpty(PUB_DATA_QUEUE)) {
            Timestamped_Data new_log = dequeue(PUB_DATA_QUEUE);
            if (CONF.Realtime_Timestep) {
                fprintf(pub_file, "%s,%f,%f,%ld\n", new_log.data.Name, new_log.data.Value, new_log.data.Time, new_log.realTime.tv_sec);
            }
            else {
                fprintf(pub_file, "%s,%f,%f\n", new_log.data.Name, new_log.data.Value, new_log.data.Time);
            }
        }

        if (!isEmpty(UP_DATA_QUEUE)) {
            Timestamped_Data new_log = dequeue(UP_DATA_QUEUE);
            if (CONF.Realtime_Timestep) {
                fprintf(up_file, "%s,%f,%f,%ld\n", new_log.data.Name, new_log.data.Value, new_log.data.Time, new_log.realTime.tv_sec);
            }
            else {
                fprintf(up_file, "%s,%f,%f\n", new_log.data.Name, new_log.data.Value, new_log.data.Time);
            }
        }
    }

    fclose(up_file);
    fclose(pub_file);
    printf("Test");
    sleep(1);
    return 0;
}

Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (q == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    q->front = q->rear = NULL;
    pthread_mutex_init(&q->lock, NULL);
    return q;
}

void enqueue(Queue* q, DATA value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    newNode->realTime = ts;
    newNode->data = value;
    newNode->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    }
    else {
        q->rear->next = newNode;
        q->rear = newNode;
    }

    pthread_mutex_unlock(&q->lock);
}

Timestamped_Data dequeue(Queue* q) {
    pthread_mutex_lock(&q->lock);

    Timestamped_Data TSData;

    if (q->front == NULL) {
        fprintf(stderr, "Queue is empty\n");
        pthread_mutex_unlock(&q->lock);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        TSData.realTime = ts;
        DATA empty_data = { "", 0.0, 0.0 };
        TSData.data = empty_data;
        return TSData;
    }

    Node* temp = q->front;
    TSData.realTime = temp->realTime;
    TSData.data = temp->data;
    q->front = q->front->next;

    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    pthread_mutex_unlock(&q->lock);
    return TSData;
}

int isEmpty(Queue* q) {
    pthread_mutex_lock(&q->lock);
    int empty = (q->front == NULL);
    pthread_mutex_unlock(&q->lock);
    return empty;
}

void clearQueue(Queue* q) {
    Node* current = q->front;
    Node* next;

    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    q->front = NULL;
    q->rear = NULL;
    pthread_mutex_destroy(&q->lock);
    //free(q); causes a failure on exit
}
