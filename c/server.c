// File name . server.c
// Written and tested on Linux Fedora Core 12 VM

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 50

int main()
{
        // Two socket descriptors which are just integer numbers used to access a socket
        int sock_descriptor, conn_desc;

        // Two socket address structures - One for the server itself and the other for client
        struct sockaddr_in serv_addr, client_addr;

        // Buffer to store data read from client
        char buff[MAX_SIZE];

        // Create socket of domain - Internet (IP) address, type - Stream based (TCP) and protocol unspecified
        // since it is only useful when underlying stack allows more than one protocol and we are choosing one.
        // 0 means choose the default protocol.
        sock_descriptor = socket(AF_INET, SOCK_STREAM, 0);

        // A valid descriptor is always a positive value
        if(sock_descriptor < 0)
          printf("Failed creating socket\n");

        // Initialize the server address struct to zero
        bzero((char *)&serv_addr, sizeof(serv_addr));

        // Fill server's address family
        serv_addr.sin_family = AF_INET;

        // Server should allow connections from any ip address
        serv_addr.sin_addr.s_addr = INADDR_ANY;

        // 16 bit port number on which server listens
        // The function htons (host to network short) ensures that an integer is interpretted
        // correctly (whether little endian or big endian) even if client and server have different architectures
        serv_addr.sin_port = htons(1234);
 
        // Attach the server socket to a port. This is required only for server since we enforce
        // that it does not select a port randomly on it's own, rather it uses the port specified
        // in serv_addr struct.
        if (bind(sock_descriptor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        	printf("Failed to bind\n");
       
        // Server should start listening - This enables the program to halt on accept call (coming next)
        // and wait until a client connects. Also it specifies the size of pending connection requests queue
        // i.e. in this case it is 5 which means 5 clients connection requests will be held pending while
        // the server is already processing another connection request.
        listen(sock_descriptor, 5);
 
        printf("Waiting for connection...\n");
        int size = sizeof(client_addr);

        // Server blocks on this call until a client tries to establish connection.
        // When a connection is established, it returns a 'connected socket descriptor' different
        // from the one created earlier.
        conn_desc = accept(sock_descriptor, (struct sockaddr *)&client_addr, &size);         
        if (conn_desc == -1)
        	printf("Failed accepting connection\n");
        else
		printf("Connected\n");

        // The new descriptor can be simply read from / written up just like a normal file descriptor
        if ( read(conn_desc, buff, sizeof(buff)-1) > 0)
        	printf("Received %s", buff);

        else
        	printf("Failed receiving\n");


        // Program should always close all sockets (the connected one as well as the listening one)
        // as soon as it is done processing with it
        close(conn_desc);
        close(sock_descriptor); 
	return 0;
}
