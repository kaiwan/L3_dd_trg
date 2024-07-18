/*
 * ioctl_kdrv.c
 *
 * Simple kernel module to demo using the ioctl method.
 * Device node:
 *  has to be manually created; major # is dynamic,
 *  minor# is 0.
 * (c) Kaiwan NB, kaiwanTECH.
 *
 * License: dual MIT/GPL
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/version.h>

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,11,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include "ioctl_kdrv.h"
#include "convenient.h"

#define MODULE_VER 	"0.1"
#define MODULE_NAME "ioctl_kdrv"

static int iok_major = 0, power = 1;

/* 
 * The ioctl method of this dummy/stub driver:
 * See the header ioctl_kdrv.h for "cmd" and "arg" usage details.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static long iok_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int iok_ioctl(struct inode *ino, struct file *filp, unsigned int cmd,
		     unsigned long arg)
#endif
{
	int err = 0, retval = 0;

	MSG("In ioctl method, cmd=%d\n", _IOC_NR(cmd));

	/* Check arguments - not for us? */
	if (_IOC_TYPE(cmd) != IOCTL_KDRV_MAGIC) {
		MSG("ioctl fail 1\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > IOCTL_KDRV_MAXIOCTL) {
		MSG("ioctl fail 2\n");
		return -ENOTTY;
	}

#if 0
	/* Verify direction */
	if (_IOC_DIR(cmd) & _IOC_READ)
		/* userspace read => kernel-space -> userspace write operation */
		err =
		    !access_ok(VERIFY_WRITE, (void __user *)arg,
			       _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		/* userspace write => userspace -> kernel-space read operation */
		err =
		    !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;
#endif
	switch (cmd) {
	case IOCTL_KDRV_IOCRESET:
		MSG("In option: IOCTL_KDRV_IOCRESET\n");
		break;
	case IOCTL_KDRV_IOCQPOWER:	/* Get: arg is pointer to result */
		MSG("In option: IOCTL_KDRV_IOCQPOWER\n\
arg=0x%x (drv) power=%d\n", (unsigned int)arg, power);
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		retval = __put_user(power, (int __user *)arg);
		break;
	case IOCTL_KDRV_IOCSPOWER:	/* Tell: arg is the value to set */
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		power = arg;
		MSG("In option: IOCTL_KDRV_IOCSPOWER\n\
power=%d now.\n", power);
		break;
	default:
		return -ENOTTY;	/* redundant, but still..; 
				   -ENOTTY: according to POSIX */
	}
	return retval;
}

static struct file_operations iok_fops = {
	.llseek = no_llseek,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	.unlocked_ioctl = iok_ioctl,	// use the 'unlocked' version!
	//.compat_ioctl  =      iok_ioctl,
#else
	.ioctl = iok_ioctl,
#endif
};

static int iok_open(struct inode *inode, struct file *filp)
{
	MSG("Device node with minor # %d being used\n", iminor(inode));

	switch (iminor(inode)) {
	case 0:
		filp->f_op = &iok_fops;
		break;
	default:
		return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode, filp);	/* Minor-specific open */

	return 0;
}

/* Major-wide open routine */
static struct file_operations iok_open_fops = {
	.open = iok_open,	/* just a means to get at the real open */
};

/*
 * function ioctl_kdrv_init
 */
static int __init ioctl_kdrv_init_module(void)
{
	int result;

	MSG("iok_major=%d\n", iok_major);

	/*
	 * Register the major, and accept a dynamic number.
	 * The return value is the actual major # assigned.
	 */
	result = register_chrdev(iok_major, MODULE_NAME, &iok_open_fops);
	if (result < 0) {
		MSG("register_chrdev() failed trying to get iok_major=%d\n",
		    iok_major);
		return result;
	}

	if (iok_major == 0)
		iok_major = result;	/* dynamic */
	MSG("registered:: iok_major=%d\n", iok_major);

	printk(KERN_INFO "%s %s initialized\n", MODULE_NAME, MODULE_VER);
	return 0;		/* success */
}

/*
 * function ioctl_kdrv_cleanup
 */
static void ioctl_kdrv_cleanup_module(void)
{
	unregister_chrdev(iok_major, MODULE_NAME);
	printk(KERN_INFO "%s %s removed\n", MODULE_NAME, MODULE_VER);
}

module_init(ioctl_kdrv_init_module);
module_exit(ioctl_kdrv_cleanup_module);

MODULE_AUTHOR("My Name");
MODULE_DESCRIPTION("A simple ioctl kernel usage demo");
MODULE_LICENSE("GPL");
/* End ioctl_kdrv.c */
