/*
 * client.c
 * Simple TCP/IP communication using sockets
 *
 * Sokratis Poutas <poutasok@gmail.com>
 * Aggelos Stais <aggelosstaisv@gmail.com>
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

/* Read from fd until a '\n' is read */
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

int main(int argc, char *argv[])
{
	int sd, port;
	pid_t reading_pid, writing_pid;
	char buf1[100], buf2[100];
	char *hostname;
	struct hostent *hp;
	struct sockaddr_in sa;

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
	if ( !(hp = gethostbyname(hostname))) {     ////
		printf("DNS lookup failed for host %s\n", hostname);
		exit(1);
	}

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port); //htons sets port in network byte order
	//copies server address from hp structure to sockaddr_in
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... "); fflush(stderr);
	//connect takes pointer to sockaddr_in struct after type casting it to sockaddr struct
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("connect");
		exit(1);
	}
	fprintf(stderr, "Connected.\n");

	/*
	 * Let the remote know we're not going to write anything else.
	 * Try removing the shutdown() call and see what happens.
	 
	//Should be put somewhere...find where....
	// πρέπει να χρησιμοποιηθεί εντος κάποιου interrupt handler
	// που θα δίνουμε ένα συνδυασμο πλήκτρων και θα μπαίνω στο handler
	if (shutdown(sd, SHUT_WR) < 0) {
		perror("shutdown");
		exit(1);
	}
	*/

	// Process reading incoming messages is created
	reading_pid = fork();
	if(reading_pid < 0){
		perror("fork error");
		exit(1);
	}
	//Reading process
	else if(reading_pid == 0){
		while(1){
			int bytes_num = read(sd, buf1, sizeof(buf1));
			if (bytes_num < 0) {
				perror("read");
				exit(1);
			}
			//if (bytes_num <= 0) //// 
			// Giati? An den exoun graftei bytes tha bgei ap to loop?
			// Kai an graftoun argotera?
				break;

			if (insist_write(1, buf1, bytes) != bytes) {
				perror("write");
				exit(1);
			}
			fprintf(stdout, "\n");
		}
		exit(0);
	}

	// Process writing outgoing messages is created
	writing_pid = fork();
	if(writing_pid < 0){
		perror("fork error");
		exit(1);
	}
	else if(writing_pid == 0){
		//writing process
		while(1){

			// Read stdin for the outgoing message
			int bytes_num = read_line(0, buf2);

			if (insist_write(sd, buf2, bytes) != bytes) {
				perror("write");
				exit(1);
			}
		}
		exit(0);
	}

	waitpid(-1, NULL, 0);
	waitpid(-1, NULL, 0);
	if (close(sd) < 0)
		perror("close");

	return 0;
}
