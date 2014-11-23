import sys
import cv2
import cv

imageWidth=640 #1280
imageHeight=360 #720

cap = cv2.VideoCapture(0)
if cap.isOpened():
  print "Camera is opened!"
else:
  print "Failed to open camera"
  exit(0)

  #set the width and height
  cap.set(3,imageWidth)
  cap.set(4,imageHeight)

while True:
  ret, img = cap.read()
  print "cap.read():%r"%ret
  cv2.imshow("img", img)
  cv2.imwrite("outimg.jpg",img);
  cv2.waitKey(1)
