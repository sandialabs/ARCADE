#include "ZMQ_Client.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "init_Server.h"
#include "utils.h"
#include "UDP_Server.h"

bool ZMQ_SEND_TRIGGERED = false;
bool ZMQ_RESPONSE_READY = false;
pthread_mutex_t ZMQ_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ZMQ_COND = PTHREAD_COND_INITIALIZER;
ZMQ_TagMap ZMQ_Inputs[MAX_ZMQ_VARS];
ZMQ_TagMap ZMQ_Outputs[MAX_ZMQ_VARS];
Configs CONF_FLAGS;
int ZMQ_PUB_LEN = 0;

void* ZMQ_Client(void* args) {
    
    // initalize the configs
    CoSim_init();

    DATA ZMQ_UP_DATA[MAX_IO];
    DATA ZMQ_PUB_DATA[MAX_IO];
    int n;

    
    char *current_key = NULL;
    char buffer[512];

    sem_t* co_sim = sem_open("/co_sim", O_CREAT, 0644, 0);
    if (co_sim == SEM_FAILED) {
        perror("Failed to open co-sim semaphore");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }
    sem_t* co_sim_2 = sem_open("/co_sim_2", O_CREAT, 0644, 0);
    if (co_sim == SEM_FAILED) {
        perror("Failed to open co-sim 2 semaphore");
        Set_Stop();
        UDP_Stop();
        return (void*)-1;
    }

    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_REQ);
    int rc = zmq_connect(socket, "tcp://localhost:5556");
    if (rc != 0) {
        perror("ZMQ connection failed");
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return NULL;
    }
    
    //inital semaphore capture
    sem_wait_safe(co_sim, 0);

    //populate the up and pub data
    pthread_mutex_lock(&DATA_Mutx);
    for (n = 0; n < CONF_FLAGS.UP_N; n++) {
        ZMQ_UP_DATA[n] = UP_DATA[n];
    }
    for (n = 0; n < CONF_FLAGS.PUB_N; n++) {
        ZMQ_PUB_DATA[n] = PUB_DATA[n];
    }
    pthread_mutex_unlock(&DATA_Mutx);

    sem_post(co_sim_2);


    //find indexes for PUB data
    int i;
    for (n = 0; n < ZMQ_PUB_LEN; n++) {
        for (i = 0; i < MAX_IO; i++)
            {
                if (strcmp(ZMQ_PUB_DATA[i].Name, ZMQ_Outputs[n].tag) == 0) {
                    ZMQ_Outputs[i].index = i;
                    break;
                }
            }
    }
    
    while (1) {
        
        //capture the semaphore from shm_interface
        sem_wait_safe(co_sim, 0);

        //get the latest PUB data
        pthread_mutex_lock(&DATA_Mutx);
        for (n = 0; n < CONF_FLAGS.PUB_N; n++) {
        ZMQ_PUB_DATA[n] = PUB_DATA[n];
        }
        pthread_mutex_unlock(&DATA_Mutx);

        cJSON* root = cJSON_CreateObject();

        for (int i = 0; i < ZMQ_PUB_LEN; ++i) {
            int idx = ZMQ_Outputs[i].index;
            cJSON_AddNumberToObject(root, ZMQ_Outputs[i].tag, ZMQ_PUB_DATA[idx].Value);
        }

        char* out_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        zmq_send(socket, out_str, strlen(out_str), 0);

        free(out_str);

        int len = zmq_recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (len < 0) {
            perror("ZMQ receive failed");
            break;
        }
        buffer[len] = '\0';
        
        cJSON* reply = cJSON_Parse(buffer);
        cJSON *current_element;
        if (reply) {
            pthread_mutex_lock(&DATA_Mutx);
            cJSON_ArrayForEach(current_element, reply){
                current_key = current_element->string;
                
                for (int j = 0; j < CONF_FLAGS.UP_N; ++j) {
                        if (strcmp(UP_DATA[j].Name, current_key) == 0) {
                            UP_DATA[j].Value = current_element->valuedouble;
                            break;
                        }
                    }
            }
            pthread_mutex_unlock(&DATA_Mutx);
            cJSON_Delete(reply);
            cJSON_Delete(current_element);
        } else {
            fprintf(stderr, "Failed to parse response JSON: %s\n", buffer);
        }

        sem_post(co_sim_2);

        if (Sem_Stop()) break;
    }

    zmq_close(socket);
    zmq_ctx_destroy(context);
    return NULL;
}

void CoSim_init(){
    int i = 0;
    char *saveptr;
    // read values
    char *buffer = Read_Vars("cosim", "outputs", "\0");

    //Parse values out to struct
    char* token = strtok_r(buffer, ",", &saveptr); 

    while (token != NULL) {
        strncpy(ZMQ_Outputs[i].tag, token, sizeof(ZMQ_Outputs[i].tag));
        ZMQ_Outputs[i].tag[sizeof(ZMQ_Outputs[i].tag)] = '\0';

        token = strtok_r(NULL, ",", &saveptr);
        i++;
    }

    ZMQ_PUB_LEN = i;

    //get configs and loop until shm_interface hears back from the simulator
    pthread_mutex_lock(&FLAG_Mutx);
    CONF_FLAGS = CONF;
    pthread_mutex_unlock(&FLAG_Mutx);
    while (!CONF_FLAGS.config_captured) {
        usleep(5000);
        pthread_mutex_lock(&FLAG_Mutx);
        CONF_FLAGS = CONF;
        pthread_mutex_unlock(&FLAG_Mutx);
        
        if (Sem_Stop()) break;
    }
}