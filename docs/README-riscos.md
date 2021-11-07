RISC OS
=======

Requirements:

* RISC OS 3.5 or later.
* [SharedUnixLibrary](http://www.riscos.info/packages/LibraryDetails.html#SharedUnixLibraryarm).
* [DigitalRenderer](http://www.riscos.info/packages/LibraryDetails.html#DRendererarm), for audio support.
* [Iconv](http://www.netsurf-browser.org/projects/iconv/), for `SDL_iconv` and related functions.


Compiling:
----------

Currently, SDL2 for RISC OS only supports compiling with GCCSDK under Linux. Both the autoconf and CMake build systems are supported.

The following commands can be used to build SDL2 for RISC OS using autoconf:

    ./configure --host=arm-unknown-riscos --prefix=$GCCSDK_INSTALL_ENV --disable-gcc-atomics
    make
    make install

The following commands can be used to build SDL2 for RISC OS using CMake:

    cmake -Bbuild-riscos -DCMAKE_TOOLCHAIN_FILE=$GCCSDK_INSTALL_ENV/toolchain-riscos.cmake -DRISCOS=ON -DCMAKE_INSTALL_PREFIX=$GCCSDK_INSTALL_ENV -DCMAKE_BUILD_TYPE=Release -DSDL_GCC_ATOMICS=OFF
    cmake --build build-riscos
    cmake --build build-riscos --target install


Current level of implementation
-------------------------------

The video driver currently provides full screen video support with keyboard and mouse input. Windowed mode is not yet supported, but is planned in the future. Only software rendering is supported.

The filesystem APIs return either Unix-style paths or RISC OS-style paths based on the value of the `__riscosify_control` symbol, as is standard for UnixLib functions.

The audio, loadso, thread and timer APIs are currently provided by UnixLib.

GCC atomics are currently broken on some platforms, meaning it's currently necessary to compile with `--disable-gcc-atomics` using autotools or `-DSDL_GCC_ATOMICS=OFF` using CMake.

The joystick, locale and power APIs are not yet implemented.
