/* 
 * A generic protection in case you include this 
 * from multiple files 
 */
#ifndef _KERNEL_FASTOPEN
#define _KERNEL_FASTOPEN
 
/* conditional define for TCP_FASTOPEN */
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN   23
#endif
 
/* conditional define for MSG_FASTOPEN */
#ifndef MSG_FASTOPEN
#define MSG_FASTOPEN   0x20000000
#endif
 
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
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr; 

    if(argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n",argv[0]);
        return 1;
    } 

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(12345); 

    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    char str[] = "hello world";
    
    sendto(sockfd, str, strlen(str), MSG_FASTOPEN, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));
    char readbuff[1000];
    int num = read(sockfd, readbuff, sizeof(readbuff)-1);
    readbuff[num] = 0;
    printf("%s\n", readbuff);

    if(n < 0)
    {
        printf("\n Read error \n");
    } 

    return 0;
}
