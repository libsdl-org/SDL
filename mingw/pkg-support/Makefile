#
# Makefile for installing the mingw32 version of the SDL library

CROSS_PATH := /usr/local
ARCHITECTURES := i686-w64-mingw32 x86_64-w64-mingw32

all install:
	@echo "Type \"make native\" to install 32-bit to /usr"
	@echo "Type \"make cross\" to install 32-bit and 64-bit to $(CROSS_PATH)"

native:
	make install-package arch=i686-w64-mingw32 prefix=/usr

cross:
	for arch in $(ARCHITECTURES); do \
	    make install-package arch=$$arch prefix=$(CROSS_PATH)/$$arch; \
	done

install-package:
	@if test -d $(arch) && test -d $(prefix); then \
	    (cd $(arch) && cp -rv bin include lib share $(prefix)/); \
	    sed "s|^prefix=.*|prefix=$(prefix)|" <$(arch)/bin/sdl2-config >$(prefix)/bin/sdl2-config; \
	    chmod 755 $(prefix)/bin/sdl2-config; \
	    sed "s|^libdir=.*|libdir=\'$(prefix)/lib\'|" <$(arch)/lib/libSDL2.la >$(prefix)/lib/libSDL2.la; \
	    sed "s|^libdir=.*|libdir=\'$(prefix)/lib\'|" <$(arch)/lib/libSDL2main.la >$(prefix)/lib/libSDL2main.la; \
	    sed "s|^prefix=.*|prefix=$(prefix)|" <$(arch)/lib/pkgconfig/sdl2.pc >$(prefix)/lib/pkgconfig/sdl2.pc; \
	else \
	    echo "*** ERROR: $(arch) or $(prefix) does not exist!"; \
	    exit 1; \
	fi
