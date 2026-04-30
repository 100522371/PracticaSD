CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: server

server: server.c
	$(CC) $(CFLAGS) server.c -o server

clean:
	rm -f server