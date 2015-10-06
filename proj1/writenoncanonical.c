#include "shared.h"

#define BAUDRATE B9600
#define MODEMDEVICE "/dev/ttyS1"
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

int sendFrame(unsigned char* buffer, int buffer_sz) {
	
	int i;
	int bytesWritten = 0;

	for (i = 0; i < buffer_sz; i++) {
		if (write(fd,&buffer[i],sizeof(unsigned char)) == 1) {
			printf("[OUT] sending packet: 0x%x\n", buffer[i]);
			bytesWritten++;
		}
	}

	return bytesWritten;
}

void generateSET(unsigned char buffer[5]) {
	buffer[0] = FLAG;
	buffer[1] = A_SET;
	buffer[2] = C_SET;
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

	int res;

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
	if (tcgetattr(fd, &oldtio) == -1) {
		perror("tcgetattr");
		exit(-1);
	}
	
	unsigned char SET[5];
	unsigned char frame[255];	

	struct sigaction signalAction;

	signalAction.sa_handler = SIGALRM_handler;
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = 0;

	if (sigaction(SIGALRM, &signalAction, NULL) < 0) {
		printf("ERRO: falha na instalacao do handler para o sinal SIGALRM.\n");
		exit(-1);
	}
	
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = OPOST;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]	= 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN]	 = 1;   /* blocking read until 5 chars received */

	tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	
	generateSET();

	while (alarmCounter < 4 && STOP == FALSE) {	
		
		if (alarmFlag) {
			alarm(3);
			alarmFlag = FALSE;
			state = START;
		}
				
		printf("%d bytes written\n", sendFrame(SET, 5));
		
		while (state != STOP_OK && state != RESEND)
		{
			switch(state)
			{
			case START:

				printf("[STATE] state changed to START\n");

				if (read(fd, &frame[0], sizeof(unsigned char)) > 1) {
					printf("[READ] received more than one symbol!\n");	
				}

				if (frame[0] == FLAG){
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

				if (frame[1] == UA_A){
					printf("[read value] recebi UA_A\n");
					state = A_RCV;
					printf("valor do state apos recepcao do A: %d\n",state);
				}
				else if (frame[1] == FLAG){
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

				if (frame[2] == C_UA){
					printf("[READ] received C_UA\n");
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
				
				if (frame[3] == (A_UA ^ C_UA) {
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
						
			if (state == STOP_OK) {
				STOP = TRUE;
			}
		}
	}

	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	
	close(fd);
	
	return 0;
}