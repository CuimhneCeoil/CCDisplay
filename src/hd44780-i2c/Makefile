
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

ifneq ($(KERNELRELEASE),)

obj-m := hd44780-i2c.o
hd44780-i2c := hd44780-i2c.o

else

default:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) clean

endif
	
