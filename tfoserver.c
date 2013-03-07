/* conditional define for TCP_FASTOPEN */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif

/* conditional define for MSG_FASTOPEN */
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/tcp.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
    int listenfd, connfd, qlen = 5, num, port = 0, useTFO;
    struct sockaddr_in serv_addr;
    char sendBuff[1025];
    char readBuff[6000];
    time_t ticks;
    struct option options[] = {
        {"port", 1, NULL, 'p'},
        {"tfo", 0, NULL, 'f'}
    };
    int opt, longindex;

    while ((opt = getopt_long(argc, argv, "s:p:f", options, &longindex)) != -1) {
        switch(opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0) {
                    printf("Invalid port `%s'\n", optarg);
                    return 1;
                }
                break;
            case 'f':
                useTFO = 1;
                break;
        }
    }

    if (port == 0) {
        printf("Usage: %s -p <port> [-f]\n", argv[0]);
        printf("\t-p port | --port=port\tThe port the server is listening on\n");
        printf("\t-f | --tfo\t\tEnable TFO (disabled by default)\n");
        return 1;
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(sendBuff, 0, sizeof(sendBuff));

    if (useTFO) {
        setsockopt(listenfd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listenfd, 10) < 0) {
        perror("listen");
        return 1;
    }

    while (1) {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        if (connfd < 0) {
            perror("accept");
            return 1;
        }
        ticks = time(NULL);
        snprintf(sendBuff, sizeof(sendBuff), "Hi there, the current time is %.24s\r\n", ctime(&ticks));
        if (write(connfd, sendBuff, strlen(sendBuff)) < 0) {
            perror("write");
            return 1;
        }
        do {
            num = read(connfd, readBuff, sizeof(readBuff));
            if (num < 0) {
                perror("read");
                return 1;
            }
            write(1, readBuff, num);
        } while (num > 0);

        close(connfd);
    }
    close(listenfd);
    return 0;
}
