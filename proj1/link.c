#include "link.h"
#include "alarm.h"

LinkLayer* ll;

#define BAUDRATE B9600

/**
 * STATE MACHINE
 */
#define STOP		0
#define START		1
#define FLAG_RCV	2
#define A_RCV		3
#define C_RCV		4
#define BCC_OK		5

/**
 * INDEX DEFINITIONS
 */
#define INDEX_FLAG_START		0
#define INDEX_A					1
#define INDEX_C 				2
#define INDEX_BCC1				3
#define INDEX_FLAG_END			4

/**
 * DEBUG DEFINITIONS
 */
#define LINK_DEBUG			1
#define ERROR(msg)			fprintf(sderr, msg)
#define LOG(msg)			if (LINK_DEBUG) puts(msg)
#define LOG_FORMAT(msg)		if (LINK_DEBUG) printf(msg)

static int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < buffer_sz; i++) {
		if (write(fd, &buffer[i], sizeof(unsigned char)) == 1) {
			bytesWritten++;
		}
	}

	if (bytesWritten == buffer_sz) {
		ll->numSent++;
	}

	return bytesWritten;
}

/*
 * ----------------------------- 
 *	  TRANSMISSION COMMANDS
 * -----------------------------
 */
static unsigned char* generateCommand(int source, unsigned char C) {

	unsigned char* CMD = (unsigned char*) malloc(S_LENGTH * sizeof(unsigned char));

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
	
	unsigned char* CMD = (unsigned char*) malloc(S_LENGTH * sizeof(unsigned char));

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
	
	while (state != STOP) {
	
		if (alarmFlag) {
			alarmFlag = FALSE;
			state = STOP;
			rv = -1;
		}
		
		switch (state) 
		{
		case START:

			LOG("[STATE] entering START state...");
			read(fd, &frame[0], sizeof(unsigned char));

			if (frame[0] == original[0]) {
				LOG("[READ] received FLAG");
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received invalid symbol, returning to START...");
			}

			break;

		case FLAG_RCV:
			
			LOG("[STATE] entering FLAG_RCV state...");
			read(fd, &frame[1], sizeof(unsigned char));

			if (frame[1] == original[1]) {
				LOG("[READ] received ADDRESS");
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received invalid symbol, returning to START...");
				state = START;
			}

			break;
		
		case A_RCV:

			LOG("[STATE] entering A_RCV state...");
			read(fd, &frame[2], sizeof(unsigned char));

			if (frame[2] == original[2]) {
				LOG("[READ] received COMMAND");
				state = C_RCV;
			}
			else if(frame[2] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received invalid symbol, returning to START...");
				state = START;
			}

			break;
		
		case C_RCV:

			LOG("[STATE] entering C_RCV state...");
			read(fd, &frame[3], sizeof(unsigned char));

			if (frame[3] == original[3]) {
				LOG("[READ] received correct BCC checksum!");
				state = BCC_OK;
			}
			else if (frame[3] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received wrong BCC checksum, returning to START...");
				state = START;
			}

			break;
		
		case BCC_OK:

			LOG("[STATE] entering BCC_OK state...");
			read(fd, &frame[4], sizeof(unsigned char));

			if (frame[4] == original[4]) {
				LOG("[READ] received FLAG");
				state = STOP;
			}
			else {
				LOG("[READ] received invalid symbol, returning to START...");
				state = START;
			}

			break;
		}
	}
	
	if (rv == 0) {
		ll->numReceived++;
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

int receiveData(int fd, unsigned char* buffer) {

	unsigned char data[255];

	int nBytes = 0;
	int readSuccessful = FALSE;

	while (read(fd, &data[nBytes], sizeof(unsigned char)) > 0) {

		if (alarmFlag) {
			alarmFlag = FALSE;
			return -1;
		}

		if (data[nBytes++] == FLAG) {
			readSuccessful = TRUE;
			break;
		}
	}

	ll->numReceived++;

	if (!readSuccessful) {
		puts("[ERROR] receiveData(): unexpected end of frame...");
		return -1;	
	}

	if (data[0] != FLAG) {
		puts("[ERROR] receiveData(): invalid symbol, expected FLAG...");
		return -1;
	}

	if (nBytes < 7) {
		puts("[ERROR] receiveData(): received incomplete data packet, must be at least 7 bytes long.");
		return -1;
	}

	if (data[1] != A_SET) {
		puts("[ERROR] receiveData(): invalid symbol, expected A_SET...");
		return -1;	
	}

	int ns = (data[2] & (1 << 5)) >> 5;
	int j;
	int i = 0;
	int k;

	if (ns != ll->ns) {
		puts("[ERROR] receiveData(): received wrong or duplicated message, ignoring...");
		return -1;
	}

	if (data[3] != (A_SET ^ (ns << 5))) {
		puts("[[ERROR] receiveData(): wrong BCC checksum!");
		return -1;	
	}

	if (data[4] == FLAG) {
		puts("[ERROR] receiveData(): received FLAG, expected sequence of DATA octets...");
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
		puts("[ERROR] receiveData(): wrong BCC2 checksum, requesting retransmission...");
		sendREJ(fd, RECEIVER, ll->ns);
		return -1;
	}

	if (data[j] != FLAG) {
		puts("[ERROR] receiveData(): invalid symbol, expected FLAG...");
		return -1;
	}

/*	for (k = 0; k < i; k++) {
		printf("[LLREAD] received data: 0x%x (%c)\n", buffer[k], (char)(buffer[k]));	
	}
*/
	ll->ns = !ns;

	return nBytes;
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
int llread(int fd, unsigned char* buffer) {

	int readSuccessful = FALSE;

	while (!readSuccessful) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		if (receiveData(fd, buffer) > 0) {
			readSuccessful = TRUE;
		}
	}

	alarm_stop();

	if (!readSuccessful) {
		// connection timed out
		return -1;
	}

	int nBytes = sendRR(fd, RECEIVER, ll->ns);

	if (nBytes == S_LENGTH) {
		ll->numSentRR++;
		LOG_FORMAT("[LLREAD] sent RR response to TRANSMITTER, %d bytes written...\n", nBytes);
	}
	else {
		puts("[LLREAD] connection failed: error sending RR response to TRANSMITTER!");
		return -1;
	}

	return 0;
}

int llwrite(int fd, unsigned char* buffer, int length) {

	unsigned char I[MAX_SIZE];
	unsigned char BCC2 = 0; 
	int i = 4;
	int j = 0;

	I[INDEX_FLAG_START] = FLAG;
	I[INDEX_A] = A_SET;
	I[INDEX_C] = ll->ns << 5; 
	I[INDEX_BCC1] = I[1] ^ I[2];

	for (j = 0; j < length; j++) {

		unsigned char BYTE = buffer[j];

		BCC2 ^= BYTE;

		if (BYTE == FLAG || BYTE == ESCAPE) {
			I[i++] = ESCAPE;
			I[i++] = BYTE ^ BYTE_XOR;
		}
		else {
			I[i++] = BYTE;
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

	int writeSuccessful = FALSE;
	unsigned char* RR = generateRR(RECEIVER, !ll->ns);

	while (!writeSuccessful) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		LOG_FORMAT("[LLWRITE] sending information frame (attempt %d/%d...\n", alarmCounter + 1, ll->connectionTries + 1);
		
		if (sendFrame(fd, I, i) == i) {
			LOG_FORMAT("[LLWRITE] sent INFORMATION frame to RECEIVER, %d bytes written...\n", i);
		}
		else {
			ERROR("[LLWRITE] connection failed: error sending INFORMATION frame to RECEIVER!\n");
			alarm_stop();
			return -1;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLWRITE] waiting for RR response from RECEIVER (attempt %d/%d)...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, RR) == 0) {
			writeSuccessful = TRUE;
			ll->numReceivedRR++;
		}
	}

	alarm_stop();

	if (!writeSuccessful) {
		ERROR("[LLWRITE] connection failed: no response from RECEIVER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}

	return 0;
}

static int llopen_RECEIVER(int fd) {

	unsigned char* SET = generateSET(TRANSMITTER);
	int connected = FALSE;

	while (!connected) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLOPEN] waiting for SET command from TRANSMITTER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, SET) == 0) {
			connected = TRUE;
		}
	}

	alarm_stop();

	if (!connected) {
		printf("[LLOPEN] disconnected: no response from TRANSMITTER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}
	
	int nBytes = sendUA(fd, RECEIVER);

	if (nBytes == S_LENGTH) {
		LOG_FORMAT("[LLOPEN] sent UA response to TRANSMITTER, %d bytes written...\n", nBytes);
	}
	else {
		puts("[LLOPEN] disconnected: error sending UA response to TRANSMITTER!");
		return -1;
	}

	return 0;
}

static int llopen_TRANSMITTER(int fd) {
	
	unsigned char* UA = generateUA(RECEIVER);
	int connected = FALSE;
	
	while (!connected) {	
		
		if (alarmCounter > ll->connectionTries) {			
			break;
		}

		int nBytes = sendSET(fd, TRANSMITTER);
	
		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLOPEN] sent SET command to RECEIVER, %d bytes written...\n", nBytes);
		}
		else {
			ERROR("[LLOPEN] disconnected: error sending SET command to RECEIVER!\n");
			return -1;
		}
		
		if (alarmCounter == 0) {		
			alarm_start();
		}

		LOG_FORMAT("[LLOPEN] waiting for UA response from RECEIVER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1); 

		if (receiveCommand(fd, UA) == 0) {		
			connected = TRUE;
		}
	}

	alarm_stop();

	if (!connected) {
		ERROR("[LLOPEN] disconnected: no response from RECEIVER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}
	
	return 0;
}

int llopen(char* port, int mode) {

	int rv = -1;
	int fd = open(port, O_RDWR | O_NOCTTY);
	
	if (fd < 0) {
		perror(port); 
		return -1;
	}
	
	ll->fd = fd;	
	initializeTermios(fd);

	if (mode == MODE_TRANSMITTER) {
		rv = llopen_TRANSMITTER(fd);
	}
	else if (mode == MODE_RECEIVER) {
		rv = llopen_RECEIVER(fd);
	} 

	if (rv < 0) {
		return -1;
	}
	
	puts("[INFORMATION] connection established sucessfully!");
			
	return fd;
}

static int llclose_TRANSMITTER(int fd) {

	unsigned char* DISC = generateDISC(RECEIVER);
	int discReceived = FALSE;
	int nBytes = 0;
		
	while (!discReceived) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		nBytes = sendDISC(fd, TRANSMITTER);

		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLCLOSE] sent DISC command to RECEIVER, %d bytes written...\n", nBytes);
		} 
		else {
			ERROR("[LLCLOSE] disconnected: error sending DISC command to RECEIVER!\n");
			alarm_stop();
			return -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLCLOSE] waiting for DISC response from RECEIVER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, DISC) == 0) {
			discReceived = TRUE;
		}
	}

	alarm_stop();

	if (!discReceived) {
		ERROR("[LLCLOSE] disconnected: no response from RECEIVER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}

	nBytes = sendUA(fd, TRANSMITTER);

	if (nBytes == S_LENGTH) {
		LOG_FORMAT("[LLCLOSE] sent UA response to RECEIVER, %d bytes written...\n", nBytes);
	}
	else {
		ERROR("[LLCLOSE] disconnected: error sending UA response to RECEIVER!\n");
		return -1;
	}
	
	return 0;
}

static int llclose_RECEIVER(int fd) {

	unsigned char* DISC = generateDISC(TRANSMITTER);
	unsigned char* UA = generateUA(TRANSMITTER);
	int discReceived = FALSE;
	int uaReceived = FALSE;

	while (!discReceived) {
		
		if (alarmCounter > ll->connectionTries) {
			break;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}
			
		LOG("[READ] waiting for DISC response from TRANSMITTER (attempt %d/%d)...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, DISC) == 0) {
			discReceived = TRUE;
		}
	}
	
	alarm_stop();
	
	if (!discReceived) {
		ERROR("[LLCLOSE] disconnected: no response from TRANSMITTER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}

	while (!uaReceived) {
	
		if (alarmCounter > ll->connectionTries) {
			break;
		}

		int nBytes = sendDISC(fd, RECEIVER);

		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLCLOSE] sent DISC command to TRANSMITTER, %d bytes written...\n", nBytes);
		} 
		else {
			ERROR("[LLCLOSE] disconnected: error sending DISC command to TRANSMITTER!\n");
			alarm_stop();
			return -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}
		
		LOG_FORMAT("[LLCLOSE] waiting for UA response from TRANSMITTER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);
	
		if (receiveCommand(fd, UA) == 0) {
			uaReceived = TRUE;
		}
	}

	alarm_stop();
	
	if (!uaReceived) {
		ERROR("[LLCLOSE] disconnected: no answer from TRANSMITTER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}
	
	return 0;
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
		puts("*** Connection terminated. ***");
		resetTermios(fd);
		close(fd);
	}
	
	return rv;
}

int link_initialize(char* port, int mode, int baudrate, int retries, int timeout) {

	ll = (LinkLayer*) malloc(sizeof(LinkLayer));
	strcpy(ll->port, port);
	ll->ns = 0;
	ll->connectionBaudrate = baudrate;
	ll->connectionMode = mode;
	ll->connectionTimeout = timeout;
	ll->connectionTries = retries;
	ll->numSent = 0;
	ll->numSentRR = 0;
	ll->numSentREJ = 0;
	ll->numReceived = 0;
	ll->numReceivedRR = 0;
	ll->numReceivedREJ = 0;
	ll->numTimeouts = 0;
	ll->fd = llopen(port, mode);
	alarm_set(timeout);
	
	return ll->fd;
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

void logConnection(void) {

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

void logStatistics(void) {
	puts("\n=============================");
	puts("=   CONNECTION STATISTICS   =");
	puts("=============================");
	printf("# Sent messages:\t%d\n", ll->numSent);
	printf("# Received messages:\t%d\n#\n", ll->numReceived);
	printf("# Time-outs:\t%d\n#\n", ll->numTimeouts);
	printf("# Sent RR:\t%d\n", ll->numSentRR);
	printf("# Received RR:\t%d\n#\n", ll->numReceivedRR);
	printf("# Sent REJ:\t%d\n", ll->numSentREJ);
	printf("# Received REJ:\t%d\n", ll->numReceivedREJ);
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