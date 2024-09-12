/* 
Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
certain rights in this software.

 S-Function connector program to import and export data and control
 signals in simulink

 Compile via mex: mex sfun_connector.c -lpthread -lrt

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define S_FUNCTION_NAME  sfun_connector
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#define MSG_SIZE_MULT 256
#define PUBLISH_POINTS_SHM_SEM "/pp_sem"
#define UPDATE_POINTS_SHM_SEM "/up_sem"
#define STOP_SEM "/stop"

key_t keyp = 10618;
key_t keyu = 10619;
key_t msg_key = 10620;

typedef struct {
        char Name[128];
        char Type[50];
        double Value;
        double Time;
        } DATA;
        
typedef struct {
    int PUB;
    int UP;
    double TimeStep;
    } MSG_DATA;

#define UP_IDX  0
#define UP_TAGS(S) ssGetSFcnParam(S,UP_IDX)

#define PUB_IDX   1
#define PUB_TAGS(S) ssGetSFcnParam(S,PUB_IDX)

#define NPARAMS   2

int Sem_Stop(void)
{
    sem_t *stop;
    int ST;
	stop = sem_open("/stop", O_CREAT, 0644, 0);
	sem_getvalue(stop, &ST);
	int value = (int)ST;
	return value;
}

static void mdlInitializeSizes(SimStruct *S)
{
    int_T nInputPorts  = 1;  /* number of input ports  */
    int_T nOutputPorts = 1;  /* number of output ports */
    int_T needsInput   = 1;  /* direct feed through    */

    int_T inputPortIdx  = 0;
    int_T outputPortIdx = 0;


    ssSetNumSFcnParams(S, 2);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /*
         * If the number of expected input parameters is not equal
         * to the number of parameters entered in the dialog box return.
         * Simulink will generate an error indicating that there is a
         * parameter mismatch.
         */
        return;
    }


    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, 1);   /* number of discrete states             */

    if (!ssSetNumInputPorts(S, nInputPorts)) return;    
    
    ssSetInputPortDirectFeedThrough(S, inputPortIdx, 1);

    if (!ssSetNumOutputPorts(S, nOutputPorts)) return;

    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);

    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */

    ssSetNumRWork(         S, DYNAMICALLY_SIZED);   /* number of real work vector elements   */
    ssSetNumIWork(         S, 2);   /* number of integer work vector elements*/
    ssSetNumPWork(         S, 4);   /* number of pointer work vector elements*/
    ssSetNumModes(         S, 0);   /* number of mode work vector elements   */
    ssSetNumNonsampledZCs( S, 0);   /* number of non-sampled zero crossings   */
    
    
    
    ssSetOperatingPointCompliance(S, USE_DEFAULT_OPERATING_POINT);
    
    ssSetOptions(S,
                 SS_OPTION_EXCEPTION_FREE_CODE |
                 SS_OPTION_ALLOW_INPUT_SCALAR_EXPANSION |
                 SS_OPTION_CALL_TERMINATE_ON_EXIT);


} /* end mdlInitializeSizes */


#if defined(MATLAB_MEX_FILE)
# define MDL_SET_INPUT_PORT_WIDTH
  static void mdlSetInputPortWidth(SimStruct *S, int_T port,
                                    int_T inputPortWidth)
  {
      
      ssSetInputPortWidth(S,port,inputPortWidth); 
  }


# define MDL_SET_OUTPUT_PORT_WIDTH
  static void mdlSetOutputPortWidth(SimStruct *S, int_T port,
                                     int_T outputPortWidth)
  {
      ssSetOutputPortWidth(S,port,outputPortWidth);
  }

# define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
  /* Function: mdlSetDefaultPortDimensionInfo ===========================================
   * Abstract:
   *   In case no ports were specified, the default is an input port of width 2
   *   and an output port of width 1.
   */
  static void mdlSetDefaultPortDimensionInfo(SimStruct        *S)
  {
      int n_In, n_Out;
      size_t    nu_I, nu_O ;
      
      char_T *IN_TAGS;
      char_T *OUT_TAGS;
      
      nu_I = mxGetNumberOfElements(UP_TAGS(S));
      nu_O = mxGetNumberOfElements(PUB_TAGS(S));
      
      /* take and count tags to determine port sizes */
      
      /* allocate memory for the tags. set error if we cant */
      if ( (IN_TAGS=(char*)malloc(nu_I+1)) == NULL ) {
            ssSetErrorStatus(S,"Memory allocation error in mdlPortDem");
            return;
        }
      
      if ( (OUT_TAGS=(char*)malloc(nu_O+1)) == NULL ) {
            ssSetErrorStatus(S,"Memory allocation error in mdlPortDem");
            return;
        }
      /* pull strings from parameters */
      if ( mxGetString(UP_TAGS(S),IN_TAGS,nu_I+1) != 0 ) {
            free(IN_TAGS);
            ssSetErrorStatus(S,"mxGetString error in mdlStart");
            return;
        }
      
      if ( mxGetString(PUB_TAGS(S),OUT_TAGS,nu_O+1) != 0 ) {
            free(OUT_TAGS);
            ssSetErrorStatus(S,"mxGetString error in mdlStart");
            return;
        }
      
      /* separate and count tags */
      char* token = strtok(IN_TAGS,";");
      n_In = 0;
      while (token != NULL) {
          token = strtok(NULL,";");
          n_In++;
      }
      
      
      free(token);
      token = strtok(OUT_TAGS,";");
      n_Out = 0;
      while (token != NULL) {
          token = strtok(NULL,";");
          n_Out++;
      }
      
      ssSetInputPortWidth(S, 0, n_In);
      
      ssSetOutputPortWidth(S, 0, n_Out);
      
      ssPrintf("Output: %u Input: %u \n",n_In,n_Out);
      
      
      free(OUT_TAGS);
      free(IN_TAGS);
      
      
  }
#endif


static void mdlInitializeSampleTimes(SimStruct *S)
{
    /* Register one pair for each sample time */
    ssSetSampleTime(S, 0, CONTINUOUS_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
    
} /* end mdlInitializeSampleTimes */



#define MDL_SET_WORK_WIDTHS   /* Change to #undef to remove function */
#if defined(MDL_SET_WORK_WIDTHS) && defined(MATLAB_MEX_FILE)

  static void mdlSetWorkWidths(SimStruct *S)
  {
      ssSetNumRWork(S, ssGetOutputPortWidth(S,0));
  }
#endif /* MDL_SET_WORK_WIDTHS */


#define MDL_INITIALIZE_CONDITIONS   /* Change to #undef to remove function */
#if defined(MDL_INITIALIZE_CONDITIONS)

  static void mdlInitializeConditions(SimStruct *S)
  {
      int_T   Iwidth = ssGetInputPortWidth(S,0);
      int_T   Owidth = ssGetOutputPortWidth(S,0);
      

      /* set up shared memory */
      int shmidu = shmget(keyu, Owidth * sizeof(DATA), 0600|IPC_CREAT);
      int shmidp = shmget(keyp, Iwidth * sizeof(DATA), 0600|IPC_CREAT);

      DATA *publishPointsShmAddress = (DATA *) shmat(shmidp,NULL,0); 
      DATA *updatePointsShmAddress = (DATA *) shmat(shmidu,NULL,0); 
      
      /* Collect TAGS */
      
      size_t    nu_I, nu_O ;
      
      char_T *IN_TAGS;
      char_T *OUT_TAGS;
      
      
      nu_I = mxGetNumberOfElements(UP_TAGS(S));
      nu_O = mxGetNumberOfElements(PUB_TAGS(S));
      
      /* take and count tags to determine port sizes */
      
      /* allocate memory for the tags. set error if we cant */
      if ( (IN_TAGS=(char*)malloc(nu_I+1)) == NULL ) {
            ssSetErrorStatus(S,"Memory allocation error in mdlPortDem");
            return;
        }
      
      if ( (OUT_TAGS=(char*)malloc(nu_O+1)) == NULL ) {
            ssSetErrorStatus(S,"Memory allocation error in mdlPortDem");
            return;
        }
      /* pull strings from parameters */
      if ( mxGetString(UP_TAGS(S),IN_TAGS,nu_I+1) != 0 ) {
            free(IN_TAGS);
            ssSetErrorStatus(S,"mxGetString error in mdlStart");
            return;
        }
      
      if ( mxGetString(PUB_TAGS(S),OUT_TAGS,nu_O+1) != 0 ) {
            free(OUT_TAGS);
            ssSetErrorStatus(S,"mxGetString error in mdlStart");
            return;
        }
      
      /*ssSetPWorkValue(S,2,IN_TAGS);
      ssSetPWorkValue(S,3,OUT_TAGS); */
      
      /* collect tags in array of strings */
      char *savetok;
      
      int_T i_i, i_o, i;
      
      char* token = strtok_r(IN_TAGS,";",&savetok);
      i_i = 0;
      
      while (token != NULL) {
          strcpy(publishPointsShmAddress[i_i].Name, token);
          token = strtok_r(NULL,";",&savetok);
          i_i++;
          
      }
      
      free(token);
      token = strtok_r(OUT_TAGS,";",&savetok);
      i_o = 0;
      
      while (token != NULL) {
          strcpy(updatePointsShmAddress[i_o].Name, token);
          updatePointsShmAddress[i_o].Value = -100000000000000.0;
          updatePointsShmAddress[i_o].Time = 0.0;
          strcpy(updatePointsShmAddress[i_o].Type, "DOUBLE");
          token = strtok_r(NULL,";",&savetok);
          i_o++;
          
      }
      
      shmdt(publishPointsShmAddress);
      shmdt(updatePointsShmAddress);
      
      free(OUT_TAGS);
      free(IN_TAGS);

      real_T *rwork = ssGetRWork(S);
      real_T *y     = ssGetOutputPortRealSignal(S,0);
      
      for (i = 0; i < Owidth; i++){
          *rwork++ = -100000000000000.0;
      }
      i=0;
      for (i = 0; i < Owidth; i++) {
          *y++ = *rwork++;
      }
      
      /* Send number of inputs and outputs to data broker */
      sem_t *msg_sem;
      msg_sem = sem_open("/msg", O_CREAT, 0644, 0);
      
      int shmdb = shmget(msg_key, sizeof(MSG_DATA), 0600|IPC_CREAT);

      MSG_DATA *MSG_DB = (MSG_DATA *) shmat(shmdb,NULL,0); 
      
      MSG_DB->UP = Owidth;
      MSG_DB->PUB = Iwidth;
      MSG_DB->TimeStep = ssGetFixedStepSize(S);
      
      shmdt(MSG_DB);
      
      sem_post(msg_sem);

  }
#endif /* MDL_INITIALIZE_CONDITIONS */

/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector(s),
 *    ssGetOutputPortSignal.
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
    int_T  i;
    real_T *y     = ssGetOutputPortRealSignal(S,0);
    int_T  ny     = ssGetOutputPortWidth(S,0);
    real_T *rwork = ssGetRWork(S);

    UNUSED_ARG(tid); /* not used in single tasking mode */

    for (i = 0; i < ny; i++) {
        *y++ = *rwork++;
    }
} /* end mdlOutputs */


#define MDL_UPDATE  /* Change to #undef to remove function */
#if defined(MDL_UPDATE)
  /* Function: mdlUpdate ======================================================
   * Abstract:
   *    This function is called once for every major integration time step.
   *    Discrete states are typically updated here, but this function is useful
   *    for performing any tasks that should only take place once per
   *    integration step.
   */
  static void mdlUpdate(SimStruct *S, int_T tid)
  {
    
        
    int_T             i;
    InputRealPtrsType uPtrs  = ssGetInputPortRealSignalPtrs(S,0);
    real_T            *rwork = ssGetRWork(S);
    real_T            Time   = ssGetT(S);
    
    int_T   Iwidth = ssGetInputPortWidth(S,0);
    int_T   Owidth = ssGetOutputPortWidth(S,0);
    
    DATA PUB_DATA[Iwidth];
    DATA UP_DATA[Owidth];

    /*set up semaphores */
    
    sem_t *semu;
    sem_t *semp;
    
    semu = sem_open(UPDATE_POINTS_SHM_SEM, 0);
    semp = sem_open(PUBLISH_POINTS_SHM_SEM, 0);
    
    
    /* set up shared memory */
    int shmidu = shmget(keyu, Owidth * sizeof(DATA), 0600|IPC_CREAT);
    int shmidp = shmget(keyp, Iwidth * sizeof(DATA), 0600|IPC_CREAT);
    
    DATA *publishPointsShmAddress = (DATA *) shmat(shmidp,NULL,0); 
    DATA *updatePointsShmAddress = (DATA *) shmat(shmidu,NULL,0); 

    UNUSED_ARG(tid); /* not used in single tasking mode */
    
    sem_wait(semu);
    
    for (i = 0; i < Iwidth; i++){
        publishPointsShmAddress[i].Value = *uPtrs[i];
        publishPointsShmAddress[i].Time = Time;
    }
    for (i = 0; i < Owidth; i++){
        *rwork++ = updatePointsShmAddress[i].Value;
    }
    
    shmdt(publishPointsShmAddress);
    shmdt(updatePointsShmAddress);
    
    sem_post(semp);
    
    int STOP = Sem_Stop();
    if (STOP > 0)
    {
        ssSetStopRequested(S, 1);
    }
    

  }
#endif /* MDL_UPDATE */



/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was allocated
 *    in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    
    sem_t *stop;
    stop = sem_open(STOP_SEM, 0);
    sem_post(stop);

    sem_t *semp;
    semp = sem_open(PUBLISH_POINTS_SHM_SEM, 0);
    sem_post(semp);
    sem_post(semp);
    
}


/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
