CC = gcc
CFLAGS = -g

HDRS = common.h
SRCS = common.c

all: server sensor

server: server.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o server server.c common.c

sensor: sensor.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o sensor sensor.c common.c

clean:
	rm -f server sensor *.o

.PHONY: all clean
