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
#include <vector>
#include "common.h"

#define MAX_BACKLOG 10
#define PROTO_START_MSG "msg from "
using namespace std;

const char* prompt_line = "client> ";

fd_set client_fds;
int fdmax = 0;

struct user_info {
	user_info(const string &h, const string &p, int &f) : host(h), port(p), fd(f) {}
	user_info() : host("<none>"), port("-1"), fd(-1) {}
	string host;
	string port;
	int fd;
	vector<string> msg;
};
 
map<string, user_info> user_list;


void list_users()
{
	int i;
	map<string, user_info>::iterator it;
	for(it = user_list.begin(), i = 0; it != user_list.end(); ++it, i++)
		cout << "User #" << i << ": " << it->first << " " 
			 << it->second.host <<  " " << it->second.port << " "<< it->second.fd << endl << flush;
}

int send_msg(user_info &user, string client_name, string msg)
{   
    string to_send; 
	string secv = PROTO_START_MSG;
	secv.append(client_name);
	to_send.append(secv);
	to_send.append(" ");
	to_send.append(msg);	

	int connfd = 0;
	if (user.fd == -1) {
		connfd = connect_to(user.host.c_str(), user.port.c_str());
		insert_fd(client_fds, fdmax, connfd);
	}
	else 
		connfd = user.fd;
	if (connfd == -1) {
		perros("connect_to() in send_msg()");
		return -1;
	}
	int rc = writeln(connfd, to_send);
	if (rc == -1) {
		perros("writeln() in send_msg()");
		return -1;
	}
	return 0;
}

int add_msg(string user, string msg)
{
	map<string, user_info>::iterator it;
	it = user_list.find(user);
	if ( it != user_list.end())
		it->second.msg.push_back(msg);
	return 0;
}

int read_msg(int fd)
{
	string line, what, from, who, msg;
	int rc = readln(fd, line);
	if (rc == -1) {
		perros("readln() in read_msg()");
		return -1;
	}
	stringstream ss (stringstream::in|stringstream::out);
	ss << line;
	ss >> what >> from >> who;
	if (what == "msg") {
		char* mesaj = (char*) malloc(line.size() - (what.size() + from.size() + who.size()));
		ss.getline(mesaj, (line.size() - (what.size() + from.size() + who.size())));
		msg = string(mesaj);
		add_msg(who, msg);
	}
	return 0;
}

int print_msg_nr()
{
	map<string, user_info>::iterator it;
	for (it = user_list.begin(); it != user_list.end(); ++it)
		cout << it->first << ": " << it->second.msg.size() << endl << flush;
	return 0;
}

int update_user_list(string user_bulk)
{    
	string h, p, name;
	map<string, user_info>::iterator it;
	stringstream ss(stringstream::in|stringstream::out);

	ss << user_bulk;
	ss >> name >> h >> p;
	int f = -1;

	user_info* user = new user_info(h, p, f);
	it = user_list.find(name);

	if (it == user_list.end())
		/* if the username is not in the map add it to the user_list */
		user_list.insert(pair<string, user_info>(name, *user));
	else if (( it->second.host != user->host ) || (it->second.port != user->port)) {
		/* if the username already was in the list, but the host/port changed */
		it->second.host = user->host;
		it->second.port = user->port;
		//it->second.fd = -1;
	}
	return 0;
}

 
int run_command_from_server(int fd)
{
	string line;
	int rc = readln(fd, line);
	if (rc == -1) {
		perros("readln in run_command_from_server()");
		return -1;
	}
    update_user_list(line);
	return 0;	
}


int break_command(string command, int comm, string &name, int &no, string &msg)
{
	string co, nr;
	stringstream ss(stringstream::in|stringstream::out);
	ss << command;

	if (comm == 1) { 
		ss >> co >> name >> nr;
		no = atoi(nr.c_str());
		return 0;
	}
	if (comm == 2) {
		ss >> co;
		return 0;
	}
	if (comm == 3) {
		ss >> co >> name;
		char* mesaj = (char*) malloc(command.size() - (name.size() + co.size()));
		ss.getline(mesaj, (command.size() - (name.size() + co.size())));
		msg = string(mesaj);
	} 
	return 0;
}


user_info* get_info_for_user(const string & name)
{
	map<string, user_info>::iterator it;
	it = user_list.find(name);
	if (it != user_list.end())
		return &it->second;
	return NULL;
}

int get_command_type(string command)
{
	string comm, user, no, msg;
	stringstream ss (stringstream::in|stringstream::out);
	ss << command;
	ss >> comm >> user >> no;
	if (comm == "read" && user != "" && no != "")
		return 1;
	else if (comm == "read" && user == "" && no == "")
		return 2;
	else if (comm == "send" && user != "" && no != "")
		return 3;
	return 0;
}


int print_specific_msg(string name, int no)
{
	map<string, user_info>::iterator it;
	it = user_list.find(name);
	if ( it == user_list.end()) {
		perros("The username you entered does not exist.");
		return -1;
	}
	if (no > it->second.msg.size()) {
		perros("message number requested bigger than available msgs number.");
		return -1;
	}
	cout << it->second.msg[no] << endl;
	
    vector<string>::iterator v_it;
	int i;
	for (v_it = it->second.msg.begin(), i = 0; v_it != it->second.msg.end(), i < no; ++v_it, ++i) {}
	it->second.msg.erase(v_it);

	return 0;
}

int run_command_from_user(int fd, string client_name)
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
		string name, msg;
		int no;
		int comm = get_command_type(command);
		break_command(command, comm, name, no, msg);
		
		if (comm == 3) {
			user_info* user = new user_info();
			user = get_info_for_user(name);
			int rc = send_msg(*user, client_name, msg);
			if (rc == -1) {
				perros("send_msg() in run_command_from_user()");
				return -1;
			}
			user->fd = rc;
			prompt();
		}
		else if (comm == 2) {
			print_msg_nr();
			prompt();
		}
		else if (comm == 1) {
			print_specific_msg(name, no - 1);
			prompt();
		}
		else {
			cout << command << ": command not found" << endl;
			prompt();
		}
	}
	return 0;
}

int accept_new_peer(int bfd, fd_set &client_fds, int &fdmax)
{
	int connfd;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	connfd = accept(bfd, (sockaddr *)&addr, &len);
	if (connfd == -1) {
		perros("accept()");
		return -1;
	}
	insert_fd(client_fds, fdmax, connfd);
   return connfd;	
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
    string client_name = string(name);
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
				run_command_from_user(fd, client_name);
			}
			else if (fd == bfd) {
				accept_new_peer(bfd, client_fds, fdmax);
			}
			else {
				read_msg(fd);
			}
		}
	}
	return 0;
}

