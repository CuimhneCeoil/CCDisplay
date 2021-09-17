# Developer notes

## Building

This code support two methods of building.  The recommended way is to use DKMS to build and package the project.  You must have DKMS installed on your system.  Running `DKMS=1 make install` will build and install the package on your system.

Alternatively the standard make is also available.  This results in a kernel object that you have to install yourself.

A recommended approach is to execute the following:

```
export KERNELDIR=../linux
export ARCH=arm
export CROSS_COMPILE=../tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi-
make build
```