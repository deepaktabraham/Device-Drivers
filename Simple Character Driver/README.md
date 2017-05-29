# Simple Character Driver
A character device driver supporting variable number of devices is implemented here.
The devices support concurrent file operations like open, close, read, write and lseek.
An IOCTL to clear device buffer is also implemented.


### Important Commands
1. To install linux headers: **`sudo apt-get install linux-headers-$(uname -r)`**
2. To compile the LKM: **`make`**
3. To clean the compiled objects: **`make clean`**
4. To insert the module: **`sudo insmod char_driver.ko`** (creates three /dev entries - /dev/mycdev0, /dev/mycdev1 and /dev/mycdev2 by default)
5. To insert the module with variable number of devices, use the ***NUM_DEVICES*** parameter. For example, **`sudo insmod char_driver.ko NUM_DEVICES=5`** will create five /dev entries
6. To check if the module is inserted: **`lsmod | grep char_driver`**
7. To remove the LKM: **`sudo rmmod char_driver`**


### Using the custom IOCTL
The IOCTL ***CLEAR_BUF*** which clears the device buffer, is declared in the header file mycdev.h
Add mycdev.h in your test program so as to correctly call this custom ioctl.
That is, include the following line to your test program (replace <path_to_directory> appropriately): <br />
***#include "<path_to_directory>/mycdev.h"***

You can also achieve the same effect by defining the following in your test program: <br />
***#include <linux/ioctl.h>*** <br />
***#define CLEAR_BUF ('k', 1)***

The following system call can be added in your test program to invoke this ioctl (replace <file_desc> appropriately):
***ioctl(<file_desc>, CLEAR_BUF);***


### NOTE:
Make sure that you have read and write privilege for the device entries created during insertion of the module
before testing. 
One way of achieving this is to run the following command in the terminal before running your test program: <br />
**`sudo chmod 660 /dev/mycdev*`**

You can also achieve the same effect by running your test program with superuser privilege: <br />
**`$ sudo ./<executable_test_app>`**
