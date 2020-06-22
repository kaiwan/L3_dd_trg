/*
 * Kernel timing functions
 * (c) Linux Device Drivers Cookbook, Packt
 * (Minimal enhancements, comments; Kaiwan NB).
 */
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timekeeping.h>

#define print_time(str, code)           \
    do {                    \
        u64 t0, t1;         \
        t0 = ktime_get_real_ns();   \
        code;               \
        t1 = ktime_get_real_ns();   \
        pr_info(str " -> %11llu ns = %5llu us = %3llu ms\n", \
			          (t1 - t0), (t1-t0)/1000, (t1-t0)/1000000); \
    } while (0)

/*
 * Init & exit stuff
 */

static int __init time_init(void)
{
	/* Atomic, busy-loops, no sleep! */
    pr_info("*delay() functions (atomic, in a delay loop):\n");
    print_time("        10 ns via ndelay(10)", ndelay(10));
	/* Preferred interface is udelay() */
    print_time("    10,000 ns via udelay(10)", udelay(10));
    print_time("10,000,000 ns via mdelay(10)", mdelay(10));

	/* Non-atomic, blocking APIs; causes schedule() to be invoked */
    pr_info("*sleep() functions (process ctx, sleeps/schedule()'s out):\n\n");
	/* usleep_range(): HRT-based; for approx range [10us - 20ms] */
    print_time("        10,000 ns via usleep_range(10,10)", usleep_range(10, 10));
	/* msleep(): jiffies/legacy-based; for longer sleeps (> 10ms) */
    print_time("    10,000,000 ns via          msleep(10)", msleep(10));
    print_time("    10,000,000 ns via     msleep_intr(10)", msleep_interruptible(10));
	/* ssleep() is a wrapper over msleep():  = msleep(ms*1000); */
    print_time("10,000,000,000 ns via          ssleep(10)", ssleep(10));

	/* --- ALSO see our implementation of sleep(n) as
	 * convenient.h:DELAY_SEC(n)
	 * -it uses the schedule_timeout() API
	 */

    return -EINVAL;
    /* We deliberately abort here as there's nothing more to do, really...
     * so pl ignore the:
     * 'insmod: ERROR: could not insert module ./time.ko: Invalid parameters'
     * message on stdout
     */
}

static void __exit time_exit(void)
{
    pr_info("unloaded\n");
}

module_init(time_init);
module_exit(time_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodolfo Giometti");
MODULE_DESCRIPTION("Kernel timing functions");
MODULE_VERSION("0.1");
