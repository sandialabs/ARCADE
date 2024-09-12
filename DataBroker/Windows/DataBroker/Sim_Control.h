#ifndef sim_cont
#define sim_cont
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <Windows.h>
DWORD WINAPI Sim_Control(LPVOID arg);
#endif