/*
 * Kernel timing functions
 * (c) Linux Device Drivers Cookbook, Packt
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
        pr_info(str " -> %11llu ns\n", t1 - t0); \
    } while (0)

/*
 * Init & exit stuff
 */

static int __init time_init(void)
{
    pr_info("*delay() functions:\n");
    print_time("        10 ns via ndelay(10)", ndelay(10));
    print_time("    10,000 ns via udelay(10)", udelay(10));
    print_time("10,000,000 ns via mdelay(10)", mdelay(10));

    pr_info("*sleep() functions:\n\n");
    print_time("        10,000 ns via usleep_range(10,10)", usleep_range(10, 10));
    print_time("    10,000,000 ns via          msleep(10)", msleep(10));
    print_time("    10,000,000 ns via     msleep_intr(10)", msleep_interruptible(10));
    print_time("10,000,000,000 ns via          ssleep(10)", ssleep(10));

    return -EINVAL;
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
