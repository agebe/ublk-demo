# ublk-demo

sudo apt install liburing-dev
https://docs.kernel.org/block/ublk.html
sudo apt install autoconf
https://github.com/ublk-org/ublksrv

# libtool is required to run 'autoreconf -i' to compile the ublksrv
sudo apt install libtool

https://cateee.net/lkddb/web-lkddb/BLK_DEV_UBLK.html
https://www.google.com/search?client=firefox-b-e&q=debian+how+to+change+kernel+configuration+and+rebuild+modules
To change the kernel configuration and rebuild modules in Debian, you can do the following: 

    Go to /usr/src/linux
    Enter the command make config
    Select the features you want the kernel to support
    Install or update the kernel sources
    Create or modify the kernel configuration file
    Build the kernel from the configuration file
    Install the kernel 

You can use the modprobe command to load a module and set options. You can specify which options to use for individual modules by using configuration files under /etc/modprobe. 

grep BLK_DEV_UBLK /boot/config-6.1.0-28-amd64 
# CONFIG_BLK_DEV_UBLK is not set

I've installed the kernel ublk_drv by installing kernel source with 'apt source linux' and building as described above. Then copy the ublk_drv.ko into /lib/modules/{kernelversion}/updates/block/ and followed the instructions below.

https://www.google.com/search?client=firefox-b-e&q=what+is+the+%2Flib%2Fmodules+updates+directory+for+in+debian

The /lib/modules/{kernelversion}/updates directory in Debian is used to store kernel modules that have precedence. The kernelversion is the output of uname -r. 
Here are some steps to test kernel modules: 

    Create the /lib/modules/{kernelversion}/updates directory
    Copy the driver to /lib/modules/{kernelversion}/updates/
    Run depmod -a to resolve dependencies
    Run modprobe {driver} to load the driver
    Run lsmod | grep {driver} to check if the driver loaded
    Run modinfo {driver} to confirm the driver loaded from the /lib/modules/{kernelversion}/update directory

---

the [demo_null.c](https://github.com/ublk-org/ublksrv/blob/master/demo_null.c) is from the git repo ublksrv







