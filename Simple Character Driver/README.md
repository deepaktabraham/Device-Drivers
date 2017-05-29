# Simple Character  Driver
A character device driver supporting variable number of devices is implemented here.
The devices support concurrent file operations like open, close, read, write and lseek.
An IOCTL to clear device buffer is also implemented.


### Command to compile the LKM
The program char_driver.c can be compiled to create a loadable kernel module using the Makefile with the command ***make***


### Command to clean the compiled objects
Use **`make clean`** to clean the compiled objects


### Steps to insert the module
The default command which creates three /dev entries (/dev/mycdrv0, /dev/mycdrv1 and /dev/mycdrv2) is **`sudo insmod char_driver.ko`**

Variable number of devices can be initialized by using "NUM_DEVICES" parameter.
Example: The following command will create five /dev entries -
**`sudo insmod char_driver.ko NUM_DEVICES=5`**


### Command to check if the module is inserted
**`lsmod | grep char_driver`**


### Command to remove the LKM
**`sudo rmmod char_driver`**


### Custom IOCTL
The IOCTL ***CLEAR_BUF*** to clear the device buffer is declared in the header file mycdev.h
Add mycdev.h in your test program, so as to correctly call this custom ioctl.
That is, include the following line to your test program (replace <path_to_directory> appropriately):
#include "<path_to_directory>/mycdev.h"

You can also achieve the same effect by defining the following in your test program:
#include <linux/ioctl.h>
#define CLEAR_BUF ('k', 1)

The following system call can be added in your test program to invoke this ioctl (replace <file_desc> appropriately):
ioctl(<file_desc>, ASP_CLEAR_BUF);


NOTE:
Please make sure that you have read and write privilege for the device entries created during insertion of the module,
before testing. 
One way of achieving this is to run the following command in the terminal, before running your test program:
$ sudo chmod 660 /dev/mycdrv*

You can also achieve the same effect by running your test program, with superuser privilege:
$ sudo ./<executable_test_app>
