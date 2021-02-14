Using SDL with Microsoft Visual C++
===================================

### by [Lion Kimbro](mailto:snowlion@sprynet.com) with additions by [James Turk](mailto:james@conceptofzero.net)

You can either use the precompiled libraries from the [SDL](https://www.libsdl.org/download.php) web site, or you can build SDL
yourself.

### Building SDL

0. To build SDL, your machine must, at a minimum, have the DirectX9.0c SDK installed. It may or may not be retrievable from
the [Microsoft](https://www.microsoft.com) website, so you might need to locate it [online](https://duckduckgo.com/?q=directx9.0c+sdk+download&t=h_&ia=web.
_Editor's note: I've been able to successfully build SDL using Visual Studio 2019 without the DX9.0c SDK_

1. Open the Visual Studio solution file at `./VisualC/SDL.sln`.

2. Your IDE will likely prompt you to upgrade this solution file to whatever later version of the IDE you're using. In the `Retarget Projects` dialog,
all of the affected project files should be checked allowing you to use the latest `Windows SDK Version` you have installed, along with
the `Platform Toolset`.
   
If you choose NOT to upgrade to use the latest `Windows SDK Version` or `Platform Toolset`, then you'll need the `Visual Studio 2010 Platform Toolset`.

3. Build the `.dll` and `.lib` files by right clicking on each project in turn (Projects are listed in the _Workspace_ 
panel in the _FileView_ tab), and selecting `Build`.

You may get a few warnings, but you should not get any errors.

Later, we will refer to the following `.lib` and `.dll` files that have just been generated:

-   `SDL2.dll`
-   `SDL2.lib`
-   `SDL2main.lib`

Search for these using the Windows Find (Windows-F) utility inside the `VisualC` directory.

### Creating a Project with SDL

- Create a project as a `Win32 Application`.

- Create a C++ file for your project.

- Set the C runtime to `Multi-threaded DLL` in the menu:
`Project|Settings|C/C++                  tab|Code Generation|Runtime Library `.

- Add the SDL `include` directory to your list of includes in the menu:
`Project|Settings|C/C++ tab|Preprocessor|Additional include directories `

 ** VC7 Specific: Instead of doing this, I find it easier to add the
include and library directories to the list that VC7 keeps. Do this by
selecting Tools|Options|Projects|VC++ Directories and under the "Show
Directories For:" dropbox select "Include Files", and click the "New
Directory Icon" and add the [SDLROOT]\\include directory (e.g. If you
installed to c:\\SDL\\ add c:\\SDL\\include). Proceed to change the
dropbox selection to "Library Files" and add [SDLROOT]\\lib.**

The `include directory` I am referring to is the `include` folder within the main SDL directory (the one that this HTML file located within).

Now we're going to use the files that we had created earlier in the Build SDL step.

Copy the following file into your Project directory:

-   `SDL2.dll`

Add the following files to your project (It is not necessary to copy them to your project directory):

-   `SDL2.lib`
-   `SDL2main.lib`

To add them to your project, right click on your project, and select
`Add files to project`.

**Instead of adding the files to your project, it is more desirable to
add them to the linker options: Project|Properties|Linker|Command Line
and type the names of the libraries to link with in the "Additional
Options:" box. Note: This must be done for each build configuration
(e.g. Release,Debug).**

### SDL 101, First Day of Class

Now create the basic body of your project. The body of your program
should take the following form:

```
    #include "SDL.h"

    int main( int argc, char* argv[] )
    {
      // Body of the program goes here.
      return 0;
    }
 ```               

### That's it!

I hope that this document has helped you get through the most difficult part of using the SDL: installing it. Suggestions for improvements to
this document should be sent to the writers of this document.

### Credits

Thanks to [Paulus Esterhazy](mailto:pesterhazy@gmx.net), for the work on VC++ port.

This document was originally called "VisualC.txt", and was written by [Sam Lantinga](mailto:slouken@libsdl.org).

Later, it was converted to HTML and expanded into the document that you see today by [Lion Kimbro](mailto:snowlion@sprynet.com).

Minor Fixes and Visual C++ 7 Information (In Green) was added by [James Turk](mailto:james@conceptofzero.net)
