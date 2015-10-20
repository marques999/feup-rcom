#include "gui.h"
#include "link.h"

static void clearBuffer() {
	int c;
	while ((c = getchar()) != '\n' && c != EOF) 
	{
	}
}

static int readInteger(int start, int end) {

	int input;

	while (1) {
		
		printf("? ");

		if (scanf("%d", &input) == 1 && input >= start && input <= end) {
			break;
		}

		puts("[ERROR] you have entered an invalid value, please try again...");
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

		puts("[ERROR] you have entered an invalid value, please try again...");
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

int main() {

	puts("---------------------------------");
	puts("|\tRCOM FILE TRANSFER\t|");
	puts("---------------------------------");
	puts("\n> PLEASE CHOOSE AN OPTION:");
	puts("(1) Send\n(2) Receive\n");

	int mode = readInteger(1, 2) - 1;
	int baudrate;

	do
	{
		puts("\n> ENTER TRANSMISSION BAUD RATE:");
		puts("\t[200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600]\n");
		baudrate = link_getBaudrate(readInteger(0, 57600));
		printf("\n");
	} while (baudrate == -1);

	puts("\n> ENTER MESSAGE MAXIMUM DATA SIZE:");
	int messageSize = readInteger(1, 512);
	puts("\n> ENTER MAXIMUM NUMBER OF RETRIES:");
	int numRetries = readInteger(0, 10);
	puts("\n> ENTER TIMEOUT VALUE (seconds):");
	int timeout = readInteger(1, 10);
	printf("Please enter the port which should be used? (/dev/ttySx) ");
	int portNum = readInteger(0, 9);

	char port[20] = "/dev/ttySx";
	port[9] = '0' + portNum;
	printf("Using port: %s\n", port);
	printf("\n");

	if (mode == MODE_TRANSMITTER) {
		puts("What file do you wish to transfer? ");
	} else {
		puts("Which name should the received file have? ");
	}

	char* file = readString();
	puts("\n");

	//link_initialize(port, 0, mode);
	//link_setBaudrate(baudrate);
	//link_setNumberRetries(numRetries);
	//link_setTimeout(timeout);
	//printConnectionInfo();
}