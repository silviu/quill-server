#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>

#define BUF_SIZE 10

using namespace std;

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

void insert_fd(fd_set &s, int &fdmax, int fd)
{
	if (fdmax < fd)
		fdmax = fd;
	FD_SET(fd, &s);
}
 
int accept_new_client(int sfd, fd_set &all_fds, int &fdmax)
{
	int newsfd;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	newsfd = accept(sfd, (sockaddr *)&addr, &len);
	if (newsfd == -1) {
		perror("accept()");
		return -1;
	}
	insert_fd(all_fds, fdmax, newsfd);
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int rc = getnameinfo( (sockaddr*) &addr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_NUMERICSERV);
	if (rc != 0) {
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
		return -1;
	}
	printf("new connection from: %s : %s\n", host, serv);
	return 0;
}

int run_command_from_user()
{
	return 0;
}

int run_command_from_client(int fd)
{
	char buf[BUF_SIZE];
	read(fd, &buf, BUF_SIZE);
	write(fd, &buf, BUF_SIZE);
	return 0;
}


void copy_fdset(fd_set &source, fd_set &dest)
{
	memcpy(&dest, &source, sizeof(struct addrinfo));
}


int readln(int fd)
{
	char* buf = (char*) malloc(sizeof(char) * BUF_SIZE);
	for(;;) {
		recv(fd, &buf, BUF_SIZE, MSG_PEEK);
		for (int i = 0; i < BUF_SIZE; i++)
			if ( buf[i] == '\n') {
				recv(fd, &buf, i, NULL);
				return i;
			}
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
	fd_set all_fds;
	FD_ZERO(&all_fds);
    /* stdin is selectable */
	insert_fd(all_fds, fdmax, 0);
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
				run_command_from_user();
			} else {
				run_command_from_client(fd);
			}
		}
	}
	return 0;
}
