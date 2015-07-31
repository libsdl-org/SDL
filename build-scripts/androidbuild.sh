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

NDKBUILD=`which ndk-build`
if [ -z "$NDKBUILD" ];then
    echo "Could not find the ndk-build utility, install Android's NDK and add it to the path"
    exit 1
fi

ANDROID=`which android`
if [ -z "$ANDROID" ];then
    echo "Could not find the android utility, install Android's SDK and add it to the path"
    exit 1
fi

ANT=`which ant`

if [ -z "$ANT" ];then
    echo "Could not find the ant utility, install Android's SDK and add it to the path"
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
mkdir -p $BUILDPATH/jni/SDL
if [ -z "$COPYSOURCE" ]; then
    ln -s $SDLPATH/src $BUILDPATH/jni/SDL
    ln -s $SDLPATH/include $BUILDPATH/jni/SDL
else
    cp -r $SDLPATH/src $BUILDPATH/jni/SDL
    cp -r $SDLPATH/include $BUILDPATH/jni/SDL
fi

cp -r $SDLPATH/Android.mk $BUILDPATH/jni/SDL
sed -i "s|YourSourceHere.c|$MKSOURCES|g" $BUILDPATH/jni/src/Android.mk
sed -i "s|org\.libsdl\.app|$APP|g" $BUILDPATH/AndroidManifest.xml

# Copy user sources
for src in "${SOURCES[@]}"
do
    cp $src $BUILDPATH/jni/src
done

# Create an inherited Activity
cd $BUILDPATH/src
for folder in "${APPARR[@]}"
do
    mkdir -p $folder
    cd $folder
done

ACTIVITY="${folder}Activity"
sed -i "s|SDLActivity|$ACTIVITY|g" $BUILDPATH/AndroidManifest.xml
sed -i "s|SDLActivity|$APP|g" $BUILDPATH/build.xml

# Fill in a default Activity
echo "package $APP;" >  "$ACTIVITY.java"
echo "import org.libsdl.app.SDLActivity;" >> "$ACTIVITY.java"
echo "public class $ACTIVITY extends SDLActivity {}" >> "$ACTIVITY.java"

# Update project and build
cd $BUILDPATH
$ANDROID update project --path $BUILDPATH
$NDKBUILD -j $NCPUS $NDKARGS
$ANT debug

cd $CURDIR

APK="$BUILDPATH/bin/$APP-debug.apk"

if [ -f "$APK" ]; then
    echo "Your APK is ready at $APK"
    echo "To install to your device: "
    echo "cd  $BUILDPATH"
    echo "ant debug install"
    exit 0
fi

echo "There was an error building the APK"
exit 1