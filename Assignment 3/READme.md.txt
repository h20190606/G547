To remove the kernel loaded USB drivers 
$ sudo rmmod uas 
$ sudo rmmod usb_storage
To insert the driver. 
$ sudo insmod main.ko
Now insert the pendrive.
To mount the PD Filesystem, mount -t vfat /dev/USB_BLOCK_DRIVER /mnt
To check kernel logs, dmesg

