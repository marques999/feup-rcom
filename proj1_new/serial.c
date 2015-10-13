#include "shared.h"
#include "link.h"
#include "application.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1

int alarmFlag = TRUE;
int alarmCounter = 1;
int state = START;

void SIGALRM_handler() {
	printf("[timeout] after 3 seconds, #%d of 3\n", alarmCounter);
	alarmFlag = TRUE;
	state = RESEND;
	alarmCounter++;
}

static ApplicationLayer *app;

#define MAX_SIZE        20

static int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

static int sendData(unsigned char* data, int ns) {
	
    unsigned char I_FRAME[MAX_SIZE];
	unsigned char BCC2 = 0;
	unsigned numberBytes = sizeof(data) / sizeof(data[0]);
  
    I_FRAME[INDEX_FLAG_START] = FLAG;
	I_FRAME[INDEX_A] = A_SET;
	I_FRAME[INDEX_C] = ns << 5; 
	I_FRAME[INDEX_BCC1] = I_FRAME[INDEX_A] ^ I_FRAME[INDEX_C];
    
    int i = 4;
    int j;
    
    for (j = 0; j < numberBytes; j++) {
   
        unsigned char byte = data[j];
        
        BCC2 ^= byte;
        
        if (byte == FLAG || byte == ESCAPE) {
            I_FRAME[i++] = ESCAPE;
            I_FRAME[i++] = byte ^ 0x20;
        } else {
         	I_FRAME[i++] = byte;
        }    
    }
    
    I_FRAME[i++] = BCC2;
    I_FRAME[i++] = FLAG;
    
	/*for (j = 0; j < i; j++) {
        printf("0x%x\n", I_FRAME[j]);
    }*/
    
	return sendFrame(fd, I_FRAME, i);
}

/*
 * reads a variable length frame from the serial port
 * @param fd serial port file descriptor
 * @eturn "0" if read sucessful, less than 0 otherwise
 */
static int receiveData(int fd, unsigned char* out) {

	unsigned char data[MAX_SIZE];

	if (read(fd, &data[0], sizeof(unsigned char)) > 1) {
		printf("[READ] received more than one symbol!\n");	
	}

	// FIRST CHECK IF THE FRAME IS VALID (BEGINS WITH FLAG)
	if (data[0] != FLAG) {
		printf("[IN] receive failed: received invalid symbol, expected FLAG...\n");
		return -1;
	}

	int nBytes = 1;
	int readSuccessful = FALSE;

	// READ ALL BYTES UNTIL BUFFER IS EMPTY OR FLAG IS RECEIVED
	while (read(fd, &data[nBytes], sizeof(unsigned char)) == 1) {
		if (data[nBytes++] == FLAG) {
			readSuccessful = TRUE;
			break;
		}
	}

	// LOG ERROR MESSAGE AND RETURN IF LAST BYTE OF THE DATA FRAME WASN'T FOUND
	if (!readSuccessful) {
		puts("[READ] receive failed: couldn't find last byte of the data frame...");
		return -1;	
	}

	// LOG ERROR MESSAGE AND RETURN IF NOT ENOUGH BYTES WERE RECEIVED
	if (nBytes < 7) {
		puts("[READ] receive failed: not a data frame, must be at least 7 bytes long...");
		return -1;	
	}

	if (data[1] != A_SET) {
		puts("[READ] receive failed: received invalid symbol, expected A_SET...");
		return -1;	
    }

    if (data[2] != 0x01) {
		puts("[READ] receive failed: received invalid symbol, expected 0 or 1...");
		return -1;	
    }

	if (data[3] != (A_SET ^ 0x01) {
		puts("[READ] receive failed: wrong BCC checksum...");
		return -1;	
    }

    if (data[4] == FLAG) {
    	puts("[READ] receive failed: unexpected FLAG found in beginning of DATA");
    	return -1;
    }

	unsigned char BCC2 = 0;
    int j;
    int i = 0;

    // DESTUFF EVERY BYTE(S) INSIDE THE DATA PACKETS
    // CALCULATE BCC2 CHECKSUM FOR EACH SUCCESSFULLY DESTUFFED BYTE
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
    
    // CHECK CALCULATED RESULT AGAINST EXPECTED RESULT OF BCC2
    if (BCC2 != data[j++]) {
		printf("[READ] receive failed: wrong BCC2 checksum\n");
		return -1;
	}

	// DOUBLE CHECK IF THE LAST RECEIVED BYTE WAS A FLAG
	if (FLAG != data[j]) {
		printf("[READ] receive failed: received invalid symbol, expected FLAG...\n");
		return -1;
	}

	return nBytes;
}

int main(int argc, char** argv) {

    if (argc < 3 || !checkPath(argv[2])) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial r(eceive)/s(end) /dev/ttyS1\n");
		exit(1);
    }

    app = malloc(sizeof(applicationLayer));
    
    char* modeString = argv[1];

	unsigned char data = { 0x52, 0x2a, 0x46, 0x7e, 0x7e, 0x10 }

    stuffBytes(0);
    
    return 0;
	
    if (strcmp("r", modeString) == 0 || strcmp("receieve", modeString) == 0) {
		app->status = MODE_RECEIVER;
	}
	else if (strcmp("s", modeString) == 0 || strcmp("send", modeString) == 0) {
		app->status = MODE_TRANSMITTER;
	}
	else {
		printf("invalid mode '%s'...", argv[1]);
		return -1;
	}

	ll->fd = llopen(argv[2], mode);
	
	if (ll->fd < 0) {
		return -1;
	}

	if (tcsetattr(fd,TCSANOW,&ll->oldtio) == -1) {
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

    ll->fd = fd;

	/* save current port settings */
    if (tcgetattr(fd,&ll->oldtio) == -1) {
		perror("tcgetattr");
		return -1;
	}
	
	printf("[LOGGING] new termios structure set\n");
	link_initialize(port, fd, mode);

	if (mode == MODE_TRANSMITTER) {
		ret = llopen_TRANSMITTER(fd);
	}
	else if (mode == MODE_RECEIVER) {
		ret = llopen_RECEIVER(fd);
	} 
	else {
		return -1;
	}
	
	if (ret > 0) {
		printf("[INFO] connection established sucessfully!\n");
	} else {
		return -1;
	}
	
	return fd;
}

int llopen_RECEIVER(int fd) {
	 
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

    int bytesWritten = sendFrame(fd, generateUA(RECEIVER), 5);

    if (bytesWritten == S_LENGTH) {
    	printf("[OUT] sent response, %d bytes written\n", bytesWritten);
    }
    else {
    	return -1;
    }
  
  	return 0;
}

int llwrite() {

	int numberAttempt = 0;
	int frameSent = 0;

	while (!frameSent) {

		if (numberAttempt == 0 || alarmFlag) {
			
			alarmFlag = FALSE;

			if (numberAttempt >= ll->numTries) {
				alarm_stop();
				printf("ERROR: maximum number of retries exceeded, message not sent...\n");
				return 0;
			}

			sendMessage(fd, buf, bufSize);

			if (++numberAttempt == 1) {
				alarm_start();
			}
		}

		Message* receivedMessage = receiveMessage(fd);

		if (// received RR) {
			if (ll->ns != receivedMessage->nr)
				ll->ns = receivedMessage->nr;

			stopAlarm();
			frameSent = 1;
		} else if (// received REJ) {
			stopAlarm();
			numberAttempt = 0;
		}
	}

	stopAlarm();

	return 1;
}

int alarm_stop() {

	struct sigaction signalAction;

	signalAction.sa_handler = NULL;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = 0;

	if (sigaction(SIGALRM, &signalAction, NULL) < 0) {
		printf("ERRO: falha na instalacao do handler para o sinal SIGALRM.\n");
		return -1;
	}

	alarm(0);
}

int alarm_start() {

	struct sigaction signalAction;
	
	signalAction.sa_handler = SIGALRM_handler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = 0;

	if (sigaction(SIGALRM, &signalAction, NULL) < 0) {
		printf("ERROR: SIGALRM handler installation failed...\n");
		return -1;
	}

	alarm(ll->connectionTimeout);
}

int llopen_TRANSMITTER(int fd) {
	
	unsigned char frame[5];
	unsigned char* SET = generateSET(MODE_TRANSMITTER);

	alarm_start();
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