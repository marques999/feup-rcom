#include "link.h"

#define _POSIX_SOURCE 1

int checkPath(char* ttyPath) {
	return strcmp("/dev/ttyS0", ttyPath) == 0 ||
			strcmp("/dev/ttyS1", ttyPath) == 0 ||
			strcmp("/dev/ttyS4", ttyPath) == 0 ||
			strcmp("/dev/ttyS5", ttyPath) == 0;
}

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

	llInitialize(argv[2], 0, mode);
	
	int fd = llopen(argv[2], mode);
	
	if (fd < 0) {
		return -1;
	}
	
	unsigned char out[255];
	unsigned char data[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!' };

    if (mode == MODE_TRANSMITTER) {
		llwrite(fd, data, sizeof(data)/sizeof(data[0]));
	} else {
		llread(fd, out);
	}

	llclose(fd, mode);

    return 0;
}