/*
 * dtdemo_platdrv.c
 *
 * A platform driver (for a pseudo non-existant platform device); just to
 * demonstrate that what OF properties have been put in the Device Tree can
 * be retrieved and displayed in the platform driver!
 * Tested by modifying the DT of the Raspberry Pi 3B+ (or whichever).
 *
 * For the Raspberry Pi boards, a sample DT node in the dts for our fake
 * platform device:
 * (The node name doesn't matter; it's the *compatible* property that matches
 *  with the same in this driver!):
 *
 * ---------------------------------------------
 * F.e. for the Raspberry Pi 4B:
 * ---------------------------------------------
 * in rpi4b_dtb_stuff/rpi4b_gen.dts :
 * ...
 * soc {
 * ...
 *        dtdemo_chip {
 *                compatible = "dtdemo,dtdemo_platdev";
 *                aproperty = "my prop 1";
 * 		  my_value = <14>;
 *                status = "okay";
 *        };
 * ...
 *
 * Working:
 * - Convert the existing DTB to DTS:
 *
 * - compile the DTS into a DTB - a device tree blob binary
 *   (or, easier, generate a DT overlay 'fragment' (a .dtbo) and specify it at
 *    boot (via the /boot/config.txt))
 * - boot via this DTB (or DT overlay .dtbo)
 * - the kernel 'unrolls'/parses the DTB/DTBO at boot via it's OF APIs:
 *    - it thus 'sees' the new dtdemo_chip (pseudo) device
 *    - it constructs it as a platform device
 *    - the platform bus driver is running it's 'match loop' looking to match
 *      a platform device to it's driver via the 'compatible' property
 * - this driver loads up, registering itse;f as a platform driver, with the
 *   *same* 'compatible' string
 * - this causes a ,atch! and voila, the bus driver invokes the driver's probe
 *   routine, and we're on the way to driving it!
 *
 * ---------------------------------------------
 * For the BBB - Beagle Bone Black
 * ---------------------------------------------
 * 1. Setup the DTBO to get parsed at boot:
 *    - Will have to boot via eMMC internal flash (as the /boot stuff's there)
 *    - compile the DTS to the DTBO (DT overlay blob)
 *    - copy it into the appropriate location: /boot/dtbs/5.10.168-ti-r79/overlays/BB-BONE-DTDEMO-01.dtbo
 *    - insert this line into /boot/uEnv.txt to enable our DTBO to get loaded up on boot:
 *      dtb_overlay=/boot/dtbs/5.10.168-ti-r79/overlays/BB-BONE-DTDEMO-01.dtbo
 * 2. Boot via eMMC flash (remove the uSD card & boot)
 * 3. U-Boot prints should show that our DTBO got loaded
 *    ...
 *    uboot_overlays: loading /boot/dtbs/5.10.168-ti-r79/overlays/BB-HDMI-TDA998x-00A0.dtbo ...
 *   5321 bytes read in 6 ms (865.2 KiB/s)
 *   uboot_overlays: [dtb_overlay=/boot/dtbs/5.10.168-ti-r79/overlays/BB-BONE-DTDEMO-01.dtbo] ...
 *   uboot_overlays: loading /boot/dtbs/5.10.168-ti-r79/overlays/BB-BONE-DTDEMO-01.dtbo ...
 *   432 bytes read in 6 ms (70.3 KiB/s)
 *   loading /boot/initrd.img-5.10.168-ti-r79 ...
 *   ...
 *   After getting a shell:
 *    $ cat /proc/device-tree/my_device/compatible
 *    dtdemo,dtdemo_platdev
 * 4. Compile and run the driver; the probe method gets called, priving it worked!
 *    sudo insmod ././dtdemo_platdrv.ko
 *    dmesg
 *    ...
 *    [  185.450304] dtdemo_platdrv:dtdemo_platdrv_init(): inserted
 *    [  185.450591] dtdemo_platdev my_device: platform driver probe enter
 *    [  185.450612] dtdemo_platdev my_device: DT property 'aproperty' = my prop 1 (len=10)
 *  Done!
 *
 * ---------------------------------------------
 * Kaiwan N Billimoria, kaiwanTECH
 * License: Dual MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/of.h>   // of_* APIs (OF = Open Firmware!)

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_DESCRIPTION(
"Demo: setting up a DT node, so that this demo platform driver can get bound");

int dtdemo_platdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const char *prop = NULL;
	s32 myval;
	int len = 0;

	dev_dbg(dev, "platform driver probe enter\n");
	/*  So, let's retrieve the property of the node by name, 'aproperty' */
	if (pdev->dev.of_node) {
		prop = of_get_property(pdev->dev.of_node, "aproperty", &len);
		if (!prop) {
			dev_warn(dev, "getting DT property 'aproperty' failed\n");
			return -1;
		}
		dev_info(dev, "DT property 'aproperty' = %s (len=%d)\n", prop, len);

		// Note that the ret value isn't the property value..(the 3rd param is)
		len = of_property_read_s32(pdev->dev.of_node, "my_value", &myval);
		if (len < 0) {
			dev_warn(dev, "getting DT property 'my_value' failed\n");
			return -1;
		}
		dev_info(dev, "DT property 'my_value' = %d\n", myval);
	} else {
		dev_warn(dev, "couldn't access DT property!\n");
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
int dtdemo_platdev_remove(struct platform_device *pdev)
#else
void dtdemo_platdev_remove(struct platform_device *pdev)
#endif
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "platform driver remove\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
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
		.name = "dtdemo_platdev",
		/* platform driver name must
		 * EXACTLY match the DT 'compatible' property - described in the DT [overlay]
		 * for the board - for binding to occur;
		 * then, this module is loaded up and it's probe method invoked!
		 */
#ifdef CONFIG_OF
		.of_match_table = my_of_ids,
#endif
		.owner = THIS_MODULE,
	}
};

static int dtdemo_platdrv_init(void)
{
	int ret_val;

	pr_info("inserted\n");

	// Register ourself to the platform bus, as we're a platform device
	ret_val = platform_driver_register(&my_platform_driver);
	if (ret_val != 0) {
		pr_err("platform value returned %d\n", ret_val);
		return ret_val;
	}
	return 0;	/* success */
}
static void dtdemo_platdrv_cleanup(void)
{
	platform_driver_unregister(&my_platform_driver);
	pr_info("removed\n");
}
module_init(dtdemo_platdrv_init);
module_exit(dtdemo_platdrv_cleanup);
