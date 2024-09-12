#ifndef sem_server
#define sem_server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
	#define win_check 1
#include <WS2tcpip.h>
	#include <Windows.h>

#elif __linux__
	#define win_check 0
	#include <unistd.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
#endif // _WIN32

void UDP_Server(char *msg);
void UDP_Stop(void);
// UDP server defines
#define PORT 8000
#endif