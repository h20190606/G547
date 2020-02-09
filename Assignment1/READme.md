- To read insert the kernel module
	$make 
	$sudo insmod adc8.ko 

- To run the user application
	$gcc -o userapp userapp.c
	$./userapp

- Then enter the ADC channel,alignment

- To remove the kernel module
	$sudo rmmod adc8.ko
	$make clean
