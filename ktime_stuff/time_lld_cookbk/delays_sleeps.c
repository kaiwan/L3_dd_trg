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

 Don't name it time_init()! <--- namespace collision!
 Got this err on build:
 ...
 error: conflicting types for ‘time_init’; have ‘int(void)’
   25 | static int __init time_init(void)
      |                   ^~~~~~~~~
In file included from /usr/src/linux-headers-6.6.51+rpt-common-rpi/arch/arm64/include/asm/alternative.h:9,
		-- snip --
                 from /usr/src/linux-headers-6.6.51+rpt-common-rpi/include/linux/module.h:13,
                 from /home/pi/kaiwanTECH/L3_dd_trg/ktime_stuff/time_lld_cookbk/time.c:7:
/usr/src/linux-headers-6.6.51+rpt-common-rpi/include/linux/init.h:154:6: note: previous declaration of ‘time_init’ with type ‘void(void)’
  154 | void time_init(void);
      |      ^~~~~~~~~

static int __init time_init(void)
 */
static int __init delays_sleeps_init(void)
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
	 * convenient.h:delay_sec(n)
	 * -it uses the schedule_timeout() API
	 */

    return -EINVAL;
    /* We deliberately abort here as there's nothing more to do, really...
     * so pl ignore the:
     * 'insmod: ERROR: could not insert module ./time.ko: Invalid parameters'
     * message on stdout
     */
}

static void __exit delays_sleeps_exit(void)
{
    pr_info("unloaded\n");
}

module_init(delays_sleeps_init);
module_exit(delays_sleeps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rodolfo Giometti");
MODULE_DESCRIPTION("Kernel timing functions");
MODULE_VERSION("0.1");
