#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include "shared.h"

typedef struct {
	int fd;
	int mode;
	char* fileName
} ApplicationLayer;

int alInitialize(char* fileName, int mode);
int alStart();

#endif /* __APPLICATION_H_ */
