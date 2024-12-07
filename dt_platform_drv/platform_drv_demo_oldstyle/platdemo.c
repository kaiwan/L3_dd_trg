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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include "../../convenient.h"

MODULE_DESCRIPTION
    ("Simple demo platform driver; can use as a template for a platform device/driver");
MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_LICENSE("Dual MIT/GPL");

#define DRVNAME  "platdemo"
/*------------------------ Structs, prototypes, .. ------------------*/
static int platdev_probe(struct platform_device *);

// From 6.11, use the remove_new hook (until it's converted back); see commit #0edb555a65d1ef
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int platdev_remove(struct platform_device *);
#else
static void platdev_remove(struct platform_device *);
#endif
static void plat0_release(struct device *);

/* Platform Device */
static struct platform_device plat0 = {
#if 1		/*
		 * Make 0 to test using different names in the platform device
		 * and driver.
		 * We find that unless the name given here *precisely* matches
		 * the name provided in the platform_driver structure, the driver
		 * core can't bind them, and the probe method is never invoked...
		 */
	.name = "splat",	// oops, bad name...!
	/* ... But then again, we can use the 'driver_override' member to force
	 * a match to the driver's name! Then it still works..
	 */
	.driver_override = DRVNAME,	/* Driver name to force a match! */
#else
	.name = "wrong_name",
#endif
	.id = 0,
	.dev = {
		.platform_data = NULL,
		/* (the old deprecated way):
		 *   Plug in the device's resources here: memory ranges/IRQs/
		 *   IOports/DMA channels/clocks/etc + optionally 'data'
		 *   (typically a private structure).
		 *   The new way - on platforms that support it - is of course
		 *   to use the Device Tree (ARM* / PPC) / ACPI tables (x86)
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	.remove = platdev_remove,
#else
	.remove_new = platdev_remove,
#endif
	.driver = {
		   .name = DRVNAME,	/* IMP: matches platform device name, and
					 * thus the probe method gets invoked
					 */
		   /*.name = DRVNAME"xyz", *//* this will obviously *not* match,
		    * thus the probe method never gets invoked
		    */
		   .owner = THIS_MODULE,
		   },
};

/* Most drivers have a global "context" struct that gets passed around .. */
struct stMyCtx {
	//struct net_device *netdev; // or whichever appropriate struct 'specialized' by-type
	int txpktnum, rxpktnum;
	int tx_bytes, rx_bytes;
	unsigned int data_xform;
	//spinlock_t lock;
	//mutex mutex;
};

/*--------------------------------------------------------------------*/

static int platdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stMyCtx *pstCtx = dev_get_platdata(dev);
	/* could have done the above directly with:
	 *  ... *pstCtx = pdev->dev.platform_data;
	 * but using the kernel helper is recommended
	 */

	pstCtx->data_xform = 100;
	dev_dbg(dev, "platform drv probe method invoked! implies name match\n\
setting data_xform to 100\n");
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int platdev_remove(struct platform_device *pdev)
#else
static void platdev_remove(struct platform_device *pdev)
#endif
{
	struct device *dev = &pdev->dev;
	struct stMyCtx *pstCtx = dev_get_platdata(dev);

	dev_dbg(dev, "remove method invoked\n data_xform=%d\n", pstCtx->data_xform);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static void plat0_release(struct device *dev)
{
	dev_dbg(dev, "release method invoked\n");
}

static int __init platdev_init(void)
{
	int res = 0;
	struct stMyCtx *pstCtx = NULL;

	pr_info("Initializing platform demo driver now...\n");

	pstCtx = kzalloc(sizeof(struct stMyCtx), GFP_KERNEL);
	if (!pstCtx)
		return -ENOMEM;
	plat0.dev.platform_data = pstCtx;	// convenient to access later

	/*
	 * Here, we're simply 'manually' adding a platform device (old-style)
	 * via the platform_add_devices() API, which is a wrapper over
	 * platform_device_register().
	 * The preferred modern way is to do so via the Device Tree (DT)! In
	 * this (modern) approach, the DT devices are auto-generated as platform
	 * devices by the kernel at early boot
	 */
	res = platform_add_devices(platdemo_platform_devices,
				   ARRAY_SIZE(platdemo_platform_devices));
	if (res) {
		pr_warn("platform_add_devices() failed!\n");
		goto out_fail_pad;
	}

	/*
	 * As stated: (V IMP!)
	 * - In the init code: the client driver (us) must register itself to
	 * an appropriate bus driver (infrastructure) as it relies upon it
	 * - When recognized as a driver capable of driving a device that’s
	 * become visible (or has already been visible), the kernel bus driver
	 * invokes the client’s driver probe() method
	 * - In probe(), it could register itself to some existing kernel
	 * framework
	 */

	// Register ourself with the platform bus driver, as we're a platform device
	res = platform_driver_register(&platdrv);
	if (res) {
		pr_warn("platform_driver_register failed!\n");
		goto out_fail_pdr;
	}
	/*
	 * Successful platform_driver_register() will cause the platform bus to,
	 * as it runs its 'match dev-to-drv' loop, match this platform driver to
	 * the specified (pseudo) platform device! And thus have the registered
	 * 'probe' method to be invoked now..
	 *
	 * $ cat /sys/devices/platform/splat.0/driver_override 
	 * platdemo
	 * $ cat /sys/devices/platform/splat.0/modalias        
	 * platform:splat
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
	struct stMyCtx *pstCtx = dev_get_platdata(dev);
	//struct stMyCtx *pstCtx = plat0.dev.platform_data;

	dev_dbg(dev, "unloading\n");
	platform_driver_unregister(&platdrv);
	platform_device_unregister(&plat0);
	kfree(pstCtx);
}

module_init(platdev_init);
module_exit(platdev_exit);
