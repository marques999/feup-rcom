#ifndef __SHARED_H_
#define __SHARED_H_

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BOOL	int
#define FALSE	0
#define TRUE	1

#define SOCKET_SIZE		32768
#define MESSAGE_SIZE	1024
/*
 * logs current file transfer progress on screen
 * @param current number of bytes transfered so far
 * @param total total number of bytes being transfered
 * @param speed current transfer speed (kBytes/sec)
 */
void logProgress(double current, double total, double speed);

/*
 * reads an integer value from stdin
 * @param start number lower bound (minimum value)
 * @param end number upper bound (maximum value)
 * @returns number containing the user input
 */
int readInteger(int start, int end);

long long getCurrentTime();

char* getIP(const char* hostName);

/*
 * reads a string from stdin
 * @returns string containing the user input
 */
char* readString();

#endif
