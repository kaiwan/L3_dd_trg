/*
 * irq_work.c
 * Emulate interrupt handling by using the irq_work* functionality..
 * 
 * Procedure:
 * a) specify the 'work func' via the init_irq_work() interface
 * b) "run it" by queuing up the work via the irq_work_queue() api.
 * (Seems to use the IPI - inter-processor-interrupts - to raise an IRQ).
 * Ref: https://lwn.net/Articles/411605/
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/irq_work.h>
#include "../convenient.h"

#define OURMODNAME    "irq_work"

/* Module parameters */
static int sleep_in_intrctx;
module_param(sleep_in_intrctx, int, 0660);
MODULE_PARM_DESC(sleep_in_intrctx,
"Parameter to control whether to (stupidly) attempt sleeping in interrupt context! [default=0]; \
pass 1 to attempt to and thus create a Bug!");

MODULE_AUTHOR("Kaiwan NB, kaiwanTECH");
MODULE_DESCRIPTION("A simple demo of running in interrupt context via the irq_work");
MODULE_LICENSE("Dual MIT/GPL");

static struct irq_work irqwork;

/*
 * This function runs in (hardirq) interrupt context
 */
static void my_irq_work(struct irq_work *irqwk)
{
	PRINT_CTX();
	dump_stack();
	printk_ratelimited(KERN_WARNING "\n%s: a ratelimited printk here!\n", OURMODNAME);
	trace_printk("... followed by a trace_printk() too!\n");

	if (sleep_in_intrctx == 1) {
		pr_info("%s: attempting sleep in irq ctx now...\n", OURMODNAME);
		schedule();
	}
}

static int __init intr_drv_init(void)
{
	int i;

	pr_info("%s: param sleep_in_intrctx=%d (%s)\n",
		OURMODNAME, sleep_in_intrctx,
		(sleep_in_intrctx==0?"ok":"Aaargh!"));
	PRINT_CTX();

	init_irq_work(&irqwork, my_irq_work);

#define NUM_TIMES_TRIGGER 3
	for (i=0; i<NUM_TIMES_TRIGGER; i++) {
		pr_info("%s: trigger interrupt work, instance #%02d\n", OURMODNAME, i);
		/* Enqueue &irqwork on the current cpu */
		irq_work_queue(&irqwork);
	}

	return 0;		// success
}

static void __exit intr_drv_exit(void)
{
	pr_info("%s: removed\n", OURMODNAME);
}

module_init(intr_drv_init);
module_exit(intr_drv_exit);
