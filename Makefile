CFLAGS = -Wall -g
CC = gcc

all: server subscriber

server: server.o common.o list.o
	$(CC) -o $@ $^

subscriber: subscriber.o common.o
	$(CC) -o $@ $^

common.o: common.c
	$(CC) $(CFLAGS) -o $@ -c $<

list.o: list.c
	$(CC) $(CFLAGS) -o $@ -c $<

server.o: server.c
	$(CC) $(CFLAGS) -o $@ -c $<

subscriber.o: subscriber.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f server subscriber *.o