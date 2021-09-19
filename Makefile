
VERSION:=$(shell cat src/hd44780-i2c/dkms.conf | grep "^PACKAGE_VERSION" | cut -f2 -d'"')
HAS_SUDO=$(shell if [ `groups | grep -c "sudo"` -eq 1 ] ; then echo "sudo"; fi )
IS_ROOT:=$(shell  if [ `id -u` -eq 0 ] ; then  echo 'printf ""; ' ; fi )
SUDO:=$(or $(HAS_SUDO), $(IS_ROOT), $(error sudo or root access is required) )
ARCH_FLg:=$(if $(ARCH), -k $(ARCH))

help: 
	@echo hd44780-i2c makefile.
	@echo " "
	@echo Targets:
	@echo 	help............This help
	@echo	prolog..........The Standard/DKMS build message
	@echo	build...........Compiles the source.  In DKMS mode ARCH specifies the version to build, otherwise the current kernel and arch is built.
	@echo	clean...........Clean up
	@echo	remove..........\(DKMS only\) Removes the source from the module tree.  If ARCH is not specified removes all.
	@echo	install.........\(DKMS only\) Installs the driver. If specified, ARCH identifies the version to install, otherwise the current kernel and arch is installed.
	@echo	uninstall.......\(DKMS only\) Uninstalls the driver. If specified, ARCH identifies the version to uninstall, otherwise the current kernel and arch is uninstalled.
	@echo	pkg.............\(DKMS only\) Builds a debian package of the source. If specified, ARCH identifies the version to package, otherwise the current kernel and arch is packaged.
	@echo " "
	@echo Options
	@echo DKMS - Specifies that a DKMS bild should be used 
	@echo For example "DKMS=1 make install" will use DKMS to build and install.
	@echo ""
	@echo ARCH - Specifies the specific architecture or kernel to manipulate.

prolog : 
ifeq ($(DKMS),)
	@echo "Satandard build"
else
	@echo "DKMS style build"
endif	

build: prolog 
ifeq ($(DKMS),)
	$(MAKE) -C src
else
	$(if $(shell dkms status | grep "hd44780-i2c, $(VERSION)"), $(shell $(SUDO) cp -r src/hd44780-i2c/* /usr/src/hd44780-i2c-$(VERSION)), $(SUDO) dkms add src/hd44780-i2c )
	$(SUDO) dkms build hd44780-i2c/${VERSION} $(ARCH_Flg)
endif

remove: prolog
ifeq ($(DKMS),)
	@echo "No standard remove available"
else
	$(SUDO) dkms remove hd44780-i2c/${VERSION} $(if $(ARCH), -k $(ARCH), --all )
endif

clean: prolog
ifeq ($(DKMS),)
	$(MAKE) -C src clean
else
	$(SUDO) dkms remove hd44780-i2c/${VERSION} $(if $(ARCH), -k $(ARCH), --all )
	rm -rf /usr/src/hd44780-i2c/${VERSION}
endif

install: default
ifeq ($(DKMS),)
	@echo "No standard installer available"
else
	$(SUDO) dkms install hd44780-i2c/${VERSION} $(ARCH_Flg)
endif

uninstall: prolog
ifeq ($(DKMS),)
	@echo "No standard uninstaller available"
else
	$(SUDO) dkms uninstall hd44780-i2c/${VERSION} $(ARCH_Flg)
endif

pkg: prolog 
ifeq ($(DKMS),)
	@echo "No standard debian package builder available"
else
	$(if $(shell dkms status | grep "hd44780-i2c, $(VERSION)"), , $(SUDO) dkms add src/hd44780-i2c )
	$(SUDO) dkms mkdeb hd44780-i2c/${VERSION} $(ARCH_Flg) --source-only
endif
