#include "application.h"

ApplicationLayer* al;

int alInitialize(char* fileName, int mode) {

	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));
	al->fd = openSerialPort(port);
	
	if (al->fd < 0) {
		printf("ERROR: Serial port could not be open.\n");
		return 0;
	}
	
	al->mode = mode;
	al->fileName = fileName;

	logConnection();
	alStart();
	logStatistics();

	return 1;
}

int alStart() {
	
	int rv;
	
	if (al->mode == MODE_TRANSMITTER) {
		rv = alSend();
	}
	else if(al->mode == MODE_RECEIVER) {
		rv = alReceive();
	}
	else {
		return -1;
	}
	
	if (rv == 0) {
		puts("[INFORMATION] file transfer completed successfully!");
	}
	
	return rv;
}

size_t calculateSize(FILE* file) {

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
	CONTROL PACKAGES
	**/
	
static int sendControl();
static int receiveControl();

/**
	DATA PACAKGES
*/
static int sendData();
static int receiveData();

/**
	APPLICATION LAYER FUNCTIONS
*/

static int alSend() {

	FILE* fp;
	
	if ((fopen(al->fileName, "rb") == NULL) {
		perror(al->fileName);
		return -1;
	}
	
	printf("INFORMATION: opened file for sendng");
	
	int fileSize = calculateSize(fp);
	char fileSizeString[16];
	
	sprintf(fileSizeString, "%d", fileSize);
	
	// send start control pakage
	
	printf("[INFORMATION] starting file transfer...\n");
	
	size_t bytesRead = 0;
	size_t bytesWritten = 0;
	int i;
	
//	while ((bytesRead = fread(fileBuf, sizeof(unsigned char), 
	
	if (fclose(fp) < 0) {
		perror(al->fileName);
		return -1;
	}
	
	// llclose
	
	printf("[INFORMATION] file transfer completed successfully!\n");
}

static int alReceive() {
	
	int fd;
	
	if ((fd = llopen(ll->mode)) < 0) { 
		return -1;
	}
	
	// TODO create struct to carry these
	int controlStart, fileSize;
	char* fileName;

	printf("[READ] waiting for START control package from transmitter...\n");
	
/*	if (!receiveControlPackage(al->fd, &controlStart, &fileSize, &fileName))
		return 0;
*/

	if (controlStart != CTRL_PKG_START) {
		printf(
				"ERROR: Control package received but its control field - %d - is not C_PKG_START",
				controlStart);
		return -1;
	}

	// create output file
	
	FILE* fp;
	
	if ((fp = fopen(al->fileName, "wb") == NULL) {
		perror(al->fileName);
		return -1;
	}

	printf("\n");
	printf("[INFORMATION] created output file: %s\n", al->fileName);
	printf("[INFORMATION] expected file size: %d (bytes)\n", fileSize);
	printf("\n");
	
	
	printf("[INFORMATION] starting file transfer...\n");

	// end
	return 0;
}
