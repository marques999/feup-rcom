#ifndef __LINK_H_
#define __LINK_H_

#include "shared.h"

int llopen(char* port, int mode, int baudrate, int retries, int timeout, int maxsize);
int llclose(int fd);
int llread(int fd, unsigned char* buffer);
int llwrite(int fd, unsigned char* buffer, int length);

void logStatistics();

#endif /* __LINK_H_ */
