#include "ftp.h"

int main(int argc, char** argv) {

	char *userName = NULL, *userPassword = NULL;
	char *userHostname = NULL, *userPath = NULL;
	int userPort = 21;
	char* userCommand = malloc(MESSAGE_SIZE * sizeof(char));

	if (argc < 2) {

		puts("---------------------------------");
		puts("|\tRCOM FTP DOWNLOAD\t|");
		puts("---------------------------------");
		puts("\n> ENTER HOST NAME:");
		userHostname = readString();
		puts("\n> ENTER HOST PORT (DEFAULT 21): ");
		userPort = readInteger(1, 65535);
		puts("\n> ENTER SOURCE FILENAME:");
		userPath = readString();
		puts("\n> ENTER USERNAME:");
		userName = readString();

		if (strcmp("anonymous", userName)) {
			puts("\n> ENTER PASSWORD:");
			userPassword = readString();
			sprintf(userCommand, "ftp://%s:%s@%s:%d/%s", userName, userPassword, userHostname, userPort, userPath);
		}
		else {
			sprintf(userCommand, "ftp://%s:%d/%s", userHostname, userPort, userPath);
		}

		printf("\n");

		return action_startConnection(userCommand);
	}

	if (argc != 2) {
		printf("USAGE: ./download ftp://[<username>:<password>@]<hostname>/<path>\n");
		exit(0);
	}

	return action_startConnection(argv[1]);
}