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
#include "common.h"
#define BUF_SIZE 10
#define PING_INTERVAL 4
#define MAX_NO_RESPOND 4

using namespace std;
const char* prompt_line = "server> ";
enum state_t {
	S_INIT,
	S_AUTH
};

struct user_info {
	string name;
	string host;
	string port;
	state_t state;
	time_t time;
};

map<int, user_info> user_map;
fd_set all_fds;

void insert_fd(fd_set &s, int &fdmax, int fd)
{
	if (fdmax < fd)
		fdmax = fd;
	FD_SET(fd, &s);
}

/** Reads the username, host and port from the client */
user_info* get_user_info(int fd)
{
	string line;
	stringstream ss(stringstream::in|stringstream::out);
	user_info* user = new(user_info);

	int rc = readln(fd, line);
	if (rc == -1) {
		fprintf(stderr, "in get_user_info readln returned -1\n");
		return NULL;
	}
	ss << line;
	ss >> user->name >> user->host >> user->port;
	user->state = S_INIT;
	time_t tm;
	user->time = time(&tm);
	/*cout << "DBG: read user data '" << line << "'" << endl;
	cout << "DBG: -- name: " << user.name << " host: " << user.host
		 << " port: " << user.port << endl;*/
	return user;
}

/** Checks if a user exists in the map */
bool user_exists(user_info user)
{

	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		if (it->second.name == user.name)
			return true;
	return false;
}

/** Sends the userlist to all clients. */
int send_user_list(int sfd)
{
	 map<int, user_info>::iterator it;
	 for (it = user_map.begin(); it != user_map.end(); ++it) {
		 string to_send;
		 to_send = it->second.name + " " +
				   it->second.host + " " +
				   it->second.port;
		 int rc = writeln(sfd, to_send);
		 if (rc == -1)
			 return -1;
	}
	return 0;
}

/** Checks if the username is to be accepted */
bool check_user(user_info* user)
{
	if (user == NULL){
		perros("user = NULL in accept_new_client()");
		return false;
	}
	if (!user_exists(*user)) {
		cout << "\n  +" << user->name << " is online.\n" << flush;
		prompt();
		return true;
	}
	else {
		return false;
	}
	return true;
}

int accept_new_client(int sfd, fd_set &all_fds, int &fdmax)
{
	int connfd;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	connfd = accept(sfd, (sockaddr *)&addr, &len);
	if (connfd == -1) {
		perros("accept()");
		return -1;
	}
	/**get user info "username host port" */
	user_info* user = new(user_info);
	user = get_user_info(connfd);

	/**check if the user is correct
	 * and add it's fd to the set. */
	if (check_user(user)) {
		/* add the fd to the set */
		insert_fd(all_fds, fdmax, connfd);
		/* add the user to the map */
		user_map.insert(pair<int, user_info>(connfd, *user));
	}
	else {
		/* the user is already in use,
		 * so send a NACK to the client
		 * and close the socket*/
		send(connfd, "NACK\n", 6, 0);
		close(connfd);
	}

	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int rc = getnameinfo( (sockaddr*) &addr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_NUMERICSERV);
	if (rc != 0) {
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
		return -1;
	}
	return 0;
}

void copy_fdset(fd_set &source, fd_set &dest)
{
	memcpy(&dest, &source, sizeof(struct addrinfo));
}


/** Prints the userlist to the server console. */
void list_users()
{
	int i;
	map<int, user_info>::iterator it;
	for (it = user_map.begin(), i = 0; it != user_map.end(); ++it, i++)
		cout << "User #" << i << ": " << it->second.name << " "
			 << it->second.host << " " << it->second.port << " "
			 << it->second.time << endl;
	if (i == 0)
		cout << "No users online." << endl;
}


int run_command_from_user(int sfd)
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
		close(sfd);
		exit(EXIT_SUCCESS);
	}
	else {
		cout << command << ": command not found" << endl;
		prompt();
	}
	return 0;
}

int update_users()
{
	int rc;
	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		rc = send_user_list(it->first);
	if (rc == -1)
		return -1;
	return 0;
}

int run_command_from_client(int fd)
{
	string line;
	map<int, user_info>::iterator it;
	int rc = readln(fd, line);
	if (rc == -1) {
		return -1;
	}
	for (it = user_map.begin(); it != user_map.end(); ++it)
		if (it->second.state == S_INIT) {
			rc = send(fd, "ACK\n", 5, 0);
			if (rc == -1) {
				perros("ACK in run_command_from_client()");
				return -1;
			}
			rc = send_user_list(it->first);
			if (rc == -1) {
				perros("send_user_list() in run_command_from_client()");
				return -1;
			}
			it->second.state = S_AUTH;
		}
		else {
			rc = send_user_list(it->first);
			if (rc == -1) {
				perros("send_user_list() in run_command_from_client()");
				return -1;
			}
		}
	return rc;
}

void ping_users(time_t curr_time, int max_no_respond)
{
	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		if (difftime(curr_time, it->second.time) > max_no_respond) {
			close(it->first);
			FD_CLR(it->first, &all_fds);
			cout << "\n  -" << it->second.name << " is offline.\n" << flush;
			prompt();
			user_map.erase(it);
			update_users();
		}
}


int main(int argc, char** argv)
{
	int sfd, lst;
	if (argc != 2) {
		fprintf(stderr, "ERR--------->Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	sfd = bind_thy_self(NULL, argv[1]);

	lst = listen(sfd, 10);

	if (lst != 0) {
		fprintf(stderr, "Could not listen on speciffied socket\n");
		exit(EXIT_FAILURE);
	}
    int fdmax = 0;
	FD_ZERO(&all_fds);
    /* stdin is selectable */
	insert_fd(all_fds, fdmax, STDIN_FILENO);
    /* the "listen" socket is added to the socket set */
	insert_fd(all_fds, fdmax, sfd);
	prompt();

	time_t start_time, curr_time;
	time(&start_time);
	for(;;) {
		time(&curr_time);
		if (difftime(curr_time, start_time) > PING_INTERVAL) {
			ping_users(curr_time, MAX_NO_RESPOND);
			time(&start_time);
		}
		fd_set tmp_fds;
		int selected_fds = 0;
		copy_fdset(all_fds, tmp_fds);

		selected_fds = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);

		if (selected_fds == -1) {
			perros("ERROR in select");
			exit(EXIT_FAILURE);
		}

		for(int fd = 0, i = 0; fd <= fdmax && i < selected_fds; fd++) {
			if (!FD_ISSET(fd, &tmp_fds))
				continue;
			i++;
			if (fd == sfd) {
				accept_new_client(sfd, all_fds, fdmax);
			} else if (fd == STDIN_FILENO) {
				run_command_from_user(sfd);
			} else {
				int rc = run_command_from_client(fd);
				if (rc == -1) {
					FD_CLR(fd, &all_fds);
					close(fd);
				}
			}
		}
	}
	return 0;
}
