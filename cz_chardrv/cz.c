/*
 * cz.c
 *
 * Very simple demo char device driver for memory device;
 * Major # : 0 => dynamic major # allocation
 *
 * Implements two minor devices:
 * a) zero source        : minor # 1 : /dev/czero
 * b) sink (null device) : minor # 2 : /dev/cnul
 *
 * Author: Kaiwan N Billimoria
 * License: MIT/GPLv2
 */

/* 
 * #define	DEBUG	1    
 * #			//set to 0 to turn off debug messages
 * 
 * Lets not hardcode the debug flag. In the Makefile, 
 * you can specify this using:
 * EXTRA_CFLAGS += -DDEBUG
 * (under the "obj-m" line) if you want the flag defined.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/fs.h>		/* no_llseek */
#include <linux/slab.h>		/* kmalloc */
#include <linux/uaccess.h>	/* copy_to_user() */

#define	DRVNAME		"cz"
#define CZ_MAJOR  	0    /* 0 => dynamic major number assignment */
#define MAX_READ_COUNT	PAGE_SIZE

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
 * czero_read:
 * Simple zero source implementation: just fill a buffer with zeroes
 * (for upto PAGE_SIZE bytes ONLY) & pass it back to user-space.
 */ 
static ssize_t czero_read(struct file *filp, char __user *buf, 
		size_t count, loff_t *offp)
{
	char *zbuf;
	int status;

	/* simplification */
	if (count > MAX_READ_COUNT)
		count = MAX_READ_COUNT;
	
	/* kzalloc() not supported on RHEL5's kernel ver */
#if 0
	if ((zbuf = kzalloc (count, GFP_KERNEL)) == NULL ) {
		status = -ENOMEM;
		goto out_no_mem;
	}
#endif
	if ((zbuf = kmalloc (count, GFP_KERNEL)) == NULL ) {
		status = -ENOMEM;
		goto out_no_mem;
	}
	memset (zbuf, 0, count);

	MSG("process %s [pid %d] to read %ld bytes\n", 
			current->comm, current->pid, count );

	if (copy_to_user (buf, zbuf, count)) {
		status = -EFAULT;
		goto out_copy_err;
	}

	status = count;

out_copy_err:
	kfree(zbuf);
out_no_mem:
	return status;
}

static ssize_t czero_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%ld\n", 
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
	MSG( "process %s [pid %d], count=%ld\n", 
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
	MSG( "process %s [pid %d], count=%ld\n\tjiffies=%lu\n", 
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
