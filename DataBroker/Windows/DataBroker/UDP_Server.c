#include "UDP_Server.h"

void UDP_Server(char *msg)
{
	struct sockaddr_in servaddr;
	WSADATA Data;
	int err = WSAStartup(MAKEWORD(2, 2), &Data);
	if (err != 0)
	{
		printf("Error: Could not open WSAStartup. Error: %i", err);
		exit(EXIT_FAILURE);
	}

	// Set socket file description
	int sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd <= 0)
	{
		printf("Error: Could not open socket. Error: %i", sockfd);
		exit(EXIT_FAILURE);
	}

	// Set socket options enable broadcast
	int broadcastEnable = 1;
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
	if (ret)
	{
		printf("Error: Could not open set socket to broadcast mode");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// Filling server information
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, "255.255.255.255", &servaddr.sin_addr);
	servaddr.sin_port = htons(PORT);

	// Broadcast message
	//printf("%s\n", (const char *)msg);
	sendto(sockfd, (const char *)msg, strlen(msg),
		   0, (const struct sockaddr *)&servaddr,
		   sizeof(servaddr));
	closesocket(sockfd);

	WSACleanup();
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