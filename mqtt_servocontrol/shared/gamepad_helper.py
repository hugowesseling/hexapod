import math
import array

GP_PORT = 13254
TAU_IP = '192.168.1.84'

packetSize = 15 #5+10 #+2

def float01to255int(value):
  byteVal = int((value+1)*128)
  if byteVal>255: byteVal = 255
  if byteVal<0: byteVal = 0
  return byteVal

