# st7565-linux

![Intro image](http://cyantist.org/hello.jpg "Intro Image")

## Overview

This is a proof of concept for using a small graphical LCD with the ST7565 chipset as a character device in Linux.

So far, the code has been tested on a BeagleBone Black running Debian 7.0 (Wheezy) connected to this LCD:
>http://www.adafruit.com/products/438

## Setup and How-To

A fair amount of work has to be done first, before the driver code can be compiled. The community for the BeagleBone Black has made this pretty easy though.

1. Prepare an SD card with a Debian image:
>http://elinux.org/BeagleBoardDebian

2. Install a new kernel and kernel headers. You must make sure to choose one that has the kernel headers included; not all of them do.
    >http://rcn-ee.net/deb/wheezy-armhf/

    For example, this has the kernel headers included:
    >http://rcn-ee.net/deb/wheezy-armhf/v3.8.13-bone23/

    Now, just grab the installer script and run it:
    ```
	wget http://rcn-ee.net/deb/wheezy-armhf/v3.8.13-bone23/install-me.sh
	su
	sh install-me.sh
	reboot
	```

	After rebooting, run 'uname -r' and the result should be '3.8.13-bone23'.

3. Use this script to download, patch and prepare the kernel source for your board. This will not work if you don't have kernel headers available.
	>https://github.com/gkaindl/beaglebone-ubuntu-scripts/blob/master/bb-get-rcn-kernel-source.sh

4. Now you should be able to compile kernel drivers.
	```
	make clean
	make
	su
	insmod st7565.ko
	```
	The command 'dmesg' will print kernel messages and help you debug problems.

5. Wire up the LCD. You can change the pin definitions in st7565.c to suit your needs. By default, they are:
	```
	#define ST7565_CS	GPIO_TO_PIN(1, 13)
	#define ST7565_RST	GPIO_TO_PIN(1, 12)
	#define ST7565_A0	GPIO_TO_PIN(0, 26)
	#define ST7565_CLK	GPIO_TO_PIN(1, 15)
	#define ST7565_SI	GPIO_TO_PIN(1, 14)
	```

6. Finally, print some text on the LCD!
	```
	su
	echo "hi from linux" > /dev/st7565
	```

## Notes

This driver bit-bangs SPI instead of using the hardware SPI. The reason is that the hardware SPI has a pin conflict with the HDMI output on the BeagleBone Black, and I found it a better compromise to bit-bang rather than disable the HDMI.

You also might notice that at several points above, I used 'su' instead of 'sudo', which is probably considered poor practice. The reason for this is that several of the required steps fail with sudo.