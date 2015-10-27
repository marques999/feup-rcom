#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define _POSIX_SOURCE 1

#define FLAG		0x7e
#define ESCAPE     	0x7d
#define BYTE_XOR	0x20
#define FALSE		0
#define TRUE		1

#define TRANSMITTER 	0
#define RECEIVER 	1

#define MAX_SIZE	768+4+4
#define PATH_MAX	255
#define I_LENGTH	384
#define S_LENGTH	5
