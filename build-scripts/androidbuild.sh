#!/bin/bash

SOURCES=()
MKSOURCES=""
CURDIR=`pwd -P`

# Fetch sources
if [[ $# -ge 2 ]]; then
    for src in ${@:2}
    do
        SOURCES+=($src)
        MKSOURCES="$MKSOURCES $(basename $src)"
    done
else
    if [ -n "$1" ]; then
        while read src
        do
            SOURCES+=($src)
            MKSOURCES="$MKSOURCES $(basename $src)"
        done
    fi
fi

if [ -z "$1" ] || [ -z "$SOURCES" ]; then
    echo "Usage: androidbuild.sh com.yourcompany.yourapp < sources.list"
    echo "Usage: androidbuild.sh com.yourcompany.yourapp source1.c source2.c ...sourceN.c"
    echo "To copy SDL source instead of symlinking: COPYSOURCE=1 androidbuild.sh ... "
    echo "You can pass additional arguments to ndk-build with the NDKARGS variable: NDKARGS=\"-s\" androidbuild.sh ..."
    exit 1
fi



SDLPATH="$( cd "$(dirname "$0")/.." ; pwd -P )"

if [ -z "$ANDROID_HOME" ];then
    echo "Please set the ANDROID_HOME directory to the path of the Android SDK"
    exit 1
fi

NDKBUILD=`which ndk-build`
if [ -z "$NDKBUILD" ];then
    echo "Could not find the ndk-build utility, install Android's NDK and add it to the path"
    exit 1
fi

ANDROID="$ANDROID_HOME/tools/android"
if [ ! -f "$ANDROID" ]; then
    ANDROID=`which android`
fi
if [ -z "$ANDROID" ];then
    echo "Could not find the android utility, install Android's SDK and add it to the path"
    exit 1
fi

NCPUS="1"
case "$OSTYPE" in
    darwin*)
        NCPU=`sysctl -n hw.ncpu`
        ;; 
    linux*)
        if [ -n `which nproc` ]; then
            NCPUS=`nproc`
        fi  
        ;;
  *);;
esac

APP="$1"
APPARR=(${APP//./ })
BUILDPATH="$SDLPATH/build/$APP"

# Start Building

rm -rf $BUILDPATH
mkdir -p $BUILDPATH

cp -r $SDLPATH/android-project/* $BUILDPATH

# Copy SDL sources
mkdir -p $BUILDPATH/app/jni/SDL
if [ -z "$COPYSOURCE" ]; then
    ln -s $SDLPATH/src $BUILDPATH/app/jni/SDL
    ln -s $SDLPATH/include $BUILDPATH/app/jni/SDL
else
    cp -r $SDLPATH/src $BUILDPATH/app/jni/SDL
    cp -r $SDLPATH/include $BUILDPATH/app/jni/SDL
fi

cp -r $SDLPATH/Android.mk $BUILDPATH/app/jni/SDL
sed -i -e "s|YourSourceHere.c|$MKSOURCES|g" $BUILDPATH/app/jni/src/Android.mk
sed -i -e "s|org\.libsdl\.app|$APP|g" $BUILDPATH/app/build.gradle
sed -i -e "s|org\.libsdl\.app|$APP|g" $BUILDPATH/app/src/main/AndroidManifest.xml

# Copy user sources
for src in "${SOURCES[@]}"
do
    cp $src $BUILDPATH/app/jni/src
done

# Create an inherited Activity
cd $BUILDPATH/app/src/main/java
for folder in "${APPARR[@]}"
do
    mkdir -p $folder
    cd $folder
done

ACTIVITY="${folder}Activity"
sed -i -e "s|\"SDLActivity\"|\"$ACTIVITY\"|g" $BUILDPATH/app/src/main/AndroidManifest.xml

# Fill in a default Activity
cat >"$ACTIVITY.java" <<__EOF__
package $APP;

import org.libsdl.app.SDLActivity;

public class $ACTIVITY extends SDLActivity
{
}
__EOF__

# Update project and build
cd $BUILDPATH
pushd $BUILDPATH/app/jni
$NDKBUILD -j $NCPUS $NDKARGS
popd

# Start gradle build
$BUILDPATH/gradlew build

cd $CURDIR

APK="$BUILDPATH/app/build/outputs/apk/app-debug.apk"

if [ -f "$APK" ]; then
    echo "Your APK is ready at $APK"
    echo "To install to your device: "
    echo "$ANDROID_HOME/platform-tools/adb install -r $APK"
    exit 0
fi

echo "There was an error building the APK"
exit 1
