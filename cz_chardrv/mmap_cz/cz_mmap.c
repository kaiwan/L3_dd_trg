/*
 * cz_mmap.c
 *
 * Simple demo char device driver for memory device.
 * Here, the new feature is the 'mmap' implementation.
 * This lets the userspace app map directly into the driver's kernel
 * memory (that has been zeroed out), *without any memory copying* from
 * kernel<->userspace! 
 *
 * In effect, this is a simple Zero-Copy implementation of the previous
 * 'cz_enh' char driver!
 *
 * Major # : 0 => dynamic major # allocation
 *
 * Implements two minor devices:
 * a) zero source        : minor # 1 : /dev/czero
 * b) sink (null device) : minor # 2 : /dev/cnul
 *
 * Author: Kaiwan N Billimoria <kaiwan@kaiwantech.com>
 * License: MIT
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* current, jiffies */
#include <linux/fs.h>		/* no_llseek */
#include <linux/version.h>      /* ver macros */
#include <linux/slab.h>		/* kmalloc */
#include <linux/mm.h>           /* remap_pfn_range */

//--- copy_[to|from]_user()
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,11,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include <asm/io.h>		/* virt_to_phys() */
#include "../../convenient.h"

#define	DRVNAME		"cz_mmap"
#define CZ_MAJOR  	0    /* 0 => dynamic major number assignment */

static int cz_major = CZ_MAJOR;

/* ----------The czero_* functionality routines 
 *
 * czero is designed to be a zero source
 */


static char *zbuf=NULL; // TODO- protection
static int czero_open(struct inode *inode, struct file *filp)
{
QP;
	return 0;
}
static int czero_close(struct inode *inode, struct file *filp)
{
QP;
	if (zbuf)
		MSG("zbuf non-null\n");
	else
		MSG("zbuf null\n");
	if (zbuf) {
		kfree (zbuf);
		zbuf = NULL;
	}
	return 0;
}

static ssize_t czero_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%ld\n", 
			current->comm, current->pid, count);
	return -ENOSYS;
}

#define MAX_MMAP_PAGES 3 	// in units of pages
static int czero_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int len=vma->vm_end - vma->vm_start, status=0;
QP;

	if (zbuf)
		kfree (zbuf);
	zbuf=NULL;
	
	MSG("vma: start=0x%08x end=0x%08x len=%d pgoff=%lu\n", 
		(u32)vma->vm_start, (u32)vma->vm_end, len, vma->vm_pgoff);

	/* Validity check: there is a hard limit (MAX_MMAP_OFFSET) pages that the userspace
	   app can mmap.
	*/
	if (len > (MAX_MMAP_PAGES*PAGE_SIZE)) {
		MSG("mmap length too large! (artificial max of %d pages here)\n", MAX_MMAP_PAGES);
		if (zbuf)
			MSG("zbuf non-null\n");
		else
			MSG("zbuf null\n");
		status = -EINVAL;
		goto out;
	}
	/* Validity check: don't let the app try to mmap beyond the to-be-allocated size.
	   vm_pgoff: offset (within vm_file) in PAGE_SIZE units, *not* PAGE_CACHE_SIZE */
	if ((vma->vm_pgoff*PAGE_SIZE) > len) {
		MSG("offset too large!\n");
		status = -EINVAL;
		goto out;
	}

	if ((zbuf = kzalloc(len, GFP_KERNEL)) == NULL) {
		status = -ENOMEM;
		goto out_no_mem;
	}
	// only freed in close (so that the userspace thread has access)

	/*
	Map entire range of physically contiguous pages..
	int remap_pfn_range(struct vm_area_struct *, unsigned long addr,
                        unsigned long pfn, unsigned long size, pgprot_t);
 	* remap_pfn_range - remap kernel memory to userspace
	 * @vma: user vma to map to
	 * @addr: target user address to start at
	 * @pfn: physical address of kernel memory
	 * @size: size of map area
	 * @prot: page protection flags for this mapping

	*/
	if ((status = remap_pfn_range(vma,
		vma->vm_start,
		virt_to_phys(zbuf) >> PAGE_SHIFT, // express in pages
		len,
		vma->vm_page_prot) < 0)) {
			MSG("remap_pfn_range failed!\n");
			goto out;
	}
	MSG("remap_pfn_range succeeded..\n");
out:
//	kfree (zbuf);
out_no_mem:
	return status;
}


/* ----------The cnul_* functionality routines 
*
* cnul is designed to be a sink
*/
static ssize_t cnul_read(struct file *filp, char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%ld\n", 
			current->comm, current->pid, count);

	/* as Linux does it, return 0 */
	return 0; 
		/* A read() to the nul device should always return 0 (EOF);
	           the advantage of doing it this way is that a user 
		   can do stuff like truncate file fname by doing: 
			 $ cat /dev/cnul > fname 
	or one could just:
	return -ENOSYS;   causing perror to show : "Function not implemented"
	*/
}

static ssize_t cnul_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *offp)
{
	MSG( "process %s [pid %d], count=%ld\n\tjiffies=%lu\n", 
			current->comm, current->pid, count, jiffies );
	return count;
	/* a write() to the nul device should always succeed! */
}


/* Minor-specific open routines */
static struct file_operations czero_fops = {
	.llseek =	no_llseek,
	.open   =	czero_open,
//	.read   =	czero_read,
	.write  =	czero_write,
	.mmap   =       czero_mmap,
	.release=	czero_close,
};

static struct file_operations cnul_fops = {
	.llseek =	no_llseek,
	.read =		cnul_read,
	.write =	cnul_write,
};

static int cz_open(struct inode * inode, struct file * filp)
{
	MSG( "Device node with minor # %d being used\n", iminor(inode));

	switch (iminor(inode)) {
		case 1:
			filp->f_op = &czero_fops;
			break;
		case 2:
			filp->f_op = &cnul_fops;
			break;
		default:
			return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode,filp); 
		   /* Minor-specific open : jumps to the 
		   suitable entry point - the correct open() call if 
	      	   one has been defined */
	return 0;
}

/* Major-wide open routine */
static struct file_operations czopen_fops = {
	.open =		cz_open, /* just a means to get at the real open */
};

/*
 * Register the char driver with the kernel.
 *
 *  On 2.6 kernels, we could use the new alloc_register_chrdev() function; 
 *  here, we use the "classic" register_chrdev() API.
 *	result = register_chrdev( CZ_MAJOR, "cz", &cz_fops );
 */
static int __init cz_init_module(void)
{
	int result;

	MSG( "cz_major=%d\n",cz_major);

	/*
	 * Register the major, and accept a dynamic number.
	 * The return value is the actual major # assigned.
	 */
	result = register_chrdev(cz_major, DRVNAME, &czopen_fops);
	if (result < 0) {
		MSG( "register_chrdev() failed trying to get cz_major=%d\n",
		cz_major);
		return result;
	}

	if (cz_major == 0) cz_major = result; /* dynamic */
	MSG( "registered:: cz_major=%d\n",cz_major);

	return 0; /* success */
}

static void __exit cz_cleanup_module(void)
{
	unregister_chrdev(cz_major, DRVNAME);
	MSG("Unregistered.\n");
}

module_init(cz_init_module);
module_exit(cz_cleanup_module);

module_param(cz_major, int, 0); /* 0 => param won't show up in sysfs, 
				   non-zero are mode (perms) */
MODULE_PARM_DESC(cz_major, "Major number to attempt to use");
MODULE_AUTHOR("Kaiwan");
MODULE_DESCRIPTION("Simple (demo) null and zero memory driver driver");
MODULE_LICENSE("Dual MIT/GPL");
