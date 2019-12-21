/*
 * badcdrv.c
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define	DRVNAME			"badcdrv"	// the device nodes will be named /dev/<DRVNAME>.<minor#>
#define DRV_MINOR_START			0
#define DRV_COUNT        		1	// # of devices (& device nodes) to create
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


static ssize_t badread(struct file *filp, char __user *buf, 
		size_t count, loff_t *offp)
{
	char *kbuf=NULL;
	int ret = count;

	if (count > PAGE_SIZE) // simplification
		count = PAGE_SIZE;

	MSG("\n-----PID %d. userspace dest addr = %pK (0x%lx)\n",
		current->tgid, buf, (unsigned long)buf);
	kbuf = kmalloc (count, GFP_KERNEL);
	if (!kbuf) {
		ret = -ENOMEM;
		goto out_mem_failed;
	}

	//buf=0x0;  // access_ok passes this!
	/* access_ok fails this (obvious) case.
 	   Implying, any address > PAGE_OFFSET is failed. */
	//buf=(void *)(PAGE_OFFSET+0x2000); 
	if (!access_ok (VERIFY_WRITE, buf, count)) {
		printk(KERN_ALERT "%s:%s:access_ok check failed!\n", DRVNAME, __func__);
		ret = -EFAULT;
		goto out_copy_failed;
	}
	MSG(" access_ok check passed.\n");

	ret = copy_to_user (buf, kbuf, count);
	if (ret) {
		printk(KERN_ALERT "%s:%s:copy_to_user failed! ret=%d\n", DRVNAME, __func__, ret);
		ret = -EIO;
		goto out_copy_failed;
	}
	MSG(" copy_to_user to 0x%08x succeeded!\n", (unsigned int)buf);

	ret = count;
out_copy_failed:
	kfree (kbuf);
out_mem_failed:
	return ret;
}

/* Minor-specific open routines */
static struct file_operations drv_fops = {
	.llseek =       no_llseek,
	.read   =       badread,
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

	res = alloc_chrdev_region(&chardrv_dev_number, DRV_MINOR_START, DRV_COUNT, DEVICE_FILE);
	if (res) {
		printk(KERN_WARNING "%s: could not allocate device\n", DRVNAME);
		return res;
	} else {
		printk (KERN_INFO "%s: registered with major number %d\n", DRVNAME, MAJOR(chardrv_dev_number));
	}

	chardrv_devp = kmalloc(DRV_COUNT * sizeof(struct chardrv_dev), GFP_KERNEL);
	if (NULL == chardrv_devp) {
		return -ENOMEM;
	}

	memset(chardrv_devp, 0, DRV_COUNT * sizeof(struct chardrv_dev));
	for (i = 0 ; i < DRV_COUNT; i++) {
		cdev_init (&chardrv_devp[i].cdev, &cdrv_open_fops);
		chardrv_devp[i].cdev.owner = THIS_MODULE;
		chardrv_devp[i].cdev.ops = &cdrv_open_fops;
		res = cdev_add (&chardrv_devp[i].cdev, MKDEV(MAJOR(chardrv_dev_number), MINOR(chardrv_dev_number)+i), 1);
		if (res) {
			printk(KERN_NOTICE "%s: Error on cdev_add for %d\n", DRVNAME, res);
		} else {
			printk(KERN_INFO "%s: cdev %s.%d added\n", DRVNAME, DRVNAME, i);
		}
	}

	/* Create the devices.
	 * Note: APIs class_create, device_create, etc exported as EXPORT_SYMBOL_GPL(...); so will not
  	 * show up unless the module license is GPL.
	 */
	chardrv_class = class_create (THIS_MODULE, DRVNAME);
	for ( i = 0 ; i < DRV_COUNT; i++) {
		if (!device_create(chardrv_class, NULL, MKDEV(MAJOR(chardrv_dev_number), MINOR(chardrv_dev_number)+i),
			NULL, "%s.%d", DRVNAME, i)) {
			printk(KERN_NOTICE "%s: Error creating device node /dev/%s.%d !\n", DRVNAME, DRVNAME, i);
		}
		else {
			printk(KERN_INFO " %s: Device node /dev/%s.%d created.\n", DRVNAME, DRVNAME, i);
		}
	}
	return res;
}


static int __init cdrv_init_module(void)
{
	chardev_registration();
	return 0; /* success */
}

static void __exit cdrv_cleanup_module(void)
{
	int i=0;

	/* Char driver unregister */
	for (i=0; i<DRV_COUNT; i++) {
		cdev_del(&chardrv_devp[i].cdev);
		device_destroy(chardrv_class, MKDEV(MAJOR(chardrv_dev_number), MINOR(chardrv_dev_number)+i));
	}
	class_destroy(chardrv_class);
	kfree(chardrv_devp);
	unregister_chrdev_region(chardrv_dev_number, DRV_COUNT);
	MSG("Unregistered.\n");
}

module_init(cdrv_init_module);
module_exit(cdrv_cleanup_module);

MODULE_AUTHOR("Me !");
MODULE_DESCRIPTION("Test illegal memory accesses (write) from char driver.");
MODULE_LICENSE("GPL");

