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

struct LengthString
{
    int32_t length;
    char buffer[256];
};

int main(int argc, char *argv[])
{
  int sockfd;
  int len;
  int result;
  int n;
  struct LengthString lengthString;
  struct sockaddr_in address;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(12345);
  len = sizeof(address);

  result = connect(sockfd, (struct sockaddr*)&address, len);
  if(result<0)
  {
    error("ERROR connecting");
  }
  printf("Please enter the message: ");
  fgets(lengthString.buffer,255,stdin);
  lengthString.length=strlen(lengthString.buffer);
  n = write(sockfd,&lengthString,lengthString.length+4);
  if (n < 0) 
       error("ERROR writing to socket");
  bzero(lengthString.buffer,256);
  n = read(sockfd,lengthString.buffer,255);
  if (n < 0) 
       error("ERROR reading from socket");
  printf("%s\n",lengthString.buffer);
  close(sockfd);
  return 0;
}
