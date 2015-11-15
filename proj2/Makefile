CC= gcc
FLAGS= -O3 -Wall
EXECUTABLES= serial

all:
	$(CC) $(FLAGS) -o download shared.c ftp.c download.c

clean:
	rm -f $(EXECUTABLES)
