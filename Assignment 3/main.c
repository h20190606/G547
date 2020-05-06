#include <linux/fs.h>                
#include <linux/genhd.h>            
#include <linux/module.h>            
#include <linux/kernel.h>           
#include <linux/blkdev.h>           
#include <linux/bio.h>              
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/hdreg.h>            
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/timer.h>	
#include <linux/slab.h>
#include <linux/workqueue.h> 

//sony 4gb 054c : 02a5
#define KERNEL_SECTOR_SIZE 512      
//#define NSECTORS 1024                
#define MYDISK_MINORS 3                
#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)
//#define USB_LBA 31266815

#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)

#define SAN_VID  0x0781
#define SAN_PID  0x5572

#define SONY_VID  0x054C
#define SONY_PID  0x02A5

#define MOSR_VID  0x1EC9
#define MOSR_PID  0xB081

#define USB_EP_IN                     0x81
#define USB_EP_OUT                  0x02

#define READ_10 0x28
#define WRITE_10 0x2A
#define READ_10_CMD_LEN	 10
#define READ_CAPACITY_LENGTH          0x08
#define RETRY_MAX                     5
#define REQUEST_SENSE_LENGTH          0x12

#define BOMS_GET_MAX_LUN              0xFE
#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])

#define BOMS_RESET		     0xff
#define BOMS_RESET_REQ_TYPE          0x21

#define DEVICE_NAME "USB_BLOCK_DEVICE"  
#define MAJOR_NR 125 
#define USB_SECTOR_SIZE 512 

int send_mass_storage_command(struct usb_device*, uint8_t, uint8_t,uint8_t*, uint8_t, int, uint32_t*);
int get_mass_storage_status(struct usb_device*, uint8_t, uint32_t);
int read_capacity(void);
int read_usb(unsigned int , int, unsigned char*);
int init_usb_bulk(unsigned int);
void cleanup_usb_bulk(void);
void usbdev_request(struct request_queue*);
void do_usbdev_data_transfer(struct work_struct*);
int __do_usbdev_data_transfer(struct request*);
int write_usb( int, unsigned int, unsigned char*);



struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};


struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

struct usbdev_private 
{ 
	struct usb_device *device; 
	unsigned char ep_in;
	unsigned char ep_out;

};

struct blkdev_private 
{  
	struct workqueue_struct *usbdevQ;  
	spinlock_t lock;  
	struct request_queue *queue;  
	struct gendisk *gd;  
};  

struct usbdev_work 
{  
	struct work_struct work;  
	struct request *req;  
};

 

struct usbdev_private *p_usbdev;
struct blkdev_private *p_blkdev = NULL; 
long block_size_usb;
long max_lba_usb;
long device_size_usb;

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

/*static u_int mydisk_major = 0;
u8 *dev_data;
struct request *req;*/

static void usbdev_disconnect(struct usb_interface *interface)
{
	printk(KERN_INFO "USBDEV Device Removed\n");
	cleanup_usb_bulk();
	return;
}

static struct usb_device_id usbdev_table [] = {
	{USB_DEVICE(SAN_VID, SAN_PID)},
	{USB_DEVICE(SONY_VID, SONY_PID)},
	{USB_DEVICE(MOSR_VID, MOSR_PID)},
	{}	
};



static void display_buffer_hex(unsigned char *buffer, unsigned size)
{
	unsigned i, j, k;

	for (i=0; i<size; i+=16) {
		printk(KERN_INFO"\n  %08x  ", i);
		for(j=0,k=0; k<16; j++,k++) {
			if (i+j < size) {
				printk(KERN_INFO"%02x", buffer[i+j]);
			} else {
				printk(KERN_INFO"  ");
			}
			printk(KERN_INFO" ");
		}
		printk(KERN_INFO" ");
		for(j=0,k=0; k<16; j++,k++) {
			if (i+j < size) {
				if ((buffer[i+j] < 32) || (buffer[i+j] > 126)) {
					printk(KERN_INFO".");
				} else {
					printk(KERN_INFO"%c", buffer[i+j]);
				}
			}
		}
	}
	printk(KERN_INFO"\n" );
}

int send_mass_storage_command(struct usb_device *device, uint8_t endpoint_out, uint8_t lun,uint8_t *cdb, uint8_t direction, int data_length, uint32_t *ret_tag)  

{
	static uint32_t tag = 1;
	uint8_t cdb_len;
	int i, r, size;
	struct command_block_wrapper *cbw;
	cbw = (struct command_block_wrapper*)kmalloc(sizeof(struct command_block_wrapper), GFP_KERNEL);
	
	if ( cbw == NULL ) {
		printk(KERN_INFO "Cannot allocate memory\n");
		return -1;}

	if (cdb == NULL) {
		return -1;
	}
	if (endpoint_out & USB_EP_IN) 
	{
		printk(KERN_INFO "send_mass_storage_command: cannot send command on IN endpoint\n") ;
		return -1;
	}

	cdb_len = cdb_length[cdb[0]];

	if ((cdb_len == 0) || (cdb_len > sizeof(cbw -> CBWCB))) 
	{
		printk(KERN_INFO "send_mass_storage_command: don't know how to handle this command (%02X, length %d)\n", cdb[0], cdb_len);
		return -1;
	}
	cdb_len = cdb_length[cdb[0]];
	
	memset(cbw,'\0',sizeof(struct command_block_wrapper));
	cbw->dCBWSignature[0] = 'U';
	cbw->dCBWSignature[1] = 'S';
	cbw->dCBWSignature[2] = 'B';
	cbw->dCBWSignature[3] = 'C';
	*ret_tag = tag;
	cbw->dCBWTag = tag++;
	cbw->dCBWDataTransferLength = data_length;
	cbw->bmCBWFlags = direction;
	cbw->bCBWLUN = lun;
	// Subclass is 1 or 6 => cdb_len
	cbw->bCBWCBLength = cdb_len;
	memcpy(cbw->CBWCB, cdb, cdb_len);

	r= usb_bulk_msg(device,usb_sndbulkpipe(device,endpoint_out), (unsigned char*)&cbw, 31, &size, 1000);

	i = 0;
	do {
		// The transfer length must always be exactly 31 bytes.
		r= usb_bulk_msg(device,usb_sndbulkpipe(device,endpoint_out), (unsigned char*)cbw, 31, &size, 1000);
		printk(KERN_INFO"After usb_bulk_msg in send SCSI command, r = %d\n",r);
		if (r != 0) 
		{
			usb_clear_halt(device, usb_sndbulkpipe(device,endpoint_out));
				
		}
		i++;
	} while (0);


	printk(KERN_INFO "r=%d \n",r);
	
	if (r !=0) {
		printk(KERN_INFO"   error in send endpoint command\n");
		return -1;
	}
	
	printk(KERN_INFO"   sent %d CDB bytes\n", cdb_len);

	kfree(cbw);
	return 0;
}


int get_mass_storage_status(struct usb_device *device, uint8_t endpoint, uint32_t expected_tag) 
{
	int i, r, size;
	struct command_status_wrapper *csw;

	if( !(csw = (struct command_status_wrapper*)kmalloc(sizeof(struct command_status_wrapper), GFP_KERNEL)) ) 
	{
		printk(KERN_INFO "Cannot allocate memory for command status buffer\n");
		return -1;
	}
	
	i = 0;
	do{
		r = usb_bulk_msg(device, usb_rcvbulkpipe(device,endpoint), (unsigned char*)csw, 13, &size, 1000);
		if (r!=0){
			 usb_clear_halt(device,usb_sndbulkpipe(device,endpoint));
			  }
		i++;
	   } while((r!=0) && (i<5));

	if (r != 0) 
	{
		printk(KERN_INFO "get_mass_storage_status: %d\n",r);
		return -1;
	}

	printk(KERN_INFO "Received CSW having status %d\n", csw->bCSWStatus);

	if (size != 13) 
	{
		printk(KERN_INFO "get_mass_storage_status: received %d bytes (expected 13)\n", size);
		return -1;
	}

	if (csw->dCSWTag != expected_tag) 
	{
		printk(KERN_INFO "get_mass_storage_status: mismatched tags (expected %08X, received %08X)\n",expected_tag, csw->dCSWTag);
		return -1;
	}

	//printk(KERN_INFO "Received CSW having status %d\n", csw->bCSWStatus);

	kfree(csw);

	return 0;	
	
}



int read_capacity(void)
{
	int r, size,read_ep;
	uint8_t lun=0;
	uint32_t expected_tag;
	
	uint8_t cdb[16];	// SCSI Command Descriptor Block the SCSI command that we want to send
	uint8_t *buffer=NULL;
	

	if ( !(buffer = (uint8_t *)kmalloc(sizeof(uint8_t)*64, GFP_KERNEL)) ) {
		printk(KERN_INFO"Cannot allocate memory for rcv buffer\n");
		return -1;
	}

	// Read capacity
	printk(KERN_INFO"Reading Capacity:\n");
	
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x25;	// Read Capacity

	r = usb_control_msg(p_usbdev-> device,usb_sndctrlpipe(p_usbdev-> device,0),BOMS_RESET,BOMS_RESET_REQ_TYPE,0,0,NULL,0,1000);
	if(r < 0)
		printk(KERN_INFO "Cannot reset\n");
	else
		printk(KERN_INFO "Reset done\n");

	if( send_mass_storage_command(p_usbdev-> device,p_usbdev-> ep_out,lun,cdb,USB_EP_IN,READ_CAPACITY_LENGTH,&expected_tag) != 0 ) {
		printk(KERN_INFO"Send command error\n");
		return -1;
	}
	
	read_ep=usb_bulk_msg(p_usbdev-> device, usb_rcvbulkpipe(p_usbdev-> device,p_usbdev-> ep_in), (unsigned char*)buffer, READ_CAPACITY_LENGTH, &size, 1000);

	printk(KERN_INFO"r = %d\n",read_ep);
	printk(KERN_INFO"   received %d bytes\n", size);
	max_lba_usb = be_to_int32(buffer);
	block_size_usb = be_to_int32(buffer+4);
	device_size_usb = (max_lba_usb+1)*block_size_usb/(1024*1024*1024);

	printk(KERN_INFO "Device Size: %ld GB\n",device_size_usb);
	kfree(buffer);
	

	get_mass_storage_status(p_usbdev-> device,p_usbdev-> ep_in,expected_tag);

	return 0;
}



int read_usb(unsigned int transfer_length, int lba_read,unsigned char *data)     //transfer_length is in bytes
{
    unsigned char *buff;    //lba_read is in blocks
    int r, size;
    uint8_t cdb[16];
    uint8_t lun=0;
    uint32_t expected_tag;
    //cbw = (struct command_block_wrapper*)kmalloc(sizeof(struct command_block_wrapper), GFP_KERNEL);
    buff = (unsigned char*) kmalloc(transfer_length,GFP_KERNEL);
    if (buff == NULL) {
        printk(KERN_INFO "   unable to allocate buff buffer\n");
        return -1;
    }

    printk(KERN_INFO "Attempting to read %d bytes:\n", transfer_length);
    memset(cdb, 0, sizeof(cdb));

    cdb[0] = READ_10;  // Read(10)
    cdb[2] = (unsigned char)(lba_read >> 24);
	cdb[3] = (unsigned char)(lba_read >> 16);
    cdb[4] = (unsigned char)(lba_read >> 8);
    cdb[5] = (unsigned char)(lba_read);
    cdb[7] = (unsigned char)  ((transfer_length/KERNEL_SECTOR_SIZE) >> 8);
    cdb[8] = (unsigned char)  (transfer_length/KERNEL_SECTOR_SIZE); 

    //send_mass_storage_command(device,endpoint_out,lun,cdb,USB_EP_IN,READ_CAPACITY_LENGTH,&expected_tag)


    send_mass_storage_command(p_usbdev->device, p_usbdev-> ep_out, lun, cdb, USB_EP_IN, transfer_length, &expected_tag);
    //read_ep=usb_bulk_msg(device, usb_rcvbulkpipe(device,endpoint_in), (unsigned char*)buffer, READ_CAPACITY_LENGTH, &size, 1000);


    r = usb_bulk_msg(p_usbdev-> device, usb_rcvbulkpipe(p_usbdev-> device,p_usbdev->ep_in), (unsigned char*)buff, transfer_length, &size, 5000);
    printk(KERN_INFO"   READ: received %d bytes\n", size);
    /*if (get_mass_storage_status(p_usbdev-> device, p_usbdev-> ep_in,expected_tag) == -1) {
        printk(KERN_ERR "RETURN STATUS ERRORNEOUS for read command");
    } */
    printk(KERN_INFO " DATA READ IS %x",buff);
    memcpy(data, buff, size);
    display_buffer_hex(buff, size);
    
    kfree(buff);

    return 0;
}

int write_usb( int lba_write, unsigned int transfer_length, unsigned char *data)
{
	unsigned char *buff;    //lba_read is in blocks
    int r, size;
    uint8_t cdb[16];
    uint8_t lun=0;
    uint32_t expected_tag;
    //cbw = (struct command_block_wrapper*)kmalloc(sizeof(struct command_block_wrapper), GFP_KERNEL);
    buff = (unsigned char*) kmalloc(transfer_length,GFP_KERNEL);
    if (buff == NULL) {
        printk(KERN_INFO "   unable to allocate buff buffer\n");
        return -1;
    }

    printk(KERN_INFO "Attempting to write %d bytes:\n", transfer_length);
    memset(cdb, 0, sizeof(cdb));

    cdb[0] = WRITE_10;  // Read(10)
    cdb[2] = (unsigned char)(lba_write >> 24);
	cdb[3] = (unsigned char)(lba_write >> 16);
    cdb[4] = (unsigned char)(lba_write >> 8);
    cdb[5] = (unsigned char)(lba_write);
    cdb[7] = (unsigned char)  ((transfer_length/KERNEL_SECTOR_SIZE) >> 8);
    cdb[8] = (unsigned char)  (transfer_length/KERNEL_SECTOR_SIZE); 

    //send_mass_storage_command(device,endpoint_out,lun,cdb,USB_EP_IN,READ_CAPACITY_LENGTH,&expected_tag)
    memcpy(buff, data, size);

    send_mass_storage_command(p_usbdev->device, p_usbdev-> ep_out, lun, cdb, USB_EP_IN, transfer_length, &expected_tag);
    //read_ep=usb_bulk_msg(device, usb_rcvbulkpipe(device,endpoint_in), (unsigned char*)buffer, READ_CAPACITY_LENGTH, &size, 1000);


    r = usb_bulk_msg(p_usbdev-> device, usb_rcvbulkpipe(p_usbdev-> device,p_usbdev->ep_out), (unsigned char*)buff, transfer_length, &size, 5000);
    printk(KERN_INFO"   WRITE: sent %d bytes\n", size);

    get_mass_storage_status(p_usbdev-> device,p_usbdev-> ep_in,expected_tag);
    printk(KERN_INFO " DATA WRITTEN IS %x",buff);
    //memcpy(data, buff, size);
    display_buffer_hex(buff, size);
    
    kfree(buff);

    return 0;
}

//int write_usb( unsigned int transfer_length, int lba_read,unsigned char data);

/*************************BLOCK DEVICE****************************/

static int blkdev_open(struct block_device *bdev, fmode_t mode)  
{  
	printk(KERN_INFO "blkdev_open called\n");  
	return 0;  
}  

static void blkdev_release(struct gendisk *gd, fmode_t mode)  
{  
	printk(KERN_INFO"blkdev_release called\n");  
	return;  
}  

 static struct block_device_operations blkdev_ops =  
{  
	.owner =  THIS_MODULE,  
	.open = blkdev_open,  
	.release =  blkdev_release,
}; 



int __do_usbdev_data_transfer(struct request *current_request)  
{  
	int result=-1, num_sect=0;  
	unsigned int direction = rq_data_dir(current_request);  
	struct bio *bio;  
	//rq_for_each_bio(bio, current_request)  
	//{  
		struct req_iterator i;  
		struct bio_vec bvec;  
		sector_t sector = blk_rq_pos(current_request);
		printk(KERN_INFO "Sector to be read or write is %d",sector);  
		rq_for_each_segment(bvec, current_request , i)  
		{  
			char *buffer = NULL;  
			int total_len = bvec.bv_len >> 9;  //check bio_cur_sectors function
			/*
			static inline unsigned int bio_cur_sectors(struct bio *bio)
			{
				if (bio->bi_vcnt)
					return bio_iovec(bio)->bv_len >> 9;

				return 0;
			}
			*/
			unsigned char *tmp_buffer = kmalloc(total_len, GFP_ATOMIC);  
			if(!tmp_buffer)  
			{  
				printk("buffer allocation failed\n");  
				return 0;  
			}  
			if(direction == READ) 
			{  
				result = read_usb( total_len,sector,tmp_buffer);  
				//buffer = __bio_kmap_atomic(bio, i, KM_USER0);  //check funtion kmap_atomic
				buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
				memcpy(buffer, tmp_buffer, total_len);  
				kunmap_atomic(buffer);  
			}  
			if(direction == WRITE) 
			{  
				buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;  
				memcpy(tmp_buffer, buffer, total_len);  
				kunmap_atomic(buffer);  
				result = write_usb( sector, total_len, tmp_buffer);  
			}  
			kfree(tmp_buffer);  
			if(result < 0) 
			{  
				printk(KERN_INFO "USBDEV_SEND_%s_10 FAILED\n",  direction == READ? "READ":"WRITE"); 

 			}  
 			else 
 			{  
 				printk(KERN_INFO "USBDEV_SEND_%s_10 OK\n",  direction == READ? "READ":"WRITE");  
 			}  
 			sector += total_len;  
 		//}  
 		num_sect += total_len/USB_SECTOR_SIZE;  
 	}  
 	return num_sect;  
}

void do_usbdev_data_transfer(struct work_struct *work)  
{  
	struct usbdev_work *usb_work;  
	struct request *current_request;  
	int sectors_xferred=1;  
	unsigned long flags;  
	usb_work = container_of(work, struct usbdev_work, work);  
	current_request = usb_work->req;  
	printk(KERN_INFO"do_usbdev_data_transfer req = %p\n",  current_request);  
	sectors_xferred = __do_usbdev_data_transfer(current_request);  
	spin_lock_irqsave(&p_blkdev->lock, flags); 
	if(!blk_end_request (  current_request, 0, sectors_xferred)) 
	{  
		printk(KERN_INFO "req %p finished\n", current_request);  
		blk_end_request_all(current_request, -EIO);                   ////check funtion in kernel 4.15
	}  
	spin_unlock_irqrestore(&p_blkdev->lock, flags);  
	kfree(usb_work);  
	return;
}

void usbdev_request(struct request_queue *q)  
{  
	struct request *req;  
	struct usbdev_work *usb_work = NULL;  
	while((req = blk_fetch_request(q)) != NULL)
	{
		if (blk_rq_is_passthrough(req)) {
	    printk (KERN_NOTICE "Skip non-fs request\n");
	    __blk_end_request_all(req, -EIO);
	    continue;
		}
		usb_work = kmalloc(  sizeof(struct usbdev_work), GFP_ATOMIC);  
		if(!usb_work) 
		{  
			printk("Memory allocation failed\n");  
			 __blk_end_request_all(req, -EIO); 
			continue;  
		}  
		printk(KERN_INFO "deferring req=%p\n", req);  
		usb_work->req = req;  
		INIT_WORK(&usb_work->work, do_usbdev_data_transfer);  
		queue_work(p_blkdev->usbdevQ, &usb_work->work); 
		//blk_end_request_all(req,-EIO);  
	}  
	return;  
}

int init_usb_bulk(unsigned int capacity)  
{  
	struct gendisk *usb_disk = NULL;  
	p_blkdev = kmalloc(sizeof(struct blkdev_private), GFP_KERNEL);  
	if(!p_blkdev) 
	{  
		printk(KERN_ERR "ENOMEM in %s at %d\n",  __FUNCTION__,  __LINE__);  
		return 0;  
	}  
	memset(p_blkdev, 0, sizeof(struct blkdev_private));  
	p_blkdev->usbdevQ = create_workqueue("usbdevQ");  
	if(!p_blkdev->usbdevQ) 
	{  
		printk("create work queue failed\n"); 
	goto err;  
	}  

	spin_lock_init(&p_blkdev->lock);  
	p_blkdev->queue = blk_init_queue(  usbdev_request, &p_blkdev->lock);  
	usb_disk = p_blkdev->gd = alloc_disk(2);  
	if(!usb_disk)  
		goto err2;  
	usb_disk->major = MAJOR_NR;  
	usb_disk->first_minor = 0;  
	usb_disk->fops = &blkdev_ops;  
	usb_disk->queue = p_blkdev->queue;  
	usb_disk->private_data = p_blkdev;  
	strcpy(usb_disk->disk_name, DEVICE_NAME);  
	set_capacity(usb_disk, capacity);  
	add_disk(usb_disk);  
	printk(KERN_INFO"registered block device\n");  
	return 0;  
	err2:  destroy_workqueue(p_blkdev->usbdevQ);  
	err:  kfree(p_blkdev);  
	printk(KERN_INFO"could not register block device\n");  
	return 0;  
}

void cleanup_usb_bulk(void)  
{  
	struct gendisk *usb_disk = p_blkdev->gd;  
	del_gendisk(usb_disk);  
	blk_cleanup_queue(p_blkdev->queue);  
	destroy_workqueue(p_blkdev->usbdevQ);  
	kfree(p_blkdev);  
	return;  
} 





 


static int usbdev_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int i;
	unsigned char epAddr, epAttr;
    unsigned char endpoint_in = 0, endpoint_out = 0;
    p_usbdev = kmalloc(sizeof(struct usbdev_private),  GFP_KERNEL);  
	//struct usb_device *device;
	//unsigned char *data_read;
	//data_read = (unsigned char*) kmalloc(512*2,GFP_KERNEL);
	struct usb_endpoint_descriptor *ep_desc;
	
        p_usbdev->device = interface_to_usbdev(interface);

	if((id->idProduct == SAN_PID)&(id->idVendor == SAN_VID))
	{
		printk(KERN_INFO "Known SanDisk USB drive detected.\n");
	}
	else if((id->idProduct == SONY_PID)&(id->idVendor == SONY_VID))
	{
		printk(KERN_INFO "Known SONY USB drive detected.\n");
	}
	else if((id->idProduct == MOSR_PID)&(id->idVendor == MOSR_VID))
	{
		printk(KERN_INFO "Known MOSR USB drive detected.\n");
	}
	else{
		printk(KERN_INFO "Unknown USB drive detected.\n");
	}
	
	printk(KERN_INFO "VID = %d \n",id->idVendor);
	printk(KERN_INFO "PID = %d \n",id->idProduct);

	printk(KERN_INFO "USB DEVICE CLASS : %x", interface->cur_altsetting->desc.bInterfaceClass);
	printk(KERN_INFO "USB DEVICE SUB CLASS : %x", interface->cur_altsetting->desc.bInterfaceSubClass);
	printk(KERN_INFO "USB DEVICE Protocol : %x", interface->cur_altsetting->desc.bInterfaceProtocol);

	printk(KERN_INFO "No. of Altsettings = %d\n",interface->num_altsetting);
	printk(KERN_INFO "No. of Endpoints = %d\n", interface->cur_altsetting->desc.bNumEndpoints);

	for(i=0;i<interface->cur_altsetting->desc.bNumEndpoints;i++)
	{
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;
		epAddr = ep_desc->bEndpointAddress;
		epAttr = ep_desc->bmAttributes;
	
		if((epAttr & USB_ENDPOINT_XFERTYPE_MASK)==USB_ENDPOINT_XFER_BULK)
		{
			if(epAddr & 0x80){
				endpoint_in = ep_desc->bEndpointAddress;
				p_usbdev-> ep_in = endpoint_in;
				printk(KERN_INFO "EP %d is Bulk IN - %d \n", i, endpoint_in);
			}
			else{
				endpoint_out = ep_desc->bEndpointAddress;
				p_usbdev-> ep_out = endpoint_out;
				printk(KERN_INFO "EP %d is Bulk OUT - %d \n", i, endpoint_out);
			}
		}
	}

	if((interface->cur_altsetting->desc.bInterfaceClass == 0x08)&((interface->cur_altsetting->desc.bInterfaceSubClass == 0x01)|(interface->cur_altsetting->desc.bInterfaceSubClass == 0x06))&(interface->cur_altsetting->desc.bInterfaceProtocol == 0x50)){
		printk(KERN_INFO "Connected device is a USB Mass Storage - SCSI device");
	}
	else{
		printk(KERN_INFO "Connected device doesnot support SCSI commands");
	}

	if(read_capacity() !=0)
	{ 
		printk(KERN_INFO"error in reading capacity \n");
		return -1;
	}

	/*if(read_usb(512,1,data_read) !=0)
	{ 
		printk(KERN_INFO"error in reading capacity \n");
		return -1;
	}*/

	init_usb_bulk(max_lba_usb+1); 
	return 0; 
}



	


/*Operations structure*/
static struct usb_driver usbdev_driver = {
	name: "usbdev",  //name of the device
	probe: usbdev_probe, // Whenever Device is plugged in
	disconnect: usbdev_disconnect, // When we remove a device
	id_table: usbdev_table, //  List of devices served by this driver
};


int device_init(void)
{
	usb_register(&usbdev_driver);
	return 0;
}

void device_exit(void)
{
    usb_deregister(&usbdev_driver);
}

module_init(device_init);
module_exit(device_exit);
MODULE_LICENSE("GPL");



