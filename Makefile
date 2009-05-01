CFLAGS = -Wall -g

all: build run

build:
	g++ $(CFLAGS) server.cpp -o server

run: build
	./server 2222

clean:
	rm -f server *~ *.o

.PHONY: build clean run
