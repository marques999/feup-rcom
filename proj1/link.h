#ifndef __LINK_H_
#define __LINK_H_

#include "shared.h"

int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz);

unsigned char* generateDISC(int source);
unsigned char* generateREJ(int source, int nr);
unsigned char* generateRR(int source, int nr);
unsigned char* generateSET(int source);
unsigned char* generateUA(int source);

int sendDISC(int fd, int source);
int sendSET(int fd, int source);
int sendREJ(int fd, int source, int nr);
int sendRR(int fd, int source, int nr);
int sendUA(int fd, int source);

#endif /* __LINK_H_ */
