#include "application.h"
#include "link.h"
#include <sys/time.h>

ApplicationLayer* al = NULL;

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

#define PROGRESS_LENGTH		40
#define APPLICATION_DEBUG 	0
#define ERROR(msg)			fprintf(stderr, msg, "[ERROR]"); return -1
#define LOG(msg)			if (APPLICATION_DEBUG) puts(msg)
#define LOG_FORMAT(...)		if (APPLICATION_DEBUG) printf(__VA_ARGS__)

void logProgress(double current, double total, double speed) {

	const double percentage = 100.0 * current / total;
	const int pos = (int)(percentage * PROGRESS_LENGTH / 100.0);
	int i;

	printf("\033cCompleted: %6.2f%% [", percentage);

	for (i = 0; i < PROGRESS_LENGTH; i++) {
		i <= pos ? printf("=") : printf(" ");
	}

	printf("] %.2f kBytes/sec\n", speed);
}

long long current_time() {
    struct timeval te;
    gettimeofday(&te, NULL);
    return (te.tv_sec * 1000LL) + (te.tv_usec / 1000);
}

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

	unsigned packageSize = 4 + length;
	unsigned char* DP = (unsigned char*) malloc(packageSize * sizeof(unsigned char));

	DP[DATA_C] = CTRL_PKG_DATA;
	DP[DATA_N] = N;
	DP[DATA_L2] = (unsigned char) (length / 256);
	DP[DATA_L1] = (unsigned char) (length % 256);
	memcpy(&DP[DATA_P], buffer, length);

	if (llwrite(fd, DP, packageSize) < 0) {
		free(DP);
		ERROR("%s connection problem: DATA package not sent\n");
	}

	free(DP);

	return 0;
}

static int receive_data(int fd, int* N, unsigned char* buffer, int* length) {

	unsigned char* DP = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	unsigned packageSize = llread(fd, DP);

	if (packageSize < 0) {
		free(DP);
		ERROR("%s connection problem: DATA package not received\n");
	}
	else if (DP[DATA_C] != CTRL_PKG_DATA) {
		free(DP);
		ERROR("%s received wrong DATA package: control field is not CTRL_PKG_DATA...\n");
	}
	else {
		printf("saved N, value=%d\n", *N);
		*N = DP[DATA_N];
		*length = 256 * DP[DATA_L2] + DP[DATA_L1];
		memcpy(buffer, &DP[DATA_P], *length);
		free(DP);
	}

	return 0;
}

/**
 * CONTROL PACKAGES
 */

static int send_control(int fd, unsigned char C, const char* fileSize, const char* fileName) {

	if (C == CTRL_PKG_START) {
		LOG("[INFORMATION] sending START control package to RECEIVER...");
	}
	else if (C == CTRL_PKG_END) {
		LOG("[INFORMATION] sending END control package to RECEIVER...");
	}
	else {
		ERROR("%s sending UNKNOWN control package to RECEIVER...\n");
	}

	int i = 3;
	const unsigned sizeLength = strlen(fileSize);
	const unsigned nameLength = strlen(fileName);
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

	if (llwrite(fd, CP, i) < 0) {
		free(CP);
		return -1;
	}

	free(CP);

	if (C == CTRL_PKG_START) {
		LOG("[INFORMATION] START control package successfully sent!");
	}
	else if (C == CTRL_PKG_END) {
		LOG("[INFORMATION] END control package successfully sent!");
	}

	LOG_FORMAT("[INFORMATION] fileSize=%s, fileName=%s\n", fileSize, fileName);

	return 0;
}

static int receive_control(int fd, unsigned char* C, int* length, char* filename) {

	unsigned char* CP = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	unsigned packageSize = llread(fd, CP);
	int i = 3;

	if (packageSize < 0) {
		free(CP);
		return -1;
	}

	if (CP[CTRL_C] == CTRL_PKG_START) {
		LOG("[INFORMATION] received START control package from TRANSMITTER");
	}
	else if (CP[CTRL_C] == CTRL_PKG_END) {
		LOG("[INFORMATION] received END control package from TRANSMITTER");
	}
	else {
		free(CP);
		ERROR("%s received wrong package: not a valid CONTROL package...\n");
	}

	*C = CP[CTRL_C];

	if (CP[CTRL_T1] != PARAM_FILE_SIZE) {
		free(CP);
		ERROR("%s received wrong parameter: PARAM_FILE_SIZE expected...\n");
	}

	unsigned fileSizeLength = CP[CTRL_L1];
	char* fileSize = (char*) malloc(fileSizeLength * sizeof(char));

	memcpy(fileSize, &CP[i], fileSizeLength);
	fileSize[fileSizeLength] = '\0';
	*length = atoi(fileSize);
	free(fileSize);
	i += fileSizeLength;

	if (CP[i++] != PARAM_FILE_NAME) {
		free(CP);
		ERROR("%s received wrong parameter: PARAM_FILE_NAME expected...\n");
	}

	const unsigned fileNameLength = CP[i++];
	const unsigned expectedSize = 5 + fileSizeLength + fileNameLength;

	memcpy(filename, &CP[i], fileNameLength);
	filename[fileNameLength] = '\0';
	free(CP);
	i += fileNameLength;

	if (i != expectedSize) {
		ERROR("%s received wrong package: size mismatch...\n");
	}

	LOG_FORMAT("[INFORMATION] fileSize=%d, fileName=%s\n", *length, filename);

	return 0;
}

/*
 * APPLICATION LAYER FUNCTIONS
 */
static int application_SEND(void) {

	long long lastUpdate = current_time();
	long long totalTime = 0;
	int fileSize = calculate_size(al->fp);
	char fileSizeString[16];

	printf("[INFORMATION] opened file %s for sending...\n", al->filename);
	sprintf(fileSizeString, "%d", fileSize);

	// SEND "START" CONTROL PACKAGE
	if (send_control(al->fd, CTRL_PKG_START, fileSizeString, al->filename) < 0) {
		return -1;
	}

	printf("[INFORMATION] starting file transfer...\n");

	// BEGIN FILE TRANSFER
	int sequenceNumber = 0;
	unsigned char* fileBuffer = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	unsigned bytesRead = 0;
	unsigned bytesWritten = 0;
	long long currentUpdate;
	double transferSpeed;

	// READ INPUT FILE
	while ((bytesRead = fread(fileBuffer, 1, al->maxsize, al->fp)) > 0) {

		if (send_data(al->fd, (sequenceNumber++) % al->maxsize, fileBuffer, bytesRead) == -1) {
			return -1;
		}

		memset(fileBuffer, 0, MAX_SIZE);
		bytesWritten += bytesRead;
		currentUpdate = current_time();
		transferSpeed = bytesRead / (double) (currentUpdate - lastUpdate);
		totalTime += currentUpdate - lastUpdate;
		lastUpdate = currentUpdate;
		logProgress(bytesWritten, fileSize, transferSpeed);
	}

	// FREE ALLOCATED MEMORY
	free(fileBuffer);

	// CLOSE FILE
	if (fclose(al->fp) < 0) {
		perror(al->filename);
		return -1;
	}

	// SEND "END" CONTROL PACKAGE
	if (send_control(al->fd, CTRL_PKG_END, fileSizeString, al->filename) < 0) {
		return -1;
	}

	al->fp = NULL;
	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES WRITTEN: %d bytes\n", bytesWritten);
	printf("[INFORMATION] AVERAGE TRANSFER SPEED: %.2f kBytes/sec\n", (double) fileSize / totalTime);

	return 0;
}

static int application_RECEIVE(void) {

	unsigned char controlMessage;
	long long lastUpdate = current_time();
	long long totalTime = 0;
	char* fileName = (char*) malloc(MAX_SIZE * sizeof(char));
	int sequenceNumber = -1;
	int fileSize;

	LOG("[INFORMATION] waiting for START control package from transmitter...");

	// RECEIVE "START" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &fileSize, fileName) == -1) {
		fclose(al->fp);
		ERROR("%s connection problem: couldn't receive START control package\n");
	}

	// CHECK IF VALID "START" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_START) {
		ERROR("%s received wrong control package: control field is not CTRL_PKG_START...\n");
	}

	printf("\n[INFORMATION] created output file: %s\n", al->filename);
	printf("[INFORMATION] expected output file size: %d (bytes)\n\n", fileSize);
	puts("[INFORMATION] starting file transfer...");

	unsigned bytesRead = 0;
	unsigned char* fileBuffer = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	long long currentUpdate;
	double transferSpeed;

	while (bytesRead != fileSize) {

		int lastNumber = sequenceNumber;
		int length = 0;

		if (receive_data(al->fd, &sequenceNumber, fileBuffer, &length) == -1) {
			free(fileBuffer);
			fclose(al->fp);
			ERROR("%s connection problem: couldn't receive DATA control package\n");
		}

		if (sequenceNumber && (lastNumber + 1 != sequenceNumber)) {
			printf("[ERROR] connection problem: received sequence number %d, expected %d...\n", sequenceNumber, lastNumber + 1);
		}
		else {
			fwrite(fileBuffer, 1, length, al->fp);
			memset(fileBuffer, 0, MAX_SIZE);
			bytesRead += length;
			currentUpdate = current_time();
			transferSpeed = length / (double) (currentUpdate - lastUpdate);
			totalTime += currentUpdate - lastUpdate;
			lastUpdate = currentUpdate;
			logProgress(bytesRead, fileSize, transferSpeed);
		}
	}

	LOG("[READ] waiting for END control package from transmitter...");
	free(fileBuffer);

	// CLOSE INPUT FILE
	if (fclose(al->fp) < 0) {
		perror(al->filename);
		return -1;
	}

	char* lastName = (char*) malloc(MAX_SIZE * sizeof(char));
	int lastSize;

	// RECEIVE "END" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &lastSize, lastName) == -1) {
		ERROR("%s connection problem: couldn't receive END control package\n");
	}

	// CHECK FOR VALID "END" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_END) {
		ERROR("%s received wrong END package: control field is not CTRL_PKG_END...\n");
	}

	// CHECK FILE NAMES
	if (strcmp(fileName, lastName) != 0) {
		ERROR("%s START and END control file name parameters don't match!\n");
	}

	// CHECK FILE SIZES
	if (fileSize != lastSize) {
		ERROR("%s START and END control file size parameters don't match!\n");
	}

	al->fp = NULL;
	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES READ: %d bytes\n", bytesRead);
	printf("[INFORMATION] AVERAGE TRANSFER SPEED: %.2f kBytes/sec\n", (double) fileSize / totalTime);

	return 0;
}

int application_start(void) {

	if (al == NULL || al->fd < 0) {
		return -1;
	}

	int rv = 0;

	if (al->mode == TRANSMITTER) {
		al->fp = fopen(al->filename, "rb");
	}
	else if(al->mode == RECEIVER) {
		al->fp = fopen(al->filename, "wb");
	}

	if (al->fp == NULL) {
		perror(al->filename);
		return -1;
	}

	if (al->mode == TRANSMITTER) {
		rv = application_SEND();
	}
	else if (al->mode == RECEIVER) {
		rv = application_RECEIVE();
	}

	if (rv < 0) {
		return -1;
	}

	return application_close();
}

int application_close(void) {

	if (llclose(al->fd, al->mode) == -1) {
		free(al->filename);
		ERROR("%s connection problem: attempt to disconnect failed\n");
	}

	free(al->filename);
	logStatistics();

	return 0;
}

int application_config(int baudrate, int retries, int timeout, int maxsize) {

	LinkLayer* ll = llinit(al->port, al->mode, baudrate, retries, timeout, maxsize);

	if (ll == NULL) {
		return -1;
	}

	al->fd = llopen(al->port, al->mode);

	if (al->fd < 0) {
		return -1;
	}

	al->maxsize = maxsize;
	logConnection();

	return 0;
}

int application_init(char* port, int mode, char* filename) {

	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));

	if (al == NULL) {
		return -1;
	}

	al->mode = mode;
	al->filename = (char*) malloc(MAX_SIZE * sizeof(char));
	strcpy(al->filename, filename);
	strcpy(al->port, port);

	return 0;
}
