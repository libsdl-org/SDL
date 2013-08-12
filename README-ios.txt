==============================================================================
Building the Simple DirectMedia Layer for iPhone OS 2.0
==============================================================================

Requirements: Mac OS X v10.5 or later and the iPhone SDK.

Instructions:
1.  Open SDL.xcodeproj (located in Xcode-iOS/SDL) in XCode.
2.  Select your desired target, and hit build.

There are three build targets:
- libSDL.a:
	Build SDL as a statically linked library
- testsdl
	Build a test program (there are known test failures which are fine)
- Template:
	Package a project template together with the SDL for iPhone static libraries and copies of the SDL headers.  The template includes proper references to the SDL library and headers, skeleton code for a basic SDL program, and placeholder graphics for the application icon and startup screen.

==============================================================================
Build SDL for iOS from the command line
==============================================================================

1. cd (PATH WHERE THE SDL CODE IS)/build-scripts
2. ./iosbuild.sh

If everything goes fine, you should see a build/ios directory, inside there's
two directories "lib" and "include". 
"include" contains a copy of the SDL headers that you'll need for your project,
make sure to configure XCode to look for headers there.
"lib" contains find two files, libSDL2.a and libSDL2main.a, you have to add both 
to your XCode project. These libraries contain three architectures in them,
armv6 for legacy devices, armv7, and i386 (for the simulator).
By default, iosbuild.sh will autodetect the SDK version you have installed using 
xcodebuild -showsdks, and build for iOS >= 3.0, you can override this behaviour 
by setting the MIN_OS_VERSION variable, ie:

MIN_OS_VERSION=4.2 ./iosbuild.sh

==============================================================================
Using the Simple DirectMedia Layer for iOS
==============================================================================

FIXME: This needs to be updated for the latest methods

Here is the easiest method:
1.  Build the SDL libraries (libSDL.a and libSDLSimulator.a) and the iPhone SDL Application template.
1.  Install the iPhone SDL Application template by copying it to one of XCode's template directories.  I recommend creating a directory called "SDL" in "/Developer/Platforms/iOS.platform/Developer/Library/XCode/Project Templates/" and placing it there.
2.  Start a new project using the template.  The project should be immediately ready for use with SDL.

Here is a more manual method:
1.  Create a new iPhone view based application.
2.  Build the SDL static libraries (libSDL.a and libSDLSimulator.a) for iPhone and include them in your project.  XCode will ignore the library that is not currently of the correct architecture, hence your app will work both on iPhone and in the iPhone Simulator.
3.  Include the SDL header files in your project.
4.  Remove the ApplicationDelegate.h and ApplicationDelegate.m files -- SDL for iPhone provides its own UIApplicationDelegate.  Remove MainWindow.xib -- SDL for iPhone produces its user interface programmatically.
5.  Delete the contents of main.m and program your app as a regular SDL program instead.  You may replace main.m with your own main.c, but you must tell XCode not to use the project prefix file, as it includes Objective-C code.

==============================================================================
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

    
==============================================================================
Notes -- Accelerometer as Joystick
==============================================================================

SDL for iPhone supports polling the built in accelerometer as a joystick device.  For an example on how to do this, see the accelerometer.c in the demos directory.

The main thing to note when using the accelerometer with SDL is that while the iPhone natively reports accelerometer as floating point values in units of g-force, SDL_JoystickGetAxis reports joystick values as signed integers.  Hence, in order to convert between the two, some clamping and scaling is necessary on the part of the iPhone SDL joystick driver.  To convert SDL_JoystickGetAxis reported values BACK to units of g-force, simply multiply the values by SDL_IPHONE_MAX_GFORCE / 0x7FFF.

==============================================================================
Notes -- OpenGL ES
==============================================================================

Your SDL application for iPhone uses OpenGL ES for video by default.

OpenGL ES for iPhone supports several display pixel formats, such as RGBA8 and RGB565, which provide a 32 bit and 16 bit color buffer respectively.  By default, the implementation uses RGB565, but you may use RGBA8 by setting each color component to 8 bits in SDL_GL_SetAttribute.

If your application doesn't use OpenGL's depth buffer, you may find significant performance improvement by setting SDL_GL_DEPTH_SIZE to 0.

Finally, if your application completely redraws the screen each frame, you may find significant performance improvement by setting the attribute SDL_GL_RETAINED_BACKING to 1.

==============================================================================
Notes -- Keyboard
==============================================================================

The SDL keyboard API has been extended to support on-screen keyboards:

void SDL_StartTextInput()
	-- enables text events and reveals the onscreen keyboard.
void SDL_StopTextInput()
	-- disables text events and hides the onscreen keyboard.
SDL_bool SDL_IsTextInputActive()
	-- returns whether or not text events are enabled (and the onscreen keyboard is visible)

==============================================================================
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

==============================================================================
Notes -- iPhone SDL limitations
==============================================================================

Windows:
	Full-size, single window applications only.  You cannot create multi-window SDL applications for iPhone OS.  The application window will fill the display, though you have the option of turning on or off the menu-bar (pass SDL_CreateWindow the flag SDL_WINDOW_BORDERLESS).

Textures:
	The optimal texture formats on iOS are SDL_PIXELFORMAT_ABGR8888, SDL_PIXELFORMAT_ABGR8888, SDL_PIXELFORMAT_BGR888, and SDL_PIXELFORMAT_RGB24 pixel formats.

Loading Shared Objects:
	This is disabled by default since it seems to break the terms of the iPhone SDK agreement.  It can be re-enabled in SDL_config_iphoneos.h.

==============================================================================
Game Center 
==============================================================================

Game Center integration requires that you break up your main loop in order to yield control back to the system. In other words, instead of running an endless main loop, you run each frame in a callback function, using:
    
int SDL_iPhoneSetAnimationCallback(SDL_Window * window, int interval, void (*callback)(void*), void *callbackParam);

This will set up the given function to be called back on the animation callback, and then you have to return from main() to let the Cocoa event loop run.

e.g.

extern "C"
void ShowFrame(void*)
{
    ... do event handling, frame logic and rendering
}

int main(int argc, char *argv[])
{
   ... initialize game ...

#if __IPHONEOS__
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
