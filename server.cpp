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
#define MAX_NO_RESPOND 40000
#define MAX_BACKLOG 10
#define PING "<PING>"
using namespace std;

const char* prompt_line = "server> ";

enum state_t {
	S_INIT,
	S_AUTH
};

struct user_info {
	user_info(string &n, const string &h, const string &p, time_t &t)
		: name(n), host(h), port(p), state(S_INIT), time(t) {}
	user_info() : name("<none>"), host("<none>"), port("-1"), state(S_INIT), time(0) {}
	string name;
	string host;
	string port;
	state_t state;
	time_t time;
};

map<int, user_info> user_map;
fd_set all_fds;


int update_users();
void close_connection(int fd)
{
	close(fd);
	FD_CLR(fd, &all_fds);
	user_map.erase(user_map.find(fd));
	update_users();
}
 
/** Reads the username, host and port from the client */
user_info* get_user_info(int fd)
{
	string line, n, h, p;
	stringstream ss(stringstream::in|stringstream::out);

	int rc = readln(fd, line);
	if (rc == -1) {
		perros("in get_user_info readln returned -1\n");
		close_connection(fd);
		return NULL;
	}
	ss << line;
	ss >> n >> h >> p;
	time_t t = time(&t);

	user_info* user = new user_info(n, h, p, t);
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
		 if (rc == -1) {
			 perros("writeln in send_user_list()");
			 close_connection(sfd);
			 return -1;
		 }
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
		cout << "\nadd user:" << user->name << " " 
							   << user->host << " " 
							   << user->port << endl << flush;
		prompt();
		return true;
	}
	else {
		return false;
	}
	return true;
}

/** Sends the current online user list to all users
 * or closes the connection with those who do not responde
 */
int update_users()
{
	int rc;
	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		rc = send_user_list(it->first);
	if (rc == -1){
		perros("send_user_list in update_users()");
		close_connection(it->first);
		return -1;
	}
	return 0;
}

/** Accepts a new connection from a client */
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
	user_info* user = new user_info();
	user = get_user_info(connfd);

	/**check if the user is correct
	 * and add it's fd to the set. */
	if (check_user(user)) {
		/* add the fd to the set */
		insert_fd(all_fds, fdmax, connfd);
		/* add the user to the map */
		user_map.insert(pair<int, user_info>(connfd, *user));
		int rc = writeln(connfd, "ACK");
		if ( rc == -1) {
			perros("writeln in accept_new_client()");
			close_connection(connfd);
			return -1;
		}
		update_users();
	}
	else {
		/* the user is already in use,
		 * so send a NACK to the client
		 * and close the socket*/
		int rc = writeln(connfd, "NACK");
		if ( rc == -1) {
			perros("writeln in accept_new_client()");
			close_connection(connfd);
			return -1;
		}
		return -1;
	}

	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int rc = getnameinfo( (sockaddr*) &addr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_NUMERICSERV);
	if (rc != 0) {
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
		return -1;
	}
	return 0;
}



/** Prints the userlist to the server console. */
void list_users()
{
	int i;
	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		cout << it->second.name << " "
			 << it->second.host << " "
			 << it->second.port << endl;
	if (i == 0)
		cout << "No users online." << endl;
}


/** Runs commands from the user */
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

/** Updates the time in the "user_info" "time" field */
int update_ping_response(int fd)
{
	time_t t;
	map<int, user_info>::iterator it;
	it = user_map.find(fd);
	if (it == user_map.end())
		return -1;
	it->second.time = time(&t);
	return 0;
}


int run_command_from_client(int fd)
{
	string line, name;
	map<int, user_info>::iterator it;
	int rc = readln(fd, line);
	if (rc == -1) {
		it = user_map.find(fd);
		if (it != user_map.end())
			name = it->second.name;
		cout << "\ndel user:" << name << endl << flush;
		prompt();
		perros("readln in run_command_from_client()");
		close_connection(fd);
		return -1;
	}
	if (line == "<RSP>")
		update_ping_response(fd);
	for (it = user_map.begin(); it != user_map.end(); ++it)
		/** If the user is new */
		if (it->second.state == S_INIT) {
			/** Send the ACK */
			rc = writeln(fd, "ACK");
			if (rc == -1) {
				perros("ACK in run_command_from_client()");
				close_connection(fd);
				return -1;
			}
			/** Send the user list */
			rc = send_user_list(it->first);
			if (rc == -1) {
				perros("send_user_list() in run_command_from_client()");
				close_connection(fd);
				return -1;
			}
			/** Set the state to authentificated */
			it->second.state = S_AUTH;
		}
		else {
			rc = send_user_list(it->first);
			if (rc == -1) {
				perros("send_user_list() in run_command_from_client()");
				close_connection(fd);
				return -1;
			}
		}
	return rc;
}

int ping_users()
{
    map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it) {
		int rc = writeln(it->first, PING);
		if (rc == -1) {
			perros("writeln() in ping_users()");
			return -1;
		}
	}
	return 0;
}


/** Remove the users that have not responded to the ping */
void remove_non_response(time_t curr_time, int max_no_respond)
{
	map<int, user_info>::iterator it;
	for (it = user_map.begin(); it != user_map.end(); ++it)
		if (difftime(curr_time, it->second.time) > max_no_respond) {
			/** Close the socket */
			close_connection(it->first);
			/** Remove the socket from the fd_set */
			FD_CLR(it->first, &all_fds);
			cout << "\n  -" << it->second.name << " is offline.\n" << flush;
			prompt();
		}
}


int main(int argc, char** argv)
{
	int sfd, lst;
	if (argc != 2) {
		fprintf(stderr, "ERR--------->Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	sfd = bind_to(NULL, argv[1]);

	lst = listen(sfd, MAX_BACKLOG);

	if (lst != 0) {
		fprintf(stderr, "Could not listen on speciffied socket\n");
		exit(EXIT_FAILURE);
	}
    int fdmax = 0;
	/** Initialize the set */
	FD_ZERO(&all_fds);
    /** stdin is selectable */
	insert_fd(all_fds, fdmax, STDIN_FILENO);
    /** the "listen" socket is added to the socket set */
	insert_fd(all_fds, fdmax, sfd);
	prompt();

	time_t start_time, curr_time;
	time(&start_time);
	for(;;) {
		time(&curr_time);
		if (difftime(curr_time, start_time) > PING_INTERVAL) {
			ping_users();
			remove_non_response(curr_time, start_time);
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
				int rc = accept_new_client(sfd, all_fds, fdmax);
				if (rc == -1) {
					perros("accept_new_client in main");
				}
			} else if (fd == STDIN_FILENO) {
				run_command_from_user(sfd);
			} else
				run_command_from_client(fd);
		}
	}
	return 0;
}

