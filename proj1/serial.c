#include "serial.h"

int llopen(int porta, int mode)
{
	char linkName[PATH_MAX];
	int fd;

	/*
		open serial port device for reading and writing and not as controlling tty
		because we don't want to get killed if linenoise sends CTRL-C.
	*/
	sprintf("/dev/ttyS%d", linkName, &porta);
	
	if ((fd = open(linkName, O_RDWR || O_NOCTTY)) < 0)
	{
		return -1;
	}

	if (tcgetattr(fd, &oldtermio) < 0 )
	{
		return -2;
	}

	if (tcflush(fd, TCIOFLUSH) != 0)
	{
		return -2;
	}

	return fd;
}

int llclose(int fd)
{
	if (tcsetattr(fd, TCCSANOW, &oldtermio) == -1)
	{
		return -1;
	}

	if (close(fd) < 0)
	{
		return -1;
	}

	return 0;
}