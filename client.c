/*
 * client.c
 * Simple TCP/IP communication using sockets
 *
 * Sokratis Poutas <poutasok@gmail.com>
 * Aggelos Stais <aggelosstaisv@gmail.com>
 * 
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

#include <arpa/inet.h>
#include <netinet/in.h>

#include "socket-common.h"

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

int main(int argc, char *argv[])
{
	int sd, port;
	ssize_t n;
	char buf[100];
	char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;
	int activity;
	fd_set readfds;	//set of file descriptors


	if (argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
		exit(1);
	}
	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	fprintf(stderr, "Created TCP socket\n");
	
	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname))) {
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");
	
	FD_ZERO(&readfds); // Intialization of readfds set
	
	/* Read answer and write it to standard output */
	for (;;) {
		
		// Add socket file descriptor and standard inpout in readfds set.
		FD_SET(sd, &readfds); 
		FD_SET(0, &readfds);

		// If any of the sockets is ready to be read (so content has been written)
		// select will wake up
		activity = select(sd+1, &readfds , NULL , NULL , NULL);
		
		if ((activity < 0) && (errno!=EINTR)){
            		printf("select error");
        	}

		// Incoming messages have been written in the socket by other clients.
		// Checking whether sd is present in readfds, so socket file descriptor would be ready for reading
		if(FD_ISSET(sd, &readfds)) {
			n = read(sd, buf, sizeof(buf));
			if (n < 0) {
				perror("read");
				exit(1);
			}
			if (n <= 0) // No incoming messages
				break;
			printf("Remote says:\n"); ////Change
			// Write incoming messages to stdout
			if (insist_write(1, buf, n) != n) {
				perror("write");
				exit(1);
			}
		}
		
		// Outgoing messages have been written in the keyboard.
		// Checking whether sd is present in readfds, so socket file descriptor would be ready for reading
		if(FD_ISSET(0, &readfds)) {			    
			n = read(0, buf, sizeof(buf));
			if (n < 0) {
				perror("read");
				exit(1);
			}
			if (n <= 0) // No outgoing messages
				break;
			
			// Write outgoing messages to socket file
			if (insist_write(sd, buf, n) != n) {
				perror("write");
				exit(1);
			}
		}

	}

	fprintf(stderr, "Communication Finished.\n");
	if (close(sd) < 0)
		perror("close");
	return 0;
}