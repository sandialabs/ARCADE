#ifndef init_Ser
#define init_Ser
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "cJSON.h"
#ifdef _WIN32
	#include <Windows.h>
	#include <io.h>
#elif __linux__
	#include <pthread.h>
	#include <unistd.h>
#endif // _WIN32

void *init_Server(void *);
char *ReadFile_init(char *);
char *SimName(void);
#endif