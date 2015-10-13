#ifndef __LINK_H_
#define __LINK_H_

#include <termios.h>
#include "shared.h"

#define MODE_TRANSMITTER 0
#define MODE_RECEIVER 1

typedef struct {

	char port[20];			// serial port (/dev/ttyS0, /dev/ttyS1...)
	unsigned char ns;		// frame sequence number (0 | 1)

	int fd;
	int messageDataMaxSize;	
	int connectionBaudrate;			// serial transmission speed
	int connectionMode;		// connection mode (TRANSMITTER | RECEIVE)
	int connectionTimeout;	// connection timeout value (in seconds)	
	int connectionTries;	// number of retries in case of failure

	char frame[MAX_SIZE];	// actual frame

	struct termios oldtio; 	// old termios struct (serial port configuration)
	struct termios newtio; 	// new termios struct (serial port configuration)

	int numSent; 			// number of frames sent
	int numSentRR; 			// number of reciever ready messages sent
	int numSentREJ; 		// number of rejected (negative ACK) messages sent
	int numReceived; 		// number of frames received
	int numReceivedRR; 		// number of reciever ready messages received
	int numReceivedREJ; 	// number of received rejected messages received
	int numTimeouts;		// number of connection timeouts
	
} LinkLayer;

extern LinkLayer* ll;

int link_initialize(const char* port, int fd, int mode);
int link_getBaudrate(int baudrate);

void printConnectionInfo();
void printStatistics();

unsigned char* generateDISC(int source);
unsigned char* generateRR(int source, int nr);
unsigned char* generateSET(int source);
unsigned char* generateUA(int source);

void link_setBaudrate(int baudrate);
void link_setNumberRetries(int numRetries);
void link_setTimeout(int timeout);

#endif /* __LINK_H_ */
