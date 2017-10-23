CC=gcc
CFLAGS= 


SRC= webServer.c
 

all: $(SRC)
	$(CC) -o server.o $(SRC)

.PHONY : clean
clean :
	-rm -f *.o 
