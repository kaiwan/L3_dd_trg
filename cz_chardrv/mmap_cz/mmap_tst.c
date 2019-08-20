/*
 * mmap_tst.c
 * Test bed for demo drivers; specifically, for the 2cz_mmap.c zero-source
 * zero-copy implementation of the 'cz' driver (using mmap instead of read).
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 * License: [L]GPL
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#define FLGS		O_RDONLY
#define DMODE		0
#define SZ		1024

void sig( int signum )
{
	fprintf (stderr,"In sig: signum=%d\n", signum);
}

int main(int argc, char **argv)
{
	int fd, *addr=0, len; 
	off_t offset=0x0;
	struct sigaction act;
	size_t num=0;
	
	if( argc!=3 ) {
		fprintf(stderr,"Usage: %s device_file num_bytes_to_read\n", argv[0]);
		exit(1);
	}

	act.sa_handler = sig;
	act.sa_flags = SA_RESTART;
	sigemptyset (&act.sa_mask);
	if ((sigaction (SIGINT, &act, 0)) == -1) {
		perror("sigaction"), exit (1);
	}	

	if( (fd=open(argv[1],FLGS,DMODE)) == -1)
		perror("open"),exit(1);
	printf("PID %d: device opened: fd=%d\n", getpid(), fd);

	num = atoi(argv[2]);
	if ((num < 0) || (num > INT_MAX)) {
		fprintf(stderr,"%s: number of bytes '%d' invalid.\n", argv[0], num);
		close (fd);
		exit (1);
	}

//	len = getpagesize(); // TODO- len to be what user passes
	len = num;
	offset = 0x0; //0x1000;

	if (offset % getpagesize()) {
		printf("%s: offset must be page-aligned, aborting...\n", argv[0]);
		close(fd);
		return -1;
	}

	/*
	 void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
	*/
	addr = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, offset);
	if (-1 == (int)addr) {
		perror("mmap failed (see dmesg output as well)");
		close(fd);
		return -1;
	}
	printf("%s: mmap addr = %p\n", argv[0], addr);


	int i, j, stepsz = 32;
	unsigned char *caddr = 	(unsigned char *)addr;

	printf("\nMmap-ped memory dump (@ step size of %d byte(s))\n"
	"Address :Offset: Contents ", stepsz);
	for(i=offset, j=0; i<(len+offset); i+=stepsz,j++) {
		if (!(j%32))
			printf("\n%08x:%06d: ", (unsigned int)addr+i, i);
		printf("%02x ", (caddr[i]));
	}
	printf("\n");

	munmap(addr, len);
   	close(fd);
	exit(0);       
}
