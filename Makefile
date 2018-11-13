.PHONY: all clean test_client

# Build setup
CC = gcc
C_FLAGS = -Wall -pedantic -O0 -g -DDEBUG=1 -std=gnu99
L_FLAGS = -Wall -pedantic -O0 -g

# PHONY targets
all: acquired client

clean:
	rm -f acquired *.o *.a *.so

# Object targets
%.o: %.c
	$(CC) $(C_FLAGS) -c -o $@ $<

# Binary targets
acquired: acquired.o flock.o threadpool.o
	$(CC) $(L_FLAGS) -pthread -o $@ $^

client: client.o
	$(CC) $(L_FLAGS) -o $@ $^

test_client:
	for i in $(shell seq 1 32); do \
		./client & \
	done
