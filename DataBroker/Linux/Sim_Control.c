#include "Sim_Control.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "init_Server.h"
#include "utils.h"

void User_Control(void){
    
    sem_t *stop;
	stop = sem_open("/stop", O_CREAT, 0644, 0);

    char invar;

    while(1){
        printf("***Enter X to stop simulation***\n\n");
        fflush(stdin);
        
        scanf("%c", &invar);
        
        if (invar == 'x' || invar == 'X'){
            printf("Stopping Simulator\n");
            sem_post(stop);
            sleep(1); /*slow down the kill to let threads close out safely*/
            break;
        }

    }
}

/* Notes: Its important not to fork the process if we are not executing the simulator from the DB.
The relationship between the parent process and child has to change depending on external or internal
execution of the simulator. This is the simplest and most stable solution. */
void *Sim_Control(){

    pid_t pid;
    char *args[2];
    args[0] = SimName();
    args[1] = NULL;

    // Setup special flags
    Special_Flags SHM_FLAGS;
    pthread_mutex_lock(&FLAG_Mutx);
    SHM_FLAGS = FLAGS;
    pthread_mutex_unlock(&FLAG_Mutx);
    
    /* checking if Simulink external simulator was selected*/
    if(!strcmp(args[0],"Simulink")){
        printf("External simulator selected. \n****You may now start the simulator****\n");
        User_Control();
    }
    else{
        /* fork process */
        pid = fork();

        switch(pid){
            case -1:
                /* Fork failed*/
                perror("Fork failed");
                break;
            case 0:
                /* Child process will run the user control */
                printf("Starting Simulator\n");
                execv(args[0],args);
                printf("Simulator has failed to load! \n");
                break;
            default:
                /* Parent starts the user control */
                printf("User Control Initializing\n");
                User_Control();
                break;
        }
        kill(pid,SIGTERM);
    }

pthread_exit((void *)0);
}