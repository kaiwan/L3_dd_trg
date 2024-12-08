/*
 * L3_dd_trg/input_drv_mouse
 * Mostly from the excellent ELDD book...
 * vms = 'virtual mouse'
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

// copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

MODULE_DESCRIPTION("Input driver - example, simple mouse emulator demo; ref: ELDD book");
/*
 * We *require* the module to be released under GPL license (as well) to please
 * several core driver routines (like sysfs_create_group,
 * platform_device_register_simple, etc which are exported to GPL only (using
 * the EXPORT_SYMBOL_GPL() macro))
 */
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("0.1");

#define SYSFS_FILE1		vms

static struct input_dev *vms_input_dev;
static struct platform_device *sysfs_demo_platdev;	/* Device structure */

/* Note that in both the show and store methods, the buffer 'buf' is
 * a *kernel*-space buffer. (So don't try copy_[from|to]_user stuff!)
 *
 * From linux/device.h:
--snip--
// interface for exporting device attributes
struct device_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
};
*/

static ssize_t vms_store(struct device *dev,
			 struct device_attribute *attr, const char *buf, size_t count)
{
	int x, y;

	/*
	 * A userspace process - coord_gen - is emulating our virtual mouse
	 * movements by continually (once/sec) writing (x,y,z) (random) coordinates
	 * to us; they arrive in 'buf'. Read them in and process (we ignore the 'z'
	 * value (always 0))
	 */
	sscanf(buf, "%d%d", &x, &y);
	dev_dbg(dev, "%s(): count=%zu; (%d, %d) reported up\n", __func__, count, x, y);

	/* Report relative coordinates via the event interface */
	input_report_rel(vms_input_dev, REL_X, x);
	input_report_rel(vms_input_dev, REL_Y, y);
	input_sync(vms_input_dev);

	/*
	   "- The first statement generates a REL_X event or a relative device movement in the X direction.
 	    - The second produces a REL_Y event or a relative movement in the Y direction. 
	    - input_sync() indicates that this event is complete, so the input subsystem collects these
	   two events into a single evdev packet and sends it out of the door through /dev/input/eventX, 
	   where X is the interface number assigned to the vms driver. 

	   An application reading this file will receive event packets in the input_event format described earlier.

	   Run the coords_gen user mode app in one window.. 

	   Can use the evtest app to 'see' events as they occur: f.e.:
	   $ sudo evtest /dev/input/event7
	   Input driver version is 1.0.1
	   Input device ID: bus 0x0 vendor 0x0 product 0x0 version 0x0
	   Input device name: "Unknown"
	   Supported events:
	   Event type 0 (EV_SYN)
	   Event type 2 (EV_REL)
	   Event code 0 (REL_X)
	   Event code 1 (REL_Y)
	   Properties:
	   Testing ... (interrupt to exit)
	   Event: time 1610437930.721948, type 2 (EV_REL), code 0 (REL_X), value -3
	   Event: time 1610437930.721948, type 2 (EV_REL), code 1 (REL_Y), value 6
	   Event: time 1610437930.721948, -------------- SYN_REPORT ------------
	   Event: time 1610437931.724963, type 2 (EV_REL), code 0 (REL_X), value -17
	   Event: time 1610437931.724963, type 2 (EV_REL), code 1 (REL_Y), value -15
	   Event: time 1610437931.724963, -------------- SYN_REPORT ------------
	   Event: time 1610437932.745446, type 2 (EV_REL), code 0 (REL_X), value -13
	   Event: time 1610437932.745446, type 2 (EV_REL), code 1 (REL_Y), value -15
	   Event: time 1610437932.745446, -------------- SYN_REPORT ------------
	   [ ... ]
	   ^C$

	   Alternately, to request gpm (general purpose mouse) to attach to this event interface
	   and accordingly chase the cursor around your screen, do this:
	   sudo gpm -m /dev/input/eventX -t evdev
	     # -m : Choose the mouse file to open. Must be before -t and -o.
	     # -t : name
              Set the mouse type. Use `-t help' to get a list of allowable types.  Use -t after  you  selected
              the mouse device with -m.
	 */

	return count;
}

/* The DEVICE_ATTR{_RW|RO|WO}() macro instantiates a struct device_attribute
 * dev_attr_<name> here...
 * The name of the 'show' callback function is llkdsysfs_pgoff_show
 */
static DEVICE_ATTR_WO(vms);	/* it's store callback is above.. */

/*
 * From <linux/device.h>:
DEVICE_ATTR{_RW} helper interfaces (linux/device.h):
--snip--
#define DEVICE_ATTR_RW(_name) \
    struct device_attribute dev_attr_##_name = __ATTR_RW(_name)
#define __ATTR_RW(_name) __ATTR(_name, 0644, _name##_show, _name##_store)
--snip--
and in <linux/sysfs.h>:
#define __ATTR(_name, _mode, _show, _store) {              \
	.attr = {.name = __stringify(_name),               \
		.mode = VERIFY_OCTAL_PERMISSIONS(_mode) }, \
	.show   = _show,                                   \
	.store  = _store,                                  \
}
 */

static int __init input_drv_vmouse_init(void)
{
	int stat = 0;

	if (unlikely(!IS_ENABLED(CONFIG_SYSFS))) {
		pr_warn("sysfs unsupported! Aborting ...\n");
		return -EINVAL;
	}
	// LDM: register with a bus; here we register our platform device with the platform bus

	/* 0. Register a (dummy) platform device; required as we need a
	 * struct device *dev pointer to create the sysfs file via
	 * the device_create_file() API:
	 *  struct platform_device *platform_device_register_simple(
	 *              const char *name, int id,
	 *              const struct resource *res, unsigned int num);
	 */
#define PLAT_NAME	"vms"
	sysfs_demo_platdev = platform_device_register_simple(PLAT_NAME, -1, NULL, 0);
	if (IS_ERR(sysfs_demo_platdev)) {
		stat = PTR_ERR(sysfs_demo_platdev);
		pr_info("error (%d) registering our platform device, aborting\n", stat);
		goto out1;
	}
	// 1. Create our sysfile file :
	/* The device_create_file() API creates a sysfs attribute file for
	 * given device (1st parameter); the second parameter is the pointer
	 * to it's struct device_attribute structure dev_attr_<name> which was
	 * instantiated by our DEV_ATTR{_RW|RO} macros above ...
	 * API used:
	 * int device_create_file(struct device *dev,
	 *              const struct device_attribute *attr);
	 */
	stat = device_create_file(&sysfs_demo_platdev->dev, &dev_attr_vms);
	/*
	 * A potentially confusing aspect: as this is the first time, we
	 * explain it via this comment:
	 * The &dev_attr_vms structure seen above (2nd param to the
	 * device_create_file() API), is actually *instantiated* via this
	 * declaration above:
	 *  static DEVICE_ATTR_WO(vms);
	 * This DEVICE_ATTR{_RW|RO|WO}() macro instantiates a
	 *  struct device_attribute dev_attr_<name>    data structure!
	 * ... and hence we automatically get the rd/wr callbacks registered.
	 * (IOW, the DEVICE_ATTR_XX(name) macro becomes a
	 *  struct device_attribute dev_attr_name data structure!)
	 */
	if (stat) {
		pr_info("device_create_file failed (%d), aborting now\n", stat);
		goto out2;
	}
	pr_debug("sysfs file (/sys/devices/platform/%s/%s) created\n", PLAT_NAME, "vms");

	/* Allocate an input device data structure */
	vms_input_dev = input_allocate_device();
	if (!vms_input_dev) {
		dev_warn(&sysfs_demo_platdev->dev, "input_alloc_device() failed\n");
		goto out3;
	}
	vms_input_dev->name = "virtual mouse (vms)";
	vms_input_dev->phys = "virtual/input0";

	/* Announce that the virtual mouse will generate relative coordinates */
	set_bit(EV_REL, vms_input_dev->evbit);
	/* declare the event codes that our virt mouse produces */
	set_bit(REL_X, vms_input_dev->relbit);	// rel 'x' movement
	set_bit(REL_Y, vms_input_dev->relbit);	// rel 'y' movement

#if 0
	// If your virtual mouse is also capable of generating button clicks, you need to add this
	set_bit(EV_KEY, vms_input_dev->evbit);	/* Event Type is EV_KEY */
	set_bit(BTN_0, vms_input_dev->keybit);	/* Event Code is BTN_0 */
#endif

	// LDM: register with a kernel framework; here we register with the input subsystem
	stat = input_register_device(vms_input_dev);
	if (stat) {
		input_free_device(vms_input_dev);
		goto out3;
	}
	pr_info("Virtual Mouse Driver Initialized.\n");

	return 0;		/* success */
 out3:
	device_remove_file(&sysfs_demo_platdev->dev, &dev_attr_vms);
 out2:
	platform_device_unregister(sysfs_demo_platdev);
 out1:
	return stat;
}

static void __exit input_drv_vmouse_cleanup(void)
{
	/* Unregister from kernel input framework */
	input_unregister_device(vms_input_dev);

	/* Cleanup sysfs nodes */
	device_remove_file(&sysfs_demo_platdev->dev, &dev_attr_vms);

	/* Unregister the (dummy) platform device */
	platform_device_unregister(sysfs_demo_platdev);
	pr_info("removed\n");
}

module_init(input_drv_vmouse_init);
module_exit(input_drv_vmouse_cleanup);
