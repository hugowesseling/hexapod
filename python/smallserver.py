
import socket
host=socket.gethostbyname("localhost")
port=12345

sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
sock.bind((host, port))
print "Listening on port %d"%port
sock.listen(5)
c, addr = sock.accept() 
print "Connected as server to port %d"%port

