#include "alarm.h"
#include "link.h"

LinkLayer* ll = NULL;

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
 * FRAME INDEX
 */
#define INDEX_FLAG_START	0
#define INDEX_A			1
#define INDEX_C 		2
#define INDEX_BCC1		3
#define INDEX_FLAG_END		4

/**
 * FRAME BITS
 */
#define A_SET		0x03
#define A_UA		0x01
#define C_DIS		0x0b
#define C_SET		0x07
#define C_REJ		0x05
#define C_RR		0x01
#define C_UA		0x03

/**
 * DEBUG DEFINITIONS
 */
#define LINK_DEBUG		0
#define ERROR(...)		fprintf(stderr, __VA_ARGS__)
#define LOG(msg)		if (LINK_DEBUG) puts(msg)
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

	if (source == TRANSMITTER) {
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

	if (source == TRANSMITTER) {
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
	ll->numSentRR++;
	return sendFrame(fd, generateRR(source, nr), 5);
}

static int sendREJ(int fd, int source, int nr) {
	ll->numSentREJ++;
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
static unsigned char receiveCommand(int fd, unsigned char* original, int checkCommand) {

	int state = START;
	unsigned char frame[S_LENGTH];
	unsigned char rv = 0;

	while (state != STOP) {

		switch (state)
		{
		case START:

			LOG("[STATE] entering START state...");

			if (read(fd, &frame[0], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = STOP;
			}
			else if (frame[0] == original[0]) {
				LOG("[READ] received FLAG");
				state = FLAG_RCV;
			}

			break;

		case FLAG_RCV:

			LOG("[STATE] entering FLAG_RCV state...");

			if (read(fd, &frame[1], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = STOP;
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
				state = START;
			}

			break;

		case A_RCV:

			LOG("[STATE] entering A_RCV state...");

			if (read(fd, &frame[2], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = STOP;
			}
			else if(frame[2] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				state = FLAG_RCV;
			}
			else if (checkCommand && frame[2] != original[2]) {
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
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = STOP;
			}
			else if (frame[3] == original[3]) {
				LOG("[READ] received correct BCC checksum!");
				state = BCC_OK;
			}
			else if (frame[3] == FLAG) {
				LOG("[READ] received FLAG, returning to FLAG_RCV...");
				ll->numBCC1Errors++;
				state = FLAG_RCV;
			}
			else {
				LOG("[READ] received wrong BCC checksum, returning to START...");
				ll->numBCC1Errors++;
				state = START;
			}

			break;

		case BCC_OK:

			LOG("[STATE] entering BCC_OK state...");

			if (read(fd, &frame[4], sizeof(unsigned char)) < 0) {
				LOG("[READ] attempted to read, but serial port buffer is empty...");
				state = STOP;
			}
			else if (frame[4] == original[4]) {
				LOG("[READ] received FLAG");
				state = STOP;
			}
			else {
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
	ll->newtio.c_cflag = getBaudrate(ll->connectionBaudrate) | CS8 | CLOCAL | CREAD;
	ll->newtio.c_iflag = IGNPAR;
	ll->newtio.c_oflag = 0;
	ll->newtio.c_lflag = 0;
	ll->newtio.c_cc[VTIME] = 0;
	ll->newtio.c_cc[VMIN] = 1;

	if (tcflush(fd, TCIFLUSH) < 0) {
		perror("tcflush");
		return -1;
	}

	if (tcsetattr(fd, TCSANOW, &ll->newtio) < 0) {
		perror("tcsetattr");
		return -1;
	}

	return 0;
}

static int resetTermios(int fd) {

	if (tcsetattr(fd, TCSANOW, &ll->oldtio) < 0) {
		perror("tcsetattr");
		return -1;
	}

	return 0;
}

static int receiveData(int fd, unsigned char* buffer, int resend) {

	int nBytes = 0;
	int readSuccessful = FALSE;
	int state = START;
	int ns = -1;
	unsigned char frame[MAX_SIZE];

	while (state != STOP) {

		if (alarmCounter > ll->connectionTries) {
			return -1;
		}

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		switch (state)
		{
		case START:

			LOG("[STATE] entering START state...");

			if (read(fd, &frame[0], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
			}
			else if (frame[0] ==  FLAG) {
				nBytes++;
				state = FLAG_RCV;
			}

			break;

		case FLAG_RCV:

			LOG("[STATE] entering FLAG_RCV state...");

			if (read(fd, &frame[1], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = START;
			}
			else if (frame[1] == A_SET) {
				nBytes++;
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				state = FLAG_RCV;
			}
			else {
				state = START;
			}

			break;

		case A_RCV:

			LOG("[STATE] entering A_RCV state...");

			if (read(fd, &frame[2], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = START;
			}
			else if(frame[2] == FLAG) {
				state = FLAG_RCV;
			}
			else {
				ns = (frame[2] & (1 << 5)) >> 5;
				state = C_RCV;
				nBytes++;
			}

			break;

		case C_RCV:

			LOG("[STATE] entering C_RCV state...");

			if (read(fd, &frame[3], sizeof(unsigned char)) < 0) {
				ERROR("[READ] attempted to read, but serial port buffer was empty...\n");
				state = START;
			}
			else if (frame[3] == (A_SET ^ (ns << 5)))  {
				state = STOP;
				nBytes++;
			}
			else if (frame[3] == FLAG) {
				state = FLAG_RCV;
				ll->numBCC1Errors++;
			}
			else {
				state = START;
				ll->numBCC1Errors++;
			}

			break;
		}
	}

	while (read(fd, &frame[nBytes], sizeof(unsigned char)) > 0) {
		if (frame[nBytes++] == FLAG) {
			readSuccessful = TRUE;
			break;
		}
	}

	if (!readSuccessful) {
		ERROR("ERROR: receiveData(): unexpected FLAG found before EOF\n");
		return -1;
	}

	if (nBytes < 7) {
		ERROR("[ERROR] receiveData(): received incomplete INFORMATION frame\n");
		return -1;
	}

	if (ns == ll->ns) {
		LOG("[ERROR] receiveData(): received duplicated message, ignoring...");
	}

	unsigned char expectedBCC2 = 0;
	unsigned char receivedBCC2 = 0;
	int i = 0;
	int j;

	for (j = 4; j < nBytes - 2; i++, j++) {

		unsigned char byte = frame[j];
		unsigned char destuffed;

		if (byte == ESCAPE) {

			if (j == nBytes - 3) {
				break;
			}

			destuffed = frame[j + 1] ^ BYTE_XOR;
			buffer[i] = destuffed;
			expectedBCC2 ^= destuffed;
			j++;
		}
		else {
			buffer[i] = byte;
			expectedBCC2 ^= byte;
		}
	}

	if (frame[j] == ESCAPE) {
		receivedBCC2 = frame[++j] ^ BYTE_XOR;
	} else {
		receivedBCC2 = frame[j];
	}

	if (expectedBCC2 != receivedBCC2) {
		ERROR("[ERROR] receiveData(): frame has wrong BCC2 checksum, requesting retransmission...\n");
		LOG_FORMAT("[LLREAD] sent REJ response to TRANSMITTER, %d bytes written\n", sendREJ(fd, RECEIVER, ns));
		ll->numBCC2Errors++;
		return 0;
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
	int rejSent = FALSE;
	int readSuccessful = FALSE;

	while (!readSuccessful) {

		if (alarmCounter == 0) {
			alarm_start();
		}

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		nBytes = receiveData(fd, buffer, rejSent);

		if (nBytes == 0) {
			rejSent = TRUE;
			alarm_stop();
		}
		else if (nBytes > 0) {

			readSuccessful = TRUE;
			ll->numReceived++;
			nBytes = sendRR(fd, RECEIVER, ll->ns);

			if (nBytes == S_LENGTH) {
				ll->ns = !ll->ns;
				LOG_FORMAT("[LLREAD] sent RR response to TRANSMITTER, %d bytes written...\n", nBytes);
			}
			else {
				ERROR("[LLREAD] connection problem: error sending RR response to TRANSMITTER!\n");
				alarm_stop();
				return -1;
			}
		}
	}

	alarm_stop();

	if (!readSuccessful) {
		ERROR("[LLREAD] connection problem: no response from TRANSMITTER after %d attempts\n", ll->connectionTries + 1);
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

	if (BCC2 == FLAG || BCC2 == ESCAPE) {
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

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLWRITE] sending information frame (%d)...\n", alarmCounter + 1);

		if (sendFrame(fd, I, i) == i) {
			LOG_FORMAT("[LLWRITE] sent INFORMATION frame to RECEIVER, %d bytes written...\n", i);
		}
		else {
			ERROR("[LLWRITE] connection problem: error sending INFORMATION frame to RECEIVER!\n");
			alarm_stop();
			return -1;
		}

		LOG_FORMAT("[LLWRITE] waiting for response from RECEIVER (%d)...\n", alarmCounter + 1);

		unsigned char C = receiveCommand(fd, RR, FALSE);

		if (checkCommandRR(C, !ll->ns)) {
			writeSuccessful = TRUE;
			ll->ns = !ll->ns;
			ll->numReceivedRR++;
		}
		else if (checkCommandREJ(C, ll->ns)) {
			ll->numReceivedREJ++;
			alarm_stop();
		}
	}

	alarm_stop();

	if (!writeSuccessful) {
		ERROR("[LLWRITE] connection problem: no response from RECEIVER after %d attempts\n", ll->connectionTries + 1);
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

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLOPEN] waiting for SET command from TRANSMITTER (%d)...\n", alarmCounter + 1);

		if (receiveCommand(fd, SET, TRUE)) {
			connected = TRUE;
		}
	}

	alarm_stop();

	if (!connected) {
		ERROR("[LLOPEN] disconnected: no response from TRANSMITTER after %d attempts.\n", ll->connectionTries + 1);
		return -1;
	}

	int nBytes = sendUA(fd, RECEIVER);

	if (nBytes == S_LENGTH) {
		LOG_FORMAT("[LLOPEN] sent UA response to TRANSMITTER, %d bytes written...\n", nBytes);
	}
	else {
		ERROR("[LLOPEN] connection problem: error sending UA response to TRANSMITTER!\n");
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

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		int nBytes = sendSET(fd, TRANSMITTER);

		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLOPEN] sent SET command to RECEIVER, %d bytes written...\n", nBytes);
		}
		else {
			ERROR("[LLOPEN] connection problem: error sending SET command to RECEIVER!\n");
			alarm_stop();
			return -1;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLOPEN] waiting for UA response from RECEIVER (%d)...\n", alarmCounter + 1);

		if (receiveCommand(fd, UA, TRUE)) {
			connected = TRUE;
		}
	}

	alarm_stop();

	if (!connected) {
		ERROR("[LLOPEN] connection problem: no response from RECEIVER after %d attempts.\n", ll->connectionTries + 1);
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

	initializeTermios(fd);

	if (mode == TRANSMITTER) {
		rv = llopen_TRANSMITTER(fd);
	}
	else if (mode == RECEIVER) {
		rv = llopen_RECEIVER(fd);
	}

	if (rv < 0) {
		return -1;
	}

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

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		nBytes = sendDISC(fd, TRANSMITTER);

		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLCLOSE] sent DISC command to RECEIVER, %d bytes written...\n", nBytes);
		}
		else {
			ERROR("[LLCLOSE] connection problem: error sending DISC command to RECEIVER!\n");
			alarm_stop();
			return -1;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLCLOSE] waiting for DISC response from RECEIVER (%d)...\n", alarmCounter + 1);

		if (receiveCommand(fd, DISC, TRUE)) {
			discReceived = TRUE;
		}
	}

	alarm_stop();

	if (!discReceived) {
		ERROR("[LLCLOSE] connection problem: no response from RECEIVER after %d attempts.\n", ll->connectionTries + 1);
		return -1;
	}

	nBytes = sendUA(fd, TRANSMITTER);

	if (nBytes == S_LENGTH) {
		LOG_FORMAT("[LLCLOSE] sent UA response to RECEIVER, %d bytes written...\n", nBytes);
	}
	else {
		ERROR("[LLCLOSE] connection problem: error sending UA response to RECEIVER!\n");
		return -1;
	}

	sleep(1);

	return 0;
}

static int llclose_RECEIVER(int fd) {

	unsigned char* DISC = generateDISC(TRANSMITTER);
	unsigned char* UA = generateUA(TRANSMITTER);
	int discReceived = FALSE;
	int uaReceived = FALSE;
	int nBytes;

	while (!discReceived) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLCLOSE] waiting for DISC response from TRANSMITTER (%d)...\n", alarmCounter + 1);

		if (receiveCommand(fd, DISC, TRUE)) {
			discReceived = TRUE;
		}
	}

	alarm_stop();

	if (!discReceived) {
		ERROR("[LLCLOSE] connection problem: no response from TRANSMITTER after %d attempts.\n", ll->connectionTries + 1);
		return -1;
	}

	while (!uaReceived) {

		if (alarmCounter > ll->connectionTries) {
			break;
		}

		if (alarmFlag) {
			alarmFlag = FALSE;
			ll->numTimeouts++;
		}

		nBytes = sendDISC(fd, RECEIVER);

		if (nBytes == S_LENGTH) {
			LOG_FORMAT("[LLCLOSE] sent DISC command to TRANSMITTER, %d bytes written...\n", nBytes);
		}
		else {
			ERROR("[LLCLOSE] connection problem: error sending DISC command to TRANSMITTER!\n");
			alarm_stop();
			return -1;
		}

		if (alarmCounter == 0) {
			alarm_start();
		}

		LOG_FORMAT("[LLCLOSE] waiting for UA response from TRANSMITTER (%d)...\n", alarmCounter + 1);

		if (receiveCommand(fd, UA, TRUE)) {
			uaReceived = TRUE;
		}
	}

	alarm_stop();

	if (!uaReceived) {
		ERROR("[LLCLOSE] connection problem: no answer from TRANSMITTER after %d attempts.\n", ll->connectionTries + 1);
		return -1;
	}

	return 0;
}

int llclose(int fd) {

	int rv = 0;

	if (ll->connectionMode == TRANSMITTER) {
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
	ll->messageSize = maxsize;
	ll->numSent = 0;
	ll->numSentRR = 0;
	ll->numSentREJ = 0;
	ll->numReceived = 0;
	ll->numReceivedRR = 0;
	ll->numReceivedREJ = 0;
	ll->numBCC1Errors = 0;
	ll->numBCC2Errors = 0;
	ll->numTimeouts = 0;
	alarmTimeout = timeout;

	return ll;
}

int getBaudrate(int baudrate) {

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
	case TRANSMITTER:
		puts("| Mode:\t\t\t: TRANSMITTER\t|");
		break;
	case RECEIVER:
		puts("| Mode:\t\t\t: RECEIVER\t|");
		break;
	default:
		puts("| Mode:\t\t\t: UNKNOWN\t|");
		break;
	}

	printf("| Baudrate\t\t: %d\t\t|\n", ll->connectionBaudrate);
	printf("| Maximum Message Size\t: %d\t\t|\n", ll->messageSize);
	printf("| Number Retries\t: %d\t\t|\n", ll->connectionTries);
	printf("| Timeout Interval\t: %d\t\t|\n", ll->connectionTimeout);
	printf("| Serial Port\t\t: %s\t|\n", ll->port);
	puts("+=======================================+\n");
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
	printf("| BCC1 Errors\t\t: %d\t\t|\n", ll->numBCC1Errors);
	printf("| BCC2 Errors\t\t: %d\t\t|\n", ll->numBCC2Errors);
	puts("+=======================================+\n");
}
