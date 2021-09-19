#/usr/bin/make
SRC = $(DESTDIR)/usr/src
SHARE = $(DESTDIR)/usr/share/$(NAME)-dkms

all:

clean:

install-etc:
	echo "YAH install-etc <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
	install -d $(DESTDIR)/etc
	for fn in `find etc -type d `; do install -m 755 -d $(DESTDIR)/$$fn; done
	for fn in `find etc -type f `; do install -m 644 $$fn $(DESTDIR)/$$fn; done

install:  install-etc	

#source tree
ifeq ("$(wildcard $(NAME)-$(VERSION))", "$(NAME)-$(VERSION)")
	install -d "$(SRC)"
	cp -a $(NAME)-$(VERSION) $(SRC)
	chmod 644 -R "$(SRC)/$(NAME)-$(VERSION)"
endif

#tarball, possibly with binaries
ifeq ("$(wildcard $(NAME)-$(VERSION).dkms.tar.gz)", "$(NAME)-$(VERSION).dkms.tar.gz")
	install -d "$(SHARE)"
	install -m 644 $(NAME)-$(VERSION).dkms.tar.gz "$(SHARE)"
endif

#postinst, only if we are supporting legacy mode
ifeq ("$(wildcard common.postinst)", "common.postinst")
	install -d "$(SHARE)"
	install -m 755 $(PREFIX)/usr/lib/dkms/common.postinst $(SHARE)/postinst
endif
