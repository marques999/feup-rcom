#include "shared.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1

int alarmFlag = TRUE;
int alarmCounter = 1;
int state = START;

volatile int STOP = FALSE;

void alarm_handler() {
	printf("[timeout] after 3 seconds, #%d of 3\n", alarmCounter);
	alarmFlag = TRUE;
	state = RESEND;
	alarmCounter++;
	alarm(3);
}

void alarm_start() {
	struct sigaction action;

	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarmFlag = FALSE;
	alarm(3);
}

void alarm_stop() {
	struct sigaction action;

	action.sa_handler = NULL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarm(0);
}

int sendFrame(int fd, unsigned char* buffer, unsigned buffer_sz) {
	
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

#define MAX_SIZE        20

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

int receiveRR(int fd, int ns) {

	unsigned char frame[5];

	state = START;

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

			if (frame[1] == 0x03) {
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

			if (frame[2] == (ns << 5)) {
				printf("[READ] received SEQUENCE\n");
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

			if (frame[3] == (0x03 ^ (ns << 5))) {
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

  	return 0;
}

#define TRANSMITTER 0
#define RECEIVER 1

unsigned char* generateResponse(int source, unsigned char C) {
	unsigned char* FRAME = malloc(sizeof(char) * 5);

	FRAME[0] = FLAG;

	if (source == TRANSMITTER) {
		FRAME[1] = 0x01;
	} else {
		FRAME[1] = 0x03;	
	}

	FRAME[2] = C;
	FRAME[3] = FRAME[1] ^ FRAME[2];
	FRAME[4] = FLAG;

	return FRAME;
}

unsigned char* generateRR(int source, int ns) {
	return generateResponse(source, ns << 5);
}

int sendRR(int fd, int source, int ns) {
	return sendFrame(fd, generateRR(source, ns), 5);
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
int llread(int fd, int ns, unsigned char* out) {

	unsigned char data[255];

	if (read(fd, &data[0], sizeof(unsigned char)) > 1) {
		printf("[READ] more than one byte received");
		return -1;
	}

	if (data[0] != FLAG) {
		printf("[IN] receive failed: recieved invalid symbol, expected FLAG...\n");
		return -1;
	}

	int nBytes = 1;
	int success = FALSE;

	while (read(fd, &data[nBytes], sizeof(unsigned char)) > 0) {
		if (data[nBytes++] == FLAG) {
			success = TRUE;
			break;
		}
	}

	if (!success) {
		printf("[READ] receive failed: unexpected end of frame\n");
		return -1;	
	}

	printf("number of bytes: %d\n", nBytes);
	if (nBytes < 7) {
		printf("[READ] receive failed: not a data packet, must be at least 7 bytes long\n");
		return -1;
	}

	if (data[1] != A_SET) {
		printf("[IN] receive failed: recieved invalid symbol, expected A_SET...\n");
		return -1;	
    }

    if (data[2] != (ns << 5)) {
		printf("[IN] receive failed: recieved invalid symbol, expected 0 or 1...\n");
		return -1;	
    }

	if (data[3] != (A_SET ^ (ns << 5))) {
		printf("[IN] receive failed: wrong BCC checksum...\n");
		return -1;	
    }

    if (data[4] == FLAG) {
    	printf("[IN] receive failed: unexpected FLAG found in beginning of DATA");
    	return -1;
    }

    unsigned char BCC2 = 0;
    int j;
    int i = 0;

for (j = 4; j < nBytes - 2; i++, j++) {
   
        unsigned char byte = data[j];
        unsigned char destuffed;
        
        if (byte == ESCAPE) {
			destuffed = data[j + 1] ^ 0x20;
			out[i] = destuffed;
			BCC2 ^= destuffed;
			j++;
        }
        else {
            	out[i] = byte;
		BCC2 ^= byte;
        }    
    }
    
    if (BCC2 != data[j++]) {
		printf("[IN] receive failed: wrong BCC2 checksum\n");
		return -1;
	}

	if (data[j] != FLAG) {
		printf("[IN] receive failed: recieved invalid symbol, expected FLAG...\n");
		return -1;
	}

	int k;
	for (k = 0; k < i; k++) {
		printf("received value: 0x%x\n", out[k]);	
	}

	sendRR(fd, RECEIVER, !ns);
    
	return nBytes;
}

int llwrite(int fd, int ns, unsigned char* data) {
	
    unsigned char I[2*MAX_SIZE+6];
	unsigned char BCC2 = 0;
    	
    I[0] = FLAG;
	I[1] = A_SET;
	I[2] = ns << 5; 
	I[3] = I[1] ^ I[2];
    
    int i = 4;
    int j = 0;
    
    for (j = 0; j < 6; j++) {
   
        unsigned char byte = data[j];
        
        BCC2 ^= byte;
        
        if (byte == FLAG || byte == ESCAPE) {
            I[i++] = ESCAPE;
            I[i++] = byte ^ 0x20;
        }
        else {
            I[i++] = byte;
        }    
    }
    
    I[i++] = BCC2;
    I[i++] = FLAG;
    
    for (j = 0; j < i; j++) {
        printf("0x%x\n", I[j]);
    }

	int frameSent = FALSE;    
	
	alarm_start();
	printf("READING RESPONSE...\n");

	while (!frameSent) {
		sendFrame(fd, I, i);
		
		if (receiveRR(fd, !ns) == 0) {
			frameSent = TRUE;
		}
	}

	printf("RR RECEIVED\n");
	alarm_stop();

	return i;
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

	int ns = 1;

	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	unsigned char out[255];
	unsigned char data[] = { 0x52, 0x2a, 0x46, 0x7f, 0x7e, 0x10 };

    if (mode == MODE_TRANSMITTER) {
		llwrite(fd, ns, data);
	} else {
		llread(fd, ns, out);
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
	
	if (ret >= 0) {
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
  	printf("[OUT] sent response, %d bytes written\n", sendFrame(fd, UA, 5));
  	
  	return 0;
}

int llopen_TRANSMITTER(int fd) {
	
	struct sigaction signalAction;
	
	unsigned char SET[5];
	unsigned char frame[5];

	signalAction.sa_handler = alarm_handler;
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
				
		printf("%d bytes written\n", sendFrame(fd, SET, 5));
		
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
