#include "link.h"
#include "alarm.h"

LinkLayer* ll;

#define BAUDRATE B9600
#define MAX_TRIES 3

/**
 * STATE MACHINE
 */
#define STOP_OK		0
#define START		1
#define FLAG_RCV	2
#define A_RCV		3
#define C_RCV		4
#define BCC_OK		5
#define RESEND		6

/**
 * 
 */
#define INDEX_FLAG_START		0
#define INDEX_A					1
#define INDEX_C 				2
#define INDEX_BCC1				3
#define INDEX_FLAG_END			4

static int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < buffer_sz; i++) {
		if (write(fd, &buffer[i], sizeof(unsigned char)) == 1) {
			bytesWritten++;
		}
	}

	return bytesWritten;
}

/*
 * ----------------------------- 
 *	  TRANSMISSION COMMANDS
 * -----------------------------
 */
static unsigned char* generateCommand(int source, unsigned char C) {

	unsigned char* CMD = malloc(sizeof(unsigned char) * S_LENGTH);

	CMD[INDEX_FLAG_START] = FLAG;

	if (source == MODE_TRANSMITTER) {
		CMD[INDEX_A] = A_SET;
	}
	else {
		CMD[INDEX_A] = A_UA;
	}

	CMD[INDEX_C] = C;
	CMD[INDEX_BCC1] = CMD[INDEX_A] ^ CMD[INDEX_C];
	CMD[INDEX_FLAG_END] = FLAG;

	return CMD;
}

static unsigned char* generateSET(int source) {
	return generateCommand(source, C_SET);
}

static unsigned char* generateDISC(int source) {
	return generateCommand(source, C_DIS);
}

static int sendSET(int fd, int source) {
	return sendFrame(fd, generateSET(source), S_LENGTH); 
}

static int sendDISC(int fd, int source) {
	return sendFrame(fd, generateDISC(source), S_LENGTH); 
}

/*
 * ----------------------------- 
 *		RESPONSE COMMANDS
 * -----------------------------
 */
static unsigned char* generateResponse(int source, unsigned char C) {
	
	unsigned char* CMD = malloc(sizeof(unsigned char) * S_LENGTH);

	CMD[INDEX_FLAG_START] = FLAG;

	if (source == MODE_TRANSMITTER) {
		CMD[INDEX_A] = A_UA;
	}
	else {
		CMD[INDEX_A] = A_SET;
	}

	CMD[INDEX_C] = C;
	CMD[INDEX_BCC1] = CMD[INDEX_A] ^ CMD[INDEX_C];
	CMD[INDEX_FLAG_END] = FLAG;

	return CMD;
}

static unsigned char* generateUA(int source) {
	return generateResponse(source, C_UA);
}

static unsigned char* generateRR(int source, int nr) {
	return generateResponse(source, (nr << 5) | C_RR);
}

static unsigned char* generateREJ(int source, int nr) {
	return generateResponse(source, (nr << 5) | C_REJ);
}

static int sendUA(int fd, int source) {
	return sendFrame(fd, generateUA(source), 5);
}

static int sendRR(int fd, int source, int nr) {
	return sendFrame(fd, generateRR(source, nr), 5);
}

static int sendREJ(int fd, int source, int nr) {
	return sendFrame(fd, generateREJ(source, nr), 5);
}

/*
 * ----------------------------- 
 *	MESSAGE RECEIVE STATE MACHINE
 * -----------------------------
 */
static int receiveCommand(int fd, unsigned char* original) {

	int rv = 0;
	int state = START;
	unsigned char frame[5];
	
	while (state != STOP_OK) {
	
		if (alarmFlag == TRUE) {
			alarmFlag = FALSE;
			state = STOP_OK;
			rv = -1;
		}
		
		switch (state) 
		{
		case START:

			puts("[STATE] entering START state...");

			if (read(fd, &frame[0], sizeof(unsigned char)) > 1) {
				puts("[READ] received more than one symbol!");	
			}

			if (frame[0] == original[0]) {
				puts("[READ] received FLAG");
				state = FLAG_RCV;
			}
			else {
				puts("[READ] received invalid symbol, returning to START...");
			}

			break;

		case FLAG_RCV:
			
			puts("[STATE] entering FLAG_RCV state...");

			if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
				puts("[READ] received more than one symbol!");	
			}

			if (frame[1] == original[1]) {
				puts("[READ] received ADDRESS");
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				puts("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				puts("[READ] received invalid symbol, returning to START...");
				state = START;
			}

			break;
		
		case A_RCV:

			puts("[STATE] entering A_RCV state...");

			if (read(fd, &frame[2], sizeof(unsigned char)) > 1) {
				puts("[READ] received more than one symbol!");	
			}

			if (frame[2] == original[2]) {
				puts("[READ] received COMMAND");
				state = C_RCV;
			}
			else if(frame[2] == FLAG) {
				puts("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				puts("[READ] received invalid symbol, returning to START...");
				state = START;
			}

			break;
		
		case C_RCV:

			puts("[STATE] entering C_RCV state...");

			if (read(fd, &frame[3], sizeof(unsigned char)) > 1) {
				puts("[READ] received more than one symbol!");	
			}

			if (frame[3] == (original[1] ^ original[2])) {
				puts("[READ] received correct BCC checksum!");
				state = BCC_OK;
			}
			else if (frame[3] == FLAG) {
				puts("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				puts("[READ] received wrong BCC checksum, returning to START...");
				state = START;
			}

			break;
		
		case BCC_OK:

			puts("[STATE] entering BCC_OK state...");

			if (read(fd, &frame[4], sizeof(unsigned char)) > 1) {
				puts("[READ] received more than one symbol!");	
			}

			if (frame[4] == original[4]) {
				puts("[READ] received FLAG");
				state = STOP_OK;
			}
			else {
				puts("[READ] received invalid symbol, returning to START...");;
				state = START;
			}

			break;
		}
    }
    
    return rv;
}

static int initializeTermios(int fd) {

    if (tcgetattr(fd, &ll->oldtio) < 0) {
		perror("tcgetattr");
		return -1;
	}
	
	bzero(&ll->newtio, sizeof(ll->newtio));

    ll->newtio.c_cflag = ll->connectionBaudrate | CS8 | CLOCAL | CREAD;
    ll->newtio.c_iflag = IGNPAR;
    ll->newtio.c_oflag = 0;
    ll->newtio.c_lflag = 0;
    ll->newtio.c_cc[VTIME] = 0;
    ll->newtio.c_cc[VMIN] = 1;

    if (tcflush(ll->fd, TCIFLUSH) < 0) {
    	perror("tcflush");
    	return -1;
    }

	if (tcsetattr(ll->fd, TCSANOW, &ll->newtio) < 0) {
		perror("tcsetattr");
    	return -1;
    }

    return 0;
}

static int resetTermios(int fd) {

	if (tcsetattr(fd, TCSANOW, &ll->oldtio) == -1) {
		perror("tcsetattr");
		return -1;
	}
	
	return 0;
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
int llread(int fd, unsigned char* buffer) {

	int nBytes;

	while ((nBytes = receiveData(fd, buffer)) == -1) {

	}

	sendRR(fd, RECEIVER, ll->ns);
    
	return nBytes;
}

int receiveData(int fd, unsigned char* buffer) {

	unsigned char data[255];

	int nBytes = 0;
	int readSuccessful = FALSE;

	while (read(fd, &data[nBytes], sizeof(unsigned char)) > 0) {
		if (data[nBytes++] == FLAG) {
			readSuccessful = TRUE;
			break;
		}
	}

	if (!readSuccessful) {
		puts("[READ] receive failed: unexpected end of frame");
		return -1;	
	}

	if (data[0] != FLAG) {
		puts("[READ] received invalid symbol, expected FLAG...");
		return -1;
	}

	if (nBytes < 7) {
		puts("[READ] received incomplete data packet, must be at least 7 bytes long!");
		return -1;
	}

	if (data[1] != A_SET) {
		puts("[READ] received invalid symbol, expected A_SET...");
		return -1;	
    }

    if (data[2] != (ll->ns << 5)) {
		puts("[READ] received invalid symbol, expected 0 or 1...");
		return -1;	
    }

    int ns = (data[2] & (1 << 5)) >> 5;
    int j;
    int i = 0;
	int k;

	if (ns != ll->ns) {
		puts("[READ] received wrong message, ignoring...");
		return -1;
	}

	if (data[3] != (A_SET ^ (ns << 5))) {
		puts("[READ] received wrong BCC checksum!");
		return -1;	
    }

    if (data[4] == FLAG) {
    	puts("[READ] receive failed: unexpected FLAG found in beginning of DATA");
    	return -1;
    }

    unsigned char expectedBCC2 = 0; 
    unsigned char receivedBCC2 = 0;
	
	for (j = 4; j < nBytes - 2; i++, j++) {

        unsigned char byte = data[j];
        unsigned char destuffed;
        
        if (byte == ESCAPE) {

        	if (j == nBytes - 3) {
   				break;
   			}

			destuffed = data[j + 1] ^ BYTE_XOR;
			buffer[i] = destuffed;
			expectedBCC2 ^= destuffed;
			j++;
        }
        else {
            buffer[i] = byte;
			expectedBCC2 ^= byte;
        }    
    }

    if (data[j++] == ESCAPE) {
    	receivedBCC2 = data[j++] ^ BYTE_XOR;
    } else {
    	receivedBCC2 = data[j++];
    }
    
    if (expectedBCC2 != receivedBCC2) {
		puts("[READ] received wrong BCC2 checksum, requesting retransmission...");
		sendREJ(fd, RECEIVER, ll->ns);
		return -1;
	}

	if (data[j] != FLAG) {
		puts("[READ] received invalid symbol, expected FLAG...");
		return -1;
	}

	for (k = 0; k < i; k++) {
		printf("[LLREAD] received data: 0x%x (%c)\n", buffer[k], (char)(buffer[k]));	
	}

	ll->ns = !ns;

	return nBytes;
}

int llwrite(int fd, unsigned char* buffer, int length) {
	
    unsigned char I[MAX_SIZE];
    unsigned char BCC2 = 0; 

    I[INDEX_FLAG_START] = FLAG;
	I[INDEX_A] = A_SET;
	I[INDEX_C] = ll->ns << 5; 
	I[INDEX_BCC1] = I[1] ^ I[2];
    
    int i = 4;
    int j = 0;
	int rv;
	
    for (j = 0; j < length; j++) {
   
        unsigned char byte = buffer[j];
        
        BCC2 ^= byte;
        
        if (byte == FLAG || byte == ESCAPE) {
            I[i++] = ESCAPE;
            I[i++] = byte ^ BYTE_XOR;
        }
        else {
            I[i++] = byte;
        }    
    }
    
    if (BCC2 == FLAG) {
    	I[i++] = ESCAPE;
    	I[i++] = BCC2 ^ BYTE_XOR;
    }
 	else {
 		I[i++] = BCC2;
 	}
    
    I[i++] = FLAG;
    
	int sendTimeout = FALSE;
	unsigned char* RR = generateRR(RECEIVER, !ll->ns);

	while (!sendTimeout) {
			
		printf("[LLWRITE] sending information frame (attempt %d/%d...\n", alarmCounter + 1, MAX_TRIES + 1);
		
		if (sendFrame(fd, I, i) == i) {
			printf("[LLOPEN] sent INFORMATION frame to RECEIVER, %d bytes written...\n", i);
		}
		else {
			printf("[LLOPEN] error sending INFORMATION frame to RECEIVER, %d bytes written...\n", i);
			sendTimeout = TRUE;
			rv = -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}
		
		printf("[LLWRITE] waiting for RR response from RECEIVER (attempt %d/%d)...\n", alarmCounter + 1, MAX_TRIES + 1);
		
		if (receiveCommand(fd, RR) == 0) {
			puts("[LLWRITE] received RR response from RECEIVER!");
			sendTimeout = TRUE;
			rv = 0;
		}
		
		if (alarmCounter > MAX_TRIES) {
			printf("[LLWRITE] connection failed: no response from RECEIVER after %d attempts...\n", MAX_TRIES + 1);
			sendTimeout = TRUE;
			rv = -1;
		}
	}

	alarm_stop();

	return rv;
}

static int llopen_RECEIVER(int fd) {

	if (receiveCommand(fd, generateSET(TRANSMITTER)) != 0) {
		return -1;
	}
	
	int rv = 0;
	int nBytes = sendUA(fd, RECEIVER);

	if (nBytes == S_LENGTH) {
		printf("[LLOPEN] sent UA response to TRANSMITTER, %d bytes written...\n", nBytes);
	}
	else {
		printf("[LLOPEN] error sending UA response to TRANSMITTER, %d bytes written, expected 5 bytes...\n", nBytes);
		rv = -1;
	}

  	return rv;
}

static int llopen_TRANSMITTER(int fd) {
	
	unsigned char* UA = generateUA(RECEIVER);
	int readTimeout = FALSE;
	int rv;
	
	while (!readTimeout) {	
		
		int nBytes = sendSET(fd, TRANSMITTER);
	
		if (nBytes == S_LENGTH) {
			printf("[LLOPEN] sent SET command to RECEIVER, %d bytes written...\n", nBytes);
		}
		else {
			printf("[LLOPEN] error sending SET command to RECEIVER, %d bytes written, expected 5 bytes...\n", nBytes);
			readTimeout = TRUE;
			rv = -1;
		}
		
		if (alarmCounter == 0) {		
			alarm_start();
		}

		printf("[LLOPEN] waiting for UA response from RECEIVER, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES + 1); 

		if (receiveCommand(fd, UA) == 0) {
			puts("[LLWRITE] received UA response from RECEIVER!");
			readTimeout = TRUE;
			rv = 0;
		}
		
		if (alarmCounter > MAX_TRIES) {
			printf("[LLOPEN] connection failed: no response from receiver after %d attempts...\n", MAX_TRIES + 1);
			readTimeout = TRUE;
			rv = -1;
		}
	}
	
	alarm_stop();
	
	return rv;
}

int llopen(char* port, int mode) {

	/*
	 * Open serial port device for reading and writing and not as controlling tty
	 * because we don't want to get killed if line noise sends CTRL-C.
	 */
	int ret = -1;
    int fd = open(port, O_RDWR | O_NOCTTY);
    
	if (fd < 0) {
		perror(port); 
		return -1;
    }
    
    ll->fd = fd;	
	initializeTermios(fd);

	if (mode == MODE_TRANSMITTER) {
		ret = llopen_TRANSMITTER(fd);
	}
	else if (mode == MODE_RECEIVER) {
		ret = llopen_RECEIVER(fd);
	} 

	if (ret == -1) {
		return ret;
	}
	
	puts("[INFORMATION] connection established sucessfully!");
			
	return fd;
}

static int llclose_TRANSMITTER(int fd) {

	unsigned char* DISC = generateDISC(RECEIVER);
	int disconnected = FALSE;
	int nBytes = 0;
	int rv = 0;
		
	while (!disconnected) {

		if (alarmCounter == MAX_TRIES) {
			disconnected = TRUE;
			rv = -1;
		}

		nBytes = sendDISC(fd, TRANSMITTER);

		if (nBytes == S_LENGTH) {
			printf("[OUT] sent DISC command, %d bytes written...\n", nBytes);
		} 
		else {
			printf("[OUT] sending DISC failed...\n");
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}

		printf("[READ] waiting for DISC response from receiver, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES + 1);

		if (receiveCommand(fd, DISC) == 0) {
			disconnected = TRUE;
		}
	}

	alarm_stop();
	
	if (rv == 0) {
	
		nBytes = sendUA(fd, TRANSMITTER);
	
		if (nBytes == S_LENGTH) {
			printf("[OUT] sent UA response to receiver, %d bytes written...\n", nBytes);
		} else {
			printf("[OUT] error sending UA response to receiver, %d bytes written, expected 5 bytes...\n", nBytes);
			rv = -1;
		}
	}
	else {
		printf("[ERROR] LLCLOSE failed: maximum number of retries exceeded...\n");
		printf("*** Connection aborted ***\n");
	}
		
	return rv;
}

static int llclose_RECEIVER(int fd) {

	unsigned char* DISC = generateDISC(TRANSMITTER);
	unsigned char* UA = generateUA(TRANSMITTER);
	int disconnected = FALSE;
	int uaReceived = FALSE;
	int rv = 0;
	int nBytes = 0;
	
	printf("[READ] waiting for DISC response from transmitter...\n");
	
	while (!disconnected) {
		if (receiveCommand(fd, DISC) == 0) {
			disconnected = TRUE;
		}
	}

	while (!uaReceived) {
	
		if (alarmCounter == MAX_TRIES) {
			printf("[ERROR] LLCLOSE failed: no answer from transmitter after %d attempts...\n", MAX_TRIES + 1);
			uaReceived = TRUE;
			rv = -1;
		}

		nBytes = sendDISC(fd, RECEIVER);

		if (nBytes == S_LENGTH) {
			printf("[OUT] sent DISC command to transmitter, %d bytes written...\n", nBytes);
		} 
		else {
			printf("[OUT] error sending DISC command: %d bytes written, expected 5 bytes...\n", nBytes);
			uaReceived = TRUE;
			rv = -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}
		
		printf("[READ] waiting for UA response from transmitter, attempt %d of %d...\n", alarmCounter + 1, MAX_TRIES + 1);
	
		if (receiveCommand(fd, UA) == 0) {
			uaReceived = TRUE;
		}
	}

	alarm_stop();
	
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
		resetTermios(fd);
		close(fd);
	}
	
	return rv;
}

int llInitialize(const char* port, int fd, int mode) {

	ll = (LinkLayer*) malloc(sizeof(LinkLayer));
	strcpy(ll->port, port);
	ll->fd = fd;
	ll->ns = 0;
	ll->connectionBaudrate = B9600;
	ll->connectionMode = mode;
	ll->connectionTimeout = 3;
	ll->connectionTries = 4;
	ll->numSent = 0;
	ll->numSentRR = 0;
	ll->numSentREJ = 0;
	ll->numReceived = 0;
	ll->numReceivedRR = 0;
	ll->numReceivedREJ = 0;
	ll->numTimeouts = 0;
	
	return 0;
}

int link_getBaudrate(int baudrate) {
	
	switch (baudrate) {
		case 200: return B200;
		case 300: return B300;
		case 600: return B600;
		case 1200: return B1200;
		case 1800: return B1800;
		case 2400: return B2400;
		case 4800: return B4800;
		case 9600: return B9600;
		case 19200: return B19200;
		case 38400: return B38400;
		case 57600: return B57600;
	}

	return -1;
}

void logConnection() {
	puts("====================================");
	puts("=      CONNECTION INFORMATION      =");
	puts("====================================");

	switch (ll->connectionMode) {
	case MODE_TRANSMITTER:
		puts("# Mode:\t\t\t: TRANSMITTER");
		break;
	case MODE_RECEIVER:
		puts("# Mode:\t\t\t: RECEIVER");
		break;
	default:
		puts("# Mode:\t\t\t: ERROR");
		break;
	}

	printf("# Baudrate\t\t: %d\n", ll->connectionBaudrate);
	printf("# Maximum message size\t: %d\n", ll->messageDataMaxSize);
	printf("# Number retries\t: %d\n", ll->connectionTries - 1);
	printf("# Timeout interval\t: %d\n", ll->connectionTimeout);
	printf("# Serial port\t\t: %s\n", ll->port);
}

void logStatistics() {
	puts("\n=============================");
	puts("=   CONNECTION STATISTICS   =");
	puts("=============================");
	printf("# Sent messages:\t%d\n", ll->numSent);
	printf("# Received messages:\t%d\n", ll->numReceived);
	printf("#\n");
	printf("# Time-outs:\t%d\n", ll->numTimeouts);
	printf("#\n");
	printf("# Sent RR:\t%d\n", ll->numSentRR);
	printf("# Received RR:\t%d\n", ll->numReceivedRR);
	printf("#\n");
	printf("# Sent REJ:\t%d\n", ll->numSentREJ);
	printf("# Received REJ:\t%d\n", ll->numReceivedREJ);
}

void link_setBaudrate(int baudrate) {
	ll->connectionBaudrate = baudrate;
}

void link_setNumberRetries(int numberRetries) {
	ll->connectionTries = numberRetries;
}

void link_setTimeout(int timeout) {
	ll->connectionTimeout = timeout;
}

void printCommand(unsigned char* CMD) {
	puts("-------------------------");
	printf("- FLAG: 0x%02x\t\t-\n", CMD[0]);
	printf("- A: 0x%02x\t\t-\n", CMD[1]);
	printf("- C: 0x%02x\t\t-\n", CMD[2]);
	printf("- BCC: 0x%02x\t\t\n", CMD[3]);
	printf("- FLAG: 0x%02x\t\t-\n", CMD[4]);
	puts("-------------------------");
}