#
# Makefile for installing the mingw32 version of the SDL library

DESTDIR = /usr/local
ARCHITECTURES := i686-w64-mingw32 x86_64-w64-mingw32

default:
	@echo "Run \"make install-i686\" to install 32-bit"
	@echo "Run \"make install-x86_64\" to install 64-bit"
	@echo "Run \"make install-all\" to install both"
	@echo "Add DESTDIR=/custom/path to change the destination folder"

install:
	@if test -d $(ARCH) && test -d $(DESTDIR); then \
		(cd $(ARCH) && cp -rv bin include lib share $(DESTDIR)/); \
	else \
		echo "*** ERROR: $(ARCH) or $(DESTDIR) does not exist!"; \
		exit 1; \
	fi

install-i686:
	$(MAKE) install ARCH=i686-w64-mingw32

install-x86_64:
	$(MAKE) install ARCH=x86_64-w64-mingw32

install-all:
	@if test -d $(DESTDIR); then \
		mkdir -p $(DESTDIR)/cmake; \
		cp -rv cmake/* $(DESTDIR)/cmake; \
		for arch in $(ARCHITECTURES); do \
			$(MAKE) install ARCH=$$arch DESTDIR=$(DESTDIR)/$$arch; \
		done \
	else \
		echo "*** ERROR: $(DESTDIR) does not exist!"; \
		exit 1; \
	fi

.PHONY: default install install-i686 install-x86_64 install-all
