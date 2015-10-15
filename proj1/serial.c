#include "link.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1
#define MAX_TRIES 3

#define TRANSMITTER 0
#define RECEIVER 1


int alarmFlag = FALSE;
int alarmCounter = 0;

void alarm_handler() {
	alarmFlag = TRUE;
	alarmCounter++;
	alarm(3);
}

void alarm_start() {

	struct sigaction action;

	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	
	alarmCounter = 0;
	alarmFlag = FALSE;
	alarm(3);
}

void alarm_stop() {

	struct sigaction action;

	action.sa_handler = NULL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	
	alarmCounter = 0;
	alarm(0);
}

int receiveCommand(int fd, unsigned char* original) {

	int rv = 0;
	int state = START;
	unsigned char frame[5];
	
	while (state != STOP_OK) {
	
		if (alarmFlag == TRUE) {
			alarmFlag = FALSE;
			rv = -1;
			state = STOP_OK;
		}
		
		switch (state) 
		{
		case START:

			printf("[STATE] entering START state...\n");

			if (read(fd, &frame[0], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[0] == original[0]) {
				printf("[READ] received FLAG\n");
				state = FLAG_RCV;
			}

			break;

		case FLAG_RCV:
			
			printf("[STATE] entering FLAG_RCV state...\n");

			if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[1] == original[1]) {
				printf("[READ] received ADDRESS\n");
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

			if (frame[2] == original[2]) {
				printf("[READ] received COMMAND\n");
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

			if (frame[3] == (original[1] ^ original[2])) {
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

			if (frame[4] == original[4]) {
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
    
    return rv;
}

int llclose_TRANSMITTER(int fd) {

	unsigned char* DISC = generateDISC(RECEIVER);
	int disconnected = FALSE;
	int rv = 0;
	
	alarm_start();
	
	while (!disconnected) {

		if (alarmCounter == MAX_TRIES) {
			disconnected = TRUE;
			rv = -1;
		}

		printf("[OUT] sent DISC command, %d bytes written...\n", sendDISC(fd, TRANSMITTER));
		printf("[READ] waiting for DISC response from receiver, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES + 1);

		if (receiveCommand(fd, DISC) == 0) {
			disconnected = TRUE;
		}
	}

	alarm_stop();
	
	if (rv == 0) {
		int nBytes = sendUA(fd, TRANSMITTER);
	
		if (nBytes == S_LENGTH) {
			printf("[OUT] sent UA command, %d bytes written...\n", nBytes);
		} else {
			printf("[OUT] sending UA failed...\n");
		}
	}
	else {
		printf("ERROR: Maximum number of retries exceeded.\n");
		printf("*** Connection aborted. ***\n");
	}
		
	return rv;
}

int llclose_RECEIVER(int fd) {

	unsigned char* DISC = generateDISC(TRANSMITTER);
	unsigned char* UA = generateUA(TRANSMITTER);
	int disconnected = FALSE;
	int uaReceived = FALSE;
	int rv = 0;
	
	printf("[READ] waiting for DISC response from transmitter...\n");
	
	while (!disconnected) {
		if (receiveCommand(fd, DISC) == 0) {
			disconnected = TRUE;
		}
	}

	alarm_start();
	
	while (!uaReceived) {
	
		if (alarmCounter == MAX_TRIES) {
			uaReceived = TRUE;
			rv = -1;
		}

		printf("[OUT] sent DISC command, %d bytes written...\n", sendDISC(fd, RECEIVER));
		printf("[READ] waiting for UA response from transmitter, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES + 1);
	
		if (receiveCommand(fd, UA) == 0) {
			uaReceived = TRUE;
		}
	}

	alarm_stop();
	
	if (rv == -1) {
		printf("ERROR: Disconnect could not be sent.\n");
	} 

	return rv;
}

int llclose(int fd, int mode) {
	
	int rv = 0;
	
	if (mode == MODE_TRANSMITTER) {
		rv = llclose_TRANSMITTER(fd);
	}
	else {
		rv = llclose_RECEIVER(fd);
	}
	
	if (rv == 0) {
		printf("*** Connection terminated. ***\n");
	}
	
	return rv;
}

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS2", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
int llread(int fd, int ns, unsigned char* out) {

	unsigned char data[255];

	if (read(fd, &data[0], sizeof(unsigned char)) > 1) {
		printf("[READ] more than one byte received");
		return -1;
	}

	if (data[0] != FLAG) {
		printf("[IN] receive failed: recieved invalid symbol, expected FLAG...\n");
		return -1;
	}

	int nBytes = 1;
	int readSuccessful = FALSE;

	while (read(fd, &data[nBytes], sizeof(unsigned char)) > 0) {
		if (data[nBytes++] == FLAG) {
			readSuccessful = TRUE;
			break;
		}
	}

	if (!readSuccessful) {
		printf("[READ] receive failed: unexpected end of frame\n");
		return -1;	
	}

	printf("number of bytes: %d\n", nBytes);
	if (nBytes < 7) {
		printf("[READ] receive failed: not a data packet, must be at least 7 bytes long\n");
		return -1;
	}

	if (data[1] != A_SET) {
		printf("[IN] receive failed: recieved invalid symbol, expected A_SET...\n");
		return -1;	
    }

    if (data[2] != (ns << 5)) {
		printf("[IN] receive failed: recieved invalid symbol, expected 0 or 1...\n");
		return -1;	
    }

	if (data[3] != (A_SET ^ (ns << 5))) {
		printf("[IN] receive failed: wrong BCC checksum...\n");
		return -1;	
    }

    if (data[4] == FLAG) {
    	printf("[IN] receive failed: unexpected FLAG found in beginning of DATA");
    	return -1;
    }

    unsigned char BCC2 = 0;
    int j;
    int i = 0;

for (j = 4; j < nBytes - 2; i++, j++) {
   
        unsigned char byte = data[j];
        unsigned char destuffed;
        
        if (byte == ESCAPE) {
			destuffed = data[j + 1] ^ 0x20;
			out[i] = destuffed;
			BCC2 ^= destuffed;
			j++;
        }
        else {
            	out[i] = byte;
		BCC2 ^= byte;
        }    
    }
    
    if (BCC2 != data[j++]) {
		printf("[IN] receive failed: wrong BCC2 checksum\n");
		return -1;
	}

	if (data[j] != FLAG) {
		printf("[IN] receive failed: recieved invalid symbol, expected FLAG...\n");
		return -1;
	}

	int k;
	for (k = 0; k < i; k++) {
		printf("received value: 0x%x (%c)\n", out[k], (char)(out[k]));	
	}

	sendRR(fd, RECEIVER, !ns);
    
	return nBytes;
}

int llwrite(int fd, int ns, unsigned char* data, int data_sz) {
	
    unsigned char I[2*MAX_SIZE+6];
    unsigned char BCC2 = 0; 

    I[0] = FLAG;
	I[1] = A_SET;
	I[2] = ns << 5; 
	I[3] = I[1] ^ I[2];
    
    int i = 4;
    int j = 0;
    
    for (j = 0; j < data_sz; j++) {
   
        unsigned char byte = data[j];
        
        BCC2 ^= byte;
        
        if (byte == FLAG || byte == ESCAPE) {
            I[i++] = ESCAPE;
            I[i++] = byte ^ 0x20;
        }
        else {
            I[i++] = byte;
        }    
    }
    
    I[i++] = BCC2;
    I[i++] = FLAG;
    
    for (j = 0; j < i; j++) {
        printf("0x%x\n", I[j]);
    }

	int frameSent = FALSE;    
	unsigned char* RR = generateRR(RECEIVER, !ns);
	
	alarm_start();

	while (!frameSent && alarmCounter < MAX_TRIES) {
	
		printf("[OUT] sending information frame (attempt %d/%d...\n", alarmCounter + 1, MAX_TRIES + 1);
		sendFrame(fd, I, i);
		printf("[READ] waiting for RR response from receiver (attempt %d/%d)...\n", alarmCounter + 1, MAX_TRIES + 1);
		
		if (receiveCommand(fd, RR) == 0) {
			frameSent = TRUE;
			break;
		}
	}
	
	alarm_stop();

	if (frameSent) {
		printf("[READ] received response successfully!\n");
	}
	else {
		printf("[READ] response failed after %d tries...\n", alarmCounter);
	}

	return i;
}

struct termios oldtio;
struct termios newtio;

int main(int argc, char** argv) {

    if (argc < 3 || !checkPath(argv[2])) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial r(eceive)/s(end) /dev/ttyS1\n");
		exit(1);
    }
    
    char* modeString = argv[1];
	int mode = -1;

    if (strcmp("r", modeString) == 0 || strcmp("receieve", modeString) == 0) {
		mode = MODE_RECEIVER;
	}
	else if (strcmp("s", modeString) == 0 || strcmp("send", modeString) == 0) {
		mode = MODE_TRANSMITTER;
	}
	else {
		printf("invalid mode '%s'...", argv[1]);
	}

	int fd = llopen(argv[2], mode);
	
	if (fd < 0) {
		return -1;
	}

	int ns = 0;

	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	unsigned char out[255];
	unsigned char data[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!' };

    if (mode == MODE_TRANSMITTER) {
		llwrite(fd, ns, data, sizeof(data)/sizeof(data[0]));
	} else {
		llread(fd, ns, out);
	}

	llclose(fd, mode);
	close(fd);

    return 0;
}

int llopen(char* port, int mode) {

	/*
	 * Open serial port device for reading and writing and not as controlling tty
	 * because we don't want to get killed if linenoise sends CTRL-C.
	 */
	int ret;
    int fd = open(port, O_RDWR | O_NOCTTY);
    
	if (fd < 0) {
		perror(port); 
		return -1;
    }

	/* save current port settings */
    if (tcgetattr(fd,&oldtio) == -1) {
		perror("tcgetattr");
		return -1;
	}
	
	printf("[LOGGING] new termios structure set\n");

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
		return -1;
    }

	if (mode == MODE_TRANSMITTER) {
		ret = llopen_TRANSMITTER(fd);
	}
	else if (mode == MODE_RECEIVER) {
		ret = llopen_RECEIVER(fd);
	} 
	else {
		ret = -1;
	}
	
	if (ret >= 0) {
		printf("[END] connection established sucessfully!\n");
	} else {
		return -1;
	}
	
	return fd;
}

int llopen_RECEIVER(int fd) {

	if (receiveCommand(fd, generateSET(TRANSMITTER)) == 0) {
		printf("[OUT] sent UA response, %d bytes written\n", sendUA(fd, RECEIVER));
	}
  	  	
  	return 0;
}

int llopen_TRANSMITTER(int fd) {
	
	unsigned char* UA = generateUA(RECEIVER);
	int readTimeout = FALSE;
	int rv = 0;
	
	alarm_start();
	
	while (!readTimeout) {	

		if (alarmCounter > MAX_TRIES) {
			readTimeout = TRUE;
			rv = -1;
		}

		printf("[OUT] sent SET command, %d bytes written...\n", sendSET(fd, TRANSMITTER));
		printf("[READ] waiting for UA response from receiver, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES);

		if (receiveCommand(fd, UA) == 0) {
			readTimeout = TRUE;
		}
	}
	
	alarm_stop();
	
	if (rv == -1) {
		printf("[END] connection failed after %d retries...", alarmCounter);
	}
	
	return rv;
}
