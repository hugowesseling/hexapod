import pygame
import math
import array

GP_PORT = 13254

packetSize = 5 #+10+2

def float01to255int(value):
  byteVal = int((value+1)*128)
  if byteVal>255: byteVal = 255
  if byteVal<0: byteVal = 0
  return byteVal

def encode(joystick):
  #Parts to encode: axis 0-4, button 0-9, hat 0
  intArray = []
  for axis in range(0,5):
    axisValue = joystick.get_axis(axis)
    axisByte = float01to255int(axisValue)
    intArray.append(axisByte)
  byteArray = array.array('B',intArray).tostring()
  if len(byteArray) != packetSize:
    print "ERROR: Not encoding into correct packetSize: len(%r)=%d != %d"%(byteArray,len(byteArray),packetSize)
    exit(1)
  return byteArray
