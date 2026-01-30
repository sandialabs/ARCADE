#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define win_check 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <Windows.h>
    #pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib
#elif defined(__linux__) || defined(__APPLE__)
    #define win_check 0
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #endif
#endif

void UDP_Server(char *msg);
void UDP_Stop(void);

// UDP server defines
#define PORT 8000

#endif // UDP_SERVER_H
