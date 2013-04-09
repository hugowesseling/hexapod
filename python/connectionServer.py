import simplesocket
import time

sock=simplesocket.simplesocket(12345)

counter=0
morecounter=0
while True:
    print "Bla%d"%counter
    counter+=1
    time.sleep(0.5)
    if counter>3:
        counter=0
        morecounter+=1
        print "Sending"
        sock.send("Testing %d"%morecounter)
    print "Testing to receive"
    received,buf=sock.receive()
    if received:
        print "Received:",buf


'''
import socket

s = socket.socket()         # Create a socket object
host = socket.gethostname() # Get local machine name
port = 12345                # Reserve a port for your service.
s.bind((host, port))

s.listen(5)                 # Now wait for client connection.

c, addr = s.accept()     # Establish connection with client.
print 'Got connection from', addr
c.send('Thank you for connecting')
c.close()                # Close the connection
'''