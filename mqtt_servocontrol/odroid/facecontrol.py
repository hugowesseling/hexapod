import paho.mqtt.client as mqtt
import sys
import cv2.cv as cv
from optparse import OptionParser
import time
import math


# Parameters for haar detection
# From the API:
# The default parameters (scale_factor=2, min_neighbors=3, flags=0) are tuned 
# for accurate yet slow object detection. For a faster operation on real video 
# images the settings are: 
# scale_factor=1.2, min_neighbors=2, flags=CV_HAAR_DO_CANNY_PRUNING, 
# min_size=<minimum possible face size

min_size = (20, 20)
image_scale = 1
haar_scale = 1.2
min_neighbors = 2
haar_flags = 0
pan=0
tilt=0
prevpanchange=0
prevtiltchange=0

def detect_and_draw(img, cascade):
    global pan,tilt,prevpanchange,prevtiltchange
    # allocate temporary images
    gray = cv.CreateImage((img.width,img.height), 8, 1)
    cv.CvtColor(img, gray, cv.CV_BGR2GRAY)

    cv.EqualizeHist(gray,gray)

    if(cascade):
        t = cv.GetTickCount()
        faces = cv.HaarDetectObjects(gray, cascade, cv.CreateMemStorage(0),
                                     haar_scale, min_neighbors, haar_flags, min_size)
        t = cv.GetTickCount() - t
        print "detection time = %gms" % (t/(cv.GetTickFrequency()*1000.))
        if faces:
            for ((x, y, w, h), n) in faces:
                # the input to cv.HaarDetectObjects was resized, so scale the 
                # bounding box of each face and convert it to two CvPoints
                pt1 = (int(x * image_scale), int(y * image_scale))
                pt2 = (int((x + w) * image_scale), int((y + h) * image_scale))
                cv.Rectangle(img, pt1, pt2, cv.RGB(255, 0, 0), 3, 8, 0)
                
                facemidx = x+w/2
                facemidy = y+h/2
                facemidx_fromcenter = (facemidx - img.width/2)*2
                facemidy_fromcenter = (facemidy - img.height/2)*2
                print "Face found at %d,%d" % (facemidx_fromcenter, facemidy_fromcenter)
                
                oldpan = pan
                oldtilt = tilt
                panchange = -facemidx_fromcenter # -prevpanchange
                tiltchange = -facemidy_fromcenter # -prevtiltchange
                pan += panchange
                tilt += tiltchange
                
                """
                rotx = -tilt*math.pi/2000
                roty = 0
                rotz = -pan*math.pi/2000
                """
                rotx = -tilt*math.pi/10000
                roty = 0
                rotz = -pan*math.pi/10000
                print "New pan tilt:%f,%f"%(rotz,rotx)
                
                client.publish("motioncontrol", "R %f %f %f"%(rotx,roty,rotz))

                #comOut.write("#0P%d#1P%d T300\r"%(tilt,pan))
                break

    cv.ShowImage("result", img)


def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

def on_message(client, userdata, msg):
    print("on_message: %r" % msg.payload)


if __name__ == '__main__':
    parser = OptionParser(usage = "usage: %prog [options] [filename|camera_index]")
    parser.add_option("-c", "--cascade", action="store", dest="cascade", type="str", help="Haar cascade file, default %default", default = "face.xml")
    (options, args) = parser.parse_args()

    if len(args) != 1:
        parser.print_help()
        sys.exit(1)

    #Startup
    cascade = cv.Load(options.cascade)
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect("192.168.1.84", 1883, 60)
    client.loop_start()

    input_name = args[0]
    if input_name.isdigit():
        capture = cv.CreateCameraCapture(int(input_name))
    else:
        capture = None

    cv.NamedWindow("result", 1)  #Uncomment to create the result window

    width = 160 #leave None for auto-detection
    height = 120 #leave None for auto-detection

    if width is None:
        width = int(cv.GetCaptureProperty(capture, cv.CV_CAP_PROP_FRAME_WIDTH))
    else:
        cv.SetCaptureProperty(capture,cv.CV_CAP_PROP_FRAME_WIDTH,width)    

    if height is None:
        height = int(cv.GetCaptureProperty(capture, cv.CV_CAP_PROP_FRAME_HEIGHT))
    else:
        cv.SetCaptureProperty(capture,cv.CV_CAP_PROP_FRAME_HEIGHT,height) 

    if capture:
        frame_copy = None
        while True:
            frame = cv.QueryFrame(capture)
            if not frame:
                cv.WaitKey(0)
                break
            if not frame_copy:
                frame_copy = cv.CreateImage((frame.width,frame.height),
                                            cv.IPL_DEPTH_8U, frame.nChannels)

            if frame.origin == cv.IPL_ORIGIN_TL:
                cv.Copy(frame, frame_copy)
            else:
                cv.Flip(frame, frame_copy, 0)
            
            detect_and_draw(frame_copy, cascade)

            if cv.WaitKey(10) >= 0:
                break
    else:
        image = cv.LoadImage(input_name, 1)
        detect_and_draw(image, cascade)
        cv.WaitKey(0)

    cv.DestroyWindow("result")
    s.close()
