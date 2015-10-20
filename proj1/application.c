#include "application.h"

ApplicationLayer* al;

#define CONTROL_PACKAGE_START	1
#define CONTROL_PACKAGE_DATA	0
#define CONTROL_PACKAGE_END		2

#define PARAM_FILE_SIZE		0
#define PARAM_FILE_NAME		1

#define CONTROL_C 		0
#define CONTROL_T1		1
#define CONTROL_L1		2

#define DATA_C		CONTROL_C
#define DATA_N		CONTROL_T1
#define DATA_L2		CONTROL_L1
#define DATA_L1		3
#define DATA_P		4
	
#define APPLICATION_DEBUG 1

#define LOG(msg) 	if (APPLICATION_DEBUG) puts(msg);

int alInitialize(char* port, int mode, char* filename) {

	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));
	al->mode = mode;
	al->fileName = filename;
	al->fd = llopen(port, file);
	
	if (al->fd < 0) {
		printf("[ERROR] llopen failed, no response from serial port...\n");
		return -1;
	}
	
	logConnection();
	application_start();

	if (llclose(al->fd) == -1) {
		printf("[ERROR] llclose failed, connection wasn't terminated.");
		return -1;
	}

	logStatistics();

	return 0;
}

int application_start() {
	
	int rv;
	
	if (al->mode == MODE_TRANSMITTER) {
		rv = application_SEND();
	}
	else if(al->mode == MODE_RECEIVER) {
		rv = application_RECEIVE();
	}
	else {
		return -1;
	}
	
	if (rv == 0) {
		puts("[INFORMATION] file transfer completed successfully!");
	}
	
	return rv;
}

static size_t calculate_size(FILE* file) {

	size_t currentPosition = ftell(file);
	size_t currentSize;

	if (fseek(file, 0, SEEK_END) == -1) {
		return -1;
	}

	currentSize = ftell(file);
	fseek(file, 0, currentPosition);

	return currentSize;
}

/**
 * DATA PACKAGES
 */

static int data_send(int fd, int N, const char* buffer, int length) {

	unsigned char* DP = (unsigned char*) malloc(packageSize * sizeof(unsigned char));
	unsigned int packageSize = 4 + length;

	DP[DATA_C] = CONTROL_PACKAGE_DATA;
	DP[DATA_N] = N;
	DP[DATA_L2] = length / 256;
	DP[DATA_L1] = length % 256;
	memcpy(&DP[DATA_P], buffer, length);

	if (llwrite(fd, DP, packageSize) == -1) {
		puts("[LLWRITE] sending data package failed...");
		free(DP);
		return -1;
	}

	free(DP);

	return 0;
}

static int data_receive(int fd, int* N, char** buffer, int* length) {

	unsigned char* DP;
	unsigned int packageSize = llread(fd, &DP);

	if (packageSize == -1) {
		puts("[APPLICATION] DATA package reception failed...");
		return -1;
	}

	if (DP[DATA_C] != CONTROL_PACKAGE_DATA) {
		puts("[APPLICATION] received wrong DATA package, control field is not DATA.");
		return -1;
	}

	*N = DP[DATA_N];
	*length = 256 * DP[DATA_L2] + DP[DATA_L1];
	*buffer = malloc(*length);
	memcpy(*buffer, &DP[DATA_P], *length);
	free(DP);

	return 0;
}

/**
 * CONTROL PACKAGES
 */

static int control_send(int fd, unsigned char C, unsigned char* fileSize, char* fileName) {

	if (C == CONTROL_PACKAGE_START) {
		LOG("[INFORMATION] sending START control package to RECEIVER...");
	}
	else if (C == CONTROL_PACKAGE_END) {
		LOG("[INFORMATION] sending END control package to RECEIVER...");
	}
	else {
		LOG("[ERROR] sending UNKNOWN control package to RECEIVER...");
		return -1;
	}

	size_t sizeLength = strlen(fileSize);
	size_t nameLength = strlen(fileName);

	unsigned char* CP = malloc(packageSize * sizeof(unsigned char));
	int packageSize = 5 + strlen(fileSize) + strlen(fileName);
	int sequenceNumber = 0;
	int i = 3;

	CP[CONTROL_C] = C;
	CP[CONTROL_T1] = PARAM_FILE_SIZE;
	CP[CONTROL_L1] = sizeLength;
	memcpy(&CP[i], fileSize, sizeLength);
	i += sizeLength;
	CP[i++] = PARAM_FILE_NAME;
	CP[i++] = nameLength;
	memcpy(&CP[i], fileName, nameLength);
	i += sizeLength;

	for (int j = 0; j < i; j++) {
		printf("CONTROL[%d] = 0x%x\n", j, (unsigned char)CP[j]);
	}

	if (llwrite(fd, CP, i) == -1) {
		puts("[LLWRITE] sending control package failed...");
		free(CP);
		return -1;
	}

	if (C == CONTROL_PACKAGE_START) {
		puts("[INFORMATION] START control package successfully sent!");
	}
	else if (C == CONTROL_PACKAGE_end) {
		puts("[INFORMATION] END control package successfully sent!");
	}

	free(CP);

	return 0;
}

int control_receive(int fd, unsigned char* C, int* length, char** filename) {
	
	unsigned char* CP;
	unsigned int packageSize = llread(fd, &CP);
	int i = 3;

	if (packageSize == -1) {
		puts("[APPLICATION] control package reception failed...");
		return -1;
	}

	if (CP[CONTROL_C] == CONTROL_PACKAGE_START) {
		LOG("[APPLICATION] received START control package from TRANSMITTER...");
	}
	else if (CP[CONTROL_C] == CONTROL_PACKAGE_END) {
		LOG("[APPLICATION] received END control package from TRANSMITTER...");
	}
	else {
		puts("[APPLICATION] received unknown control package from TRANSMITTER...");
		return -1;
	}

	*C = CP[CONTROL_C];

	if (CP[CONTROL_T1] != PARAM_FILE_SIZE) {
		puts("[APPLICATION] received wrong parameter, expected PARAM_FILE_SIZE...");
		return -1;
	}

	unsigned fileSizeLength = CP[CONTROL_L1];
	char* fileSize = (char*) malloc(fileSizeLength * sizeof(char));

	memcpy(fileSize, &CP[i], fileSizeLength);
	*length = atoi(fileSize);
	free(fileSize);
	i += fileSizeLength;

	if (CP[i++] != PARAM_FILE_NAME) {
		puts("[APPLICATION] received wrong parameter, expected PARAM_FILE_NAME...");
		return -1;
	}

	unsigned fileNameLength = CP[i++];
	unsigned expectedSize = 5 + fileSizeLength + fileNameLength;

	memcpy(*filename, &CP[i], flieNameLength);
	i += fileNameLength;

	if (i != expectedSize) {
		puts("[APPLICATION] received package has wrong size");
	}

	return 0;
}

/*
 * APPLICATION LAYER FUNCTIONS
 */

static int application_SEND() {

	FILE* fp;
	
	if ((fopen(al->fileName, "rb") == NULL) {
		perror(al->fileName);
		return -1;
	}
	
	printf("INFORMATION: opened file %s for sending...\n", al->fileName);
	
	int fileSize = calculate_size(fp);
	char fileSizeString[16];
	
	sprintf(fileSizeString, "%d", fileSize);
	
	// send start control pakage
	if (send_control(al->fd), CTRL_PKG_START, fileSizeString, al->fileName) == -1) {

	}
	
	printf("[INFORMATION] starting file transfer...\n");
	
	int sequenceNumber = 0;
	char fileBuffer[MAX_SIZE];
	size_t bytesRead = 0;
	size_t bytesWritten = 0;

	while ((bytesRead = fread(fileBuf, sizeof(unsigned char), MAX_SIZE, file)) > 0) {
		if (send_data(al->fd, sequenceNumber++ % 255, fileBuffer, bytesRead)) == -1) {
			// error sending data package
			return -1;
		}

		bzero(fileBuffer ,MAX_SIZE);
		bytesWritten += bytesRead;
		// refresh progress
	}
	
	free(fileBuffer);

	if (fclose(fp) < 0) {
		perror(al->fileName);
		return -1;
	}

	if (!send_control(fd, CTRL_PKG_END, sizeof(unsigned char), 0) == -1) {
		printf("[SEND] error sending END control package...");
		return -1;
	}
		
	return 0;
}

static int application_RECEIVE() {
	
	// TODO create struct to carry these
	int controlStart, fileSize;
	char* fileName;

	LOG("[READ] waiting for START control package from transmitter...");
	
/*	if (!receiveControlPackage(al->fd, &controlStart, &fileSize, &fileName))
		return 0;
*/

	if (controlStart != CTRL_PKG_START) {
		puts("[READ] received wrong control package: control field is not C_PKG_START...");
		return -1;
	}

	// create output file
	
	FILE* fp = fopen(al->fileName, "wb");
	
	if (fp == NULL) {
		perror(al->fileName);
		return -1;
	}

	printf("\n");
	printf("[INFORMATION] created output file: %s\n", al->fileName);
	printf("[INFORMATION] expected file size: %d (bytes)\n", fileSize);
	printf("\n");
	
	/////
	
	printf("[INFORMATION] starting file transfer...\n");

	int packageReceived = FALSE;

	LOG("[READ] waiting for END control package from transmitter...");
	
	if(control_receive(al->fd, &packageReceived, 0, NULL)) {
		puts("[RECEIVE] received wrong END package, not a control package.");
		return -1;
	}

	if (packageReceived != CTRL_PKG_END) {
		puts("[RECEIVED] received wrong END package, control field is not END.");
		return -1;
	}	

	if (fclose(fp) < 0) {
		perror(al->fileName);
		return -1;
	}
		
	return 0;
}