#!/bin/bash

SDKDIR="/emsdk_portable"
ENVSCRIPT="$SDKDIR/emsdk_env.sh"
if [ ! -f "$ENVSCRIPT" ]; then
   echo "ERROR: This script expects the Emscripten SDK to be in '$SDKDIR'." 1>&2
   exit 1
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
set -e
set -x
cd "$SDLBASE"
rm -rf buildbot
mkdir buildbot
cd buildbot

echo "Configuring..."
emconfigure ../configure --host=asmjs-unknown-emscripten --disable-assembly --disable-threads --enable-cpuinfo=false CFLAGS="-O2 -Wno-warn-absolute-paths"

echo "Building..."
emmake $MAKE

set +x
echo "Done! The library is in $SDLBASE/buildbot/build/.libs/libSDL2.a ..."

exit 0

# end of emscripten-buildbot.sh ...

