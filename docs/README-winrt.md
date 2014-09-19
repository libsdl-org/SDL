WinRT
=====

This port allows SDL applications to run on Microsoft's platforms that require
use of "Windows Runtime", aka. "WinRT", APIs.  WinRT apps are currently
full-screen only, and run in what Microsoft sometimes refers to as their
"Modern" (formerly, "Metro"), environment.  For Windows 8.x, Microsoft may also
refer to them as "Windows Store" apps, due to them being distributed,
primarily, via a Microsoft-run online store (of the same name).

Some of the operating systems that include WinRT, are:

* Windows 8.x
* Windows RT 8.x (aka. Windows 8.x for ARM processors)
* Windows Phone 8.x


Requirements
------------

* Microsoft Visual C++ (aka Visual Studio), either 2013 or 2012 versions
  - Free, "Express" editions may be used, so long as they include support for 
    either "Windows Store" or "Windows Phone" apps.  Versions marked as
    supporting "Windows Desktop" development typically do not include support
    for creating WinRT apps.
  - Visual C++ 2012 can only build apps that target versions 8.0 of Windows, or 
    Windows Phone.  8.0-targetted apps will still run on devices running 
    8.1 editions of Windows, however they will not be able to take advantage of 
    8.1-specific features.
  - Visual C++ 2013 can only create app projects that target 8.1 versions
    of Windows, which do NOT run on 8.0 devices.  An optional Visual Studio
    add-in, "Tools for Maintaining Store apps for Windows 8", allows projects
    that are created with Visual C++ 2012, which can create Windows 8.0 apps,
    to be loaded and built with non-Express editions of Visual C++ 2013.  More
    details on targeting different versions of Windows can found at the
    following web pages:
      - [Develop apps by using Visual Studio 2013](http://msdn.microsoft.com/en-us/library/windows/apps/br211384.aspx)
      - [To add the Tools for Maintaining Store apps for Windows 8](http://msdn.microsoft.com/en-us/library/windows/apps/dn263114.aspx#AddMaintenanceTools)
* A valid Microsoft account - This requirement is not imposed by SDL, but
  rather by Microsoft's Visual C++ toolchain.  This is required to launch or 
  debug apps.


Setup, High-Level Steps
-----------------------

The steps for setting up a project for an SDL/WinRT app looks like the
following, at a high-level:

1. create a new Visual C++ project using Microsoft's template for a,
   "Direct3D App".
2. remove most of the files from the project.
3. make your app's project directly reference SDL/WinRT's own Visual C++
   project file, via use of Visual C++'s "References" dialog.  This will setup
   the linker, and will copy SDL's .dll files to your app's final output.
4. adjust your app's build settings, at minimum, telling it where to find SDL's
   header files.
5. add a file that contains a WinRT-appropriate main function.
6. add SDL-specific app code.
7. build and run your app.


Setup, Detailed Steps
---------------------

### 1. Create a new project ###

Create a new project using one of Visual C++'s templates for a plain, non-XAML,
"Direct3D App" (XAML support for SDL/WinRT is not yet ready for use).  If you
don't see one of these templates, in Visual C++'s 'New Project' dialog, try
using the textbox titled, 'Search Installed Templates' to look for one.


### 2. Remove unneeded files from the project ###

In the new project, delete any file that has one of the following extensions:

- .cpp
- .h
- .hlsl

When you are done, you should be left with a few files, each of which will be a
necessary part of your app's project.  These files will consist of:

- an .appxmanifest file, which contains metadata on your WinRT app.  This is
  similar to an Info.plist file on iOS, or an AndroidManifest.xml on Android.
- a few .png files, one of which is a splash screen (displayed when your app
  launches), others are app icons.
- a .pfx file, used for code signing purposes.


### 3. Add references to SDL's project files ###

SDL/WinRT can be built in multiple variations, spanning across three different
CPU architectures (x86, x64, and ARM) and two different configurations
(Debug and Release).  WinRT and Visual C++ do not currently provide a means
for combining multiple variations of one library into a single file.
Furthermore, it does not provide an easy means for copying pre-built .dll files
into your app's final output (via Post-Build steps, for example).  It does,
however, provide a system whereby an app can reference the MSVC projects of
libraries such that, when the app is built:

1. each library gets built for the appropriate CPU architecture(s) and WinRT
   platform(s).
2. each library's output, such as .dll files, get copied to the app's build 
   output.

To set this up for SDL/WinRT, you'll need to run through the following steps:

1. open up the Solution Explorer inside Visual C++ (under the "View" menu, then
   "Solution Explorer")
2. right click on your app's solution.
3. navigate to "Add", then to "Existing Project..."
4. find SDL/WinRT's Visual C++ project file and open it.  Different project
   files exist for different WinRT platforms.  All of them are in SDL's
   source distribution, in the following directories:
    * `VisualC-WinRT/WinPhone80_VS2012/` - for Windows Phone 8.0 apps
    * `VisualC-WinRT/WinPhone81_VS2013/` - for Windows Phone 8.1 apps
    * `VisualC-WinRT/WinRT80_VS2012/` - for Windows 8.0 apps
    * `VisualC-WinRT/WinRT81_VS2013/` - for Windows 8.1 apps
5. once the project has been added, right-click on your app's project and
   select, "References..."
6. click on the button titled, "Add New Reference..."
7. check the box next to SDL
8. click OK to close the dialog
9. SDL will now show up in the list of references.  Click OK to close that
   dialog.

Your project is now linked to SDL's project, insofar that when the app is
built, SDL will be built as well, with its build output getting included with
your app.


### 4. Adjust Your App's Build Settings ###

Some build settings need to be changed in your app's project.  This guide will
outline the following:

- making sure that the compiler knows where to find SDL's header files
- **Optional for C++, but NECESSARY for compiling C code:** telling the
  compiler not to use Microsoft's C++ extensions for WinRT development.
- **Optional:** telling the compiler not generate errors due to missing
  precompiled header files.

To change these settings:

1. right-click on the project
2. choose "Properties"
3. in the drop-down box next to "Configuration", choose, "All Configurations"
4. in the drop-down box next to "Platform", choose, "All Platforms"
5. in the left-hand list, expand the "C/C++" section
6. select "General"
7. edit the "Additional Include Directories" setting, and add a path to SDL's
   "include" directory
8. **Optional: to enable compilation of C code:** change the setting for
   "Consume Windows Runtime Extension" from "Yes (/ZW)" to "No".  If you're 
   working with a completely C++ based project, this step can usually be 
   omitted.
9. **Optional: to disable precompiled headers (which can produce 
   'stdafx.h'-related build errors, if setup incorrectly:** in the left-hand 
   list, select "Precompiled Headers", then change the setting for "Precompiled 
   Header" from "Use (/Yu)" to "Not Using Precompiled Headers".
10. close the dialog, saving settings, by clicking the "OK" button


### 5. Add a WinRT-appropriate main function to the app. ###

C/C++-based WinRT apps do contain a `main` function that the OS will invoke when 
the app starts launching. The parameters of WinRT main functions are different 
than those found on other platforms, Win32 included.  SDL/WinRT provides a 
platform-appropriate main function that will perform these actions, setup key 
portions of the app, then invoke a classic, C/C++-style main function (that take 
in "argc" and "argv" parameters).  The code for this file is contained inside 
SDL's source distribution, under `src/main/winrt/SDL_winrt_main_NonXAML.cpp`.  
You'll need to add this file, or a copy of it, to your app's project, and make 
sure it gets compiled using a Microsoft-specific set of C++ extensions called 
C++/CX.

**NOTE: C++/CX compilation is currently required in at least one file of your 
app's project.  This is to make sure that Visual C++'s linker builds a 'Windows 
Metadata' file (.winmd) for your app.  Not doing so can lead to build errors.**

To include `SDL_winrt_main_NonXAML.cpp`:

1. right-click on your project (again, in Visual C++'s Solution Explorer), 
   navigate to "Add", then choose "Existing Item...".
2. open `SDL_winrt_main_NonXAML.cpp`, which is found inside SDL's source 
   distribution, under `src/main/winrt/`.  Make sure that the open-file dialog 
   closes, either by double-clicking on the file, or single-clicking on it and 
   then clicking Add.
3. right-click on the file (as listed in your project), then click on 
   "Properties...".
4. in the drop-down box next to "Configuration", choose, "All Configurations"
5. in the drop-down box next to "Platform", choose, "All Platforms"
6. in the left-hand list, click on "C/C++"
7. change the setting for "Consume Windows Runtime Extension" to "Yes (/ZW)".
8. click the OK button.  This will close the dialog.


### 6. Add app code and assets ###

At this point, you can add in SDL-specific source code.  Be sure to include a 
C-style main function (ie: `int main(int argc, char *argv[])`).  From there you 
should be able to create a single `SDL_Window` (WinRT apps can only have one 
window, at present), as well as an `SDL_Renderer`.  Direct3D will be used to 
draw content.  Events are received via SDL's usual event functions 
(`SDL_PollEvent`, etc.)  If you have a set of existing source files and assets, 
you can start adding them to the project now.  If not, or if you would like to 
make sure that you're setup correctly, some short and simple sample code is 
provided below.


#### 6.A. ... when creating a new app ####

If you are creating a new app (rather than porting an existing SDL-based app), 
or if you would just like a simple app to test SDL/WinRT with before trying to 
get existing code working, some working SDL/WinRT code is provided below.  To 
set this up:

1. right click on your app's project
2. select Add, then New Item.  An "Add New Item" dialog will show up.
3. from the left-hand list, choose "Visual C++"
4. from the middle/main list, choose "C++ File (.cpp)"
5. near the bottom of the dialog, next to "Name:", type in a name for your 
source file, such as, "main.cpp".
6. click on the Add button.  This will close the dialog, add the new file to 
your project, and open the file in Visual C++'s text editor.
7. Copy and paste the following code into the new file, then save it.


    #include <SDL.h>
    
    int main(int argc, char **argv)
    {
        SDL_DisplayMode mode;
        SDL_Window * window = NULL;
        SDL_Renderer * renderer = NULL;
        SDL_Event evt;
    
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return 1;
        }
    
        if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
            return 1;
        }
    
        if (SDL_CreateWindowAndRenderer(mode.w, mode.h, SDL_WINDOW_FULLSCREEN, &window, &renderer) != 0) {
            return 1;
        }
    
        while (1) {
            while (SDL_PollEvent(&evt)) {
            }
    
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }
    }


#### 6.B. Adding code and assets ####

If you have existing code and assets that you'd like to add, you should be able 
to add them now.  The process for adding a set of files is as such.

1. right click on the app's project
2. select Add, then click on "New Item..."
3. open any source, header, or asset files as appropriate.  Support for C and 
C++ is available.

Do note that WinRT only supports a subset of the APIs that are available to 
Win32-based apps.  Many portions of the Win32 API and the C runtime are not 
available.

A list of unsupported C APIs can be found at 
<http://msdn.microsoft.com/en-us/library/windows/apps/jj606124.aspx>

General information on using the C runtime in WinRT can be found at 
<http://msdn.microsoft.com/en-us/LIBRARY/hh972425(v=vs.110).aspx>

A list of supported Win32 APIs for Windows 8/RT apps can be found at 
<http://msdn.microsoft.com/en-us/library/windows/apps/br205757.aspx>.  To note, 
the list of supported Win32 APIs for Windows Phone 8 development is different.  
That list can be found at 
<http://msdn.microsoft.com/en-us/library/windowsphone/develop/jj662956(v=vs.105).aspx>


### 7. Build and run your app ###

Your app project should now be setup, and you should be ready to build your app.  
To run it on the local machine, open the Debug menu and choose "Start 
Debugging".  This will build your app, then run your app full-screen.  To switch 
out of your app, press the Windows key.  Alternatively, you can choose to run 
your app in a window.  To do this, before building and running your app, find 
the drop-down menu in Visual C++'s toolbar that says, "Local Machine".  Expand 
this by clicking on the arrow on the right side of the list, then click on 
Simulator.  Once you do that, any time you build and run the app, the app will 
launch in window, rather than full-screen.


#### 7.A. Running apps on ARM-based devices ####

To build and run the app on ARM-based, "Windows RT" devices, you'll need to:

- install Microsoft's "Remote Debugger" on the device.  Visual C++ installs and 
  debugs ARM-based apps via IP networks.
- change a few options on the development machine, both to make sure it builds 
  for ARM (rather than x86 or x64), and to make sure it knows how to find the 
  Windows RT device (on the network).

Microsoft's Remote Debugger can be found at 
<http://msdn.microsoft.com/en-us/library/vstudio/bt727f1t.aspx>.  Please note 
that separate versions of this debugger exist for different versions of Visual 
C++, one for debugging with MSVC 2012, another for debugging with MSVC 2013.

To setup Visual C++ to launch your app on an ARM device:

1. make sure the Remote Debugger is running on your ARM device, and that it's on 
   the same IP network as your development machine.
2. from Visual C++'s toolbar, find a drop-down menu that says, "Win32".  Click 
   it, then change the value to "ARM".
3. make sure Visual C++ knows the hostname or IP address of the ARM device.  To 
   do this:
    1. open the app project's properties
    2. select "Debugging"
    3. next to "Machine Name", enter the hostname or IP address of the ARM 
       device
    4. if, and only if, you've turned off authentication in the Remote Debugger, then change the setting for "Require Authentication" to No
    5. click "OK"
4. build and run the app (from Visual C++).  The first time you do this, a 
   prompt will show up on the ARM device, asking for a Microsoft Account.  You 
   do, unfortunately, need to log in here, and will need to follow the 
   subsequent registration steps in order to launch the app.  After you do so, 
   if the app didn't already launch, try relaunching it again from within Visual 
   C++.


TODO
----

- Document details of SDL satellite library support
- Make [NuGet](https://www.nuget.org) packages for SDL/WinRT
- Create templates for both MSVC 2012 and MSVC 2013, and have the corresponding
  VSIX packages either include pre-built copies of SDL, or reference binaries
  available via MSVC's NuGet servers
    - Write setup instructions that use MSVC 201x templates
- Write a list of caveats found in SDL/WinRT, such as APIs that don't work due
  to platform restrictions, or things that need further work
