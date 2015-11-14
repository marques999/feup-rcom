#include "shared.h"

#define h_addr 	h_addr_list[0]

#define APPLICATION_DEBUG		0
#define ERROR(msg)				fprintf(stderr, "%s: %s\n", "ERROR", msg); return FALSE
#define LOG(msg)				if (APPLICATION_DEBUG) puts(msg)
#define LOG_FORMAT(...)			if (APPLICATION_DEBUG) printf(__VA_ARGS__)

typedef struct {
	char* userName;
	char* userPassword;
	char* fileName;
	char* remoteHostname;
	int remotePort;
	int fdControl;
	int fdData;
} FTPConnection;

typedef struct {
	char* serverHostname;
	char* serverPath;
	char* serverIP;
	int serverPort;
} URL;

URL* url;
FTPConnection* ftp;

static int parseLogin(char* userName) {

	char* findSeparator = strchr(userName, ':');
	char* userPassword = NULL;
	int usernameSize = findSeparator - userName;

	if (ftp == NULL) {
		ftp = (FTPConnection*) malloc(sizeof(FTPConnection));
	}

	if (findSeparator != NULL) {

		userName[usernameSize] = '\0';
		userPassword = &userName[usernameSize + 1];

		if (strlen(&userName[usernameSize + 1]) <= 0) {
			printf("ERROR: you have entered an invalid password.\n");
			return FALSE;
		}
	}

	if (strlen(userName) <= 0) {
		printf("ERROR: you have entered an invalid username.\n");
		return FALSE;
	}

	ftp->userName = userName;
	ftp->userPassword = userPassword;

	return TRUE;
}

static int parseHostname(char* hostName) {

	char* portLocation = strchr(hostName, ':');
	int hostnameSize = portLocation - hostName;
	int serverPort = 21;

	if (url == NULL) {
		url = (URL*) malloc(sizeof(URL));
	}

	if (portLocation != NULL) {

		hostName[hostnameSize] = '\0';
		serverPort = atoi(&hostName[hostnameSize + 1]);

		if (serverPort <= 0) {
			printf("ERROR: you have entered an invalid port.\n");
			return FALSE;
		}
	}

	if (strlen(hostName) <= 0) {
		printf("ERROR: you have entered an invalid hostname.\n");
		return FALSE;
	}

	url->serverHostname = hostName;
	url->serverPort = serverPort;

	return TRUE;
}

static int parseURL(const char* userUrl) {

	if (strncmp(userUrl, "ftp://", 6)) {
		printf("ERROR: user has specified an invalid protocol.\n");
		return FALSE;
	}

	const char* ftpAddress = &userUrl[6];
	char* stringLocation = strchr(ftpAddress, '@');

	if (stringLocation != NULL) {
		char* authenticationString = malloc(stringLocation - ftpAddress + 1);
		strncpy(authenticationString, ftpAddress, stringLocation - ftpAddress);
		parseLogin(authenticationString);
	}

	char* pathLocation = strchr(stringLocation, '/');

	if (pathLocation != NULL) {
		char* hostnameString = malloc(pathLocation - stringLocation);
		strncpy(hostnameString, stringLocation + 1, pathLocation - stringLocation - 1);
		parseHostname(hostnameString);
		url->serverPath = pathLocation;
		url->serverIP = getIP(url->serverHostname);
	}

	return TRUE;
}

#define PASV_ADDR_LENGTH		4
#define PASV_PORT_LENGTH		2
#define MAX_SIZE		255
#define	PASV_READY		"227"
#define SERVICE_READY	"220"
#define	USER_OK			"331"
#define PASS_OK			"230"
#define STATUS_OK		"150"

//////////////////////////////
//		FTP COMMANDS		//
//////////////////////////////

static char* receiveCommand(int fd, const char* responseCmd) {

	char* responseMessage = (char*) malloc(MAX_SIZE + 1);

	if (read(fd, responseMessage, MAX_SIZE) <= 0) {
		return NULL;
	}

	printf("received message: %s", responseMessage);

	if (strncmp(responseCmd, responseMessage, strlen(responseCmd))) {
		return NULL;
	}

	return responseMessage;
}

static char* receivePasv(int fd) {

	char* responseMessage = (char*) malloc(MAX_SIZE + 1);

	if (read(fd, responseMessage, MAX_SIZE) <= 0) {
		return NULL;
	}

	if (strncmp(PASV_READY, responseMessage, strlen(PASV_READY))) {
		return NULL;
	}

	return responseMessage;
}

static int sendCommand(int fd, const char* msg, unsigned length) {

	int nBytes = write(fd, msg, length);

	if (nBytes <= 0) {
		return FALSE;
	}

	LOG_FORMAT("sent message: %s%d bytes written\n", msg, nBytes);

	return TRUE;
}

//////////////////////////////
//		DEBUG MESSAGES		//
//////////////////////////////

static void debugURL(void) {
	puts("+=======================================+");
	puts("|         CONNECTION STATISTICS         |");
	puts("+=======================================+");
	printf("| Hostname\t: %-21s |\n", url->serverHostname);
	printf("| IP Address\t: %-21s |\n", url->serverIP);
	printf("| Port\t\t: %-21d |\n", url->serverPort);
	puts("+=======================================+\n");
}

static void debugFTP(void) {
	puts("+=======================================+");
	puts("|             FTP STATISTICS            |");
	puts("+=======================================+");
	printf("| CONTROL\t: %-21d |\n", ftp->fdControl);
	printf("| DATA\t\t: %-21d |\n", ftp->fdData);
	printf("| Username\t: %-21s |\n", ftp->userName);
	printf("| Password\t: %-21s |\n", ftp->userPassword);
	puts("+=======================================+\n");
}

static int connectSocket(void) {

	struct sockaddr_in server_addr;

	// SERVER ADDRESS HANDLING
	memset((void*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(url->serverIP);
	server_addr.sin_port = htons(url->serverPort);

	// CONNECTING TO TCP SOCKET
	ftp->fdControl = socket(AF_INET, SOCK_STREAM, 0);

	if (ftp->fdControl < 0) {
		return FALSE;
	}

	// TRYING TO ESTABLISH CONNECTION TO THE REMOTE SERVER
	if (connect(ftp->fdControl, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		return FALSE;
	}

	return TRUE;
}

int connectPassive(void) {

	struct sockaddr_in server_addr;

	// SERVER ADDRESS HANDLING
	memset((void*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ftp->remoteHostname);
	server_addr.sin_port = htons(ftp->remotePort);

	// CONNECTING TO TCP SOCKET
	ftp->fdData = socket(AF_INET, SOCK_STREAM, 0);

	if (ftp->fdData < 0) {
		return FALSE;
	}

	// TRYING TO ESTABLISH CONNECTION TO THE REMOTE SERVER
	if (connect(ftp->fdData, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		return FALSE;
	}

	return TRUE;
}

static int sendPASS(int fd) {

	char userCommand[MAX_SIZE + 1];

	// FORMAT "PASS" COMMAND ARGUMENTS
	sprintf(userCommand, "PASS %s\r\n", ftp->userPassword);

	// SEND "PASS" COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending PASS command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, PASS_OK)) {
		ERROR("received invalid response from server, wrong password?");
	}

	return TRUE;
}

static int sendUSER(int fd) {

	char userCommand[MAX_SIZE + 1];

	// FORMAT "USER" COMMAND ARGUMENTS
	sprintf(userCommand, "USER %s\r\n", ftp->userName);

	// SEND "USER" COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending USER command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, USER_OK)) {
		ERROR("received invalid response from server, expected 331[USER_OK]");
	}

	return TRUE;
}

int sendCWD(const char* path) {

	char userCommand[MAX_SIZE + 1];

	// FORMAT "CWD" COMMAND ARGUMENTS
	sprintf(userCommand, "CWD %s\r\n", url->serverPath);

	// SEND "CWD" (change working directory) COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending CWD command to server failed!");
		return FALSE;
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, USER_OK)) {
		puts("ERROR: received invalid response from server, expected 331 [username okay, need password]");
		//return FALSE;
	}

	return TRUE;
}

int action_listDirectory() {

	// SEND "LIST" COMMAND
	if (!sendCommand(ftp->fdControl, "LIST\r\n", strlen("LIST\r\n"))) {
		ERROR("sending LIST command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, STATUS_OK)) {
		ERROR("received invalid response from server, expected 150[STATUS_OK]");
	}

	int bytesRead;
	int bytesWritten;
	char dataBuffer[1024];

	while ((bytesRead = read(ftp->fdData, dataBuffer, sizeof(dataBuffer))) > 0) {
		if ((bytesWritten = write(STDOUT_FILENO, dataBuffer, bytesRead)) < 0) {
			return FALSE;
		}
	}

	return bytesWritten;
}

int action_retrieveFile(const char* fileName) {

	char userCommand[MAX_SIZE + 1];

	// FORMAT "RETR" COMMAND ARGUMENTS
	sprintf(userCommand, "RETR %s\r\n", fileName);

	// SEND "RETR" COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending RETR command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	char* responseCommand = receiveCommand(ftp->fdControl, STATUS_OK);
	if (!responseCommand) {
		ERROR("received invalid response from server, expected 150[STATUS_OK]");
	}

	char expectedFilename[PATH_MAX];
	unsigned expectedSize = 0;

	if (sscanf(responseCommand, "150 Opening BINARY mode data connection for %s (%d bytes)",
		expectedFilename, &expectedSize) < 2) {
		ERROR("file transfer terminated unexpectedly");
	}

	printf("expected file name: %s\n", expectedFilename);
	printf("expected file size: %d\n", expectedSize);

	int fd = open(fileName, O_WRONLY | O_CREAT, 0600);

	if (fd < 0) {
		perror(fileName);
		return FALSE;
	}

	int bytesRead = 0;
	int bytesWritten = 0;
	int length;
	char dataBuffer[1024];
	long long lastUpdate = getCurrentTime();
	long long totalTime = 0LL;
	long long currentUpdate;
	double transferSpeed;

	while ((length = read(ftp->fdData, dataBuffer, sizeof(dataBuffer))) > 0) {

		if ((bytesWritten = write(fd, dataBuffer, bytesRead)) < 0) {
			printf("ERROR: Cannot write data in file.\n");
			return FALSE;
		}

			bytesRead += length;
			currentUpdate = getCurrentTime();
			transferSpeed = length / (double) (currentUpdate - lastUpdate);
			//logProgress(bytesRead, expectedSize, transferSpeed);
			totalTime += currentUpdate - lastUpdate;
			lastUpdate = currentUpdate;
	}

	if(close(fd) < 0) {
		perror(fileName);
		return FALSE;
	}

	// check if command return code is valid
	if (!receiveCommand(fd, "226")) {
		ERROR("received invalid response from server, expected 226 (TRANSFER_COMPLETE)");
	}

	return TRUE;
}

int sendPASV(int fd) {

	// SEND "PASV" COMMAND
	if (!sendCommand(fd, "PASV\r\n", strlen("PASV\r\n"))) {
		ERROR("sending PASV command to server failed!");
	}

	char* pasvResponse = receivePasv(fd);
	int remoteIP[PASV_ADDR_LENGTH];
	int remotePort[PASV_PORT_LENGTH];
	int responseLength = PASV_ADDR_LENGTH + PASV_PORT_LENGTH;

	if (pasvResponse == NULL) {
		ERROR("received invalid response from server, expected 221 (PASV)...");
	}

	if ((sscanf(pasvResponse, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
		&remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3], &remotePort[0], &remotePort[1])) < responseLength)
	{
		ERROR("generating remote address did not succeed");
	}

	ftp->remoteHostname = (char*) malloc(strlen(pasvResponse) + 1);

	// forming ip
	if ((sprintf(ftp->remoteHostname,
			"%d.%d.%d.%d",
			remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3])) < 0) {
		ERROR("generating remote address did not succeed");
	}

	// calculate new port
	ftp->remotePort = remotePort[0] * 256 + remotePort[1];

	if (!connectPassive()) {
		ERROR("connection to remote host failed!");
	}

	return TRUE;
}

static int action_quitConnection() {

	// SEND "QUIT" COMMAND
	if (!sendCommand(ftp->fdControl, "QUIT\r\n", strlen("QUIT\r\n"))) {
		ERROR("sending QUIT command to server failed!");
		return FALSE;
	}

	if (ftp->fdControl && close(ftp->fdControl) < 0) {
		perror("close()");
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////
//	   MAIN APPLCIATION		//
//////////////////////////////

int main(int argc, char** argv) {

	if (argc != 2) {
		printf("ERROR: you have entered an invalid number of arguments...\n");
		printf("USAGE: ./download ftp://[<username>:<password>@]<hostname>/<path>\n");
		return -1;
	}

	if (!parseURL(argv[1])) {
		printf("ERROR: you have entered an invalid number of arguments...\n");
		printf("USAGE: ./download ftp://[<username>:<password>@]<hostname>/<path>\n");
		return -1;
	}

	if (!connectSocket()) {
		perror("socket()");
		return -1;
	}

	// print server information
	debugURL();

	// print FTP connection information
	debugFTP();

	if (!receiveCommand(ftp->fdControl, SERVICE_READY)) {
		puts("ERROR: received invalid response from server, expected 220 (SERVICE_READY)");
		return -1;
	}

	if (!sendUSER(ftp->fdControl)) {
		return -1;
	}

	if (!sendPASS(ftp->fdControl)) {
		return -1;
	}

	if (!sendPASV(ftp->fdControl)) {
		return -1;
	}

	int userInput;

	puts("\n> PLEASE CHOOSE AN OPTION:");
	puts("(1) List directory\n(2) Download file\n(0) Exit\n");
	userInput = readInteger(1, 2);

	char* fileName = strrchr(argv[1], '/') + 1;

	if (fileName == '\0') {
		ERROR("server file path must not end with a slash ('/')");
	}

	if (userInput == 1) {
		action_listDirectory();
	}
	else if (userInput == 2) {
		action_retrieveFile(fileName);
	}

	return action_quitConnection();
}