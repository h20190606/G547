#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include<linux/init.h>
#include<linux/device.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
 
#define BULK_EP_OUT 0x02
#define BULK_EP_IN 0x81
#define MAX_PKT_SIZE 512
#define READ_CAPACITY_LENGTH          0x08
#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])

#define SANDISK_VID  0x0781
#define SANDISK_PID  0x5572

struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

// Section 5.2: Command Status Wrapper (CSW)
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static uint8_t cdb_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};
 
static struct usb_device *device;
static struct usb_class_driver class;
 
static int myusb_open(struct inode *i, struct file *f)
{
    return 0;
}
static int myusb_close(struct inode *i, struct file *f)
{
    return 0;
}
static ssize_t myusb_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
   	int retval;
	uint8_t buffer[64];
	int size_ret;
	uint32_t max_lba, block_size;
	//double device_size;

////////////////////////////////////////////////////////////////////////////////////////////////

    /* Read the data from the bulk endpoint */
    retval = usb_bulk_msg(device, usb_rcvbulkpipe(device, BULK_EP_IN),
            &buffer, READ_CAPACITY_LENGTH , &size_ret, 5000);
	max_lba = be_to_int32(&buffer[0]);
	block_size = be_to_int32(&buffer[4]);
	//device_size = (double)((max_lba+1))*(block_size/(1024*1024*1024));
	 printk(KERN_INFO "Capacity of the Pendrive is : Max LBA: %08X, Block Size: %08X\n", max_lba, block_size);
	//printk(KERN_INFO"GB: %lf \n",device_size);

///////////////////////////////////////////////////////////////////////////////////////////

   
        printk(KERN_INFO"Read capacity from USB Device\n");
     
 
    return size_ret;
}

static ssize_t myusb_write(struct file *f, const char __user *buf, size_t cnt, loff_t *off)
{
    int retval;

//////////////////////////////////////////////////////////////////////////////////////////////
	struct command_block_wrapper cbw;
	static uint32_t tag = 1;
	int size;
	uint8_t cdb[16];	// SCSI Command Descriptor Block
	uint8_t cdb_len;
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x25;	// Read Capacity
	cdb_len = cdb_length[cdb[0]];
	memset(&cbw, 0, sizeof(cbw));
	cbw.dCBWSignature[0] = 'U';
	cbw.dCBWSignature[1] = 'S';
	cbw.dCBWSignature[2] = 'B';
	cbw.dCBWSignature[3] = 'C';
	cbw.dCBWTag = tag++;
	cbw.dCBWDataTransferLength = READ_CAPACITY_LENGTH;
	cbw.bmCBWFlags = 0;
	cbw.bCBWLUN = 0;
	// Subclass is 1 or 6 => cdb_len
	cbw.bCBWCBLength = cdb_len;
	memcpy(cbw.CBWCB, cdb, cdb_len);
/////////////////////////////////////////////////////////////////////////////////////////////
 
    /* Write the data into the bulk endpoint */
    retval = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT), &cbw , 31 , &size, 5000);
   
        printk(KERN_INFO"Sent SCSI READ Capacity command. Sent %d bytes\n", cdb_len);
        return retval;
   
 
    return 0;
}
 
static struct file_operations fops =
{
	.owner = THIS_MODULE,    
	.open = myusb_open,
        .release = myusb_close,
        .read = myusb_read,
        .write = myusb_write,
};
 
static int myusb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    	int retval,i;
	unsigned char ep_addr, ep_attr;
	struct usb_endpoint_descriptor *ep_desc;
 
    device = interface_to_usbdev(interface);
    class.name = "myusb";
    class.fops = &fops;
    if ((retval = usb_register_dev(interface, &class)) < 0)
    {
        /* Not able to register*/
        printk(KERN_ALERT"Not able to register.");
    }
    else
    {
        printk(KERN_INFO "Known USB drive detected\n");
    }

	//Descriptor details of USB
	printk(KERN_INFO "VID of the DEVICE : %x\n" , id->idVendor);
	printk(KERN_INFO"PID of the DEVICE : %x\n", id->idProduct);	
	printk(KERN_INFO "INTERFACE CLASS : %x\n", interface->cur_altsetting->desc.bInterfaceClass);
	printk(KERN_INFO "INTERFACE SUB CLASS : %x\n", interface->cur_altsetting->desc.bInterfaceSubClass);
	printk(KERN_INFO "INTERFACE Protocol : %x\n", interface->cur_altsetting->desc.bInterfaceProtocol);


	//ENdpoint Details

	printk(KERN_INFO "No. of Endpoints = %d\n", interface->cur_altsetting->desc.bNumEndpoints);

	for(i=0;i<interface->cur_altsetting->desc.bNumEndpoints;i++)
	{
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;
		ep_addr = ep_desc->bEndpointAddress;
		ep_attr = ep_desc->bmAttributes;
	
		if((ep_attr & USB_ENDPOINT_XFERTYPE_MASK)==USB_ENDPOINT_XFER_BULK)
		{
			if(ep_addr & 0x80)
				printk(KERN_INFO "EP %d is Bulk IN\n", i);
			else
				printk(KERN_INFO "EP %d is Bulk OUT\n", i);
	
		}
	}
 
    return retval;
}
 
static void myusb_disconnect(struct usb_interface *interface)
{
    usb_deregister_dev(interface, &class);
}
 
/* Table of devices that work with this driver */
static struct usb_device_id myusb_table[] =
{
    { USB_DEVICE(SANDISK_VID, SANDISK_PID) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, myusb_table);
 
static struct usb_driver myusb_driver =
{
    .name = "myusb_driver",
    .probe = myusb_probe,
    .disconnect = myusb_disconnect,
    .id_table = myusb_table,
};
 
static int __init myusb_init(void)
{
    int r;
 
    /* Register this driver with the USB subsystem */
    if ((r = usb_register(&myusb_driver)))
    {
        printk(KERN_ALERT"usb_register failed. Error number %d", r);
    }
	else
	{	
		printk(KERN_INFO" UAS READ Capacity Driver Inserted");
	}
    return r;
}
 
static void __exit myusb_exit(void)
{
    /* Deregister this driver with the USB subsystem */
    usb_deregister(&myusb_driver);
}
 
module_init(myusb_init);
module_exit(myusb_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jolly Mishra");
MODULE_DESCRIPTION("USB Device Driver for reading capacity");
