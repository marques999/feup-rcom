#include "application.h"
#include "link.h"
#include <sys/time.h>

typedef struct {
	int fd;
	int mode;
	FILE* fp;
	int maxsize;
	char port[20];
	char* filename;
} ApplicationLayer;

ApplicationLayer* al = NULL;

/*
 * CONTROL PACKAGE COMMANDS
 */
#define CTRL_PKG_START	1
#define CTRL_PKG_DATA	0
#define CTRL_PKG_END	2

/*
 * CONTROL PACKAGE PARAMETERS
 */
#define PARAM_FILE_SIZE		0
#define PARAM_FILE_NAME		1

/*
 * CONTROL PACKAGE FIELDS
 */
#define CTRL_C 		0
#define CTRL_T1		1
#define CTRL_L1		2

/*
 * DATA PACKAGE FIELDS
 */
#define DATA_C		0
#define DATA_N		1
#define DATA_L2		2
#define DATA_L1		3
#define DATA_P		4

/*
 * LOGGING AND USER INTERFACE
 */
#define PROGRESS_LENGTH		40
#define APPLICATION_DEBUG 	0
#define ERROR(msg)			fprintf(stderr, msg, "[ERROR]"); return -1
#define LOG(msg)			if (APPLICATION_DEBUG) puts(msg)
#define LOG_FORMAT(...)		if (APPLICATION_DEBUG) printf(__VA_ARGS__)

static void logProgress(double current, double total, double speed) {

	const double percentage = 100.0 * current / total;
	const int pos = (int)(percentage * PROGRESS_LENGTH / 100.0);
	int i;

	printf("\033cCompleted: %6.2f%% [", percentage);

	for (i = 0; i < PROGRESS_LENGTH; i++) {
		i <= pos ? printf("=") : printf(" ");
	}

	printf("] %.2f kBytes/sec\n", speed);
}

static long long current_time() {
    struct timeval te;
    gettimeofday(&te, NULL);
    return (te.tv_sec * 1000LL) + (te.tv_usec / 1000LL);
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

static int send_data(int fd, int N, const unsigned char* buffer, int length) {

	int packageSize = 4 + length;
	unsigned char* DP = (unsigned char*) malloc(packageSize * sizeof(unsigned char));

	DP[DATA_C] = CTRL_PKG_DATA;
	DP[DATA_N] = N;
	DP[DATA_L2] = length / 256;
	DP[DATA_L1] = length % 256;
	memcpy(&DP[DATA_P], buffer, length);

	if (llwrite(fd, DP, packageSize) <= 0) {
		free(DP);
		ERROR("%s connection problem: couldn't send DATA package.\n");
	}

	free(DP);

	return 0;
}

static int receive_data(int fd, int* N, unsigned char* buffer, int* length) {

	unsigned char* DP = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	int packageSize = llread(fd, DP);

	if (packageSize < 0) {
		free(DP);
		ERROR("%s connection problem: couldn't receive DATA package.\n");
	}

	if (DP[DATA_C] != CTRL_PKG_DATA) {
		free(DP);
		ERROR("%s package validation failed: control field is not CTRL_PKG_DATA...\n");
	}

	*N = DP[DATA_N];
	*length = 256 * DP[DATA_L2] + DP[DATA_L1];
	memcpy(buffer, &DP[DATA_P], *length);
	free(DP);

	return 0;
}

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

	const unsigned sizeLength = strlen(fileSize);
	const unsigned nameLength = strlen(fileName);
	int i = 3;
	int packageSize = 5 + sizeLength + nameLength;
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

	if (llwrite(fd, CP, i) <= 0) {
		free(CP);
		ERROR("%s connection problem: couldn't send CONTROL package.\n");
	}

	free(CP);

	if (C == CTRL_PKG_START) {
		LOG("[INFORMATION] START control package successfully sent!");
	}
	else if (C == CTRL_PKG_END) {
		LOG("[INFORMATION] END control package successfully sent!");
	}

	return 0;
}

static int receive_control(int fd, unsigned char* C, int* length, char* filename) {

	unsigned char* CP = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	int i = 3;

	if (llread(fd, CP) < 0) {
		free(CP);
		ERROR("%s connection problem: couldn't receive CONTROL package.\n");
	}

	if (CP[CTRL_C] == CTRL_PKG_START) {
		LOG("[INFORMATION] received START control package from TRANSMITTER!");
	}
	else if (CP[CTRL_C] == CTRL_PKG_END) {
		LOG("[INFORMATION] received END control package from TRANSMITTER!");
	}
	else {
		free(CP);
		ERROR("%s package validation failed: not a CONTROL package...\n");
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
		ERROR("%s package validation failed: package sizes don't match...\n");
	}

	return 0;
}

static int application_SEND(void) {

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
	int bytesRead = 0;
	int bytesWritten = 0;
	unsigned char* fileBuffer = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	long long lastUpdate = current_time();
	long long totalTime = 0LL;
	long long currentUpdate;
	double transferSpeed;

	// READ INPUT FILE
	while ((bytesRead = fread(fileBuffer, 1, al->maxsize, al->fp)) > 0) {

		if (send_data(al->fd, (sequenceNumber++) % 256, fileBuffer, bytesRead) < 0) {
			free(fileBuffer);
			ERROR("%s connection problem: transmission aborted after timeout...\n");
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

	// SEND "END" CONTROL PACKAGE
	if (send_control(al->fd, CTRL_PKG_END, fileSizeString, al->filename) < 0) {
		return -1;
	}

	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES WRITTEN: %d bytes\n", bytesWritten);
	printf("[INFORMATION] AVERAGE TRANSFER SPEED: %.2f kBytes/sec\n", (double) fileSize / totalTime);

	return 0;
}

static int application_RECEIVE(void) {

	unsigned char controlMessage;
	char* fileName = (char*) malloc(PATH_MAX * sizeof(char));
	int sequenceNumber = -1;
	int fileSize;

	LOG("[INFORMATION] waiting for START control package from transmitter...");

	// RECEIVE "START" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &fileSize, fileName) < 0) {
		free(fileName);
		ERROR("%s connection problem: couldn't receive START control package\n");
	}

	// CHECK IF VALID "START" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_START) {
		free(fileName);
		ERROR("%s received wrong control package: control field is not CTRL_PKG_START...\n");
	}

	printf("\n[INFORMATION] created output file: %s\n", al->filename);
	printf("[INFORMATION] expected output file size: %d (bytes)\n\n", fileSize);
	puts("[INFORMATION] starting file transfer...");

	unsigned char* fileBuffer = (unsigned char*) malloc(MAX_SIZE * sizeof(unsigned char));
	long long lastUpdate = current_time();
	long long totalTime = 0LL;
	long long currentUpdate;
	double transferSpeed;
	int bytesRead = 0;

	while (bytesRead != fileSize) {

		int lastNumber = sequenceNumber;
		int length = 0;

		if (receive_data(al->fd, &sequenceNumber, fileBuffer, &length) < 0) {
			free(fileName);
			free(fileBuffer);
			ERROR("%s connection problem: transmission aborted after timeout...\n");
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

	char* lastName = (char*) malloc(PATH_MAX * sizeof(char));
	int lastSize;

	// RECEIVE "END" CONTROL PACKAGE
	if (receive_control(al->fd, &controlMessage, &lastSize, lastName) == -1) {
		free(fileName);
		free(lastName);
		ERROR("%s connection problem: couldn't receive END control package\n");
	}

	// CHECK FOR VALID "END" CONTROL PACKAGE
	if (controlMessage != CTRL_PKG_END) {
		free(fileName);
		free(lastName);
		ERROR("%s received wrong END package: control field is not CTRL_PKG_END...\n");
	}

	// CHECK START AND END FILE NAMES
	if (strcmp(fileName, lastName) != 0) {
		free(fileName);
		free(lastName);
		ERROR("%s START and END package file name parameters don't match!\n");
	}

	free(fileName);
	free(lastName);

	// CHECK START AND END FILE SIZES
	if (fileSize != lastSize) {
		ERROR("%s START and END package file size parameters don't match!\n");
	}

	// CHECK EXPECTED FILE SIZE
	if (fileSize != bytesRead) {
		ERROR("%s connection problem: expected and received file size don't match!");
	}

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
		rv = application_SEND();
	}
	else if (al->mode == RECEIVER) {
		rv = application_RECEIVE();
	}
	else {
		return -1;
	}

	if (rv < 0) {
		ERROR("%s connection problem: file transfer didn't complete successfully.\n");
	}

	if (llclose(al->fd) < 0) {
		ERROR("%s connection problem: attempt to disconnect failed.\n");
	}

	logStatistics();

	return 0;
}

int application_close(void) {

	// CLOSE INPUT FILE
	if (al->fp != NULL) {
		fclose(al->fp);
		al->fp = NULL;
	}

	// FREE ALLOCATED RESOURCES
	if (al->filename != NULL) {
		free(al->filename);
		al->filename = NULL;
	}

	return 0;
}

int application_connect(int baudrate, int retries, int timeout, int maxsize) {

	if (al->fp != NULL) {
		fclose(al->fp);
		al->fp = NULL;
	}

	if (al->mode == TRANSMITTER) {
		al->fp = fopen(al->filename, "rb");
	}
	else if(al->mode == RECEIVER) {
		al->fp = fopen(al->filename, "wb");
	}
	else {
		return -1;
	}

	if (al->fp == NULL) {
		perror(al->filename);
		return -1;
	}

	// INITIALIZE LINK LAYER
	al->maxsize = maxsize;

	// TRY TO ESTABLISH CONNECTION
	al->fd = llopen(al->port, al->mode, baudrate, retries, timeout, maxsize);

	if (al->fd < 0) {
		return -1;
	}

	return 0;
}

int application_init(char* port, int mode, char* filename) {

	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));

	if (al == NULL) {
		ERROR("%s system error: memory allocation failed.");
	}

	al->mode = mode;
	al->filename = (char*) malloc(PATH_MAX * sizeof(char));
	strcpy(al->filename, filename);
	strcpy(al->port, port);

	return 0;
}
