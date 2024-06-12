iOS
======

Building the Simple DirectMedia Layer for iOS 9.0+
==============================================================================

Requirements: Mac OS X 10.9 or later and the iOS 9.0 or newer SDK.

Instructions:

1. Open SDL.xcodeproj (located in Xcode/SDL) in Xcode.
2. Select your desired target, and hit build.


Using the Simple DirectMedia Layer for iOS
==============================================================================

1. Run Xcode and create a new project using the iOS Game template, selecting the Objective C language and Metal game technology.
2. In the main view, delete all files except for Assets and LaunchScreen
3. Right click the project in the main view, select "Add Files...", and add the SDL project, Xcode/SDL/SDL.xcodeproj
4. Select the project in the main view, go to the "Info" tab and under "Custom iOS Target Properties" remove the line "Main storyboard file base name"
5. Select the project in the main view, go to the "Build Settings" tab, select "All", and edit "Header Search Path" and drag over the SDL "Public Headers" folder from the left
6. Select the project in the main view, go to the "Build Phases" tab, select "Link Binary With Libraries", and add SDL2.framework from "Framework-iOS"
7. Select the project in the main view, go to the "General" tab, scroll down to "Frameworks, Libraries, and Embedded Content", and select "Embed & Sign" for the SDL library.
8. In the main view, expand SDL -> Library Source -> main -> uikit and drag SDL_uikit_main.c into your game files
9. Add the source files that you would normally have for an SDL program, making sure to have #include "SDL.h" at the top of the file containing your main() function.
10. Add any assets that your application needs.
11. Enjoy!


TODO: Add information regarding App Store requirements such as icons, etc.


Notes -- Retina / High-DPI and window sizes
==============================================================================

Window and display mode sizes in SDL are in "screen coordinates" (or "points",
in Apple's terminology) rather than in pixels. On iOS this means that a window
created on an iPhone 6 will have a size in screen coordinates of 375 x 667,
rather than a size in pixels of 750 x 1334. All iOS apps are expected to
size their content based on screen coordinates / points rather than pixels,
as this allows different iOS devices to have different pixel densities
(Retina versus non-Retina screens, etc.) without apps caring too much.

By default SDL will not use the full pixel density of the screen on
Retina/high-dpi capable devices. Use the SDL_WINDOW_ALLOW_HIGHDPI flag when
creating your window to enable high-dpi support.

When high-dpi support is enabled, SDL_GetWindowSize() and display mode sizes
will still be in "screen coordinates" rather than pixels, but the window will
have a much greater pixel density when the device supports it, and the
SDL_GL_GetDrawableSize() or SDL_GetRendererOutputSize() functions (depending on
whether raw OpenGL or the SDL_Render API is used) can be queried to determine
the size in pixels of the drawable screen framebuffer.

Some OpenGL ES functions such as glViewport expect sizes in pixels rather than
sizes in screen coordinates. When doing 2D rendering with OpenGL ES, an
orthographic projection matrix using the size in screen coordinates
(SDL_GetWindowSize()) can be used in order to display content at the same scale
no matter whether a Retina device is used or not.


Notes -- Application events
==============================================================================

On iOS the application goes through a fixed life cycle and you will get
notifications of state changes via application events. When these events
are delivered you must handle them in an event callback because the OS may
not give you any processing time after the events are delivered.

e.g.

    int HandleAppEvents(void *userdata, SDL_Event *event)
    {
        switch (event->type)
        {
        case SDL_APP_TERMINATING:
            /* Terminate the app.
               Shut everything down before returning from this function.
            */
            return 0;
        case SDL_APP_LOWMEMORY:
            /* You will get this when your app is paused and iOS wants more memory.
               Release as much memory as possible.
            */
            return 0;
        case SDL_APP_WILLENTERBACKGROUND:
            /* Prepare your app to go into the background.  Stop loops, etc.
               This gets called when the user hits the home button, or gets a call.
            */
            return 0;
        case SDL_APP_DIDENTERBACKGROUND:
            /* This will get called if the user accepted whatever sent your app to the background.
               If the user got a phone call and canceled it, you'll instead get an SDL_APP_DIDENTERFOREGROUND event and restart your loops.
               When you get this, you have 5 seconds to save all your state or the app will be terminated.
               Your app is NOT active at this point.
            */
            return 0;
        case SDL_APP_WILLENTERFOREGROUND:
            /* This call happens when your app is coming back to the foreground.
               Restore all your state here.
            */
            return 0;
        case SDL_APP_DIDENTERFOREGROUND:
            /* Restart your loops here.
               Your app is interactive and getting CPU again.
            */
            return 0;
        default:
            /* No special processing, add it to the event queue */
            return 1;
        }
    }

    int main(int argc, char *argv[])
    {
        SDL_SetEventFilter(HandleAppEvents, NULL);

        ... run your main loop

        return 0;
    }


Notes -- Accelerometer as Joystick
==============================================================================

SDL for iPhone supports polling the built in accelerometer as a joystick device.  For an example on how to do this, see the accelerometer.c in the demos directory.

The main thing to note when using the accelerometer with SDL is that while the iPhone natively reports accelerometer as floating point values in units of g-force, SDL_JoystickGetAxis() reports joystick values as signed integers.  Hence, in order to convert between the two, some clamping and scaling is necessary on the part of the iPhone SDL joystick driver.  To convert SDL_JoystickGetAxis() reported values BACK to units of g-force, simply multiply the values by SDL_IPHONE_MAX_GFORCE / 0x7FFF.


Notes -- OpenGL ES
==============================================================================

Your SDL application for iOS uses OpenGL ES for video by default.

OpenGL ES for iOS supports several display pixel formats, such as RGBA8 and RGB565, which provide a 32 bit and 16 bit color buffer respectively. By default, the implementation uses RGB565, but you may use RGBA8 by setting each color component to 8 bits in SDL_GL_SetAttribute().

If your application doesn't use OpenGL's depth buffer, you may find significant performance improvement by setting SDL_GL_DEPTH_SIZE to 0.

Finally, if your application completely redraws the screen each frame, you may find significant performance improvement by setting the attribute SDL_GL_RETAINED_BACKING to 0.

OpenGL ES on iOS doesn't use the traditional system-framebuffer setup provided in other operating systems. Special care must be taken because of this:

- The drawable Renderbuffer must be bound to the GL_RENDERBUFFER binding point when SDL_GL_SwapWindow() is called.
- The drawable Framebuffer Object must be bound while rendering to the screen and when SDL_GL_SwapWindow() is called.
- If multisample antialiasing (MSAA) is used and glReadPixels is used on the screen, the drawable framebuffer must be resolved to the MSAA resolve framebuffer (via glBlitFramebuffer or glResolveMultisampleFramebufferAPPLE), and the MSAA resolve framebuffer must be bound to the GL_READ_FRAMEBUFFER binding point, before glReadPixels is called.

The above objects can be obtained via SDL_GetWindowWMInfo() (in SDL_syswm.h).


Notes -- Keyboard
==============================================================================

The SDL keyboard API has been extended to support on-screen keyboards:

void SDL_StartTextInput()
	-- enables text events and reveals the onscreen keyboard.

void SDL_StopTextInput()
	-- disables text events and hides the onscreen keyboard.

SDL_bool SDL_IsTextInputActive()
	-- returns whether or not text events are enabled (and the onscreen keyboard is visible)


Notes -- Mouse
==============================================================================

iOS now supports Bluetooth mice on iPad, but by default will provide the mouse input as touch. In order for SDL to see the real mouse events, you should set the key UIApplicationSupportsIndirectInputEvents to true in your Info.plist


Notes -- Reading and Writing files
==============================================================================

Each application installed on iPhone resides in a sandbox which includes its own Application Home directory.  Your application may not access files outside this directory.

Once your application is installed its directory tree looks like:

    MySDLApp Home/
        MySDLApp.app
        Documents/
        Library/
            Preferences/
        tmp/

When your SDL based iPhone application starts up, it sets the working directory to the main bundle (MySDLApp Home/MySDLApp.app), where your application resources are stored.  You cannot write to this directory.  Instead, I advise you to write document files to "../Documents/" and preferences to "../Library/Preferences".

More information on this subject is available here:
http://developer.apple.com/library/ios/#documentation/iPhone/Conceptual/iPhoneOSProgrammingGuide/Introduction/Introduction.html


Notes -- xcFramework
==============================================================================

The SDL.xcodeproj file now includes a target to build SDL2.xcframework. An xcframework is a new (Xcode 11) uber-framework which can handle any combination of processor type and target OS platform.

In the past, iOS devices were always an ARM variant processor, and the simulator was always i386 or x86_64, and thus libraries could be combined into a single framework for both simulator and device. With the introduction of the Apple Silicon ARM-based machines, regular frameworks would collide as CPU type was no longer sufficient to differentiate the platform. So Apple created the new xcframework library package.

The xcframework target builds into a Products directory alongside the SDL.xcodeproj file, as SDL2.xcframework. This can be brought in to any iOS project and will function properly for both simulator and device, no matter their CPUs. Note that Intel Macs cannot cross-compile for Apple Silicon Macs. If you need AS compatibility, perform this build on an Apple Silicon Mac.

This target requires Xcode 11 or later. The target will simply fail to build if attempted on older Xcodes.

In addition, on Apple platforms, main() cannot be in a dynamically loaded library. This means that iOS apps which used the statically-linked libSDL2.lib and now link with the xcframwork will need to define their own main() to call SDL_UIKitRunApp(), like this:

#ifndef SDL_MAIN_HANDLED
#ifdef main
#undef main
#endif

int
main(int argc, char *argv[])
{
	return SDL_UIKitRunApp(argc, argv, SDL_main);
}
#endif /* !SDL_MAIN_HANDLED */

Using an xcFramework is similar to using a regular framework. However, issues have been seen with the build system not seeing the headers in the xcFramework. To remedy this, add the path to the xcFramework in your app's target ==> Build Settings ==> Framework Search Paths and mark it recursive (this is critical). Also critical is to remove "*.framework" from Build Settings ==> Sub-Directories to Exclude in Recursive Searches. Clean the build folder, and on your next build the build system should be able to see any of these in your code, as expected:

#include "SDL_main.h"
#include <SDL.h>
#include <SDL_main.h>


Notes -- iPhone SDL limitations
==============================================================================

Windows:
	Full-size, single window applications only.  You cannot create multi-window SDL applications for iPhone OS.  The application window will fill the display, though you have the option of turning on or off the menu-bar (pass SDL_CreateWindow() the flag SDL_WINDOW_BORDERLESS).

Textures:
	The optimal texture formats on iOS are SDL_PIXELFORMAT_ABGR8888, SDL_PIXELFORMAT_ABGR8888, SDL_PIXELFORMAT_BGR888, and SDL_PIXELFORMAT_RGB24 pixel formats.

Loading Shared Objects:
	This is disabled by default since it seems to break the terms of the iOS SDK agreement for iOS versions prior to iOS 8. It can be re-enabled in SDL_config_iphoneos.h.


Notes -- CoreBluetooth.framework
==============================================================================

SDL_JOYSTICK_HIDAPI is disabled by default. It can give you access to a lot
more game controller devices, but it requires permission from the user before
your app will be able to talk to the Bluetooth hardware. "Made For iOS"
branded controllers do not need this as we don't have to speak to them
directly with raw bluetooth, so many apps can live without this.

You'll need to link with CoreBluetooth.framework and add something like this
to your Info.plist:

<key>NSBluetoothPeripheralUsageDescription</key>
<string>MyApp would like to remain connected to nearby bluetooth Game Controllers and Game Pads even when you're not using the app.</string>


Game Center
==============================================================================

Game Center integration might require that you break up your main loop in order to yield control back to the system. In other words, instead of running an endless main loop, you run each frame in a callback function, using:

    int SDL_iPhoneSetAnimationCallback(SDL_Window * window, int interval, SDL_iOSAnimationCallback callback, void *callbackParam);

This will set up the given function to be called back on the animation callback, and then you have to return from main() to let the Cocoa event loop run.

e.g.

    extern "C"
    void ShowFrame(void*)
    {
        ... do event handling, frame logic and rendering ...
    }

    int main(int argc, char *argv[])
    {
        ... initialize game ...

    #ifdef __IPHONEOS__
        // Initialize the Game Center for scoring and matchmaking
        InitGameCenter();

        // Set up the game to run in the window animation callback on iOS
        // so that Game Center and so forth works correctly.
        SDL_iPhoneSetAnimationCallback(window, 1, ShowFrame, NULL);
    #else
        while ( running ) {
            ShowFrame(0);
            DelayFrame();
        }
    #endif
        return 0;
    }


Deploying to older versions of iOS
==============================================================================

SDL supports deploying to older versions of iOS than are supported by the latest version of Xcode, all the way back to iOS 8.0

In order to do that you need to download an older version of Xcode:
https://developer.apple.com/download/more/?name=Xcode

Open the package contents of the older Xcode and your newer version of Xcode and copy over the folders in Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/DeviceSupport

Then open the file Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk/SDKSettings.plist and add the versions of iOS you want to deploy to the key Root/DefaultProperties/DEPLOYMENT_TARGET_SUGGESTED_VALUES

Open your project and set your deployment target to the desired version of iOS

Finally, remove GameController from the list of frameworks linked by your application and edit the build settings for "Other Linker Flags" and add -weak_framework GameController
