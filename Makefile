.PHONY: all clean

# Build setup
CC = gcc
C_FLAGS = -Wall -pedantic -O0 -g -DDEBUG=1
L_FLAGS = -Wall -pedantic -O0 -g

# PHONY targets
all: acquired

clean:
	rm -f acquired *.o *.a *.so

# Object targets
%.o: %.c
	$(CC) $(C_FLAGS) -c -o $@ $<

# Binary targets
acquired: acquired.o flock.o
	$(CC) $(L_FLAGS) -o $@ $^