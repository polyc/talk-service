CC = gcc -Wall -g
LDFLAGS = -lpthread

all: server client

server: server.c util.h util.c common.h server.h
	$(CC) 'pkg-config --cflags glib-2.0' -o server server.c util.c 'pkg-config --libs glib-2.0' $(LDFLAGS)

client: client.c client.h util.h util.c common.h
	$(CC) 'pkg-config --cflags glib-2.0' -o client client.c util.c 'pkg-config --libs glib-2.0' $(LDFLAGS)

.PHONY: clean
clean:
	rm -f client server
