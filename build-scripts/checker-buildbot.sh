#!/bin/bash

# This is a script used by some Buildbot buildslaves to push the project
#  through Clang's static analyzer and prepare the output to be uploaded
#  back to the buildmaster. You might find it useful too.

# To use: get CMake from http://cmake.org/ or "apt-get install cmake" or whatever.
# And download checker at http://clang-analyzer.llvm.org/ and unpack it in
#  /usr/local ... update CHECKERDIR as appropriate.

# this currently expects a mercurial working copy that it can modify a little.

CHECKERDIR="/usr/local/checker-276"

FINALDIR="$1"

if [ ! -d "$CHECKERDIR" ]; then
    echo "$CHECKERDIR not found." 1>&2
    exit 1
fi

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

set -x
set -e

cd `dirname "$0"`
cd ..

# Turn off the dynamic API. Makes the analysis output clearer.
echo '#pragma once' >src/dynapi/SDL_dynapi.h
echo '#define SDL_DYNAMIC_API 0' >>src/dynapi/SDL_dynapi.h

rm -rf checker-buildbot analysis
if [ ! -z "$FINALDIR" ]; then
    rm -rf "$FINALDIR"
fi

mkdir checker-buildbot
cd checker-buildbot
#cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER="$CHECKERDIR/libexec/ccc-analyzer" -DSDL_STATIC=OFF ..
CC="$CHECKERDIR/libexec/ccc-analyzer" CFLAGS="-O0" ../configure --disable-static
PATH="$CHECKERDIR:$PATH" scan-build -o analysis $MAKE
mv analysis/* ../analysis
rmdir analysis   # Make sure this is empty.
cd ..
chmod -R a+r analysis
chmod -R go-w analysis
find analysis -type d -exec chmod a+x {} \;
if [ -x /usr/bin/xattr ]; then find analysis -exec /usr/bin/xattr -d com.apple.quarantine {} \; 2>/dev/null ; fi

if [ ! -z "$FINALDIR" ]; then
    mv analysis "$FINALDIR"
else
    FINALDIR=analysis
fi

rm -rf checker-buildbot
hg revert src/dynapi/SDL_dynapi.h

echo "Done. Final output is in '$FINALDIR' ..."

# end of checker-buildbot.sh ...

