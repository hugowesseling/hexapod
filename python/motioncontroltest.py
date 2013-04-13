import simplesocket
import time

sock=simplesocket.simplesocket(12345)

counter=0
while True:
    counter+=1
    print "Sending"
    rotx=0
    roty=0
    rotz=sin(counter*0.1)*0.1
    sock.send("R %f %f %f"%(rotx,roty,rotz))
    time.sleep(1.0)
    print "Testing to receive"
    received,buf=sock.receive()
    if received:
        print "Received:",buf

