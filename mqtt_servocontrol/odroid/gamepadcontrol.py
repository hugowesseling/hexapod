import paho.mqtt.client as mqtt
import time
import math

received = False
received_buf = ""

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("gamepad")

def on_message(client, userdata, msg):
    global received, received_buf
    print("on_message: %r" % msg.payload)
    received_buf = msg.payload
    received = True

def main():
    global received, received_buf
    #Setup
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect("192.168.1.84", 1883, 60)
    client.loop_start()
    lastbutton1 = False
    lastbutton2 = False
    currentGait = 0
    servosPowered = False

    print("Mainloop started")
    #Mainloop
    while True:
        time.sleep(0.1)
        if received:
            buf = received_buf[:]
            received = False
            print "Received:%r"%buf
            print "Axisvalues: %3d,%3d,%3d,%3d,%3d"%(ord(buf[0]),ord(buf[1]),ord(buf[2]),ord(buf[3]),ord(buf[4]))
            print "Buttonvalues: %2d,%2d,%2d,%2d, %2d,%2d,%2d,%2d, %2d,%2d"%(ord(buf[5]),ord(buf[6]),ord(buf[7]),ord(buf[8]), ord(buf[9]),ord(buf[10]),ord(buf[11]),ord(buf[12]), ord(buf[13]),ord(buf[14]))

            stringToSend = ""
            button0 = (ord(buf[5]) != 0)
            button1 = (ord(buf[6]) != 0)
            button2 = (ord(buf[7]) != 0)
            if button2:
                if not lastbutton2:
                    lastbutton2 = True
                    servosPowered = not servosPowered
                    stringToSend = "P %d"%(1 if servosPowered else 0)
            else:
                lastbutton2 = False
            if stringToSend == "":
                if button0:
                    legMoveX = ord(buf[0])/32.0-2.0 #-2 .. 6
                    legMoveY = ord(buf[2])/32.0-6.0 #-6 .. 2
                    legMoveZ = ord(buf[1])/32.0-1.0 #-1 .. 7
                    stringToSend = "S 4 %g %g %g"%(legMoveX,legMoveY,legMoveZ)
                else:
                    speedx = ord(buf[0])/-128.0+1.0
                    speedy = ord(buf[1])/-64.0+2.0
                    print "Speed x,y: (%g,%g)"%(speedx,speedy)
                    if math.hypot(speedx,speedy)>0.4:
                        stringToSend = "W %g %g %g %d"%(speedx,speedy,0,1*25)
                    else:
                        rotx = (ord(buf[3])/128.0-1.0)*0.25
                        roty = 0
                        rotz = (ord(buf[4])/128.0-1.0)*0.25
                        height = ord(buf[2])/128.0-1.0
                        stringToSend = "R %g %g %g %g"%(rotx,roty,rotz,height)
                if button1:
                    if not lastbutton1:
                        lastbutton1 = True
                        currentGait += 1
                        if currentGait > 2:
                            currentGait = 0
                        stringToSend = "G %d"%currentGait
                else:
                    lastbutton1 = False
            print "stringToSend: '%s'"%stringToSend
            client.publish("motioncontrol", stringToSend)

if __name__ == "__main__":
    main()
