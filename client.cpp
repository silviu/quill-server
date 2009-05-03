#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <ctime>
#include <list>
#include "common.h"

#define MAX_BACKLOG 10
using namespace std;

const char* prompt_line = "client> ";

fd_set client_fds;
list<string> user_list;

int respond_to_ping(int fd)
{
	return 0;
}

void list_users()
{
	int i;
	list<string>::iterator it;
	for(it = user_list.begin(), i = 0; it != user_list.end(); ++it, i++)
		cout << "User #" << i << ": " << *it << flush;
}
 
int run_command_from_server(int fd)
{
	string line;
	int rc = readln(fd, line);
	if (rc == -1) {
		perros("readln in run_command_from_server()");
		return -1;
	}
	user_list.push_back(line);
	return 0;	
}
int run_command_from_user(int fd)
{
	string command;
	getline(cin, command);
	if (command == "")
		prompt();
	else if (command == "list") {
		list_users();
		prompt();
	}
	else if (command == "quit") {
		close(fd);
		exit(EXIT_SUCCESS);
	}
	else {
		cout << command << ": command not found" << endl;
		prompt();
	}
	return 0;
}

int bind_to_random_port(string &host, string & port)
{    
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);

	int bfd = bind_to(NULL, "0");
	int rc = getsockname(bfd, (sockaddr*)&addr, &len);
	if (rc == -1) {
		perros("getsockname in bind_to_random_port()");
		return -1;
	}
	char addr_host[NI_MAXHOST], addr_serv[NI_MAXSERV];
	rc = getnameinfo( (sockaddr*) &addr, len, addr_host, NI_MAXHOST, addr_serv, NI_MAXSERV, NI_NUMERICSERV|NI_NUMERICHOST);
	if (rc != 0) {
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
		return -1;
	}
	host.assign(addr_host);
	port.assign(addr_serv);
	return bfd;
}
 

int main(int argc, char** argv)
{    
	if (argc != 4) {
		perror("main: Too few arguments. Usage: ./client username host port");
		exit(EXIT_FAILURE);
	}

 	string host, port;
	int bfd = bind_to_random_port(host, port);
	if (bfd == -1) {
		perros("bind_to_random_port returned -1 in main");
		return -1;
	}

	int rc = listen(bfd, MAX_BACKLOG);
	if ( rc == -1) {
		perros("listen in main");
		return -1;
	}

    char* name = argv[1];
	char* server_host = argv[2];
	char* server_port = argv[3];

	/* Connect to the server. Get the dile descriptor. */
	int cfd = connect_to(server_host, server_port);
    
	string to_send;
	to_send.append(name); to_send.append(" ");
	to_send.append(host); to_send.append(" ");
	to_send.append(port);

    /* Send user info to the server */
	rc = writeln(cfd, to_send);
	if (rc == -1) {
		perros("writeln in main");
		close(cfd);
	}

	/* Wait for the respnde ACK/NACK */
	string response;
	rc = readln(cfd, response);

	if (rc == -1) {
		perror("recv: in readln while waiting for ACK/NACK\n");
		close(cfd);
		return -1;
	}

	cout << response << flush;
	if (response == "NACK") {
		perror("Username already in use.\n");
		close(cfd);
		return -1;
	}         

	

	int fdmax = 0;
	FD_ZERO(&client_fds);
    /* stdin is selectable */
	insert_fd(client_fds, fdmax, STDIN_FILENO);
    /* the "listen" socket is added to the socket set */
	insert_fd(client_fds, fdmax, bfd);
	/* the server connected fd is added to the socket set */
	insert_fd(client_fds, fdmax, cfd);
	prompt();
	for(;;) {
		fd_set tmp_fds;
		int selected_fds = 0;
		copy_fdset(client_fds, tmp_fds);

		selected_fds = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);

		if (selected_fds == -1) {
			perros("ERROR in select");
			exit(EXIT_FAILURE);
		}

		for(int fd = 0, i = 0; fd <= fdmax && i < selected_fds; fd++) {
			if (!FD_ISSET(fd, &tmp_fds))
				continue;
			i++;
			if (fd == cfd) {
				run_command_from_server(cfd);
			} else if (fd == STDIN_FILENO) {
				run_command_from_user(fd);
			}
		}
	}
	return 0;
}

