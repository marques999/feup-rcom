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

	char* input = malloc(MAX_SIZE * sizeof(char));

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

int main(int argc, char** argv) {

	puts("---------------------------------");
	puts("|\tRCOM FILE TRANSFER\t|");
	puts("---------------------------------");
	puts("\n> PLEASE CHOOSE AN OPTION:");
	puts("(1) Send\n(2) Receive\n");

	int connectionMode = readInteger(1, 2) - 1;
	int connectionBaudrate = 0;

	while (getBaudrate(connectionBaudrate) == -1) {
		puts("\n> ENTER TRANSMISSION BAUD RATE:");
		puts("\t[200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600]\n");
		connectionBaudrate = readInteger(200, 57600);
	}

	puts("\n> ENTER MESSAGE MAXIMUM SIZE:");
	int messageSize = readInteger(1, I_LENGTH);
	puts("\n> ENTER MAXIMUM NUMBER OF RETRIES:");
	int connectionRetries = readInteger(0, 10);
	puts("\n> ENTER TIMEOUT VALUE (seconds):");
	int connectionTimeout = readInteger(1, 10);
	puts("\n> ENTER SERIAL PORT: (/dev/ttySx):");
	int numPort = readInteger(0, 9);

	char portName[20] = "/dev/ttySx";
	portName[9] = '0' + numPort;

	if (connectionMode == TRANSMITTER) {
		puts("\n> ENTER SOURCE FILENAME:");
	}
	else {
		puts("\n> ENTER DESTINATION FILENAME:");
	}

	char* fileName = readString();
	
	printf("\n");

	int try = -1;
	int numberTries = 3;

	while (try < 0 && numberTries--) {

		if (application_init(portName, connectionMode, fileName) < 0) {
			continue;
		}

		if (application_config(connectionBaudrate, connectionRetries, connectionTimeout, messageSize) < 0) {
			continue;
		}

		try = application_start();
	}

	return 0;
}