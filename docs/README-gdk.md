GDK
=====

This port allows SDL applications to run via Microsoft's Game Development Kit (GDK).

Windows (GDK) and  Xbox One/Xbox Series (GDKX) are supported. Although most of the Xbox code is included in the public SDL source code, NDA access is required for a small number of source files. If you have access to GDKX, these required Xbox files are posted on the GDK forums [here](https://forums.xboxlive.com/questions/130003/).


Requirements
------------

* Microsoft Visual Studio 2022 (in theory, it should also work in 2017 or 2019, but this has not been tested)
* Microsoft GDK June 2022 or newer (public release [here](https://github.com/microsoft/GDK/releases/tag/June_2022))
* To publish a package or successfully authenticate a user, you will need to create an app id/configure services in Partner Center. However, for local testing purposes (without authenticating on Xbox Live), the identifiers used by the GDK test programs in the included solution will work.


Windows GDK Status
------

The Windows GDK port supports the full set of Win32 APIs, renderers, controllers, input devices, etc., as the normal Windows x64 build of SDL.

* Additionally, the GDK port adds the following:
  * Compile-time platform detection for SDL programs. The `__GDK__` is `#define`d on every GDK platform, and the  `__WINGDK__` is `#define`d on Windows GDK, specifically. (This distinction exists because other GDK platforms support a smaller subset of functionality. This allows you to mark code for "any" GDK separate from Windows GDK.)
  * GDK-specific setup:
    * Initializing/uninitializing the game runtime, and initializing Xbox Live services
    * Creating a global task queue and setting it as the default for the process. When running any async operations, passing in `NULL` as the task queue will make the task get added to the global task queue.
  
  * An implementation on `WinMain` that performs the above GDK setup that you can use by #include'ing SDL_main.h in the source file that includes your standard main() function. If you are unable to do this, you can instead manually call `SDL_RunApp` from your entry point, passing in your `SDL_main` function and `NULL` as the parameters. To use `SDL_RunApp`, `#define SDL_MAIN_HANDLED` before `#include <SDL3/SDL_main.h>`.
  * Global task queue callbacks are dispatched during `SDL_PumpEvents` (which is also called internally if using `SDL_PollEvent`).
  * You can get the handle of the global task queue through `SDL_GDKGetTaskQueue`, if needed. When done with the queue, be sure to use `XTaskQueueCloseHandle` to decrement the reference count (otherwise it will cause a resource leak).
  
* What doesn't work:
  * Compilation with anything other than through the included Visual C++ solution file

## VisualC-GDK Solution

The included `VisualC-GDK/SDL.sln` solution includes the following targets for the Gaming.Desktop.x64 configuration:

* SDL3 (DLL) - This is the typical SDL3.dll, but for Gaming.Desktop.x64.
* tests/testgamecontroller - Standard SDL test program demonstrating controller functionality.
* tests/testgdk - GDK-specific test program that demonstrates using the global task queue to login a user into Xbox Live.
  *NOTE*: As of the June 2022 GDK, you cannot test user logins without a valid Title ID and MSAAppId. You will need to manually change the identifiers in the `MicrosoftGame.config` to your valid IDs from Partner Center if you wish to test this.
* tests/testsprite - Standard SDL test program demonstrating sprite drawing functionality.

If you set one of the test programs as a startup project, you can run it directly from Visual Studio.

Windows GDK Setup, Detailed Steps
---------------------

These steps assume you already have a game using SDL that runs on Windows x64 along with a corresponding Visual Studio solution file for the x64 version. If you don't have this, it's easiest to use one of the test program vcxproj files in the `VisualC-GDK` directory as a starting point, though you will still need to do most of the steps below.

### 1. Add a Gaming.Desktop.x64 Configuration ###

In your game's existing Visual Studio Solution, go to Build > Configuration Manager. From the "Active solution platform" drop-down select "New...". From the drop-down list, select Gaming.Desktop.x64 and copy the settings from the x64 configuration.

### 2. Build SDL3 for GDK ###

Open `VisualC-GDK/SDL.sln` in Visual Studio, you need to build the SDL3 target for the Gaming.Desktop.x64 platform (Release is recommended). You will need to copy/keep track of the `SDL3.dll`, `XCurl.dll` (which is output by Gaming.Desktop.x64), and `SDL3.lib` output files for your game project.

*Alternatively*, you could setup your solution file to instead reference the SDL3 project file targets from the SDL source, and add those projects as a dependency. This would mean that SDL3 would be built when your game is built.

### 3. Configuring Project Settings ###

While the Gaming.Desktop.x64 configuration sets most of the required settings, there are some additional items to configure for your game project under the Gaming.Desktop.x64 Configuration:

*  Under C/C++ > General > Additional Include Directories, make sure the `SDL/include` path is referenced
* Under Linker > General > Additional Library Directories, make sure to reference the path where the newly-built SDL3.lib are
* Under Linker > Input > Additional Dependencies, you need the following:
  * `SDL3.lib`
  * `xgameruntime.lib`
  * `../Microsoft.Xbox.Services.141.GDK.C.Thunks.lib`
* Note that in general, the GDK libraries depend on the MSVC C/C++ runtime, so there is no way to remove this dependency from a GDK program that links against GDK.

### 4. Setting up SDL_main ###

Rather than using your own implementation of `WinMain`, it's recommended that you instead `#include <SDL3/SDL_main.h>` and declare a standard main function. If you are unable to do this, you can instead manually call `SDL_RunApp` from your entry point, passing in your `SDL_main` function and `NULL` as the parameters; in that case `#define SDL_MAIN_HANDLED` before including SDL_main.h

### 5. Required DLLs ###

The game will not launch in the debugger unless required DLLs are included in the directory that contains the game's .exe file. You need to make sure that the following files are copied into the directory:

* Your SDL3.dll
* "$(Console_GRDKExtLibRoot)Xbox.Services.API.C\DesignTime\CommonConfiguration\Neutral\Lib\Release\Microsoft.Xbox.Services.141.GDK.C.Thunks.dll"
* XCurl.dll

You can either copy these in a post-build step, or you can add the dlls into the project and set its Configuration Properties > General > Item type to "Copy file," which will also copy them into the output directory.

### 6. Setting up MicrosoftGame.config ###

You can copy `VisualC-GDK/tests/testgdk/MicrosoftGame.config` and use that as a starting point in your project. Minimally, you will want to change the Executable Name attribute, the DefaultDisplayName, and the Description.

This file must be copied into the same directory as the game's .exe file. As with the DLLs, you can either use a post-build step or the "Copy file" item type.

For basic testing, you do not need to change anything else in `MicrosoftGame.config`. However, if you want to test any Xbox Live services (such as logging in users) _or_ publish a package, you will need to setup a Game app on Partner Center.

Then, you need to set the following values to the values from Partner Center:

* Identity tag - Name and Publisher attributes
* TitleId
* MSAAppId

### 7. Adding Required Logos

Several logo PNG files are required to be able to launch the game, even from the debugger. You can use the sample logos provided in `VisualC-GDK/logos`. As with the other files, they must be copied into the same directory as the game's .exe file.


### 8. Copying any Data Files ###

When debugging GDK games, there is no way to specify a working directory. Therefore, any required game data must also be copied into the output directory, likely in a post-build step.


### 9. Build and Run from Visual Studio ###

At this point, you should be able to build and run your game from the Visual Studio Debugger. If you get any linker errors, make sure you double-check that you referenced all the required libs.

If you are testing Xbox Live functionality, it's likely you will need to change to the Sandbox for your title. To do this:

1. Run "Desktop VS 2022 Gaming Command Prompt" from the Start Menu
2. Switch the sandbox name with:
   `XblPCSandbox SANDBOX.#`
3. (To switch back to the retail sandbox):
   `XblPCSandbox RETAIL`

### 10. Packaging and Installing Locally

You can use one of the test program's `PackageLayout.xml` as a starting point. Minimally, you will need to change the exe to the correct name and also reference any required game data. As with the other data files, it's easiest if you have this copy to the output directory, although it's not a requirement as you can specify relative paths to files.

To create the package:

1. Run "Desktop VS 2022 Gaming Command Prompt" from the Start Menu
2. `cd` to the directory containing the `PackageLayout.xml` with the correct paths (if you use the local path as in the sample package layout, this would be from your .exe output directory)
3. `mkdir Package` to create an output directory
4. To package the file into the `Package` directory, use:
    `makepkg pack /f PackageLayout.xml /lt /d . /nogameos /pc /pd Package`
5. To install the package, use:
   `wdapp install PACKAGENAME.msixvc`
6. Once the package is installed, you can run it from the start menu.
7. As with when running from Visual Studio, if you need to test any Xbox Live functionality you must switch to the correct sandbox.


Troubleshooting
---------------

#### Xbox Live Login does not work

As of June 2022 GDK, you must have a valid Title Id and MSAAppId in order to test Xbox Live functionality such as user login. Make sure these are set correctly in the `MicrosoftGame.config`. This means that even testgdk will not let you login without setting these properties to valid values.

Furthermore, confirm that your PC is set to the correct sandbox.


#### "The current user has already installed an unpackaged version of this app. A packaged version cannot replace this." error when installing

Prior to June 2022 GDK, running from the Visual Studio debugger would still locally register the app (and it would appear on the start menu). To fix this, you have to uninstall it (it's simplest to right click on it from the start menu to uninstall it).
