#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <crypto/cryptodev.h>

#define KEY_SIZE	16  /* AES128 */

/* Insist until all of the data has been read */
ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret < 0)
                        return ret;
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}

static int fill_urandom_buf(unsigned char *buf, size_t cnt)
{
        int crypto_fd;
        int ret = -1;

        crypto_fd = open("/dev/urandom", O_RDONLY);
        if (crypto_fd < 0)
                return crypto_fd;

        ret = insist_read(crypto_fd, buf, cnt);
        close(crypto_fd);

        return ret;
}

int main(int argc, char const *argv[]) {
    unsigned char key[KEY_SIZE];

    if (fill_urandom_buf(key, KEY_SIZE) < 0) {
		perror("getting data from /dev/urandom\n");
		return 1;
	}

    printf("\nKey:\n");
	for (int i = 0; i < KEY_SIZE; i++)
		printf("%x", key[i]);
	printf("\n\n");
    return 0;
}
