/*
 * slpy.c
 * Simple demo driver; readers are put to sleep on a
 * wait queue. Any writer coming along awakens the readers.
 * This version does implement proper SMP locking.
 * Uses the 'misc' char driver framework.
 *
 * Author: Kaiwan N Billimoria
 * License: Dual GPLv2/MIT
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>		// k[m|z]alloc(), k[z]free(), ...
#include <linux/mm.h>		// kvmalloc()
#include <linux/fs.h>		// the fops
#include <linux/sched.h>	// get_task_comm()

// copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include "../convenient.h"

static atomic_t data_present = ATOMIC_INIT(0);	/* event condition flag */
DECLARE_WAIT_QUEUE_HEAD(wq);

static int exclusive_wakeup;
module_param(exclusive_wakeup, int, 0);
MODULE_PARM_DESC(exclusive_wakeup, "set this to 1 to perform exclusive waiter/wakeups (only 1 sleeper will be awoken at a time);"
"default (0) is that all sleepers are awoken immd");

MODULE_AUTHOR("Kaiwan");
MODULE_DESCRIPTION("Trivial demo of using a wait queue");
MODULE_LICENSE("GPL/MIT");

/*
 * In this demo driver, we just put the current reader to sleep
 * trivially..; in a 'real' driver one would check at the h/w level: is data
 * available? If no, put the reader to sleep, else continue...stuff like that.
 */
static ssize_t sleepy_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	/*
	 * We don't make any assumptions when we're awoken; no, we recheck the
	 * predicate (condition) via the while()... only if data is available
	 * we proceed outside the loop, else we put the process ctx to sleep
	 * once again!
	 */
	while (atomic_read(&data_present) == 0) {
		pr_debug("process %d (%s) going to sleep\n", current->pid, current->comm);

		if (exclusive_wakeup == 1) {
			if (wait_event_interruptible_exclusive(wq, (atomic_read(&data_present) == 1))) {
				pr_debug("wait interrupted by signal, ret -ERESTARTSYS to VFS..\n");
				return -EINTR;
				//return -ERESTARTSYS; // old way
			}
		} else {
			if (wait_event_interruptible(wq, (atomic_read(&data_present) == 1))) {
				pr_debug("wait interrupted by signal, ret -ERESTARTSYS to VFS..\n");
				return -EINTR;
				//return -ERESTARTSYS; // old way
			}
		}

		/*
		 * Blocks here..the reader (user context) process/thread is put to sleep;
		 * this is actually effected by making the
		 * task state <- TASK_INTERRUPTIBLE and invoking the scheduler.
		 */
		pr_debug("awoken %d (%s), data_present=%d\n",
			 current->pid, current->comm, atomic_read(&data_present));
	}
	pr_debug("%d (%s): Data is available, proceeding...\n", current->pid, current->comm);
	/* Actual read work done here (in a 'real' driver)... 
	 * Perhaps with a mutex to avoid the 'thundering herd' issue..
	 */

	return count;
}

static ssize_t sleepy_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *offp)
{
	pr_debug("process %d (%s) awakening the readers...\n", current->pid, current->comm);

	/* Set the wake-up event condition to True, simulating the
	 * "data is available" condition... */
	atomic_set(&data_present, 1);
	/* and awaken all the lazy sleepy heads :-) */
	wake_up_interruptible(&wq);

	return count;
}

/* The driver 'functionality' is encoded via the fops */
static const struct file_operations slpy_misc_fops = {
	.read = sleepy_read,
	.write = sleepy_write,
	.llseek = no_llseek,	// dummy, we don't support lseek(2)
};

static struct miscdevice slpy_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* kernel dynamically assigns a free minor# */
	.name = "slpy_miscdrv",	/* when misc_register() is invoked, the kernel
				 * will auto-create device file as /dev/slpy_miscdrv;
				 *  also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,		/* ... dev node perms set as specified here */
	.fops = &slpy_misc_fops,	/* connect to this driver's 'functionality' */
};

static int __init miscdrv_rdwr_init(void)
{
	int ret = 0;
	struct device *dev;

	ret = misc_register(&slpy_miscdev);
	if (ret) {
		pr_err("misc device registration failed, aborting\n");
		return ret;
	}
	/* Retrieve the device pointer for this device */
	dev = slpy_miscdev.this_device;
	dev_info(dev, "'sleepy' misc driver (major # 10) registered, minor# = %d,"
		" dev node is /dev/%s\n"
		" exclusive waiter/wakeup? %s\n",
		slpy_miscdev.minor, slpy_miscdev.name,
		(exclusive_wakeup==1?"yes":"no"));

	return 0;		/* success */
}

static void __exit miscdrv_rdwr_exit(void)
{
	misc_deregister(&slpy_miscdev);
	pr_info("slpy misc driver deregistered\n");
}

module_init(miscdrv_rdwr_init);
module_exit(miscdrv_rdwr_exit);
