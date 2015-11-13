#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"

int main(int argc, char** argv) {

	if (argc != 2) {
		printf("ERROR: you have entered an invalid number of arguments...\n");
		printf("USAGE: ./download ftp://[username:password]@hostname/path\n");
		return -1;
	}

	if (strncmp(argv[1], "ftp://", 6)) {
		printf("ERROR: user has specified an invalid protocol.\n");
		return -1;
	}

	char* ftpAddress = &argv[1][6];
	int userPasswordSize = (strchr(ftpAddress, '@') - ftpAddress);
	printf("%d\n", userPasswordSize);

	if (userPasswordSize <= 0) {
		printf("ERROR: user has not specified username and password.\n");
		return -1;
	}

	char* userPassword = malloc(userPasswordSize + 1);

	puts(ftpAddress);
	puts(userPassword);
	strncpy(userPassword, ftpAddress, userPasswordSize);

	int usernameSize = (int)(strchr(userPassword, ':') - userPassword);
	printf("%d\n", usernameSize);
	if (usernameSize <= 0) {
		printf("ERROR: you have entered an invalid username.\n");
		return -1;
	}

	char* username = malloc(usernameSize + 1);
	char* password = &userPassword[usernameSize + 1];

	if (strlen(password) <= 0) {
		printf("ERRROR: you have entered an invalid password.\n");
		return -1;
	}

	strncpy(username, userPassword, usernameSize);

/*	struct sockaddr_in server_addr;
	char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";

	//server address handling
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);	//32 bit Internet address network byte ordered
	server_addr.sin_port = htons(SERVER_PORT);		// server TCP port must be network byte ordered

	// open an TCP socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		perror("socket()");
		return -1;
	}

	// connect to the server
	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect()");
		return -1;
	}

	// send a string to the server
	printf("Bytes escritos %d\n", write(sockfd, buf, strlen(buf)));

	if (close(sockfd) < 0) {
		perror("close()");
		return -1;
	}
*/
	return 0;
}