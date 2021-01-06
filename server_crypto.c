/*
 * server.c
 * Simple TCP/IP communication using sockets
 *
 * Sokratis Poutas <poutasok@gmail.com>
 * Aggelos Stais <aggelosstaisv@gmail.com>
 * 
 */

/* Επιτρέπει την επικοινωνία μεταξύ 2 πελατών.
 * Τα μηνύματα εμφανίζονται στο τερματικό του server και των συνδεδεμένων πελατών.
 * 
 * Μπορούν μέχρι ακόμη 3 πελάτες να περιμένουν στην ουρά αναμονής για σύνδεση και να γράφουν
 * τα μηνύματα τους στο stdin, χωρίς φυσικά να εμφανίζονται στη συνομιλία.
 * Μόλις ο πελάτης αποσυνδεθεί ο server αποδέχεται τον επόμενο πελάτη και εμφανίζει τα μηνύματα του.
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>

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

int main(void)
{
	char buf[128];
	char addrstr[INET_ADDRSTRLEN],addrstr2[INET_ADDRSTRLEN];
	int sd,client1_fd,client2_fd,port1,port2,activity,max;
	ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;
	fd_set readfds;	 //set of file descriptors

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
	sa.sin_addr.s_addr = htonl(INADDR_ANY);		//we don't need to bind a socket to a specific IP
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

	// Intialization of client fds
	client1_fd=0;
	client2_fd=0;

	/* Loop forever, accepting connections */
	for (;;) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept first incoming connection */
		while(client1_fd==0){
				len = sizeof(struct sockaddr_in);
				if ((client1_fd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
					perror("accept");
					exit(1);
				}
				if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
					perror("could not format IP address");
					exit(1);
				}
				fprintf(stderr, "Incoming connection from %s:%d\n",
					addrstr, ntohs(sa.sin_port));
				
				// Port of the first client connected
				port1=ntohs(sa.sin_port);
			}
			
			/* Accept second incoming connection */
			while(client2_fd==0){
				if ((client2_fd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
					perror("accept");
					exit(1);
				}
				if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr2, sizeof(addrstr2))) {
					perror("could not format IP address");
					exit(1);
				}
				fprintf(stderr, "Incoming connection from %s:%d\n",
					addrstr, ntohs(sa.sin_port));
				
				// Port of the first client connected
				port2=ntohs(sa.sin_port);
			}


		// Intialization of readfds set
		FD_ZERO(&readfds); 
		
		/* We break out of the loop when one remote peer goes away */
		for (;;) {
			
			// Add client socket file descriptors in readfds set.
			// Should be done in every recursion because we don't know which one was removed
			FD_SET(client1_fd, &readfds);
			FD_SET(client2_fd, &readfds);

					
			if(client1_fd<client2_fd) 
				max=client2_fd;
			else
				max=client1_fd;

			// If any of the clients is ready to be read (so content has been written)
			// select will wake up
			activity = select(max+1, &readfds , NULL , NULL , NULL);
			if ((activity < 0) && (errno!=EINTR))
            	printf("select error");


			// Incoming messages have been written in the socket by first client
			// Checking whether client1_fd is present in readfds, so socket file descriptor would be ready for reading
			if(FD_ISSET(client1_fd, &readfds)) {
				n = read(client1_fd, buf, sizeof(buf));
				if (n < 0) {
					perror("read");
					exit(1);
				}
				if (n <= 0){ // Client 1 exited
					if (close(client1_fd) < 0)
						perror("close");
					fprintf(stderr,"Client %s:%d exited.\n",addrstr,port1);
					client1_fd=0;
					break;
				}

				fprintf(stderr,"Client %s:%d says:\n",addrstr,port1);
				// Write incoming messages to stdout
				if (insist_write(1, buf, n) != n) {
					perror("write");
					exit(1);
				}
				// Write incoming message to other client socket file
				if (insist_write(client2_fd,buf, n) != n) {
					perror("write");
					exit(1);
				}
			}
			
			// Incoming messages have been written in the socket by second client
			// Checking whether client2_fd is present in readfds, so socket file descriptor would be ready for reading
			if(FD_ISSET(client2_fd, &readfds)) {
				n = read(client2_fd, buf, sizeof(buf));
				if (n < 0) {
					perror("read");
					exit(1);
				}
				if (n <= 0) { //Client 2 exited
					if (close(client2_fd) < 0)
						perror("close");
					fprintf(stderr,"Client %s:%d exited.\n",addrstr2,port2);
					client2_fd=0;
					break;
				}

				fprintf(stderr,"Client %s:%d says:\n",addrstr2,port2);
				// Write incoming messages to stdout
				if (insist_write(1, buf, n) != n) {
					perror("write");
					exit(1);
				}
				// Write incoming message to other client socket file
				if (insist_write(client1_fd,buf, n) != n) {
					perror("write");
					exit(1);
				}
			}
		}
		
	}

	/* Make sure we don't leak open files */
	if (close(client1_fd) < 0)
		perror("close");
	if (close(client2_fd) < 0)
		perror("close");
	if (close(sd) < 0)
		perror("close");
	
	return 1;
}