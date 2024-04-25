CC = gcc
CFLAGS = -std=gnu99 -Wall
LFLAGS = -pthread -lm

all: server client

server: server_main.c server.c server.h util.c util.h myperf.h Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

client: client_main.c client.c client.h util.c util.h myperf.h Makefile 
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

clean:
	rm -f server client
