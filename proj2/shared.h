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

/**
 * logs current file transfer progress on screen
 * @param current number of bytes transfered so far
 * @param total total number of bytes being transfered
 * @param speed current transfer speed (kBytes/sec)
 */
void logProgress(double current, double total, double speed);

/**
 * clears standard input buffer
 */
void clearBuffer();

/**
 * reads an integer value from standard input
 * @param start number lower bound (minimum value)
 * @param end number upper bound (maximum value)
 * @returns number entered by the user
 */
int readInteger(int start, int end);

/**
 * queries the current system time (in milliseconds)
 * @returns current time in milliseconds
 */
long long getCurrentTime();

/**
 * resolves IP address given a hostname
 * @param hostName remote server hostname
 * @returns string containing the server IP address (xx.xx.xx.xx)
 */
char* getIP(const char* hostName);

/**
 * reads a string from standard input
 * @returns string containing the user input
 */
char* readString();

#endif