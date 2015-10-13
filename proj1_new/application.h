#ifndef __APPLICATION_H_
#define __APPLICATION_H_

typedef struct {
	int fd; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
} ApplicationLayer;

#endif /* __APPLICATION_H_ */