/*
 * chardrv_template.c
 * A "template" for modern 2.6 "class-style" character driver.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#define	DRVNAME			"chardrv"	// the device nodes will be named /dev/<DRVNAME>.<minor#>
#define DRV_MINOR_START			0
#define DEV_COUNT        		2	// # of devices (& device nodes) to create
#define DEVICE_FILE		"/dev/cdrv"

#ifdef DEBUG
	#define MSG(string, args...) \
 	   printk(KERN_DEBUG "%s:%s:%d: " string, \
  	   DRVNAME, __FUNCTION__, __LINE__, ##args)
#else
	#define MSG(string, args...)
#endif

dev_t chardrv_dev_number;
struct chardrv_dev {
	char name[10];
	struct cdev cdev;     /* Char device structure      */
} *chardrv_devp;
struct class *chardrv_class=NULL;

/* Minor-specific open routines */
static struct file_operations drv_fops = {
	.llseek =       no_llseek,
	/* Add char device f_ops above this line */
};

static int cdrv_open(struct inode * inode, struct file * filp)
{
	MSG( "Device node with minor # %d being used\n", iminor(inode));

	switch (iminor(inode)) {
		case 0:
			filp->f_op = &drv_fops;
			break;
		default:
			return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode,filp);
	return 0;
}

/* Major-wide open routine */
static struct file_operations cdrv_open_fops = {
	.open =         cdrv_open, /* just a means to get at the real open */
};

/*--- Dynamic Char Device Registration & device nodes creation---------*/
static int chardev_registration(void)
{
    int res=0,i=0;

	res = alloc_chrdev_region(&chardrv_dev_number, DRV_MINOR_START, DEV_COUNT, DEVICE_FILE);
	if (res) {
		pr_warn("%s: could not allocate device\n", DRVNAME);
		return res;
	} else {
		pr_info("%s: registered with major number %d\n", DRVNAME, MAJOR(chardrv_dev_number));
	}

	chardrv_devp = kzalloc(DEV_COUNT * sizeof(struct chardrv_dev), GFP_KERNEL);
	if (NULL == chardrv_devp) {
		pr_warn("%s: kzalloc failed!\n", DRVNAME);
		return -ENOMEM;
	}

	for (i=0 ; i<DEV_COUNT; i++) {
		cdev_init (&chardrv_devp[i].cdev, &cdrv_open_fops);
		chardrv_devp[i].cdev.owner = THIS_MODULE;
		chardrv_devp[i].cdev.ops = &cdrv_open_fops;
		res = cdev_add (&chardrv_devp[i].cdev, MKDEV(MAJOR(chardrv_dev_number), 
			   MINOR(chardrv_dev_number)+i), 1);
		if (res) {
			pr_notice("%s: Error on cdev_add for %d\n", DRVNAME, res);
			return res;
		} else {
			pr_info("%s: cdev %s.%d added\n", DRVNAME, DRVNAME, i);
		}
	}

	/* Create the devices.
	 * !Note! : APIs class_create, device_create, etc exported as EXPORT_SYMBOL_GPL(...); 
	 * so will not show up unless the module license is GPL.
	 */
	chardrv_class = class_create (THIS_MODULE, DRVNAME);
	for ( i=0 ; i<DEV_COUNT; i++) {
		if (!device_create(chardrv_class, NULL, MKDEV(MAJOR(chardrv_dev_number), 
		      MINOR(chardrv_dev_number)+i), NULL, "%s.%d", DRVNAME, i)) {
			pr_notice("%s: Error creating device node /dev/%s.%d !\n", 
				DRVNAME, DRVNAME, i);
			return res;
		}
		else {
			pr_info(" %s: Device node /dev/%s.%d created.\n", DRVNAME, DRVNAME, i);
		}
	}
	return res;
}


static int __init cdrv_init_module(void)
{
	return chardev_registration();
}

static void __exit cdrv_cleanup_module(void)
{
	int i=0;

	/* Char driver unregister */
	for (i=0; i<DEV_COUNT; i++) {
		cdev_del(&chardrv_devp[i].cdev);
		device_destroy(chardrv_class, MKDEV(MAJOR(chardrv_dev_number), MINOR(chardrv_dev_number)+i));
	}
	class_destroy(chardrv_class);
	kfree(chardrv_devp);
	unregister_chrdev_region(chardrv_dev_number, DEV_COUNT);
	MSG("Unregistered.\n");
}

module_init(cdrv_init_module);
module_exit(cdrv_cleanup_module);

MODULE_AUTHOR("Your Name here");
MODULE_DESCRIPTION("Write a one-line description of the char device driver here.");
#if 1
MODULE_LICENSE("GPL"); /* necessary for the device API */
#endif
/*
 If not released under license "GPL", we get these errors upon insmod:
$ dmesg
...
chardrv_template: Unknown symbol __class_create (err 0)
chardrv_template: Unknown symbol class_destroy (err 0)
chardrv_template: Unknown symbol device_create (err 0)
chardrv_template: Unknown symbol device_destroy (err 0)

as the above routines are declared via the EXPORT_SYMBOL_GPL() macro!
 */
