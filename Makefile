CC = gcc
CFLAGS = -g

HDRS = common.h
SRCS = common.c

all: server client

server: server.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o server server.c common.c

client: client.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o client client.c common.c

clean:
	rm -f server client *.o

.PHONY: all clean
