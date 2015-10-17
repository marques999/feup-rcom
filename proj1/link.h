#ifndef __LINK_H_
#define __LINK_H_

#include <termios.h>
#include "shared.h"

typedef struct {

	char port[20];			// serial port (/dev/ttyS0, /dev/ttyS1...)
	unsigned char ns;		// frame sequence number (0 | 1)

	int fd;
	int messageDataMaxSize;	
	int connectionBaudrate;	// serial transmission speed
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

extern	LinkLayer* ll;

int		llopen(char* port, int mode);
int		llclose(int fd, int mode);
int		llread(int fd, unsigned char* buffer);
int		llwrite(int fd, unsigned char* buffer, int length);

int 	llInitialize(const char* port, int fd, int mode);
int		llGetBaudrate(int baudrate);

void 	llSetBaudrate(int baudrate);
void	llSetNumberRetries(int numRetries);
void	llSetTimeout(int timeout);
void 	printConnectionInfo();
void 	printStatistics();

#endif /* __LINK_H_ */
