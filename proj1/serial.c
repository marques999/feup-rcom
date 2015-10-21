#include "application.h"
#include "link.h"

#define _POSIX_SOURCE 1

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

static void clearBuffer() {	
	int c; while ((c = getchar()) != '\n' && c != EOF);
}

static int readInteger(int start, int end) {

	int input;

	while (1) {
		
		printf("? ");

		if (scanf("%d", &input) == 1 && input >= start && input <= end) {
			break;
		}

		puts("[ERROR] you have entered an invalid value, please try again...\n");
		clearBuffer();
	}

	return input;
}

static char* readString() {
	
	char* input =  malloc(MAX_SIZE * sizeof(char));

	while (1) {

		printf("? ");

		if (scanf("%s", input) == 1) {
			break;
		}

		puts("[ERROR] you have entered an invalid value, please try again...\n");
		clearBuffer();
	}

	return input;
}

size_t getFileSize(FILE* file) {

	size_t currentPosition = ftell(file);
	size_t currentSize;

	if (fseek(file, 0, SEEK_END) == -1) {
		return -1;
	}

	currentSize = ftell(file);
	fseek(file, 0, currentPosition);

	return currentSize;
}

#define PROGRESS_LENGTH		40

void writeProgress(double current, double total) {

	const double percentage = 100.0 * current / total;
	const int pos = (int) (percentage * PROGRESS_LENGTH / 100.0);

	printf("\rCompleted: %6.2f%% [", percentage);

	int i;	
	for (i = 0; i < PROGRESS_LENGTH; i++) {
		i <= pos ? printf("=") : printf(" ");
	}

	puts("]\n");
}

int main(int argc, char** argv) {
	
	puts("---------------------------------");
	puts("|\tRCOM FILE TRANSFER\t|");
	puts("---------------------------------");
	puts("\n> PLEASE CHOOSE AN OPTION:");
	puts("(1) Send\n(2) Receive\n");

	int connectionMode = readInteger(1, 2) - 1;
	int connectionBaudrate = -1;

	while (connectionBaudrate == -1) {
		puts("\n> ENTER TRANSMISSION BAUD RATE:");
		puts("\t[200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600]\n");
		connectionBaudrate = link_getBaudrate(readInteger(200, 57600));
	}

	puts("\n> ENTER MESSAGE MAXIMUM DATA SIZE:");
	int messageSize = readInteger(1, 256);
	puts("\n> ENTER MAXIMUM NUMBER OF RETRIES:");
	int connectionRetries = readInteger(0, 10);
	puts("\n> ENTER TIMEOUT VALUE (seconds):");
	int connectionTimeout = readInteger(1, 10);
	puts("\n> ENTER SERIAL PORT: (/dev/ttySx):");
	int numPort = readInteger(0, 9);

	char portName[20] = "/dev/ttySx";
	portName[9] = '0' + numPort;

	if (connectionMode == MODE_TRANSMITTER) {
		puts("\n> ENTER SOURCE FILENAME:");
	} else {
		puts("\n> ENTER DESTINATION FILENAME:");
	}

	char* fileName = readString();
	int fd = link_initialize(portName, connectionMode, connectionBaudrate, 
			connectionRetries, connectionTimeout);
	
	logConnection();
		
	if (fd < 0) {
		return -1;
	}
	
	application_init(portName, fd, connectionMode, fileName);
	
	unsigned char out[255];
	unsigned char data[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!' };

    if (connectionMode == MODE_TRANSMITTER) {
		llwrite(fd, data, sizeof(data)/sizeof(data[0]));
	} else {
		llread(fd, out);
	}

	llclose(fd, connectionMode);

    return 0;
}
