#include "UDP_Server.h"

void UDP_Server(char *msg) {
    struct sockaddr_in servaddr;

    // Set socket file description
    int sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd <= 0) {
        perror("Error: Could not open socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options to enable broadcast
    int broadcastEnable = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret) {
        perror("Error: Could not set socket to broadcast mode");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "255.255.255.255", &servaddr.sin_addr);
    servaddr.sin_port = htons(PORT);

    // Broadcast message
    sendto(sockfd, msg, strlen(msg), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    close(sockfd);
}

void UDP_Stop(void) {
    char msg[256];
    memset(msg, 0, sizeof(msg));
    sprintf(msg, "STOP\nSTOP\n");
    UDP_Server(msg);
    for (int i = 0; i < 5; i++) {
        UDP_Server(msg);
    }
}
