import sys
#sys.path.append(r"C:\opencv\build\python\2.7")
import cv2
import cv
import time
import simplesocket
import simplemjpegsocket

imageWidth=640 #1280
imageHeight=360 #720


def setupCapture():
  cap = cv2.VideoCapture(0)
  if cap.isOpened():
    print "Camera is opened!"
  else:
    print "Failed to open camera"
    exit(0)

  #set the width and height
  cap.set(3,imageWidth)
  cap.set(4,imageHeight)
  return cap


cap = setupCapture()
sock = simplemjpegsocket.create(8080)
#Mainloop
while True:
  #Analyze camera picture
  ret, img = cap.read()
  print "cap.read():"+str(ret)
  simplemjpegsocket.send(sock, img)
  #cv2.imshow("img", img)
  if cv2.waitKey(1) & 0xFF == ord('q'):
    break

cap.release()
cv2.destroyAllWindows()

