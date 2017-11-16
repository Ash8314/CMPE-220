#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/fs.h> // required for various structures related to files liked fops.
#include <asm/uaccess.h> // required for copy_from and copy_to user functions

#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#define BULK_EP_OUT 0x02
#define BULK_EP_IN 0x81
#define MAX_PKT_SIZE 512

/*------------------------------- File operation  -------------------------------*/
struct usb_device *connected_device;
static struct usb_class_driver class;
static unsigned char bulk_buf[MAX_PKT_SIZE];

static int pen_open(struct inode *i, struct file *f)
{
	return 0;
}
static int pen_close(struct inode *i, struct file *f)
{
	return 0;
}
static ssize_t pen_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
	int retval;
	int read_cnt;

	/* Read the data from the bulk endpoint */
	retval = usb_bulk_msg(connected_device, usb_rcvbulkpipe(connected_device, BULK_EP_IN),
			bulk_buf, MAX_PKT_SIZE, &read_cnt, 5000);
	if (retval)
	{
		printk(KERN_ERR "Bulk message returned %d\n", retval);
		return retval;
	}
	if (raw_copy_to_user(buf, bulk_buf, MIN(cnt, read_cnt)))
	{
		return -EFAULT;
	}

	return MIN(cnt, read_cnt);
}
static ssize_t pen_write(struct file *f, const char __user *buf, size_t cnt,loff_t *off)
{
	int retval;
	int wrote_cnt = MIN(cnt, MAX_PKT_SIZE);

	if (raw_copy_from_user(bulk_buf, buf, MIN(cnt, MAX_PKT_SIZE)))
	{
		return -EFAULT;
	}

	/* Write the data into the bulk endpoint */
	retval = usb_bulk_msg(connected_device, usb_sndbulkpipe(connected_device, BULK_EP_OUT),
			bulk_buf, MIN(cnt, MAX_PKT_SIZE), &wrote_cnt, 5000);
	if (retval)
	{
		printk(KERN_ERR "Bulk message returned %d\n", retval);
		return retval;
	}

	return wrote_cnt;
}

static struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = pen_open,
	.release = pen_close,
	.read = pen_read,
	.write = pen_write,
};

/*------------------------------- #include<usb.h> -------------------------------*/
/* This function is called automatically when we insert an USB device provided this driver
*  is installed successfully. if this function is not being called one of the reasons can be that 
*  some other USB driver has access to USB bus and probe function of its driver is being called 
*  everytime when we insert USB. You can check which driver is working and which is being called 
*  by a debugging methode provided by Linux. Terminal command for this is dmesg. It lists out all
*  the debug messages printed out by different drivers. Try " dmesg | grep -i <your tag here> " 
*  command for filtering debug messages which has tag same as you mentioned in command. Thus, make  
*  sure you keep tag in your message. Anything before ":" in printk() is counted as tag.
*/
static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int retval;
	connected_device = interface_to_usbdev(interface);
	
	class.name = "usb/pen%d";
	class.fops = &my_fops;

	printk(KERN_ERR "CMPE-220: new USB drive plugged\n");
	printk(KERN_ERR "CMPE-220: Vendor ID: %04X, Product ID: %04X\n", id->idVendor, id->idProduct);
	printk(KERN_ERR "CMPE-220: Type: %s \n",connected_device->product);
	printk(KERN_ERR "CMPE-220: Manufacturer: %s\n",connected_device->manufacturer);
	printk(KERN_ERR "CMPE-220: serial: %s",connected_device->serial);
	dev_alert(&interface->dev,"CMPE-220: device now attached\n");

	if ((retval = usb_register_dev(interface, &class)) < 0)
	{
		/* Something prevented us from registering this driver */
		printk(KERN_ERR "Not able to get a minor for this device.");
	}
	else
	{
		printk(KERN_INFO "Minor obtained: %d\n", interface->minor);
	}

	return retval;
}


/* This function is being called automatically when we remove an USB device. If probe function 
*  of this driver is working then this function will work for sure. 
*/

static void pen_disconnect(struct usb_interface *interface)
{
	printk(KERN_ERR "CMPE-220: USB Drive removed.");
	usb_deregister_dev(interface, &class);
}



/* This is the list of USB devices supported by this driver.
*  it is in pair of Vendor ID and Product ID of USB device.
*  The last {} blank entry is called termination entry. 
*/
static struct usb_device_id pen_table[] = 
{
	// (VendorID, ProductID)
	{USB_DEVICE(0x054c,0x09c2)},	
	{USB_DEVICE(0x8564,0x1000)},
	{USB_DEVICE(0x0bc2,0xab26)},
	{}
};


/* We register this array of supported USB devices by following function. Thus, whenever we
*  insert USB device, its vendor ID and Product ID is searched in this list and served if it 
*  is found there.
*/
MODULE_DEVICE_TABLE(usb,pen_table);


/* This is the main structure of device driver, where we register all our functions
*  to the default calling functions. 
*/
static struct usb_driver pen_driver = 
{
	.name = "USB stick driver",
	.id_table = pen_table,		
	.probe = pen_probe,
	.disconnect = pen_disconnect,
};


/* This is a function which plays role in installing the USB driver and registering 
*  it in kernel. To register it in kernel it uses the function given by Linux distro  
*  called usb_resigter(). This returns a value which indicates the result of registration.
*  It returns 0 on succsess and negative values on failure. 
*/
static int __init pen_init(void)
{
	int result = -2;
	printk(KERN_ERR "CMPE-220: Installing my USB Driver.");

        /* register this driver with the USB subsystem */
        result = usb_register(&pen_driver);
        if (result < 0) {
                printk(KERN_ERR "CMPE-220: usb_register failed. error- %d", result);
                return -1;
        }
	
	printk(KERN_ERR "CMPE-220: usb registration successful.");
        return result;
}


/* This is a function which plays role in uninstalling the USB driver and deregistering 
*  it in kernel. To deregister it in kernel it uses the function given by Linux distro  
*  called usb_deresigter().
*/
static void __exit pen_exit(void)
{
	printk(KERN_ERR "CMPE-220: Uninstalling my USB driver");
	usb_deregister(&pen_driver);
}

/*------------------------------- #include<module.h> -------------------------------*/
/* Here we are registering our driver-installation function to the function provided by
*  Linux distro called module_init(). Important to mention that in Linux drivers are called
*  modules. To see the list of modules running in background can be checked by command lsmod.
*  It shows list of all the modules working in background. Try lsmod | grep -i usb to filer 
*  modules with usb word in it.
*/
module_init(pen_init);

/* Here we are registering our driver-installation function to the function provided by
*  Linux distro called module_exit(). It indicates that whenever user wants to uninstall
*  this driver, call pen_exit function.
*/
module_exit(pen_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("we 4");
MODULE_DESCRIPTION("writting our own USB device driver.");
