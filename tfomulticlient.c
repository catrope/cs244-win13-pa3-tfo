/* conditional define for TCP_FASTOPEN */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif

/* conditional define for MSG_FASTOPEN */
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define HEAD_THRESHOLD 4096

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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

struct request {
    int fd;
    int group;
    int connection;
    int waitForHead;
    int waitForCompletion;
    int responseLength;
    int headLength;
};

struct request *reqs = NULL;
int numReqs;
int *connections;
int *reqForConn;
int numConns;
fd_set openConns;
int maxOpenConn = 0;
int *groups;
int *groupsHead;
int numGroups;
int useTFO = 0;
struct sockaddr_in serv_addr;

void readReqsFromFile(FILE *f) {
    int i, maxGroup = 0, maxConn = 0;
    char filename[1025];
    fscanf(f, "%d\n", &numReqs);
    reqs = malloc(numReqs * sizeof(struct request));
    for (i = 0; i < numReqs; i++) {
        fscanf(f, "%s %d %d %d %d%d\n", filename, &reqs[i].group, &reqs[i].connection,
            &reqs[i].waitForHead, &reqs[i].waitForCompletion, &reqs[i].responseLength);
        reqs[i].fd = open(filename, 0);
        if (reqs[i].fd < 0) {
            perror("open");
            exit(1);
        }
        maxGroup = reqs[i].group > maxGroup ? reqs[i].group : maxGroup;
        maxConn = reqs[i].connection > maxConn ? reqs[i].connection : maxConn;
    }
    numConns = maxConn + 1;
    connections = malloc(numConns*sizeof(int));
    reqForConn = malloc(numConns*sizeof(int));
    numGroups = maxGroup + 1;
    groups = malloc(numGroups*sizeof(int));
    groupsHead = malloc(numGroups*sizeof(int));
    memset(connections, 0, numConns*sizeof(int));
    memset(groups, 0, numGroups*sizeof(int));
    memset(groupsHead, 0, numGroups*sizeof(int));
    for (i = 0; i < numReqs; i++) {
        groups[reqs[i].group]++;
        groupsHead[reqs[i].group]++;
    }
    for (i = 0; i < numConns; i++) {
        reqForConn[i] = -1;
    }
}

void sendRequest(int i) {
    int j, r = 0, dr, w = 0, dw, existingConn;
    struct stat st;
    char *buf;
    /* Stat the file to find out its size */
    fstat(reqs[i].fd, &st);
    /* Allocate a buffer for the entire file */
    buf = malloc(st.st_size);
    /* Read the entire file into the buffer */
    while (r < st.st_size) {
        dr = read(reqs[i].fd, buf, st.st_size);
        if (dr < 0) {
            perror("read");
        }
        r += dr;
    }
    close(reqs[i].fd);

    /* Check if the connection has already been set up */
    j = reqs[i].connection;
    existingConn = connections[j] > 0;
    if (!existingConn) {
        /* Create a new socket */
        connections[j] = socket(AF_INET, SOCK_STREAM, 0);
        if (connections[j] < 0) {
            perror("socket");
            exit(1);
        }
        if (useTFO) {
            /* Use sendto() to open the connection with data in the SYN */
            w = sendto(connections[j], buf, st.st_size, MSG_FASTOPEN, 
                (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
            if (w < 0) {
                perror("sendto");
                exit(1);
            }
        } else {
            /* Use connect() to open the connection */
            if (connect(connections[j], (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
                perror("connect");
                exit(1);
            }
        }
    }
    reqForConn[j] = i;
    FD_SET(connections[j], &openConns);
    maxOpenConn = connections[j] > maxOpenConn ? connections[j] : maxOpenConn;

    /* Write the request. For the new TFO connection case, this will write whatever sendto() didn't
     * write; for the other cases, it'll write everything. */
    while (w < st.st_size) {
        dw = write(connections[j], &buf[w], st.st_size - w);
        if (dw < 0) {
            perror("write");
            exit(1);
        }
        w += dw;
    }
}

void handleResponse(int i) {
    int j, r;
    char buf[8193];
    j = reqForConn[i];
    r = read(connections[i], buf, sizeof(buf) - 1);
    buf[r] = 0;
    reqs[j].responseLength -= r;
    reqs[j].headLength -= r;
    if (reqs[j].headLength <= 0) {
        groupsHead[reqs[j].group]--;
    }
    if (reqs[j].responseLength <= 0) {
        groups[reqs[j].group]--;
        reqForConn[i] = -1;
        FD_CLR(connections[i], &openConns);
    }
}

int getReadyRequest() {
    int i;
    for (i = 0; i < numReqs; i++) {
        if (
            reqs[i].responseLength > 0 &&
            reqForConn[reqs[i].connection] == -1 &&
            (
                reqs[i].waitForHead < 0 ||
                groupsHead[reqs[i].waitForHead] <= 0
            ) && (
                reqs[i].waitForCompletion < 0 ||
                groups[reqs[i].waitForCompletion] <= 0
            )
        ) {
            return i;
        }
    }
    return -1;
}

int allDone() {
    int i;
    for (i = 0; i < numReqs; i++) {
        if (reqs[i].responseLength > 0) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int port = 0, opt, longindex, serverSet = 0, i, nextReq;
    fd_set fds;
    struct timeval start, end;
    struct option options[] = {
        {"server", 1, NULL, 's'},
        {"port", 1, NULL, 'p'},
        {"tfo", 0, NULL, 'f'},
        {"requests", 1, NULL, 'r'}
    };
    FILE *f = NULL;
    FD_ZERO(&openConns);

    while ((opt = getopt_long(argc, argv, "s:p:fr:", options, &longindex)) != -1) {
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
            case 'r':
                f = fopen(optarg, "rb");
                if (!f) {
                    perror("fopen");
                    printf("Failed to open requests file `%s'\n", optarg);
                    return 1;
                }
                readReqsFromFile(f);
                fclose(f);
                break;
        }
    }

    if (!serverSet || port == 0 || !reqs) {
        printf("Usage: %s -s <server IP> -p <server port> -r <requests file> [-f]\n\n", argv[0]);
        printf("\t-s IP | --server=IP\t\t\tThe IP address of the server\n");
        printf("\t-p port | --port=port\t\t\tThe port the server is listening on\n");
        printf("\t-f | --tfo\t\t\t\tEnable TFO (disabled by default)\n");
        printf("\t-r filename | --requests=filename\tFile specifying requests\n");
        return 1;
    }

    gettimeofday(&start, NULL);
    while (!allDone()) {
	do {
	        nextReq = getReadyRequest();
	        if (nextReq >= 0) {
	            sendRequest(nextReq);
	        }
	} while (nextReq >= 0);
        FD_ZERO(&fds);
        for (i = 0; i < numConns; i++) {
            if (FD_ISSET(connections[i], &openConns)) {
                FD_SET(connections[i], &fds);
            }
        }
        if (select(maxOpenConn + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }
        for (i = 0; i < numConns; i++) {
            if (FD_ISSET(connections[i], &fds)) {
                handleResponse(i);
            }
        }
    }
    gettimeofday(&end, NULL);
    for (i = 0; i < numConns; i++) {
        if (connections[i] >= 0) {
            close(connections[i]);
        }
    }

    printf("%ld\n", 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec);

    return 0;
}
