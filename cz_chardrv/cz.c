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
#define pr_fmt(fmt) "%s:%s():%d: " fmt, KBUILD_MODNAME, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/fs.h>		/* no_llseek */
#include <linux/slab.h>		/* kmalloc */

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#define CZ_MAJOR	0	/* 0 => dynamic major number assignment */
#define MAX_READ_COUNT	PAGE_SIZE

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

	zbuf = kzalloc(count, GFP_KERNEL);
	if (unlikely(zbuf == NULL)) {
		status = -ENOMEM;
		goto out_no_mem;
	}

	/* TIP: for portability between 32 and 64-bit, for size_t use %zu, for
	 * ssize_t use %zd
	 */
	pr_debug("process %s [pid %d] to read %zu bytes\n",
	    current->comm, current->pid, count);

	if (copy_to_user(buf, zbuf, count)) {
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
	pr_debug("process %s [pid %d], count=%zu\n",
	    current->comm, current->pid, count);
	return -ENOSYS; // 'Function not implemented'
}

/* ----------The cnul_* functionality routines
 *
 * cnul is designed to be a sink; but let's make it useful by ret 0,
 * allowing a file to be truncated!
 */
static ssize_t cnul_read(struct file *filp, char __user * buf,
			 size_t count, loff_t * offp)
{
	pr_debug("process %s [pid %d], count=%zu\n",
	    current->comm, current->pid, count);

	/* as Linux does it, return 0 */
	return 0;
	/* A read() to the nul device should always return 0 (EOF);
	 * the advantage of doing it this way is that a user
	 * can do stuff like truncate file fname by doing:
	 * $ cat /dev/cnul > fname
	 * or one could just:
	 * return -ENOSYS;   causing perror to show : "Function not implemented"
	 */
}

/* 
The signature of the driver 'method' is IDENTICAL to the file_operations member..
ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *); 
*/
static ssize_t cnul_write(struct file *filp, const char __user * buf,
			  size_t count, loff_t * offp)
{
	pr_debug("process %s [pid %d], count=%zu\n\tjiffies=%lu\n",
	    current->comm, current->pid, count, jiffies);
	return count;
	/* a write() to the nul device should always succeed! */
}

/* Minor-specific routines
 * For both the null and zero devices, set the llseek to no_llseek() which always
 * returns failure; as seeks aren't supported but not defining it can result in a
 * false positive
 */
static const struct file_operations czero_fops = {
	.llseek = no_llseek,
	.read = czero_read,
	.write = czero_write,
};

static const struct file_operations cnul_fops = {
	.llseek = no_llseek,
	.read = cnul_read,
	.write = cnul_write,
};

static int cz_open(struct inode *inode, struct file *filp)
{
	pr_debug("Device node with minor # %d being used\n", iminor(inode));

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
		return filp->f_op->open(inode, filp);
	/* Minor-specific open : jumps to the
	 * suitable entry point - the correct open() call if
	 * one has been defined
	 */
	return 0;
}

/* Major-wide open routine
 * This guarantees that *any and all* minor devices belonging to this major
 * are opened, the kernel will run this open routine as well!
 */
static const struct file_operations czopen_fops = {
	.open = cz_open,	/* just a means to get at the real open */
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

	pr_debug("cz_major=%d\n", cz_major);

	/*
	 * Register the major, and accept a dynamic number.
	 * The return value is the actual major # assigned.
	 */
	result = register_chrdev(cz_major, KBUILD_MODNAME, &czopen_fops);
	if (result < 0) {
		pr_info("register_chrdev() failed trying to get cz_major=%d\n",
		    cz_major);
		return result;
	}

	if (cz_major == 0)
		cz_major = result;	/* dynamic */
	pr_info("registered:: cz_major=%d\n", cz_major);

	return 0;		/* success */
}

static void __exit cz_cleanup_module(void)
{
	unregister_chrdev(cz_major, KBUILD_MODNAME);
	pr_info("Unregistered.\n");
}

module_init(cz_init_module);
module_exit(cz_cleanup_module);

module_param(cz_major, int, 0);	/* 0 => param won't show up in sysfs,
				   * non-zero are mode (perms) */
MODULE_PARM_DESC(cz_major, "Major number to attempt to use");
MODULE_AUTHOR("Kaiwan");
MODULE_DESCRIPTION("Simple (demo) null and zero memory driver driver");
MODULE_LICENSE("GPL");
