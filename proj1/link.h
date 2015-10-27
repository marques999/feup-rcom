#ifndef __LINK_H_
#define __LINK_H_

#include <fcntl.h>
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

LinkLayer* llinit(char* port, int mode, int baudrate, int retries, int timeout, int maxsize);

int	llopen(char* port, int mode);
int	llclose(int fd);
int	llread(int fd, unsigned char* buffer);
int	llwrite(int fd, unsigned char* buffer, int length);

int	getBaudrate(int baudrate);

void logStatistics();
void logConnection();
void logCommand();

#endif /* __LINK_H_ */
