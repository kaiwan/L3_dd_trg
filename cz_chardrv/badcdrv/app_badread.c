/*
 * bad_rd_tst.c
 * Test bed for demo drivers
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define FLGS		O_RDONLY
#define DMODE		0
#define SZ		1024


void sig( int signum )
{
	fprintf (stderr,"In sig: signum=%d\n", signum);
}

int main(int argc, char **argv)
{
	int fd=-1,n=0;
	struct sigaction act;
	size_t num=0;
	unsigned long dest_addr=0x0;
	FILE *fp1=NULL, *fp2=NULL;
	char cmd1[128], cmd2[128], sAddr[16], sVMA[128];
	
	if( argc != 3 ) {
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
	printf("%s: PID %d.\n",argv[0], getpid());

	num = atoi(argv[2]);
	if ((num < 0) || (num > INT_MAX)) {
		fprintf(stderr,"%s: number of bytes '%d' invalid.\n", argv[0], num);
		close (fd);
		exit (1);
	}

	// test reading..
	//n=read(fd,buf,num);

	/* BAD READ. DELIBRATELY set the address of the buffer to read into as offsets into
	 * each of the starting virtual addresses of each segment (VMA). We use 
	 * /proc/self/maps to scan the starting va of each of this process's segments.
	 * Add an offset to it and attempt to read into that location!
	 */
	snprintf (cmd1, 128, "grep '^........-' /proc/%d/maps |cut -f1 -d' '|cut -f1 -d'-'", getpid());
	fp1 = popen (cmd1, "r");

	snprintf (cmd2, 128, "cat /proc/%d/maps", getpid());
	fp2 = popen (cmd2, "r");

	if (!fp1 || !fp2) {
		perror("popen");
		close (fd);
		exit (1);
	}
		
	while (fgets (sAddr, 16, fp1)) {
		fgets (sVMA, 128, fp2);
		sVMA[strlen(sVMA)-1]='\0';
		printf("\nVMA: %s\n", sVMA);
		sAddr[strlen(sAddr)-1]='\0';
		dest_addr = strtoll (sAddr, NULL, 16) + 0x500;
		printf("sAddr = %s dest_addr = 0x%08x\n", sAddr, (unsigned int)dest_addr);
#if 1
		n = read (fd, (void *)dest_addr, num);
		if( n < 0 )
			perror("read failed");
		else
			printf("read %d bytes\n", n);
#endif
	}

	pclose (fp1);
	pclose (fp2);
    close(fd);
	exit(0);       
}
