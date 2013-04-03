#Scans horizontally and shows int values
import serial
import time

def measure(s):
  s.write("VA\r")
  return ord(s.read())

s = serial.Serial(port='/dev/ttyAMA0',baudrate=115200)
for y in range(25):
  yservopos=1700+(y*200/25)
  s.write("#0 P"+str(yservopos)+" #1 P1300\r")
  time.sleep(0.3)
  s.write("#1 P1700 S400\r")
  for i in range(25):
    print measure(s)
    time.sleep(0.04)
  print "|"
  yservopos=1500-200+(y*400/25)
  time.sleep(0.1)
s.close()
