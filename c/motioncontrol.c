#include <wiringSerial.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "simplesocket.c"

using namespace std;

bool legCrossedBoundary=false;

const char *ZEROSTRING="#21PO-50 #7PO-20 #6PO-20 #8PO30 #16PO-20 #18PO30\r";
#define LEGCNT 6

//gait modes
#define M_WALKING 0
#define M_STAND6  1 //stand with 6 legs on ground
#define M_STAND4  2 //stand with 4 legs on ground

//sequence steps
#define S_GROUND 0
#define S_MOVEUP 1
#define S_MOVEDOWN 2

#define STEPCNT 4

typedef struct T_Position
{
	float x,y,z;
} Position;

Position createPosition(float x,float y,float z)
{
  Position ret{x,y,z};
  return ret;
}

typedef struct T_World
{
	Position trans;
	Position rot;
} World;

Position worldToPod(World *world,Position worldPos)
{
  float cosrx=cosf(world->rot.x),sinrx=sinf(world->rot.x);
  float cosrz=cosf(world->rot.z),sinrz=sinf(world->rot.z);
  Position transPos{worldPos.x+world->trans.x,
                    worldPos.y+world->trans.y,
                    worldPos.z+world->trans.z};
  Position posrz{transPos.x*cosrz+transPos.y*sinrz,
                 transPos.y*cosrz-transPos.x*sinrz,
                 transPos.z};
  Position podPos{posrz.x,
                  posrz.y*cosrx+posrz.z*sinrx,
                  posrz.z*cosrx-posrz.y*sinrx};
  return podPos;
}

Position podToWorld(World *world,Position podpos)
{
  float cosrx=cos(-world->rot.x),sinrx=sin(-world->rot.x);
  float cosrz=cos(-world->rot.z),sinrz=sin(-world->rot.z);
  Position posrz{podpos.x,
                 podpos.y*cosrx+podpos.z*sinrx,
                 podpos.z*cosrx-podpos.y*sinrx};
  Position transPos{posrz.x*cosrz+posrz.y*sinrz,
                    posrz.y*cosrz-posrz.x*sinrz,
                    posrz.z};
  Position worldPos{transPos.x-world->trans.x,
                    transPos.y-world->trans.y,
                    transPos.z-world->trans.z};
  return worldPos;
}
/* Not needed
typedef struct T_Pos2d
{
  float x,y;
} Pos2d;
*/

float coxaLength=2.1;
float femurLength=8.0;
float tibiaLength=13.0;

typedef struct T_Leg
{
  int servoStartPos;
  bool isTripodA;
  bool previousOnGround;
  Position groundPos,neutralAirPos,st4Pos;
  Position tipPos;
  Position jointPos;
  float coxaZeroRotation;
  Position femurStart;
  int prevStep;
  int mode,groundPositionForMode;
} Leg;

typedef struct T_Angles
{
  float vL;
  float vu;
  float h;
} Angles;

Leg createLeg(Position groundPos,Position jointPos,Position st4Pos,float coxaZeroRotation,int servoStartPos,bool isTripodA,bool previousOnGround)
{
  Leg leg;
  leg.servoStartPos=servoStartPos;
  leg.isTripodA=isTripodA;
  leg.previousOnGround=previousOnGround;
  leg.groundPos=groundPos;
  leg.st4Pos=st4Pos;
  Position neutralAirPos{groundPos.x,groundPos.y,-4};
  leg.neutralAirPos=neutralAirPos;
  leg.tipPos=groundPos;
  leg.jointPos=jointPos;
  leg.coxaZeroRotation=coxaZeroRotation;
  leg.prevStep=0;
  leg.mode=M_STAND6;
  leg.groundPositionForMode=-1;  //invalid ground position
  return leg;
}
Position interpolatePosition(Position pos1,Position pos2,float alpha)
{
  Position pos{pos1.x*(1-alpha)+pos2.x*alpha,
               pos1.y*(1-alpha)+pos2.y*alpha,
               pos1.z*(1-alpha)+pos2.z*alpha};
  return pos;
}


int sequenceMap[2][STEPCNT]={
		{S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN},
		{S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND} };


// See tripod sequence here: http://www.lynxmotion.com/images/assembly/ssc32/h2seqdia.gif
void setSeqPos(Leg *leg,int step,float partial,World *world,float moveX,float moveY,float groundZ,int mode)
{
  int status=S_GROUND;
  // leg->mode plays catch-up with mode given to this function. A leg has to run through part of the sequence before reaching a mode
  switch(leg->mode)
  {
    case M_WALKING:
      status=sequenceMap[leg->isTripodA][step];
      break;
    case M_STAND6:
      status=S_GROUND;
      break;
    case M_STAND4:
      status=S_GROUND;
      break;
  }
  switch(status)
  {
    case S_GROUND:
      leg->tipPos= worldToPod(world,leg->groundPos);
      //leg can only change it's mode to M_STAND6 or M_STAND4 if the groundposition is correct for that mode
      if( (mode==M_WALKING) || ((mode==M_STAND6 || mode==M_STAND4) && leg->groundPositionForMode==mode) )
        leg->mode=mode;
      break;
    case S_MOVEUP:
      leg->tipPos=interpolatePosition(worldToPod(world,leg->groundPos),leg->neutralAirPos,partial);
      break;
    case S_MOVEDOWN:
      if(step!=leg->prevStep)
      {
        //calculate new groundPosition
        //for now use old position with some translation
        Position worldGroundPos;
        if(mode==M_STAND4)
          worldGroundPos=createPosition(leg->st4Pos.x,leg->st4Pos.y,groundZ);
        else
          worldGroundPos=createPosition(leg->neutralAirPos.x,leg->neutralAirPos.y,groundZ);
        if(mode==M_WALKING) //only adjust ground position away from neutralposition if walking
        {
          //New groundPosition=<airpos projected on ground> + <movement>
          worldGroundPos.x-=moveX;
          worldGroundPos.y-=moveY;
        }
        leg->groundPos=podToWorld(world,worldGroundPos);
        //Store for which mode the ground position is calculated
        leg->groundPositionForMode=mode;
      }
      leg->tipPos=interpolatePosition(leg->neutralAirPos,worldToPod(world,leg->groundPos),partial);
      break;
  }
  leg->prevStep=step;
}

float angleModPiMinHalfPiToHalfPi(float angle)
{
  while(angle<-0.5*M_PI)angle+=M_PI;
  while(angle>0.5*M_PI)angle-=M_PI;
  return angle;
}

// Calculate the angles needed to reach the tipPos from jointPos
Angles calculateAngles(Leg *leg)
{
  //x+ right
  //y+ front
  //z+ up in the air 
  //First calculate the horizontal angle
  float h=atan2(leg->tipPos.y-leg->jointPos.y,leg->tipPos.x-leg->jointPos.x);
  //h angle can not be more than 90 degrees from coxaZeroRotation
  h=angleModPiMinHalfPiToHalfPi(h-leg->coxaZeroRotation)+leg->coxaZeroRotation;
  
  Position femurStart{leg->jointPos.x+cosf(h)*coxaLength,leg->jointPos.y+sinf(h)*coxaLength,leg->jointPos.z};
  leg->femurStart=femurStart;
  //calculate 2d differences for two joint solution
  //calculate with sincos instead (or somehow get negative numbers: maybe dotproduct with coxa vector, no: rotate femurstart->tip vector using sin(h) & cos(h))
  //float xydist=(float)Math.hypot(femurStart.x-tipPos.x,femurStart.y-tipPos.y);
  float xydist= (leg->tipPos.x-leg->femurStart.x)*cos(h)+(leg->tipPos.y-leg->femurStart.y)*sin(h);
  float zdist=leg->tipPos.z-leg->femurStart.z;
  //now use xydist and zdist to calculate vl and vu
  //len1=femurlength, len2=tibiaLength
  float squaredist=xydist*xydist+zdist*zdist;
//  printf("squaredist: %g\n",squaredist);
  float partialsqr=((femurLength+tibiaLength)*(femurLength+tibiaLength)-squaredist)/
                      (squaredist-(femurLength-tibiaLength)*(femurLength-tibiaLength));
  if(partialsqr<0)
  {
    legCrossedBoundary=true;
  }else
  {
    float partial=-sqrt(partialsqr);
    float vl=2*atanf(partial);

    float part1=femurLength+tibiaLength*cos(vl);
    float part2=tibiaLength*sin(vl);
//    printf("part1: %g, part2: %g, vl: %g\n",part1,part2,vl);
    float cosVu=(xydist*part1+zdist*part2);
    float sinVu=(zdist*part1-xydist*part2);
    float vu=atan2f(sinVu,cosVu);
//    printf("cosVu: %g, sinVu: %g, vu:%g\n",cosVu,sinVu,vu);
    Angles ret{vl,vu,h};
    return ret;
  }

  Angles ret{0,0,0};
  return ret;
}

int convertAndCap(float angle,int servoMin,int servoMax)
{
  int out=(int)((180*angle/M_PI)*1000/60);
  if(out<servoMin)
  {
    out=servoMin;
    legCrossedBoundary=true;
  }
  if(out>servoMax)
  {
    out=servoMax;
    legCrossedBoundary=true;
  }
  return out;
}

float angleModMinPiToPi(float angle)
{
  while(angle<-M_PI)angle+=2*M_PI;
  while(angle>M_PI)angle-=2*M_PI;
  return angle;
}


//strbuf must have at least a size of 3x(1+2+1+4+1=9)=27 (=+-30 with safety)
void getHexCommands(Leg *leg,char *strbuf) {
  Angles angles=calculateAngles(leg);
  int vlServoPos=convertAndCap(angles.vL+(180-43)*M_PI/180,-100,1000);
  int vuServoPos=convertAndCap(-angles.vu+(90-43)*M_PI/180,-500,1000);
  int hServoPos=convertAndCap(angleModMinPiToPi(-angles.h+leg->coxaZeroRotation),-700,700);
  if(leg->servoStartPos>=16)
  {
    snprintf(strbuf,30,"#%dP%d #%dP%d #%dP%d ",
            leg->servoStartPos,  1500-vlServoPos,
            leg->servoStartPos+1,1500-vuServoPos,
            leg->servoStartPos+2,1500+ hServoPos);
  }else
  {
    snprintf(strbuf,30,"#%dP%d #%dP%d #%dP%d ",
            leg->servoStartPos,  1500+vlServoPos,
            leg->servoStartPos+1,1500+vuServoPos,
            leg->servoStartPos+2,1500+ hServoPos);
  }
}


// according to http://www.acroname.com/robotics/info/articles/irlinear/irlinear.html : R = (6787 / (V - 3)) - 4
float ir2Distance(int val)
{
  if(val<21)return 80;
  return ( 6787.0 / (val*4 - 3))-4;
}

timespec timespecDiff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

float getDistance(int fd)
{
  serialPuts(fd,(char *)"VA\r");
  int irVoltage=serialGetchar(fd);
  float dist=ir2Distance(irVoltage);
  printf("Distance: %g\n",dist);
  return dist;
}

void readvoltages(int fd)
{
  serialPuts(fd,(char *)"VB\r");
  int voltage6V=serialGetchar(fd);
  serialPuts(fd,(char *)"VC\r");
  int voltage9V=serialGetchar(fd);
  printf("6V=%g, 9V=%g\n",10.0f*voltage6V/255,10.0f*voltage9V/255);
}

int main(int argc,char *argv[])
{
  char buffer[256];
  struct LengthString lengthStringToReceive;
  int sock=simplesocket_create(12345);


  struct timespec lastScanTime,curTime,diffTime;
  World world{{0,0,0},{0,0,0}};
  Leg legs[LEGCNT];

#define JntFBX 4
#define JntMX 5.85
#define JntFBY 8
#define GndFBX 13
#define GndMX 14.85
#define GndFBY 13
#define GndZ (-8)
#define JntZ 0
#define St4FX GndFBX
#define St4MX 16
#define St4BX GndFBX
#define St4FY -15
#define St4MY -6
#define St4BY GndFBY

  //left legs (-x)
  legs[0]=createLeg(createPosition(-GndFBX, -GndFBY,GndZ),createPosition(-JntFBX,-JntFBY,JntZ),createPosition(-St4FX, St4FY,GndZ),(-180+22)*M_PI/180,2,true, true);  //front
  legs[1]=createLeg(createPosition(-GndMX,        0,GndZ),createPosition(-JntMX,       0,JntZ),createPosition(-St4MX, St4MY,GndZ),(-180   )*M_PI/180,5,false,true);  //mid
  legs[2]=createLeg(createPosition(-GndFBX,  GndFBY,GndZ),createPosition(-JntFBX, JntFBY,JntZ),createPosition(-St4BX, St4BY,GndZ),(-180-22)*M_PI/180,8,true, true);  //back

  //right legs (+x)
  legs[3]=createLeg(createPosition( GndFBX, -GndFBY,GndZ),createPosition( JntFBX,-JntFBY,JntZ),createPosition( St4FX, St4FY,GndZ),     -22 *M_PI/180,16,false,true);   //back
  legs[4]=createLeg(createPosition( GndMX,        0,GndZ),createPosition( JntMX,       0,JntZ),createPosition( St4MX, St4MY,GndZ),       0 *M_PI/180,19,true, true);   //mid
  legs[5]=createLeg(createPosition( GndFBX,  GndFBY,GndZ),createPosition( JntFBX, JntFBY,JntZ),createPosition( St4BX, St4BY,GndZ),      22 *M_PI/180,22,false,true);  //front   

  printf("Starting\n");
  int fd=serialOpen((char *)"/dev/ttyAMA0",115200);
  printf("serialOpen returns: %d\n",fd);
  if(fd==-1)exit(1);
  char serialBuffer[1000];
  char partialBuffer[31];
  int i;
  int step=0;
  float partial=0;
  float moveX=0,moveY=0;
  float drotz=0;
  float dist;
  Position rot=createPosition(0,0,0);
  bool scanDirectionRight=true;
  int modeCounter=0;

  clock_gettime(CLOCK_MONOTONIC,&lastScanTime);
  while(true)
  {
    printf("Receiving..\n");
    if(simplesocket_receive(sock,&lengthStringToReceive))
    {
      printf("Received:%s\n",lengthStringToReceive.buffer);
      if(lengthStringToReceive.buffer[0]=='R')
      {
        sscanf(lengthStringToReceive.buffer,"R %f %f %f",&rot.x,&rot.y,&rot.z);
        printf("New z rotation: %f\n",rot.z);
      }
      //send acknowledgement
      simplesocket_send(sock,".");
    }

    clock_gettime(CLOCK_MONOTONIC,&curTime);
    diffTime=timespecDiff(lastScanTime,curTime);
    if(diffTime.tv_sec>=1)
    {
      lastScanTime=curTime;
      if(scanDirectionRight)
      {
        serialPuts(fd,(char *)"#1 P1650 S300\r");
      }else
      {
        serialPuts(fd,(char *)"#1 P1350 S300\r");
      }
      scanDirectionRight=!scanDirectionRight;
    }
    readvoltages(fd);
    dist=getDistance(fd);
    float maxSpeed=4.0f,speed=0.0f;
    if(dist>40)
    {
      moveX=0.0f;
      moveY=maxSpeed;
      drotz=0.0f;
      printf("FULL SPEED\n");
    }else
    if(dist>20)
    {
      moveX=0.0f;
      moveY=0.5f*maxSpeed;
      drotz=0.0f;
      printf("HALF SPEED\n");
    }else
    {
      moveX=0.0f;
      moveY=0.0f;;
      drotz=0.02f;
      printf("ROTATE\n");
    }
    int mode=modeCounter&16?M_WALKING:M_STAND4;
    if(mode==M_STAND4 || mode==M_STAND6) //stop all movement if standing
    {
      moveX=moveY=0;
    }

    float cosrz=cosf(-world.rot.z),sinrz=sinf(-world.rot.z);
    float moveGroundX=moveX*cosrz+moveY*sinrz;
    float moveGroundY=moveY*cosrz-moveX*sinrz;
    modeCounter++;
    serialBuffer[0]=0;
    printf("MODE: %d\n",mode);
    for(i=0;i<LEGCNT;i++)
    {
       setSeqPos(&legs[i],step,partial,&world,moveX,moveY,-8,mode);
       getHexCommands(&legs[i],partialBuffer);
       strncat(serialBuffer,partialBuffer,1000);
    }
    printf("serialBuffer: %s\n",serialBuffer);
    strncat(serialBuffer,"T100\r",1000);
    if(legCrossedBoundary)
      printf("Crossed boundary!\n");

    serialPuts(fd,serialBuffer);

    partial+=0.5;
    if(partial>=1)
    {
      partial-=1;
      step++;
      if(step>=STEPCNT)step=0;
    }
    printf("Step: %d, partial: %g\n",step,partial);
    //move speed max = move(X,Y) / ( 8 / partialAdd) ?

    world.trans.x+=moveGroundX/6.0;
    world.trans.y+=moveGroundY/6.0;
    world.rot.x=0;//rot.x;
    world.rot.y=0;//rot.y;
    world.rot.z=0;//rot.z; //+=drotz;
    usleep(100000);
  }
  serialClose(fd);
  close(sock);
}
