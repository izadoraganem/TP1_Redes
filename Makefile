CC = gcc
CFLAGS = -g

HDRS = common.h
SRCS = common.c

all: server sensor

server: server2.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o server server2.c common.c

sensor: sensor.c common.c $(HDRS)
	$(CC) $(CFLAGS) -o sensor sensor.c common.c

clean:
	rm -f server sensor *.o

.PHONY: all clean
