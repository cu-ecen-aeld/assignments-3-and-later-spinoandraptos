# If CROSS_COMPILE not specified, then we use native gcc, else use specified cross compiler
CC ?= $(CROSS_COMPILE)gcc

# Compile with code warnings (-Wall) and debugging information (-g)
CFLAGS ?= -Wall -Werror -g

TARGET ?= aesdsocket

LDFLAGS ?= -pthread -lrt

OBJS ?= aesdsocket.c 

all: 
	${CC} $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)
	
# Use of phony to let makefile know clean target not associated with a file named "clean"
.PHONY : clean

# Clean target with forced removal of all .o files (*.o) and executables (*.elf)
clean:
	rm -f  *.o *.elf $(TARGET)
	
	

