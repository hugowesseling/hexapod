'''
  Finds AR markers
  todo: 
    extract edges directly from Canny function
    try out canny with higher apertureSize
'''

import sys
import time
import simplesocket

def main():
  #Setup
  sock = simplesocket.simplesocket(12345,"192.168.1.84")

  #Mainloop
  while True:
      #print "Sending \"%s\" and then waiting %d seconds"%(stringToSend,executeTime+captureDelayTime)
      #sock.send(stringToSend)
      received,buf = sock.receive()
      if received:
         print "Received:%r"%buf
      time.sleep(0.1)

main()
