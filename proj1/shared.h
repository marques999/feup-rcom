#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define A_SET		0x03
#define A_UA		0x01

#define C_SET		0x07
#define C_DIS		0x0b
#define C_UA		0x03

#define INDEX_FLAG_START		0
#define INDEX_A					1
#define INDEX_C 				2
#define INDEX_BCC1				3
#define INDEX_FLAG_END			4

#define FLAG		0x7e
#define ESCAPE      0x7d
#define FALSE		0
#define TRUE		1

#define STOP_OK		0
#define START		1
#define FLAG_RCV	2
#define A_RCV		3
#define C_RCV		4
#define BCC_OK		5
#define RESEND		6

#define TRANSMITTER 0
#define RECEIVER 1

#define MODE_TRANSMITTER	0
#define MODE_RECEIVER		1

#define MAX_SIZE	255
#define I_LENGTH	255
#define S_LENGTH	5
