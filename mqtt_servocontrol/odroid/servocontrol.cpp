// First: Add single leg gate to gracefully get into stand4 mode
// To do that: Create mapping from leg to leg group (tripodgait has two groups, ripple three and single leg six)
// Create seqMaps for all and link them all in a single pointer array
// Next step: create long temporal command: walkfor <steps>
// Command will execute, and send "Command End" upon completion

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "serial_util.c"
#include "mqtt_connection.h"

using namespace std;

bool legCrossedBoundary=false;

const char *ZEROSTRING="#21PO-50 #7PO-20 #6PO-20 #8PO30 #16PO-20 #18PO30\r";
const char *POWEREDDOWNSTRING="#0L #1L #2L #3L #4L #5L #6L #7L #8L #9L #10L #11L #12L #13L #14L #15L #16L #17L #18L #19L #20L #21L #22L #23L #24L #25L #26L #27L #28L #29L #30L #31L \r";

#define SLEEP_MICROS 50000

#define LEGCNT 6

//gait modes
#define M_WALKING 0
#define M_STAND6  1 //stand with 6 legs on ground
#define M_STAND4  2 //stand with 4 legs on ground

//sequence steps
#define S_GROUND 0
#define S_MOVEUP 1
#define S_MOVEDOWN 2

#define GROUND_Z -8 //position of ground compared to zero of pod


typedef struct T_Position
{
	float x,y,z;
} Position;

Position createPosition(float x,float y,float z)
{
  Position ret = {x,y,z};
  return ret;
}

Position pos(float x, float y, float z)
{
  Position ret = {x,y,z};
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
  float cosry=cosf(world->rot.y),sinry=sinf(world->rot.y);
  float cosrz=cosf(world->rot.z),sinrz=sinf(world->rot.z);
  Position transPos = {worldPos.x+world->trans.x,
                       worldPos.y+world->trans.y,
                       worldPos.z+world->trans.z};
  Position posrz = {transPos.x*cosrz+transPos.y*sinrz,
                    transPos.y*cosrz-transPos.x*sinrz,
                    transPos.z};
  Position posrzy = {posrz.x*cosry+posrz.z*sinry,
                     posrz.y,
                     posrz.z*cosry-posrz.x*sinry};
  Position podPos = {posrz.x,
                     posrzy.y*cosrx+posrzy.z*sinrx,
                     posrzy.z*cosrx-posrzy.y*sinrx};
  return podPos;
}

Position podToWorld(World *world,Position podpos)
{
  float cosrx=cosf(-world->rot.x),sinrx=sinf(-world->rot.x);
  float cosry=cosf(-world->rot.y),sinry=sinf(-world->rot.y);
  float cosrz=cosf(-world->rot.z),sinrz=sinf(-world->rot.z);
  Position posrzy = {podpos.x,
                    podpos.y*cosrx+podpos.z*sinrx,
                    podpos.z*cosrx-podpos.y*sinrx};
  Position posrz = {posrzy.x*cosry+posrzy.z*sinry,
                    posrzy.y,
                    posrzy.z*cosry-posrzy.x*sinry};
  Position transPos = {posrz.x*cosrz+posrz.y*sinrz,
                       posrz.y*cosrz-posrz.x*sinrz,
                       posrz.z};
  Position worldPos = {transPos.x-world->trans.x,
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
  int legNr;
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

enum
{
  Com_Move,
  Com_Rotate,
  Com_Stand4,
  Com_Stand6,
  Com_Gesture
};

typedef struct T_Gesture_Step
{
  int ticks;
  int legmode;
  float headpan,headtilt;
  Position frontlegpos;
  Position worldtrans,worldrot;
} GestureStep;

GestureStep createGSHeadMove(int ticks, float headpan, float headtilt)
{
  GestureStep gs={ticks, 0, 0,0, 0,0,0, 0,0,0, 0,0,0};
  gs.legmode = M_STAND6;
  gs.headpan = headpan;
  gs.headtilt = headtilt;
  return gs;
}

GestureStep createGSWorldTransRot(int ticks, Position trans, Position rot)
{
  GestureStep gs={ticks, 0, 0,0, 0,0,0, 0,0,0, 0,0,0};
  gs.legmode = M_STAND6;
  gs.worldtrans = trans;
  gs.worldrot = rot;
  return gs;
}

#define MAX_GESTURE_STEPS 10
#define G_YES 0
#define G_NO 1
#define G_BOW 2
#define G_UPDOWN 3
#define G_STANDUP 4
#define G_COUNT 5

GestureStep gestures[G_COUNT][MAX_GESTURE_STEPS]=
{
  {createGSHeadMove(2,0,0), createGSHeadMove(1,0,-1), createGSHeadMove(1,0,1), createGSHeadMove(1,0,0)},
  {createGSHeadMove(2,0,0), createGSHeadMove(1,-0.5,0), createGSHeadMove(1,0.5,0), createGSHeadMove(1,0,0)},
  {createGSWorldTransRot(2,pos(0,0,0),pos(0,0,0)), createGSWorldTransRot(10,pos(0,0,0),pos(0.4,0,0)),createGSWorldTransRot(20,pos(0,0,0),pos(-0.4,0,0)),createGSWorldTransRot(10,pos(0,0,0),pos(0,0,0))},
  {createGSWorldTransRot(2,pos(0,0,0),pos(0,0,0)), createGSWorldTransRot(10,pos(0,0,2),pos(0,0,0)),createGSWorldTransRot(20,pos(0,0,-2),pos(0,0,0)),createGSWorldTransRot(10,pos(0,0,0),pos(0,0,0))},
  {createGSWorldTransRot(0,pos(0,0,4),pos(0,0,0)), createGSWorldTransRot(10,pos(0,0,0),pos(0,0,0))}

};
int gesture_length[G_COUNT]={4,4,4,4,2};


typedef struct T_Command
{
  int type;
  float drotz;
  int ticks;
  Position rot, move;
  int gesture_id;
  int gesture_step;
} Command;

Leg createLeg(Position groundPos,Position jointPos,Position st4Pos, float coxaZeroRotation,int servoStartPos,int legNr, bool previousOnGround)
{
  Leg leg;
  leg.servoStartPos=servoStartPos;
  leg.legNr=legNr;
  leg.previousOnGround=previousOnGround;
  leg.groundPos=groundPos;
  leg.st4Pos=st4Pos;
  Position neutralAirPos = {groundPos.x,groundPos.y,-4};
  leg.neutralAirPos=neutralAirPos;
  leg.tipPos=groundPos;
  leg.jointPos=jointPos;
  leg.coxaZeroRotation=coxaZeroRotation;
  leg.prevStep=0;
  leg.mode=M_STAND6;
  leg.groundPositionForMode=-1;  //invalid ground position
  return leg;
}
float interpolatefloat(float v1, float v2, float alpha)
{
  return v1*(1-alpha) + v2*alpha;
}
Position interpolatePosition(Position pos1,Position pos2,float alpha)
{
  Position pos = {pos1.x*(1-alpha)+pos2.x*alpha,
                  pos1.y*(1-alpha)+pos2.y*alpha,
                  pos1.z*(1-alpha)+pos2.z*alpha};
  return pos;
}

#define TRIPODSTEPCNT 4
#define TWOMOVESTEPCNT 6
#define SINGLELEGSTEPCNT 6


int tripodSeqMap[2][TRIPODSTEPCNT]={
		{S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN},
		{S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND} };

int twoMoveSeqMap[3][TWOMOVESTEPCNT]={
		{S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND,S_GROUND,S_GROUND},
		{S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND},
		{S_GROUND,S_GROUND,S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN} };

int singleLegSeqMap[6][SINGLELEGSTEPCNT]={
	{S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND,S_GROUND,S_GROUND},
	{S_GROUND,S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND,S_GROUND},
	{S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN,S_GROUND,S_GROUND},
	{S_GROUND,S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN,S_GROUND},
	{S_GROUND,S_GROUND,S_GROUND,S_GROUND,S_MOVEUP,S_MOVEDOWN},
	{S_MOVEDOWN,S_GROUND,S_GROUND,S_GROUND,S_GROUND,S_MOVEUP} };

// leg order: left: front,mid,back, right:front,mid,back
int legNrToSeqMap[3][6]={
	{1,0,1,0,1,0},
	{0,1,2,2,1,0},
	{0,2,4,3,5,1} };

#define TRIPODGAIT 0
#define TWOMOVEGAIT 1
#define SINGLELEGGAIT 2

int stepCounts[]={TRIPODSTEPCNT,TWOMOVESTEPCNT,SINGLELEGSTEPCNT};
float maxMoveSpeeds[]={3.5f,2.5f,1.5f};


//int ***seqMaps[3]={&tripodSeqMap,&twoMoveSeqMap,&singleLegSeqMap}; Doesn't work.

int currentGait = SINGLELEGGAIT;


int getLegStatus(int currentGait,int legNr,int step)
{
  switch(currentGait)
  {
  case TRIPODGAIT:
    if(step<stepCounts[currentGait])
      return tripodSeqMap[legNrToSeqMap[currentGait][legNr]][step];
    break;
  case TWOMOVEGAIT:
    if(step<stepCounts[currentGait])
      return twoMoveSeqMap[legNrToSeqMap[currentGait][legNr]][step];
    break;
  case SINGLELEGGAIT:
    if(step<stepCounts[currentGait])
      return singleLegSeqMap[legNrToSeqMap[currentGait][legNr]][step];
    break;
  }
  printf("ERROR: getLegStatus(%d,%d,%d)\n",currentGait,legNr,step);
  return 0;
}


Position frontLegPos = {0,0,0};

// See tripod sequence here: http://www.lynxmotion.com/images/assembly/ssc32/h2seqdia.gif
void setSeqPos(Leg *leg,int step,float partial,World *world,float moveX,float moveY,int mode)
{
  int status=S_GROUND;
  // leg->mode plays catch-up with mode given to this function.
  // A leg has to run through part of the sequence before reaching a mode
  int legmode = leg->mode;
  // if the groundposition the leg has now is not for this mode, switch to walking mode until it gets the correct position.
  if((mode==M_STAND6 || mode==M_STAND4) && leg->groundPositionForMode!=mode)
    legmode = M_WALKING;
  switch(legmode)
  {
    case M_WALKING:
//      status=tripodSeqMap[leg->isTripodA][step];
      status=getLegStatus(currentGait,leg->legNr,step);
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
      if(leg->legNr==0) //front leg
      {
        leg->tipPos.x += frontLegPos.x;
        leg->tipPos.y += frontLegPos.y;
        leg->tipPos.z += frontLegPos.z;
      }
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
        Position podGroundPos;
        if(mode==M_STAND4)
          podGroundPos=createPosition(leg->st4Pos.x,leg->st4Pos.y,0);
        else
          podGroundPos=createPosition(leg->neutralAirPos.x,leg->neutralAirPos.y,0);
        if(mode==M_WALKING) //only adjust ground position away from neutralposition if walking
        {
          //New groundPosition=<airpos projected on ground> + <movement>
          podGroundPos.x-=moveX;
          podGroundPos.y-=moveY;
        }
        leg->groundPos=podToWorld(world,podGroundPos);
        (leg->groundPos).z = GROUND_Z;
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
  
  Position femurStart = {leg->jointPos.x+cosf(h)*coxaLength,leg->jointPos.y+sinf(h)*coxaLength,leg->jointPos.z};
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
    Angles ret = {vl,vu,h};
    return ret;
  }

  Angles ret = {0,0,0};
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

void readvoltages(int fd, MqttConnection *mqtt_connection)
{
  serialPuts(fd,(char *)"VB\r");
  int voltage6V=serialGetchar(fd);
  serialPuts(fd,(char *)"VC\r");
  int voltage9V=serialGetchar(fd);
  char bufstring[256];
  sprintf(bufstring,"-V 6V=%g, 9V=%g", 10.0f*voltage6V/255, 10.0f*voltage9V/255);
  printf("%s\n", bufstring);
  mqtt_connection->publish_string(bufstring);
}
    /*
    // Scanning
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
    }*/

void sendServoCommands(int serialHandle, Leg legs[],int step,float partial,World *world, int mode, float moveX, float moveY, float headpan, float headtilt)
{
  char serialBuffer[1000];
  char partialBuffer[31];
  int i;
  serialBuffer[0]=0;
  //printf("MODE: %d\n",mode);
  for(i=0;i<LEGCNT;i++)
  {
     setSeqPos(&legs[i],step,partial,world,moveX,moveY,mode);
     getHexCommands(&legs[i],partialBuffer);
     strncat(serialBuffer,partialBuffer,1000);
  }
  int tilt=1500-headtilt*2000/M_PI;
  int pan=1500-headpan*2000/M_PI;
  if(tilt<1200)tilt=1200;
  if(tilt>1800)tilt=1800;
  if(pan<1200)pan=1200;
  if(pan>1800)pan=1800;
  snprintf(partialBuffer,30,"#0P%d #1P%d ",tilt,pan);
  strncat(serialBuffer,partialBuffer,1000);
  printf("Servocommands to be printed:\n");
  printf("Servocommands: %s\n",serialBuffer);
  strncat(serialBuffer,"T100\r",1000);
  //if(legCrossedBoundary)
    //printf("Crossed boundary!\n");
  serialPuts(serialHandle,serialBuffer);
}

void sendPoweredDown(int serialHandle)
{
  serialPuts(serialHandle,POWEREDDOWNSTRING);
}

void sendZeroString(int serialHandle)
{
  serialPuts(serialHandle,ZEROSTRING);
}

void updateStepAndPartial(int *step, float *partial)
{
  // Update step and partial
  *partial+=0.5;
  if(*partial>=1)
  {
    *partial = *partial - 1;
    *step = *step + 1;
    if(*step>=stepCounts[currentGait])
      *step=0;
  }
  printf("Step: %d, partial: %g\n",*step,*partial);
}

int received = 0;
char received_msg[1000] = {0};

void on_message_func(const struct mosquitto_message *message)
{
    printf("on_message_func: %s\n", (char *)(message->payload));
    if(!received)
    {
        strcpy(received_msg,(char *)(message->payload));
        received = 1;
    }
}


int main(int argc,char *argv[])
{
  float headpan = 0, headtilt = -0.25;
  struct timespec lastScanTime, curTime, diffTime;
  World world = {{0,0,0},{0,0,0}};
  Position worldbasetrans = {0,0,0};
  Leg legs[LEGCNT];
  class MqttConnection *mqtt_connection;
  char received_copy[1000] = {0};
  GestureStep lastgesturestep = {1, 0, 0,0, 0,0,0, 0,0,0, 0,0,0};

  printf("Mqtt init\n");
  mosqpp::lib_init();
  printf("Connect to synology\n");
  mqtt_connection = new MqttConnection("mqtt_sender", "localhost", 1883, "motioncontrol", on_message_func);
  printf("Starting loop\n");
  mqtt_connection->loop_start();
  printf("Mqtt setup complete\n");
  
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

  //Leg createLeg(Position groundPos,Position jointPos,Position st4Pos, float coxaZeroRotation,int servoStartPos,int legNr, bool previousOnGround)
  //left legs (-x)
  legs[0]=createLeg(createPosition(-GndFBX, -GndFBY,GndZ),createPosition(-JntFBX,-JntFBY,JntZ),createPosition(-St4FX, St4FY,GndZ),(-180+22)*M_PI/180,2,0, true);  //front
  legs[1]=createLeg(createPosition(-GndMX,        0,GndZ),createPosition(-JntMX,       0,JntZ),createPosition(-St4MX, St4MY,GndZ),(-180   )*M_PI/180,5,1,true);  //mid
  legs[2]=createLeg(createPosition(-GndFBX,  GndFBY,GndZ),createPosition(-JntFBX, JntFBY,JntZ),createPosition(-St4BX, St4BY,GndZ),(-180-22)*M_PI/180,8,2, true);  //back

  //right legs (+x)
  legs[3]=createLeg(createPosition( GndFBX, -GndFBY,GndZ),createPosition( JntFBX,-JntFBY,JntZ),createPosition( St4FX, St4FY,GndZ),     -22 *M_PI/180,16,3,true);   //back
  legs[4]=createLeg(createPosition( GndMX,        0,GndZ),createPosition( JntMX,       0,JntZ),createPosition( St4MX, St4MY,GndZ),       0 *M_PI/180,19,4, true);   //mid
  legs[5]=createLeg(createPosition( GndFBX,  GndFBY,GndZ),createPosition( JntFBX, JntFBY,JntZ),createPosition( St4BX, St4BY,GndZ),      22 *M_PI/180,22,5,true);  //front

  printf("Starting\n");
  int fd=serialOpen((char *)"/dev/ttyUSB0",115200);
  int counter = 0;
  printf("serialOpen returns: %d\n",fd);
  usleep(100000);
  sendZeroString(fd);
  if(fd==-1)exit(1);
  int step=0;
  float partial=0;
  float moveX=0,moveY=0;
  float drotz=0;
  float dist;
  int modeCounter=0;
  Command command;
  bool commandActive=0;
  int commandTicks=0;
  int mode = M_STAND4;
  int servosPowered = 0;
  float maxSpeed = 0;

  clock_gettime(CLOCK_MONOTONIC,&lastScanTime);
  while(true)
  {
    World worldmodifier = {{0,0,0},{0,0,0}};
    if(received)
    {
      strcpy(received_copy, received_msg);
      received = 0;

      printf("Received1:'%s'\n",received_msg);
      printf("Received2:'%s'\n",received_copy);
      if(received_copy[0]=='G')
      {
        //gesture
        int gesture_id = 0;
        sscanf(received_copy,"G %d", &gesture_id);
        if(gesture_id>=0 && gesture_id<G_COUNT){
          printf("Doing gesture %d", gesture_id);
          command.type = Com_Gesture;
          commandActive = 1;
          commandTicks = 0;
          command.gesture_id = gesture_id;
          command.gesture_step = 0;
        }else{
          printf("Invalid gesture: %d\n", gesture_id);
          commandActive = 0;
        }
      }
      if(received_copy[0]=='P')
      {
        // Toggle servo power
        int powerUpOrDown = 0;
        sscanf(received_copy,"P %d",&powerUpOrDown);
        servosPowered = !!powerUpOrDown;
        if(servosPowered){
          command.type = Com_Gesture;
          commandActive = 1;
          commandTicks = 0;
          command.gesture_id = G_STANDUP;
          command.gesture_step = 0;
        }
      }
      if(received_copy[0]=='R')
      {
        // Rotate
        servosPowered = !0;
        sscanf(received_copy,"R %f %f %f",&command.rot.x,&command.rot.y,&command.rot.z);
        printf("New x,y,z rotation: %f,%f,%f\n",command.rot.x,command.rot.y,command.rot.z);
        command.type = Com_Rotate;
        commandActive = 1;
        commandTicks = 0;
      }
      if(received_copy[0]=='W')
      {
        // Walk
        sscanf(received_copy,"W %f %f %f %d",&command.move.x,&command.move.y,&command.drotz,&command.ticks);
        printf("Move command received: move(%f,%f) rot:%f for %d ticks\n",command.move.x,command.move.y,command.drotz,command.ticks);
        commandActive=1;
        commandTicks=0;
        command.type = Com_Move;
      }
      if(received_copy[0]=='S')
      {
        // Stand 4 / 6
        int standtype = 0;
        sscanf(received_copy,"S %d %f %f %f %f %f %f", &standtype, &command.move.x, &command.move.y, &command.move.z, &command.rot.x, &command.rot.y, &command.rot.z);
        commandActive = 1;
        commandTicks = 0;
        if(standtype == 4)
	  command.type = Com_Stand4;
        else
          command.type = Com_Stand6;
      }
      if(received_copy[0]=='T')
      {
        // Change gait
        int gaitType = 0;
        sscanf(received_copy,"T %d",&gaitType);
        if(gaitType == 0)
          currentGait = TRIPODGAIT;
        else if(gaitType == 1)
          currentGait = TWOMOVEGAIT;
        else
          currentGait = SINGLELEGGAIT;
      }
    }

    clock_gettime(CLOCK_MONOTONIC,&curTime);
    diffTime=timespecDiff(lastScanTime,curTime);

    dist=getDistance(fd);
    maxSpeed = maxMoveSpeeds[currentGait];


    // Command override
    if(commandActive)
    {
      GestureStep gs;
      float alpha = 0;
      frontLegPos.x = 0;
      frontLegPos.y = 0;
      frontLegPos.z = 0;
      switch(command.type)
      {
      case Com_Gesture:
        gs = gestures[command.gesture_id][command.gesture_step];
        printf("Executing gesture %d, step %d, legmode: %d for %d ticks\n", command.gesture_id, command.gesture_step, gs.legmode, gs.ticks);
        if(gs.ticks>0)
          alpha = commandTicks*1.0f/gs.ticks;
        else
          alpha = 1; //zero ticks, means directly into the next stance
        mode = gs.legmode;
        headpan = gs.headpan;
	headtilt = gs.headtilt;
        worldmodifier.trans = interpolatePosition(lastgesturestep.worldtrans, gs.worldtrans, alpha);
        worldmodifier.rot = interpolatePosition(lastgesturestep.worldrot, gs.worldrot, alpha);
        frontLegPos = gs.frontlegpos;
        commandTicks++;
        if(commandTicks>gs.ticks){
          lastgesturestep = gs;
          commandTicks = 0;
          command.gesture_step ++;
          if(command.gesture_step >= gesture_length[command.gesture_id]){
            commandActive = 0;
          }
        }
        break;
      case Com_Move:
        mode=M_WALKING; //override mode when executing command
        moveX=command.move.x*maxSpeed;
        moveY=command.move.y*maxSpeed;
        drotz=command.drotz;
        printf("Command active, move, rot:(%g,%g),%g tick: %d/%d\n",
               moveX,moveY,drotz,commandTicks,command.ticks);
        commandTicks++;
        if(commandTicks>command.ticks)
        {
          commandActive=0;
//          simplesocket_send(sock,"CE");  //command executed
        }
        break;
      case Com_Rotate:
        mode = M_STAND6; //will be overwritten
        printf("Command active, rotating..");
        headpan = command.rot.z;
        headtilt = command.rot.x;
        worldmodifier.rot = command.rot;
        break;
      case Com_Stand4:
        mode = M_STAND4;
        printf("Command active, stand4-ing..");
        frontLegPos = command.move;
        break;
      case Com_Stand6:
        mode = M_STAND6;
        printf("Command active, stand6-ing..");
        worldmodifier.rot = command.rot;
        worldmodifier.trans = command.move;
        break;
      }
    }
    if(!commandActive) // Only the rotate command can execute during standard behavior
    {
      modeCounter++;
      //mode=modeCounter%32<16?M_WALKING:modeCounter%64<32?M_STAND4:M_STAND6;
      mode = M_STAND6;
      // Standard behavior
      if(dist>40)
      {
        moveX=0.0f;
        moveY=maxSpeed;
        drotz=0.0f;
        //printf("FULL SPEED\n");
      }else
      if(dist>20)
      {
        moveX=0.0f;
        moveY=0.5f*maxSpeed;
        drotz=0.0f;
        //printf("HALF SPEED\n");
      }else
      {
        moveX=0.0f;
        moveY=0.0f;;
        drotz=0.02f;
        //printf("ROTATE\n");
      }
    }
    printf ("moveX: %g, moveY: %g, drotz: %g\n",moveX,moveY,drotz);
    if(mode==M_STAND4 || mode==M_STAND6) //stop all movement if standing
    {
      moveX=moveY=0;
      drotz=0;
    }

    // Adjust world
    float cosrz=cosf(-world.rot.z),sinrz=sinf(-world.rot.z);
    float moveGroundX=moveX*cosrz+moveY*sinrz;
    float moveGroundY=moveY*cosrz-moveX*sinrz;
    //move speed max = move(X,Y) / ( 8 / partialAdd) ?
    worldbasetrans.x+=moveGroundX/6.0;
    worldbasetrans.y+=moveGroundY/6.0;
    // worldbaserot += drotz?
    world.trans = worldbasetrans;
    world.trans.x += worldmodifier.trans.x;
    world.trans.y += worldmodifier.trans.y;
    world.trans.z += worldmodifier.trans.z;
    world.rot = worldmodifier.rot;
    printf("World: %g,%g,%g, %g,%g,%g\n", world.trans.x,world.trans.y,world.trans.z, world.rot.x,world.rot.y,world.rot.z);
    char legModes[7];
    for(int i=0;i<6;i++){
      legModes[i]=(legs[i].mode==M_WALKING?'W':(legs[i].mode==M_STAND6?'6':'4'));
    }
    legModes[6]=0;
    printf("Leg modes2: %s\n", legModes);
    if(servosPowered){
      printf("Send servo commands:\n");
      sendServoCommands(fd,legs,step,partial,&world,mode, moveX,moveY, headpan, headtilt);
    } else {
      printf("Servos not powered\n");
      sendPoweredDown(fd);
    }

    updateStepAndPartial(&step, &partial);

    usleep(SLEEP_MICROS);
    counter ++;
    if(counter>(10000000/SLEEP_MICROS)){
      counter = 0;
      readvoltages(fd, mqtt_connection);
    }
  }
  serialClose(fd);
  mqtt_connection->loop_stop();
  mosqpp::lib_cleanup();

}
