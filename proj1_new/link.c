#include "link.h"

LinkLayer* ll;

int link_initialize(const char* port, int fd, int mode) {

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

void printConnectionInfo() {
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

void printStatistics() {
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

int setTERMIOS() {

	bzero(&ll->newtio, sizeof(ll->newtio));
   
    ll->newtio.c_cflag = ll->connectionBaudrate | CS8 | CLOCAL | CREAD;
    ll->newtio.c_iflag = IGNPAR;
    ll->newtio.c_oflag = 0;
    ll->newtio.c_lflag = 0;
    ll->newtio.c_cc[VTIME] = 0;		// inter-character timer unused
    ll->newtio.c_cc[VMIN] = 1;		// blocking read until 5 chars received

    if (tcflush(ll->fd, TCIFLUSH) < 0) {
    	perror("tcsetattr");
    	return -1;
    }

	if (tcsetattr(ll->fd, TCSANOW, &ll->newtio) <  0) {
		perror("tcsetattr");
    	return -1;
    }

    return 0;
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

unsigned char* generateSET(int source) {
	return generateCommand(source, C_SET);
}

unsigned char* generateDISC(int source) {
	return generateCommand(source, C_DIS);
}

int sendSET(int source) {
	return sendFrame(generateSET(source), 5); 
}

int sendDISC(int source) {
	return sendFrame(generateDISC(source), 5); 
}

/*
*
*
*/
int sendFrame(unsigned char* buffer, unsigned buffer_sz) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < buffer_sz; i++) {
		if (write(ll->fd, &buffer[i], sizeof(unsigned char)) == 1) {
			printf("[OUT] sending packet: 0x%x\n", buffer[i]);
			bytesWritten++;
		}
	}
	
	if (bytesWritten == buffer_sz) {
		ll->numFramesSent++;
	}
	
	return bytesWritten;
}

void printCommand(unsigned char* buf) {
	puts("-------------------------");
	printf("- FLAG: 0x%02x\t\t-\n", buf[0]);
	printf("- A: 0x%02x\t\t-\n", buf[1]);
	printf("- C: 0x%02x\t\t-\n", buf[2]);
	printf("- BCC: 0x%02x = 0x%02x\t-\n", buf[3], buf[1] ^ buf[2]);
	printf("- FLAG: 0x%02x\t\t-\n", buf[4]);
	puts("-------------------------");
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

unsigned char* generateUA(int source) {
	return generateResponse(source, C_UA);
}

unsigned char* generateRR(int source, int nr) {
	return generateResponse(source, (nr << 5) | 0x01);
}

unsigned char* generateREJ(int source, int nr) {
	return generateResponse(source, (nr << 5) | 0x01);
}

int sendUA(int source) {
	return sendFrame(generateUA(source), 5); 
}

int sendRR(int source, int nr) {
	ll->numSentRR++;
	return sendFrame(generateRR(source, nr), 5);
}

int sendREJ(int source, int nr) {
	ll->numSentREJ++;
	return sendFrame(generateREJ(source, nr), 5);
}
