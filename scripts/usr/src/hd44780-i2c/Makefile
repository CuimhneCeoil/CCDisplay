VERSION=1.0.0

ifneq ($(KERNELRELEASE),)
	obj-m := hd44780-i2c.o
	hd44780-i2c := hd44780-i2c.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
	echo "KERNELDIR = $(KERNELDIR)"

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
	
