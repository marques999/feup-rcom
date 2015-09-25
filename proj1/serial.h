#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define M_RECEIVER		0
#define M_TRANSMITTER	1

#define BAUDRATE		B9600
#define _POSIX_SOURCE 	1 /* POSIX compliant source */

#define BOOL	int
#define FALSE 	0
#define TRUE 	1