#include "link.h"
#include "alarm.h"

LinkLayer* ll;

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
#define LINK_DEBUG			0
#define ERROR(...)			fprintf(stderr, __VA_ARGS__)
#define LOG(msg)			if (LINK_DEBUG) puts(msg)
#define LOG_FORMAT(...)		if (LINK_DEBUG) printf(__VA_ARGS__)

static int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz) {
	return write(fd, buffer, buffer_sz);
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

static int checkCommandRR(unsigned char C, int ns) {
	return C == ((ns << 5) | C_RR);
}

static int checkCommandREJ(unsigned char C, int ns) {
	return C == ((ns << 5) | C_REJ);
}

/*
 * ----------------------------- 
 *	MESSAGE RECEIVE STATE MACHINE
 * -----------------------------
 */
static unsigned char receiveCommand(int fd, unsigned char* original, int compareC) {

	int state = START;
	unsigned char frame[S_LENGTH];
	unsigned char rv = 0;

	while (state != STOP) {
	
		if (alarmFlag) {
			alarmFlag = FALSE;
			state = STOP;
			rv = 0;
			ll->numTimeouts++;
		}
		
		switch (state) 
		{
		case START:

			LOG("[STATE] entering START state...");
			
			if (read(fd, &frame[0], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer is empty...");
			}
			else if (frame[0] == original[0]) {
				LOG("[READ] received FLAG");
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received invalid symbol, returning to START...");
			}

			break;

		case FLAG_RCV:
			
			LOG("[STATE] entering FLAG_RCV state...");
			
			if (read(fd, &frame[1], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer is empty...");
			}
			else if (frame[1] == original[1]) {
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

			if (read(fd, &frame[2], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer was empty...");
			}
			else if(frame[2] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else if (compareC && frame[2] != original[2]) {
				LOG("[READ] received invalid symbol, returning to START...");
				state = START;
			}
			else {
				rv = frame[2];
				state = C_RCV;
			}

			break;
		
		case C_RCV:

			LOG("[STATE] entering C_RCV state...");
			
			if (read(fd, &frame[3], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer was empty...");
			}
			else if (frame[3] == original[3]) {
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
			
			if (read(fd, &frame[4], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer is empty...");
			}
			else if (frame[4] == original[4]) {
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
	
	return rv;
}

static int initializeTermios(int fd) {

	if (tcgetattr(fd, &ll->oldtio) < 0) {
		perror("tcgetattr");
		return -1;
	}
	
	int BAUDRATE = link_getBaudrate(ll->connectionBaudrate);
	
	bzero(&ll->newtio, sizeof(ll->newtio));
	ll->newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
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

	int nBytes = 1;
	int readSuccessful = FALSE;
	
	if (read(fd, &data[0], sizeof(unsigned char)) < 0) {
		return -1;
	}

	if (data[0] != FLAG) {
		LOG("[READ] received invalid symbol, expected FLAG...");
		return -1;
	}

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
	
	if (!readSuccessful) {
		puts("[READ] receive failed: unexpected end of frame");
		return -1;	
	}

	ll->numReceived++;

	if (data[0] != FLAG) {
		LOG("[ERROR] receiveData(): invalid symbol, expected FLAG...");
		return -1;
	}

	if (nBytes < 7) {
		puts("[ERROR] receiveData(): received incomplete data packet, must be at least 7 bytes long.");
		return -1;
	}

	if (data[1] != A_SET) {
		LOG("[ERROR] receiveData(): invalid symbol, expected A_SET...");
		return -1;	
	}

	int ns = (data[2] & (1 << 5)) >> 5;
	int j;
	int i = 0;

	if (ns == ll->ns) {
		puts("[ERROR] receiveData(): received wrong or duplicated message, ignoring...");
		return -1;
	}

	if (data[3] != (A_SET ^ (ns << 5))) {
		LOG("[[ERROR] receiveData(): wrong BCC checksum!");
		return -1;	
	}

	if (data[4] == FLAG) {
		LOG("[ERROR] receiveData(): received FLAG, expected sequence of DATA octets...");
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

	if (data[j] == ESCAPE) {
		receivedBCC2 = data[++j] ^ BYTE_XOR;
	} else {
		receivedBCC2 = data[j];
	}
	
	if (expectedBCC2 != receivedBCC2) {
		puts("[ERROR] receiveData(): wrong BCC2 checksum, requesting retransmission...");
		sendREJ(fd, RECEIVER, !ll->ns);
		ll->numSentREJ++;
		return -1;
	}

	if (data[++j] != FLAG) {
		LOG("[ERROR] receiveData(): invalid symbol, expected FLAG...");
		return -1;
	}

	return nBytes;
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
int llread(int fd, unsigned char* buffer) {

	int nBytes;
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
		ERROR("[LLREAD] connection failed: no response from TRANSMITTER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}
	
	ll->numReceived++;
	nBytes = sendRR(fd, RECEIVER, ll->ns);

	if (nBytes == S_LENGTH) {
		ll->numSentRR++;
		ll->ns = !ll->ns;
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

		unsigned char C = receiveCommand(fd, RR, FALSE);
		
		if (checkCommandRR(C, !ll->ns)) {
			writeSuccessful = TRUE;
			ll->ns = !ll->ns;
			ll->numReceivedRR++;
		}
		else if (checkCommandREJ(C, !ll->ns)) {
			ll->numReceivedREJ++;
		}
	}

	alarm_stop();

	if (!writeSuccessful) {
		ERROR("[LLWRITE] connection failed: no response from RECEIVER after %d attempts...\n", ll->connectionTries + 1);
		return -1;
	}
	
	ll->numSent++;

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

		printf("[LLOPEN] waiting for SET command from TRANSMITTER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, SET, TRUE)) {
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
		printf("[LLOPEN] sent UA response to TRANSMITTER, %d bytes written...\n", nBytes);
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
			printf("[LLOPEN] sent SET command to RECEIVER, %d bytes written...\n", nBytes);
		}
		else {
			ERROR("[LLOPEN] disconnected: error sending SET command to RECEIVER!\n");
			return -1;
		}
		
		if (alarmCounter == 0) {		
			alarm_start();
		}

		printf("[LLOPEN] waiting for UA response from RECEIVER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1); 

		if (receiveCommand(fd, UA, TRUE)) {		
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
			printf("[LLCLOSE] sent DISC command to RECEIVER, %d bytes written...\n", nBytes);
		} 
		else {
			ERROR("[LLCLOSE] disconnected: error sending DISC command to RECEIVER!\n");
			alarm_stop();
			return -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}

		printf("[LLCLOSE] waiting for DISC response from RECEIVER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, DISC, TRUE)) {
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
		printf("[LLCLOSE] sent UA response to RECEIVER, %d bytes written...\n", nBytes);
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
			
		printf("[READ] waiting for DISC response from TRANSMITTER (attempt %d/%d)...\n", alarmCounter + 1, ll->connectionTries + 1);

		if (receiveCommand(fd, DISC, TRUE)) {
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
			printf("[LLCLOSE] sent DISC command to TRANSMITTER, %d bytes written...\n", nBytes);
		} 
		else {
			ERROR("[LLCLOSE] disconnected: error sending DISC command to TRANSMITTER!\n");
			alarm_stop();
			return -1;
		}
		
		if (alarmCounter == 0) {
			alarm_start();
		}
		
		printf("[LLCLOSE] waiting for UA response from TRANSMITTER, attempt %d of %d...\n", alarmCounter + 1, ll->connectionTries + 1);
	
		if (receiveCommand(fd, UA, TRUE)) {
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

LinkLayer* llinit(char* port, int mode, int baudrate, int retries, int timeout, int maxsize) {

	ll = (LinkLayer*) malloc(sizeof(LinkLayer));	
	strcpy(ll->port, port);
	ll->ns = mode;
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
	alarm_set(ll->connectionTimeout);

	return ll;
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

	puts("+=======================================+");
	puts("|        CONNECTION INFORMATION         |");
	puts("+=======================================+");

	switch (ll->connectionMode) {
	case MODE_TRANSMITTER:
		puts("| Mode:\t\t\t: TRANSMITTER\t|");
		break;
	case MODE_RECEIVER:
		puts("| Mode:\t\t\t: RECEIVER\t|");
		break;
	default:
		puts("| Mode:\t\t\t: UNKNOWN\t|");
		break;
	}

	printf("| Baudrate\t\t: %d\t\t|\n", ll->connectionBaudrate);
	printf("| Maximum Message Size\t: %d\t\t|\n", ll->messageDataMaxSize);
	printf("| Number Retries\t: %d\t\t|\n", ll->connectionTries);
	printf("| Timeout Interval\t: %d\t\t|\n", ll->connectionTimeout);
	printf("| Serial Port\t\t: %s\t|\n", ll->port);
	puts("+=======================================+");
}

void logStatistics(void) {
	puts("+=======================================+");
	puts("|         CONNECTION STATISTICS         |");
	puts("+=======================================+");
	printf("| Sent Messages\t\t: %d\t\t|\n", ll->numSent);
	printf("| Received Messages\t: %d\t\t|\n", ll->numReceived);
	printf("| Connection Timeouts\t: %d\t\t|\n", ll->numTimeouts);
	printf("| Sent RR\t\t: %d\t\t|\n", ll->numSentRR);
	printf("| Received RR\t\t: %d\t\t|\n", ll->numReceivedRR);
	printf("| Sent REJ\t\t: %d\t\t|\n", ll->numSentREJ);
	printf("| Received REJ\t\t: %d\t\t|\n", ll->numReceivedREJ);
	puts("+=======================================+");
}

void logCommand(unsigned char* CMD) {
	puts("-------------------------");
	printf("- FLAG: 0x%02x\t\t-\n", CMD[0]);
	printf("- A: 0x%02x\t\t-\n", CMD[1]);
	printf("- C: 0x%02x\t\t-\n", CMD[2]);
	printf("- BCC: 0x%02x\t\t\n", CMD[3]);
	printf("- FLAG: 0x%02x\t\t-\n", CMD[4]);
	puts("-------------------------");
}
