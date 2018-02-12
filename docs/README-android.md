Android
================================================================================

Matt Styles wrote a tutorial on building SDL for Android with Visual Studio:
http://trederia.blogspot.de/2017/03/building-sdl2-for-android-with-visual.html

The rest of this README covers the Android gradle style build process.

If you are using the older ant build process, it is no longer officially
supported, but you can use the "android-project-ant" directory as a template.


================================================================================
 Requirements
================================================================================

Android SDK (version 19 or later)
https://developer.android.com/sdk/index.html

Android NDK r10e or later
https://developer.android.com/tools/sdk/ndk/index.html

Minimum API level supported by SDL: 14 (Android 4.0.1)


================================================================================
 How the port works
================================================================================

- Android applications are Java-based, optionally with parts written in C
- As SDL apps are C-based, we use a small Java shim that uses JNI to talk to 
  the SDL library
- This means that your application C code must be placed inside an Android 
  Java project, along with some C support code that communicates with Java
- This eventually produces a standard Android .apk package

The Android Java code implements an "Activity" and can be found in:
android-project/app/src/main/java/org/libsdl/app/SDLActivity.java

The Java code loads your game code, the SDL shared library, and
dispatches to native functions implemented in the SDL library:
src/core/android/SDL_android.c


================================================================================
 Building an app
================================================================================

For simple projects you can use the script located at build-scripts/androidbuild.sh

There's two ways of using it:

    androidbuild.sh com.yourcompany.yourapp < sources.list
    androidbuild.sh com.yourcompany.yourapp source1.c source2.c ...sourceN.c

sources.list should be a text file with a source file name in each line
Filenames should be specified relative to the current directory, for example if
you are in the build-scripts directory and want to create the testgles.c test, you'll
run:

    ./androidbuild.sh org.libsdl.testgles ../test/testgles.c

One limitation of this script is that all sources provided will be aggregated into
a single directory, thus all your source files should have a unique name.

Once the project is complete the script will tell you where the debug APK is located.
If you want to create a signed release APK, you can use the project created by this
utility to generate it.

Finally, a word of caution: re running androidbuild.sh wipes any changes you may have
done in the build directory for the app!


For more complex projects, follow these instructions:
    
1. Copy the android-project directory wherever you want to keep your projects
   and rename it to the name of your project.
2. Move or symlink this SDL directory into the "<project>/app/jni" directory
3. Edit "<project>/app/jni/src/Android.mk" to include your source files
4. Run 'ndk-build' (a script provided by the NDK). This compiles the C source

If you want to use Android Studio (recommended), skip to the Android Studio section below.

5. Run './gradlew installDebug' in the project directory. This compiles the .java, creates an .apk with the native code embedded, and installs it on any connected Android device

Here's an explanation of the files in the Android project, so you can customize them:

    android-project/app
        build.gradle            - build info including the application version and SDK
        src/main/AndroidManifest.xml	- package manifest. Among others, it contains the class name
        			  of the main Activity and the package name of the application.
        jni/			- directory holding native code
        jni/Application.mk	- Application JNI settings, including target platform and STL library
        jni/Android.mk		- Android makefile that can call recursively the Android.mk files in all subdirectories
        jni/SDL/		- (symlink to) directory holding the SDL library files
        jni/SDL/Android.mk	- Android makefile for creating the SDL shared library
        jni/src/		- directory holding your C/C++ source
        jni/src/Android.mk	- Android makefile that you should customize to include your source code and any library references
        src/main/assets/	- directory holding asset files for your application
        src/main/res/		- directory holding resources for your application
        src/main/res/mipmap-*	- directories holding icons for different phone hardware
        src/main/res/values/strings.xml	- strings used in your application, including the application name
        src/main/java/org/libsdl/app/SDLActivity.java - the Java class handling the initialization and binding to SDL. Be very careful changing this, as the SDL library relies on this implementation. You should instead subclass this for your application.


================================================================================
 Customizing your application name
================================================================================

To customize your application name, edit AndroidManifest.xml and replace
"org.libsdl.app" with an identifier for your product package.

Then create a Java class extending SDLActivity and place it in a directory
under src matching your package, e.g.

    src/com/gamemaker/game/MyGame.java

Here's an example of a minimal class file:

    --- MyGame.java --------------------------
    package com.gamemaker.game;
    
    import org.libsdl.app.SDLActivity; 
    
    /**
     * A sample wrapper class that just calls SDLActivity 
     */ 
    
    public class MyGame extends SDLActivity { }
    
    ------------------------------------------

Then replace "SDLActivity" in AndroidManifest.xml with the name of your
class, .e.g. "MyGame"


================================================================================
 Customizing your application icon
================================================================================

Conceptually changing your icon is just replacing the "ic_launcher.png" files in
the drawable directories under the res directory. There are several directories
for different screen sizes.


================================================================================
 Loading assets
================================================================================

Any files you put in the "app/src/main/assets" directory of your project
directory will get bundled into the application package and you can load
them using the standard functions in SDL_rwops.h.

There are also a few Android specific functions that allow you to get other
useful paths for saving and loading data:
* SDL_AndroidGetInternalStoragePath()
* SDL_AndroidGetExternalStorageState()
* SDL_AndroidGetExternalStoragePath()

See SDL_system.h for more details on these functions.

The asset packaging system will, by default, compress certain file extensions.
SDL includes two asset file access mechanisms, the preferred one is the so
called "File Descriptor" method, which is faster and doesn't involve the Dalvik
GC, but given this method does not work on compressed assets, there is also the
"Input Stream" method, which is automatically used as a fall back by SDL. You
may want to keep this fact in mind when building your APK, specially when large
files are involved.
For more information on which extensions get compressed by default and how to
disable this behaviour, see for example:
    
http://ponystyle.com/blog/2010/03/26/dealing-with-asset-compression-in-android-apps/


================================================================================
 Pause / Resume behaviour
================================================================================

If SDL is compiled with SDL_ANDROID_BLOCK_ON_PAUSE defined (the default),
the event loop will block itself when the app is paused (ie, when the user
returns to the main Android dashboard). Blocking is better in terms of battery
use, and it allows your app to spring back to life instantaneously after resume
(versus polling for a resume message).

Upon resume, SDL will attempt to restore the GL context automatically.
In modern devices (Android 3.0 and up) this will most likely succeed and your
app can continue to operate as it was.

However, there's a chance (on older hardware, or on systems under heavy load),
where the GL context can not be restored. In that case you have to listen for
a specific message, (which is not yet implemented!) and restore your textures
manually or quit the app (which is actually the kind of behaviour you'll see
under iOS, if the OS can not restore your GL context it will just kill your app)


================================================================================
 Threads and the Java VM
================================================================================

For a quick tour on how Linux native threads interoperate with the Java VM, take
a look here: https://developer.android.com/guide/practices/jni.html

If you want to use threads in your SDL app, it's strongly recommended that you
do so by creating them using SDL functions. This way, the required attach/detach
handling is managed by SDL automagically. If you have threads created by other
means and they make calls to SDL functions, make sure that you call
Android_JNI_SetupThread() before doing anything else otherwise SDL will attach
your thread automatically anyway (when you make an SDL call), but it'll never
detach it.


================================================================================
 Using STL
================================================================================

You can use STL in your project by creating an Application.mk file in the jni
folder and adding the following line:

    APP_STL := stlport_static

For more information check out CPLUSPLUS-SUPPORT.html in the NDK documentation.


================================================================================
 Additional documentation
================================================================================

The documentation in the NDK docs directory is very helpful in understanding the
build process and how to work with native code on the Android platform.

The best place to start is with docs/OVERVIEW.TXT


================================================================================
 Using Android Studio
================================================================================

You can open your project directory with Android Studio and run it normally.


================================================================================
 Using the emulator
================================================================================

There are some good tips and tricks for getting the most out of the
emulator here: https://developer.android.com/tools/devices/emulator.html

Especially useful is the info on setting up OpenGL ES 2.0 emulation.

Notice that this software emulator is incredibly slow and needs a lot of disk space.
Using a real device works better.


================================================================================
 Troubleshooting
================================================================================

You can see if adb can see any devices with the following command:

    adb devices

You can see the output of log messages on the default device with:

    adb logcat

You can push files to the device with:

    adb push local_file remote_path_and_file

You can push files to the SD Card at /sdcard, for example:

    adb push moose.dat /sdcard/moose.dat

You can see the files on the SD card with a shell command:

    adb shell ls /sdcard/

You can start a command shell on the default device with:

    adb shell

You can remove the library files of your project (and not the SDL lib files) with:

    ndk-build clean

You can do a build with the following command:

    ndk-build

You can see the complete command line that ndk-build is using by passing V=1 on the command line:

    ndk-build V=1

If your application crashes in native code, you can use addr2line to convert the
addresses in the stack trace to lines in your code.

For example, if your crash looks like this:

    I/DEBUG   (   31): signal 11 (SIGSEGV), code 2 (SEGV_ACCERR), fault addr 400085d0
    I/DEBUG   (   31):  r0 00000000  r1 00001000  r2 00000003  r3 400085d4
    I/DEBUG   (   31):  r4 400085d0  r5 40008000  r6 afd41504  r7 436c6a7c
    I/DEBUG   (   31):  r8 436c6b30  r9 435c6fb0  10 435c6f9c  fp 4168d82c
    I/DEBUG   (   31):  ip 8346aff0  sp 436c6a60  lr afd1c8ff  pc afd1c902  cpsr 60000030
    I/DEBUG   (   31):          #00  pc 0001c902  /system/lib/libc.so
    I/DEBUG   (   31):          #01  pc 0001ccf6  /system/lib/libc.so
    I/DEBUG   (   31):          #02  pc 000014bc  /data/data/org.libsdl.app/lib/libmain.so
    I/DEBUG   (   31):          #03  pc 00001506  /data/data/org.libsdl.app/lib/libmain.so

You can see that there's a crash in the C library being called from the main code.
I run addr2line with the debug version of my code:

    arm-eabi-addr2line -C -f -e obj/local/armeabi/libmain.so

and then paste in the number after "pc" in the call stack, from the line that I care about:
000014bc

I get output from addr2line showing that it's in the quit function, in testspriteminimal.c, on line 23.

You can add logging to your code to help show what's happening:

    #include <android/log.h>
    
    __android_log_print(ANDROID_LOG_INFO, "foo", "Something happened! x = %d", x);

If you need to build without optimization turned on, you can create a file called
"Application.mk" in the jni directory, with the following line in it:

    APP_OPTIM := debug


================================================================================
 Memory debugging
================================================================================

The best (and slowest) way to debug memory issues on Android is valgrind.
Valgrind has support for Android out of the box, just grab code using:

    svn co svn://svn.valgrind.org/valgrind/trunk valgrind

... and follow the instructions in the file README.android to build it.

One thing I needed to do on Mac OS X was change the path to the toolchain,
and add ranlib to the environment variables:
export RANLIB=$NDKROOT/toolchains/arm-linux-androideabi-4.4.3/prebuilt/darwin-x86/bin/arm-linux-androideabi-ranlib

Once valgrind is built, you can create a wrapper script to launch your
application with it, changing org.libsdl.app to your package identifier:

    --- start_valgrind_app -------------------
    #!/system/bin/sh
    export TMPDIR=/data/data/org.libsdl.app
    exec /data/local/Inst/bin/valgrind --log-file=/sdcard/valgrind.log --error-limit=no $*
    ------------------------------------------

Then push it to the device:

    adb push start_valgrind_app /data/local

and make it executable:

    adb shell chmod 755 /data/local/start_valgrind_app

and tell Android to use the script to launch your application:

    adb shell setprop wrap.org.libsdl.app "logwrapper /data/local/start_valgrind_app"

If the setprop command says "could not set property", it's likely that
your package name is too long and you should make it shorter by changing
AndroidManifest.xml and the path to your class file in android-project/src

You can then launch your application normally and waaaaaaaiiittt for it.
You can monitor the startup process with the logcat command above, and
when it's done (or even while it's running) you can grab the valgrind
output file:

    adb pull /sdcard/valgrind.log

When you're done instrumenting with valgrind, you can disable the wrapper:

    adb shell setprop wrap.org.libsdl.app ""


================================================================================
 Graphics debugging
================================================================================

If you are developing on a compatible Tegra-based tablet, NVidia provides
Tegra Graphics Debugger at their website. Because SDL2 dynamically loads EGL
and GLES libraries, you must follow their instructions for installing the
interposer library on a rooted device. The non-rooted instructions are not
compatible with applications that use SDL2 for video.

The Tegra Graphics Debugger is available from NVidia here:
https://developer.nvidia.com/tegra-graphics-debugger


================================================================================
 Why is API level 14 the minimum required?
================================================================================

The latest NDK toolchain doesn't support targeting earlier than API level 14.
As of this writing, according to https://developer.android.com/about/dashboards/index.html
about 99% of the Android devices accessing Google Play support API level 14 or
higher (October 2017).


================================================================================
 A note regarding the use of the "dirty rectangles" rendering technique
================================================================================

If your app uses a variation of the "dirty rectangles" rendering technique,
where you only update a portion of the screen on each frame, you may notice a
variety of visual glitches on Android, that are not present on other platforms.
This is caused by SDL's use of EGL as the support system to handle OpenGL ES/ES2
contexts, in particular the use of the eglSwapBuffers function. As stated in the
documentation for the function "The contents of ancillary buffers are always 
undefined after calling eglSwapBuffers".
Setting the EGL_SWAP_BEHAVIOR attribute of the surface to EGL_BUFFER_PRESERVED
is not possible for SDL as it requires EGL 1.4, available only on the API level
17+, so the only workaround available on this platform is to redraw the entire
screen each frame.

Reference: http://www.khronos.org/registry/egl/specs/EGLTechNote0001.html


================================================================================
 Known issues
================================================================================

- The number of buttons reported for each joystick is hardcoded to be 36, which
is the current maximum number of buttons Android can report.

