/* coord_gen.c
 * Generates random coordinates emulating our virtual mouse
 * (c) ELDD book!
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define DEVFILE "/sys/devices/platform/vms/vms"

int main(int argc, char *argv[])
{
	int sim_fd;
	int x, y;
	char buffer[10];

/* Open the sysfs coordinate node */
	sim_fd = open(DEVFILE, O_WRONLY);
	if (sim_fd < 0) {
		perror("Couldn't open vms coordinate file\n");
		exit(-1);
	}

	while (1) {
/* Generate random relative coordinates */
		x = random() % 20;
		y = random() % 20;
		if (x % 2)
			x = -x;
		if (y % 2)
			y = -y;
/* Convey simulated coordinates to the virtual mouse driver */
		sprintf(buffer, "%d %d %d", x, y, 0);
		if (write(sim_fd, buffer, strlen(buffer)) < 0)
			perror("write failed");
		fsync(sim_fd);
		sleep(1);
	}
	close(sim_fd);
}
