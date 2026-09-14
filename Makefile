CC = gcc
CFLAGS = -Wall -g -I include
LDFLAGS = -lpthread -lsqlite3

TARGETS = server recv

all: $(TARGETS)

server: src/main.c src/queue.c src/db.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

recv: client/multicast_recv.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)