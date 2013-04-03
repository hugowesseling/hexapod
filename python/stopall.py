#Sends discrete output 0 to all.
import serial
import time

s = serial.Serial(port='/dev/ttyAMA0',baudrate=115200)
fullcommand=""
for i in range(0,32):
  fullcommand+="#"+str(i)+"L "
print("Sending \""+fullcommand+"\"")
s.write(fullcommand+"\r")
time.sleep(0.1)
s.close()
