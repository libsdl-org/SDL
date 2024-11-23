# To compile and install SDL:

##  Windows with Visual Studio:

Read ./docs/README-visualc.md

## Windows building with mingw-w64 for x86:

Run: `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build-scripts/cmake-toolchain-mingw64-i686.cmake && cmake --build build && cmake --install build`

## Windows building with mingw-w64 for x64:

Run: `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build-scripts/cmake-toolchain-mingw64-x86_64.cmake && cmake --build build && cmake --install build`

## macOS with Xcode:

Read docs/README-macos.md

## macOS from the command line:

Run: `cmake -S . -B build && cmake --build build && cmake --install build`

### macOS for universal architecture:

Run: `cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" && cmake --build build && cmake --install build`

## Linux and other UNIX systems:

Run: `cmake -S . -B build && cmake --build build --parallel $(nproc) && cmake --install build`

## Android:

Read docs/README-android.md

## iOS:

Read docs/README-ios.md

## Using CMake:

Read docs/README-cmake.md

# Example code

Look at the example programs in ./test, and check out the online
documentation at https://wiki.libsdl.org/SDL3/

# Discussion

## Forums/mailing lists

Join the SDL developer discussions, sign up on

https://discourse.libsdl.org/

and go to the development forum

https://discourse.libsdl.org/c/sdl-development/6

Once you sign up, you can use the forum through the website, or as a mailing
list from your email client.

## Announcement list

Sign up for the announcement list through the web interface:

https://www.libsdl.org/mailing-list.php

