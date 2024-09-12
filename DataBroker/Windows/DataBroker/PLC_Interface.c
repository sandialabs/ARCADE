#include <zmq.h>
#include "Shm_Interface.h"
#include "PLC_Interface.h"
#include "Sem_Stop.h"

DWORD WINAPI PLC_Interface(LPVOID arg)
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
    DWORD DB_WaitResult;

    DATA_Mutx = OpenMutex(SYNCHRONIZE, FALSE, "Global\\DB_DATA_Mutx");
    if (DATA_Mutx == NULL) {
        fprintf(stderr, "Failed to get DATA Mutex.");
        Set_Stop();
        return -1;
    }

    while (1) {
        memset(buffer,0,sizeof(MSG_BUFFER));
        nbytes = zmq_recv (responder, buffer, MSG_BUFFER, 0);
        
        if (nbytes != -1) {
            name = strtok_s(buffer, ":", &saveptr); //NEED to use strtok_r or not thread safe!
            value = strtok_s(NULL, ":", &saveptr);  //IE if strtok is used in another thread they could collide
            //printf ("%s\n",value);

            //wait for mutex for 5 seconds
            DB_WaitResult = WaitForSingleObject(DATA_Mutx, 5000L);

            // If mutex is captured
            if (DB_WaitResult == WAIT_OBJECT_0) {
                for (n = 0; n < MAX_IO; n++)
                {
                    if (strcmp(UP_DATA[n].Name, name) == 0) {
                        UP_DATA[n].Value = strtod(value, NULL);
                        break;
                    }
                }
                //Release mutex
                if (!ReleaseMutex(DATA_Mutx)) {
                    fprintf(stderr, "Failed to release DATA Mutex.");
                }
            }
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

return 0;

}

