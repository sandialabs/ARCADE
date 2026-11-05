#include "UDP_Server.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "Sem_Stop.h"
#include "atomicSet.h"

int UDP_Server(const char *msg)
{
    size_t size = strlen(msg);
    if (size > UDP_MAX_PAYLOAD) {
        errno = EMSGSIZE;
        perror("UDP publication");
        setErrorFlag();
        Set_Stop();
        return -1;
    }
    int sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Could not create a socket");
        setErrorFlag();
        Set_Stop();
        return -1;
    }

    int broadcastEnable = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable,
                         sizeof(broadcastEnable));
    if (ret) {
        perror("Could not set socket options");
        close(sockfd);
        setErrorFlag();
        Set_Stop();
        return -1;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "255.255.255.255", &servaddr.sin_addr);
    servaddr.sin_port = htons(UDP_PORT);

    ssize_t sent;
    do {
        sent = sendto(sockfd, msg, size, 0, (const struct sockaddr *)&servaddr,
                      sizeof(servaddr));
    } while (sent < 0 && errno == EINTR);
    int saved = errno;
    close(sockfd);
    if (sent < 0 || (size_t)sent != size) {
        errno = sent < 0 ? saved : EIO;
        perror("Sending UDP publication");
        setErrorFlag();
        Set_Stop();
        return -1;
    }
    return 0;
}

void UDP_Stop(void)
{
    char msg[256];
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "STOP\nSTOP\n");
    UDP_Server(msg);
    for (int i = 0; i < 5; i++) {
        UDP_Server(msg);
    }
}
