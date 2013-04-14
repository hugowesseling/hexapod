import simplesocket
import time
import math

sock=simplesocket.simplesocket(12345)

counter=0
while True:
    counter+=1
    print "Sending"
    rotx=math.sin(counter*0.3)*0.1
    roty=math.sin(counter*0.5)*0.1
    rotz=math.sin(counter*0.7)*0.1
    sock.send("R %f %f %f"%(rotx,roty,rotz))
    while True:
      print "Testing to receive"
      received,buf=sock.receive()
      if received:
        print "Received:",buf
        break
      time.sleep(0.01)

