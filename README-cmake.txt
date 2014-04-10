================================================================================
CMake build system for SDL (www.cmake.org)
================================================================================

SDL's build system was traditionally based on autotools. Over time, this
approach has suffered from several issues across the different supported 
platforms.
To solve these problems, a new build system based on CMake is under development.
It works in parallel to the legacy system, so users can experiment with it
without complication.
While still experimental, the build system should be usable on the following
platforms:
    
    * FreeBSD
    * Linux
    * VS.NET 2010
    * MinGW and Msys
    * OS X with support for XCode
    
================================================================================
Usage
================================================================================

Assuming the source for SDL is located at ~/sdl

cd ~
mkdir build
cd build
cmake ../sdl

This will build the static and dynamic versions of SDL in the ~/build directory.
