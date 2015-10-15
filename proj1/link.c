#include "link.h"

int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz) {
	
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

unsigned char* generateSET(int source) {
	return generateCommand(source, C_SET);
}

unsigned char* generateDISC(int source) {
	return generateCommand(source, C_DIS);
}

int sendSET(int fd, int source) {
	return sendFrame(fd, generateSET(source), S_LENGTH); 
}

int sendDISC(int fd, int source) {
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

unsigned char* generateUA(int source) {
	return generateResponse(source, C_UA);
}

unsigned char* generateRR(int source, int nr) {
	return generateResponse(source, (nr << 5) | 0x01);
}

unsigned char* generateREJ(int source, int nr) {
	return generateResponse(source, (nr << 5) | 0x01);
}

int sendUA(int fd, int source) {
	return sendFrame(fd, generateUA(source), 5);
}

int sendRR(int fd, int source, int nr) {
	return sendFrame(fd, generateRR(source, nr), 5);
}

int sendREJ(int fd, int source, int nr) {
	return sendFrame(fd, generateREJ(source, nr), 5);
}
