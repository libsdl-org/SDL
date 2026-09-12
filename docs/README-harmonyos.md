# HarmonyOS

(And also OpenHarmony.)

## What is HarmonyOS?

(This is my current understanding, please correct these docs if incorrect.)

HarmonyOS is an operating system for tablets and phones. It is developed by
[Huawei](https://www.huawei.com/en/). It is competitive with platforms like
Android and iOS, capable of powering high-end phones. Unlike Google and Apple,
Huawei is a Chinese company, and HarmonyOS is largely serving the Chinese
market at this time.

The system uses some custom pieces, but also relies on open source software
from outside of the project, such as NodeJS, musl, etc.

Much of the platform is open source, under the name "OpenHarmony," and Huawei's
proprietary spin of OpenHarmony is called HarmonyOS. Huawei is the primary
contributor to OpenHarmony.

So it is probably best said: HarmonyOS is to Android what OpenHarmony is to
AOSP. But, to be clear, OpenHarmony is not derived from Android.

This document will generally refer to HarmonyOS, but from a development
viewpoint, you are actually targeting OpenHarmony, even if you're building
with intent to only target HarmonyOS on Huawei-produced phones.


## Supported versions

All testing to date has been done on a real phone, a Huawei Mate 80. SDL
currently supports HarmonyOS 5.0.4 (API level 16) and later, although low-end
testing hasn't been done so there might be little changes needed for extreme
compatibility. Report bugs or send patches if this becomes a problem.


## Some basic target platform truths

More or less, an app on HarmonyOS is a NodeJS process, in the same way that
an app on Android is a Java virtual machine process. While one generally
writes apps in Java or Kotlin for Android, HarmonyOS uses a TypeScript
variant called ArkTS. However, like SDL on Android, we will be focusing on
native code in C or C++, so SDL makes moves similar to its Android strategy,
migrating from ArkTS to native code as quickly as possible and handing control
over to a (presumably C or C++) native code app, and making it so the app can
entirely opt out of interacting with the ArkTS layer directly.

The equivalent of the Android "JNI" layer is NodeJS's NAPI interfaces. A large
portion of the system APIs are accessible directly from OS-provided native
libraries, so there is a lot less reason to dip back down into the ArkTS layer
for most tasks, whereas lots of things on Android need to talk to Java-only
interfaces to accomplish common and important tasks, even in an
otherwise-entirely native program.

The C/C++ compiler is based on Clang (you might end up using a compiler called
"BiSheng", which is Clang with some custom optimization techniques developed
by Huawei, but the binary is still called "clang").

The platform triplet is "aarch64-linux-ohos" (other CPU architectures are
possible, but this is likely the most common. The
devtools mention 32-bit ARM and x86-64, too). The CMake toolchain file sets
the variable `OHOS` (OpenHarmony OS) to "OHOS", so `if(OHOS)` works. Further,
`CMAKE_SYSTEM_NAME` is set to "OHOS". The C preprocessor defines `__OHOS__`.


## Get HarmonyOS development tools

You will need to download Huawei's developer tools. They offer a full IDE,
called "DevEco Studio," but this is not strictly necessary for compiling SDL
itself.

There are also command line tools available for Windows, macOS, and Linux, and
these _are_ sufficient to compile SDL. For building full applications, you
likely need DevEco Studio to at least set up your project and manage signing
keys, etc. Some things (but not all) are doable from the command line tools,
and a handful of things are (currently) not.

DevEco Studio, the GUI, is only available for Windows and macOS. It appears to
be a fork of IntelliJ IDEA. It does not appear to be open source, afaict.

To download the developer tools, you'll need to register an account at
[the Huawei developer site](https://developer.huawei.com/en/). Then visit
[the tool download page](https://developer.huawei.com/consumer/en/download/)
For Linux, scroll down to "Command Line Tools." Download for your platform,
which should be a simple .zip file, and unpack it. If you're on Windows or
macOS, download and install DevEco Studio itself while you're here, too.


## Don't fight against DevEco Studio

I'm a Linux user, and I avoid programming IDEs in general, so my instinct is
to just get everything done with my text editor and the command line tools,
and this _feels_ possible, but I haven't quite figured it out. The system is
friendly to things like CMake, clang, etc, and the documentation, even in
English, is pretty thorough, but I still couldn't get there without using
DevEco Studio a little.

The system appears designed to at least hand off a working project from
DevEco Studio to a Linux machine for building and deploying from the command
line tools, for Continuous Integration if nothing else. Figuring out how to
eliminate the Windows/Mac GUI dependency entirely is an ongoing and unresolved
concern. I definitely needed it to create and configure a project, and manage
signing keys. Once I got that far, I was able to copy the appropriate
directories to a Linux system and use the `hvigorw` command line tool to build
and sign an app, and `hdc` to install it on a real phone.


## Install command line tools.

(If you're entirely focused on DevEco Studio, you can probably skip this.)

I unzipped the downloaded command line tools so they're sitting in
`$HOME/HarmonyOS-SDK/command-line-tools`, but anywhere you want to put it is
fine.

When this document refers to this directory, it'll call it `$CLT` for short.

Add some things to your PATH, so you can find `hdc` and `hvigorw`, etc.

```bash
export PATH=$PATH:$CLT/sdk/default/openharmony/toolchains:$CLT/bin
```


## Putting a HarmonyOS phone in developer mode.

If you want to test and debug your SDL apps natively on a real phone directly,
you have to put the device in Developer Mode. As the name implies, this is not
necessary to install published apps, but only apps under development locally.
Any retail phone running HarmonyOS should be able to do this. This is almost
identical to how one would do this for local Android development.

On the phone, go to Settings, Device Name (it's "Mate 80" on mine, in a box by
itself near the top of the main settings page). This will bring up an "About"
page. Scroll down to "Software version" and tap the version number seven times
quickly. The phone will pop up a dialog asking if you want to enable developer
mode and reboot. Say yes. If you've already done this, it will tell you so.

After reboot, go into Settings, System. There will be a new "Developer options"
section at the bottom. Turn on USB debugging. Plug in a USB cable to your
computer, and choose to trust the connection (and choose "use USB to transfer
files" if given the option, otherwise it will only allow charging over USB).

At this point, you should be able to use "hdc" somewhat like you would use
"adc" on an Android device:

See if the phone is visible/accessible:

```bash
hdc list targets -v
```

You can also connect the phone over Wifi on the same LAN and not use USB. If
you want to try, turn on "Wireless debugging" in Developer Options, note the
IP address and port, and run:

```bash
hdc tconn 192.168.1.104:40317
```

Where the IP address and port match the phone.

(DevEco Studio: add the device by IP address and port by going to "Tools" ->
"IP Connection" on the menu bar. This appears to be IPv4 only at the moment,
but it successfully connected to the phone. This was a good option, because
for whatever reason the phone's USB connection would continually drop and
reconnect on my VirtualBox Windows 10 VM. But YMMV!)


## HarmonyOS App Development with SDL

### Create a new project

Copy the "openharmony-project" directory out of the root of SDL's source tree
to where you want your project to live, and name it appropriately
("mygame-openharmony" or whatever). Then, in that new directory:

- in AppScope/app.json5, change at least "bundleName" and "vendor" to match
  your project.
- in AppScope/resources/base/element/string.json, change "app_name"'s value to
  the name of your app.
- in build-profile.json5, you need to add a signingConfig (this is probably
  doable without the GUI, but I have not entirely solved how to do this outside
  of DevEco Studio yet).

Do not create a new project in DevEco Studio, since there are pieces of ArkTS
scripting that need to be set up correctly for SDL, and that code is already
present in openharmony-project. You _should_ be able to open the project in
DevEco Studio once you've set it up.


### Add source code to your new project.

Native code building uses CMake, whether you use DevEco Studio or not. This
makes integrating SDL into your project easy!

Edit the text file entry/src/main/cpp/CMakeLists.txt inside your new project
directory and change this line:

```cmake
add_library(main SHARED myapp.c)
```

"myapp.c" should be replaced with any source files that comprise your app.
This is a standard CMake project file, so you can get as fancy as you want
or just have a list of files separated by spaces.

These files do not have to be in this directory; they can live outside of
the project directory entirely, as long as you specify the correct directory
to reach them.

You must either copy the SDL source tree into entry/src/main/cpp, or make a
symlink to it, or edit the `add_subdirectory("SDL")` line to point to it.
This line makes SDL's source compile with the rest of the project. The
directory you point to must be the root of an SDL clone, the one that holds
CMakeLists.txt, src, and include.

Your app _must_ compile to a shared library named "libmain.so" ... the
"add_library" line does this. Do not build an executable! It must be a
shared library, due to how HarmonyOS deals with apps. When you're done
building, you should have a libmain.so and a libSDL3.so file.


### Customize your project

If you need some specific permissions (microphone/camera access, internet
access, etc), you must list them in
openharmony-project/entry/src/main/module.json5 ...some of these are
automatically granted on installation, some will require the user to
approve them on first use at runtime, but all of them must be listed. See
the "requestPermissions" section of this file for examples. There are more
permissions available than are listed there!

You must set up some basic strings, before publishing your app, or you will
either have an app icon with name "label" or fail certification because you
didn't explain clearly why you need various permissions.

These are in a JSON file in:

openharmony-project/entry/src/main/resources/base/element/string.json

You can do localizations of these strings by making a second copy of the
file, where "base" is replaced with the proper localization ("en_GB" or
whatever). See the documentation at:

https://developer.huawei.com/consumer/en/doc/harmonyos-guides/resource-categories-and-access

Static data files that ship with your app go in the directory:

openharmony-project/entry/src/main/resources/rawfile/

The file tree under this directory will be packaged up with the app and made
available through SDL APIs (see "Filesystem" and "IOStream" sections, below).


### Get a HarmonyOS Debug Certificate

You need to sign apps to run them on a real device, even for local debugging
and testing and not just deployment and release.

For now, just open your project in DevEco Studio, then in the menu bar, go
to "File", "Project Structure", "Signing Configs", "Automatically generate
signature". Log in to your Huawei developer account if needed. Let it do the
work. Later, I copied the project's directory and ".ohos" dir in my home
directory to a Linux system, adjusted a few Windows absolute paths in
build-profile.json5, and that was good enough.


### Build your app from the command line

Building your project from DevEco Studio is just a single click, but you
can also build from the command line on macOS, Windows, or Linux.

If you have everything (signing certs, the entire project structure, etc) in
place, you can build an app from the command line. The HarmonyOS equivalent of
Android's `gradlew` command is `hvigorw`. Go to the root folder of the project
and run:

```bash
hvigorw assembleHap
```

If everything went well (AND IT OFTEN DOES NOT, READ THE OUTPUT!), you'll have
a signed HarmonyOS app bundle (a ".hap" file) you can install to a device.

There is a Bash shell script to automate this as much as possible: see the
"The Helper Script" section, below.


### Install on real hardware from the command line.

Make sure the phone is connected (run `hdc list targets -v` to verify).

```bash
# (or whatever the directory and .hap file are named.)
hdc install ./entry/build/default/outputs/default/entry-default-signed.hap
```

There is a Bash shell script to automate this as much as possible: see the
"The Helper Script" section, below.


### Debugging

If you just want to do "printf debugging," then SDL_Log() will write to the
system's "hilog" output, and you can use `hdc hilog |grep -F "APPBUNDLENAME"`
to find your app's output in the absolute firehose of text that is produced
through that interface.

One can use a real GUI debugger through DevEco Studio without a lot of drama,
and one can also use lldb from the command line with "remote debugging." The
latter option takes a lot of manual setup, and additionally you have to be
familiar with LLDB's text interface to find this useful, but it _can_ be done
from Linux, where DevEco Studio isn't available (and also Windows and macOS,
if all you have are the command line tools).

There is a Bash shell script to automate this as much as possible: see the
"The Helper Script" section, below.

The instructions on setup from scratch, without the shell script, are here:

https://developer.huawei.com/consumer/en/doc/harmonyos-guides/debug-lldb#remote-debugging

The only thing that document fails to mention is that the PID number you need
for LLDB's `attach` command can be obtained by running `hdc shell ps` and
looking for your app's bundle ID.


### The Helper Script

There is a Bash shell script to simplify several tasks, if you're using the
command line tools. This is not necessary from DevEco Studio. This has only
been tested on Linux.

Make sure your PATH is set up (see "Install Command Line Tools," above), and
run this command:

```bash
SDL/build-scripts/harmonyos-tool.sh openharmony-project --build --debug
```

"openharmony-project" is literally the path to your project file. It can be a
relative path, including "." if you are sitting in the directory.

All the other command line arguments are optional, and can be used in any
combination (for example, you can --launch without --build'ing, or --install
and --debug in one run, etc.)

If you specify --log, the script will fire up `hdc hilog` in the background
and filter out everything but your app bundle's log calls, so you can watch
the log while other tasks complete. It will start this once passed the
--install stage (even if you didn't install), so you are watching it while
launching/debugging begins. After all other tasks are done, harmonyos-tool.sh
won't terminate until either the `hdc` process terminates or the user hits
CTRL-C (rather, until SIGINT is delivered, which CTRL-C does). The script will
kill the `hdc` process in that case before terminating itself.

If you specify --clean, the script will clean up any build artifacts from
previous compiles, etc. This will run before --build, so you can specify
`--clean --build` for a complete rebuild.

If you specify --build, the script will compile C/C++ code and assemble the
.hap file to be run on a device or emulator.

If you specify --install, the script will (re)install the .hap on a device
(which will kill the process if it's currently running).

If you specify --uninstall, the script will remove the existing app on a
device (which will kill the process if it's currently running).

If you specify --launch, the script will launch the app on a device, if it
isn't already running.

If you specify --debug, the script will prepare remote debugging via lldb,
and attach to the running app.

If you specify --kill, the script will force-stop any existing process.

(--debug implies --launch, "--build --launch" and "--build --debug" imply
--install.)

Most of these tasks obviously need a phone that hdc can talk to.

Note that debugging always attaches to a process, so if you need to debug
something at startup, plan to add a sleep to the start of the program, as it
can take several seconds for the attachment to complete. It isn't clear to me
how to launch an app from the debugger so you can break at startup, but there
is probably a way.

Also note that this script is fragile in general; be gentle, report bugs, send
patches.


## SDL/HarmonyOS subsystem details

Specifics of actually using SDL on HarmonyOS follow.


### SDL platform defines

SDL will define SDL_PLATFORM_OPENHARMONY and SDL_PLATFORM_UNIX. Do be cautioned
that while HarmonyOS has some Linux kernel compatibility interfaces, it is
_not_ comparable to a "normal" Linux system in almost any way, so
SDL_PLATFORM_LINUX is not defined (SDL for Android, which literally uses the
Linux kernel, follows this same convention inside SDL). HarmonyOS devices
don't use the actual Linux kernel (they use a microkernel named "HongMeng"),
but OpenHarmony devices that aren't specifically HarmonyOS _might_ use a Linux
kernel. Don't make assumptions.


### Dynamic API

Presumably users cannot override the SDL build in their phone's apps, so the
Dynamic API is disabled on this platform.


### Log

SDL_Log() and friends will push through hilog, so you can view the output
from the `hdc hilog` command. Note that logs are preformatted strings by the
time they hit hilog, so things like "%s" are already processed, and thus
there is no `{public}` and `{private}` format modifiers available like there
would be if calling into hilog's APIs directly.

Output from `hdc hilog` for `SDL_Log("Hello world!");` will look something like:

```
06-29 22:22:02.953 25275 25275 I A00000/org.libsdl.loopwave/SDL/APP: Hello world!
```

This is kind of chatty, but not unlike Android's `adc logcat` output.

These logs are viewable to end-users, even in release builds, if they have a
phone in Developer Mode that can talk to `hdc`, so be careful what you log!

Also plan to use "grep" to find relevant logs; `hdc hilog` is an endless
waterfall of unrelated data that you _will_ get lost in immediately.


### Main

HarmonyOS has a fairly complicated startup sequence (described later in this
document). Apps for this platform should use the "main callbacks" (with
`#define SDL_USE_MAIN_CALLBACKS`) instead of an ANSI C style "main" entry
point, as that fits the app design paradigm of OpenHarmony.

However, if you build an app with a standard "main" function, SDL will notice
this, spin a thread, and attempt to call that function from the new background
thread, and call exit() when it returns. This _happens_ to work, at least for
simple test cases that do rendering and touch input, but one uses this path at
their own risk, as several important things might result in unexpected race
conditions and unexpected behavior, possibly in a later version of the OS. It
is strongly recommended that you migrate to the Main Callbacks!


### Filesystem

HarmonyOS uses '/' as a path separator, like Unix-style (etc) systems.

SDL_GetBasePath() returns "assets://" unconditionally (see IOStream section,
below). SDL_GetPrefPath() returns a path under the string returned from
OH_AbilityRuntime_ApplicationContextGetFilesDir(), which in practice looks
like "/data/storage/el2/base/files/APPNAME" ... this looks like an absolute
path, but HarmonyOS maps this to an app-and-user-specific path behind the
scenes. The prefpath is readable/writable with both SDL_IOStream and "normal"
APIs like fopen().

There is also SDL_GetOpenHarmonyInternalStoragePath(), which returns the base
directory used for SDL_GetPrefPath() without extra subdirs appended to it or
mkdir() calls issued, but generally SDL_GetPrefPath() is preferred as the
more-portable option.


### IOStream

This works like Android: some files are accessible like normal filesystem
things, and can be accessed with fopen(), open(), etc. Other things are
installed with the application (the "HAP"), and they need special APIs to
access, that sort of map to Android's AAssetManager, but a little more
powerful (these are called "RawFile" APIs on HarmonyOS).

Both types of files can be accessed as SDL_IOStreams through SDL_IOFromFile().
SDL uses the same conventions as Android here: if the file path is a relative
path or prefixed with "assets://", it'll use the RawFile APIs. Absolute paths
will use "normal" filesystem APIs. SDL_GetBasePath() returns "assets://", so
one can access files installed with the app with SDL_IOFromFile().

"assets://" is entirely an SDL3 construct; it will not work with fopen(), and
is not understood by the system itself.


### Async I/O

SDL uses the "generic" asyncio backend here (a thread pool that uses
SDL_IOStreams). Not only is there no sort of io_uring-style interface (afaik),
but this also means you can async-load stuff from "assets://" paths without
problems.


### Video

One SDL_Window is allowed at a time and it takes the entire available display
(like Android). Currently we only report a single display (the phone/tablet's
screen).

Setting an SDL_Window to be fullscreen means it will turn off the system
status and navigation bars, allowing the window to use those extra pixels. A
non-fullscreen window leaves those bars in place, letting users see the
current time, battery/network/etc status, and other icon things.

Touch events work, and multitouch is supported. Mouse events (from an external,
physical mouse) are supported. Physical keyboard input works, and enabling
text input will pop up a screen keyboard.

Clipboard is supported for text. SDL_SetClipboardText() works as expected
and other apps will see the text. However, an app needs the
`ohos.permission.READ_PASTEBOARD` permission to obtain data from the system
clipboard, and this is a _restricted_ permission: you must list it in
modules.json5, prompt the user to approve it beforehand by calling
SDL_RequestOpenHarmonyPermission(), and also the app developer must
explicitly apply for permission from Huawei through AppGallery Connect!
Consider if you actually need to read clipboard data into your app that badly.

SDL_GetSystemTheme() and SDL_EVENT_SYSTEM_THEME_CHANGED both work.


### Render/GPU

OpenGL ES 3 is supported, as is Vulkan. Desktop OpenGL is supported by
the platform, too, but for now support is disabled within SDL itself as likely
superfluous (until it is not, open an issue if you need it). EGL is used to
manage GL contexts internally, and a HarmonyOS-specific Vulkan extension is
provided to get a surface.

Like other platforms, we favor OpenGL ES 2 for the 2D renderer; Vulkan and "GPU"
also works, but SDL's GLES is assumed to be more trusted at the current moment,
and historically GLES has been more solid on mobile devices than Vulkan, but
HarmonyOS-based phones might have significantly better Vulkan support than
other comparable smartphones; this is yet to be determined.

Since Vulkan works, the SDL3 GPU API is available and operational, in addition
to the hardware-accelerated 2D Renderer API.


### Audio

HarmonyOS support OpenSL ES, but like on Android, it is deprecated. As our
OpenSL ES backend is heavy with Android-specific code, it is not used on
HarmonyOS. There is a new backend using HarmonyOS's OHAudio API. This is
almost exactly like Android's AAudio API.

Recording works, but you must request `ohos.permission.MICROPHONE` permission
before it will work. You must list it in modules.json5, and prompt the user to
approve it beforehand by calling SDL_RequestOpenHarmonyPermission(). Once
permission is granted, recording audio devices can be opened as usual, and
this permission remains for future runs, unless the user revokes in in the
system preferences later. On HarmonyOS, if a user denies permission, they will
never get the popup request dialog again, and must go and grant permission
through the system preferences. Plan accordingly if you want your UI to
explain this to users.

Note that SDL for Android currently requests permission from the user on
behalf of the app when opening a recording device, and blocks until the user
approves or denies the request, but on HarmonyOS this is not done. This may
change in the future, but for now the app should handle requesting permission
before opening the device. If a future revision of SDL tries to manage the
permission, it won't hurt anything if the app has already taken care of it.


### Power

Battery percent, and whether the device is plugged in, are reported.


### Thread

OpenHarmony uses pthreads, and are fully supported.


### LoadSO

HarmonyOS uses ELF shared libraries and has a POSIX-style dlopen() mechanism,
so SDL_LoadObject() works as expected.


### Tray

The Tray API uses the "dummy" backend as this functionality doesn't make sense
on a phone.


### Time

The Time and Timer subsystems use the usual Unix implementations and work fine.


### Locale

SDL_GetPreferredLocales() returns a single locale, as specified in System
Preferences -> "System" -> "Language & Region". In my case, I have it set to
English/United States, so SDL reports "en_US".

SDL will send SDL_EVENT_LOCALE_CHANGED events if the user changes their locale
in the system preferences while the app is running.


### Camera

Camera access works. You must request the `ohos.permission.CAMERA` permission
in modules.json5. SDL will handle the UI to prompt the user for permission
during SDL_OpenCamera() if necessary, so the app doesn't have to manage this
further (unlike using a microphone in the audio API, SDL's camera API was
built on the idea that the user's permission or refusal might be coming at a
later time).


### Misc

SDL_OpenURL() works, and will launch the appropriate app ("ability") for
whatever protocol is specified: "https://", etc, URLs will bring up a web
browser, custom URL schemes can launch specific apps, etc.


### STILL TODO

- Assert
- Dialog
- Haptic
- HIDAPI
- Joystick
- Process
- Sensor
- Storage
- Pen (tablets work over USB-C on the phone, but this isn't wired up in SDL at all)
- SDL_EVENT_DISPLAY_ORIENTATION (window resizes when phone orientation changes, this event doesn't fire yet, though)
- Can external displays be used over USB-C => HDMI?  (yes, at least for mirroring)
- SDL_HINT_ORIENTATION (for runtime control; but you _can_ explicitly set the "orientation" setting in module.json5 for now if you need a specific global, unchanging setting.)

## Building SDL for HarmonyOS

(Skip this if you are developing an SDL-based app and not SDL itself. There's
a different section of this document for you, later.)

SDL for HarmonyOS is built with CMake, like other platforms. Please note that
you _must_ use the CMake that comes with HarmonyOS's development tools, or
building will fail, as standard CMake does not recognize this platform at all
currently.

Generate a project file for building SDL. We use Ninja, because it's a fast
command line tool that will maximize multi-CPU compiling, but it's probably
possible to generate a real Visual Studio, Xcode, etc, project.

```bash
cd SDL
mkdir buildbot-ohos   # or whatever you want to call the build directory.
cd buildbot-ohos
$CLT/sdk/default/openharmony/native/build-tools/cmake/bin/cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$CLT/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DOHOS_COMPATIBLE_SDK_VERSION=16.0.0 ..
```

(One can also add -DOHOS_ARCH=cputype, where "cputype" is "arm64-v8a" for ARM64, "armeabi-v7a" for ARM32, and "x86_64" Intel 64-bit. It defaults to ARM64 if unspecified.)


If all went well, you should be able to build SDL for HarmonyOS now. In our
case, build with Ninja:

```bash
ninja
```

Or use whatever instructs Xcode/Visual Studio/etc to build things.



## Startup sequence

(You can probably skip this section if you just want to target HarmonyOS with
an SDL3-based app and not delve into the low-level details.)

Starting an SDL app on HarmonyOS works differently than any other platform,
but efforts have been made to _hide_ most of this from the C programmer, so
this should mostly feel like any other SDL app they have built in C before.

This is how startup works:

(Exact paths might vary, and maybe this will simplify later.)

- The actual application entry point is in ArkTS, in the source file
  `$PROJECT/entry/src/main/ets/entryability/EntryAbility.ets`.
- This file defines a class, EntryAbility, that extends UIAbility, and
  also imports the native SDL library.
- By now, libSDL3.so has been loaded. In a shared library constructor named
  SDL_RegisterNativeInterfaces, it uses NAPI to register a native module. Once
  this module initializes, we'll be able to call into SDL's C code from ArkTS
  through interfaces we defined in SDL_Init_Native_Interfaces(). This is in
  SDL/src/core/openharmony/SDL_openharmony.c. This will _also_ hook into
  XComponent's API, to register some event callbacks.
- EntryAbility has an overridden method named onCreate. When this
  runs, the app ("Ability" in HarmonyOS terms) is starting up. This function
  calls into the sdl module we registered to hand it a few objects (including
  the EntryAbility instance, and some small things that are trivial to create
  in ArkTS.
- SDL, now in native code, will save off those objects as appropriate, and
  also monkey-patch EntryAbility, so that it now has several overridden
  methods that only exist in native code. This lets us deal with ArkTS-only
  interfaces on EntryAbility that the system intends to interact with, without
  providing any ArkTS scripting for them.
- EntryAbility now has an overridden method named onWindowStageCreate. When this
  runs, the system is ready for the app to display to the screen. This
  method, in C, calls `windowStage.loadContent('pages/Index', ...)`.
- This causes the app to load `$PROJECT/entry/src/main/ets/pages/Index.ets`.
- Index.ets specifies a layout for the window/view/whatever. We simply have a
  single row and column containing a single "XComponent" which is more or
  less a UI widget backed by native code.
- When the XComponent's OnSurfaceCreated callback fires (landing in
  SDL_XComponent_OnSurfaceCreatedCallback()), we are then ready to hand control
  to the actual C application. This starts in
  SDL_OpenHarmonyMainSurfaceCreated(), which eventually lands in
  RunAppOpenHarmonyMain().
- Here we load libmain.so, which is where the app's actual C code lives.
  If we see if libmain contains a symbol named "SDL_main", we spin a thread
  and call into that symbol from the new thread as a standard ANSI C "main"
  entry point, and exit() the process when it returns (which looks like a
  crash on HarmonyOS, so please don't return.)
- If there was no symbol called "SDL_main", we look for the Main Callbacks
  symbols in libmain.so: SDL_AppInit, SDL_AppIterate, etc. If found, we will
  call libmain.so's SDL_AppInit() and respond appropriately. If not found, we
  panic and terminate the process, since we're out of options and something
  was obviously built incorrectly.
- Later, whenever the XComponent fires its OnFrame callback, we will call
  SDL_OpenHarmonyOnFrameCallback(), which calls libmain.so's SDL_AppIterate(),
  if we ended up using the Main Callbacks.



## Incomplete notes

Everything below here is incomplete, in-progress, and maybe incomprehensible.
You can stop reading now, if you like.

- A lot of how to get started with native code and rendering was gleaned from
  https://gitee.com/harmonyos_samples/ndk-opengl and then trying to find
  various symbols used in there in the HarmonyOS documentation.

- https://github.com/openharmony/app_samples/ETSUI/XComponent, seems to do
  a lot of the native startup without the windowStage nonsense. Not clear if
  this still works, or is safe to do, but it _would_ simplify things a little.


### Android mappings

These are not direct mappings (the command lines are not identical, etc).

- "adc" command line tool: "hdc"
- "adc logcat": "hdc hilog"
- "gradlew": "hvigorw"


### Get a HarmonyOS Debug Certificate without DevEco Studio

This is a giant dump of notes for doing this manually, which I didn't _quite_
manage to get working. Huawei has docs on this, which I've sort of half limped
through.

For now, just create your project in DevEco Studio,
then in the menu bar, got to "File" -> "Project Structure" ->
"Signing Configs" -> "Automatically generate signature". Log in to your Huawei
developer account if needed. Let it do the work. Later, I copied the project's
directory and ".ohos" dir in my home directory to a Linux system, adjusted a
few Windows absolute paths in build-profile.json5, and that was good enough.


(Ignore these notes if you aren't improving them.)

If you aren't using DevEco Studio to automatically generate signatures:

You need a signing certificate before you can install apps on a real HarmonyOS
device.

You get a certificate by generating a Certificate Signing Request (a "CSR")
and submitting it to Huawei.

Right now this needs DevEco Studio to generate the CSR file, as a one-time
step. Go into the IDE, click the hamburger menu icon on the top left of the
window, choose the "Build" menu item, and then click "Generate Key and CSR".
In the dialog that pops up, you'll first make a signing key in ".p12" format.
Enter a filename and save path, choose a password for the key. Fill in the
"Alias" which is any short, unique, possibly-ASCII, human readable string.
Ignore the Advanced section.

Click "Next". Now you'll generate the CSR file. The same .p12 file, password
and alias you just generated will be prefilled in the new dialog. Just tell it
where to save the new .csr file. Click "Finish" and you're done with DevEco
Studio for this task.

The Common Name ("CN=" field) on the Certificate Request is what DevEco Studio
refers as an "alias." I put this in as "icculus.org" for mine, but it might
just need to be unique. It's required, all the other fields are not (indeed,
DevEco Studio does not ask for them by default and leaves them blank).

*TODO IN THE FUTURE:* the CSR files are in a standard format, so OpenSSL's
command line utility can _probably_ generate them without having to go through
DevEco Studio, but anything OpenSSL is complicated and needs research.

Dumping the .csr file that DevEco Studio generated (using the command line:
`openssl req -in MyCSRFile.csr -noout -text`) reports the following, so there
are probably OpenSSL incantations to generate your own .p12 and .csr file
directly.

```
Certificate Request:
    Data:
        Version: 1 (0x0)
        Subject: C=, ST=, L=, O=, OU=, CN=[the "Alias" that I entered is here]
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub:
                    [hex values for public key are here]
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        Attributes:
            Requested Extensions:
                X509v3 Subject Key Identifier: 
                    [hex values for key identifier are here]
    Signature Algorithm: ecdsa-with-SHA256
    Signature Value:
        [hex values for signature are here]
```

Once you have a .csr file, go to Huawei's AppGallery Connect portal, and find
the "Certificates, app IDs, and profiles" tab at the top. Click the "New
certificate" button, and upload the .csr file you generated. Give it a name,
set the "Certificate type" to "Debug certificate" and point to your .csr file.
Click "Submit".

If everything went okay, you'll have a certificate listed for download. This
should be an immediate thing, it doesn't need approval on Huawei's side.
Download the .cer file and save it for later.

