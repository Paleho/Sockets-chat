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
 #include <fcntl.h>

 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <sys/wait.h>
 #include <sys/ioctl.h>

 #include <crypto/cryptodev.h>
 #include "aes.h"

 #include <arpa/inet.h>
 #include <netinet/in.h>

 #include "socket-common.h"

 #define	KEY_SIZE	16
 #define 	DATA_SIZE	128

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

int aes_ctx_init(struct cryptodev_ctx* ctx, int cfd, const uint8_t *key, unsigned int key_size)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->cfd = cfd;

	ctx->sess.cipher = CRYPTO_AES_CBC;
	ctx->sess.keylen = key_size;
	ctx->sess.key = (void*)key;
	if (ioctl(ctx->cfd, CIOCGSESSION, &ctx->sess)) {
		perror("ioctl(CIOCGSESSION)");
		return -1;
	}

	return 0;
}

void aes_ctx_deinit(struct cryptodev_ctx* ctx)
{
	if (ioctl(ctx->cfd, CIOCFSESSION, &ctx->sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
	}
}

int
aes_encrypt(struct cryptodev_ctx* ctx, const void* iv, const void* plaintext, void* ciphertext, size_t size)
{
	struct crypt_op cryp;

	memset(&cryp, 0, sizeof(cryp));

	/* Encrypt data.in to data.encrypted */
	cryp.ses = ctx->sess.ses;
	cryp.len = size;
	cryp.src = (void*)plaintext;
	cryp.dst = ciphertext;
	cryp.iv = (void*)iv;
	cryp.op = COP_ENCRYPT;
	if (ioctl(ctx->cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return -1;
	}

	return 0;
}

int
aes_decrypt(struct cryptodev_ctx* ctx, const void* iv, const void* ciphertext, void* plaintext, size_t size)
{
	struct crypt_op cryp;

	memset(&cryp, 0, sizeof(cryp));

	/* Encrypt data.in to data.encrypted */
	cryp.ses = ctx->sess.ses;
	cryp.len = size;
	cryp.src = (void*)ciphertext;
	cryp.dst = plaintext;
	cryp.iv = (void*)iv;
	cryp.op = COP_DECRYPT;
	if (ioctl(ctx->cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char plaintext1[DATA_SIZE], plaintext2[DATA_SIZE];
	char iv1[AES_BLOCK_SIZE];
	uint8_t key1[KEY_SIZE] = { 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	struct cryptodev_ctx ctx;
	int sd, port, cfd;
	ssize_t n;
	char buf[DATA_SIZE];
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

	cfd = open("/dev/crypto", O_RDWR, 0);
	if (cfd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}
	aes_ctx_init(&ctx, cfd, key1, sizeof(key1));
	memset(iv1, 0x0, sizeof(iv1));

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

			memset(plaintext1, 0x0, DATA_SIZE);
			aes_decrypt(&ctx, iv1, buf, plaintext1, DATA_SIZE);

			int length;
			for (length = 0; length < DATA_SIZE; length++) {
				if(plaintext1[length] == '\0') break;
			}
			// Write incoming messages to stdout
			if (insist_write(1, plaintext1, length) != length) {
				perror("write");
				exit(1);
			}
			fprintf(stdout, "\n");
		}

		// Outgoing messages have been written in the keyboard.
		// Checking whether 0 is present in readfds, so stdin would be ready for reading
		if(FD_ISSET(0, &readfds)) {
			n = read_line(0, buf);
			if (n < 0) {
				perror("read");
				exit(1);
			}
			if (n <= 0) // No outgoing messages
				break;

			memset(plaintext2, 0x0, DATA_SIZE);
			aes_encrypt(&ctx, iv1, buf, plaintext2, DATA_SIZE);
			// Write outgoing messages to socket file
			if (insist_write(sd, plaintext2, DATA_SIZE) != DATA_SIZE) {
				perror("write");
				exit(1);
			}
		}

	}
	aes_ctx_deinit(&ctx);
	fprintf(stderr, "Communication Finished.\n");
	if (close(sd) < 0)
		perror("close");
	if (close(cfd)) {
		perror("close(cfd)");
		return 1;
	}
	return 0;
}