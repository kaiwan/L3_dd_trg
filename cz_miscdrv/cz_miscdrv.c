/*
 * cz_miscdrv.c
 *
 * Very simple demo char device driver for memory devices;
 * Uses char 'misc' framework, major # 10.
 * This driver implements two char 'misc' devices; minors are dynamically alloted:
 * a) zero source        : /dev/czero_miscdev
 * b) sink (null device) : /dev/cnul_miscdev
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
 * ccflags-y += -DDEBUG
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/fs.h>		/* no_llseek */
#include <linux/slab.h>		/* kmalloc */
#include <linux/miscdevice.h>

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

/* ----------The czero_* functionality routines
 *
 * czero is designed to be a zero source
 */
#define MAX_READ_COUNT	PAGE_SIZE

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


/* The 'null' device as a char 'misc' device */
static const struct file_operations cz_cnul_misc_fops = {
	.llseek = no_llseek,
	.read = cnul_read,
	.write = cnul_write,
};
static struct miscdevice cz_cnul_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* kernel dynamically assigns a free minor# */
	.name = "cnul_miscdev",	/* when misc_register() is invoked, the kernel
				 * will auto-create device file as /dev/cnul_miscdev ;
				 * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,		/* ... dev node perms set as specified here */
	.fops = &cz_cnul_misc_fops,	/* connect to this driver's 'functionality' */
};

/* The 'zero' device as a char 'misc' device */
static const struct file_operations cz_czero_misc_fops = {
	.llseek = no_llseek,
	.read = czero_read,
	.write = czero_write,
};
static struct miscdevice cz_czero_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* kernel dynamically assigns a free minor# */
	.name = "czero_miscdev",	/* when misc_register() is invoked, the kernel
				 * will auto-create device file as /dev/czero_miscdev ;
				 * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,			/* ... dev node perms set as specified here */
	.fops = &cz_czero_misc_fops,	/* connect to this driver's 'functionality' */
};

/*
 * Register the char 'misc' framework driver with the kernel.
 */
static int __init cz_init_module(void)
{
	int ret;
	struct device *dev;

	ret = misc_register(&cz_czero_miscdev);
	if (ret != 0) {
		pr_notice("misc device registration 1 failed, aborting\n");
		return ret;
	}
	dev = cz_czero_miscdev.this_device;
	dev_info(dev, "cz czero misc driver (major # 10) registered, minor# = %d,"
		" dev node is /dev/%s\n", cz_czero_miscdev.minor, cz_czero_miscdev.name);

	ret = misc_register(&cz_cnul_miscdev);
	if (ret != 0) {
		pr_notice("misc device registration 2 failed, aborting\n");
		return ret;
	}
	dev_info(dev, "cz cnul misc driver (major # 10) registered, minor# = %d,"
		" dev node is /dev/%s\n", cz_cnul_miscdev.minor, cz_cnul_miscdev.name);

	return 0;		/* success */
}

static void __exit cz_cleanup_module(void)
{
	misc_deregister(&cz_cnul_miscdev);
	misc_deregister(&cz_czero_miscdev);
	pr_info("Unregistered.\n");
}

module_init(cz_init_module);
module_exit(cz_cleanup_module);

MODULE_AUTHOR("Kaiwan NB, kaiwanTECH");
MODULE_DESCRIPTION("Simple (demo) null and zero memory driver driver");
MODULE_LICENSE("Dual MIT/GPL");
