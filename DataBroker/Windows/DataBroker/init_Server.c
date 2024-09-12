#include <zmq.h>
#include "Sem_Stop.h"
#include "init_Server.h"

void *init_Server(void *arg)
{

    //Simulator variables
    int STOP = 0;
    //JSON file variables
    cJSON *elem, *name;
    char *json_string;
    const cJSON *object = NULL;
    const cJSON *endpoints = NULL;
    const cJSON *plcs = NULL;
    const cJSON *plc = NULL;
    const cJSON* servers = NULL;
    const cJSON* server = NULL;

    //ZMQ variables
    char IP_buffer[256];
    char *tag, *IP, *saveptr;
    int nbytes = 1;
    char IP_buf[256] = "", buffer[256] = "";
    void *responder, *requester;
    void* context = zmq_ctx_new();
    char *msg_string;
    // Read in JSON file
    json_string = ReadFile_init("input.json");
    cJSON *root = cJSON_Parse(json_string);
    int n = cJSON_GetArraySize(root);

    // Make sure JSON file is read succesfully
    if (!root)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
    }
    
    //Start reading JSON and send configs to endpoints
    endpoints = cJSON_GetObjectItem(root, "endpoints");

    if (endpoints){
        cJSON_ArrayForEach(object, endpoints)
        {
            cJSON* IP_Host = cJSON_GetObjectItem(object, "IP_Host");
            cJSON* node = cJSON_GetObjectItem(object, "node");

            /* Check to see if everthing needed is there */
            if (!node) node = cJSON_CreateString("WishIHadAName");
            if (!IP_Host) {
                printf("Need IP_Host in the JSON!!\n");
                break;
            }
 
            //setup zmq socket
            requester = zmq_socket(context, ZMQ_REQ);
            snprintf(IP_buf, sizeof(IP_buf), "%s%s%s", "tcp://", IP_Host->valuestring, ":6666");
            zmq_connect(requester, IP_buf);

            //Get PLC objects and send them
            bool json_recieved = false;
            char* Valid = "VALID";
            char* Skip = "SKIP";
            plcs = cJSON_GetObjectItem(object, "PLCS");
            cJSON_ArrayForEach(plc, plcs)
            {
                while (!json_recieved) {
                    //Check if type exists and write PLC if not
                    cJSON* Type = cJSON_GetObjectItem(plc, "Type");
                    if (!Type) cJSON_AddStringToObject(plc, "Type", "plc");

                    //convert JSON to string
                    msg_string = cJSON_Print(plc);

                    //inform user that you are sending data
                    printf("Sending PLC Initalization to Endpoint: : %s\n", IP_Host->valuestring);

                    //Send data via ZMQ
                    zmq_send(requester, msg_string, strlen(msg_string), 0);
                    zmq_recv(requester, buffer, sizeof(buffer), 0);

                    //Check return from Endpoint 
                    if (strcmp(buffer, Valid) == 0) {
                        printf("Received: %s\nPLC config recieved.\n", buffer);
                        json_recieved = true;
                    }
                    else if (strcmp(buffer, Skip) == 0) {
                        printf("Received: %s\nPLC config skipped.\n", buffer);
                        json_recieved = true;
                    }
                    else {
                        printf("Received: %s\nPLC config failed. Retry.\n", buffer);
                        json_recieved = false;
                    }
                }
                json_recieved = false;
            }

            servers = cJSON_GetObjectItem(object, "Server");
            cJSON_ArrayForEach(server, servers)
            {
                while (!json_recieved) {
                    //Check if type exists and write PLC if not
                    cJSON* Type = cJSON_GetObjectItem(server, "Type");
                    if (!Type) cJSON_AddStringToObject(server, "Type", "server");

                    //convert JSON to string
                    msg_string = cJSON_Print(server);

                    //inform user that you are sending data
                    printf("Sending Server Initalization to Endpoint: : %s\n", IP_Host->valuestring);

                    //Send data via ZMQ
                    zmq_send(requester, msg_string, strlen(msg_string), 0);
                    zmq_recv(requester, buffer, sizeof(buffer), 0);
                    
                    //Check return from Endpoint 
                    if (strcmp(buffer, Valid) == 0) {
                        printf("Received: %s\nServer config recieved.\n", buffer);
                        json_recieved = true;
                    }
                    else if (strcmp(buffer, Skip) == 0) {
                        printf("Received: %s\nServer config skipped.\n", buffer);
                        json_recieved = true;
                    }
                    else {
                        printf("Received: %s\nServer config failed. Retry.\n", buffer);
                        json_recieved = false;
                    }
                }
                json_recieved = false;
            }
            //Signal end of config message 
            json_recieved = false;
            while (!json_recieved) {
                msg_string = "END_MESSAGE";
                zmq_send(requester, msg_string, strlen(msg_string), 0);
                zmq_recv(requester, buffer, sizeof(buffer), 0);
                //Check return from Endpoint 
                if (strcmp(buffer, Valid) == 0) {
                    printf("Received: %s\nEnd message recieved.\n", buffer);
                    json_recieved = true;
                }
                else {
                    printf("Received: %s\nEnd message failed. Retry.\n", buffer);
                    json_recieved = false;
                }
            }
            
            //close zmq connection
            zmq_close(requester);

        }

    }
    
    cJSON_Delete(root);

    zmq_ctx_destroy(context);
    printf("Endpoint Initialization Complete\n");
}

char *SimName(void)
{
    // Vars setup
    char *json_string;
    const cJSON *simulator = NULL;
    const cJSON *EXE = NULL;

    // Read in JSON file
    json_string = ReadFile_init("input.json");
    cJSON *root = cJSON_Parse(json_string);
    int n = cJSON_GetArraySize(root);

    // Make sure JSON file is read succesfully
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
    }

    // Read Simulator information
    simulator = cJSON_GetObjectItem(root, "simulator");
    cJSON_ArrayForEach(EXE, simulator){
            cJSON *ExecutableName = cJSON_GetObjectItem(EXE, "executableName");
            if (!ExecutableName){
                printf("Executable Name = Null \n****You need a simulation executable in the JSON!****\n");
                break;
            }else{
                printf("Executable Name = %s \n", ExecutableName->valuestring);
                return ExecutableName->valuestring;
            }
    }
}

char *ReadFile_init(char *filename)
{
    //Declare Vars
    char* buffer = NULL;
    int string_size, read_size;
    FILE* handler;

    //Find CDW in windows
    LPTSTR buff[FILENAME_MAX];
    DWORD buff_len = 260;
    GetCurrentDirectory(buff_len, buff);
    printf("Current working dir: %s\n", buff); //print it for debug
    
    //Use CDW to access full path of input file
    char* full_file_path;
    full_file_path = malloc(strlen(buff) + strlen(filename) + 1);
    strcpy(full_file_path, buff);
    strcat(full_file_path, "\\");
    strcat(full_file_path, filename);
    printf("Full file path: %s\n", full_file_path); //print for debug
    handler = fopen(full_file_path, "r");

    //Open file, allocate string space, copy to buffer
    if (handler)
    {
        fseek(handler, 0, SEEK_END);
        string_size = ftell(handler);
        fseek(handler, 0, SEEK_SET);
        buffer = malloc(string_size);
        if (buffer)
        {
            fread(buffer, 1, string_size, handler);
        }
        fclose(handler);
    }

    return buffer;
}