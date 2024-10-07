# If CROSS_COMPILE not specified, then we use native gcc, else use specified cross compiler
CC=$(CROSS_COMPILE)gcc

# Compile with code warnings (-Wall) and debugging information (-g)
C_FLAGS= -Wall -Werror -g

all: aesdsocket

# aesdsocket.o object file depends on aesdsocket.c and compiled with CC without linking, just object file (-c)
aesdsocket.o: aesdsocket.c
	$(CC) $(C_FLAGS) -c aesdsocket.c -o aesdsocket.o

# aesdsocket executable depends on aesdsocket.o and linked to libraries found from the root directory ("/") for referenced headers
aesdsocket: aesdsocket.o
	$(CC) $(C_FLAGS) -I/ aesdsocket.o -o aesdsocket
	
# Use of phony to let makefile know clean target not associated with a file named "clean"
.PHONY : clean

# Clean target with forced removal of all .o files (*.o) and executables (*.elf)
clean:
	rm -f  *.o *.elf aesdsocket
