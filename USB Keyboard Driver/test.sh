#!/bin/bash
echo -n "2-2:1.0" > /sys/bus/usb/drivers/usbhid/unbind
echo -n "2-2:1.0" > /sys/bus/usb/drivers/usbkbd/bind
