#include "Sim_Control.h"
#include "Sem_Stop.h"
#include "init_Server.h"

/* WINDOWS ONLY */
void User_Control(void){
    
    HANDLE stop;
    stop = OpenSemaphore(SEMAPHORE_MODIFY_STATE, FALSE, "Global\\stop_DB_sem");
    if (stop == NULL) {
        fprintf(stderr, "Failed to open stop semaphore.");
    }
    char invar;

    while(1){
        printf("***Enter X to stop simulation***\n\n");
        fflush(stdin);
        
        scanf_s("%c", &invar, 1);
        
        if (invar == 'x' || invar == 'X'){
            printf("Stopping Simulator\n");
            ReleaseSemaphore(stop, 5, NULL);
            Sleep(1000); /*slow down the kill to let threads close out safely*/
            break;
        }

    }
}

/* Notes: Its important not to fork the process if we are not executing the simulator from the DB.
The relationship between the parent process and child has to change depending on external or internal
execution of the simulator. This is the simplest and most stable solution. */

DWORD WINAPI Sim_Control(LPVOID arg) {
    
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    char* Simulink = "Simulink";

    char* args[2];
    args[0] = SimName();
    args[1] = NULL;

    if (args[0] != NULL) {
        /* checking if Simulink external simulator was selected*/
        int str_check = strcmp(args[0], Simulink);
        if (str_check == 0) {
            printf("External simulator selected. \n****You may now start the simulator****\n");
            User_Control();
        }
        else {
            if (!CreateProcess(NULL, args[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                fprintf(stderr, "Failed to start simulator executable.");
            }
            //start user control in this thread
            User_Control();

            //Wait for process and thread
            WaitForSingleObject(pi.hProcess, INFINITE);

            //close process and handles
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

        }
    }
    else {
        fprintf(stderr, "No simulator name in input.json ");
    }
    return 0;

}