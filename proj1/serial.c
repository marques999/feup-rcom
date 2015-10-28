#include "application.h"
#include "link.h"

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

	char* input = malloc(PATH_MAX * sizeof(char));

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

static void logUsage() {
	printf("USAGE: ./serial [(r)eceive/(s)end] [/dev/ttySx] [input/output file])\n");
	exit(0);
}

int main(int argc, char** argv) {

	int connectionMode;
	int connectionBaudrate;
	int messageSize;
	int connectionRetries;
	int connectionTimeout;
	int numPort;
	char portName[20];
	char* fileName = NULL;

	if (argc < 2) {
		puts("---------------------------------");
		puts("|\tRCOM FILE TRANSFER\t|");
		puts("---------------------------------");
		puts("\n> PLEASE CHOOSE AN OPTION:");
		puts("(1) Send\n(2) Receive\n");
		connectionMode = readInteger(1, 2) - 1;
		connectionBaudrate = 0;

		while (getBaudrate(connectionBaudrate) == -1) {
			puts("\n> ENTER TRANSMISSION BAUD RATE:");
			puts("\t[200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600]\n");
			connectionBaudrate = readInteger(200, 57600);
		}

		puts("\n> ENTER MESSAGE MAXIMUM SIZE:");
		messageSize = readInteger(1, I_LENGTH);
		puts("\n> ENTER MAXIMUM NUMBER OF RETRIES:");
		connectionRetries = readInteger(0, 10);
		puts("\n> ENTER TIMEOUT VALUE (seconds):");
		connectionTimeout = readInteger(1, 10);
		puts("\n> ENTER SERIAL PORT: (/dev/ttySx):");
		numPort = readInteger(0, 9);
		strcpy(portName, "/dev/ttySx");
		portName[9] = '0' + numPort;	
		
		if (connectionMode == TRANSMITTER) {
			puts("\n> ENTER SOURCE FILENAME:");
		}
		else {
			puts("\n> ENTER DESTINATION FILENAME:");
		}
	
		fileName = readString();
	}
	else {
	
		if (argv[1] == NULL || argc > 4) {
			logUsage();
		}

		if (strcmp(argv[1], "receive") == 0 || strcmp(argv[1], "r") == 0) {
			connectionMode = RECEIVER;
		}
		else if (strcmp(argv[1], "send") == 0 || strcmp(argv[1], "s") == 0) {
			connectionMode = TRANSMITTER;
		}
		else {
			logUsage();
		}
		
		if (argv[2] == NULL || argv[3] == NULL) {
			logUsage();
		}
			
		connectionBaudrate = 9600;
		messageSize = 256;
		connectionRetries = 3;
		connectionTimeout = 3;
		fileName = malloc(PATH_MAX * sizeof(char));
		strcpy(portName, argv[2]);	
		strcpy(fileName, argv[3]);
	}
	
	printf("\n");

	int try = -1;
	int numberTries = 3;

	if (application_init(portName, connectionMode, fileName) < 0) {
		return -1;
	}

	while (try < 0 && numberTries--) {

		if (application_connect(connectionBaudrate, connectionRetries, connectionTimeout, messageSize) < 0) {
			continue;
		}

		try = application_start();
	}

	application_close();

	return 0;
}
