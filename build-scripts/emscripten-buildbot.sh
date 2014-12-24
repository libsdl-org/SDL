#!/bin/bash

SDKDIR="/emsdk_portable"
ENVSCRIPT="$SDKDIR/emsdk_env.sh"
if [ ! -f "$ENVSCRIPT" ]; then
   echo "ERROR: This script expects the Emscripten SDK to be in '$SDKDIR'." 1>&2
   exit 1
fi

TARBALL="$1"
if [ -z $1 ]; then
    TARBALL=sdl-emscripten.tar.xz
fi

cd `dirname "$0"`
cd ..
SDLBASE=`pwd`

if [ -z "$MAKE" ]; then
    OSTYPE=`uname -s`
    if [ "$OSTYPE" == "Linux" ]; then
        NCPU=`cat /proc/cpuinfo |grep vendor_id |wc -l`
        let NCPU=$NCPU+1
    elif [ "$OSTYPE" = "Darwin" ]; then
        NCPU=`sysctl -n hw.ncpu`
    elif [ "$OSTYPE" = "SunOS" ]; then
        NCPU=`/usr/sbin/psrinfo |wc -l |sed -e 's/^ *//g;s/ *$//g'`
    else
        NCPU=1
    fi

    if [ -z "$NCPU" ]; then
        NCPU=1
    elif [ "$NCPU" = "0" ]; then
        NCPU=1
    fi

    MAKE="make -j$NCPU"
fi

echo "\$MAKE is '$MAKE'"

echo "Setting up Emscripten SDK environment..."
source "$ENVSCRIPT"

echo "Setting up..."
set -x
cd "$SDLBASE"
rm -rf buildbot
rm -rf emscripten-sdl2-installed
mkdir buildbot
pushd buildbot

echo "Configuring..."
emconfigure ../configure --host=asmjs-unknown-emscripten --disable-assembly --disable-threads --enable-cpuinfo=false CFLAGS="-O2 -Wno-warn-absolute-paths -Wdeclaration-after-statement -Werror=declaration-after-statement" --prefix="$SDLBASE/emscripten-sdl2-installed"

echo "Building..."
emmake $MAKE

echo "Moving things around..."
emmake $MAKE install
mkdir -p ./usr
mv "$SDLBASE/emscripten-sdl2-installed" ./usr/local
popd
tar -cJvvf $TARBALL -C buildbot usr
rm -rf buildbot

exit 0

# end of emscripten-buildbot.sh ...

