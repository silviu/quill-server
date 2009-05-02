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

#define BUF_SIZE 10

using namespace std;


struct user_info {
	string name;
	string host;
	string port;
};


map<int, user_info> user_map;
map<int, user_info>::iterator it;


int readln(int fd, string & s);

/* Creates a socket. Binds to a port.
 * Returns a socket fd. */
int bind_thy_self(char* host, char* port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(host, port, &hints, &result);

	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;
		/* Make the server override the 120 sec standard timeout
		 * until it is allowed to reuse a certain port
		 */
		int on = 1;
		int rc = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if ( rc == -1)
			perror("setsockopt");

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;                  /* Success */

		close(sfd);
	}
	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);           /* No longer needed */
	return sfd;
}

void prompt()
{
	cout << "\nserver> "<<flush;
}

void insert_fd(fd_set &s, int &fdmax, int fd)
{
	if (fdmax < fd)
		fdmax = fd;
	FD_SET(fd, &s);
}


int get_user_info(int fd)
{
	string line;
	stringstream ss(stringstream::in|stringstream::out);
	user_info user;

	int rc = readln(fd, line);
	if (rc == -1) {
		fprintf(stderr, "in get_user_info readln returned -1\n");
		return -1;
	}
	ss << line;
	ss >> user.name >> user.host >> user.port;
	/*cout << "DBG: read user data '" << line << "'" << endl;
	cout << "DBG: -- name: " << user.name << " host: " << user.host
		 << " port: " << user.port << endl;*/
	user_map.insert(pair<int, user_info>(fd, user));
	return 0;
}


int accept_new_client(int sfd, fd_set &all_fds, int &fdmax)
{
	int connfd;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	connfd = accept(sfd, (sockaddr *)&addr, &len);
	if (connfd == -1) {
		perror("accept()");
		return -1;
	}
	//get user info "username host port"
	get_user_info(connfd);
	prompt();

	insert_fd(all_fds, fdmax, connfd);
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int rc = getnameinfo( (sockaddr*) &addr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_NUMERICSERV);
	if (rc != 0) {
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
		return -1;
	}
	//printf("new connection from: %s : %s\n", host, serv);
	return 0;
}


void copy_fdset(fd_set &source, fd_set &dest)
{
	memcpy(&dest, &source, sizeof(struct addrinfo));
}


/**
 * reads len characters from the socket into buf.
 * if the socket gets shutdown or a socket error occurs, return -1
 * else return len
 */
int readall(int fd, char * buf, size_t len)
{
	size_t remaining = len;
	while (remaining) {
		int rc = recv(fd, buf, remaining, 0);
		if (rc == -1) {
			perror("recv in readall");
			return -1;
		}
		if (rc == 0) {
			perror("recv: unexpected socket shutdown");
			return -1;
		}
		buf += rc;
		remaining -= rc;
	}
	return len;
}

/**
 * writes len characters to the socket
 */
int writeall(int fd, const char * buf, size_t len)
{
	size_t remaining = len;
	while (remaining) {
		int rc = send(fd, buf, remaining, 0);
		if (rc == -1) {
			perror("send in writeall");
			return -1;
		}
		buf += rc;
		remaining -= rc;
	}
	return len;
}

int writeall(int fd, const string & str)
{
	return writeall(fd, str.c_str(), str.length());
}

/**
 * adds characters to 'dest' until needle is found (inclusive)
 */
int read_to_char(int fd, string & dest, int needle)
{
	char buf[BUF_SIZE+1];
	bool found = false;
	while(!found) {
		int rc = recv(fd, buf, BUF_SIZE, MSG_PEEK);
		if (rc == -1) {
			perror("recv peek");
			return -1;
		}
		buf[rc] = '\0';
		//cout << "READ  " << rc << " chars :: " << buf << endl;
		char* pos = (char*) memchr(buf, needle, BUF_SIZE);
		int len = BUF_SIZE;
		if (pos != NULL) {
			len = pos - buf + 1;
			found = true;
		}

		rc = readall(fd, buf, len);
		if (rc == -1)
			return -1;
		dest.append(buf, rc);
	}
	return 0;
}

/**
 * reads a line from the socket 
 * (terminator == '\n', included in line)
 */
int readln(int fd, string & dest)
{
	return read_to_char(fd, dest, '\n');
}

int writeln(int fd, const string & s_)
{
	string s = s_;
	size_t len = s.length();
	if (len == 0)
		return writeall(fd, "\n");

	if (s[s.length()-1] != '\n')
		s += "\n";

	return writeall(fd, s);
}


void list_users()
{
	int i;
	for (it = user_map.begin(), i = 0; it != user_map.end(); ++it, i++)
		cout << "User #" << i << ": " << it->second.name << " "
			 << it->second.host << " " << it->second.port << endl;
}


int run_command_from_user(int sfd)
{
	string command;
	getline(cin, command);
	if( command == "list") {
		list_users();
		prompt();
	}
	if ( command == "quit") {
		close(sfd);
		exit(EXIT_SUCCESS);
	}
	return 0;
}

int run_command_from_client(int fd)
{
	string line;
	int rc = readln(fd, line);
	if (rc == -1) {
		return -1;
		//TODO: XXX: remove fd from the select() set
	}
	//cout << "DBG  :: read '" << line << "' from user" << endl;
    rc = writeln(fd, line);
	return rc;
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
	fd_set all_fds;
	FD_ZERO(&all_fds);
    /* stdin is selectable */
	insert_fd(all_fds, fdmax, STDIN_FILENO);
    /* the "listen" socket is added to the socket set */
	insert_fd(all_fds, fdmax, sfd);

	for(;;) {
		fd_set tmp_fds;
		int selected_fds = 0;
		copy_fdset(all_fds, tmp_fds);

		selected_fds = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);

		if (selected_fds == -1) {
			perror("ERROR in select");
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
				run_command_from_client(fd);
			}
		}
	}
	return 0;
}
