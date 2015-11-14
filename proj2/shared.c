#include "shared.h"

#define PROGRESS_LENGTH 40

/*
 * clears stdin input buffer
 */
static void clearBuffer() {
	int c;
	while ((c = getchar()) != '\n' && c != EOF);
}

/*
 * reads an integer value from stdin
 * @param start number lower bound (minimum value)
 * @param end number upper bound (maximum value)
 * @returns number containing the user input
 */
int readInteger(int start, int end) {

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

/*
 * reads a string from stdin
 * @returns string containing the user input
 */
char* readString() {
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

long long getCurrentTime() {
	struct timeval te;
	gettimeofday(&te, NULL);
	return (te.tv_sec * 1000LL) + (te.tv_usec / 1000LL);
}

char* getIP(const char* hostName) {

	struct hostent *h = gethostbyname(hostName);

	if (h == NULL) {
		herror("gethostbyname");
		return NULL;
	}

	return inet_ntoa(*((struct in_addr *)h->h_addr));
}

/*
 * logs current file transfer progress on screen
 * @param current number of bytes transfered so far
 * @param total total number of bytes being transfered
 * @param speed current transfer speed (kBytes/sec)
 */
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