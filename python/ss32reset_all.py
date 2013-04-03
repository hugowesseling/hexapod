import serial
import time

ZEROPOSSTRING="#21PO-50 #7PO-20 #6PO-20 #8PO70 #16PO-50 #18PO30\r"

s = serial.Serial(port='/dev/ttyAMA0',baudrate=115200)
s.write(ZEROPOSSTRING)
fullcommand=""
for i in range(0,32):
  fullcommand+="#"+str(i)+" P1500 "
print("Sending \""+fullcommand+"\"")
s.write(fullcommand+"\r")
time.sleep(1.0)
s.close()

