ifneq ($(KERNELRELEASE),)
	obj-m := hd44780-cuimhne.o
	hd44780-cuimhne-y := hd44780-i2c.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

VERSION=1.0.0

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif

