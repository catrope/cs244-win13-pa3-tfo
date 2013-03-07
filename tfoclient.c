/* conditional define for TCP_FASTOPEN */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif
/* conditional define for MSG_FASTOPEN */
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
int main(int argc, char *argv[])
{
    int sockfd, n, port;
    char readbuff[1000];
    struct sockaddr_in serv_addr;
    char str[] = "hello world";
    if (argc < 3)
    {
        printf("Usage: %s <server IP> <server port> \n", argv[0]);
        return 1;
    }
    port = atoi(argv[2]);
    if (port <= 0) {
        printf("Invalid port number `%s'\n", argv[2]);
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    {
        printf("Invalid IP address `%s'\n", argv[1]);
        return 1;
    }

    if (sendto(sockfd, str, strlen(str), MSG_FASTOPEN, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
        perror("sendto");
        return 1;
    }

    n = read(sockfd, readbuff, sizeof(readbuff)-1);
    if (n < 0) {
        perror("read");
        return 1;
    }
    readbuff[n] = 0;
    printf("%s\n", readbuff);
    return 0;
}