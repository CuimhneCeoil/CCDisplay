ifneq ($(KERNELRELEASE),)
	obj-m := hd44780-i2c.o
	hd44780-i2c := hd44780-i2c.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

VERSION=1.0.0

dkms.conf: Makefile
	echo "PACKAGE_NAME=\"hd44780-i2c\"" > dkms.conf
	echo "PACKAGE_VERSION=\"$(VERSION)\"" >> dkms.conf
	echo "BUILT_MODULE_NAME[0]=\"hd44780-i2c\"" >> dkms.conf
	echo "DEST_MODULE_LOCATION[0]=\"/kernel/drivers/auxdisplay\"" >> dkms.conf
	echo "AUTOINSTALL=\"yes\"" >> dkms.conf

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
	
