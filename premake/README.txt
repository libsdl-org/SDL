Author: Ben Henning <b.henning@digipen.edu>

The goal of this project is to provide a lightweight and portable meta-build
system for generating build systems for various platforms and architectures, all
for the SDL2 library and subsequently dependent executables.

Following is a table of contents for the entire README file.

[0] OVERVIEW
[1] GENERATING PROJECTS AND COMMAND-LINE OPTIONS
[2] STRUCTURE
[3] SUPPORT ON WINDOWS AND VISUAL STUDIO
[4] SUPPORT ON MAC OS X AND XCODE
[5] SUPPORT FOR IOS
[6] SUPPORT FOR LINUX
[7] SUPPORT FOR MINGW
[8] SUPPORT FOR CYGWIN
[9] EXTENDING THE SYSTEM TO NEW PROJECTS OR PLATFORMS (code samples)

[0] OVERVIEW

The system is capable of generating projects for many different platforms and
architectures. How to generically generate projects is described in the next
section. Subsequent sections thereafter describe more specific ways to generate
projects and dependencies projects have.

All of the projects inherently have things in common, such as depending on the
same source tree for header and source files. All projects generated will also
have both debug and release configurations available to be built. More
information on how to build either will be provided below.

To view a list of progress on the project, view the changelog.

[1] GENERATING PROJECTS AND COMMAND-LINE OPTIONS

To receive help with various premake actions and command-line options, or to
view the options available for the current premake environment, run the
following command:

    ./premake4 --file=./path/to/premake4.lua help

To construct the project files, run this local command from any command line:

    .\premake4 --file=.\path\to\premake4.lua --to=.\resultDirectory [opts] [vs2008/vs2010/vs2012]
OR
    ./premake4 --file=./path/to/premake4.lua --to=./resultDirectory [opts] [xcode3/xcode4/gmake]

opts may be one of:
  --mingw
  --cygwin
  --ios

opts may also include any of the following:
  --alsa        :  Force the ALSA dependency on for Linux targets.
  --dbus        :  Force the D-Bus dependency on for Linux targets.
  --directx     :  Force the DirectX dependency on for Windows, MinGW, and Cygwin targets.
  --dlopen      :  Force the DLOpen dependency on for Linux targets.
  --esd         :  Force the ESD dependency on for Linux targets.
  --nas         :  Force the NAS dependency on for Linux targets.
  --opengl      :  Force the OpenGL dependency on for any target.
  --oss         :  Force the OSS dependency on for Linux targets.
  --pulseaudio  :  Force the PulseAudio dependency on for Linux targets.
  --x11         :  Force the X11 dependency on for Linux targets.

All projects have debug and release configurations that may be built. For IDE
projects such as Visual Studio and Xcode, there are configurations in the former
and schemas in the latter to handle this.

For make files, the following command line may be used:
    make config=debug
or:
    make config=release

The make files also have a level of verbosity that will print all compiler and
linking commands to the command line. This can be enabled with the following
command:
    make verbose=1

[2] STRUCTURE

The structure of the meta-build system is split into three parts:

  1. The core system which runs all of the other scripts, generates the premake
    Lua file that is used to generate the actual build system, and sets up
    premake to generate it. (premake4.lua)

  2. The utility files for performing various convenience operations, ranging
    from string operations and a file wrapper to custom project definitions and
    complex dependency checking using CMake-esque functions. There is also a
    file containing custom dependency functions for checked support.
    (everything in the util folder)

  3. The project definition files, which define each and every project related
    to SDL2. This includes the SDL2 library itself, along with all of its
    current tests and iOS Demos. These files also related to dependency handling
    and help build dependency trees for the various projects.
    (everything in the projects folder)

The premake4.lua file is lightly documented and commented to explain how it
interfaces with the other utility files and project files. It is not extensively
documented because the actual generation process is not considered to be
pertinent to the overall usage of the meta-build system.

The utility files have thorough documentation, since they are the foundation for
the entire project definition and dependency handling systems.

The project definition files are lightly documented, since they are expected to
be self-explanatory. Look through each and every project definition file
(especially SDL2.lua, testgl2.lua, testshape.lua, testsprite2.lua, and
testnative.lua) to gain experience and familiarity with most of the project
definition system.

The dependency system is very straightforward. As explained in both
sdl_projects.lua and sdl_dependency_checkers.lua, a function for checking the
actual dependency support is registered by its name and then referenced to in
the project definitions (such as for SDL2.lua). These definitions are allowed to
do anything necessary to determine whether the appropriate support exists in the
current build environment or not. The possibilities for checking can be seen
specifically in the function for checking DirectX support and any of the Linux
dependency functions using the sdl_check_compile.lua functions.

As far as building the projects is concerned, the project definitions are
allowed to set configuration key-value pairs which will be translated and placed
inside a generated SDL config header file, similar to the one generated by both
autotools and CMake.

[3] SUPPORT ON WINDOWS AND VISUAL STUDIO

Check the Windows README for more information on SDL2 support on Windows and
Visual Studio. Current support exists for Visual Studio 2008, 2010, and 2012.

[4] SUPPORT ON MAC OS X AND XCODE

Check the Mac OS X README for more information on SDL2 support on Mac OS X using
Xcode. Current support should exist for Mac OS X 10.6, 10.7, and 10.8 (as
tested, but more may be supported). Supported Xcode versions are 3 and 4. It
supports building for both i686 and x86_64 architectures, as well as support for
universal 32-bit binaries, universal 64-bit binaries, and universal combined
binaries.

[5] SUPPORT FOR IOS

EXPERIMENTAL SUPPORT

Check the iOS README for more information on SDL2 support on iOS using Xcode.
Current support has been tested on the iOS 6 emulators for iPhone and iPad,
using both Xcode 3 and Xcode 4. The iOS project will reference all the Demos
the manual project does.

[6] SUPPORT FOR LINUX

EXPERIMENTAL SUPPORT

Check the Linux README for more information on SDL2 support on Linux. Currently,
only a subset of the Linux dependencies are supported, and they are supported
partially. Linux also builds to a static library instead of a shared library.
The tests run well and as expected.

[7] SUPPORT FOR MINGW

Check the MinGW README for more information on SDL2 support on MinGW. Currently,
all of the tests that work using the Visual Studio projects also seem to work
with MinGW, minus DirectX support. DirectX is not inherently supported, but can
be forcibly turned on if the user knows what they are doing.

[8] SUPPORT FOR CYGWIN

BROKEN SUPPORT

Check the Cygwin README for more information on the progress of supporting SDL2
on Cygwin.

[9] EXTENDING THE SYSTEM TO NEW PROJECTS OR PLATFORMS

In order to create a new project, simply create a Lua file and place it within
the projects directory. The meta-build system will automatically include it.
It must contain a SDL_project definition. Projects *must* have source files as
well, otherwise they will be ignored by the meta-build system. There are a
plethora of examples demonstrating how to defined projects, link them to various
dependencies, and to create dependencies.

Here is an example that creates a new project named foo, it's a ConsoleApp
(which is the default for SDL projects, look at http://industriousone.com/kind
for more information). Its language is C and its source directory is "../test"
(this path is relative to the location of premake4.lua). It's project location
is "tests", which means it will be placed in the ./tests/ folder of whichever
destination directory is set while generating the project (for example,
./VisualC/tests). It is including all the files starting with "foo." from the
"../test" folder.

    SDL_project "foo"
    	SDL_kind "ConsoleApp"
    	SDL_language "C"
    	SDL_sourcedir "../test"
    	SDL_projectLocation "tests"
    	SDL_files { "/testrendercopyex.*" }

Now, we can extend this project slightly:

    SDL_project "foo"
    	SDL_kind "ConsoleApp"
    	SDL_notos "ios|cygwin"
    	SDL_language "C"
    	SDL_sourcedir "../test"
    	SDL_projectLocation "tests"
    	SDL_projectDependencies { "SDL2main", "SDL2test", "SDL2" }
    	SDL_files { "/foo.*" }
    	SDL_copy { "icon.bmp", "sample.bmp" }

We now specified that this application will not work on iOS or Cygwin targets,
so it will be discluded when generating projects for those platforms. We have
also specified that this project depends on 'SDL2main', 'SDL2test', and 'SDL2',
which are other projects that are already defined. We can set the dependency
to any projects the SDL2 meta-build system is aware of. We also have an
interesting SDL_copy directive, which will automatically copy the files
"icon.bmp" and "sample.bmp" from "<sdl_root>/test" to the directory of foo's
executable when it's built.

Let's take a look at another example:

    SDL_project "testgl2"
    	SDL_kind "ConsoleApp"
    	SDL_notos "ios|cygwin"
    	SDL_language "C"
    	SDL_sourcedir "../test"
    	SDL_projectLocation "tests"
    	SDL_projectDependencies { "SDL2main", "SDL2test", "SDL2" }
    	SDL_defines { "HAVE_OPENGL" }
    	SDL_dependency "OpenGL"
    		-- opengl is platform independent
    		SDL_depfunc "OpenGL"
    		SDL_files { "/testgl2.*" }

This is a copy of the testgl2.lua file. Most of this is already familiar, but
there are a few new things to point out. We can set preprocessor definitions by
using the 'SDL_defines' directive. We can also create a dependency for the
project on some varied criteria. For example, testgl2 is obviously dependent on
the presence of the OpenGL library. So, the only way it will include the
"testgl2.*" (testgl2.c/testgl2.h) files is if the dependency function "OpenGL"
returns information regarding the whereabouts of the OpenGL library on the
current system. This function is registered in sdl_dependency_checkers.lua:

    function openGLDep()
    	print("Checking OpenGL dependencies...")
    	...
    	return { found = foundLib, libDirs = { }, libs = { libname } }
    end
    ...
    SDL_registerDependencyChecker("OpenGL", openGLDep)

This function is called when it's time to decide whether testgl2 should be
generated or not. openGLDep can use any and all functions to decide whether
OpenGL is supported.

Dependencies and projects can become much more sophisticate, if necessary. Take
the following example from the SDL2.lua project definition:

    -- DirectX dependency
    SDL_dependency "directx"
    	SDL_os "windows|mingw"
    	SDL_depfunc "DirectX"
    	SDL_config
    	{
    		["SDL_AUDIO_DRIVER_DSOUND"] = 1,
    		["SDL_AUDIO_DRIVER_XAUDIO2"] = 1,
    		["SDL_JOYSTICK_DINPUT"] = 1,
    		["SDL_HAPTIC_DINPUT"] = 1,
    		["SDL_VIDEO_RENDER_D3D"] = 1
    	}
    	SDL_paths
    	{
    		"/audio/directsound/",
    		"/audio/xaudio2/",
    		"/render/direct3d/",
    		-- these two depend on Xinput
    		"/haptic/windows/",
    		"/joystick/windows/",
    	}

This dependency is, as expected, for DirectX. One thing to note here is even
dependencies can be dependent on an operating system. This dependency will not
even be resolved if SDL2 is being generated on, say, Linux or Mac OS X. Two new
things shown here are 'SDL_config' and 'SDL_paths' directives. SDL_config allows
you to set preprocessor definitions that will be pasted into
SDL_config_premake.h (which acts as a replacement to SDL_config.h when building
the project). This allows for significant flexibility (look around SDL2.lua's
dependencies, especially for Linux). SDL_paths works like SDL_files, except it
includes all .c, .h, and .m files within that directory. The directory is still
relative to the source directory of the project (in this case, <sdl_root>/src).

Finally, dependency checking can be done in a huge variety of ways, ranging
from simply checking for an environmental variable to scanning directories on
Windows. Even more flexibly, the build environment itself can be checked using
functions similar to those provided in CMake to check if a function compiles,
library exists, etc. The following example comes from
sdl_dependency_checkers.lua and is used by the Linux dependency in the SDL2
project to determine whether the OSS sound system is supported:

    function ossDep()
    	print("Checking for OSS support...")
    	if not check_cxx_source_compiles([[
    				#include <sys/soundcard.h>
    				int main() { int arg = SNDCTL_DSP_SETFRAGMENT; return 0; }]])
    			and not check_cxx_source_compiles([[
    				#include <soundcard.h>
    				int main() { int arg = SNDCTL_DSP_SETFRAGMENT; return 0; }]]) then
    		print("Warning: OSS unsupported!")
    		return { found = false }
    	end
    	return { found = true }
    end

Notice how it uses 'check_cxx_source_compiles'. There are even more functions
than this to check and, rather than going in detail with them here, I encourage
you to look at the documented functions within ./util/sdl_check_compile.lua.

In order to support new platforms, start with the minimal configuration template
provided and work off of the initial SDL2 project. You may add additional
dependencies to define other source files specific to that platform (see how
it's done with Windows and Mac OS X), or you can add special dependencies that
rely on dependency functions you may implement yourself (see DirectX and
OpenGL). Dependencies can use the 'SDL_config' directive to specify special
values that can be pasted into the resulting configuration header file upon
generation.

For more detailed information about the functions supported and how they work,
look at all of the Lua files in the util directory, as well as any of the
example projects in the projects directory to demonstrate how many of these
functions are used. The information above is only a quick subset of the
capabilities of the meta-build system.