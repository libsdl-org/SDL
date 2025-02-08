RISC OS
=======

Requirements:

* RISC OS 3.5 or later.
* [SharedUnixLibrary](http://www.riscos.info/packages/LibraryDetails.html#SharedUnixLibraryarm).
* [DigitalRenderer](http://www.riscos.info/packages/LibraryDetails.html#DRendererarm), for audio support.
* [Iconv](http://www.netsurf-browser.org/projects/iconv/), for `SDL_iconv` and related functions.


Compiling:
----------

Currently, SDL for RISC OS only supports compiling with GCCSDK under Linux.

The following commands can be used to build SDL for RISC OS using CMake:

    cmake -Bbuild-riscos -DCMAKE_TOOLCHAIN_FILE=$GCCSDK_INSTALL_ENV/toolchain-riscos.cmake -DRISCOS=ON -DCMAKE_INSTALL_PREFIX=$GCCSDK_INSTALL_ENV -DCMAKE_BUILD_TYPE=Release
    cmake --build build-riscos
    cmake --install build-riscos

When using GCCSDK 4.7.4 release 6 or earlier versions, the builtin atomic functions are broken, meaning it's currently necessary to compile with `-DSDL_GCC_ATOMICS=OFF` using CMake. Newer versions of GCCSDK don't have this problem.


Current level of implementation
-------------------------------

The video driver currently provides full screen video support with keyboard and mouse input. Windowed mode is not yet supported, but is planned in the future. Only software rendering is supported.

The filesystem APIs return either Unix-style paths or RISC OS-style paths based on the value of the `__riscosify_control` symbol, as is standard for UnixLib functions.

The audio, loadso, thread and timer APIs are currently provided by UnixLib.

The joystick, locale and power APIs are not yet implemented.
