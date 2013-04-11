#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include <errno.h>

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

bool receive(int sockfd, struct LengthString *lengthString)
{
  int n;
  n = read(sockfd,&(lengthString->length),4);
  if (n < 0)
  {
    // In case the timeout occured, this is not an error
    if (errno == EAGAIN)return false;
    error("ERROR: reading length from socket");
  }
  if ( lengthString->length>255 )error("ERROR: Length of string to read to large");
  do
  {
    n = read(sockfd,lengthString->buffer,lengthString->length);
    if (n < 0)
    {
      // In case the timeout occured, this is not an error
      if (errno == EAGAIN)continue;
      error("ERROR: reading buffer from socket");
    }
  }while(0);
  lengthString->buffer[lengthString->length]=0;
  return true;
}


int main(int argc, char *argv[])
{
  int sockfd;
  int len;
  int result;
  int n,i;
  struct LengthString lengthStringToSend,lengthStringToReceive;
  struct sockaddr_in address;
  struct timeval tv;

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

  tv.tv_sec =  0;  /* 50 ms Timeout */
  tv.tv_usec = 50000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

  printf("Please enter the message: ");
  fgets(lengthStringToSend.buffer,255,stdin);
  lengthStringToSend.length=strlen(lengthStringToSend.buffer);

  for(i=0;i<50;i++)
  {
    if(i%3==0)
    {
      printf("Sending string with length %d\n",lengthStringToSend.length);
      n = write(sockfd,&lengthStringToSend,lengthStringToSend.length+4);
      if (n < 0)error("ERROR writing to socket1");
    }
    printf("Receiving..\n");
    if(receive(sockfd,&lengthStringToReceive))
    {
      printf("Received:%s\n",lengthStringToReceive.buffer);
    }
    usleep(500000);
  }
  close(sockfd);
  return 0;
}
