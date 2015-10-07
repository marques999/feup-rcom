#include "shared.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1

volatile int STOP = FALSE;

int sendFrame(int fd, unsigned char* buffer) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < 5; i++) {
		if (write(fd, &buffer[i], sizeof(unsigned char)) == 1) {
			printf("[OUT] sending packet: 0x%x\n", buffer[i]);
			bytesWritten++;
		}
	}

	return bytesWritten;
}

void generateUA(unsigned char buffer[5]) {
	buffer[0] = FLAG;
	buffer[1] = A_UA;
	buffer[2] = C_UA; 
	buffer[3] = buffer[1] ^ buffer[2];
	buffer[4] = FLAG;
}

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

int main(int argc, char** argv) {

    unsigned char frame[5];

    struct termios oldtio;
    struct termios newtio;

    if (argc < 2 || !checkPath(argv[1])) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
    }

	/*
	 * Open serial port device for reading and writing and not as controlling tty
	 * because we don't want to get killed if linenoise sends CTRL-C.
	 */
    int fd = open(argv[1], O_RDWR | O_NOCTTY);

	if (fd < 0) {
		perror(argv[1]); 
		exit(-1); 
    }

	/* save current port settings */
    if (tcgetattr(fd,&oldtio) == -1) {
		perror("tcgetattr");
		exit(-1);
	}

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

    tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
    }

    printf("[LOGGING] new termios structure set\n");


	tcsetattr(fd, TCSANOW, &oldtio);
	close(fd);

    return 0;
}

#define TRANSMITTER		0
#define RECEIVER		1

int llopen(int port, int mode) {
		
}

void ... {
	    int state = START;

	while (state != STOP_OK) 
	{
		switch(state) 
		{
		case START:

			printf("[STATE] entering START state...\n");

			if (read(fd, &frame[0], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[0] == FLAG) {
				printf("[READ] received FLAG\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
			}

			break;

		case FLAG_RCV:
			
			printf("[STATE] entering FLAG_RCV state...\n");

			if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[1] == A_SET) {
				printf("[READ] received A_SET\n");
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
				state = START;
			}

			break;
		
		case A_RCV:

			printf("[STATE] entering A_RCV state...\n");

			if (read(fd, &frame[2], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[2] == C_SET) {
				printf("[READ] received C_SET\n");
				state = C_RCV;
			}
			else if(frame[2] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
				state = START;
			}

			break;
		
		case C_RCV:

			printf("[STATE] entering C_RCV state...\n");

			if (read(fd, &frame[3], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[3] == (A_SET ^ C_SET)) {
				printf("[READ] received correct BCC checksum!\n");
				state = BCC_OK;
			}
			else if (frame[3] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] wrong BCC checksum, returning to START...\n");
				state = START;
			}

			break;
		
		case BCC_OK:

			printf("[STATE] entering BCC_OK state...\n");

			if (read(fd, &frame[4], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[4] == FLAG) {
				printf("[READ] received FLAG\n");
				state = STOP_OK;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");;
				state = START;
			}

			break;
		}
    }
	
	unsigned char UA[5];

	generateUA(UA);
  	printf("[OUT] sent response, %d bytes written\n", sendFrame(fd, UA));
	printf("[END] connection established \n");
}
