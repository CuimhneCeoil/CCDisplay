VERSION:=$(shell cat version.txt)

.PHONY: all, clean, install, install-usr, install-etc, update-debian

all: scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf 

update-debian:
	sed -i "s/^Version:.*$$/Version: $(VERSION)/g" debian/control
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/changelog
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/postinst
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/postrm
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/preinst
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/prerm
	

scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf:
	echo "PACKAGE_NAME=\"hd44780-i2c\"" > scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	echo "PACKAGE_VERSION=\"$(VERSION)\"" >> scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	echo "BUILT_MODULE_NAME[0]=\"hd44780-i2c\"" >> scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	echo "DEST_MODULE_LOCATION[0]=\"/kernel/drivers/auxdisplay\"" >> scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	echo "AUTOINSTALL=\"yes\"" >> scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	


clean:
	rm -f scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	
install-usr: scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	install -d $(DESTDIR)/usr
	for fn in `find scripts/usr -type d | sed 's+scripts/usr++g'`; do install -m 755 -d $(DESTDIR)/usr$$fn; done
	for fn in `find scripts/usr -type f | sed 's+scripts/usr++g'`; do install -m 644 scripts/usr$$fn $(DESTDIR)/usr$$fn; done

install-etc:
	install -d $(DESTDIR)/etc
	for fn in `find scripts/etc -type d | sed 's+scripts/etc++g'`; do install -m 755 -d $(DESTDIR)/etc$$fn; done
	for fn in `find scripts/etc -type f | sed 's+scripts/etc++g'`; do install -m 644 scripts/etc$$fn $(DESTDIR)/etc$$fn; done

install:  install-usr install-etc	