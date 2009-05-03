CC = g++
CFLAGS = -Wall -g -Wextra -Weffc++
SERVER_PORT = 2222
RUN_SERVER = ./server $(SERVER_PORT)
RUN_CLIENT = ./client silviu localhost $(SERVER_PORT)

all: build

build: server client

server: server.o common.o
client: client.o common.o

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
