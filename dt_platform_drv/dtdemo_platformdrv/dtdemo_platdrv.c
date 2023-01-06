/* 
 * dtdemo_platdrv.c
 *
 * A platform driver (for a pseudo non-existant platform device); just to
 * demonstrate that what OF properties have been put in the Device Tree can
 * be retrieved and displayed in the platform driver!
 * Tested by modifying the DT of the Raspberry Pi 3B+.
 *
 * Kaiwan N Billimoria, kaiwanTECH
 * Dual MIT/GPL
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,11,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/of.h>   // of_* APIs (Open Firmware!)

#define DRVNAME "dtdemo_platdrv"

int dtdemo_platdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const char *prop = NULL;
	int len = 0;

	dev_dbg(dev, "platform driver probe enter\n");
	/* The DT node within the board DT is:
	 * ...
	 *  dtdemo_chip {
     *         compatible = "dtdemo,dtdemo_platdev";
     *         aproperty = "my prop 1";
     *  };
	 *
	 *  So, let's retrieve a property of the node by name, 'aproperty'
	 */
	if (pdev->dev.of_node) {
		prop = of_get_property(pdev->dev.of_node, "aproperty", &len);
		if (!prop) {
			pr_warn("%s: getting DT property failed\n", DRVNAME);
			return -1;
		}
		pr_info("%s: DT property 'aproperty' = %s (len=%d)\n", DRVNAME, prop, len);
	}
	else {
		pr_warn("couldn't access DT property!\n");
		return -EINVAL;
	}

	/* Initialize the device, mapping I/O memory, registering the interrupt handlers. The
     * bus infrastructure provides methods to get the addresses, interrupt numbers and
     * other device-specific information
	 */
	 // ...

	// Register the device to the proper kernel framework
	// eg. register_netdev(...);
	// ...

	return 0;
}

int dtdemo_platdev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "platform driver remove\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id my_of_ids[] = {
	/*
	 * DT compatible property syntax: <manufacturer,model> ...
	 * Can have multiple pairs of <oem,model>, from most specific to most general.
	 * This is especially important: it MUST EXACTLY match the 'compatible'
	 * property in the DT; *even a mismatched space will cause the match to
	 * fail* !
	 */
	{ .compatible = "dtdemo,dtdemo_platdev"},
	{},
};
MODULE_DEVICE_TABLE(of, my_of_ids);
#endif

static struct platform_driver my_platform_driver = {
	.probe = dtdemo_platdev_probe,
	.remove = dtdemo_platdev_remove,
	.driver = {
		.name = "dtdemo_platdev", /* platform driver name must
	       EXACTLY match the DT 'compatible' property - described in the DT
		   for the board - for binding to occur;
	       then, this module is loaded up and it's probe method invoked! */
#ifdef CONFIG_OF
		.of_match_table = my_of_ids,
#endif
		.owner = THIS_MODULE,
	}
};

static int dtdemo_platdrv_init(void)
{
	int ret_val;

	pr_info("%s: inserted\n", DRVNAME);

	// Register ourself to a bus
	ret_val = platform_driver_register(&my_platform_driver);
	if (ret_val !=0) {
		pr_err("platform value returned %d\n", ret_val);
		return ret_val;
	}
	return 0;	/* success */
}

static void dtdemo_platdrv_cleanup(void)
{
	platform_driver_unregister(&my_platform_driver);
	pr_info("%s: removed\n", DRVNAME);
}

module_init(dtdemo_platdrv_init);
module_exit(dtdemo_platdrv_cleanup);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_DESCRIPTION(
"Demo: setting up a DT node, so that this platform drv can get bound");
