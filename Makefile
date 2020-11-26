VERSION:=$(shell cat debian/control | grep "^Version:" | cut -f2 -d' ')

DESTDIR?=debian
VARDIR?=/var
ETCDIR?=/etc
USRDIR?=/usr



.PHONY: all, clean, install, install-etc, install-var

all: scripts/var/local/cuimhne/configs/rec_comment.txt debian/requirements.txt

clean:
	rm -f scripts/var/local/cuimhne/configs/rec_comment.txt
	rm -f debian/requirements.txt

scripts/var/local/cuimhne/configs/rec_comment.txt: debian/control
	echo "Cuimhne Ceoil V $(VERSION)" > scripts/var/local/cuimhne/configs/rec_comment.txt

debian/requirements.txt : requirements.in
	pip-compile requirements.in > debian/requirements.txt


install-etc:
	install -d $(DESTDIR)$(ETCDIR)
	for fn in `find scripts/etc -type d | sed 's+scripts/etc++g'`; do install -m 755 -d $(DESTDIR)$(ETCDIR)$$fn; done
	for fn in `find scripts/etc -type f | sed 's+scripts/etc++g'`; do install -m 644 scripts/etc$$fn $(DESTDIR)$(ETCDIR)$$fn; done

install-var:
	install -d $(DESTDIR)$(VARDIR)
	for fn in `find scripts/var -type d | sed 's+scripts/var++g'`; do install -m 755 -d $(DESTDIR)$(VARDIR)$$fn; done
	for fn in `find scripts/var -type f | sed 's+scripts/var++g'`; do install -m 644 scripts/var$$fn $(DESTDIR)$(VARDIR)$$fn; done

install-usr:
	install -d $(DESTDIR)$(USRDIR)
	for fn in `find scripts/usr -type d | sed 's+scripts/usr++g'`; do install -m 755 -d $(DESTDIR)$(USRDIR)$$fn; done
	for fn in `find scripts/usr -type f | sed 's+scripts/usr++g'`; do install -m 644 scripts/usr$$fn $(DESTDIR)$(USRDIR)$$fn; done

install: install-etc install-var install-usr

