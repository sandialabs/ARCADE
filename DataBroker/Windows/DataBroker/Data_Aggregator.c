#include "Shm_Interface.h"
#include "Data_Aggregator.h"
#include "Sem_Stop.h"
#include "atomicSet.h"

DWORD DB_WaitResult;
int STOP = 0;
DATA UP_DATA_OUT[MAX_IO];

HANDLE init_Map_File;
HANDLE semaphore_Write;
HANDLE semaphore_Done;
MSG_DATA* init_Values;

Queue* UP_DATA_QUEUE;
Queue* PUB_DATA_QUEUE;

DWORD WINAPI Data_Aggregator(LPVOID arg)
{
    TCHAR init_shm_name[] = TEXT("Local\\DA_init_shm");
    UP_DATA_QUEUE = createQueue();
    PUB_DATA_QUEUE = createQueue();

    DATA_Mutx = OpenMutex(SYNCHRONIZE, FALSE, "Global\\DB_DATA_Mutx");
    if (DATA_Mutx == NULL) {
        fprintf(stderr, "Failed to get DATA Mutex.");
        Set_Stop();
        setErrorFlag();
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return -1;
    }

    init_Map_File = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MSG_DATA), init_shm_name);

    if (init_Map_File == NULL) {
        printf("Could not create file mampping object (%d).\n", GetLastError());
        Set_Stop();
        setErrorFlag();
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return -1;
    }

    init_Values = (MSG_DATA*)MapViewOfFile(init_Map_File, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MSG_DATA));

    if (init_Values == NULL) {
        printf("Could not map view of file (%d).\n", GetLastError());
        CloseHandle(init_Map_File);
        Set_Stop();
        setErrorFlag();
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return -1;
    }

    semaphore_Write = CreateSemaphore(NULL, 0, 1, "Local\\SemaphoreWrite");
    semaphore_Done = CreateSemaphore(NULL, 0, 1, "Local\\SemaphoreDone");
    printf("Semaphores created by Data_Aggregator\n");


    if (semaphore_Write == NULL || semaphore_Done == NULL) {
        printf("Could not create semaphore (%d).\n", GetLastError());
        Set_Stop();
        setErrorFlag();
        UnmapViewOfFile(init_Values);
        CloseHandle(init_Map_File);
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        return -1;
    }

    //Initialize the values
    init_Values->PUB = 0;
    init_Values->UP = 0;
    init_Values->TimeStep = 0.0;
    //Notify the Shm_Interface to set the values
    ReleaseSemaphore(semaphore_Write, 1, NULL);

    //Wait for Shm_Interface to notify the values have been written
    WaitForSingleObject(semaphore_Done, INFINITE);

    int PUB = init_Values->PUB;
    int UP = init_Values->UP;
    double TimeStep = init_Values->TimeStep;

    printf("Recieved from Shm_Interface: PUB = %d, UP = %d, TimeStep = %f\n", init_Values->PUB, init_Values->UP, init_Values->TimeStep);
    // Clean up
    UnmapViewOfFile(init_Values);
    CloseHandle(init_Map_File);
    CloseHandle(semaphore_Done);
    CloseHandle(semaphore_Write);

    FILE* up_file = fopen("up_data.csv", "w");
    if (up_file == NULL) {
        printf("Error opening file!\n");
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        setErrorFlag();
        return -1;
    }

    FILE* pub_file = fopen("pub_data.csv", "w");
    if (pub_file == NULL) {
        printf("Error opening file!\n");
        clearQueue(UP_DATA_QUEUE);
        clearQueue(PUB_DATA_QUEUE);
        fclose(up_file);
        setErrorFlag();
        return -1;
    }
    //Write column headers
    fprintf(up_file, "Name,Value,Time\n");
    fprintf(pub_file, "Name,Value,Time\n");
    
    while (1) {
        if (!isEmpty(PUB_DATA_QUEUE)) {
            DATA new_log = dequeue(PUB_DATA_QUEUE);
            fprintf(pub_file, "%s,%f,%f\n", new_log.Name, new_log.Value, new_log.Time);
            //sprintf("%s %s %f %f sec \n", new_log.Name, new_log.Type, new_log.Value, new_log.Time);
        }

        if (!isEmpty(UP_DATA_QUEUE)) {
            DATA new_log = dequeue(UP_DATA_QUEUE);
            fprintf(up_file, "%s,%f,%f\n", new_log.Name, new_log.Value, new_log.Time);
            //sprintf("%s %s %f %f sec \n", new_log.Name, new_log.Type, new_log.Value, new_log.Time);
        }
        //Check stop semaphore
       if (checkErrorFlag() == true) {
            printf("Error Flag set to True checked");
        }
       STOP = Sem_Stop();
       if (STOP > 0)
       {
           break;
       }
    }
    while (!isEmpty(PUB_DATA_QUEUE) || !isEmpty(UP_DATA_QUEUE)) {
        if (!isEmpty(PUB_DATA_QUEUE)) {
            DATA new_log = dequeue(PUB_DATA_QUEUE);
            fprintf(pub_file, "%s,%f,%f\n", new_log.Name, new_log.Value, new_log.Time);
            //sprintf("%s %s %f %f sec \n", new_log.Name, new_log.Type, new_log.Value, new_log.Time);
        }

        if (!isEmpty(UP_DATA_QUEUE)) {
            DATA new_log = dequeue(UP_DATA_QUEUE);
            fprintf(up_file, "%s,%f,%f\n", new_log.Name, new_log.Value, new_log.Time);
            //sprintf("%s %s %f %f sec \n", new_log.Name, new_log.Type, new_log.Value, new_log.Time);
        }
    }
    //clearQueue(UP_DATA_QUEUE);
    //clearQueue(PUB_DATA_QUEUE);
    fclose(up_file);
    fclose(pub_file);
    printf("Test");
    Sleep(1000);
    return 0;

}

Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (q == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    q->front = q->rear = NULL;
    InitializeCriticalSection(&q->lock);
    return q;
}

void enqueue(Queue* q, DATA value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = value;
    newNode->next = NULL;

    EnterCriticalSection(&q->lock);

    if (q->rear == NULL) {
        // If queue is empty, new node is both front and rear
        q->front = q->rear = newNode;
    }
    else {
        // Add the new node at the end of queue and change rear
        q->rear->next = newNode;
        q->rear = newNode;
    }

    LeaveCriticalSection(&q->lock);
}

DATA dequeue(Queue* q) {
    EnterCriticalSection(&q->lock);
    if (q->front == NULL) {
        fprintf(stderr, "Queue is empty\n");
        LeaveCriticalSection(&q->lock);
        return;  // Or other error code
    }

    Node* temp = q->front;
    DATA data = temp->data;
    q->front = q->front->next;

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    LeaveCriticalSection(&q->lock);
    return data;
}

int isEmpty(Queue* q) {
    return q->front == NULL;
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
    //free(q); //freeing causes program to hang on exit
    DeleteCriticalSection(&q->lock);
}

