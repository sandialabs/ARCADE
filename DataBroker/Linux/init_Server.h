#ifndef init_Ser
#define init_Ser
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include "cJSON.h"
void *init_Server();
char *ReadFile();
char *SimName();
bool Read_flags(char *JSON_Catagory, char *flagname, bool default_condition);
char *Read_Vars(char *JSON_Catagory, char *Varname, char *default_condition);
#endif