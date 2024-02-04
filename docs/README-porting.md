Porting
=======

* Porting To A New Platform

  The first thing you have to do when porting to a new platform, is look at
include/SDL_platform.h and create an entry there for your operating system.
The standard format is "__PLATFORM__", where PLATFORM is the name of the OS.
Ideally SDL_platform_defines.h will be able to auto-detect the system it's building
on based on C preprocessor symbols.

There are two basic ways of building SDL at the moment:

1. CMake:  cmake -S . -B build && cmake --build build && cmake --install install

   If you have a system that supports CMake, then you might try this.  Edit CMakeLists.txt,

   take a look at the large section labelled:

	"Platform-specific options and settings!"

   Add a section for your platform, and then re-run 'cmake -S . -B build' and build!

2. Using an IDE:

   If you're using an IDE or other non-configure build system, you'll probably want to create a custom `SDL_build_config.h` for your platform.  Edit `include/build_config/SDL_build_config.h`, add a section for your platform, and create a custom `SDL_build_config_{platform}.h`, based on `SDL_build_config_minimal.h` and `SDL_build_config.h.cmake`

   Add the top level include directory to the header search path, and then add
   the following sources to the project:

	src/*.c
	src/atomic/*.c
	src/audio/*.c
	src/cpuinfo/*.c
	src/events/*.c
	src/file/*.c
	src/haptic/*.c
	src/joystick/*.c
	src/power/*.c
	src/render/*.c
	src/render/software/*.c
	src/stdlib/*.c
	src/thread/*.c
	src/timer/*.c
	src/video/*.c
	src/audio/disk/*.c
	src/audio/dummy/*.c
	src/filesystem/dummy/*.c
	src/video/dummy/*.c
	src/haptic/dummy/*.c
	src/joystick/dummy/*.c
	src/thread/generic/*.c
	src/timer/dummy/*.c
	src/loadso/dummy/*.c


Once you have a working library without any drivers, you can go back to each
of the major subsystems and start implementing drivers for your platform.

If you have any questions, don't hesitate to ask on the SDL mailing list:
	http://www.libsdl.org/mailing-list.php

Enjoy!
	Sam Lantinga				(slouken@libsdl.org)

