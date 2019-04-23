#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                printf ("set_interface_attribs: ERROR %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("set_blocking: ERROR %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf ("set_blocking: ERROR %d setting term attributes", errno);
}


int serialOpen(char *portname, int baudrate)
{
	int serialSpeed = 0;
	switch(baudrate)
	{
		case 115200:
			serialSpeed = B115200;
			break;
		default:
			printf("serialOpen: ERROR: cannot switch to baudrate: %d\n",baudrate);
			return 0;
	}
	int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);

        if(fd < 0)
        {
                printf ("serialOpen: ERROR: %d opening %s: %s", errno, portname, strerror (errno));
                return 0;
        }

        set_interface_attribs (fd, serialSpeed, 0);
        set_blocking (fd, 0);
	return fd;
}

void serialClose(int fd)
{
	close(fd);
}

void serialPuts(int fd,const char *sendstr)
{
	printf("serialPuts: Sending ***\n%s\n*** over serial fd:%d\n",sendstr,fd);
	if(!fd)return;
	int len = strlen(sendstr);
        write (fd, sendstr, len);
        usleep ((len + 25) * 100);
}

int serialGetchar(int fd)
{
	char buf [2];
        int n = read (fd, buf, 1);  // read up to 100 characters if ready to read
        buf[1]=0;
        printf("serialGetchar: Received %d chars: %s\n",n,buf);
	return buf[0];
}
