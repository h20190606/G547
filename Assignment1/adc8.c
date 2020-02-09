#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/kdev_t.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/random.h>

#define MAGIC_NUM 100
#define CHANNEL_NO _IOW(MAGIC_NUM,0,int32_t*)
#define ALIGNMENT _IOW(MAGIC_NUM,1,char*)


int32_t channel_no=0;
char alignment;
char L,R;
static dev_t adc;
static int __init adc8_init(void);
static struct cdev c_dev;
static struct class *cls;
static void __exit adc8_exit(void);
uint8_t *kernel_buff; 
static unsigned int i;
unsigned int align;


static int permission(struct device* dev,struct kobj_uevent_env* env)
{
	add_uevent_var(env,"DEVMODE=%#o",0666);
	return 0;
}

static int adc_open(struct inode *inode,struct file *file)
{
	printk(KERN_INFO"Device file is opened\n");
	return 0;	
}

static int adc_close(struct inode *inode,struct file *file)
{
	//kfree(kernel_buff);
	printk(KERN_INFO"Device file closed\n");
	return 0;
} 


static ssize_t adc_read(struct file *file, char __user *buf,size_t len, loff_t *off)
{
	
	get_random_bytes(&i,2);
	i = i % 1024;
	if(align ==1)
	{
		i = i << 6;
	}
	else if(align ==2)
	{
		i = i >> 6;
	}
	printk(KERN_INFO"ADC Reading is %d\n\n",i);
	copy_to_user(buf,&i,sizeof(i));
	return sizeof(i);
	return 0;
}

static long adc_ioctl(struct file *file,unsigned int cmd,unsigned long arg)
{
	switch(cmd)
	{	
		case CHANNEL_NO:
				copy_from_user(&channel_no,(int32_t*)arg,sizeof(channel_no));
				printk(KERN_INFO"The selected channel no is %d\n",channel_no);
		break;
		case ALIGNMENT:
				
				copy_from_user(&alignment,(char*)arg,sizeof(alignment));
				printk(KERN_INFO"Required alignemnt is %c\n",alignment);
				if(alignment == L) align = 1;
				else if(alignment == R) align =2;
				else return -1;								
				
				
		break;

	}
return 0;
}


//###################################################

static struct file_operations fops =
{
	.owner          = THIS_MODULE,
	.open           = adc_open,
	.unlocked_ioctl = adc_ioctl,
	.release        = adc_close,
	.read           = adc_read
};




static int __init adc8_init(void)
{
	/*reserve major and minor number*/
	if((alloc_chrdev_region(&adc,0,1,"ADC")) < 0) {
		printk(KERN_INFO"Cannot allocate the major number..\n");
		return -1;
	}

	printk(KERN_INFO"Major=%d,Minor=%d\n",MAJOR(adc),MINOR(adc));
	
	/*dynamically create device node*/
	if((cls = class_create(THIS_MODULE,"chardrv")) == NULL)
	{
		unregister_chrdev_region(adc,1);
		return -1;
	}


	cls ->dev_uevent = permission;
	
	if(device_create(cls,NULL,adc,NULL,"adc_device") == NULL)
	{
		class_destroy(cls);
		unregister_chrdev_region(adc,1);
		return -1;
	}

	//Link fops and cdev to device node
	cdev_init(&c_dev,&fops);
	if(cdev_add(&c_dev,adc,1) == -1)
	{
		device_destroy(cls,adc);
		class_destroy(cls);
		unregister_chrdev_region(adc,1);
		return -1;
	}
	printk(KERN_INFO"ADC Device registered successfully\n\n");
	return 0;

}

static void __exit adc8_exit(void)
{
	cdev_del(&c_dev);	
	device_destroy(cls,adc);
	class_destroy(cls);
	unregister_chrdev_region(adc,1);
	printk(KERN_INFO"ADC driver unregistered \n\n");
}


module_init(adc8_init);
module_exit(adc8_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("JOLLY MISHRA");
MODULE_DESCRIPTION("Adc Character device driver");






