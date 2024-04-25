CC = gcc
CFLAGS = -std=gnu99 -Wall
LFLAGS = -pthread -lm

all: server client myperf

server: server_main.c server.c server.h util.c util.h myperf.h Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

client: client_main.c client.c client.h util.c util.h myperf.h Makefile 
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

myperf: myperf.c sender.c receiver.c client.c util.c server.c
	$(CC) $(CFLAGS) -o $@ $^  $(LFLAGS)	

clean:
	rm -f server client myperf
