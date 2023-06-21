#!/bin/bash
#
# Build the Android libraries without needing a project
# (AndroidManifest.xml, jni/{Application,Android}.mk, etc.)
#
# Usage: androidbuildlibs.sh [arg for ndk-build ...]"
#
# Useful NDK arguments:
#
#  NDK_DEBUG=1          - build debug version
#  NDK_LIBS_OUT=<dest>  - specify alternate destination for installable
#                         modules.
#


# Android.mk is in srcdir
srcdir=`dirname $0`/..
srcdir=`cd $srcdir && pwd`
cd $srcdir


#
# Create the build directories
#

build=build
buildandroid=$build/android
platform=android-16
abi="arm64-v8a" # "armeabi-v7a arm64-v8a x86 x86_64"
obj=
lib=
ndk_args=

# Allow an external caller to specify locations and platform.
while [ $# -gt 0 ]; do
    arg=$1
    if [ "${arg:0:8}" == "NDK_OUT=" ]; then
        obj=${arg#NDK_OUT=}
    elif [ "${arg:0:13}" == "NDK_LIBS_OUT=" ]; then
        lib=${arg#NDK_LIBS_OUT=}
    elif [ "${arg:0:13}" == "APP_PLATFORM=" ]; then
        platform=${arg#APP_PLATFORM=}
    elif [ "${arg:0:8}" == "APP_ABI=" ]; then
        abi=${arg#APP_ABI=}
    else
        ndk_args="$ndk_args $arg"
    fi
    shift
done

if [ -z $obj ]; then
    obj=$buildandroid/obj
fi
if [ -z $lib ]; then
    lib=$buildandroid/lib
fi

for dir in $build $buildandroid $obj $lib; do
    if test -d $dir; then
        :
    else
        mkdir $dir || exit 1
    fi
done


# APP_* variables set in the environment here will not be seen by the
# ndk-build makefile segments that use them, e.g., default-application.mk.
# For consistency, pass all values on the command line.
ndk-build \
    NDK_PROJECT_PATH=null \
    NDK_OUT=$obj \
    NDK_LIBS_OUT=$lib \
    APP_BUILD_SCRIPT=Android.mk \
    APP_ABI="$abi" \
    APP_PLATFORM="$platform" \
    APP_MODULES="SDL3" \
    $ndk_args
