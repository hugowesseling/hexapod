'''
  Now refactor to enable call like:
    distance,xpos,rotation = findMarkers(cap)
  Finds AR markers
  todo: 
    extract edges directly from Canny function
    try out canny with higher apertureSize
'''

import sys
sys.path.append(r"C:\opencv\build\python\2.7")
import cv2
import cv
import time
import cairo
import numpy
import math

imageWidth=640 #1280
imageHeight=360 #720

#SQUAREMARGIN= 1+ % of size difference allowed to still be considered square 
# 1.01 works well for straight viewing, 1.4 still recognized AR
SQUAREMARGIN=1.4 

names={7132:"Victini",6621:"Snivy",3037:"Servine",6486:"Serperior",2902:"Tepig",2391:"Pignite",6999:"Emboar",2398:"Oshawott",6495:"Samurott",
        471:"0123456789",4575:"bla2",3105:"bla3",982:"bla4",5086:"bla5",3616:"bla6"}
#1110
#1001
#1101
#0000
marker3dPoints={471:[[1,0,0],[0,0,0],[0,1,0],[1,1,0]]}

def approxPoly2Pts(approxPoly):
  pts=[]
  for p in approxPoly:
    pts.append([p[0][0],p[0][1]])
  return numpy.array(pts,dtype=numpy.int32)

# formula from http://stackoverflow.com/questions/451426/how-do-i-calculate-the-surface-area-of-a-2d-polygon
def segments(p):
  res=[]
  lenp=len(p)
  for i in range(lenp):
    res.append((p[i],p[(i+1)%lenp]))
  return res

def calculateArea(p):
  return 0.5 * abs(sum(x0*y1 - x1*y0 for ((x0, y0), (x1, y1)) in segments(p)))

def averageSideLength(p):
  #print "p:",p,"segments:",segments(p)
  return sum(math.hypot(x1-x0,y1-y0) for ((x0, y0), (x1, y1)) in segments(p)) / len(p)

def maxSideLength(p):
  return max(math.hypot(x1-x0,y1-y0) for ((x0, y0), (x1, y1)) in segments(p))
  

def getMidMarker(pts):
  lenpts = len(pts)
  if lenpts == 0:
    return (0,0)
  xmid,ymid = (0,0)
  for x,y in pts:
    xmid += x
    ymid += y
  return (xmid / lenpts, ymid / lenpts)

def isSquare(pts):
  global SQUAREMARGIN
  if len(pts)!=4: return False
  #Not: Test if inproduct  cos(phi)=(v.u)/(|v||u|) is close to zero
  #Test if area is close to max
  area=calculateArea(pts)
  #average length of sides
  sideLength=averageSideLength(pts)
  sideLengthArea=sideLength*sideLength
  #if both areas are less than 1% different, return True
  if sideLengthArea*SQUAREMARGIN>area and area*SQUAREMARGIN>sideLengthArea:
    result=True
  else:
    result=False
  #print "area:",area,"sideLength:",sideLength,"sideLength^2",sideLength*sideLength,"result: ",result
  return result
  
def interpolatePoint(p1,p2,alpha):
  return (p1[0]*(1-alpha)+p2[0]*alpha, p1[1]*(1-alpha)+p2[1]*alpha)

foundMarkers = []

def scanPoints(img,pts,insideSize):
  global names, foundMarkers
  #print "pts:",pts
  #pt0 - pt1
  # |     |
  #pt3 - pt2
  scannedColors=[]
  for y in range(insideSize):
    #calculate normalized y
    ynormal=(y+1.5)/(insideSize+2)
    #calculate endpoints of horizontal vector
    hor1=interpolatePoint(pts[0],pts[3],ynormal)
    hor2=interpolatePoint(pts[1],pts[2],ynormal)
    for x in range(insideSize):
      xnormal=(x+1.5)/(insideSize+2)
      scanPoint=interpolatePoint(hor1,hor2,xnormal)
      scanPoint=(int(scanPoint[0]),int(scanPoint[1]))
      rgb=img[scanPoint[1]][scanPoint[0]]
      scannedColors.append(int(rgb[0])+rgb[1]+rgb[2])
      #cv2.circle(img, scanPoint, 1, (255,255,0))
  divider=(max(scannedColors)+min(scannedColors))/2
  #print "scannedColors:",scannedColors,"max:",max(scannedColors),"min:",min(scannedColors),"divider:",divider
  bits=[]
  for scannedColor in scannedColors:
    if scannedColor>divider:
      bits.append(1)  #white
    else:
      bits.append(0)  #black
  #print "bits:",bits
  #Following part only valid for insideSize==4!
  # middle: 5,6,9,10 (5=10,9='1',6='0')
  #rotate until middle condition is correct
  correct=False
  for i in range(4):
    #  0  1  2  3
    #  4  5  6  7
    #  8  9 10 11
    # 12 13 14 15
    if bits[5]==bits[10] and bits[9]==1 and bits[6]==0:
      correct=True
      rotatedTimes = i
      break
    #rotate bits CW
    newbits=[]
    for y in range(4):
      for x in range(4):
        # nx = y, ny = 3-x
        newbits.append(bits[4*(3-x)+y])
    bits=newbits
    #rotate points CW
    swappts = pts[0]
    pts[0] = pts[1]
    pts[1] = pts[2]
    pts[2] = pts[3]
    pts[3] = swappts
    
  if correct:
    value=bits[ 0]      + bits[ 1]*   2 + bits[ 2]*   4 + bits[ 3]*   8 +\
          bits[ 4]*  16 + bits[ 5]*  32 +               + bits[ 7]*  64 +\
          bits[ 8]* 128 +                               + bits[11]* 256 +\
          bits[12]* 512 + bits[13]*1024 + bits[14]*2048 + bits[15]*4096
    #print "Correct: bits:",bits
    #print "Value: ",value
    middle=interpolatePoint(interpolatePoint(pts[0],pts[1],0.5),interpolatePoint(pts[2],pts[3],0.5),0.5)
    if value in names:
      name = names[value]
      foundMarkers.append((value,middle,maxSideLength(pts)))
      if value in marker3dPoints:
        points3d = marker3dPoints[value]
        calculateCamTest(pts,points3d,name,bits,rotatedTimes)
    else:
      name="%d"%value
    middle=(int(middle[0])-20,int(middle[1]))
    cv2.putText(img, name, middle, cv2.FONT_HERSHEY_PLAIN, 0.6, (255,255,255),2)
    cv2.putText(img, name, middle, cv2.FONT_HERSHEY_PLAIN, 0.6, (0,0,0),1)
    
def calculateCamTest(points2d,points3d,name,bits,rotatedTimes):
  print "%s: points2d: %r, points3d: %r, rot: %d, bits: %r"%(name,points2d,points3d,rotatedTimes,bits)

def drawMarkerInfo(cr,size1,size2,x,dist):
  cr.set_source_rgb(1.0,1.0,1.0)
  cr.paint()
  cr.set_source_rgb(1,0,0)
  cr.move_to(100 - dist /6 ,100)
  cr.line_to(100 + dist /6, 100)
  cr.move_to(100 - dist /6, 100 - size1)
  cr.line_to(100 - dist /6, 100)
  cr.move_to(100 + dist /6, 100 - size2)
  cr.line_to(100 + dist /6, 100)
  cr.move_to(100, 95)
  cr.line_to(100,105)
  cr.stroke()
  #1m dist: size1,2 = 72, dist = 240
  z1 = 100 / (size1/72.0)  #in cm
  z2 = 100 / (size2/72.0)
  angle = math.atan2(z2-z1,40) #armarker dist = 40cm
  zmid = 100 / (dist/math.cos(angle)/240.0)
  xmid = x * zmid * 2 #Why *2 ?
  print "xmid:",xmid
  dx = math.cos(angle)*30
  dy = math.sin(angle)*30
  cr.set_source_rgb(0,1,0)
  cr.move_to(100+xmid-dx,200 - (zmid-dy))
  cr.line_to(100+xmid+dx,200 - (zmid+dy))
  cr.stroke()

  #viewport lines
  cr.set_source_rgb(0,0,1)
  cr.move_to(100,200)
  cr.line_to(0,100)
  cr.move_to(100,200)
  cr.line_to(200,100)
  cr.stroke()


cap = cv2.VideoCapture(0)
if cap.isOpened():
  print "Camera is opened!"
else:
  print "Failed to open camera"
  exit(0)

#set the width and height
cap.set(3,imageWidth)
cap.set(4,imageHeight)
analysisimg = numpy.zeros((200,200,4),numpy.uint8)
surface = cairo.ImageSurface.create_for_data(analysisimg,cairo.FORMAT_ARGB32,200,200)
cr = cairo.Context(surface)

while True:
    ret, img = cap.read()
    print "cap.read():"+str(ret)
    imgGray=cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
    canny=cv2.Canny(imgGray,200,255) #worked okay until slightly upward tilted markers: 155,100 #200,555
    #cannydisplay=cv2.Canny(imgGray,555,200) #200,555
    contours, hierarchy = cv2.findContours(canny,cv2.RETR_EXTERNAL,cv2.CHAIN_APPROX_SIMPLE)
    foundMarkers=[]
    for contour in contours:
      approx=cv2.approxPolyDP(contour, cv2.arcLength(contour,True)*0.1, True) 
      if len(approx)==4:
        arcLength=cv2.arcLength(approx,True)
        #print "arclength: %d"%arcLength
        if arcLength>40:
          approxPts=approxPoly2Pts(approx)
          if isSquare(approxPts):
            #print "approxPts:",approxPts
            #cv2.drawContours(img,approx,-1,(0,0,255),3)
            cv2.polylines(img, [approxPts], True,(0,0,255),1)
            scanPoints(img,approxPts,4)
          else:
            cv2.drawContours(img,approx,-1,(0,0,255),3)
        else:
          cv2.drawContours(img,approx,-1,(0,0,128),3)
      else:
        cv2.drawContours(img,approx,-1,(128,128,128),3)
    
    if len(foundMarkers)==2:
      #extracting x,size1,size2,dist
      ms=foundMarkers
      x1,y1 = ms[0][1]
      x2,y2 = ms[1][1]
      x = (x1 + x2) / 2 / imageWidth - 0.5  # x = -0.5 .. 0.5
      dist = abs(x1 - x2)
      size1 = ms[0][2]
      size2 = ms[1][2]
      if x1>x2:
         size1,size2 = size2,size1
      print "Found 2 markers:",ms
      print "size1:%f, size2:%f, x:%f, dist:%f"%(size1,size2,x,dist)
      drawMarkerInfo(cr,size1,size2,x,dist)
    cv2.imshow("img", img)
    #cv2.imshow("analysis", analysisimg)
    #cv2.imshow("canny", canny)
    #time.sleep(0.1)
    cv2.waitKey(1)

#cv2.destroyAllWindows() 
#cv2.VideoCapture(0).release()
