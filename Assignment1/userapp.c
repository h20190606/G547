#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/ioctl.h>

int read_buf;
int32_t ch_no;
char align;
#define MAGIC_NUM 100
#define CHANNEL_NO _IOW(MAGIC_NUM,0,int32_t*)
#define ALIGNMENT _IOW(MAGIC_NUM,1,char*)

int main()
{

	int fd;
	int32_t val;
	int option;

	printf("Testing ADC Device Driver\n");
	fd = open("/dev/adc_device", O_RDWR);
	if(fd < 0)
	{
		printf("Cannot open the device file\n");
		return 0;
	}
	
		CHANNEL:printf("Select ADC Channel \n");
			printf("Enter the channel number from 0 to 7\n\n");
			scanf("%d",&ch_no);
			if(ch_no <= 7 && ch_no >=0)
			{
			printf("Writing the value to the driver\n\n");
			printf("The selected channel number is %d\n\n",ch_no);
			ioctl(fd,CHANNEL_NO,(int32_t*)&ch_no);
			}
			else
			{
			printf("Please select a channel from 0 to 7 range\n\n");
			goto CHANNEL;			
			}	

		ALIGN: 	printf("Select the alignment for ADC reading (L/H)\n\n");
			scanf("   %c",&align);
			if(align == 'L' | align == 'H')
			{
			printf("Writing the value to the driver\n\n");
			printf("Your selected alignment is %c\n\n",align);
			ioctl(fd,ALIGNMENT,(char*)&align);
			}
			else
			{	
			printf("Enter a valid alignment\n\n");
			goto ALIGN;
			}
				
			printf("Reading ADC value from selected channel\n");
			read(fd,&read_buf,10);
			printf("Done\n");
			printf("ADC Channel Reading = %d\n\n",read_buf); 
						
 
	close(fd);

}
			









