/*
 * ktimers/ktimers.c
 ***************************************************************
 * < Your desc, comments, etc >
 * (c) Author:
 ****************************************************************
 * Brief Description:
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include "../convenient.h"

#define OURMODNAME   "ktimers"

MODULE_AUTHOR("Kaiwan NB");
MODULE_DESCRIPTION("L3 LDD trg: using simple kernel timers");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("0.1");

static struct kdata {
	struct timer_list tmr;
	int data;
} kd;
static unsigned long exp_ms = 500;

static void ding(struct timer_list *timr)
{
	struct kdata *mykd = from_timer(mykd, timr, tmr);

	pr_info("%s:%s()! timed out... data=%d\n",
		OURMODNAME, __func__, mykd->data--);

	/* fire it again! */
	mod_timer(&mykd->tmr, jiffies + msecs_to_jiffies(exp_ms));
}

static int __init ktimers_init(void)
{
	int stat = 0;

	pr_debug("%s: inserted\n", OURMODNAME);
	kd.data = 114;
	timer_setup(&kd.tmr, ding, 0);

	pr_info("%s: timer set to expire in %ld ms\n", OURMODNAME, exp_ms);
	stat = mod_timer(&kd.tmr, jiffies + msecs_to_jiffies(exp_ms));
	if (stat != 0) {
		pr_warn("%s: mod_timer failed (%d)\n", OURMODNAME, stat);
		return stat;
	}

	return 0;		/* success */
}

static void __exit ktimers_exit(void)
{
	pr_info("%s: wait for possible timeouts to complete...\n", OURMODNAME);
	del_timer_sync(&kd.tmr);
	pr_debug("%s: removed\n", OURMODNAME);
}

module_init(ktimers_init);
module_exit(ktimers_exit);
