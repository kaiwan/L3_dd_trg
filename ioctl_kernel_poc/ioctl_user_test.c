/*
 * ioctl_user_test.c
 * Test ioctl_kdrv a bit -- /dev/ioctlkdrv
 * Note that the user (as root) must create the device file.
 *
 *TODO - use inlines..
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include "ioctl_kdrv.h"

#define FLGS		O_RDWR
#define DMODE		0

int main(int argc, char **argv)
{
	int fd, power;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s device_file\n\
  If device_file does not exist, create it using mknod(1) (as root)\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], FLGS, DMODE)) == -1)
		perror("open"), exit(1);
	printf("device opened: fd=%d\n", fd);

	printf("IOCTL_KDRV_IOCRESET = 0x%x IOCTL_KDRV_IOCQPOWER= 0x%x IOCTL_KDRV_IOCSPOWER=0x%x\n",
	     IOCTL_KDRV_IOCRESET, (unsigned int)IOCTL_KDRV_IOCQPOWER, 
		 (unsigned int)IOCTL_KDRV_IOCSPOWER);

	// test ioctl's ...
	if (ioctl(fd, IOCTL_KDRV_IOCRESET, 0) == -1) {
		perror("ioctl IOCTL_KDRV_IOCRESET failed");
		close(fd);
		exit(1);
	}
	printf("%s: device reset.\n", argv[0]);

	if (ioctl(fd, IOCTL_KDRV_IOCQPOWER, &power) == -1) {
		perror("ioctl IOCTL_KDRV_IOCQPOWER failed");
		close(fd);
		exit(1);
	}
	printf("%s: power=%d\n", argv[0], power);

	if (!power) {
		printf("Device OFF, powering it on in 3s...\n");
		sleep(3);	/* yes, careful here of sleep & signals! */
		if (ioctl(fd, IOCTL_KDRV_IOCSPOWER, 1) == -1) {
			perror("ioctl IOCTL_KDRV_IOCSPOWER failed");
			close(fd);
			exit(1);
		}
		printf("%s: power ON now.\n", argv[0]);
	} else {
		printf("%s: powering OFF now..\n", argv[0]);
		if (ioctl(fd, IOCTL_KDRV_IOCSPOWER, 0) == -1) {
			perror("ioctl IOCTL_KDRV_IOCSPOWER failed");
			close(fd);
			exit(1);
		}
		printf("%s: power OFF ok, exiting..\n", argv[0]);
	}

	close(fd);
	exit(0);
}

// end user_test.c
