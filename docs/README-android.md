Android
================================================================================

Matt Styles wrote a tutorial on building SDL for Android with Visual Studio:
http://trederia.blogspot.de/2017/03/building-sdl2-for-android-with-visual.html

The rest of this README covers the Android gradle style build process.


Requirements
================================================================================

Android SDK (version 35 or later)
https://developer.android.com/sdk/index.html

Android NDK r15c or later
https://developer.android.com/tools/sdk/ndk/index.html

Minimum API level supported by SDL: 21 (Android 5.0)


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


Building a simple app
================================================================================

For simple projects you can use the script located at build-scripts/create-android-project.py

There's two ways of using it:

    ./create-android-project.py com.yourcompany.yourapp < sources.list
    ./create-android-project.py com.yourcompany.yourapp source1.c source2.c ...sourceN.c

sources.list should be a text file with a source file name in each line
Filenames should be specified relative to the current directory, for example if
you are in the build-scripts directory and want to create the testgles.c test, you'll
run:

    ./create-android-project.py org.libsdl.testgles ../test/testgles.c

One limitation of this script is that all sources provided will be aggregated into
a single directory, thus all your source files should have a unique name.

Once the project is complete the script will tell you how to build the project.
If you want to create a signed release APK, you can use the project created by this
utility to generate it.

Running the script with `--help` will list all available options, and their purposes.

Finally, a word of caution: re running create-android-project.py wipes any changes you may have
done in the build directory for the app!


Building a more complex app
================================================================================

For more complex projects, follow these instructions:

1. Get the source code for SDL and copy the 'android-project' directory located at SDL/android-project to a suitable location in your project.

   The 'android-project' directory can basically be seen as a sort of starting point for the android-port of your project. It contains the glue code between the Android Java 'frontend' and the SDL code 'backend'. It also contains some standard behaviour, like how events should be handled, which you will be able to change.

2. If you are _not_ already building SDL as a part of your project (e.g. via CMake add_subdirectory() or FetchContent) move or [symlink](https://en.wikipedia.org/wiki/Symbolic_link) the SDL directory into the 'android-project/app/jni' directory. Alternatively you can [use the SDL3 Android Archive (.aar)](#using-the-sdl3-android-archive-aar), see bellow for more details.

    This is needed as SDL has to be compiled by the Android compiler.

3. Edit 'android-project/app/build.gradle' to include any assets that your app needs by adding 'assets.srcDirs' in 'sourceSets.main'.

    For example: `assets.srcDirs = ['../../assets', '../../shaders']`

If using CMake:

4. Edit 'android-project/app/build.gradle' to set 'buildWithCMake' to true and set 'externalNativeBuild' cmake path to your top level CMakeLists.txt.

    For example: `path '../../CMakeLists.txt'`

5. Change the target containing your main function to be built as a shared library called "main" when compiling for Android. (e.g. add_executable(MyGame main.c) should become add_library(main SHARED main.c) on Android)

If using Android Makefiles:

4. Edit 'android-project/app/jni/src/Android.mk' to include your source files. They should be separated by spaces after the 'LOCAL_SRC_FILES := ' declaration.

To build your app, run `./gradlew installDebug` or `./gradlew installRelease` in the project directory. It will build and install your .apk on any connected Android device. If you want to use Android Studio, simply open your 'android-project' directory and start building.

Additionally the [SDL_helloworld](https://github.com/libsdl-org/SDL_helloworld) project contains a small example program with a functional Android port that you can use as a reference.

Here's an explanation of the files in the Android project, so you can customize them:

    android-project/app
        build.gradle            - build info including the application version and SDK
        src/main/AndroidManifest.xml	- package manifest. Among others, it contains the class name of the main Activity and the package name of the application.
        jni/			- directory holding native code
        jni/Application.mk	- Application JNI settings, including target platform and STL library
        jni/Android.mk		- Android makefile that can call recursively the Android.mk files in all subdirectories
        jni/CMakeLists.txt	- Top-level CMake project that adds SDL as a subproject
        jni/SDL/		- (symlink to) directory holding the SDL library files
        jni/SDL/Android.mk	- Android makefile for creating the SDL shared library
        jni/src/		- directory holding your C/C++ source
        jni/src/Android.mk	- Android makefile that you should customize to include your source code and any library references
        jni/src/CMakeLists.txt	- CMake file that you may customize to include your source code and any library references
        src/main/assets/	- directory holding asset files for your application
        src/main/res/		- directory holding resources for your application
        src/main/res/mipmap-*	- directories holding icons for different phone hardware
        src/main/res/values/strings.xml	- strings used in your application, including the application name
        src/main/java/org/libsdl/app/SDLActivity.java - the Java class handling the initialization and binding to SDL. Be very careful changing this, as the SDL library relies on this implementation. You should instead subclass this for your application.


Using the SDL3 Android Archive (.aar)
================================================================================

The Android archive allows use of SDL3 in your Android project, without needing to copy any SDL C or JAVA source into your project.
For integration with CMake/ndk-build, it uses [prefab](https://google.github.io/prefab/).

Copy the archive to a `app/libs` directory in your project and add the following to `app/gradle.build`:
```
android {
    /* ... */
    buildFeatures {
        prefab true
    }
}
dependencies {
    implementation files('libs/SDL3-X.Y.Z.aar') /* Replace with the filename of the actual SDL3-x.y.z.aar file you downloaded */
    /* ... */
}
```

If you use CMake, add the following to your CMakeLists.txt:
```
find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(yourgame PRIVATE SDL3::SDL3)
```

If you use ndk-build, add the following before `include $(BUILD_SHARED_LIBRARY)` to your `Android.mk`:
```
LOCAL_SHARED_LIBRARIES := SDL3 SDL3-Headers
```
And add the following at the bottom:
```
# https://google.github.io/prefab/build-systems.html
# Add the prefab modules to the import path.
$(call import-add-path,/out)
# Import @PROJECT_NAME@ so we can depend on it.
$(call import-module,prefab/@PROJECT_NAME@)
```

The `build-scripts/create-android-project.py` script can create a project using Android aar-chives from scratch:
```
build-scripts/create-android-project.py --variant aar com.yourcompany.yourapp < sources.list
```

Customizing your application name
================================================================================

To customize your application name, edit AndroidManifest.xml and build.gradle to replace
"org.libsdl.app" with an identifier for your product package.

Then create a Java class extending SDLActivity and place it in a directory
under src matching your package, e.g.

    app/src/main/java/com/gamemaker/game/MyGame.java

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


Customizing your application icon
================================================================================

Conceptually changing your icon is just replacing the "ic_launcher.png" files in
the drawable directories under the res directory. There are several directories
for different screen sizes.


Loading assets
================================================================================

Any files you put in the "app/src/main/assets" directory of your project
directory will get bundled into the application package and you can load
them using the standard functions in SDL_iostream.h.

There are also a few Android specific functions that allow you to get other
useful paths for saving and loading data:
* SDL_GetAndroidInternalStoragePath()
* SDL_GetAndroidExternalStorageState()
* SDL_GetAndroidExternalStoragePath()
* SDL_GetAndroidCachePath()

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


Activity lifecycle
================================================================================

On Android the application goes through a fixed life cycle and you will get
notifications of state changes via application events. When these events
are delivered you must handle them in an event callback because the OS may
not give you any processing time after the events are delivered.

e.g.

    bool HandleAppEvents(void *userdata, SDL_Event *event)
    {
        switch (event->type)
        {
        case SDL_EVENT_TERMINATING:
            /* Terminate the app.
               Shut everything down before returning from this function.
            */
            return false;
        case SDL_EVENT_LOW_MEMORY:
            /* You will get this when your app is paused and iOS wants more memory.
               Release as much memory as possible.
            */
            return false;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            /* Prepare your app to go into the background.  Stop loops, etc.
               This gets called when the user hits the home button, or gets a call.

               You should not make any OpenGL graphics calls or use the rendering API,
               in addition, you should set the render target to NULL, if you're using
               it, e.g. call SDL_SetRenderTarget(renderer, NULL).
            */
            return false;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            /* Your app is NOT active at this point. */
            return false;
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
            /* This call happens when your app is coming back to the foreground.
               Restore all your state here.
            */
            return false;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            /* Restart your loops here.
               Your app is interactive and getting CPU again.

               You have access to the OpenGL context or rendering API at this point.
               However, there's a chance (on older hardware, or on systems under heavy load),
               where the graphics context can not be restored. You should listen for the
               event SDL_EVENT_RENDER_DEVICE_RESET and recreate your OpenGL context and
               restore your textures when you get it, or quit the app.
            */
            return false;
        default:
            /* No special processing, add it to the event queue */
            return true;
        }
    }

    int main(int argc, char *argv[])
    {
        SDL_SetEventFilter(HandleAppEvents, NULL);

        ... run your main loop

        return 0;
    }


Note that if you are using main callbacks instead of a standard C main() function,
your SDL_AppEvent() callback will run as these events arrive and you do not need to
use SDL_SetEventFilter.

If SDL_HINT_ANDROID_BLOCK_ON_PAUSE hint is set (the default),
the event loop will block itself when the app is paused (ie, when the user
returns to the main Android dashboard). Blocking is better in terms of battery
use, and it allows your app to spring back to life instantaneously after resume
(versus polling for a resume message).

You can control activity re-creation (eg. onCreate()) behaviour. This allows you
to choose whether to keep or re-initialize java and native static datas, see
SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY in SDL_hints.h.


Insets and Safe Areas
================================================================================

As of Android 15, SDL windows cover the entire screen, extending under notches
and system bars. The OS expects you to take those into account when displaying
content and SDL provides the function SDL_GetWindowSafeArea() so you know what
area is available for interaction. Outside of the safe area can be potentially
covered by system bars or used by OS gestures.


Mouse / Touch events
================================================================================

In some case, SDL generates synthetic mouse (resp. touch) events for touch
(resp. mouse) devices.
To enable/disable this behavior, see SDL_hints.h:
- SDL_HINT_TOUCH_MOUSE_EVENTS
- SDL_HINT_MOUSE_TOUCH_EVENTS


Misc
================================================================================

For some device, it appears to works better setting explicitly GL attributes
before creating a window:
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);


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


If you ever want to use JNI in a native thread (created by "SDL_CreateThread()"),
it won't be able to find your java class and method because of the java class loader
which is different for native threads, than for java threads (eg your "main()").

the work-around is to find class/method, in you "main()" thread, and to use them
in your native thread.

see:
https://developer.android.com/training/articles/perf-jni#faq:-why-didnt-findclass-find-my-class


Using STL
================================================================================

You can use STL in your project by creating an Application.mk file in the jni
folder and adding the following line:

    APP_STL := c++_shared

For more information go here:
	https://developer.android.com/ndk/guides/cpp-support


Using the emulator
================================================================================

There are some good tips and tricks for getting the most out of the
emulator here: https://developer.android.com/tools/devices/emulator.html

Especially useful is the info on setting up OpenGL ES 2.0 emulation.

Notice that this software emulator is incredibly slow and needs a lot of disk space.
Using a real device works better.


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

If your application crashes in native code, you can use ndk-stack to get a symbolic stack trace:
	https://developer.android.com/ndk/guides/ndk-stack

If you want to go through the process manually, you can use addr2line to convert the
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


Memory debugging
================================================================================

The best (and slowest) way to debug memory issues on Android is valgrind.
Valgrind has support for Android out of the box, just grab code using:

    git clone https://sourceware.org/git/valgrind.git

... and follow the instructions in the file `README.android` to build it.

One thing I needed to do on macOS was change the path to the toolchain,
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


Graphics debugging
================================================================================

If you are developing on a compatible Tegra-based tablet, NVidia provides
Tegra Graphics Debugger at their website. Because SDL3 dynamically loads EGL
and GLES libraries, you must follow their instructions for installing the
interposer library on a rooted device. The non-rooted instructions are not
compatible with applications that use SDL3 for video.

The Tegra Graphics Debugger is available from NVidia here:
https://developer.nvidia.com/tegra-graphics-debugger


A note regarding the use of the "dirty rectangles" rendering technique
================================================================================

If your app uses a variation of the "dirty rectangles" rendering technique,
where you only update a portion of the screen on each frame, you may notice a
variety of visual glitches on Android, that are not present on other platforms.
This is caused by SDL's use of EGL as the support system to handle OpenGL ES/ES2
contexts, in particular the use of the eglSwapBuffers function. As stated in the
documentation for the function "The contents of ancillary buffers are always
undefined after calling eglSwapBuffers".


Ending your application
================================================================================

Two legitimate ways:

- return from your main() function. Java side will automatically terminate the
Activity by calling Activity.finish().

- Android OS can decide to terminate your application by calling onDestroy()
(see Activity life cycle). Your application will receive an SDL_EVENT_QUIT you
can handle to save things and quit.

Don't call exit() as it stops the activity badly.

NB: "Back button" can be handled as a SDL_EVENT_KEY_DOWN/UP events, with Keycode
SDLK_AC_BACK, for any purpose.


Known issues
================================================================================

- The number of buttons reported for each joystick is hardcoded to be 36, which
is the current maximum number of buttons Android can report.


Building the SDL tests
================================================================================

SDL's CMake build system can create APK's for the tests.
It can build all tests with a single command without a dependency on gradle or Android Studio.
The APK's are signed with a debug certificate.
The only caveat is that the APK's support a single architecture.

### Requirements
- SDL source tree
- CMake
- ninja or make
- Android Platform SDK
- Android NDK
- Android Build tools
- Java JDK (version should be compatible with Android)
- keytool (usually provided with the Java JDK), used for generating a debug certificate
- zip

### CMake configuration

When configuring the CMake project, you need to use the Android NDK CMake toolchain, and pass the Android home path through `SDL_ANDROID_HOME`.
```
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path/to/android.toolchain.cmake> -DANDROID_ABI=<android-abi> -DSDL_ANDROID_HOME=<path-to-android-sdk-home> -DANDROID_PLATFORM=23 -DSDL_TESTS=ON
```

Remarks:
- `android.toolchain.cmake` can usually be found at `$ANDROID_HOME/ndk/x.y.z/build/cmake/android.toolchain.cmake`
- `ANDROID_ABI` should be one of `arm64-v8a`, `armeabi-v7a`, `x86` or `x86_64`.
- When CMake is unable to find required paths, use `cmake-gui` to override required `SDL_ANDROID_` CMake cache variables.

### Building the APK's

For the `testsprite` executable, the `testsprite-apk` target will build the associated APK:
```
cmake --build . --target testsprite-apk
```

APK's of all tests can be built with the `sdl-test-apks` target:
```
cmake --build . --target sdl-test-apks
```

### Installation/removal of the tests

`testsprite.apk` APK can be installed on your Android machine using the `install-testsprite` target:
```
cmake --build . --target install-testsprite
```

APK's of all tests can be installed with the `install-sdl-test-apks` target:
```
cmake --build . --target install-sdl-test-apks
```

All SDL tests can be uninstalled with the `uninstall-sdl-test-apks` target:
```
cmake --build . --target uninstall-sdl-test-apks
```

### Starting the tests

After installation, the tests can be started using the Android Launcher GUI.
Alternatively, they can also be started using CMake targets.

This command will start the testsprite executable:
```
cmake --build . --target start-testsprite
```

There is also a convenience target which will build, install and start a test:
```
cmake --build . --target build-install-start-testsprite
```

Not all tests provide a GUI. For those, you can use `adb logcat` to read the output.
