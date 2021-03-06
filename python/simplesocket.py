import socket
import sys
import struct
import select

class simplesocket:
    def __init__(self,port,hostip = "127.0.0.1"):
        becomeServer=False
        self.sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        host=hostip
        try:
            self.sock.connect((host,port))
        except socket.error:
            becomeServer=True
        if becomeServer:
            print "No client found for port %d, becoming server"%port
            self.sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
            self.sock.bind(("", port))
            self.sock.listen(5)
            c, addr = self.sock.accept() 
            self.sock=c
            print "Connected as server to port %d"%port
        else:
            print "Connected as client to port %d"%port
        self.sock.setblocking(0)
            
    #Send and forget
    def send(self, msg):
        print "Simplesocket: Sending packet with %d bytes"%len(msg)
        us32bit = struct.pack("I", len(msg))
        self.sock.send(us32bit)
        totalsent = 0
        while totalsent < len(msg):
            sent = self.sock.send(msg[totalsent:])
            if sent == 0:
                raise RuntimeError("socket connection broken")
            totalsent = totalsent + sent

    #Non blocking receiving: Returns (bool somethingreceived,string receivedstuff)
    def receive(self):
        ready = select.select([self.sock], [], [], 0.1)
        if not ready[0]:
            return (False,'')
        us32bit=self.sock.recv(struct.calcsize("I"))
        print "Received:",
        msglen,=struct.unpack("I",us32bit)
        print "Received string with length: %d"%msglen
        msg = ''
        while len(msg) < msglen:
            ready = select.select([self.sock], [], [], 0.1)
            if ready[0]:
                chunk = self.sock.recv(msglen-len(msg))
                if chunk == '':
                    raise RuntimeError("socket connection broken")
                msg = msg + chunk
        return (True,msg)
