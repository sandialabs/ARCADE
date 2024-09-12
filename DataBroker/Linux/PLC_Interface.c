#include "PLC_Interface.h"
#include "Shm_Interface.h"
#include "Sem_Stop.h"
#include "utils.h"

void *PLC_Interface()
{
    //  Socket to talk to clients
    void *context = zmq_ctx_new ();
    void *responder = zmq_socket (context, ZMQ_PULL);
    int timeout = 3000;
    int rc = zmq_setsockopt(responder, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    assert (rc == 0);
    rc = zmq_bind (responder, "tcp://*:5555");
    assert (rc == 0);
    
    int n = 0;
    int STOP = 0;
    int nbytes;
    char buffer [MSG_BUFFER];
    char *reply = "Actuation Signal Recieved";
    char *name, *value, *saveptr;

    //special flag vars
    const char *Flag_Tags[4];
    Flag_Tags[0] = "Hold_Time";
    Flag_Tags[1] = "Start_Sim";
    Flag_Tags[2] = "Reset_Sim";
    Flag_Tags[3] = "End_Sim";

    while (1) {
        memset(buffer,0,sizeof(MSG_BUFFER));
        nbytes = zmq_recv (responder, buffer, MSG_BUFFER, 0);
        
        if (nbytes != -1) {
            name = strtok_r(buffer, ":", &saveptr); //NEED to use strtok_r or not thread safe!
            value = strtok_r(NULL, ":", &saveptr);  //IE if strtok is used in another thread they could collide
            //printf ("%s\n",value);

            //Special messages
            for (n = 0; n < 4; n++)
            {
                if (strcmp(Flag_Tags[n], name) == 0) {
                    pthread_mutex_lock(&FLAG_Mutx);
                    switch (n){
                        case 0:
                            FLAGS.Hold_Time_Flag = strtobool(value);
                            break;
                        case 1:
                            FLAGS.Hold_Time_Flag = !strtobool(value);
                            break;
                        case 2:
                            FLAGS.Reset_Sim_Flag = strtobool(value);
                            break;
                        case 3:
                            printf("Stop requested over ZMQ\n");
                            Set_Stop();
                            break;
                    }
                    pthread_mutex_unlock(&FLAG_Mutx);
                    break;
                }
            }

            //wait for mutex
            pthread_mutex_lock(&DATA_Mutx);

            for (n = 0; n < MAX_IO; n++)
            {
                if (strcmp(UP_DATA[n].Name, name) == 0) {
                    UP_DATA[n].Value = strtod(value, NULL);
                    break;
                }
            }
            //Release mutex
            pthread_mutex_unlock(&DATA_Mutx);
            
        }

        //Check stop semaphore
		STOP = Sem_Stop();
		if (STOP > 0)
		{
			break;
		}
    }
    
zmq_close(responder); 
zmq_ctx_destroy(context);
printf("ZMQ Update Server Closed Successfully \n");
pthread_exit((void *)0);

}

