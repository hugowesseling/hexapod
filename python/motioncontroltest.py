import simplesocket
import time
import math

sock=simplesocket.simplesocket(12345)

counter=0
while True:
    counter+=1
    print "Sending"
    #rotx=math.sin(counter*0.08)*0.1
    #roty=math.sin(counter*0.13)*0.1
    #rotz=math.sin(counter*0.20)*0.1
    #sock.send("R %f %f %f"%(rotx,roty,rotz))
    #sock.send(".")
    sock.send("W 0.0 -1.0 -0.00 20")
    for i in range(20):
      print "Testing to receive"
      received,buf=sock.receive()
      if received:
        print "Received:",buf
        break
      time.sleep(0.1)

