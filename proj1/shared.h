#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define _POSIX_SOURCE 	1

#define FALSE		0
#define TRUE		1

#define TRANSMITTER 	0
#define RECEIVER 		1

#define PATH_MAX	255
#define MAX_SIZE	20000
#define I_LENGTH	10000
#define S_LENGTH	5
