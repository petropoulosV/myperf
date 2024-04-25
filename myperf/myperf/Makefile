CC = gcc
CFLAGS = -std=gnu99 -Wall
LFLAGS = -pthread -lm

HEADER_DEPS = client.h util.h server.h myperf.h

all: server client myperf

server: server_main.c server.c util.c $(HEADER_DEPS) Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

client: client_main.c client.c util.c $(HEADER_DEPS) Makefile 
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

myperf: myperf.c sender.c receiver.c client.c util.c server.c $(HEADER_DEPS) Makefile
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LFLAGS)

clean:
	rm -f server client myperf
