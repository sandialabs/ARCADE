#ifndef UTILS
#define UTILS
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

bool strtobool(char *string);
void sem_wait_safe(void* arg, int wait_limit);
void sem_wait_value(void* arg, int sem_value);
void sem_decrement(void* arg, int sem_value);

//setup special functions
typedef struct {
    bool Hold_Time_Flag;
    bool Start_Time_Flag;
    bool Reset_Sim_Flag;
} Special_Flags;

//setup special configs
typedef struct {
    bool Co_Sim_Enable;
    bool Co_Sim_Sync_Enable;
    bool Realtime_Timestep;
    int UP_N;
    int PUB_N;
    double TimeStep;
    bool config_captured;
} Configs;

extern Special_Flags FLAGS;
extern Configs CONF;

extern pthread_mutex_t FLAG_Mutx;

#endif