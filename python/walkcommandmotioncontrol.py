import simplesocket
import time
import math

sock=simplesocket.simplesocket(12345)

sendNextCommand=True
walkRot=0.0
while True:
  if sendNextCommand:
    walkRot+=math.pi/2
    print "Sending"
    moveX=2.0*math.sin(walkRot)
    moveY=2.0*math.cos(walkRot)
    drotz=0.0
    ticks=100
    sock.send("W %f %f %f %d"%(moveX,moveY,drotz,ticks))
    sendNextCommand=False

  print "Testing to receive"
  received,buf=sock.receive()
  if received:
    print "Received:",buf
    if buf=="CE":
      print "Received Command Executed"
      getNextCommand=True
  time.sleep(0.5)

