/*
 * cz_enh_miscdrv.c
 *
 * Very simple demo char device driver for memory devices;
 * Uses char 'misc' framework, major # 10.
 * This driver implements two char 'misc' devices; minors are dynamically alloted:
 * a) zero source        : /dev/czero_miscdev
 * b) sink (null device) : /dev/cnul_miscdev
 *
 * This version's 'czero' implementation is superior to the previous one
 * (cz_miscdrv.c); here, you can read any number of 'zero' bytes from it (the
 * previous implementation was limited to one page).
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
/*
 * czero_read:
 * Simple zero source implementation: just fill a buffer with zeroes
 * and pass it back to user-space.
 * This is an enhanced version of the earier czero_read_onepage() method (see
 * it below, commented out) - it populates a user-space buffer of *any* size
 * (not just to a max of 1 page as in the 'onepage' version).
 *
 * Can test this "czero" device read with (one possibility):
 *  dd if=/dev/czero of=/tmp/testz2 bs=4k count=10000
 * (here we're reading 4k*10,000 ~= 40 MB of 'zeroes' from our 'zero' device)
 *
 * To greatly speed up the xfer, try:
 * 1. revoke the printk's (by undefining DEBUG)
 * 2. comment out the test to see if we should invoke the sceduler, on each loop iteration.
 */
static ssize_t czero_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	char *zbuf;
	unsigned long buf_to = 0;
	// For clarity, using more local vars here than is strictly required
	int mcount = count, i = 0, loopcount, rem = 0, status = count;

	/* TIP: for portability between 32 and 64-bit, for size_t use %zu, for
	 * ssize_t use %zd
	 */
	pr_debug("process %s [pid %d] to read %zu bytes; buf=0x%lx\n",
		 current->comm, current->pid, count, (unsigned long)buf);

	if (count > PAGE_SIZE)
		mcount = PAGE_SIZE;

	/* This is really too old to bother with any longer...
	 * But just pedantically:
	 * kzalloc() not supported on RHEL4's kernel ver; >2.6.14 ?
	 * API ref: http://gnugeneration.com/books/linux/2.6.20/kernel-api/re241.html
	 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 14)
	zbuf = kzalloc(mcount, GFP_KERNEL);
	if (zbuf == NULL) {
#else
	zbuf = kmalloc(mcount, GFP_KERNEL);
	if (zbuf == NULL) {
#endif
		status = -ENOMEM;
		goto out_no_mem;
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 14)
	memset(zbuf, 0, mcount);
#endif

#define FILL_PATTERN   0
#if (FILL_PATTERN == 1)
	/* lets fill it with 'deadface' ! */
	for (i = 0; i < mcount / 4; i++) {
#ifndef __BIG_ENDIAN		// little-endian
		zbuf[(i * 4) + 0] = 0xde;
		zbuf[(i * 4) + 1] = 0xad;
		zbuf[(i * 4) + 2] = 0xfa;
		zbuf[(i * 4) + 3] = 0xce;
#else				// big-endian
		zbuf[(i * 4) + 0] = 0xce;
		zbuf[(i * 4) + 1] = 0xfa;
		zbuf[(i * 4) + 2] = 0xad;
		zbuf[(i * 4) + 3] = 0xde;
#endif
	}
	print_hex_dump_bytes(" ", DUMP_PREFIX_OFFSET, zbuf, mcount / 4);
#endif
	/* Loop abs(count/PAGE_SIZE) times */
	loopcount = count / PAGE_SIZE;
	pr_debug("loopcount=%d\n", loopcount);

	for (i = 0; i < loopcount; i++) {
		buf_to = (unsigned long)buf + (i * PAGE_SIZE);
		pr_debug("%d: buf_to loc=0x%lx\n", i, buf_to);
		if (copy_to_user((void *)buf_to, zbuf, mcount)) {
			status = -EFAULT;
			goto out;
		}
		cond_resched();
#if 0
#ifndef CONFIG_PREEMPT
		if ((!(i % 100))
		    && unlikely(test_tsk_thread_flag(current, TIF_NEED_RESCHED)))
			schedule();
#endif
#endif
	}

	/* Remaining bytes to fill */
	rem = count % PAGE_SIZE;
	pr_debug("rem=%d\n", rem);
	if (!rem)
		goto out;

	buf_to = (unsigned long)buf + (i * PAGE_SIZE);
	pr_debug("%d: buf_to loc=0x%lx\n", i, buf_to);
	if (copy_to_user((void *)buf_to, zbuf, rem)) {
		status = -EFAULT;
		goto out;
	}
 out:
	kfree(zbuf);
 out_no_mem:
	return status;
}

#if 0
static ssize_t czero_read_onepage(struct file *filp, char __user *buf,
				  size_t count, loff_t *offp)
{
	char *zbuf;
	int status;

	/* simplification */
	if (count > PAGE_SIZE)
		count = PAGE_SIZE;

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
#endif

static ssize_t czero_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *offp)
{
	pr_debug("process %s [pid %d], count=%zu\n", current->comm, current->pid, count);
	return -ENOSYS;		// 'Function not implemented'
}

/* ----------The cnul_* functionality routines
 *
 * cnul is designed to be a sink; but let's make it useful by ret 0,
 * allowing a file to be truncated!
 */
static ssize_t cnul_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	pr_debug("process %s [pid %d], count=%zu\n", current->comm, current->pid, count);

	/* as Linux does it, return 0 */
	return 0;
	/* A read() to the nul device should always return 0 (EOF);
	 * the advantage of doing it this way is that a user
	 * can do stuff like truncate file fname by doing:
	 * $ cat /dev/cnul > fname
	 * Alternatively, one could just do this:
	 * return -ENOSYS;   causing perror to show : "Function not implemented"
	 */
}

/*
 * The signature of the driver 'method' is IDENTICAL to the file_operations member..
 * ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
 */
static ssize_t cnul_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offp)
{
	pr_debug("process %s [pid %d], count=%zu\n\tjiffies=%lu\n",
		 current->comm, current->pid, count, jiffies);
	return count;
	/* a write() to the nul device should always succeed! */
}

/* The 'null' device as a char 'misc' device */
static const struct file_operations cz_cnul_misc_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)	// commit 868941b
	.llseek = no_llseek,
#endif
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)	// commit 868941b
	.llseek = no_llseek,
#endif
	.read = czero_read,
	.write = czero_write,
};

static struct miscdevice cz_czero_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* kernel dynamically assigns a free minor# */
	.name = "czero_miscdev",	/* when misc_register() is invoked, the kernel
					 * will auto-create device file as /dev/czero_miscdev ;
					 * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,		/* ... dev node perms set as specified here */
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
MODULE_DESCRIPTION("Simple (demo) null and zero memory char (misc) driver");
MODULE_LICENSE("Dual MIT/GPL");
