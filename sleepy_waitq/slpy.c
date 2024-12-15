/*
 * slpy.c 
 * Simple demo driver; readers are put to sleep on a 
 * wait queue. Any writer coming along awakens the readers.
 * This version does implement proper SMP locking.
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 * License: Dual GPL/MIT
 */

/* 
 * #define	DEBUG	1     //set to 0 to turn off debug messages
 * 
 * Lets not hardcode the debug flag. In the Makefile, 
 * you can specify this using:
 * EXTRA_CFLAGS += -DDEBUG
 * (under the "obj-m" line) if you want the flag defined.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/sched.h>	/* current, jiffies */

#define	DRVNAME		"slpy"

static atomic_t data_present = ATOMIC_INIT(0);	/* event condition flag */
static int sleepy_major = 0;
DECLARE_WAIT_QUEUE_HEAD(wq);

MODULE_AUTHOR("Kaiwan");
MODULE_DESCRIPTION("Trivial demo of using a wait queue.");
MODULE_LICENSE("GPL/MIT");

/* 
 * In this demo driver, we just put the current reader to sleep 
 * trivially..; in a 'real' driver one would check: no data available?
 * put the reader to sleep, else continue...stuff like that.
 */
static ssize_t sleepy_read(struct file *filp, char __user * buf,
			   size_t count, loff_t * offp)
{
	if (atomic_read(&data_present) == 0) {
		pr_debug("process %d (%s) going to sleep\n", current->pid,
		    current->comm);
		if (wait_event_interruptible
		    (wq, (atomic_read(&data_present) == 1))) {
			pr_debug("wait interrupted by signal, ret -ERESTARTSYS to VFS..\n");
			return -EINTR;
			//return -ERESTARTSYS; // old way
		}
		/* 
		 * Blocks here..the reader (user context) process is put to sleep;
		 * this is actually effected by making the 
		 * task state <- TASK_INTERRUPTIBLE and invoking the scheduler.
		 */
		pr_debug("awoken %d (%s), data_present=%d\n",
		    current->pid, current->comm, atomic_read(&data_present));
	} else {
		pr_debug("%d (%s): Data is available, proceeding...\n",
		    current->pid, current->comm);
		/* Actual read work done here (in a 'real' driver)... */
	}
	return count;
}

static ssize_t sleepy_write(struct file *filp, const char __user * buf,
			    size_t count, loff_t * offp)
{
	pr_debug("process %d (%s) awakening the readers...\n",
	    current->pid, current->comm);

	/* Set the wake-up event condition to True, simulating the 
	 * "data is available" condition... */
	atomic_set(&data_present, 1);
	/* and awaken all the lazy sleepy heads :-) */
	wake_up_interruptible(&wq);

	return count;
}

static struct file_operations sleepy_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = sleepy_read,
	.write = sleepy_write,
};

static int __init slpy_init_module(void)
{
	int result;

	result = register_chrdev(sleepy_major, DRVNAME, &sleepy_fops);
	if (result < 0)
		return result;

	if (sleepy_major == 0)
		sleepy_major = result;
	printk(KERN_DEBUG "sleepy: major # = %d\n", sleepy_major);

	return 0;		/* success */
}

static void __exit slpy_cleanup_module(void)
{
	unregister_chrdev(sleepy_major, DRVNAME);
	pr_debug("Removed.\n");
}

module_init(slpy_init_module);
module_exit(slpy_cleanup_module);
