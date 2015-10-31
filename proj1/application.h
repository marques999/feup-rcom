#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include "shared.h"

int application_init(char* port, int mode, char* filename);
int application_connect(int baudrate, int retries, int timeout, int maxsize);
int application_start(void);
int application_close(void);

#endif /* __APPLICATION_H_ */
