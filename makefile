CC = gcc -Wall -g
LDFLAGS = -lpthread
GLIB2-FLAG = 'pkg-config --cflags glib-2.0'
GLIB2-LIBS = 'pkg-config --libs glib-2.0'

all: server client

server: server.c util.h util.c common.h server.h
	$(CC) $(GLIB2-FLAG) -o server server.c util.c $(GLIB2-LIBS) $(LDFLAGS)

client: client.c client.h util.h util.c common.h
	$(CC) $(GLIB2-FLAG) -o client client.c util.c $(GLIB2-LIBS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f client server
