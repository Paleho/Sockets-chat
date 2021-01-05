/*
 * server.c
 * Simple TCP/IP communication using sockets
 *
 * Sokratis Poutas <poutasok@gmail.com>
 * Aggelos Stais <>
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "socket-common.h"

#define DATA_SIZE	128

/* Convert a buffer to upercase */
void toupper_buf(char *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		buf[i] = toupper(buf[i]);
}

/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;

	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

ssize_t read_line(int fd, char * buf){
	ssize_t cnt = 0;
    int i;
	do{
		i = read(fd, buf + cnt, 1);
		cnt += i;
	}while(i == 1 && buf[cnt - 1] != '\n');

    buf[cnt - 1] = '\0';	// null termination

    return cnt;
}

int main(void)
{
	char buf1[DATA_SIZE], buf2[DATA_SIZE];
	char addrstr[INET_ADDRSTRLEN];
	int sd, part1, part2;
	pid_t pid1, pid2;
	socklen_t len;
	struct sockaddr_in sa;

	/* Make sure a broken connection doesn't kill us */
	signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		exit(1);
	}
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0) {
		perror("listen");
		exit(1);
	}

	fprintf(stderr, "Waiting for first incoming connection...\n");

	/* Accept first incoming connection */
	len = sizeof(struct sockaddr_in);
	if ((part1 = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
		perror("accept");
		exit(1);
	}
	if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
		perror("could not format IP address");
		exit(1);
	}
	fprintf(stderr, "Incoming connection from %s:%d\n",
		addrstr, ntohs(sa.sin_port));

	fprintf(stderr, "Waiting for second incoming connection...\n");

	/* Accept second incoming connection */
	len = sizeof(struct sockaddr_in);
	if ((part2 = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
		perror("accept");
		exit(1);
	}
	if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
		perror("could not format IP address");
		exit(1);
	}
	fprintf(stderr, "Incoming connection from %s:%d\n",
		addrstr, ntohs(sa.sin_port));

	//forking
	pid1 = fork();
	if(pid1 < 0){
		perror("fork error");
		exit(1);
	}
	else if(pid1 == 0){
		//child 1 process
		while(1){
			int amount = read(part1, buf1, sizeof(buf1));
			if (amount <= 0) {
				if (amount < 0)
					perror("read from remote peer failed");
				else
					fprintf(stderr, "Peer1 went away\n");
				break;
			}
			if (insist_write(part2, buf1, amount) != amount) {
				perror("write to remote peer failed");
				break;
			}
		}
		exit(0);
	}
	//parent process
	pid2 = fork();
	if(pid2 < 0){
		perror("fork error");
		exit(1);
	}
	else if(pid2 == 0){
		//child 2 process
		while(1){
			int amount = read(part2, buf2, sizeof(buf2));
			if (amount <= 0) {
				if (amount < 0)
					perror("read from remote peer failed");
				else
					fprintf(stderr, "Peer2 went away\n");
				break;
			}
			if (insist_write(part1, buf2, amount) != amount) {
				perror("write to remote peer failed");
				break;
			}
		}
		exit(0);
	}
	//parent process
	waitpid(-1, NULL, 0);
	waitpid(-1, NULL, 0);
	printf("Server -> parent exiting\n");
	/* Make sure we don't leak open files */
	if (close(part1) < 0)
		perror("close");
	if (close(part2) < 0)
		perror("close");
	if (close(sd) < 0)
		perror("close");

	return 0;
}
