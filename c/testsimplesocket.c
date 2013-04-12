#include "simplesocket.c"

int main(int argc,char *argv[])
{
  char buffer[256];
  struct LengthString lengthStringToReceive;
  int sock=simplesocket_create(12345);
  int i;
  printf("Please enter the message: ");
  fgets(buffer,255,stdin);

  for(i=0;i<50;i++)
  {
    if(i%3==0)
    {
      simplesocket_send(sock,buffer);
    }
    printf("Receiving..\n");
    if(simplesocket_receive(sock,&lengthStringToReceive))
    {
      printf("Received:%s\n",lengthStringToReceive.buffer);
    }
    usleep(500000);
  }
  close(sock);
  return 0;
}

