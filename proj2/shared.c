#include "shared.h"

void clearBuffer() {

	int c;

	while ((c = getchar()) != '\n' && c != EOF)
	{
	}
}

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
		puts("[ERROR] unknown host, check your network connection!");
		return NULL;
	}

	return inet_ntoa(*((struct in_addr *)h->h_addr));
}

void logProgress(double current, double total, double speed) {

	const double percentage = 100.0 * current / total;
	const int progressLength = 40;
	const int pos = (int)(percentage * progressLength / 100.0);
	int i;

	printf("\033cCompleted: %6.2f%% [", percentage);

	for (i = 0; i < progressLength; i++) {
		i <= pos ? printf("=") : printf(" ");
	}

	printf("] %.2f kBytes/sec\n", speed);
}