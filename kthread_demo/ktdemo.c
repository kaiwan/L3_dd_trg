/*
 * ktdemo.c
 * Simple demo of creating and using a kernel thread.
 * (c) Kaiwan NB, kaiwanTECH
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>	// {get,put}_task_struct()
#include <linux/sched/signal.h> // signal_pending()
#include <linux/signal.h>       // allow_signal()
#include <linux/kthread.h>
#include "../convenient.h"

#define MODULE_NAME	"ktdemo"
struct task_struct *gKthrd;

/* Our simple kernel thread. */
static int mykt(void *arg)
{
	pr_info("name: %s PID: %d TGID: %d\n",
		current->comm, current->pid, current->tgid);
	if (!current->mm)
		pr_info("mm field NULL.\n");

	/* By default all signals are masked for the kthread; allow a couple
	 * so that we can 'manually' kill it
	 */
	allow_signal(SIGINT);
	allow_signal(SIGQUIT);

	while (!kthread_should_stop()) {
		/* the kthread_should_stop() checks if this kthread should stop -
		 * returning True if that's the case... the kthread_stop() in the
		 * cleanup code does exactly this: it has the kthread_should_stop()
		 * return True, thus terminating this loop!
		 */
		pr_info("K Thread %d going to sleep now...\n", current->pid);
		set_current_state (TASK_INTERRUPTIBLE);
		schedule();	// and yield the processor...
		/* we're back on! here, it has to be due to either the SIGINT
 		 * or SIGQUIT signal! */
		if (signal_pending(current))
			break;
	}

	// We've been interrupted by a signal...
	set_current_state (TASK_RUNNING);
	pr_info("Kernel thread %d exiting now...\n", current->pid);

	return 0;
}

static int ktdemo_init(void)
{
	int ret=0, i=0;

	pr_info("Create a kernel thread...\n");

	/*
	 * kthread_run(threadfn, data, namefmt, ...)
	 * - it's a thin wrapper over the kthread_create() API
	 */
	gKthrd = kthread_run(mykt, NULL, "%s.%d", MODULE_NAME, i);
		/* 2nd arg is (void * arg) to pass, ret val is task ptr on success */
	if (ret < 0) {
		pr_err("kt1: kthread_create failed (%d)\n", ret);
		return ret;
	}
	get_task_struct(gKthrd); // inc refcnt, "take" the task struct

	pr_info("Module %s initialized, thread task ptr is 0x%pK (actual=0x%llx)\n"
	"See the new k thread '%s.0' with ps "
	"(and kill it with SIGINT or SIGQUIT)\n", 
		MODULE_NAME, gKthrd, (unsigned long long)gKthrd, MODULE_NAME);
	return 0;	// success
}

static void ktdemo_exit(void)
{
	kthread_stop(gKthrd);
	put_task_struct(gKthrd); // dec refcnt, "release" the task struct
	pr_info("Module %s unloaded.\n", MODULE_NAME);
}

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Kaiwan NB, kaiwanTECH");
MODULE_DESCRIPTION("Simple demo of creating a kernel thread");

module_init(ktdemo_init);
module_exit(ktdemo_exit);
