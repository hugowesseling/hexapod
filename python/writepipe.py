import os

filename = "/tmp/myFIFO"
print filename
try:
    os.mkfifo(filename)
except OSError, e:
    print "Failed to create FIFO: %s" % e
else:
    fifo = open(filename, 'r')
    # write stuff to fifo
    print "Read:"+fifo.read()
    fifo.close()
