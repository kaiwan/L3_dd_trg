/*
 * ioctl_llkd_userspace.c
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
//#include "../ioctl_llkd.h"
#include <linux/rtc.h>

int main(int argc, char **argv)
{
	int fd, power;
	struct rtc_time tm;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s device_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDWR, 0)) == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	printf("device opened: fd=%d\n", fd);

	// read time from RTC
	if (ioctl(fd, RTC_RD_TIME, &tm) == -1) {
		perror("ioctl RTC_RD_TIME failed");
		close(fd);
		exit(EXIT_FAILURE);
	}
	printf("%s: RTC read done\n", argv[0]);

	close(fd);
	exit(EXIT_SUCCESS);
}
