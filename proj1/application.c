#include "application.h"
#include "link.h"

#define CTRL_PKG_START	1
#define CTRL_PKG_DATA	0
#define CTRL_PKG_END	2

#define PARAM_FILE_SIZE		0
#define PARAM_FILE_NAME		1

#define CTRL_C 		0
#define CTRL_T1		1
#define CTRL_L1		2
#define DATA_C		0
#define DATA_N		1
#define DATA_L2		0
#define DATA_L1		3
#define DATA_P		4
	
#define APPLICATION_DEBUG 	1
#define LOG(msg)			if (APPLICATION_DEBUG) puts(msg);

ApplicationLayer* al;

static unsigned calculate_size(FILE* file) {

	unsigned currentPosition = ftell(file);
	unsigned currentSize;

	if (fseek(file, 0, SEEK_END) < 0) {
		return -1;
	}

	currentSize = ftell(file);
	fseek(file, 0, currentPosition);

	return currentSize;
}

/**
 * DATA PACKAGES
 */

static int send_data(int fd, int N, const unsigned char* buffer, int length) {
	
	int rv = 0;
	unsigned packageSize = 4 + length;
	unsigned char* DP = (unsigned char*) malloc(packageSize * sizeof(unsigned char));

	DP[DATA_C] = CTRL_PKG_DATA;
	DP[DATA_N] = N;
	DP[DATA_L2] = length / 256;
	DP[DATA_L1] = length % 256;
	memcpy(&DP[DATA_P], buffer, length);

	if (llwrite(fd, DP, packageSize) < 0) {
		puts("[ERROR] DATA package not sent: serial port write failed...");
		rv = -1;
	}

	free(DP);

	return rv;
}

static int receive_data(int fd, int* N, unsigned char* buffer, int* length) {

	unsigned char* DP = NULL;
	unsigned packageSize = llread(fd, DP);
	int rv = 0;

	if (packageSize < 0) {
		puts("[ERROR] DATA package reception failed!");
		rv = -1;
	}
	else if (DP[DATA_C] != CTRL_PKG_DATA) {
		puts("[ERROR] received wrong DATA package: control field is not CTRL_PKG_DATA...");
		rv = -1;
	}
	else {
		*N = DP[DATA_N];
		*length = 256 * DP[DATA_L2] + DP[DATA_L1];
		buffer = malloc(*length * sizeof(unsigned char));
		memcpy(buffer, &DP[DATA_P], *length);
	}

	free(DP);

	return rv;
}

/**
 * CONTROL PACKAGES
 */

static int send_control(int fd, unsigned char C, const char* fileSize, const char* fileName) {

	int rv = 0;

	if (C == CTRL_PKG_START) {
		LOG("[INFORMATION] sending START control package to RECEIVER...");
	}
	else if (C == CTRL_PKG_END) {
		LOG("[INFORMATION] sending END control package to RECEIVER...");
	}
	else {
		LOG("[ERROR] sending UNKNOWN control package to RECEIVER...");
		return -1;
	}

	const unsigned sizeLength = strlen(fileSize);
	const unsigned nameLength = strlen(fileName);
	
	int i = 3;
	unsigned packageSize = 5 + sizeLength + nameLength;
	unsigned char* CP = (unsigned char*) malloc(packageSize * sizeof(unsigned char));

	CP[CTRL_C] = C;
	CP[CTRL_T1] = PARAM_FILE_SIZE;
	CP[CTRL_L1] = sizeLength;
	memcpy(&CP[i], fileSize, sizeLength);
	i += sizeLength;
	CP[i++] = PARAM_FILE_NAME;
	CP[i++] = nameLength;
	memcpy(&CP[i], fileName, nameLength);
	i += nameLength;

	if (llwrite(fd, CP, i) < 0)  {
		puts("[ERROR] CONTROL package not sent: serial port write failed...");
		rv = -1;
	}

	free(CP);

	if (rv) {	
		return -1;
	}

	if (C == CTRL_PKG_START) {
		puts("[INFORMATION] START control package successfully sent!");
	}
	else if (C == CTRL_PKG_END) {
		puts("[INFORMATION] END control package successfully sent!");
	}

	printf("[INFORMATION] fileSize=%s, fileName=%s\n", fileSize, fileName);

	return 0;
}

unsigned char testControl[] = {
	0x1, 0x0, 0x3,
	0x31, 0x33, 0x32,
	0x1, 0xd,
	0x6d, 0x79, 0x45, 0x78,
	0x61, 0x6d, 0x70, 0x6c,
	0x65, 0x2e, 0x74, 0x78, 0x74
};

static int receive_control(int fd, unsigned char* C, int* length, char* filename) {

	unsigned char* CP = NULL;
	unsigned packageSize = llread(fd, CP);
	int i = 3;

	if (packageSize < 0) {
		free(CP);
		puts("[ERROR] CONTROL package reception failed...");
		return -1;
	}

	if (CP[CTRL_C] == CTRL_PKG_START) {
		LOG("[INFORMATION] received START control package from TRANSMITTER...");
	}
	else if (CP[CTRL_C] == CTRL_PKG_END) {
		LOG("[INFORMATION] received END control package from TRANSMITTER...");
	}
	else {
		free(CP);
		puts("[ERROR] received wrong package: not a valid CONTROL package...");
		return -1;
	}

	*C = CP[CTRL_C];

	if (CP[CTRL_T1] != PARAM_FILE_SIZE) {
		free(CP);
		puts("[ERROR] received wrong parameter: PARAM_FILE_SIZE expected...");
		return -1;
	}

	unsigned fileSizeLength = CP[CTRL_L1];
	char* fileSize = (char*) malloc(fileSizeLength * sizeof(char));

	memcpy(fileSize, &CP[i], fileSizeLength);
	*length = atoi(fileSize);
	free(fileSize);
	i += fileSizeLength;

	if (CP[i++] != PARAM_FILE_NAME) {
		free(CP);
		puts("[ERROR] received wrong parameter: PARAM_FILE_NAME expected...");
		return -1;
	}

	const unsigned fileNameLength = CP[i++];
	const unsigned expectedSize = 5 + fileSizeLength + fileNameLength;
	
	memcpy(filename, &CP[i], fileNameLength);
	free(CP);
	i += fileNameLength;

	if (i != expectedSize) {
		puts("[ERROR] received wrong package: size mismatch...");
		return -1;
	}

	printf("[INFORMATION] fileSize=%d, fileName=%s\n", *length, filename);

	return 0;
}

/*
 * APPLICATION LAYER FUNCTIONS
 */

static int application_SEND(void) {

	FILE* fp = fopen(al->filename, "rb");
	
	if (fp == NULL) {
		perror(al->filename);
		return -1;
	}

	printf("INFORMATION: opened file %s for sending...\n", al->filename);

	int fileSize = calculate_size(fp);
	char fileSizeString[16];

	sprintf(fileSizeString, "%d", fileSize);

	// SEND "START" CONTROL PACKAGE
	if (send_control(al->fd, CTRL_PKG_START, fileSizeString, al->filename) < 0) {
		return -1;
	}

	printf("[INFORMATION] starting file transfer...\n");

	int sequenceNumber = 0;
	unsigned char* fileBuffer = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	unsigned bytesRead = 0;
	unsigned bytesWritten = 0;

	while ((bytesRead = fread(fileBuffer, 1, MAX_SIZE, fp)) > 0) {

		if (send_data(al->fd, (sequenceNumber++) % 255, fileBuffer, bytesRead) == -1) {
			return -1;
		}

		bzero(fileBuffer, MAX_SIZE);
		bytesWritten += bytesRead;
	//	printProgressBar(fileSizeReadSoFar, fileSize);
	}

	// FREE ALLOCATED MEMORY
	free(fileBuffer);

	// CLOSE FILE
	if (fclose(fp) < 0) {
		perror(al->filename);
		return -1;
	}

	// SEND "END" CONTROL PACKAGE
	if (send_control(al->fd, CTRL_PKG_END, fileSizeString, al->filename) < 0) {
		return -1;
	}

	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES WRITTEN: %d bytes\n", bytesWritten);

	return 0;
}

static int application_RECEIVE(void) {
	
	unsigned char controlMessage;
	char* fileName = NULL;
	int sequenceNumber = -1;
	int fileSize;

	LOG("[INFORMATION] waiting for START control package from transmitter...");
	
	// RECEIVE "START" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &fileSize, fileName) == -1) {
		return -1;
	}

	// CHECK IF VALID "START" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_START) {
		puts("[ERROR] received wrong control package: control field is not CTRL_PKG_START...");
		return -1;
	}

	// OPEN INPUT FILE FOR READING
	FILE* fp = fopen(al->filename, "wb");
	
	if (fp == NULL) {
		perror(al->filename);
		return -1;
	}

	printf("\n[INFORMATION] created output file: %s\n", al->filename);
	printf("[INFORMATION] expected output file size: %d (bytes)\n\n", fileSize);
	puts("[INFORMATION] starting file transfer...");

	unsigned bytesRead = 0;

	while (bytesRead != fileSize) {
		
		unsigned char* fileBuffer = NULL;
		int lastNumber = sequenceNumber;
		int length = 0;

		if (receive_data(al->fd, &sequenceNumber, fileBuffer, &length) == -1) {
			free(fileBuffer);
			return -1;
		}

		if (sequenceNumber && (lastNumber + 1 != sequenceNumber)) {
			printf("[ERROR] received sequence number %d, expected %d...\n", sequenceNumber, lastNumber + 1);
			free(fileBuffer);
			return -1;
		}

		fwrite(fileBuffer, sizeof(unsigned char), length, fp);
		free(fileBuffer);
		bytesRead += length;
	//	printProgressBar(fileSizeReadSoFar, fileSize);
	}

	LOG("[READ] waiting for END control package from transmitter...");
	
	// CLOSE INPUT FILE
	if (fclose(fp) < 0) {
		perror(al->filename);
		return -1;
	}

	char* lastName = NULL;
	int lastSize;

	// RECEIVE "END" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &lastSize, lastName) == -1) {
		return -1;
	}

	// CHECK FOR VALID "END" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_END) {
		puts("[ERROR] received wrong END package: control field is not CTRL_PKG_END...");
		return -1;
	}	
	
	// CHECK 
	if (strcmp(fileName, lastName) != 0) {
		puts("[ERROR] START and END control file name parameters don't match!");
		return -1;
	}

	// CHECK 
	if (fileSize != lastSize) {
		puts("[ERROR] START and END control file size parameters don't match!");
		return -1;
	}

	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES READ: %d bytes\n", bytesRead);
		
	return 0;
}

int application_start(void) {
	
	int rv = -1;
	
	if (al->mode == MODE_TRANSMITTER) {
		rv = application_SEND();
	}
	else if(al->mode == MODE_RECEIVER) {
		rv = application_RECEIVE();
	}

	logStatistics();
	
	return rv;
}

int application_close(void) {

	int rv = 0;

	if (llclose(al->fd, al->mode) == -1) {
		printf("[ERROR] llclose failed, connection wasn't terminated.");
		rv = -1;
	}

	return rv;
}

int application_init(char* port, int fd, int mode, char* filename) {
 
	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));
	strcpy(al->filename, filename);
	al->mode = mode;
	al->fd = fd;
	
	return 0;
}