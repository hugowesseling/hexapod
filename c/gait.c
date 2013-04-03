#include <wiringSerial.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

bool legCrossedBoundary=false;

const char *ZEROSTRING="#21PO-50 #7PO-20 #6PO-20 #8PO30 #16PO-20 #18PO30\r";

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
  Position groundPos,neutralAirPos;
  Position tipPos;
  Position jointPos;
  float coxaZeroRotation;
  Position femurStart;
  int prevStep;
} Leg;

typedef struct T_Angles
{
  float vL;
  float vu;
  float h;
} Angles;

Leg createLeg(Position groundPos,Position jointPos,float coxaZeroRotation,int servoStartPos,bool isTripodA,bool previousOnGround)
{
  Leg leg;
  leg.servoStartPos=servoStartPos;
  leg.isTripodA=isTripodA;
  leg.previousOnGround=previousOnGround;
  leg.groundPos=groundPos;
  Position neutralAirPos{groundPos.x,groundPos.y,-4};
  leg.neutralAirPos=neutralAirPos;
  leg.tipPos=groundPos;
  leg.jointPos=jointPos;
  leg.coxaZeroRotation=coxaZeroRotation;
  leg.prevStep=0;
  return leg;
}
Position interpolatePosition(Position pos1,Position pos2,float alpha)
{
  Position pos{pos1.x*(1-alpha)+pos2.x*alpha,
               pos1.y*(1-alpha)+pos2.y*alpha,
               pos1.z*(1-alpha)+pos2.z*alpha};
  return pos;
}

//Determines if the leg is on the ground in this step
bool onGround(int step,bool isTripodA)
{
  if(isTripodA)
    return (step==0) || (step==1);
  return (step==2) || (step==3);
}

// See tripod sequence here: http://www.lynxmotion.com/images/assembly/ssc32/h2seqdia.gif
void setSeqPos(Leg *leg,int step,float partial,World *world,float moveX,float moveY,float groundZ)
{
  if(onGround(step,leg->isTripodA))
  {
    leg->tipPos= worldToPod(world,leg->groundPos);
  }else
  {
    if((leg->isTripodA && step==2) || (!leg->isTripodA && step==0))
    {
      leg->tipPos=interpolatePosition(worldToPod(world,leg->groundPos),leg->neutralAirPos,partial);
    }
    if((leg->isTripodA && step==3) || (!leg->isTripodA && step==1))
    {
      if((leg->isTripodA && leg->prevStep!=3) || (!leg->isTripodA && leg->prevStep!=1))
      {
        //calculate new groundPosition
        //for now use old position with some translation
        Position worldGroundPos{leg->neutralAirPos.x,leg->neutralAirPos.y,groundZ};
        worldGroundPos.x-=moveX;
        worldGroundPos.y-=moveY;
        leg->groundPos=podToWorld(world,worldGroundPos);
        //leg->groundPos.x-=moveX;
        //leg->groundPos.y-=moveY;
      }
      leg->tipPos=interpolatePosition(leg->neutralAirPos,worldToPod(world,leg->groundPos),partial);
    }
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

/*
  void screenline(float x1,float y1,float x2,float y2)
  {
    float zoom=SCREENZOOM;
    line(320+x1*zoom, 240+y1*zoom, 320+x2*zoom, 240+y2*zoom);
  }
  void screenCross(float x,float y)
  {
    float zoom=SCREENZOOM;
    line(320+(x-1)*zoom,240+(y-1)*zoom, 320+(x+1)*zoom, 240+(y+1)*zoom);
    line(320+(x+1)*zoom,240+(y-1)*zoom, 320+(x-1)*zoom, 240+(y+1)*zoom);
  }
  void screenPosLine(float x1,float y1,float x2,float y2)
  {
    float zoom=SCREENZOOM;
    line(screenPos.x+(x1-jointPos.x)*zoom,screenPos.y-(y1-jointPos.z)*zoom, 
         screenPos.x+(x2-jointPos.x)*zoom,screenPos.y-(y2-jointPos.z)*zoom);
  }
  void screenPosCross(float x,float y)
  {
    float zoom=SCREENZOOM;
    x-=jointPos.x;
    y-=jointPos.z;
    line(screenPos.x+(x-1)*zoom,screenPos.y+(-y-1)*zoom, screenPos.x+(x+1)*zoom, screenPos.y+(-y+1)*zoom);
    line(screenPos.x+(x+1)*zoom,screenPos.y+(-y-1)*zoom, screenPos.x+(x-1)*zoom, screenPos.y+(-y+1)*zoom);
  }
  void draw()
  {
    Angles angles=calculateAngles();
    System.out.println("angles.h:"+angles.h+", vu:"+angles.vu+", vl:"+angles.vL);
    stroke(255);
    screenline(jointPos.x,                                        jointPos.y,
               jointPos.x+cos(angles.h)*(coxaLength+femurLength), jointPos.y+sin(angles.h)*(coxaLength+femurLength));
    stroke(255,0,0);
    screenCross(tipPos.x,tipPos.y);
    stroke(0,255,0);
    screenCross(jointPos.x,jointPos.y); 
    
    stroke(0,255,255); 
    screenPosCross(jointPos.x,jointPos.z);
    screenPosLine(jointPos.x,jointPos.z,femurStart.x,femurStart.z);
    screenPosCross(femurStart.x,femurStart.z);
    float cosh=cos(angles.h);    
    Pos2d midPos=new Pos2d(femurStart.x+cos(angles.vu)*cosh*femurLength,femurStart.z+sin(angles.vu)*femurLength);
    screenPosLine(femurStart.x,femurStart.z,midPos.x,midPos.y);
    Pos2d endPos=new Pos2d(midPos.x+cos(angles.vu+angles.vL)*cosh*tibiaLength,midPos.y+sin(angles.vu+angles.vL)*tibiaLength);
    screenPosLine(midPos.x,midPos.y,endPos.x,endPos.y);
    stroke(255,255,0);
    screenPosCross(tipPos.x,tipPos.z);
  }
}

Vector legs;
World world;

void setup()
{
  size(640,480);
  world=new World();
  
  legs=new Vector();
  //Leg(Position groundPos,Position jointPos,float coxaZeroRotation,int servoStartPos,boolean isTripodA,boolean previousOnGround)
  //left legs (-x)
  legs.add(new Leg(new Position(-14, -12,-4),new Position(-4,  -8,0),(-180+22)*PI/180,2,true, true,new Pos2d(150,80)));   //front
  legs.add(new Leg(new Position(-15.85,0,-4),new Position(-5.85,0,0),(-180   )*PI/180,5,false,true,new Pos2d(150,240)));  //mid
  legs.add(new Leg(new Position(-14,  10,-4),new Position(-4,   8,0),(-180-22)*PI/180,8,true, true,new Pos2d(150,400)));  //back

  //right legs (+x)
  legs.add(new Leg(new Position( 14, -12,-4),new Position( 4,  -8,0),     -22 *PI/180,16,false,true,new Pos2d(490,80)));   //back
  legs.add(new Leg(new Position( 15.85,0,-4),new Position( 5.85,0,0),       0 *PI/180,19,true, true,new Pos2d(490,240)));   //mid
  legs.add(new Leg(new Position( 14,  10,-4),new Position( 4,   8,0),      22 *PI/180,22,false,true,new Pos2d(490,400)));  //front   
  
  println(Serial.list());
  myPort = new Serial(this, "COM7", 115200);
  frameRate(15);
  myPort.write(ZEROSTRING);
}

int prevMouseX=0,prevMouseY=0;

float partial=0;
int step=0;

void draw()
{
  background(0);
  String hexCommands="";
  legCrossedBoundary=false;
  for(int i=0;i<legs.size();i++)
  {
    Leg leg=(Leg)legs.get(i);
    leg.setSeqPos(step,partial,world);
    leg.draw();
    hexCommands+=leg.getHexCommands();
  }
  hexCommands+=" S500";
  System.out.println("Hex commands:"+hexCommands);
  if(!legCrossedBoundary)
  {
    myPort.write(hexCommands+"\r");
  }else
  {
    System.out.println("Legs crossed boundary. Not writing!");
  }
  partial+=0.5;
  if(partial>=1)
  {
    partial-=1;
    step++;
    if(step>7)step=0;
  }
  
  System.out.println("Step: "+step+", partial:"+partial);
  
  if(prevMouseX==-1)
  {
    prevMouseX=mouseX;
    prevMouseY=mouseY;
  }
  if(mousePressed)
  {
    if(mouseButton == RIGHT)
    {
      world.trans.z+=(prevMouseY-mouseY)*0.1;
    }else
    if(mouseButton == CENTER)
    {
      world.rot.z+=(prevMouseX-mouseX)*0.01;
      world.rot.x+=(prevMouseY-mouseY)*0.01;
    }else
    {
      world.trans.y+=(mouseY-prevMouseY)*0.1;
      world.trans.x+=(mouseX-prevMouseX)*0.1;
    }
  }
  world.trans.y+=0.5;
  prevMouseX=mouseX;
  prevMouseY=mouseY;
}

*/
#define LEGCNT 6

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
  struct timespec lastScanTime,curTime,diffTime;
  World world{{0,0,0},{0,0,0}};
  Leg legs[LEGCNT];
  //left legs (-x)
  legs[0]=createLeg(createPosition(-13, -13,-4),createPosition(-4,  -8,0),(-180+22)*M_PI/180,2,true, true);  //front
  legs[1]=createLeg(createPosition(-14.85,0,-4),createPosition(-5.85,0,0),(-180   )*M_PI/180,5,false,true);  //mid
  legs[2]=createLeg(createPosition(-13,  13,-4),createPosition(-4,   8,0),(-180-22)*M_PI/180,8,true, true);  //back

  //right legs (+x)
  legs[3]=createLeg(createPosition( 13, -13,-4),createPosition( 4,  -8,0),     -22 *M_PI/180,16,false,true);   //back
  legs[4]=createLeg(createPosition( 14.85,0,-4),createPosition( 5.85,0,0),       0 *M_PI/180,19,true, true);   //mid
  legs[5]=createLeg(createPosition( 13,  13,-4),createPosition( 4,   8,0),      22 *M_PI/180,22,false,true);  //front   

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
  bool stopped=false;
  bool scanDirectionRight=true;

  clock_gettime(CLOCK_MONOTONIC,&lastScanTime);
  while(true)
  {
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
    float cosrz=cosf(-world.rot.z),sinrz=sinf(-world.rot.z);
    float moveGroundX=moveX*cosrz+moveY*sinrz;
    float moveGroundY=moveY*cosrz-moveX*sinrz;
    serialBuffer[0]=0;
    for(i=0;i<LEGCNT;i++)
    {
       setSeqPos(&legs[i],step,partial,&world,moveGroundX,moveGroundY,-8);
       getHexCommands(&legs[i],partialBuffer);
       strncat(serialBuffer,partialBuffer,1000);
    }
    printf("serialBuffer: %s\n",serialBuffer);
    strncat(serialBuffer,"T100\r",1000);
    if(legCrossedBoundary)
      printf("Crossed boundary!\n");

    serialPuts(fd,serialBuffer);

    if(!stopped || step!=3) //Stop at position 4
    {
      partial+=0.5;
      if(partial>=1)
      {
        partial-=1;
        step++;
        if(step>3)step=0;
      }
    }
    printf("Step: %d, partial: %g\n",step,partial);
    //move speed max = move(X,Y) / ( 8 / partialAdd) ?

    world.trans.x+=moveGroundX/6.0;
    world.trans.y+=moveGroundY/6.0;
    world.rot.z+=drotz;
    usleep(100000);
  }
  serialClose(fd);
}
