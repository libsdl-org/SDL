HarmonyOS
================================================================================

The rest of this README covers the HarmonyOS hvigor style build process.



Requirements
================================================================================

HarmonyOS SDK (HarmonyOS-NEXT-DP0 or later)
HarmonyOS SDK can be downloaded DevEco Find the sdk compressed package in the SDK folder under the subdirectory and decompress it for use.

HarmonyOS SDK 11 or later
https://developer.huawei.com/consumer/cn/doc/harmonyos-references-V2/2_1_u6a21_u5757-0000001446810528-V2

Minimum API level supported by SDL: 11 (HarmonyOS 4.1.0)



How the port works
================================================================================

- HarmonyOS applications are ArkTS-based, optionally with parts written in C/C++.
- As SDL apps are C-based, we use a small ArkTS shim that uses napi_env to talk to
  the SDL library.
- This means that your application C/C++ code must be placed inside an HarmonyOS
  ArkTS project, along with some C/C++ support code that communicates with ArkTS.
- This eventually produces a standard HarmonyOS.hap package.

The HarmonyOS ArkTS code implements an "Page" and can be found in:
ohos-project\entry\src\main\ets\pagse\Index.ets.

The ArkTS code loads your SDL shared library, and dispatches to native functions implemented in the SDL library:
src\core\ohos\SDL_ohos.cpp.



Building an app
================================================================================

If you already have a project that uses CMake, the instructions change somewhat:

1. Get the source code for SDL and copy the 'ohos-project' directory located at SDL/ohos-project to a suitable location. Also make sure to rename it to your project name (In these examples: YOURPROJECT).

   (The 'ohos-project' directory can basically be seen as a sort of starting point for the ohos-port of your project. It contains the glue code between the HarmonyOS ArkTS 'frontend' and the SDL code 'backend'. It also contains some standard behaviour, like how events should be handled, which you will be able to change.)

2. Add the dependencies directory (hvigor file directory ) provided under the YOURPROJECT directory .

3. Move or [symlink](https://en.wikipedia.org/wiki/Symbolic_link) the SDL directory into the "YOURPROJECT/entry/src/main/cpp" directory

​	(This is needed as the source of SDL has to be compiled by the HarmonyOS compiler)

3. Edit "YOURPROJECT/entry/src/build-profile.json5" to specify the path to the CMakeLists.txt file
4. Edit "YOURPROJECT/build-profile.json5" to specify the version of the sdk
5. Edit "YOURPROJECT/entry/src/main/cpp/CMakeLists.txt" to include your project (it defaults to adding the SDL subdirectory). Note that you'll have SDL2  as targets in your project, so you should have "target_link_libraries(entry PUBLIC libSDL2d.so)"
   in your CMakeLists.txt file. Also be aware that you should use add_library() instead of
   add_executable() for the target containing your "main" function. "add_library(entry SHARED ${my_files})"

Here's an explanation of the files in the HarmonryOS project, so you can customize them:

    ohos-project
     	build-profile.json5					- Application-level configuration information, including signatures, product configuration.
     	hvigor/hvigor-config.json5			- Configure hvigor file path
     	entry/build-profile.json5			- Current module information, build information configuration options, including buildOption, targets configuration.
     	entry/src/main/module.json5 		- The module configuration file contains the configuration information of HAP package, the configuration information of the application on the specific device and the global configuration information of the application.
     	entry/src/main/cpp/					- directory holding your C/C++ source
     	entry/src/main/ets/					- directory holding your ArkTS source
     	entry/src/main/cpp/application/		- directory holding your ohos appliction C/C++ source
     	entry/src/main/cpp/SDL/				- (symlink to) directory holding the SDL library files
     	entry/src/main/cpp/CMakeLists.txt	- Top-level CMake project that adds SDL as a subproject		
     	entry/src/main/ets/page/Index.ets	- Harmonry OS application corresponding to the main screen and some page life cycle functions.
     	entry/src/main/resources/rawfile/	- directory holding resources for your application



Customizing your application name
================================================================================

To customize your application name, edit string.json and replace
"XComponent" with an identifier for your application name.



Customizing your application icon
================================================================================

Conceptually changing your icon is just replacing the "app_icon.png" files in the drawable directories under the media directory. 

There are several directories for different screen sizes.



Loading rawfile
================================================================================

Any files you put in the "entry/src/main/resources/rawfile" directory of your project
directory will get bundled into the application package and you can load
them using the standard functions in SDL_ohosfile.h.

The SDL ohosfile.h file contains the following functions:

```
int OHOS_FileOpen(SDL_RWops *ctx, const char *fileName, const char *mode);
Sint64 OHOS_FileSize(SDL_RWops *ctx);
Sint64 OHOS_FileSeek(SDL_RWops *ctx, Sint64 offset, int whence);
size_t OHOS_FileRead(SDL_RWops *ctx, void *buffer, size_t size, size_t maxnum);
size_t OHOS_FileWrite(SDL_RWops *ctx, const void *buffer, size_t size, size_t num);
int OHOS_FileClose(SDL_RWops *ctx, SDL_bool release);
```

The asset packaging system will, by default, compress certain file extensions.
SDL includes two asset file access mechanisms, the preferred one is the so
called "File Descriptor" method, which is faster and doesn't involve the Dalvik
GC, but given this method does not work on compressed assets, there is also the
"Input Stream" method, which is automatically used as a fall back by SDL. You
may want to keep this fact in mind when building your HAP, specially when large
files are involved.



Pause / Resume behaviour
================================================================================

If SDL_HINT_OHOS_BLOCK_ON_PAUSE hint is set (the default),
the event loop will block itself when the app is paused (ie, when the user
returns to the main Harmonry OS dashboard). Blocking is better in terms of battery
use, and it allows your app to spring back to life instantaneously after resume
(versus polling for a resume message).

Upon resume, SDL will attempt to restore the GL context automatically.
In modern devices this will most likely succeed and your app can continue to operate as it was.

However, there's a chance (on older hardware, or on systems under heavy load),
where the GL context can not be restored. In that case you have to listen for
a specific message (SDL_EVENT_RENDER_DEVICE_RESET) and restore your textures
manually or quit the app.

You should not use the SDL renderer API while the app going in background:
- SDL_EVENT_WILL_ENTER_BACKGROUND:
    after you read this message, GL context gets backed-up and you should not
    use the SDL renderer API.

    When this event is received, you have to set the render target to NULL, if you're using it.
    (eg call SDL_SetRenderTarget(renderer, NULL))

- SDL_EVENT_DID_ENTER_FOREGROUND:
   GL context is restored, and the SDL renderer API is available (unless you
   receive SDL_EVENT_RENDER_DEVICE_RESET).



# Page lifecycle

aboutToAppear?(): void

The aboutToAppear function is executed after a new instance of the custom component is created and before its build function is executed. 
This allows you to change the state variable in aboutToAppear, and the changes will take effect when the build function is executed.

aboutToDisappear?(): void

The aboutToDisappear function is executed before the custom component is destroyed. It is not allowed to change state variables in 
aboutToDisappear function, especially the modification of @Link variable may cause the application to behave unstable.

onPageShow?(): void

This is triggered every time the page is displayed, including the routing process, the application entry before and after the background,
 and so on.Only the @Entry custom component is applied.

onPageHide?(): void

This is triggered every time the page is hidden, including the routing process, the app entry front and back, and so on.
Only the @Entry custom component is active.



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



Threads Model
================================================================================

When calling onPageShow in the ArsTS source code of HarmonryOS, it will be called through the so library compiled by SDL2:

```
sdl.init(callbackRef)
sdl.sdlAppEntry("libentry.so", "main", ...)	
```

Create a thread by calling the OHOS_RunThread function from the SDLAppEntry function in the SDL project,

The SDL_CreateThreadInternal function is called to create a thread to execute our SDL_main function.



Using STL
================================================================================

You can use STL in your project by edit  "YOURPROJECT/build-profile.json5" file configure argments in externalNativeOptions

```c++
"arguments": "-DOHOS_STL=c++_shared"
```



Using the emulator
================================================================================

There are some good tips and tricks for getting the most out of the
emulator.

Especially useful is the info on setting up OpenGL ES 2.0 emulation.

Notice that this software emulator is incredibly slow and needs a lot of disk space.
Using a real device works better.



Troubleshooting
================================================================================

You can run the following command to view the hdc version

    hdc -v

You can see if hdc can see any devices with the following command:

    hdc list targets

You can see the output of log messages on the default device with:

    hdc hilog

You can push files to the device with:

    hdc file send local_file remote_path_and_file

You can start a command shell on the default device with:

    hdc shell

You can restart the device using the following command:

    hdc shell reboot

You can restart the hdc using the following command:

    hdc start -r



HarmonyOS 4.1 Beta1 provides the first API Level 11 interfaces
================================================================================

The use of the API Level 11 interface provides significant improvements in several areas.

ArkUI components are richer and more powerful, enabling developers to create more colorful                                                                                                    user interfaces. Graphical Windows move more smoothly, and window adaptation has been                                                                                                    enhanced to ensure a great visual experience on a variety of devices.



Ending your application
================================================================================

The app will be closed when all pages need to be closed or cleared from the background. In this case ArkTS calls aboutToDisappear(),

Your application will receive an SDL_QUIT you  can handle to save things and quit.

Do not use exit() because it will crash.

NB: "Back button" can be handled as a SDL_EVENT_KEY_DOWN/UP events, with Keycode
SDLK_AC_BACK, for any purpose.



Building the SDL tests
================================================================================

SDL's CMake build system can create HAP's for the tests.
It can build all tests with a single command  dependency on  DevEco Studio.
HarmonyOS relies on Deveco studio to build HAP packages. There are two ways to do this.

The first method the test files, in "ohos-project/entry/src/main/cpp/application" directory, connected 

devices and the engineering construction of HAP package, through the HDC cmake command on the device test file execution.

In the second method, the Deveco build project sends the HAP package generated under

 "ohos-project\entry\build\default\outputs\default" to the device via the hdc command for execution.

### Requirements

- HarmonyOS Platform SDK 
- HarmonyOS Build tools

### Send HAP to the device

Run the hdc command to create a directory on the device:

```
hdc shell mkdir data/local/tmp/filename
```

Send the hap package from the "ohos-project\entry\build\default\outputs\default" directory to the directory created above:

```
hdc file send ohos-project\entry\build\default\outputs\default\filename.hap "data/local/tmp/filename"
```

Install the incoming HAP application on the device:

```
hdc shell bm install -p data/local/tmp/filename
```

### Starting the tests

Through the "hdc shell aa start" command to start the package called "com",for example "com.example.myapplication".

```
hdc shell aa start -a EntryAbility -b com.example.myapplication
```

