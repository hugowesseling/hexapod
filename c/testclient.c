#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in saClient;
    struct hostent *localHost;
    char *localIP;
    char buffer[256];

    printf("Starting\n");
    portno = 12345;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    localHost = gethostbyname("localhost");
    if (localHost == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    localIP = inet_ntoa (*(struct in_addr *)*localHost->h_addr_list);
    printf("Filling server address with %s\n",localIP);

    saClient.sin_family = AF_UNSPEC;
    saClient.sin_addr.s_addr = inet_addr(localIP);
    saClient.sin_port = htons(portno);

    printf("Attempting to connect\n");
    if (connect(sockfd,(struct sockaddr *)&saClient,sizeof(saClient)) < 0) 
        error("ERROR connecting");
    printf("Please enter the message: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);
    close(sockfd);
    return 0;
}
