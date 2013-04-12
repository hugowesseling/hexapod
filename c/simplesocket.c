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

void simplesocket_error(const char *msg)
{
    perror(msg);
    exit(0);
}

struct LengthString
{
    int32_t length;
    char buffer[256];
};

bool simplesocket_receive(int sockfd, struct LengthString *lengthString)
{
  int n;
  n = read(sockfd,&(lengthString->length),4);
  if (n < 0)
  {
    // In case the timeout occured, this is not an error
    if (errno == EAGAIN)return false;
    simplesocket_error("ERROR:simplesocket_receive: Reading length from socket");
  }
  if ( lengthString->length>255 )simplesocket_error("ERROR:simplesocket_receive: Length of string to read to large");
  do
  {
    n = read(sockfd,lengthString->buffer,lengthString->length);
    if (n < 0)
    {
      // In case the timeout occured, this is not an error
      if (errno == EAGAIN)continue;
      simplesocket_error("ERROR:simplesocket_receive: reading buffer from socket");
    }
  }while(0);
  lengthString->buffer[lengthString->length]=0;
  return true;
}

void simplesocket_send(int sockfd,const char *str)
{
  int n;
  int32_t length=strlen(str);
  if (length > 255)simplesocket_error("ERROR:simplesocket_send: string to long");
  printf("Sending string with length %d\n",length);
  n = write(sockfd,&length,4);
  if (n < 0)simplesocket_error("ERROR:simplesocket_send: writing to socket1");

  n = write(sockfd,str,length);
  if (n < 0)simplesocket_error("ERROR:simplesocket_send: writing to socket2");
}

int simplesocket_create(int portno)
{
  int sockfd;
  int len;
  int connectresult;
  int n,i;
  struct LengthString lengthStringToSend,lengthStringToReceive;
  struct sockaddr_in address;
  struct timeval tv;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(portno);
  len = sizeof(address);

  connectresult = connect(sockfd, (struct sockaddr*)&address, len);
  if(connectresult<0)simplesocket_error("ERROR:simplesocket_create: connect");

  tv.tv_sec =  0;  // 1 microsecond Timeout
  tv.tv_usec = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
  return sockfd;
}
