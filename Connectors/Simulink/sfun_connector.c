/*
Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
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

#define S_FUNCTION_NAME sfun_connector
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "broker_connector.h"

#define UP_TAGS(S) ssGetSFcnParam(S, 0)
#define PUB_TAGS(S) ssGetSFcnParam(S, 1)

static void connector_error(SimStruct *S, const char *message)
{
    ssSetErrorStatus(S, message);
    sem_t *stop = sem_open("/stop", 0);
    if (stop != SEM_FAILED) {
        sem_post(stop);
        sem_close(stop);
    }
}

static void mdlInitializeSizes(SimStruct *S)
{
    ssSetNumSFcnParams(S, 2);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        return;
    }
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 1);
    if (!ssSetNumInputPorts(S, 1) || !ssSetNumOutputPorts(S, 1)) {
        return;
    }
    ssSetInputPortDirectFeedThrough(S, 0, 1);
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetNumSampleTimes(S, 1);
    ssSetNumRWork(S, DYNAMICALLY_SIZED);
    ssSetNumIWork(S, 2);
    ssSetNumPWork(S, 1);
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);
    ssSetOperatingPointCompliance(S, USE_DEFAULT_OPERATING_POINT);
    ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE |
                       SS_OPTION_ALLOW_INPUT_SCALAR_EXPANSION |
                       SS_OPTION_CALL_TERMINATE_ON_EXIT);
}

#if defined(MATLAB_MEX_FILE)
#define MDL_SET_INPUT_PORT_WIDTH
static void mdlSetInputPortWidth(SimStruct *S, int_T port, int_T width)
{
    ssSetInputPortWidth(S, port, width);
}

#define MDL_SET_OUTPUT_PORT_WIDTH
static void mdlSetOutputPortWidth(SimStruct *S, int_T port, int_T width)
{
    ssSetOutputPortWidth(S, port, width);
}

/// Read parameter tags.
///
/// Returned strings are owned by the caller.
static char *parameter_tags(SimStruct *S, int index)
{
    const mxArray *parameter = ssGetSFcnParam(S, index);
    if (!mxIsChar(parameter)) {
        return NULL;
    }
    size_t size = mxGetNumberOfElements(parameter) + 1;
    char *tags = malloc(size);
    if (tags && mxGetString(parameter, tags, size)) {
        free(tags);
        return NULL;
    }
    return tags;
}

#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    char *pub = parameter_tags(S, 0);
    char *up = parameter_tags(S, 1);
    int n_pub = pub ? Broker_Tag_Count(pub) : -1;
    int n_up = up ? Broker_Tag_Count(up) : -1;
    free(pub);
    free(up);
    if (n_pub < 0 || n_up < 0) {
        ssSetErrorStatus(S, "Invalid tag parameters.");
        return;
    }
    ssSetInputPortWidth(S, 0, n_pub);
    ssSetOutputPortWidth(S, 0, n_up);
}
#endif

static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, CONTINUOUS_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}

#define MDL_SET_WORK_WIDTHS
#if defined(MATLAB_MEX_FILE)
static void mdlSetWorkWidths(SimStruct *S)
{
    ssSetNumRWork(S, ssGetOutputPortWidth(S, 0));
}
#endif

#define MDL_INITIALIZE_CONDITIONS
static void mdlInitializeConditions(SimStruct *S)
{
    Broker_Connector *previous = ssGetPWorkValue(S, 0);
    if (previous) {
        Broker_Close(previous);
        free(previous);
        ssSetPWorkValue(S, 0, NULL);
    }
    Broker_Connector *connection = calloc(1, sizeof(*connection));
    if (!connection) {
        connector_error(S, "Memory allocation error.");
        return;
    }
    size_t pub_size = mxGetNumberOfElements(UP_TAGS(S)) + 1;
    size_t up_size = mxGetNumberOfElements(PUB_TAGS(S)) + 1;
    char *pub = malloc(pub_size), *up = malloc(up_size);
    if (!mxIsChar(UP_TAGS(S)) || !mxIsChar(PUB_TAGS(S)) || !pub || !up ||
        mxGetString(UP_TAGS(S), pub, pub_size) ||
        mxGetString(PUB_TAGS(S), up, up_size) ||
        Broker_Tag_Count(pub) != ssGetInputPortWidth(S, 0) ||
        Broker_Tag_Count(up) != ssGetOutputPortWidth(S, 0)) {
        free(pub);
        free(up);
        free(connection);
        connector_error(S, "Tag parameters must match port widths.");
        return;
    }
    int result = Broker_Open(connection, pub, up, ssGetFixedStepSize(S));
    free(pub);
    free(up);
    if (result) {
        free(connection);
        connector_error(S, "Failed to initialize broker connection.");
        return;
    }
    ssSetPWorkValue(S, 0, connection);
    Broker_Initial_Values(ssGetRWork(S), ssGetOutputPortRealSignal(S, 0), connection->n_up);
    if (sem_post(connection->msg)) {
        connector_error(S, "Failed to announce broker connection.");
    }
}

static void mdlOutputs(SimStruct *S, int_T tid)
{
    UNUSED_ARG(tid);
    real_T *output = ssGetOutputPortRealSignal(S, 0);
    real_T *work = ssGetRWork(S);
    for (int i = 0; i < ssGetOutputPortWidth(S, 0); ++i) {
        output[i] = work[i];
    }
}

#define MDL_UPDATE
static void mdlUpdate(SimStruct *S, int_T tid)
{
    UNUSED_ARG(tid);
    Broker_Connector *connection = ssGetPWorkValue(S, 0);
    if (!connection) {
        connector_error(S, "Broker connection not initialized.");
        return;
    }
    int result = Broker_Wait(connection);
    if (result > 0) {
        ssSetStopRequested(S, 1);
        return;
    }
    if (result < 0) {
        connector_error(S, "Failed to wait for broker update.");
        return;
    }
    InputRealPtrsType input = ssGetInputPortRealSignalPtrs(S, 0);
    real_T *work = ssGetRWork(S);
    for (int i = 0; i < connection->n_pub; ++i) {
        connection->published[i].Value = *input[i];
        connection->published[i].Time = ssGetT(S);
    }
    for (int i = 0; i < connection->n_up; ++i) {
        work[i] = connection->updated[i].Value;
    }
    if (sem_post(connection->pub)) {
        connector_error(S, "Failed to publish broker data.");
    }
    if (Broker_Stopped(connection)) {
        ssSetStopRequested(S, 1);
    }
}

static void mdlTerminate(SimStruct *S)
{
    Broker_Connector *connection = ssGetPWorkValue(S, 0);
    if (connection) {
        sem_post(connection->stop);
        sem_post(connection->pub);
        Broker_Close(connection);
        free(connection);
        ssSetPWorkValue(S, 0, NULL);
    }
}

#ifdef MATLAB_MEX_FILE
#include "simulink.c"
#else
#include "cg_sfun.h"
#endif
