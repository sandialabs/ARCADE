#include "init_Server.h"
#include "Sem_Stop.h"
#include "utils.h"


void *init_Server()
{
    //Simulator variables
    int STOP = 0;
    //JSON file variables
    cJSON *elem, *name;
    char *json_string;
    const cJSON *object = NULL;
    const cJSON *endpoints = NULL;
    const cJSON *plcs = NULL;
    cJSON* plc = NULL;
    const cJSON* servers = NULL;
    cJSON* server = NULL;

    //ZMQ variables
    char IP_buffer[256];
    char *tag, *IP, *saveptr;
    int nbytes = 1;
    char IP_buf[256] = "", buffer[256] = "";
    void *responder, *requester;
    void* context = zmq_ctx_new();
    char *msg_string;
    // Read in JSON file
    json_string = ReadFile("input.json");
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

    //initalize special settings
    pthread_mutex_lock(&FLAG_Mutx);
    //read flags
    FLAGS.Hold_Time_Flag = Read_flags("simulator", "hold_for_dante", false);
    FLAGS.Reset_Sim_Flag = false;
    //read configs
    CONF.Co_Sim_Enable = Read_flags("simulator", "co_sim_enable", false);
    CONF.Co_Sim_Sync_Enable = Read_flags("cosim", "sync_enable", false);
    CONF.Realtime_Timestep = Read_flags("simulator", "realtime_timestep", false);
    CONF.UP_N = 0;
    CONF.PUB_N = 0;
    CONF.TimeStep = 0.0;
    CONF.config_captured = false;
    //return the mutex
    pthread_mutex_unlock(&FLAG_Mutx);

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
                    printf("Sending PLC Initalization to Endpoint: : %s\n", IP_Host->valuestring);

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
    json_string = ReadFile("input.json");
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

bool Read_flags(char *JSON_Catagory, char *flagname, bool default_condition)
{
    // Vars setup
    char *json_string;
    const cJSON *simulator = NULL;
    const cJSON *flags = NULL;

    // Read in JSON file
    json_string = ReadFile("input.json");
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
    simulator = cJSON_GetObjectItem(root, JSON_Catagory);
    cJSON_ArrayForEach(flags, simulator){
            cJSON *iter_JSON = cJSON_GetObjectItem(flags, flagname);
            if (!iter_JSON){
                fprintf(stderr, "Flag %s not in config!\n", flagname);
                return default_condition;
            }else{
                printf("Flag %s = %s \n", flagname, iter_JSON->valuestring);
                return strtobool(iter_JSON->valuestring);
            }
    }
}

char *Read_Vars(char *JSON_Catagory, char *Varname, char *default_condition)
{
    // Vars setup
    char *json_string;
    const cJSON *simulator = NULL;
    const cJSON *flags = NULL;

    // Read in JSON file
    json_string = ReadFile("input.json");
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
    simulator = cJSON_GetObjectItem(root, JSON_Catagory);
    cJSON_ArrayForEach(flags, simulator){
            cJSON *iter_JSON = cJSON_GetObjectItem(flags, Varname);
            if (!iter_JSON){
                fprintf(stderr, "Flag %s not in config!\n", Varname);
                return default_condition;
            }else{
                printf("Flag %s = %s \n", Varname, iter_JSON->valuestring);
                return iter_JSON->valuestring;
            }
    }
}

char *ReadFile(char *filename)
{
    char *buffer = NULL;
    int string_size, read_size;
    FILE *handler = fopen(filename, "r");

    if (handler)
    {
        // Seek the last byte of the file
        fseek(handler, 0, SEEK_END);
        // Offset from the first to the last byte, or in other words, filesize
        string_size = ftell(handler);
        // go back to the start of the file
        rewind(handler);
        // Allocate a string that can hold it all
        buffer = (char *)malloc(sizeof(char) * (string_size + 1));
        // Read it all in one operation
        read_size = fread(buffer, sizeof(char), string_size, handler);
        // fread doesn't set it so put a \0 in the last position
        // and buffer is now officially a string
        buffer[string_size] = '\0';
        if (string_size != read_size)
        {
            // Something went wrong, throw away the memory and set
            // the buffer to NULL
            free(buffer);
            buffer = NULL;
        }
        // Always remember to close the file.
        fclose(handler);
    }
    return buffer;
}
