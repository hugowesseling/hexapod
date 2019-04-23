#include <wiringSerial.h>
#include <stdlib.h>
#include <stdio.h>

// according to http://www.acroname.com/robotics/info/articles/irlinear/irlinear.html : R = (6787 / (V - 3)) - 4
float ir2Distance(int val)
{
  if(val<21)return 80;
  return ( 6787.0 / (val*4 - 3))-4;
}

int main(int argc,char *argv[])
{
  printf("Run twice first time for correct values\n");
  int fd=serialOpen("/dev/ttyUSB0",115200);
  printf("serialOpen returns: %d\n",fd);
  if(fd==-1)exit(1);
  serialPuts(fd,"VA\r");
  int irVoltage=serialGetchar(fd);
  serialPuts(fd,"VB\r");
  int voltage6V=serialGetchar(fd);
  serialPuts(fd,"VC\r");
  int voltage9V=serialGetchar(fd);
  printf("6V=%g, 9V=%g  IR=%d(%g cm)\n",10.0f*voltage6V/255,10.0f*voltage9V/255,irVoltage,ir2Distance(irVoltage));
  serialClose(fd);
}
