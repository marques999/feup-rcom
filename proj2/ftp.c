#include "shared.h"

#define h_addr 					h_addr_list[0]
#define APPLICATION_DEBUG		0
#define ERROR(msg)				fprintf(stderr, "%s %s\n", "[ERROR]", msg); return FALSE
#define LOG(msg)				if (APPLICATION_DEBUG) puts(msg)
#define LOG_FORMAT(...)			if (APPLICATION_DEBUG) printf(__VA_ARGS__)

typedef struct {
	char* userName;
	char* userPassword;
	int fdControl;
	int fdData;
} FTPConnection;

typedef struct {
	char* serverHostname;
	char* serverPath;
	char* serverFile;
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

	if (ftp == NULL) {
		ERROR("system memory allocation failed!");
	}

	ftp->userName = NULL;
	ftp->userPassword = NULL;

	if (findSeparator != NULL) {

		userName[usernameSize] = '\0';
		userPassword = &userName[usernameSize + 1];

		if (strlen(&userName[usernameSize + 1]) <= 0) {
			ERROR("user has entered an invalid password!");
		}
	}

	if (strlen(userName) < 1) {
		ERROR("user has entered an invalid username!");
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

	if (url == NULL) {
		ERROR("system memory allocation failed!");
	}

	if (portLocation != NULL) {

		hostName[hostnameSize] = '\0';
		serverPort = atoi(&hostName[hostnameSize + 1]);

		if (serverPort <= 0) {
			ERROR("user has entered an invalid port!");
		}
	}

	if (strlen(hostName) < 1) {
		ERROR("user has entered an invalid hostname!");
	}

	url->serverHostname = hostName;
	url->serverPort = serverPort;

	return TRUE;
}

static int parseURL(char* userUrl) {

	if (strlen(userUrl) < 7 || strncmp(userUrl, "ftp://", 6)) {
		ERROR("user has specified an invalid protocol!");
	}

	char* ftpAddress = &userUrl[6];
	char* stringLocation = strchr(ftpAddress, '@');
	char* authenticationString;
	char anonymousLogin[] = "anonymous:anonymous";
	int authenticationSize = stringLocation - ftpAddress;

	if (stringLocation != NULL) {
		authenticationString = (char*) malloc(authenticationSize + 1);
		strncpy(authenticationString, ftpAddress, authenticationSize);
	}
	else {
		stringLocation = ftpAddress;
		authenticationString = (char*) malloc(strlen(anonymousLogin) + 1);
		strcpy(authenticationString, anonymousLogin);
	}

	parseLogin(authenticationString);

	char* pathLocation = strchr(stringLocation, '/');
	int hostnameSize;

	if (stringLocation[0] == '@') {
		stringLocation++;
	}

	if (pathLocation == NULL) {
		hostnameSize = strlen(stringLocation);
	}
	else {
		hostnameSize = pathLocation - stringLocation;
	}

	char* hostnameString = (char*) malloc(hostnameSize + 1);

	strncpy(hostnameString, stringLocation, hostnameSize);
	parseHostname(hostnameString);
	url->serverIP = getIP(url->serverHostname);

	if (url->serverIP == NULL) {
		return FALSE;
	}

	url->serverPath = NULL;
	url->serverFile = NULL;

	if (pathLocation == NULL) {
		return TRUE;
	}

	char* fileLocation = strrchr(stringLocation, '/');
	int pathSize = fileLocation - pathLocation;
	int filenameSize = strlen(fileLocation + 1);

	if (pathSize > 0) {
		url->serverPath = malloc(pathSize + 1);
		strncpy(url->serverPath, pathLocation + 1, pathSize);
	}
	else {
		puts("[INFORMATION] entering root directory...");
	}

	if (filenameSize > 0) {
		url->serverFile = malloc(filenameSize + 1);
		strncpy(url->serverFile, fileLocation + 1, filenameSize);
	}
	else {
		puts("[INFORMATION] user has specified a directory!");
	}

	return TRUE;
}

#define	PASV_READY		"227"
#define SERVICE_READY	"220"
#define	USER_OK			"331"
#define PASS_OK			"230"
#define DIRECTORY_OK	"250"
#define STATUS_OK		"150"

//////////////////////////////
//   FTP GENERIC COMMANDS	//
//////////////////////////////

static char* receiveCommand(int fd, const char* responseCmd) {

	char* responseMessage = (char*) malloc(MESSAGE_SIZE + 1);

	if (read(fd, responseMessage, MESSAGE_SIZE) <= 0) {
		return NULL;
	}

	if (strncmp(responseCmd, responseMessage, strlen(responseCmd))) {
		printf("[ERROR] %s", responseMessage);
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

static void debugFTP(void) {
	puts("+=======================================+");
	puts("|         CONNECTION STATISTICS         |");
	puts("+=======================================+");
	printf("| Hostname\t: %-21s |\n", url->serverHostname);
	printf("| IP Address\t: %-21s |\n", url->serverIP);
	printf("| Port\t\t: %-21d |\n", url->serverPort);
	printf("| Username\t: %-21s |\n", ftp->userName);
	printf("| Password\t: %-21s |\n", ftp->userPassword);
	puts("+=======================================+\n");
}

static int connectSocket(const char* hostName, int hostPort) {

	struct sockaddr_in server_addr;

	// SERVER ADDRESS HANDLING
	memset((void*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(hostName);
	server_addr.sin_port = htons(hostPort);

	// CONNECT TO TCP SOCKET
	int fdSock = socket(AF_INET, SOCK_STREAM, 0);

	if (fdSock < 0) {
		return -1;
	}

	// TRY TO ESTABLISH CONNECTION WITH THE REMOTE SERVER
	if (connect(fdSock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		return -1;
	}

	return fdSock;
}

//////////////////////////////
//   FTP LOGIN COMMANDS		//
//////////////////////////////

static int sendUSER(int fd) {

	char userCommand[MESSAGE_SIZE + 1];
	char passCommand[MESSAGE_SIZE + 1];
	int anonymousMode = FALSE;

	// FORMAT "USER" COMMAND ARGUMENTS
	if (ftp->userName == NULL || strcmp("anonymous", ftp->userName) == 0) {
		puts("[INFORMATION] entering anonymous mode...");
		sprintf(userCommand, "USER %s\r\n", "anonymous");
		anonymousMode = TRUE;
	}
	else {
		puts("[INFORMATION] entering authentication mode...");
		sprintf(userCommand, "USER %s\r\n", ftp->userName);
	}
	
	if (ftp->userPassword == NULL && !anonymousMode) {
		ERROR("user must enter a password");
	}

	// FORMAT "PASS" COMMAND ARGUMENTS
	sprintf(passCommand, "PASS %s\r\n", ftp->userPassword);

	// SEND "USER" COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending USER command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, USER_OK)) {
		ERROR("received invalid response from server, wrong username?...");
	}

	// SEND "PASS" COMMAND
	if (!sendCommand(ftp->fdControl, passCommand, strlen(passCommand))) {
		ERROR("sending PASS command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID (AUTHENTICATION MODE)
	if (!anonymousMode && !receiveCommand(ftp->fdControl, PASS_OK)) {
		ERROR("received invalid response from server, wrong password?");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID (ANOYMOUS MODE)
	if (anonymousMode && !receiveCommand(ftp->fdControl, "230")) {
		ERROR("received invalid response from server, no anonymous access?...");
	}

	return TRUE;
}

//////////////////////////////
//   FTP PASSIVE COMMANDS	//
//////////////////////////////

static int sendCWD(void) {

	char userCommand[MESSAGE_SIZE + 1];

	// FORMAT "CWD" COMMAND ARGUMENTS
	sprintf(userCommand, "CWD %s\r\n", url->serverPath);

	// SEND "CWD" (change working directory) COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending CWD command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, DIRECTORY_OK)) {
		ERROR("received invalid response from server, expected [150:STATUS_OK]...");
	}

	printf("[INFORMATION] entering directory %s...\n", url->serverPath);

	return TRUE;
}

int action_listDirectory(void) {

	// SWITCH TO CURRENT WORKING DIRECTORY
	if (url->serverPath != NULL && !sendCWD()) {
		return FALSE;
	}

	// SEND "LIST" COMMAND
	if (!sendCommand(ftp->fdControl, "LIST\r\n", strlen("LIST\r\n"))) {
		ERROR("sending LIST command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, STATUS_OK)) {
		ERROR("received invalid response from server, expected [150:STATUS_OK]...");
	}

	int bytesRead = 0;
	int bytesWritten = 0;
	char dataBuffer[1024];

	while ((bytesRead = read(ftp->fdData, dataBuffer, sizeof(dataBuffer))) > 0) {

		if ((bytesWritten = write(STDOUT_FILENO, dataBuffer, bytesRead)) < 0) {
			return FALSE;
		}
	}

	return bytesWritten;
}

int action_retrieveFile(void) {

	char userCommand[MESSAGE_SIZE + 1];

	// SWITCH TO CURRENT WORKING DIRECTORY
	if (url->serverPath != NULL && !sendCWD()) {
		return FALSE;
	}

	// FORMAT "RETR" COMMAND ARGUMENTS
	sprintf(userCommand, "RETR %s\r\n", url->serverFile);

	// SEND "RETR" COMMAND
	if (!sendCommand(ftp->fdControl, userCommand, strlen(userCommand))) {
		ERROR("sending RETR command to server failed!");
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	char* responseCommand = receiveCommand(ftp->fdControl, STATUS_OK);
	if (!responseCommand) {
		ERROR("received invalid response from server, expected [150:STATUS_OK]...");
	}

	char expectedFilename[PATH_MAX];
	unsigned fileSize = 0;

	if (sscanf(responseCommand, "150 Opening BINARY mode data connection for %s (%d bytes)", expectedFilename, &fileSize) < 2) {
		ERROR("received invalid response from server, file access denied?");
	}

	printf("[INFORMATION] created output file: %s\n", url->serverFile);
	printf("[INFORMATION] output file size: %d (bytes)\n", fileSize);
	puts("[INFORMATION] starting file transfer...");

	int fd = open(url->serverFile, O_WRONLY | O_CREAT, 0600);

	if (fd < 0) {
		perror(url->serverFile);
		return FALSE;
	}

	int bytesRead = 0;
	int bytesWritten = 0;
	int bytesSinceUpdate = 0;
	int length;
	char dataBuffer[SOCKET_SIZE];
	long long lastUpdate = getCurrentTime();
	long long totalTime = 0LL;
	long long currentUpdate;
	double transferSpeed;

	while ((length = read(ftp->fdData, dataBuffer, SOCKET_SIZE)) > 0) {

		if ((bytesWritten = write(fd, dataBuffer, length)) < 0) {
			ERROR("could not write data to output file!");
		}

		bytesRead += length;
		bytesSinceUpdate += length;
		currentUpdate = getCurrentTime();

		// o que fazer se o FTP não enviar o tamanho do ficheiro?
		if (currentUpdate - lastUpdate > 400) {
			transferSpeed = bytesSinceUpdate / (double) (currentUpdate - lastUpdate);
			logProgress(bytesRead, fileSize, transferSpeed);
			bytesSinceUpdate = 0;
			totalTime += currentUpdate - lastUpdate;
			lastUpdate = currentUpdate;
		}
	}

	if (close(fd) < 0) {
		perror(url->serverFile);
		return FALSE;
	}

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (!receiveCommand(ftp->fdControl, "226")) {
		ERROR("received invalid response from server, connection interrupted?");
	}

	// CHECK EXPECTED FILE SIZE
	if (fileSize != bytesRead) {
		ERROR("expected and received file sizes don't match!");
	}

	puts("[INFORMATION] file transfer completed successfully!");
	printf("[INFORMATION] TOTAL BYTES RECEIVED: %d bytes\n", bytesRead);
	printf("[INFORMATION] AVERAGE TRANSFER SPEED: %.2f kBytes/sec\n", (double) fileSize / totalTime);

	return TRUE;
}

static int sendPASV(int fd) {

	// SEND "PASV" COMMAND
	if (!sendCommand(fd, "PASV\r\n", strlen("PASV\r\n"))) {
		ERROR("sending PASV command to server failed!");
	}

	char* pasvResponse = receiveCommand(fd, PASV_READY);
	int remoteIP[4];
	int remotePort[2];

	// CHECK IF COMMAND RETURN CODE IS VALID
	if (pasvResponse == NULL) {
		ERROR("received invalid response from server, expected [221:PASV_READY]...");
	}

	if ((sscanf(pasvResponse, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &remoteIP[0], &remoteIP[1], &remoteIP[2], &remoteIP[3], &remotePort[0], &remotePort[1])) < 6) {
		ERROR("attempt to parse remote address and port failed, invalid format?");
	}

	// calculate new port
	char* pasvHostname = (char*) malloc(strlen(pasvResponse) + 1);
	int pasvPort = remotePort[0] * 256 + remotePort[1];

	// forming ip
	if ((sprintf(pasvHostname, "%d.%d.%d.%d", remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3])) < 0) {
		ERROR("attempt to generate remote IP address failed, invalid format?");
	}

	ftp->fdData = connectSocket(pasvHostname, pasvPort);

	if (ftp->fdData < 0) {
		ERROR("connection to remote host refused!");
	}

	return TRUE;
}

static int action_quitConnection() {

	// SEND "QUIT" COMMAND
	if (!sendCommand(ftp->fdControl, "QUIT\r\n", strlen("QUIT\r\n"))) {
		ERROR("sending QUIT command to server failed!");
	}

	if (ftp->fdData && close(ftp->fdData) < 0) {
		ERROR("%s connection problem: attempt to disconnect failed.\n");
	}

	if (ftp->fdControl && close(ftp->fdControl) < 0) {
		ERROR("%s connection problem: attempt to disconnect failed.\n");
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
		return -1;
	}

	ftp->fdControl = connectSocket(url->serverIP, url->serverPort);

	if (ftp->fdControl < 0) {
		perror("socket()");
		return -1;
	}

	// PRINT HOST AND CONNECTION INFORMATION
	debugFTP();

	if (!receiveCommand(ftp->fdControl, SERVICE_READY)) {
		ERROR("received invalid response from server, expected [220:SERVICE_READY]");
	}

	if (!sendUSER(ftp->fdControl)) {
		return -1;
	}

	if (!sendPASV(ftp->fdControl)) {
		return -1;
	}

	int userInput;

	if (url->serverFile == NULL) {
		action_listDirectory();
	}
	else {
		puts("\n> PLEASE CHOOSE AN OPTION:");
		puts("(1) List directory\n(2) Download file\n(0) Exit\n");
		userInput = readInteger(1, 2);

		if (userInput == 1) {
			action_listDirectory();
		}
		else if (userInput == 2) {
			action_retrieveFile();
		}
	}

	return action_quitConnection();
}