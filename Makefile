CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: server client

server: server.c  padrao.h
	$(CC) $(CFLAGS) server.c  -o server

client: client.c  padrao.h
	$(CC) $(CFLAGS) client.c  -o client

clean:
	rm -f server client