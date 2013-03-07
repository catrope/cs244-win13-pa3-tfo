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
#include <getopt.h>

int main(int argc, char *argv[])
{
    int sockfd, n, port = 0, useTFO = 0;
    char readbuff[1000];
    struct sockaddr_in serv_addr;
    char *query = NULL;
    int querySize, bytesRead;
    int opt, longindex, serverSet = 0;
    struct option options[] = {
        {"server", 1, NULL, 's'},
        {"port", 1, NULL, 'p'},
        {"tfo", 0, NULL, 'f'},
        {"query", 1, NULL, 'q'}
    };
    FILE *f = NULL;

    while ((opt = getopt_long(argc, argv, "s:p:fq:", options, &longindex)) != -1) {
        switch(opt) {
            case 's':
                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                if (inet_pton(AF_INET, optarg, &serv_addr.sin_addr) <=0)
                {
                    printf("Invalid IP address `%s'\n", optarg);
                    return 1;
                }
                serverSet = 1;
                break;
            case 'p':
                port = atoi(optarg);
                if (port <= 0) {
                    printf("Invalid port `%s'\n", optarg);
                    return 1;
                }
                serv_addr.sin_port = htons(port);
                break;
            case 'f':
                useTFO = 1;
                break;
            case 'q':
                f = fopen(optarg, "rb");
                if (!f) {
                    perror("fopen");
                    printf("Failed to open query file `%s'\n", optarg);
                }
                query = malloc(1000);
                querySize = 0;
                while(!feof(f)) {
                    bytesRead = fread(readbuff, 1, 1000, f);
                    if (bytesRead < 0) {
                        perror("fread");
                        return 1;
                    }
                    memcpy(&query[querySize], readbuff, bytesRead);
                    querySize += bytesRead;
                    query = realloc(query, querySize + 1000);
                }
                break;
        }
    }

    if (!serverSet || port == 0) {
        printf("Usage: %s -s <server IP> -p <server port> [-f] [-q <query file>]\n\n", argv[0]);
        printf("\t-s IP | --server=IP\tThe IP address of the server\n");
        printf("\t-p port | --port=port\tThe port the server is listening on\n");
        printf("\t-f | --tfo\t\tEnable TFO (disabled by default)\n");
        printf("\t-q | --query\t\tFile with query to send. If omitted, hello world will be sent\n");
        return 1;
    }
    if (!query) {
        query = "Hello world!";
        querySize = strlen(query);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    if (useTFO) {
        if (sendto(sockfd, query, querySize, MSG_FASTOPEN, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
            perror("sendto");
            return 1;
        }
    } else {
        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
            perror("connect");
            return 1;
        }
        n = write(sockfd, query, querySize);
        if (n < 0) {
            perror("write");
            return 1;
        }
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