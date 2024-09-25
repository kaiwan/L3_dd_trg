/*
 * slpy_wq.c
 *
 * Simple demo driver; readers are put to sleep on a 
 * wait queue. Any writer coming along awakens the readers.
 *
 * Additionally, in order to demo a Work Queue feature, we have a work queue
 * structure set up: when the driver's write method is invoked, it schedules 
 * the 'deferred work' function of our work queue. This function, in order to 
 * do something, starts up and constantly resets a kernel timer.
 *
 * This version does NOT correctly implement SMP locking, for simplicity (left 
 * as an exercise to the participant!); this is done in the slpy_proper.c driver.
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 * Dual MIT/GPL
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/workqueue.h>
#include <linux/timer.h>
#include "../convenient.h"

#define	DRVNAME		"workq_splydrv"

/* In a "real" driver, we would require the use of concurrency control
 * mechanisms, which we just ignore in this simple demo case.
 */
static int sleepy_major=0;
static atomic_t data_present=ATOMIC_INIT(0); 	/* event condition flag */
DECLARE_WAIT_QUEUE_HEAD(wq);

static struct work_struct ws; /* for our workqueue */
static struct timer_list timr;
static int delaysec=7;

/*------------------------ Module stuff */
MODULE_AUTHOR("Kaiwan NB, kaiwanTECH");
MODULE_DESCRIPTION("Demo of using a work queue (and timer).");
MODULE_LICENSE("Dual MIT/GPL");

/* This is the timeout function. */
static void whee(struct timer_list *tmr)
{
	pr_debug("Timed out. jiffies=%lu\n", jiffies);
	PRINT_CTX();
//	QPDS;

#if 0
	schedule();   // BUG! trigger an Oops!
#endif
	/* Reset and activate the timer */
	mod_timer(&timr, jiffies + delaysec*HZ);
}

/* This is our deferred processing work queue handler; it
 * runs in process context (via a kernel thread)
 */
static void wq_func(struct work_struct *work) //void *data)
{
	pr_debug("We're here doing some deferred work! HZ=%d jiffies=%lu\n", HZ, jiffies);
	PRINT_CTX();
	QPDS;
	pr_debug("\nNow setting up a (auto-repeating) timer with %ds expiry...\n", delaysec);

	/* Activate the timer */
	mod_timer(&timr, jiffies + delaysec*HZ);
}

static ssize_t sleepy_read(struct file *filp, char __user *buf, 
			size_t count, loff_t *offp)
{
	pr_debug("process %d (%s) going to sleep\n", current->pid, current->comm);

	// put our process context into an interruptible sleep...
	if (wait_event_interruptible(wq, atomic_read(&data_present)==1)) {
		pr_debug("wait interrupted by signal, ret -ERESTARTSYS to VFS..\n");
		return -ERESTARTSYS;
	}

	pr_info("%s: awoken %d (%s), data_present=%d\n",
		DRVNAME, current->pid, current->comm, atomic_read(&data_present));

	return 0; /* EOF */
}

static ssize_t sleepy_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *offp)
{
	pr_info("%s: scheduling deferred work in the work queue now...\n", DRVNAME);
	schedule_work(&ws);

	pr_debug("process %d (%s) awakening the readers...\n",
		current->pid, current->comm );

	atomic_set(&data_present, 1);
	wake_up_interruptible(&wq); 
		/* Awakens all interruptible sleepers on wq */

	return count; /* succeed, to avoid retrial */
}


static struct file_operations sleepy_fops = {
	.owner = 	THIS_MODULE,
	.llseek = 	no_llseek,
	.read = 	sleepy_read,
	.write = 	sleepy_write,
};

static int __init workq_slpydrv_init_module(void)
{
	int result;

	result = register_chrdev(sleepy_major, DRVNAME, &sleepy_fops);
	if (result < 0) 
		return result;

	if (sleepy_major == 0)
		sleepy_major = result;
	pr_debug("sleepy: major # = %d\n", sleepy_major);

	// INIT_WORK(_work, _func)
	INIT_WORK(&ws, wq_func);

	// timer_setup(timer, callback, flags)
	timer_setup(&timr, whee, 0);

	pr_info("%s: workQ and timer initialized..\n", DRVNAME);
	pr_debug("Loaded ok.\n");

	return 0; /* success */
}

static void __exit workq_slpydrv_cleanup_module(void)
{
	del_timer_sync(&timr);
	flush_scheduled_work();
	unregister_chrdev(sleepy_major, DRVNAME);
	pr_debug("Removed.\n");
}

module_init(workq_slpydrv_init_module);
module_exit(workq_slpydrv_cleanup_module);
