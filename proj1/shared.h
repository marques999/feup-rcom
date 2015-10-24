#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define _POSIX_SOURCE 1

#define A_SET		0x03
#define A_UA		0x01
#define C_DIS		0x0b
#define C_SET		0x07
#define C_REJ		0x05
#define C_RR		0x01
#define C_UA		0x03

#define FLAG		0x7e
#define ESCAPE      0x7d
#define BYTE_XOR	0x20
#define FALSE		0
#define TRUE		1

#define TRANSMITTER 	0
#define RECEIVER 		1

#define MAX_SIZE	384
#define I_LENGTH	256
#define S_LENGTH	5