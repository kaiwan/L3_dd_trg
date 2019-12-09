/*
 * Char driver 'template' for using the misc major #10 and getting
 * a dynamic minor number.
 * The device file needs to be separately created..
 * Originally from: 'Linux Driver Dev for Embedded Processors', Alberto Liberal
 * Slightly modified, Kaiwan NB.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#define MYDRVNAME    "miscdrv"

static int my_dev_open(struct inode *inode, struct file *file)
{
	pr_info("%s:%s() called.\n"
		" minor # is %d\n",
		MYDRVNAME, __func__, iminor(inode));
	return 0;
}

static ssize_t my_dev_read(struct file *file, char __user *buf,
	size_t count, loff_t *off)
{
	pr_info("%s:%s() called.\n"
		" request to read %zu bytes\n",
		MYDRVNAME, __func__, count);
	return count;
}

static ssize_t my_dev_write(struct file *file, const char __user *buf,
	size_t count, loff_t *off)
{
	pr_info("%s:%s() called.\n"
		" request to write %zu bytes\n",
		MYDRVNAME, __func__, count);
	return count;
}

static int my_dev_close(struct inode *inode, struct file *file)
{
	pr_info("%s:%s() called.\n", MYDRVNAME, __func__);
	return 0;
}

static long my_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_info("%s:%s() called.\n"
		" cmd = %d, arg = %ld\n"
		, MYDRVNAME, __func__, cmd, arg);
	return 0;
}

static const struct file_operations my_dev_fops = {
	.owner = THIS_MODULE,
	.open = my_dev_open,
	.read = my_dev_read,
	.write = my_dev_write,
	.release = my_dev_close,
	.unlocked_ioctl = my_dev_ioctl,
};

/* declare & initialize struct miscdevice */
static struct miscdevice my_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR, /* dynamically allocate an available minor # */
	.name = "mydev",             /* when misc_register() is invoked, the kernel
	                               will auto-create device file as /dev/mydev */
	.fops = &my_dev_fops,        /* functionality */
};

static int __init miscdrv_init(void)
{
	int ret_val;
	pr_info("%s: misc char driver init\n", MYDRVNAME);

	/* Register the device with the Kernel */
	ret_val = misc_register(&my_miscdevice);
	if (ret_val != 0) {
		pr_err("%s: could not register the misc device mydev", MYDRVNAME);
		return ret_val;
	}

	pr_info("%s: mydev: got minor %i\n", MYDRVNAME, my_miscdevice.minor);
	return 0;
}

static void __exit miscdrv_exit(void)
{
	pr_info("%s: misc char driver exit\n", MYDRVNAME);

	/* Unregister the device with the kernel */
	misc_deregister(&my_miscdevice);
}

module_init(miscdrv_init);
module_exit(miscdrv_exit);

MODULE_LICENSE("GPL");
