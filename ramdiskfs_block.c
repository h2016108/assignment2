/* Disk on RAM Driver */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/types.h>
//#include <linux/spinlock.h>
#include <linux/genhd.h> // For basic block driver framework
#include <linux/blkdev.h> // For at least, struct block_device_operations
#include <linux/hdreg.h> // For struct hd_geometry
#include <linux/errno.h>

#include "ramdiskfs.h"

#define RB_FIRST_MINOR 0
#define RB_MINOR_CNT 16

static u_int myramdisk_major = 0;

/* 
 * The internal structure representation of our Device
 */
static struct myramdisk_device
{
	/* Size is the size of the device (in sectors) */
	unsigned int size;
	/* For exclusive access to our request queue */
	spinlock_t lock;
	/* Our request queue */
	struct request_queue *myramdisk_queue;
	/* This is kernel's representation of an individual disk device */
	struct gendisk *myramdisk_disk;
} myramdisk_dev;

static int myramdisk_open(struct block_device *bdev, fmode_t mode)
{
	unsigned unit = iminor(bdev->bd_inode);

	//printk(KERN_INFO "myramdisk: Device is opened\n");
	printk(KERN_NOTICE "myramdisk is open with Inode number  %d\n", unit);

	return 0;
}


static void myramdisk_close(struct gendisk *disk, fmode_t mode)
{
	printk(KERN_NOTICE "myramdisk: Device is closed\n");
}


static int myramdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->heads = 1;
	geo->cylinders = 32;
	geo->sectors = 32;
	geo->start = 0;
	return 0;
}

/* 
 * Actual Data transfer
 */
static int myramdisk_transfer(struct request *req)
{
	//struct myramdisk_device *dev = (struct myramdisk_device *)(req->rq_disk->private_data);

	int dir = rq_data_dir(req);
	sector_t start_sector = blk_rq_pos(req);
	unsigned int sector_cnt = blk_rq_sectors(req);


#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)
	struct bio_vec bv;
	struct req_iterator iter;

	sector_t sector_offset;
	unsigned int sectors;
	u8 *buffer;

	int ret = 0;

	//printk(KERN_DEBUG "myramdisk: Dir:%d; Sec:%lld; Cnt:%d\n", dir, start_sector, sector_cnt);

	sector_offset = 0;
	rq_for_each_segment(bv, req, iter)
	{
		buffer = page_address(BV_PAGE(bv)) + BV_OFFSET(bv);
		sectors = BV_LEN(bv) / RB_SECTOR_SIZE;
		printk(KERN_DEBUG "myramdisk: Start Sector: %lld, Sector Offset: %lld; Buffer: %p; Length: %u sectors\n",
			start_sector, sector_offset, buffer, sectors);
		if (dir == WRITE) /* Write to the device */
		{
			ramdiskfs_write(start_sector + sector_offset, buffer, sectors);
		}
		else /* Read from the device */
		{
			ramdiskfs_read(start_sector + sector_offset, buffer, sectors);
		}
		sector_offset += sectors;
	}
	return ret;
}
	
/*
 * Represents a block I/O request for us to execute
 */
static void myramdisk_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	/* Gets the current request from the dispatch queue */
	while ((req = blk_fetch_request(q)) != NULL)
	{
		ret = myramdisk_transfer(req);
		__blk_end_request_all(req, ret);
		
	}
}

/* 
 * These are the file operations that performed on the ram block device
 */
static struct block_device_operations myramdisk_fops =
{
	.owner = THIS_MODULE,
	.open = myramdisk_open,
	.release = myramdisk_close,
	.getgeo = myramdisk_getgeo,
};
	
/* 
 * This is the registration and initialization section of the ram block device
 * driver
 */
static int __init myramdisk_init(void)
{
	int ret;

	/* Set up our RAM Device */
	if ((ret = ramdiskfs_init()) < 0)
	{
		return ret;
	}
	myramdisk_dev.size = ret;

	/* Get Registered */
	myramdisk_major = register_blkdev(myramdisk_major, "myramdisk");
	if (myramdisk_major <= 0)
	{
		printk(KERN_ERR "myramdisk: Unable to get Major Number\n");
		ramdiskfs_cleanup();
		return -EBUSY;
	}

	/* Get a request queue (here queue is created) */
	spin_lock_init(&myramdisk_dev.lock);
	myramdisk_dev.myramdisk_queue = blk_init_queue(myramdisk_request, &myramdisk_dev.lock);
	if (myramdisk_dev.myramdisk_queue == NULL)
	{
		printk(KERN_ERR "myramdisk: blk_init_queue failure\n");
		unregister_blkdev(myramdisk_major, "myramdisk");
		ramdiskfs_cleanup();
		return -ENOMEM;
	}
	
	/*
	 * Add the gendisk structure
	 * By using this memory allocation is involved, 
	 * the minor number we need to pass bcz the device 
	 * will support this much partitions 
	 */
	myramdisk_dev.myramdisk_disk = alloc_disk(RB_MINOR_CNT);
	if (!myramdisk_dev.myramdisk_disk)
	{
		printk(KERN_ERR "myramdisk: alloc_disk failure\n");
		blk_cleanup_queue(myramdisk_dev.myramdisk_queue);
		unregister_blkdev(myramdisk_major, "myramdisk");
		ramdiskfs_cleanup();
		return -ENOMEM;
	}

 	/* Setting the major number */
	myramdisk_dev.myramdisk_disk->major = myramdisk_major;
  	/* Setting the first mior number */
	myramdisk_dev.myramdisk_disk->first_minor = RB_FIRST_MINOR;
 	/* Initializing the device operations */
	myramdisk_dev.myramdisk_disk->fops = &myramdisk_fops;
 	/* Driver-specific own internal data */
	myramdisk_dev.myramdisk_disk->private_data = &myramdisk_dev;
	myramdisk_dev.myramdisk_disk->queue = myramdisk_dev.myramdisk_queue;
	/*
	 * You do not want partition information to show up in 
	 * cat /proc/partitions set this flags
	 */
	//myramdisk_dev.myramdisk_disk->flags = GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(myramdisk_dev.myramdisk_disk->disk_name, "mydisk");
	/* Setting the capacity of the device in its gendisk structure */
	set_capacity(myramdisk_dev.myramdisk_disk, myramdisk_dev.size);

	/* Adding the disk to the system */
	add_disk(myramdisk_dev.myramdisk_disk);
	/* Now the disk is "live" */
	printk(KERN_INFO "myramdisk: ramdiskfs driver initialised (%d sectors; %d bytes)\n",
		myramdisk_dev.size, myramdisk_dev.size * RB_SECTOR_SIZE);

	return 0;
}
/*
 * This is the unregistration and uninitialization section of the ram block
 * device driver
 */
static void __exit myramdisk_cleanup(void)
{
	del_gendisk(myramdisk_dev.myramdisk_disk);
	put_disk(myramdisk_dev.myramdisk_disk);
	blk_cleanup_queue(myramdisk_dev.myramdisk_queue);
	unregister_blkdev(myramdisk_major, "myramdisk");
	ramdiskfs_cleanup();
}

module_init(myramdisk_init);
module_exit(myramdisk_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nandan B R <h2016108@pilani.bits-pilani.ac.in>");
MODULE_DESCRIPTION("Virtual Disk in RAM");
