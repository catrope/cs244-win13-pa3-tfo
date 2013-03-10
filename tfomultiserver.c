/* conditional define for TCP_FASTOPEN */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif

/* conditional define for MSG_FASTOPEN */
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
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
#include <sys/stat.h>
#include <fcntl.h>

struct connection {
    int fd;
    int identifiedResponse;
    int reqLen;
    char *rsBuf;
    int rsLen;
    int rsWritten;
    struct connection *prev, *next;
};

struct response {
    char *data;
    int dataLen;
    char *requestStart;
    int requestTotalLength;
};

struct connection *conns = NULL;
struct response *responses = NULL;
int numResponses;
int maxRequestStart;

void parseResponsesFile(FILE *f) {
    int i, fd, r, dr, rsLen;
    char filename[1024];
    char requestStart[8193];
    struct stat st;
    fscanf(f, "%d\n", &numResponses);
    responses = malloc(numResponses * sizeof(struct response));
    maxRequestStart = 0;
    for (i = 0; i < numResponses; i++) {
        fscanf(f, "%s %d ", filename, &responses[i].requestTotalLength);
        fd = open(filename, 0);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        fstat(fd, &st);
        responses[i].dataLen = st.st_size;
        responses[i].data = malloc(st.st_size);
        r = 0;
        while (r < st.st_size) {
            dr = read(fd, &responses[i].data[r], st.st_size - r);
            if (dr < 0) {
                perror("read");
                exit(1);
            }
            r += dr;
        }
        close(fd);

        fgets(requestStart, sizeof(requestStart) - 1, f);
        rsLen = strlen(requestStart);
        if (rsLen > 0 && requestStart[rsLen - 1] == '\n') {
            requestStart[rsLen - 1] = '\0';
            rsLen--;
        }
        responses[i].requestStart = malloc(rsLen + 1);
        strcpy(responses[i].requestStart, requestStart);
        maxRequestStart = MAX(maxRequestStart, rsLen);
    }
}

struct connection *newConnection(int connfd) {
    struct connection *c = malloc(sizeof(struct connection));
    c->fd = connfd;
    c->identifiedResponse = -1;
    c->reqLen = 0;
    c->rsLen = maxRequestStart;
    c->rsBuf = malloc(maxRequestStart);
    c->rsWritten = 0;
    c->prev = NULL;
    c->next = conns;
    if (conns) {
        conns->prev = c;
    }
    conns = c;
    return c;
}

void closeConnection(struct connection *conn) {
    if (conn->prev) {
        conn->prev->next = conn->next;
    } else {
        conns = conn->next;
    }
    if (conn->next) {
        conn->next->prev = conn->prev;
    }
    free(conn->rsBuf);
    close(conn->fd);
}

void identifyResponse(struct connection *conn) {
    int i;
    for (i = 0; i < numResponses; i++) {
        if (strncmp(conn->rsBuf, responses[i].requestStart, MIN(strlen(responses[i].requestStart), conn->rsLen)) == 0) {
            conn->identifiedResponse = i;
            return;
        }
    }
}

void sendResponse(struct connection *conn) {
    struct response *response;
    int w = 0, dw;
    response = &responses[conn->identifiedResponse];
    while (w < response->dataLen) {
        dw = write(conn->fd, &response->data[w], response->dataLen - w);
        if (dw < 0) {
            perror("write");
            exit(1);
        }
        w += dw;
    }
    conn->identifiedResponse = -1;
    conn->reqLen = 0;
    conn->rsWritten = 0;
}

void handleData(struct connection *conn) {
    int r, w;
    char buf[8192];
    r = read(conn->fd, buf, sizeof(buf));
    if (r == 0 || (r < 0 && errno == ECONNRESET)) {
        closeConnection(conn);
        return;
    } else if (r < 0) {
        perror("read");
        exit(1);
    }
    conn->reqLen += r;
    if (conn->identifiedResponse == -1 && conn->rsWritten < conn->rsLen) {
        w = MIN(conn->rsLen - conn->rsWritten, r);
        memcpy(&conn->rsBuf[conn->rsWritten], buf, w);
        conn->rsWritten += w;
        identifyResponse(conn);
    }
    if (conn->identifiedResponse != -1 && conn->reqLen >= responses[conn->identifiedResponse].requestTotalLength) {
        sendResponse(conn);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, qlen = 5, port = 0, useTFO;
    struct sockaddr_in serv_addr;
    struct option options[] = {
        {"port", 1, NULL, 'p'},
        {"tfo", 0, NULL, 'f'},
        {"responses", 1, NULL, 'r'}
    };
    int opt, longindex;
    fd_set selecting;
    int nfds = 0;
    struct connection *c, *nextC;
    FILE *f;

    while ((opt = getopt_long(argc, argv, "p:fr:", options, &longindex)) != -1) {
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
            case 'r':
                f = fopen(optarg, "rb");
                if (!f) {
                    perror("fopen");
                    printf("Failed to open responses file `%s'\n", optarg);
                    return 1;
                }
                parseResponsesFile(f);
                fclose(f);
        }
    }

    if (port == 0 || !responses) {
        printf("Usage: %s -p <port> -r <responses file> [-f]\n", argv[0]);
        printf("\t-p port | --port=port\t\t\tThe port the server is listening on\n");
        printf("\t-f | --tfo\t\t\t\tEnable TFO (disabled by default)\n");
        printf("\t-r filename | --responses=filename\tFile specifying responses\n");
        return 1;
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));

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
        FD_ZERO(&selecting);
        FD_SET(listenfd, &selecting);
        nfds = listenfd;
        for (c = conns; c; c = c->next) {
            FD_SET(c->fd, &selecting);
            nfds = MAX(c->fd, nfds);
        }
        nfds++;
        if (select(nfds, &selecting, NULL, NULL, NULL) < 0) {
            perror("select");
            return 1;
        }

        if (FD_ISSET(listenfd, &selecting)) {
            connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
            if (connfd < 0) {
                perror("accept");
                return 1;
            }
            newConnection(connfd);
        }
        c = conns;
        while (c) {
            nextC = c->next;
            if (FD_ISSET(c->fd, &selecting)) {
                handleData(c);
            }
            c = nextC;
        }
    }
    return 0;
}
