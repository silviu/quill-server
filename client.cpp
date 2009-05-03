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
 

int main(int argc, char** argv)
{    
	if (argc != 4) {
		perror("main: Too few arguments. Usage: ./client username host port");
		exit(EXIT_FAILURE);
	}

    char* name = argv[1];
	char* host = argv[2];
	char* port = argv[3];

	/* Connect to the server. Get the dile descriptor. */
	int cfd = connect_to(host, port);
    
	string to_send;
	to_send.append(name); to_send.append(" ");
	to_send.append(host); to_send.append(" ");
	to_send.append(port);

    /* Send user info to the server */
	int rc = writeln(cfd, to_send);
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

