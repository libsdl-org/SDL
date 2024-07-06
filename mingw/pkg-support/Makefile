#
# Makefile for installing the mingw32 version of the SDL library

ARCHITECTURES := i686-w64-mingw32 x86_64-w64-mingw32

all install:
	@echo "Type \"make native\" to install 32-bit to /usr"
	@echo "Type \"make cross\" to install 32-bit and 64-bit to $(CROSS_PATH)"

native:
	make install-package arch=i686-w64-mingw32 prefix=/usr

cross:
	for arch in $(ARCHITECTURES); do \
		if [ "$$arch" = "i686-w64-mingw32" ]; then \
			CROSS_PATH="/mingw32"; \
		elif [ "$$arch" = "x86_64-w64-mingw32" ]; then \
			CROSS_PATH="/mingw64"; \
		fi; \
		make install-package arch=$$arch prefix=$$CROSS_PATH/$$arch; \
	done

install-package:
	@if test -d $(arch) && test -d $(prefix); then \
	    (cd $(arch) && cp -rv bin include lib share $(prefix)/); \
	    sed "s|^prefix=.*|prefix=$(prefix)|" <$(arch)/lib/pkgconfig/sdl3.pc >$(prefix)/lib/pkgconfig/sdl3.pc; \
	else \
	    echo "*** ERROR: $(arch) or $(prefix) does not exist!"; \
	    exit 1; \
	fi
