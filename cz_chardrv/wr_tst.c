/*
 * wr_tst.c
 * Test the "sleepy" demo driver (slpy.c)
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 * License: MIT
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#define FLGS		O_RDWR
#define DMODE		0
#define SZ		1024

int main(int argc, char **argv)
{
	int fd;
	size_t n;
	char buf[SZ];

	if (argc == 1) {
		fprintf(stderr, "Usage: %s device_file\n", argv[0]);
		exit(1);
	}

	fd = open(argv[1], FLGS, DMODE);
	if (fd == -1)
		perror("open"), exit(1);

	for (n = 0; n < SZ; n++)
		buf[n] = '\0';
	// test writing..
	n = write(fd, buf, 1024);
	if (n == -1) {
		perror("write failed");
		close(fd);
		exit(1);
	}

	printf("sleepy:: wrote %d bytes\n", n);
	close(fd);
	exit(0);
}
