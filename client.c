/*
 * client.c
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
#define DATA_SIZE	128

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
	pid_t reading_pid, writing_pid;
	char buf1[DATA_SIZE], buf2[DATA_SIZE];
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
	printf("before memset\n");
	aes_ctx_init(&ctx, cfd, key1, sizeof(key1));
	memset(plaintext1, 0x0, DATA_SIZE);
	memset(plaintext2, 0x0, DATA_SIZE);
	memset(iv1, 0x0, sizeof(iv1));

	reading_pid = fork();
	if(reading_pid < 0){
		perror("fork error");
		exit(1);
	}
	else if(reading_pid == 0){
		//reading process
		while(1){
			//read incoming
			int amount = read(sd, buf1, sizeof(buf1));
			if (amount < 0) {
				perror("read");
				exit(1);
			}
			if (amount <= 0)
				break;

			memset(plaintext1, 0x0, DATA_SIZE);
			aes_decrypt(&ctx, iv1, buf1, plaintext1, DATA_SIZE);
			int length;
			for (length = 0; length < DATA_SIZE; length++) {
				if(plaintext1[length] == '\0') break;
			}
			if (insist_write(1, plaintext1, length) != length) {
				perror("write");
				exit(1);
			}
			fprintf(stdout, "\n");
		}
		exit(0);
	}

	writing_pid = fork();
	if(writing_pid < 0){
		perror("fork error");
		exit(1);
	}
	else if(writing_pid == 0){
		//writing process
		while(1){
			//read stdin
			read_line(0, buf2);

			memset(plaintext2, 0x0, DATA_SIZE);
			aes_encrypt(&ctx, iv1, buf2, plaintext2, DATA_SIZE);

			if (insist_write(sd, plaintext2, DATA_SIZE) != DATA_SIZE) {
				perror("write");
				exit(1);
			}
		}
		exit(0);
	}

	waitpid(-1, NULL, 0);
	waitpid(-1, NULL, 0);
	aes_ctx_deinit(&ctx);
	if (close(sd) < 0)
		perror("close");
	if (close(cfd)) {
		perror("close(cfd)");
		return 1;
	}

	return 0;
}
