# If CROSS_COMPILE not specified, then we use native gcc, else use specified cross compiler
CC=$(CROSS_COMPILE)gcc

# Compile with code warnings (-Wall) and debugging information (-g)
C_FLAGS= -Wall -g

# Default target depends on writer executable
all: writer

# Writer.o object file depends on writer.c and compiled with CC without linking, just object file (-c)
writer.o: writer.c
	$(CC) $(C_FLAGS) -c writer.c -o writer.o

# Writer executable depends on writer.o and linked to libraries found from the root directory ("/") for referenced headers
writer: writer.o
	$(CC) $(C_FLAGS) -I/ writer.o -o writer

# Use of phony to let makefile know clean target not associated with a file named "clean"
.PHONY: clean

# Clean target with forced removal of writer and all .o files (*.o) and executables (*.elf)
clean:
	rm -f writer *.o *.elf
