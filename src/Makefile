KERNELDIR ?= /lib/modules/$(shell uname -r)/build

OK:=$(or $(shell if [ -d ${KERNELDIR} ]; then echo "ok"; fi ), $(error directory ${KERNELDIR} must exist. Have you installed the kernel headers"))

build:
	$(MAKE) -C hd44780-i2c 

clean:
	$(MAKE) -C hd44780-i2c clean
