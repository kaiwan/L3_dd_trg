/*
 * platdemo.c
 *
 * Simple demo platform device and associated platform driver.
 *
 * Setup a bare-bones platform device & associated driver.
 * Platform devices get bound to their driver simply on the basis of the 'name'
 * field; if they match, the driver core "binds" them, invoking the 'probe'
 * routine. Conversely, the 'remove' method is invoked at unload/shutdown.
 *
 * License: Dual MIT/GPL
 * Kaiwan NB <kaiwan -at- kaiwantech -dot- com>
 * (c) kaiwanTECH
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__
#define dev_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "../../convenient.h"

#define DRVNAME  "platdemo"
/*------------------------ Structs, prototypes, .. ------------------*/
static int platdev_probe(struct platform_device *);
static int platdev_remove(struct platform_device *);
static void plat0_release(struct device *);

/* Platform Device */
static struct platform_device plat0 = {
#if 1	/* Make 1 to test using different names in the platform device and driver;
		 * We find that the driver core then can't bind them, and the probe method
	 	 * is never invoked...
		 */
	.name = "plat",
	/* But then again, we can use the 'driver_override' member to force
	 * a match to the driver's name! Then it still works..
	 */
	.driver_override = DRVNAME,	/* Driver name to force a match! */
#else
	.name = DRVNAME,
#endif
	.id = 0,
	.dev = {
		.platform_data = NULL,
		/* (the old deprecated way):
		 *   Plug in the device's resources here: memory ranges/IRQs/
		 *   IOports/DMA channels/clocks/etc + optionally 'data'
		 *   (typically a private structure).
         *   The new way - on platforms that support it - is of course
		 *   to use the Device Tree (ARM/PPC) / ACPI tables (x86)
		 */
		.release = plat0_release,
	},
};

static struct platform_device *platdemo_platform_devices[] __initdata = {
	&plat0,
};

/* Platform Driver */
static struct platform_driver platdrv = {
	.probe = platdev_probe,
	.remove = platdev_remove,
	.driver = {
	   .name = DRVNAME,	// matches platform device name
	   .owner = THIS_MODULE,
	},
};

/* Most drivers have a global "context" struct that gets passed around .. */
typedef struct MyCtx {
	//struct net_device *netdev;
	int txpktnum, rxpktnum;
	int tx_bytes, rx_bytes;
	unsigned int data_xform;
	//spinlock_t lock;
} stMyCtx, *pstMyCtx;

/*--------------------------------------------------------------------*/

static int platdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	pstMyCtx pstCtx = dev_get_platdata(&pdev->dev);
	/* could have done the above directly with:
	 *  pstMyCtx pstCtx = pdev->dev.platform_data;
	 * .. but using the kernel helper is recommended
	 */

	pstCtx->data_xform = 100;
	dev_dbg(dev, "probe method invoked; setting 'data_xform' to 100\n");
	return 0;
}

static int platdev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	pstMyCtx pstCtx = dev_get_platdata(dev);

	dev_dbg(dev, "remove method invoked\n data_xform=%d\n",
	    pstCtx->data_xform);
	return 0;
}

static void plat0_release(struct device *dev)
{
	dev_dbg(dev, "release method invoked\n");
}

static int __init platdev_init(void)
{
	int res = 0;
	pstMyCtx pstCtx = NULL;

	pr_info("Initializing platform demo driver now...\n");

	pstCtx = kzalloc(sizeof(stMyCtx), GFP_KERNEL);
	if (!pstCtx)
		return -ENOMEM;
	plat0.dev.platform_data = pstCtx;	// convenient to access later

	/* platform_add_devices() is a wrapper over platform_device_register() */
	res = platform_add_devices(platdemo_platform_devices,
				  ARRAY_SIZE(platdemo_platform_devices));
	if (res) {
		pr_warn("platform_add_devices failed!\n");
		goto out_fail_pad;
	}

	// Register with the platform bus driver
	res = platform_driver_register(&platdrv);
	if (res) {
		pr_warn("platform_driver_register failed!\n");
		goto out_fail_pdr;
	}
	/* Successful platform_driver_register() will cause the registered 'probe'
	 * method to be invoked now..
	 */
	dev_dbg(&plat0.dev, "loaded.\n");
	return res;

 out_fail_pdr:
	platform_device_unregister(&plat0);
 out_fail_pad:
	kfree(pstCtx);
	return res;
}

static void __exit platdev_exit(void)
{
	struct device *dev = &plat0.dev;
	pstMyCtx pstCtx = plat0.dev.platform_data;

	dev_dbg(dev, "unloading\n");
	platform_driver_unregister(&platdrv);
	platform_device_unregister(&plat0);
	kfree(pstCtx);
}

module_init(platdev_init);
module_exit(platdev_exit);

MODULE_DESCRIPTION
("Simple demo platform driver; can use as a template for a platform device/driver");
MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_LICENSE("Dual MIT/GPL");
