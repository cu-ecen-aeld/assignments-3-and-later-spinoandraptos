/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Juncheng Man"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    	PDEBUG("open");
	/**
	* Handle open
	*/
	filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);	
    	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    	PDEBUG("release");
	/**
	* Handle release
	*/
	filp->private_data = NULL;
    	return 0;
}

/* Reads count of data from kernel space into user space buf */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

	// The char device
	struct aesd_dev *dev = filp->private_data;
	if (dev == NULL) {
		PDEBUG("Device cannot be accessed");
		return -EINVAL;
	}
    	
    	// Invalid arguments
	if (filp == NULL || buf == NULL || f_pos == NULL){
		PDEBUG("Invalid arguments");
		return -EINVAL;
	}
	
	// Try to lock the device mutex
	int res = mutex_lock_interruptible(&dev->mutex);
	if (res != 0){
		PDEBUG("Unable to lock device mutex");
		return -ERESTARTSYS;
	}
	
	// Looks for the entry in buffer based on given fpos
    	size_t entry_offset = 0;
	struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
	if (entry == NULL)
	{
		PDEBUG("Unable to find entry");
		mutex_unlock(&dev->mutex);
		return retval;
	}

	// Find the available bytes remaining to be read
    	size_t bytes_remaining = entry->size - entry_offset;
    	
	// Reads only up to count maximum bytes
	if (bytes_remaining >= count){
		bytes_remaining = count;
	}
	
	// Copy remaining bytes to user space buf from the location indicated by fpos
    	unsigned long bytes_not_copied = copy_to_user(buf, entry->buffptr + entry_offset, bytes_remaining);
    	
    	// All bytes to be copied should be copied
	if (bytes_not_copied != 0)
	{
		PDEBUG("Unable to do user-kernel copy");
		mutex_unlock(&dev->mutex);
		return -EFAULT;
	}
    	
    	// Increment fpos by number of bytes read
	*f_pos += bytes_remaining;
	// Release the mutex
	mutex_unlock(&dev->mutex);
	// Set return value to number of bytes copied
	retval = bytes_remaining;

	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	
    	// Invalid arguments
	if (filp == NULL || buf == NULL || f_pos == NULL){
		PDEBUG("Invalid arguments");
		return -EINVAL;
	}
	
	// Allocate memory for the write
	char *data = kmalloc(count, GFP_KERNEL);
	
	if (data == NULL) {
		PDEBUG("Unable to allocate memory");
		return -ENOMEM;
	}

	// Copy data to be written from user space
	unsigned long bytes_not_copied = copy_from_user(data, buf, count);
	
	if (bytes_not_copied != 0) {
		PDEBUG("Unable to copy data from user space");
		kfree(data);
		return -EFAULT;
	}

	// Check if newline present in data up to count 
	int newline_pos = 0;
	bool write_complete = false;
	char *newline_ptr = memchr(data, '\n', count);
	// If no newline, write not complete, so newline_pos set to maximal count
	if (newline_ptr == NULL) {
		newline_pos = count;
	// Newline present, write complete and set newline_pos to offset of newline char from start of data 
	} else {
		newline_pos = newline_ptr - data + 1;
		write_complete = true;
	}
	
	// Get device
	struct aesd_dev *dev = filp->private_data;
	if (dev == NULL) {
		PDEBUG("Device cannot be accessed");
		return -EINVAL;
	}

	// Try to lock the device mutex
	int res = mutex_lock_interruptible(&dev->mutex);
	if (res != 0){
		PDEBUG("Unable to lock device mutex");
		kfree(data);
		return -ERESTARTSYS;
	}
	
	// New data size adds up to newline 
	size_t old_size = dev->entry.size;
	dev->entry.size += newline_pos;

	// Reallocate memory to append new write data
	char *new_entry_loc = krealloc(dev->entry.buffptr, dev->entry.size, GFP_KERNEL);
	
	if (new_entry_loc == NULL) {
		PDEBUG("Unable to reallocate memory\n");
		kfree(data);
		mutex_unlock(&dev->mutex);
		return -ENOMEM;
	}
	
	// Update entry to new memory location
	dev->entry.buffptr = new_entry_loc; 

	// Copy data into newly allocated space
	memcpy(dev->entry.buffptr + old_size, data, newline_pos);

	// Write complete
	if (write_complete) {
		// Add entry to circular buffer
		aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
		
		// Reset device entry
		dev->entry.size = 0;
		dev->entry.buffptr = NULL;
	}

	// Unlock mutex
	mutex_unlock(&dev->mutex);
	// Release data
	kfree(data);
	
	return newline_pos;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
    	int result;
    	result = alloc_chrdev_region(&dev, aesd_minor, 1,
           	 "aesdchar");
    	aesd_major = MAJOR(dev);
    	if (result < 0) {
        	printk(KERN_WARNING "Can't get major %d\n", aesd_major);
       	 return result;
    	}
    	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	* Initialize the AESD specific portion of the device
	*/
	aesd_circular_buffer_init(&aesd_device.buffer);
	mutex_init(&aesd_device.mutex);

    	result = aesd_setup_cdev(&aesd_device);

    	if( result ) {
        	unregister_chrdev_region(dev, 1);
    	}
    	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	* Cleanup AESD specific poritions here as necessary
	*/
     	struct aesd_buffer_entry *entry;
	uint8_t index = 0;
     	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index){
		if(entry->buffptr != NULL){
			kfree(entry->buffptr);
		}
	}

	mutex_destroy(&aesd_device.mutex);

    	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
