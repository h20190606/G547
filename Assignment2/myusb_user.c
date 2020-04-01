#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <ctype.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<time.h>

int read_buf;
int write_buf;

int main()
{

	int fd;
	int i;
	int j=0;

				

	printf("Testing USB Device Driver\n");
	fd = open("/dev/myusb", O_RDWR);
	if(fd < 0)
	{
		printf("Change the permission of /dev/myusb to 666\n");
		return 0;
	}
			printf("Writing command to USB\n");
			write(fd,&write_buf,10);
			printf("Done\n"); 

			for(i=0;i<10000;i++)
			{
				j = j + 1;
			}

			printf("Reading from USB\n");
			read(fd,&read_buf,5);
			printf("Done\n"); 
						
 
	close(fd);

}
			









