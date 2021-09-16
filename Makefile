
VERSION:=$(shell cat src/hd44780-i2c/dkms.conf | grep "^PACKAGE_VERSION" | cut -f2 -d'"')
HAS_SUDO=$(shell if [ `groups | grep -c "sudo"` -eq 1 ] ; then echo "sudo"; fi )
IS_ROOT:=$(shell  if [ `id -u` -eq 0 ] ; then  echo 'printf ""; ' ; fi )
SUDO:=$(or $(HAS_SUDO), $(IS_ROOT), $(error sudo or root access is required) )
DKMS:=$(shell echo ${DKMS})
ALL : default 

prolog : 
	echo DKMS=$(DKMS)
	echo "HAS_SUDO: ${HAS_SUDO}"
	echo "SUDO: [${SUDO}]"
	$(SUDO) echo "YAY"

default: prolog 
ifndef ($(DKMS))
	echo "Standard make"
	$(MAKE) -C src
else
	$(SUDO) dkms add src/hd44780-i2c
	$(SUDO) dkms build hd44780-i2c/${VERSION}
endif

clean: prolog
ifndef ($(DKMS))
	echo "Standard make"
	$(MAKE) -C src clean
endif

install: prolog
ifndef ($(DKMS))
	echo "No standard installer available"
else
	$(SUDO) dkms install hd44780-i2c/${VERSION}
endif

deb: prolog 
ifndef ($(DKMS))
	echo "No standard debian package builder available"
else
	$(SUDO) dkms dpkg hd44780-i2c/${VERSION}i --source-only
endif
