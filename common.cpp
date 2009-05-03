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
#include "common.h"

using namespace std;
#define BUF_SIZE 10

void prompt()
{
	cout << prompt_line << flush;
}

void perros(const char* s)
{
	cout << "\n" << flush;
	perror(s);
	prompt();
}

void insert_fd(fd_set &s, int &fdmax, int fd)
{
	if (fdmax < fd)
		fdmax = fd;
	FD_SET(fd, &s);
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
			perros("recv in readall");
			return -1;
		}
		if (rc == 0) {
			perros("recv: unexpected socket shutdown");
			return -1;
		}
		buf += rc;
		remaining -= rc;
	}
	return len;
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
			perros("recv peek");
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
 

/** Writes len characters to the socket. */
int writeall(int fd, const char * buf, size_t len)
{
	size_t remaining = len;
	while (remaining) {
		int rc = send(fd, buf, remaining, 0);
		if (rc == -1) {
			perros("send in writeall");
			return -1;
		}
		buf += rc;
		remaining -= rc;
	}
	return len;
}
/** Same as above, but accepts a "string" in place
 * of a "char*" as an argument.
 */
int writeall(int fd, const string & str)
{
	return writeall(fd, str.c_str(), str.length());
}

/** Appends a "\n" to the string that is to be sent.
 * Calls "writeall" to send the created line to the client.
 */
int writeln(int fd, const string & s_)
{
	string s = s_;
	size_t len = s.length();
	string nl = "\n";
	if (len == 0)
		return writeall(fd, nl);

	if (s[s.length()-1] != '\n')
		s += "\n";

	return writeall(fd, s);
}
 
/** Creates a socket. Binds to a port.
 * Returns a socket fd.
*/
static int connect_or_bind(int must_bind, char* host, char* port)
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
		/** Make the server override the 120 sec standard timeout
		 * until it is allowed to reuse a certain port
		 */
		int on = 1;
		int rc = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if ( rc == -1)
			perros("setsockopt");
		
        if (must_bind) {
			if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
				break;
		}
		else {
			if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
				break;
		}

		close(sfd);
	}
	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);           /* No longer needed */
	return sfd;
}

int bind_to(char* host, char* serv)
{
	return connect_or_bind(1, host, serv);
}

int connect_to(char* host, char* serv)
{
	return connect_or_bind(0, host, serv);
}
