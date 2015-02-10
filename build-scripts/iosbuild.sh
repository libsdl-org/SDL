#!/bin/sh
#
# Build a fat binary for iOS
# Based on fatbuild.sh and code from the Ignifuga Game Engine

# Number of CPUs (for make -j)
NCPU=`sysctl -n hw.ncpu`
if test x$NJOB = x; then
    NJOB=$NCPU
fi

# SDK path
XCODE_PATH=`xcode-select --print-path`
if [ -z "$XCODE_PATH" ]; then
    echo "Could not find XCode location (use xcode-select -switch to set the correct path)"
    exit 1
fi

prepare_environment() {
    ARCH=$1
    
    if test x$SDK_VERSION = x; then
      export SDK_VERSION=`xcodebuild -showsdks | grep iphoneos | sed "s|.*iphoneos||"`
      if [ -z "$XCODE_PATH" ]; then
          echo "Could not find a valid iOS SDK"
          exit 1
      fi  
    fi
    
    case $ARCH in
        armv6)
            DEV_PATH="$XCODE_PATH/Platforms/iPhoneOS.platform/Developer"
            SDK_PATH="$DEV_PATH/SDKs/iPhoneOS$SDK_VERSION.sdk"
            ;;
        armv7)
            DEV_PATH="$XCODE_PATH/Platforms/iPhoneOS.platform/Developer"
            SDK_PATH="$DEV_PATH/SDKs/iPhoneOS$SDK_VERSION.sdk"
            ;;
        i386)
            DEV_PATH="$XCODE_PATH/Platforms/iPhoneSimulator.platform/Developer"
            SDK_PATH="$DEV_PATH/SDKs/iPhoneSimulator$SDK_VERSION.sdk"
            ;;
        *)
            echo "Unknown Architecture $ARCH"
            exit 1
            ;;
    esac

    if [ ! -d "$SDK_PATH" ]; then
        echo "Could not find iOS SDK at $SDK_PATH"
        exit 1
    fi

    if test x$MIN_OS_VERSION = x; then
        export MIN_OS_VERSION="3.0"
    fi
    
    # Environment flags
    CFLAGS="-g -O2 -pipe -no-cpp-precomp -isysroot $SDK_PATH \
            -miphoneos-version-min=$MIN_OS_VERSION -I$SDK_PATH/usr/include/"
    LDFLAGS="-L$SDK_PATH/usr/lib/ -isysroot $SDK_PATH \
             -miphoneos-version-min=$MIN_OS_VERSION -static-libgcc"
    export CXXFLAGS="$CFLAGS"
    export CXXCPP="$DEV_PATH/usr/bin/llvm-cpp-4.2"
    export CPP="$CXXCPP"
    export CXX="$DEV_PATH/usr/bin/llvm-g++-4.2"
    export CC="$DEV_PATH/usr/bin/llvm-gcc-4.2"
    export LD="$DEV_PATH/usr/bin/ld"
    export AR="$DEV_PATH/usr/bin/ar"
    export AS="$DEV_PATH/usr/bin/ls"
    export NM="$DEV_PATH/usr/bin/nm"
    export RANLIB="$DEV_PATH/usr/bin/ranlib"
    export STRIP="$DEV_PATH/usr/bin/strip"
    
    # We dynamically load X11, so using the system X11 headers is fine.
    CONFIG_FLAGS="--disable-shared --enable-static"
    
    case $ARCH in
        armv6)
            export CONFIG_FLAGS="$CONFIG_FLAGS --host=armv6-apple-darwin"
            export CFLAGS="$CFLAGS -arch armv6"
            export LDFLAGS="$LDFLAGS -arch armv6"
            ;;
        armv7)
            export CONFIG_FLAGS="$CONFIG_FLAGS --host=armv7-apple-darwin"
            export CFLAGS="$CFLAGS -arch armv7"
            export LDFLAGS="$LDFLAGS -arch armv7"
            ;;
        i386)
            export CONFIG_FLAGS="$CONFIG_FLAGS --host=i386-apple-darwin"
            export CFLAGS="$CFLAGS -arch i386"
            export LDFLAGS="$LDFLAGS -arch i386"
            ;;
        *)
            echo "Unknown Architecture $ARCH"
            exit 1
            ;;
    esac
}

prepare_environment "armv6"
echo "Building with iOS SDK v$SDK_VERSION for iOS >= $MIN_OS_VERSION"

#
# Find the configure script
#
srcdir=`dirname $0`/..
srcdir=`cd $srcdir && pwd`
auxdir=$srcdir/build-scripts
cd $srcdir

#
# Figure out which phase to build:
# all,
# configure, configure-armv6, configure-armv7, configure-i386
# make, make-armv6, make-armv7, make-i386, merge
# clean
if test x"$1" = x; then
    phase=all
else
    phase="$1"
fi
case $phase in
    all)
        configure_armv6="yes"
        configure_armv7="yes"
        configure_i386="yes"
        make_armv6="yes"
        make_armv7="yes"
        make_i386="yes"
        merge="yes"
        ;;
    configure)
        configure_armv6="yes"
        configure_armv7="yes"
        configure_i386="yes"
        ;;
    configure-armv6)
        configure_armv6="yes"
        ;;
    configure-armv7)
        configure_armv7="yes"
        ;;
    configure-i386)
        configure_i386="yes"
        ;;
    make)
        make_armv6="yes"
        make_armv7="yes"
        make_i386="yes"
        merge="yes"
        ;;
    make-armv6)
        make_armv6="yes"
        ;;
    make-armv7)
        make_armv7="yes"
        ;;
    make-i386)
        make_i386="yes"
        ;;
    merge)
        merge="yes"
        ;;
    clean)
        clean_armv6="yes"
        clean_armv7="yes"
        clean_i386="yes"
        ;;
    clean-armv6)
        clean_armv6="yes"
        ;;
    clean-armv7)
        clean_armv7="yes"
        ;;
    clean-i386)
        clean_i386="yes"
        ;;
    *)
        echo "Usage: $0 [all|configure[-armv6|-armv7|-i386]|make[-armv6|-armv7|-i386]|merge|clean[-armv6|-armv7|-i386]]"
        exit 1
        ;;
esac

#
# Create the build directories
#
for dir in build build/armv6 build/armv7 build/i386; do
    if test -d $dir; then
        :
    else
        mkdir $dir || exit 1
    fi
done

#
# Build the armv6 binary
#
prepare_environment "armv6"
if test x$configure_armv6 = xyes; then
    (cd build/armv6 && \
     sh ../../configure $CONFIG_FLAGS CC="$CC" CXX="$CXX" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS") || exit 2
     # configure is not yet fully ready for iOS, some manual patching is required
     cp include/* build/armv6/include
     cp include/SDL_config_iphoneos.h build/armv6/include/SDL_config.h || exit 2
     sed -i "" -e "s|^EXTRA_CFLAGS.*|EXTRA_CFLAGS=-I./include|g" build/armv6/Makefile || exit 2
     sed -i "" -e "s|^EXTRA_LDFLAGS.*|EXTRA_LDFLAGS=-lm|g" build/armv6/Makefile || exit 2
fi
if test x$make_armv6 = xyes; then
    (cd build/armv6 && make -j$NJOB) || exit 3
fi
#
# Build the armv7 binary
#
prepare_environment "armv7"
if test x$configure_armv7 = xyes; then
    (cd build/armv7 && \
     sh ../../configure $CONFIG_FLAGS CC="$CC" CXX="$CXX" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS") || exit 2
     # configure is not yet fully ready for iOS, some manual patching is required
     cp include/* build/armv7/include
     cp include/SDL_config_iphoneos.h build/armv7/include/SDL_config.h || exit 2
     sed -i "" -e "s|^EXTRA_CFLAGS.*|EXTRA_CFLAGS=-I./include|g" build/armv7/Makefile || exit 2
     sed -i "" -e "s|^EXTRA_LDFLAGS.*|EXTRA_LDFLAGS=-lm|g" build/armv7/Makefile || exit 2
fi
if test x$make_armv7 = xyes; then
    (cd build/armv7 && make -j$NJOB) || exit 3
fi
#
# Build the i386 binary
#
prepare_environment "i386"
if test x$configure_i386 = xyes; then
    (cd build/i386 && \
     sh ../../configure $CONFIG_FLAGS CC="$CC" CXX="$CXX" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS") || exit 2
     # configure is not yet fully ready for iOS, some manual patching is required
     cp include/* build/i386/include
     cp include/SDL_config_iphoneos.h build/i386/include/SDL_config.h || exit 2
     sed -i "" -e "s|^EXTRA_CFLAGS.*|EXTRA_CFLAGS=-I./include|g" build/i386/Makefile || exit 2
     sed -i "" -e "s|^EXTRA_LDFLAGS.*|EXTRA_LDFLAGS=-lm|g" build/i386/Makefile || exit 2
fi
if test x$make_i386 = xyes; then
    (cd build/i386 && make -j$NJOB) || exit 3
fi

#
# Combine into fat binary
#
if test x$merge = xyes; then
    output=ios/lib
    sh $auxdir/mkinstalldirs build/$output
    cd build
    target=`find . -mindepth 4 -maxdepth 4 -type f -name '*.dylib' | head -1 | sed 's|.*/||'`
    (lipo -create -o $output/libSDL2.a armv6/build/.libs/libSDL2.a armv7/build/.libs/libSDL2.a i386/build/.libs/libSDL2.a &&
     lipo -create -o $output/libSDL2main.a armv6/build/libSDL2main.a armv7/build/libSDL2main.a i386/build/libSDL2main.a &&
     cp -r armv6/include ios
     echo "Build complete!" &&
     echo "Files can be found under the build/ios directory.") || exit 4
    cd ..
fi

#
# Clean up
#
do_clean()
{
    echo $*
    $* || exit 6
}
if test x$clean_armv6 = xyes; then
    do_clean rm -r build/armv6
fi
if test x$clean_armv7 = xyes; then
    do_clean rm -r build/armv7
fi
if test x$clean_i386 = xyes; then
    do_clean rm -r build/i386
fi
