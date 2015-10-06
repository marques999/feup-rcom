<<<<<<< HEAD
#include "shared.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1

volatile int STOP = FALSE;

int sendFrame(unsigned char* buffer, int buffer_sz) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < buffer_sz; i++) {
		if (write(fd, &buffer[i], sizeof(unsigned char)) == 1) {
			printf("[OUT] sending packet: 0x%x\n", buffer[i]);
			bytesWritten++;
		}
	}

	return bytesWritten;
}

void generateUA(unsigned char buffer[5]) {
	buffer[0] = FLAG;
	buffer[1] = A_UA;
	buffer[2] = C_UA; 
	buffer[3] = buffer[1] ^ buffer[2];
	buffer[4] = FLAG;
}

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

int main(int argc, char** argv) {

    unsigned char frame[5];

    struct termios oldtio;
    struct termios newtio;

    if (argc < 2 || !checkPath(argv[1])) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
    }

	/*
	 * Open serial port device for reading and writing and not as controlling tty
	 * because we don't want to get killed if linenoise sends CTRL-C.
	 */
    int fd = open(argv[1], O_RDWR | O_NOCTTY);

	if (fd < 0) {
		perror(argv[1]); 
		exit(-1); 
    }

	/* save current port settings */
    if (tcgetattr(fd,&oldtio) == -1) {
		perror("tcgetattr");
		exit(-1);
	}

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

    tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
    }

    printf("[LOGGING] new termios structure set\n");

    int state = START;

	while (state != STOP_OK) 
	{
		switch(state) 
		{
		case START:

			printf("[STATE] state changed to START\n");

			if (read(fd, &frame[0], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[0] == FLAG) {
				printf("[READ] received FLAG\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
			}

			break;

		case FLAG_RCV:
			
			printf("[STATE] state changed to FLAG_RCV\n");

			if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[1] == A_SET) {
				printf("[READ] received A_SET\n");
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				printf("Estava em FLAG_RCV e recebi 0x7e e vou voltar para FLAG_RCV \n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
				state = START;
			}

			break;
		
		case A_RCV:

			printf("[STATE] state changed to A_RCV\n");

			if (read(fd, &frame[2], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[2] == C_SET) {
				printf("[READ] received C_SET\n");
				state = C_RCV;
			}
			else if(frame[2] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
				state = START;
			}

			break;
		
		case C_RCV:

			printf("[STATE] state changed to C_RCV\n");

			if (read(fd, &frame[3], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[3] == (A_SET ^ C_SET)) {
				printf("[READ] received correct BCC checksum!\n");
				state = BCC_OK;
			}
			else if (frame[3] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] wrong BCC checksum, returning to START...\n");
				state = START;
			}

			break;
		
		case BCC_OK:

			printf("[STATE] state changed to BCC_OK\n");

			if (read(fd, &frame[4], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");		
			}

			if (frame[4] == FLAG) {
				printf("[READ] received FLAG\n");
				state = STOP_OK;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");;
				state = START;
			}

			break;
		}
    }
	
	unsigned char UA[5];

    generateUA();
  	printf("[OUT] sent response, %d bytes written\n", sendFrame(UA, 5));
	printf("[END] connection established \n");
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);

    return 0;
}
=======
/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BAUDRATE B2400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define SYMBOL_A_SEND		0x03
#define SYMBOL_A_RECEIVE	0x01
#define SYMBOL_C_SET		0x07

#define S_A			1
#define S_C			2
#define S_BCC1		3

volatile int STOP=FALSE;

int alarmFlag = TRUE;
int alarmCounter = 1;

void alarmHandler()
{
	printf("alarme # %d\n", alarmCounter);
	alarmFlag = TRUE;
	alarmCounter++;
}

int main(int argc, char** argv)
{
    int fd, res;
    struct termios oldtio,newtio;
    char buf[255];
    
    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS4", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }
	
	unsigned char SET[5];
	unsigned char UA[5];
	
	UA[0] = SET[0] = FLAG;
	UA[2] = SET[2] = SYMBOL_C_SET;
	UA[4] = SET[4] = FLAG;
	
	UA[1] = SYMBOL_A_RECEIVE;
	SET[1] = SYMBOL_A_SEND;
	UA[3] = SYMBOL_A_RECIEVE ^ SYMBOL_C_UA;
	SET[3] = SYMBOL_A_SEND ^ SYMBOL_C_SET;

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	int i;
	int j = 0;
	char rsymbol;
	char recFrame[5];
	
	while (read(fd, recFrame, 5) == 5) {
		
		if (recFrame[S_BCC1] != recFrame[S_A] ^ recFrame[S_C]) {
			printf("frame error checking failed");
			continue;
		}
		
		if (recFrame[S_C] != C_SET) {
			printf("not a set command");
			continue;
		}
		
		if (write(fd, UA, 5) < 5) {
			printf("unnumbered acknowledge sendng failed");
		}
	}
	
    //for (i = 0; STOP==FALSE && i < 255; i++) {       /* loop for input */

		//if (read(fd,&rsymbol,1) > 0) {
			//j++;
		//}   /* returns after 1 char has been input */
	
		//printf("%c ", rsymbol);
		//if (buf[i]=='z') STOP=TRUE;
    //}

	//printf("\nread %d bytes\n", j);

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */


	
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
>>>>>>> origin/master
