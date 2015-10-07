#include "shared.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1

int alarmFlag = TRUE;
int alarmCounter = 1;
int state = START;

volatile int STOP = FALSE;

void SIGALRM_handler() {
	printf("[timeout] after 3 seconds, #%d of 3\n", alarmCounter);
	alarmFlag = TRUE;
	state = RESEND;
	alarmCounter++;
}

int sendFrame(int fd, unsigned char* buffer) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < 5; i++) {
		if (write(fd, &buffer[i], sizeof(unsigned char)) == 1) {
			printf("[OUT] sending packet: 0x%x\n", buffer[i]);
			bytesWritten++;
		}
	}

	return bytesWritten;
}

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

struct termios oldtio;
struct termios newtio;

int main(int argc, char** argv) {

    if (argc < 3 || !checkPath(argv[2])) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial r(eceive)/s(end) /dev/ttyS1\n");
		exit(1);
    }
    
    char* modeString = argv[1];
	int mode = -1;
	
    if (strcmp("r", modeString) == 0 || strcmp("receieve", modeString) == 0) {
		mode = MODE_RECEIVER;
	}
	else if (strcmp("s", modeString) == 0 || strcmp("send", modeString) == 0) {
		mode = MODE_TRANSMITTER;
	}
	else {
		printf("invalid mode '%s'...", argv[1]);
	}

	int fd = llopen(argv[2], mode);
	
	if (fd < 0) {
		return -1;
	}
	
	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	
	close(fd);

    return 0;
}

int llopen(char* port, int mode) {

	/*
	 * Open serial port device for reading and writing and not as controlling tty
	 * because we don't want to get killed if linenoise sends CTRL-C.
	 */
	int ret;
    int fd = open(port, O_RDWR | O_NOCTTY);
    
	if (fd < 0) {
		perror(port); 
		return -1;
    }

	/* save current port settings */
    if (tcgetattr(fd,&oldtio) == -1) {
		perror("tcgetattr");
		return -1;
	}
	
	printf("[LOGGING] new termios structure set\n");

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
		return -1;
    }

	if (mode == MODE_TRANSMITTER) {
		ret = llopen_TRANSMITTER(fd);
	}
	else if (mode == MODE_RECEIVER) {
		ret = llopen_RECEIVER(fd);
	} 
	else {
		ret = -1;
	}
	
	if (ret > 0) {
		printf("[END] connection established sucessfully!\n");
	} else {
		return -1;
	}
	
	return fd;
}

int llopen_RECEIVER(int fd) {
	 
	state = START;

	unsigned char frame[5];
	unsigned char UA[5];
	
	while (state != STOP_OK) 
	{
		switch (state) 
		{
		case START:

			printf("[STATE] entering START state...\n");

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
			
			printf("[STATE] entering FLAG_RCV state...\n");

			if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
				printf("[READ] received more than one symbol!\n");	
			}

			if (frame[1] == A_SET) {
				printf("[READ] received A_SET\n");
				state = A_RCV;
			}
			else if (frame[1] == FLAG) {
				printf("[READ] received FLAG, returning to FLAG_RCV...\n");
				state = FLAG_RCV;
			}
			else {
				printf("[READ] received invalid symbol, returning to START...\n");
				state = START;
			}

			break;
		
		case A_RCV:

			printf("[STATE] entering A_RCV state...\n");

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

			printf("[STATE] entering C_RCV state...\n");

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

			printf("[STATE] entering BCC_OK state...\n");

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

	UA[0] = FLAG;
	UA[1] = A_UA;
	UA[2] = C_UA; 
	UA[3] = UA[1] ^ UA[2];
	UA[4] = FLAG;
  	printf("[OUT] sent response, %d bytes written\n", sendFrame(fd, UA));
  	
  	return 0;
}

int llopen_TRANSMITTER(int fd) {
	
	struct sigaction signalAction;
	
	unsigned char SET[5];
	unsigned char frame[5];

	signalAction.sa_handler = SIGALRM_handler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = 0;

	if (sigaction(SIGALRM, &signalAction, NULL) < 0) {
		printf("ERRO: falha na instalacao do handler para o sinal SIGALRM.\n");
		return -1;
	}
	
	SET[0] = FLAG;
	SET[1] = A_SET;
	SET[2] = C_SET;
	SET[3] = SET[1] ^ SET[2];
	SET[4] = FLAG;	
	state = START;
	
	while (alarmCounter < 4) {
		
		if (alarmFlag) {
			alarm(3);
			alarmFlag = FALSE;
			state = START;
		}
				
		printf("%d bytes written\n", sendFrame(fd, SET));
		
		while (state != STOP_OK && state != RESEND)
		{
			switch(state)
			{
			case START:

				printf("[STATE] entering START state...\n");

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
				
				printf("[STATE] entering FLAG_RCV state...\n");

				if (read(fd, &frame[1], sizeof(unsigned char)) > 1) {
					printf("[READ] received more than one symbol!\n");	
				}

				if (frame[1] == A_UA) {
					printf("[READ] received A_UA\n");
					state = A_RCV;
				}
				else if (frame[1] == FLAG) {
					printf("[READ] received FLAG, returning to FLAG_RCV...\n");
					state = FLAG_RCV;
				}
				else {
					printf("[READ] received invalid symbol, returning to START...\n");
					state = START;
				}

				break;
			
			case A_RCV:

				printf("[STATE] entering A_RCV state...\n");

				if (read(fd, &frame[2], sizeof(unsigned char)) > 1) {
					printf("[READ] received more than one symbol!\n");		
				}

				if (frame[2] == C_UA) {
					printf("[READ] received C_UA\n");
					state = C_RCV;
				}
				else if (frame[2] == FLAG) {
					printf("[READ] received FLAG, returning to FLAG_RCV...\n");
					state = FLAG_RCV;
				}
				else {
					printf("[READ] received invalid symbol, returning to START...\n");
					state = START;
				}

				break;
			
			case C_RCV:

				printf("[STATE] entering C_RCV state...\n");

				if (read(fd, &frame[3], sizeof(unsigned char)) > 1) {
					printf("[READ] received more than one symbol!\n");		
				}
				
				if (frame[3] == (A_UA ^ C_UA)) {
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

				printf("[STATE] entering BCC_OK state...\n");

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
						
			if (state == STOP_OK) {
				return 0;
			}
		}
	}
	
	printf("[END] connection failed after %d retries...", alarmCounter);
	
	return -1;
}
