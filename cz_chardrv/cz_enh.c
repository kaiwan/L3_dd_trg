/*
 * cz_enh.c
 *
 * Very simple demo char device driver for memory device;
 * Major # : 0 => dynamic major # allocation
 *
 * Implements two minor devices:
 * a) zero source        : minor # 1 : /dev/czero
 * b) sink (null device) : minor # 2 : /dev/cnul
 *
 * Author: Kaiwan N Billimoria <kaiwan@designergraphix.com>
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

/*#define	DEBUG		1  */ /* set to 0 to turn off debug messages */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/fs.h>		/* no_llseek */
#include <linux/version.h>  /* ver macros */
#include <linux/slab.h>
#include <asm/uaccess.h>	/* copy_to_user() */

#define	DRVNAME		"cz_enh"
#define CZ_MAJOR  	0    /* 0 => dynamic major number assignment */
//#define MAX_READ_COUNT	PAGE_SIZE

#ifdef DEBUG
	#define MSG(string, args...) \
		printk(KERN_DEBUG "%s:%s:%d: " string, \
			DRVNAME, __FUNCTION__, __LINE__, ##args)
#else
	#define MSG(string, args...)
#endif

static int cz_major = CZ_MAJOR;

/* ----------The czero_* functionality routines 
 *
 * czero is designed to be a zero source
 */


/*
 * Simple zero source implementation: just fill a buffer with zeroes
 * & pass it back to user-space.
 *
 * This is an enhanced version of the previous driver- it populates a 
 * user-space buffer of *any* size (not just to a max of 1 page as in the 
 * previous driver).
 *
 * Can test the "czero" device read with (one possibility):
 *  dd if=/dev/czero of=/tmp/testz2 bs=4k count=10000
 *
 * To greatly speed up the xfer, try:
 * 1. revoke the printk's (by undefining DEBUG)
 * 2. comment out the test to see if we should invoke the sceduler, on each loop iteration.
 *
 */ 
static ssize_t czero_read(struct file *filp, char __user *buf, 
		size_t count, loff_t *offp)
{
	char *zbuf;
	int mcount=count, i=0, loopcount, buf_to=0, rem=0, status=count;
	u32 pat=0xdeadface;

	MSG( "process %s [pid %d] to read %d bytes; buf=0x%08x\n", 
		current->comm, current->pid, count, (unsigned int)buf );

	if (count > PAGE_SIZE)
		mcount=PAGE_SIZE;

	/* kzalloc() not supported on RHEL4's kernel ver; >2.6.14 ? 
	 * API ref: http://gnugeneration.com/books/linux/2.6.20/kernel-api/re241.html
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14)
	if ( (zbuf=kzalloc (mcount, GFP_KERNEL)) == NULL) {
#else
	if ( (zbuf=kmalloc (mcount, GFP_KERNEL)) == NULL) {
#endif
		status = -ENOMEM;
		goto out_no_mem;
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,14)
	memset (zbuf, 0, mcount);
#endif

//	memset (zbuf, pat, mcount); // bug! memset fills only a byte..
	for (i=0; i<mcount/4;i++) { // done for little-endian
		zbuf[(i*4)+0]=0xce;
		zbuf[(i*4)+1]=0xfa;
		zbuf[(i*4)+2]=0xad;
		zbuf[(i*4)+3]=0xde;
	}
	print_hex_dump_bytes (" ", DUMP_PREFIX_OFFSET, zbuf, mcount/4);

	/* Loop abs(count/PAGE_SIZE) times */
	loopcount=count/PAGE_SIZE;
	MSG ("loopcount=%d\n",loopcount);

	for (i=0; i<loopcount; i++) {
		buf_to = (unsigned int)buf + (i*PAGE_SIZE);
		MSG ("%d: buf_to loc=0x%08x\n",i, buf_to);
		if( copy_to_user( (void *)buf_to, zbuf, mcount ) ) {
			status = -EFAULT;
			goto out;
		}
		cond_resched();
#if 0
 #ifndef CONFIG_PREEMPT
		if ((!(i%100)) && unlikely(test_tsk_thread_flag(current, TIF_NEED_RESCHED)))
			cond_resched();
			//schedule();
 #endif
#endif
	}

	/* Remaining bytes to fill */
	rem = count % PAGE_SIZE;
	MSG ("rem=%d\n",rem);
	if (!rem) 
		goto out;

	buf_to = (unsigned int)buf + (i*PAGE_SIZE);
	MSG ("%d: buf_to loc=0x%08x\n",i, buf_to);
	if( copy_to_user( (void *)buf_to, zbuf, rem ) ) {
		status = -EFAULT;
		goto out;
	}
out:
	kfree (zbuf);
out_no_mem:
	return status;
}

static ssize_t czero_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%d\n", 
			current->comm, current->pid, count);
	return -ENOSYS;
}


/* ----------The cnul_* functionality routines 
*
* cnul is designed to be a sink
*/
static ssize_t cnul_read(struct file *filp, char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%d\n", 
			current->comm, current->pid, count);

	/* as Linux does it, return 0 */
	return 0; 
		/* A read() to the nul device should always return 0 (EOF);
	           the advantage of doing it this way is that a user 
		   can do stuff like truncate file fname by doing: 
			 $ cat /dev/cnul > fname 
	or one could just:
	return -ENOSYS;   causing perror to show : "Function not implemented"
	*/
}

static ssize_t cnul_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%d\n\tjiffies=%lu\n", 
			current->comm, current->pid, count, jiffies );
	return count;
	/* a write() to the nul device should always succeed! */
}


/* Minor-specific open routines */
static struct file_operations czero_fops = {
	.llseek =	no_llseek,
	.read =		czero_read,
	.write =	czero_write,
};

static struct file_operations cnul_fops = {
	.llseek =	no_llseek,
	.read =		cnul_read,
	.write =	cnul_write,
};

static int cz_open(struct inode * inode, struct file * filp)
{
	MSG( "Device node with minor # %d being used\n", iminor(inode));

	switch (iminor(inode)) {
		case 1:
			filp->f_op = &czero_fops;
			break;
		case 2:
			filp->f_op = &cnul_fops;
			break;
		default:
			return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode,filp); 
		   /* Minor-specific open : jumps to the 
		   suitable entry point - the correct open() call if 
	      	   one has been defined */
	return 0;
}

/* Major-wide open routine */
static struct file_operations czopen_fops = {
	.open =		cz_open, /* just a means to get at the real open */
};

/*
 * Register the char driver with the kernel.
 *
 *  On 2.6 kernels, we could use the new alloc_register_chrdev() function; 
 *  here, we use the "classic" register_chrdev() API.
 *	result = register_chrdev( CZ_MAJOR, "cz", &cz_fops );
 */
static int __init cz_init_module(void)
{
	int result;

	MSG( "cz_major=%d\n",cz_major);

	/*
	 * Register the major, and accept a dynamic number.
	 * The return value is the actual major # assigned.
	 */
	result = register_chrdev(cz_major, DRVNAME, &czopen_fops);
	if (result < 0) {
		MSG( "register_chrdev() failed trying to get cz_major=%d\n",
		cz_major);
		return result;
	}

	if (cz_major == 0) cz_major = result; /* dynamic */
	MSG( "registered:: cz_major=%d\n",cz_major);

	return 0; /* success */
}

static void __exit cz_cleanup_module(void)
{
	unregister_chrdev(cz_major, DRVNAME);
	MSG("Unregistered.\n");
}

module_init(cz_init_module);
module_exit(cz_cleanup_module);

module_param(cz_major, int, 0); /* 0 => param won't show up in sysfs, 
				   non-zero are mode (perms) */
MODULE_PARM_DESC(cz_major, "Major number to attempt to use");
MODULE_AUTHOR("Kaiwan");
MODULE_DESCRIPTION("Simple (demo) null and zero memory driver driver");
MODULE_LICENSE("GPL");
