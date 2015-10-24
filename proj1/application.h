#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include "shared.h"

typedef struct {
	int fd;
	int mode;
	int maxsize;
	char port[20];
	char* filename;
} ApplicationLayer;

int application_init(char* port, int mode, char* filename);
int application_config(int baudrate, int retries, int timeout, int maxsize);
int application_start(void);
int application_close(void);

#endif /* __APPLICATION_H_ */
