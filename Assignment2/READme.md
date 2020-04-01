To remove the kernel loaded USB drivers
$ sudo rmmod uas
$ sudo rmmod usb_storage

To insert our USB driver.
$ sudo insmod myusb_driver.ko

Now insert the pendrive.

Changing the permission of the file
$ cd /dev
$ sudo chmod 666 myusb

Compiling and executing
$ gcc -o user myusb_user.c
$./user

To check the kernel logs
$ dmesg

The VID, PID and other details can be seen by scrolling up the logs if not immediate above "Sent Command". 
