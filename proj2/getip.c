#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define h_addr h_addr_list[0]

char* getIP(char* hostname) {

	struct hostent *h;

	if ((h=gethostbyname(argv[1])) == NULL) {
	    herror("gethostbyname");
	    return NULL;
	}

	printf("Host Name\t\t : %s\n", h->h_name);
	printf("IP Address\t\t : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));
	printf("IP Address Type\t : %d\n", h->h_addrtype);
	printf("IP Address Length\t : %d\n", h->h_length);

	return inet_ntoa(*((struct in_addr *)h->h_addr));
}