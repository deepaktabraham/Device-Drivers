# USB Keyboard Driver
The Linux USB Keyboard Driver is modified to change the way the CAPSLOCK LED is turned ON.

In the original driver, the CAPSLOCK LED is turned ON when the CAPSLOCK key is pressed for the very first time. Similarly, it is turned OFF when it is pressed again. This behavior repeats with each CAPSLOCK key press.

Two new modes are introduced in this driver: ***MODE1*** and ***MODE2***. When the driver starts functioning, it is in MODE1, in which the CAPSLOCK will be handled as usual. MODE2 will be activated when NUMLOCK is pressed, CAPSLOCK is not pressed, and CAPSLOCK is not ON. 

When transitioning to MODE2, the CAPSLOCK LED will be turned ON automatically. MODE2 will be active until NUMLOCK is pressed again. However, when in MODE2, CAPSLOCK LED will turn OFF when the CAPSLOCK is pressed the first time after transitioning to MODE2 and will turn ON when it is pressed the next time, and so ON. 

In MODE2, the driver will leave the CAPSLOCK LED status in a way that will be compatible with MODE1. As a result, in MODE2, when the CAPSLOCK LED is OFF (or ON) you will see that the characters that are typed ON the keyboard will be displayed in upper (or lower) case mode.

### Steps 
1. Perform the command **`sudo apt-get install linux-headers-$(uname -r)`** to install linux headers.
2. Build usbkbd driver using the **`make`** command.
3. Insert the driver using the command **`sudo insmod usbkbd.ko`**
4. Ensure USB keyboard is connected and detected in the Machine (or VM). Bind the usbkbd driver to the USB keyboard by the running the test.sh script as **`sudo ./test.sh`**.
5. Test the keyboard. Check dmesg logs to ensure correct behavior.
6. Remove usbkbd driver using the **`sudo rmmod usbkbd`** command.
