VERSION=1.0.0

ifneq ($(KERNELRELEASE),)
	obj-m := hd44780-i2c.o
	hd44780-i2c := hd44780-i2c.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	echo "START OF DEBUG"
	export
	echo "END OF DEBUG"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
	
