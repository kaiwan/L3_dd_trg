/*
 * ldm_template_drv.c
 *
 * This 'template' does NOT propose to be complete in any manner whatsoever.
 * Use at your own discretion... it's written in the "hope it might be useful"
 * spirit of course...
 * ALSO, it's just pseudo-code; we expect you'll make a copy and edit it,
 * correcting and adding stuff as you go along...
 *
 * (c) 2020 Kaiwan N Billimoria, kaiwanTECH
 * Dual MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__
#define dev_fmt(fmt) "%s(): " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/device.h>
//#include <linux/i2c.h>

MODULE_AUTHOR("<your name, email id>");
MODULE_DESCRIPTION("<driver 1 line decription>");
MODULE_LICENSE("<license>");
	/* one of:
	 * "GPL"						[GNU Public License v2 or later]
	 * "GPL v2"						[GNU Public License v2]
	 * "GPL and additional rights"	[GNU Public License v2 rights and more]
	 * "Dual BSD/GPL"				[GNU Public License v2
	 *									or BSD license choice]
	 * "Dual MIT/GPL"				[GNU Public License v2
	 *									or MIT license choice]
	 * "Dual MPL/GPL"				[GNU Public License v2
	 *									or Mozilla license choice]
	 * "Proprietary"				[Non free products]
	 */


/* The probe method of our driver */
static int <chipname>_probe(struct <foo>_client/dev *client/dev, // named as 'client' or 'dev'
                const struct <foo>_device_id *id)
{
	/*
	 * first param: struct <foo>_client *client
	 * the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, platform, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;

	/*
	 * Your work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */

	return 0;
}


/*------- Matching the driver to the device ------------------
 * 3 different ways: 
 *  by name : for platform & I2C devices
 *  by DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 *  by ACPI ID : for devices on ACPI tables (x86)
 */

/* 
 * 1. By name : for platform & I2C devices
 * The <foo>_device_id structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pnp, platform, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static const struct <foo>_device_id <chipname>_id[] = { 
    { "<chipname>", 0 },		/* matching by name; required for platform and i2c
									devices & drivers */
    // f.e.: { "pcf8563", 0 },
    { } 
};
MODULE_DEVICE_TABLE(<foo>, <chipname>_id);

/* 2. By DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 */
#ifdef CONFIG_OF
static const struct of_device_id <chipname>_of_match[] = {
	/* 
	 * ARM/PPC/etc: matching by DT 'compatible' property
	 * 'compatible' property: one or more strings that define the specific
	 * programming model for the device. This list of strings should be used
	 * by a client program for device driver selection. The property value
	 * consists of a concatenated list of null terminated strings,
	 * from most specific to most general. 
	 */
    { .compatible = "<manufacturer>,<model-abc-xyz>", "<oem>,<model-abc>", /*<...>*/ },
 // f.e.:   { .compatible = "nxp,pcf8563" },
    {}
};
MODULE_DEVICE_TABLE(of, <chipname>_of_match);
#endif

/* 3. By ACPI ID : for devices on ACPI tables (x86) */
static const struct acpi_device_id <chipname>_acpi_id[] = {
						/* x86: matching by ACPI ID */
	{"MMA7660", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, <chipname>_acpi_id);



/*
 * The <foo>_driver structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 */
static struct <foo>_driver <chipname>_driver = {
	.driver     = {
		.name   = "<driver-name-used-for-match>",  /* platform and I2C use the
					'name' field for the match and thus the bind between the
					DT desc/device and driver */
		.of_match_table = of_match_ptr(<chipname>_of_match),
	},
	.probe      = <chipname>_probe,		// invoked on driver/device bind
	.remove     = <chipname>_remove,	// optional; invoked on driver/device detach
	.disconnect = <chipname>_disconnect,// optional; invoked on device disconnect

	.id_table   = <chipname>_id,

	.suspend    = <chipname>_suspend,	// optional
	.resume     = <chipname>_resume,	// optional
};


/* 
 * Init and Cleanup ::
 * Instead of manually specifiying the init and cleanup handlers in the
 * 'usual' manner, a lot of boilerplate is avoided (when nothing special is
 * required in the init and cleanup code paths) by using the
 *
 * module_foo_driver() macro;
 *
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, platform, pnp, scsi, sdio, serio, spi, tty, usb, usb_serial, vme
 * There are several foo_register_driver() APIs; see a list (for 5.4.0) here:
 *  https://gist.github.com/kaiwan/04cfaca711aed9e59282601fafd8aa24
 */
module_<foo>_driver(<chipname>_driver);
