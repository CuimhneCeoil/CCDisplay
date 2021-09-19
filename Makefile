
VERSION:=$(shell cat src/hd44780-i2c/dkms.conf | grep "^PACKAGE_VERSION" | cut -f2 -d'"')
HAS_SUDO=$(shell if [ `groups | grep -c "sudo"` -eq 1 ] ; then echo "sudo"; fi )
IS_ROOT:=$(shell  if [ `id -u` -eq 0 ] ; then  echo 'printf ""; ' ; fi )
SUDO:=$(or $(HAS_SUDO), $(IS_ROOT), $(error sudo or root access is required) )
ARCH_FLg:=$(if $(ARCH), -k $(ARCH))
SRC_ADDED=$(shell dkms status -v $(VERSION) "hd44780-i2c")
RMV_CMD:=$(SUDO) dkms remove hd44780-i2c/${VERSION} $(if $(ARCH), -k $(ARCH), --all )
ADD_CMD:=$(if $(NO_LOCAL), $(SUDO) dkms add  src/hd44780-i2c, $(SUDO) dkms add --sourcetree ${PWD}/build src/hd44780-i2c )

help: 
	@echo hd44780-i2c makefile.
	@echo " "
	@echo Targets:
	@echo 	help............This help
	@echo	prolog..........The Standard/DKMS build message
	@echo	build...........Compiles the source.  In DKMS mode ARCH specifies the version to build, otherwise the current kernel and arch is built. In DKSM mode will refresh the sourec tree.  See NO_LOCAL below.
	@echo	clean...........Clean up. in DKMS mode removes the source tree.  See NO_LOCAL below.
	@echo	install.........\(DKMS only\) Installs the driver. If specified, ARCH identifies the version to install, otherwise the current kernel and arch is installed.
	@echo	uninstall.......\(DKMS only\) Uninstalls the driver. If specified, ARCH identifies the version to uninstall, otherwise the current kernel and arch is uninstalled.
	@echo	pkg.............\(DKMS only\) Builds a debian package of the source. If specified, ARCH identifies the version to package, otherwise the current kernel and arch is packaged.  Will refresh the source tree.  See NO_LOCAL below.
	@echo " "
	@echo Options
	@echo DKMS - Specifies that a DKMS build should be used. User must be root or have sudo access to execute DKSM builds.
	@echo For example \"DKMS=1 make install\" will use DKMS to build and install.
	@echo ""
	@echo ARCH - Specifies the specific architecture or kernel to manipulate.
	@echo ""
	@echo NO_LOCAL - Specifies that the /usr/src tree should be used rather than $(PWD)/build. 

prolog : 
	@echo Version: $(VERSION)
ifeq ($(DKMS),)
	@echo "Satandard build"
else
	@echo "DKMS style build"
	@echo "SRC_ADDED: $(SRC_ADDED)"
	@echo "ARCH: $(ARCH_Flg)"
	@echo "NO_LOCAL: $(NO_LOCAL)"
endif	

build: prolog 
ifeq ($(DKMS),)
	$(MAKE) -C src
else
	$(if $(SRC_ADDED), $(RMV_CMD) )
	$(ADD_CMD)
	$(SUDO) dkms build hd44780-i2c/${VERSION} $(ARCH_Flg)
endif

clean: prolog
ifeq ($(DKMS),)
	$(MAKE) -C src clean
else
	$(if $(SRC_ADDED),  $(RMV_CMD))
	$(if $(NO_LOCAL), , $(SUDO) rm -rf ${PWD}/build/hd44780-i2c-$(VERSION) )
	$(SUDO) rm -f ./build/hd44780-i2c-dkms_$(VERSION)_*
endif

install: build
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
	$(if $(SRC_ADDED),  $(RMV_CMD))
	$(ADD_CMD)
	$(SUDO) dkms mkdeb hd44780-i2c/${VERSION} $(ARCH_Flg) --source-only 
	mkdir -p ./build
	$(SUDO) cp /var/lib/dkms/hd44780-i2c/1.0.0/deb/* ./build
endif
