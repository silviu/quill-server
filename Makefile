CC = g++
CFLAGS = -Wall -g -lpthread
SERVER_PORT = 2222
RUN_SERVER = ./server $(SERVER_PORT)
RUN_CLIENT = ./client silviu localhost $(SERVER_PORT) download


all: build

build: server client

server: server.o common.o base64.o
	g++ -pthread server.o common.o base64.o -o server
client: client.o common.o base64.o

run_client: client
	$(RUN_CLIENT)

run_server: server
	$(RUN_SERVER)

dbg_server: server
	gdb --args $(RUN_SERVER)
dbg_client: client
	gdb --args $(RUN_CLIENT)

clean:
	rm -f server client *~ *.o *.swp

.PHONY: build all clean run_client run_server dbg_server dbg_client
