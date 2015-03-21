import sys
#sys.path.append(r"C:\opencv\build\python\2.7")
import cv2
import cv
import time
import socket

def opensocket(port):
  sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
  sock.bind(("", port))
  sock.listen(5)
  c, addr = sock.accept() 
  #c.setblocking(0)
  return c

def create(port):
  sock = opensocket(8080)
  sock.send("HTTP/1.0 200 OK\r\nServer: Mozarella/2.2\r\nContent-Type: multipart/x-mixed-replace;boundary=mjpegstream\r\n\r\n")
  return sock

def send(sock,img):
  ret,buf  = cv2.imencode('.jpg', img,[cv.CV_IMWRITE_JPEG_QUALITY,80])
  buflen = len(buf)
  print "buflen: %d"%buflen
  sock.send("--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n"%buflen)
  sock.send(buf)
