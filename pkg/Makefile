VERSION:=$(shell cat ../src/hd44780-i2c*/dkms.conf | grep "^PACKAGE_VERSION" | cut -f2 -d'"')

.PHONY: all, clean, install, install-usr, install-etc, update-debian, debian-pkg

update-debian:
	gbp dch --ignore-branch -S -c --git-author -N ${VERSION} ; \
	sed -i "s/^Version:.*$$/Version: $(VERSION)/g" debian/control
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/postinst
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/postrm
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/preinst
	sed -i "s/VERSION_STRING/$(VERSION)/g" debian/prerm
	
	mkdir -r ./scripts/usr
	
	cp -r ../src/* ./scripts/usr/src

all: update-debian 

clean:
	
	
install-usr: scripts/usr/src/hd44780-i2c-$(VERSION)/dkms.conf
	install -d $(DESTDIR)/usr
	for fn in `find scripts/usr -type d | sed 's+scripts/usr++g'`; do install -m 755 -d $(DESTDIR)/usr$$fn; done
	for fn in `find scripts/usr -type f | sed 's+scripts/usr++g'`; do install -m 644 scripts/usr$$fn $(DESTDIR)/usr$$fn; done

install-etc:
	install -d $(DESTDIR)/etc
	for fn in `find scripts/etc -type d | sed 's+scripts/etc++g'`; do install -m 755 -d $(DESTDIR)/etc$$fn; done
	for fn in `find scripts/etc -type f | sed 's+scripts/etc++g'`; do install -m 644 scripts/etc$$fn $(DESTDIR)/etc$$fn; done

install:  install-usr install-etc	

debian-pkg: update-debian 
	make update-debian
	dh clean
	dh build
	fakeroot dh binary
