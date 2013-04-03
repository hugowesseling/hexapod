#include <wiringSerial.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
  printf("Starting\n");
  int fd=serialOpen("/dev/ttyAMA0",115200);
  printf("serialOpen returns: %d\n",fd);
  if(fd==-1)exit(1);
  serialPuts(fd,"#1 P1400\r");
  serialClose(fd);
}
