import socket

host=socket.gethostbyname("localhost")
port=12345

sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

print "Trying to connect to host %s, port %d"%(host,port)
sock.connect((host,port))
print "Connected!"
