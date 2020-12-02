/*
 * ktimers/ktimers.c
 ***************************************************************
 * Simple demo of a kernel timer
 * (c) Author: Kaiwan NB
 * License: MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include "../../convenient.h"

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

	PRINT_CTX();
	pr_info("timed out... data=%d\n", mykd->data--);

	/* until countdown done, fire it again! */
	if (mykd->data)
		mod_timer(&mykd->tmr, jiffies + msecs_to_jiffies(exp_ms));
}

static int __init ktimers_init(void)
{
	int stat = 0;

	pr_info("inserted\n");

	kd.data = 114;
	kd.tmr.expires = jiffies + msecs_to_jiffies(exp_ms);
    kd.tmr.flags = 0;
	timer_setup(&kd.tmr, ding, 0);

	pr_info("timer set to expire in %ld ms\n", exp_ms);
	stat = mod_timer(&kd.tmr, jiffies + msecs_to_jiffies(exp_ms));
	if (stat != 0) {
		pr_warn("mod_timer failed (%d)\n", stat);
		return stat;
	}

	return 0;		/* success */
}

static void __exit ktimers_exit(void)
{
	pr_info("wait for possible timeouts to complete...\n");
	del_timer_sync(&kd.tmr);
	pr_info("removed\n");
}

module_init(ktimers_init);
module_exit(ktimers_exit);
