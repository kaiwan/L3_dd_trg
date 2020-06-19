/*
 * A sample, extra-simple block driver.
 * Updated for kernel vers upto 4.19.
 *
 * Src URL:
 * http://blog.superpat.com/2010/05/04/a-simple-block-driver-for-linux-kernel-2-6-31/
 * (C) 2003 Eklektix, Inc.
 * (C) 2010 Pat Patterson <pat at superpat dot com>
 * Redistributable under the terms of the GNU GPL.
 *
 * Minor mods:
 *  - instrumentation (configurable printks, stack dump..)
 *  - verbose deconstruction of IO requests, displaying the BIO / biovec's / segments
 *  - tested on:
                 x86-64 Ubuntu 18.04 LTS w/ 4.15.0-88-generic
				 Raspberry Pi 3B+ (ARM Cortex A-53) w/ 4.19.97-v7+
				 x86-32 2.6.35
                 x86 (Ubuntu 11.10) 3.0.0-16-generic-pae
                 ARM Linux (QEMU) kernel ver 3.1.5 and 3.2.21
 *   -kaiwan.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include "../convenient.h"

#define MYDISKNAME	"sblock0"

// Instrumentation: Show full kernel dump stack (very verbose!) or just 1 info line ?
#define SHOW_K_DUMPSTACK	0 // set to 1 to show kernel stack
#if (SHOW_K_DUMPSTACK == 1)
 #define QP_or_QPDS QPDS
#else
 #define QP_or_QPDS do { \
	MSG("process %s(%d), irq-ctx? %s\n", \
	  current->comm, current->tgid, (in_interrupt() ? "y" : "n")); \
 } while(0)
#endif

MODULE_LICENSE("Dual BSD/GPL");

static int major_num = 0;
module_param(major_num, int, 0);
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);
static int nsectors = 1024; /* How big the drive is */
module_param(nsectors, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512

/*
 * Our request queue.
 */
static struct request_queue *Queue;

/*
 * The internal representation of our device.
 */
static struct sbd_device {
	unsigned long size;
	spinlock_t lock;
	u8 *data;
	struct gendisk *gd;
} Device;

/*
 * Handle an I/O request.
 *
 * Here, in this simple implementation, we just implement the I/O via a memcpy
 * operation..
 * See the 'Essential Linux Device Drivers' ch14 for a little more realistic
 * implementation involving r/w the disk controller registers (pseudo-hardware,
 * actually) to issue the command to "disk".
 */
static void sbd_transfer(struct sbd_device *dev, sector_t sector,
		unsigned long nsect, char *buffer, int write) 
{
	unsigned long offset = sector * logical_block_size;
	unsigned long nbytes = nsect * logical_block_size;

	MSG("loc 0x%llx, buffer ptr 0x%llx, nbytes %lu, dir:%s\n",
		(u64)(dev->data + offset), (u64)buffer, nbytes, (write?"w":"r"));
	if ((offset + nbytes) > dev->size) {
		pr_notice("sbd: Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}

	//print_hex_dump_bytes(" ", DUMP_PREFIX_ADDRESS, dev->data + offset, nbytes);

	if (write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

#if 0
/* struct request is defined in include/linux/blkdev.h */
static void show_req_details(struct request *req)
{
	struct gendisk *gd;

	assert(req != NULL);
	gd = req->rq_disk;

	// See all 'command type' bits in include/linux/blkdev.h:rq_cmd_type_bits
	//  1 == REQ_TYPE_FS (filesystem rqst)
	MSG("Data dir '%s', on (gen)disk: %s\n", 
			(rq_data_dir(req) ? "W" : "R"), gd->disk_name);
	//MSG("Cmd type %d, data dir '%s', on (gen)disk: %s\n", 
//		req->cmd_type, (rq_data_dir(req) ? "W" : "R"), gd->disk_name);

#define SHOW_POS 0 // set to 1 to show addn details
#if (SHOW_POS == 1) 
/* from <linux/blkdev.h> :
 * blk_rq_pos()             : the current sector
 * blk_rq_bytes()           : bytes left in the entire request
 * blk_rq_cur_bytes()       : bytes left in the current segment
 * blk_rq_err_bytes()       : bytes left till the next error boundary
 * blk_rq_sectors()         : sectors left in the entire request
 * blk_rq_cur_sectors()     : sectors left in the current segment
*/
	MSG("curr sector: %9u\n"
		"bytes   left in: entire req=%6d, curr segment=%6d\n"
		"sectors left in: entire req=%6d, curr segment=%6d\n",
			blk_rq_pos(req),
			blk_rq_bytes(req), blk_rq_cur_bytes(req), 
			blk_rq_sectors(req), blk_rq_cur_sectors(req));
#endif
/*
#ifdef CONFIG_BLK_DEV_INTEGRITY
	if (gd->integrity)
		MSG("block (integrity) name: %s\n", gd->integrity->name);
#endif
*/

#define SHOW_BLOCKDEV 0 // set to 1 to show block_device details
#if (SHOW_BLOCKDEV == 1) 
	// Superblock
	{
	struct block_device *bd = req->bio->bi_bdev;
	struct super_block *sb = bd->bd_super;
	assert(bd != NULL);

	MSG("block dev ptr = 0x%x\n", (u32)bd);
	MSG("bd_part_count = %d, invalidated=%d, bd_contains=0x%x\n", 
		bd->bd_part_count, bd->bd_invalidated, (u32)bd->bd_contains);

	//assert(sb != NULL); // fails! Why?
	}
#endif

#define SHOW_BIO 1 // set to 1 to show bio details
#if (SHOW_BIO == 1) 
  //----------Walk the bio's
  {
	struct bio *bio = req->bio;
	struct bio_vec *bvec;
	int i=0;
	struct bvec_iter iter;

	assert (bio != NULL);

	MSG("---bio=0x%lx biotail=0x%lx\n", (unsigned long)req->bio, (unsigned long)req->biotail);
	MSG("--> curr (start) sector= %9d nr_sectors= %6u\n",
			(int)blk_rq_pos(req), blk_rq_sectors(req));

	// --strictly speaking, should not be used directly; should use rq_for_each_segment instead...
	for_each_bio(bio) { // iterate over linked list of bio struct's
		int first=1;

		bio_get(bio);
		MSG(" %2d:BIO 0x%lx: # elements (i.e. segments) in this bio's biovec array: ## %d ##\n", 
			i, (unsigned long)bio, bio->bi_vcnt);
		MSG("      segments=%d, sectors=%d, phys segments=%d, flags=0x%x\n", 
			bio_segments(bio), bio_sectors(bio), bio->bi_phys_segments, (u32)bio->bi_flags);
		//MSG("      segments=%d, sectors=%d, residual IO count=%d, phys segments=%d, flags=0x%x\n", 
		//	bio_segments(bio), bio_sectors(bio), bio->bi_size, bio->bi_phys_segments, (u32)bio->bi_flags);

#define SEE_ALL_BIOS 0
#if (SEE_ALL_BIOS == 0)
		bio_for_each_segment(bvec, bio, iter)        // see only pending bio's
#else
		__bio_for_each_segment(bvec, bio, iter, 0)   // all: start at idx pos 0
#endif
			{
			if (first) { // display contents of bio_vec for first one only (subsequent vectors work on the same page)
				MSG("  %2d:SEG: page=0x%x, offset %u, len %u\n",
				//MSG("  %2d:SEG[idx %d]: page=0x%x, offset %u, len %u\n",
					//"                           [rem %2d to ", 
					iter->bi_idx, (u32)bio_page(bio), bio_offset(bio), bio_cur_bytes(bio)); //, i+1);
					//i, bio->bi_idx, (u32)bio_page(bio), bio_offset(bio), bio_cur_bytes(bio)); //, i+1);
				if (bio->bi_vcnt > 1)
					MSG_SHORT("                           [rem %2d to ", i+1);
				first=0;
			}
			else if ((bio->bi_vcnt > 1) && (i == bio->bi_vcnt-1)) // last biovec
				MSG_SHORT("                                      %2d] not shown", i);
		}
		bio_put(bio);
	} // for_each_bio
  }
#endif
}
#endif

/* The request function.
 * Remember, it could run asynchronously wrt the process that initiated the
 * IO request; in fact, it usually does. 
 * Also bear in mind that this function *could* even be invoked in an atomic
 * or interrupt context (with the kernel holding our spinlock)..
 * Thus, don't do anything that blocks or accesses userspace.
 *
 * FYI- Addn info - [un]plug:
 * With stack dump enabled (QPDS macro below), we'll see the k stack something
 * like this:
...
 ? sbd_request+0x2e/0x138 [sbd]
[58548.856768]  [<ffffffff812c3447>] ? __generic_unplug_device+0x37/0x40
[58548.856772]  [<ffffffff812c3750>] ? generic_unplug_device+0x30/0x50
[58548.856775]  [<ffffffff812bd415>] ? blk_unplug+0x25/0x60
[58548.856778]  [<ffffffff812bd462>] ? blk_backing_dev_unplug+0x12/0x20
[58548.856782]  [<ffffffff811914d1>] ? block_sync_page+0x41/0x50
[58548.856786]  [<ffffffff8110c14b>] ? sync_page+0x3b/0x50
...

* Fyi: What do the block 'unplug' calls mean?
* Pl see this article: http://lwn.net/Articles/438256/
* ..which also explains that the [un]plug block feature has been removed in 2.6.39 & why.
*/
static void sbd_request(struct request_queue *q) 
{
	struct request *req;

	pr_info("------------------\n");
	QP_or_QPDS;

	req = blk_fetch_request(q);
	
	while (req != NULL) {
		// blk_fs_request() was removed in 2.6.36 - many thanks to
		// Christian Paro for the heads up and fix...
		//if (!blk_fs_request(req)) 
		if (req == NULL) { // || (req->cmd_type != REQ_TYPE_FS)) {
			pr_notice("Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}

#if 0
		// comment out the 'show_req_details' fn below if it's too verbose...
		show_req_details(req);
#endif

		/* sbd_transfer(struct sbd_device *dev, sector_t sector, unsigned long nsect, 
		                char *buffer, int write) 
 		   blk_rq_pos()          : the current sector
		   blk_rq_cur_sectors()  : sectors left in the current segment
		 */
#if 0
		  MSG(" sbd_transfer(&Device 0x%x, start sector %d, # sectors %d, "
			  "buffer ptr 0x%x, data dir %s)\n" ,
		  		(u32)&Device, blk_rq_pos(req), blk_rq_cur_sectors(req), 
				(u32)req->buffer, (rq_data_dir(req) ? "1" : "0"));
#endif
#if 1
// ref: https://stackoverflow.com/questions/49781081/kernel-block-device
		sbd_transfer(&Device, blk_rq_pos(req), blk_rq_cur_sectors(req),
				     bio_data(req->bio), rq_data_dir(req));
				     /*req->buffer, rq_data_dir(req));*/
#else
		pr_info("skipped: sbd_transfer : memcpy ...\n");
#endif

		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}
	}
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int sbd_getgeometry(struct block_device * block_device, struct hd_geometry * geo) 
{
	long size;

	QP_or_QPDS;
	/* We have no real geometry, of course, so make something up. */
	size = Device.size * (logical_block_size / KERNEL_SECTOR_SIZE);
	MSG("size = %ld [%ld Kb, %ld Mb]\n", size, size/1024, size/(1024*1024));
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/* 
--to get here, mount the disk & do:
# fdisk -l /dev/sblock0
*/
static int sbd_open(struct block_device *bd, fmode_t mode)
{
	QP_or_QPDS;
	if (bd->bd_super)
		MSG("superblock blocksize: %lu\n", bd->bd_super->s_blocksize);
	return 0;
}
static void sbd_release(struct gendisk *gd, fmode_t mode)
{
	QP_or_QPDS;
}


/*
 * The (block) device operations structure.
 */
static struct block_device_operations sbd_ops = {
	.owner  = THIS_MODULE,
	.getgeo = sbd_getgeometry,
	.open   = sbd_open,
	.release = sbd_release,
};

static int __init sbd_init(void) 
{
	/*
	 * Set up our internal device.
	 */
	Device.size = nsectors * logical_block_size; // by default: 1024*512 = 512 KB
	spin_lock_init(&Device.lock);
	Device.data = vmalloc(Device.size);
	if (Device.data == NULL)
		return -ENOMEM;

	/*
	 * Get a request queue.
	 * Useful to read the comment in the src: block/blk-core.c:blk_init_queue .
	 */
	Queue = blk_init_queue(sbd_request, &Device.lock);
	if (Queue == NULL)
		goto out;
QP;

	blk_queue_logical_block_size(Queue, logical_block_size); /* sets logical block size for the Q; 
  	  should be set to the lowest possible block size that the storage device can address. 
	  512 is the typical default. */

	/*
	 * Get registered.
	 */
	major_num = register_blkdev(major_num, MYDISKNAME);
	if (major_num <= 0) {
		pr_warn("sbd: unable to get major number\n");
		goto out;
	}
QP;

	/*
	 * And the gendisk structure.
	 */
	Device.gd = alloc_disk(16); // param is # of minors (in effect, partitions)
	if (!Device.gd)
		goto out_unregister;
QP;
	Device.gd->major = major_num;
	Device.gd->first_minor = 0;
	Device.gd->fops = &sbd_ops;
	Device.gd->private_data = &Device;
	strcpy(Device.gd->disk_name, MYDISKNAME);
	set_capacity(Device.gd, nsectors);
	Device.gd->queue = Queue; // associate the request Q with the disk:
	             // every blk dev has an associated request queue
	add_disk(Device.gd); // registers partitioning info for this disk; 
						 // disk is now "live"!
	MSG("sbd: registered.\n");

	return 0;

out_unregister:
	unregister_blkdev(major_num, MYDISKNAME);
out:
	vfree(Device.data);
	return -ENOMEM;
}

static void __exit sbd_exit(void)
{
	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, MYDISKNAME);
	blk_cleanup_queue(Queue);
	vfree(Device.data);
	MSG("sbd: unregistered.\n");
}

module_init(sbd_init);
module_exit(sbd_exit);
