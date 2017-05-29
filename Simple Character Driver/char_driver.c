#include <linux/init.h>           
#include <linux/module.h>         
#include <linux/device.h>        
#include <linux/kernel.h>       
#include <linux/fs.h>          
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include "mycdev.h"		// for ioctl


#define ERR		-1
#define DEVICE_NAME	"mycdev"
#define NUM_DEV		3
#define RAMDISK_SIZE 	(size_t)(16*PAGE_SIZE)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Abraham Tom");
MODULE_DESCRIPTION("Simple Character Driver");
MODULE_VERSION("1.0");           


/*
 * custom structure representing a mycdev device
 */
struct mycdev 
{
	struct cdev dev;
	char *ramdisk;
	size_t ramdisk_size;
	struct semaphore sem;
	int devNo;
};


/*
 * globals
 */
static int major_number = 0;
static struct class *mycdev_class = NULL;
struct mycdev *devices = NULL;
static int NUM_DEVICES = NUM_DEV;
module_param(NUM_DEVICES, int, S_IRUGO);


/*
 * function for device open
 */
static int dev_open(struct inode *inode, struct file *filp)
{
	int retVal = ERR;	
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	struct mycdev *dev = NULL;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_open\n", DEVICE_NAME, mn);


	// check if the major & minor numbers provided match
	// with the information in this module
	if (mj != major_number || mn < 0 || mn >= NUM_DEVICES)
	{
		printk(KERN_ERR "[/dev/%s%d] OPEN error: Device not found\n", DEVICE_NAME, mn);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_open\n", DEVICE_NAME, mn);
		return -ENODEV; 
	}


	// check if inode information provided match with 
	// with available information 
	if (inode->i_cdev != &devices[mn].dev)
	{
		printk(KERN_ERR "[/dev/%s%d] OPEN error: Device information mismatch\n", DEVICE_NAME, mn);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_open\n", DEVICE_NAME, mn);
		return -ENODEV;
	}


	// basic checks have passed	
	dev = container_of(inode->i_cdev, struct mycdev, dev);


	// semaphore down
	if(down_interruptible(&dev->sem))
	{
		printk(KERN_ERR "[/dev/%s%d] OPEN error: failed to lock device... "\
				"retrying!\n", DEVICE_NAME, mn);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_open\n", DEVICE_NAME, mn);
		return -ERESTARTSYS;
	}


	// to make sure semaphore up is performed before returning from the function
	do
	{
		// link custom device structure to private data of file
		filp->private_data = dev;


		// if opened for the 1st time, allocate buffer
		if (dev->ramdisk == NULL)
		{
			dev->ramdisk = kzalloc(dev->ramdisk_size, GFP_KERNEL);
			if (dev->ramdisk == NULL)
			{
				printk(KERN_ERR "[/dev/%s%d] OPEN error: failed to allocate memory\n", DEVICE_NAME, mn);
				retVal = -ENOMEM;
				break;
			}
		}

		printk(KERN_INFO "[/dev/%s%d] OPEN success\n", DEVICE_NAME, mn);
		retVal = 0;
	}while(0);



	// semaphore up is performed only if down has already been done 
	up(&dev->sem);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_open\n", DEVICE_NAME, mn);
	return retVal;
}


/*
 * function for device close
 */
static int dev_release(struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_release\n", DEVICE_NAME, iminor(inode));
	filp->private_data = NULL;
	printk(KERN_INFO "[/dev/%s%d] CLOSE success\n", DEVICE_NAME, iminor(inode));
	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_release\n", DEVICE_NAME, iminor(inode));
	return 0;
}


/*
 * function for device read
 */
static ssize_t dev_read(struct file *filp, char __user * buf, size_t lbuf, loff_t *ppos)
{
	ssize_t retVal = ERR;
	struct mycdev *dev = (struct mycdev *)filp->private_data;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_read\n", DEVICE_NAME, dev->devNo); 


	// semaphore down
	if(down_interruptible(&dev->sem))
	{
		printk(KERN_ERR "[/dev/%s%d] READ error: failed to lock device "\
				"... retrying!\n", DEVICE_NAME, dev->devNo);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_read\n", DEVICE_NAME, dev->devNo); 
		return -ERESTARTSYS;
	}


	// to make sure semaphore up is performed before returning from the function
	do
	{
		// if current position is at end of allocated buffer or beyond
		if (*ppos >= dev->ramdisk_size)
		{	
			printk(KERN_INFO "[/dev/%s%d] READ success: end of file reached\n", DEVICE_NAME, dev->devNo);
			retVal = 0;
			break;
		}


		// if read goes beyond end of allocated memory
		if (*ppos + lbuf > dev->ramdisk_size)
		{
			printk(KERN_INFO "[/dev/%s%d] READ expected to exceed allocated memory. "\
					"Reading only till end of allocated memory\n", DEVICE_NAME, dev->devNo);
			lbuf = dev->ramdisk_size - *ppos;
		}


		// copy to user
		if (copy_to_user(buf, &(dev->ramdisk[*ppos]), lbuf) != 0)
		{
			printk(KERN_ERR "[/dev/%s%d] READ error: copy_to_user failed\n", DEVICE_NAME, dev->devNo);
			retVal = -EFAULT;
			break;
		}


		// update position in file and set return value to length of buffer read
		printk(KERN_INFO "[/dev/%s%d] READ success\n", DEVICE_NAME, dev->devNo);
		*ppos += lbuf;
		retVal = lbuf;
	}while(0);


	// semaphore up is performed only if down has already been done 
	up(&dev->sem);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_read\n", DEVICE_NAME, dev->devNo); 
	return retVal;
}


/*
 * function for device write
 */
static ssize_t dev_write(struct file *filp, const char __user *buf, size_t lbuf, loff_t *ppos)
{
	ssize_t retVal = ERR;
	char *new_ramdisk = NULL;
	struct mycdev *dev = (struct mycdev *)filp->private_data;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_write\n", DEVICE_NAME, dev->devNo); 


	// semaphore down
	if(down_interruptible(&dev->sem))
	{
		printk(KERN_ERR "[/dev/%s%d] WRITE error: failed to lock device "\
				"... retrying!\n", DEVICE_NAME, dev->devNo);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_write\n", DEVICE_NAME, dev->devNo); 
		return -ERESTARTSYS;
	}


	// to make sure semaphore up is performed before returning from the function
	do
	{
		// check if current file position is beyond the allocated size
		if (*ppos > dev->ramdisk_size) 
		{
			printk(KERN_ERR "[/dev/%s%d] WRITE error: invalid file position\n", DEVICE_NAME, dev->devNo);
			retVal = -EINVAL;
			break;
		}


		// check if trying to write beyond allocated size  
		if (*ppos + lbuf > dev->ramdisk_size)
		{
			new_ramdisk = kzalloc(*ppos+lbuf, GFP_KERNEL);
			if (new_ramdisk == NULL)
			{
				printk(KERN_ERR "[/dev/%s%d] WRITE error: failed to allocate memory\n", DEVICE_NAME, dev->devNo);
				retVal = -ENOMEM;
				break;
			}

			memcpy(new_ramdisk, dev->ramdisk, dev->ramdisk_size);
			kfree(dev->ramdisk);

			dev->ramdisk = new_ramdisk;
			dev->ramdisk_size = *ppos+lbuf;

			printk(KERN_INFO "[/dev/%s%d] WRITE expected to exceeded allocated memory. "\
					"Reallocated size = %zu\n",\
					DEVICE_NAME, dev->devNo, dev->ramdisk_size);
		}


		// copy from user
		if (copy_from_user(&(dev->ramdisk[*ppos]), buf, lbuf) != 0)
		{
			printk(KERN_ERR "[/dev/%s%d] WRITE error: copy_from_user failed\n", DEVICE_NAME, dev->devNo);
			retVal = -EFAULT;
			break;
		}


		// update position in file and set return value to number of bytes written
		printk(KERN_INFO "[/dev/%s%d] WRITE success\n", DEVICE_NAME, dev->devNo);
		*ppos += lbuf;
		retVal = lbuf;
	}while(0);


	// semaphore up is performed only if down has already been done 
	up(&dev->sem);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_write\n", DEVICE_NAME, dev->devNo); 
	return retVal;
}


/*
 * function for device seek
 */
static loff_t dev_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t retVal = ERR, newpos = 0;
	char *new_ramdisk = NULL;
	struct mycdev *dev = (struct mycdev *)filp->private_data;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_llseek\n", DEVICE_NAME, dev->devNo); 


	// semaphore down
	if(down_interruptible(&dev->sem))
	{
		printk(KERN_ERR "[/dev/%s%d] SEEK error: failed to lock device "\
				"... retrying!\n", DEVICE_NAME, dev->devNo);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_llseek\n", DEVICE_NAME, dev->devNo); 
		return -ERESTARTSYS;
	}


	// to make sure semaphore up is performed before returning from the function
	do
	{
		switch(whence) {
			case 0: // SEEK_SET
				newpos = off;
				break;

			case 1: // SEEK_CUR
				newpos = filp->f_pos + off;
				break;

			case 2: // SEEK_END
				newpos = dev->ramdisk_size + off;
				break;

			default: // invalid
				newpos = ERR;
				break;
		}


		// check for error
		if (newpos < 0)
		{
			printk(KERN_ERR "[/dev/%s%d] SEEK error: invalid seek position\n",\
					DEVICE_NAME, dev->devNo);
			retVal = -EINVAL;
			break;
		}


		// check if trying to seek beyond allocated size 
		if(newpos > dev->ramdisk_size)
		{	
			// allocate more memory
			new_ramdisk = kzalloc(newpos, GFP_KERNEL);
			if (new_ramdisk == NULL)
			{
				printk(KERN_ERR "[/dev/%s%d] SEEK error: failed to allocate memory\n", DEVICE_NAME, dev->devNo);
				retVal = -ENOMEM;
				break;
			}


			// copy existing buffer into the new location,
			// free the old buffer and also update the new ramdisk_size
			memcpy(new_ramdisk, dev->ramdisk, dev->ramdisk_size);
			kfree(dev->ramdisk);
			dev->ramdisk = new_ramdisk;
			dev->ramdisk_size = newpos;


			printk(KERN_INFO "[/dev/%s%d] SEEK expected to exceeded allocated memory. "\
					"Reallocated size = %zu\n",\
					DEVICE_NAME, dev->devNo, dev->ramdisk_size);
		}


		// update position in file and set return value to the new position in file
		printk(KERN_INFO "[/dev/%s%d] SEEK success\n", DEVICE_NAME, dev->devNo);
		filp->f_pos = newpos;
		retVal = newpos;
	}while(0);


	// semaphore up is performed only if down has already been done 
	up(&dev->sem);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_llseek\n", DEVICE_NAME, dev->devNo); 
	return retVal;
}


/*
 * function for ioctl
 */
static long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retVal = ERR;
	struct mycdev *dev = (struct mycdev *)filp->private_data;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_ioctl\n", DEVICE_NAME, dev->devNo); 


	// semaphore down
	if(down_interruptible(&dev->sem))
	{
		printk(KERN_ERR "[/dev/%s%d] IOCTL error: failed to lock device "\
				"... retrying!\n", DEVICE_NAME, dev->devNo);
		printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_ioctl\n", DEVICE_NAME, dev->devNo); 
		return -ERESTARTSYS;
	}


	switch(cmd)
	{
		case ASP_CLEAR_BUF:
			memset(dev->ramdisk, '\0', dev->ramdisk_size);		


			// update file position pointer to 0
			filp->f_pos = 0;	


			// ASP_CLEAR_BUF successful
			printk(KERN_INFO "[/dev/%s%d] IOCTL success: ASP_CLEAR_BUF\n", DEVICE_NAME, dev->devNo);
			retVal = 0;
			break;


		default: retVal = -EINVAL; 
			 break;
	}


	// semaphore up is performed only if down has already been done 
	up(&dev->sem);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_ioctl\n", DEVICE_NAME, dev->devNo);
	return retVal;
}


/*
 * initialize file operations
 */
static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
	.llseek = dev_llseek,
	.unlocked_ioctl = dev_ioctl,
};


/*
 * function for device init
 */
static int dev_init(struct mycdev *contents, int minor_number)
{
	int retVal = ERR, count = 1;
	dev_t dev = MKDEV(major_number, minor_number);
	struct device *dev_entry = NULL;


	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_init\n", DEVICE_NAME, minor_number); 


	// initialize custom structure members for individual dev
	contents->ramdisk = NULL;	// allocate on open
	contents->ramdisk_size = RAMDISK_SIZE;
	sema_init(&contents->sem, count);
	contents->devNo = minor_number;
	cdev_init(&contents->dev, &fops);
	contents->dev.owner = THIS_MODULE;


	// to have single exit point
	do
	{
		// add dev to kernel
		retVal = cdev_add(&contents->dev, dev, 1);
		if (retVal != 0)
		{
			printk(KERN_ERR "[/dev/%s%d] INIT Error: adding device to "\
					"kernel failed\n", DEVICE_NAME, minor_number);
			break;
		}


		dev_entry = device_create(mycdev_class, NULL, dev, NULL, DEVICE_NAME "%d", minor_number);
		if (IS_ERR(dev_entry)) 
		{
			printk(KERN_ERR "[/dev/%s%d] INIT Error: /dev entry creation failed\n", DEVICE_NAME, minor_number);
			retVal = PTR_ERR(dev_entry);
			cdev_del(&contents->dev);
			break;
		}


		// update return value for success
		printk(KERN_INFO "[/dev/%s%d] INIT success\n", DEVICE_NAME, minor_number);
		retVal = 0;
	} while(0);


	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_init\n", DEVICE_NAME, minor_number); 
	return retVal;
}


/*
 * function for device exit
 */
static void dev_exit(struct mycdev *contents, int minor_number)
{
	printk(KERN_DEBUG "[/dev/%s%d] Entering dev_exit\n", DEVICE_NAME, minor_number); 
	device_destroy(mycdev_class, MKDEV(major_number, minor_number));
	cdev_del(&contents->dev);
	kfree(contents->ramdisk);
	printk(KERN_DEBUG "[/dev/%s%d] Exiting dev_exit\n", DEVICE_NAME, minor_number); 
	return;
}


/*
 * cleanup module
 */
static void cleanup(int num_dev_destroy)
{
	int i = 0;


	printk(KERN_DEBUG "Entering cleanup"); 


	// destroy devices if any
	if (devices != NULL) 
	{
		for (i = 0; i < num_dev_destroy; i++)
		{
			dev_exit(&devices[i], i);
		}
		kfree(devices);
	}


	// destroy the class
	if (mycdev_class != NULL)
	{
		class_destroy(mycdev_class);
	}


	// unregister only if registered
	unregister_chrdev_region(MKDEV(major_number, 0), NUM_DEVICES);


	printk(KERN_DEBUG "Exiting cleanup"); 
	return;
}


/*
 * function for module initialization
 */
static int __init initialize_module(void)
{
	int i = 0, num_dev_destroy = 0, retVal = ERR;
	dev_t dev = 0;


	// dynamically allocate major number for device
	retVal = alloc_chrdev_region(&dev, 0, NUM_DEVICES, DEVICE_NAME);
	if (retVal < 0) 
	{
		printk(KERN_ERR "Error: failed to allocate major number for device:%s\n", DEVICE_NAME);
		return retVal;
	}
	major_number = MAJOR(dev);


	// cleanup has to be done if any of the following fails
	do
	{
		// create class
		mycdev_class = class_create(THIS_MODULE, DEVICE_NAME);
		if (IS_ERR(mycdev_class))
		{
			printk(KERN_ERR "Error: failed to create class for device:%s\n", DEVICE_NAME);
			retVal = PTR_ERR(mycdev_class);
			break;
		}


		// allocate memory for custom structure of each individual dev
		devices = (struct mycdev *)kzalloc(NUM_DEVICES * sizeof(struct mycdev), GFP_KERNEL);
		if (devices == NULL)
		{
			printk(KERN_ERR "Error: failed to allocate memory for device:%s\n", DEVICE_NAME);
			retVal = -ENOMEM;
			break; 
		}


		// dev initialization
		for (i = 0; i < NUM_DEVICES; i++)
		{
			retVal = dev_init(&devices[i], i);
			if (retVal != 0)
			{
				printk(KERN_ERR "[/dev/%s%d] Error: failed to initialize dev\n", DEVICE_NAME, i);
				num_dev_destroy = i;
				break;
			} 
		}

		printk(KERN_INFO "Module initialized successfully!\n");
		return 0; // success
	}while(0);

	// init failed
	cleanup(num_dev_destroy);
	return retVal;
}


/*
 * function for module exit
 */
static void __exit remove_module(void)
{
	cleanup(NUM_DEVICES);
	printk(KERN_INFO "Module removed successfully!\n");
	return;

}


module_init(initialize_module);
module_exit(remove_module);
