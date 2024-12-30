# Windows

## Old systems

WinRT, Windows Phone, and UWP are no longer supported.

All desktop Windows versions, back to Windows XP, are still supported.

## LLVM and Intel C++ compiler support

SDL will build with the Visual Studio project files with LLVM-based compilers, such as the Intel oneAPI C++
compiler, but you'll have to manually add the "-msse3" command line option
to at least the SDL_audiocvt.c source file, and possibly others. This may
not be necessary if you build SDL with CMake instead of the included Visual
Studio solution.

Details are here: https://github.com/libsdl-org/SDL/issues/5186

## MinGW-64 compiler support

SDL can be built with MinGW-64 and CMake. First, you need to install and set up the MSYS2 environment, which provides the MinGW-64 toolchain. Install MSYS2, typically to `C:\msys64`, and follow the instructions on the MSYS2 wiki to use the MinGW-64 shell to update all components in the MSYS2 environment. This generally amounts to running `pacman -Syuu` from the mingw64 shell, but refer to MSYS2's documentation for more details. Once the MSYS2 environment has been updated, install the x86_64 MinGW toolchain from the mingw64 shell with the command `pacman -S mingw-w64-x86_64-toolchain`. (You could alternatively install the 32-bit MinGW toolchain from the mingw32 shell.)

To build and install SDL, you can use PowerShell or any CMake-compatible IDE. First, install CMake, Ninja, and Git. These tools can be installed using the Windows Package Manager `winget` (The winget IDs for these tools are Kitware.CMake, Git.Git, and Ninja-build.Ninja). Clone SDL to an appropriate location with `git` and run the following commands from the root of the cloned repository:

```sh
mkdir build
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=build-scripts/cmake-toolchain-mingw64-x86_64.cmake
cmake --build build --parallel
cmake --install build --prefix C:\Libraries
```

This installs SDL to `C:\Libraries`. You can specify another directory of your choice as desired. To build your game against the installed SDL using CMake, copy `build-scripts/cmake-toolchain-mingw64-x86_64.cmake` into your project directory and refer to it with the `-DCMAKE_TOOLCHAIN_FILE` option. Alternatively, you can directly copy these three lines into your `CMakeLists.txt` file:

```cmake
find_program(CMAKE_C_COMPILER NAMES x86_64-w64-mingw32-gcc)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-w64-mingw32-g++)
find_program(CMAKE_RC_COMPILER NAMES x86_64-w64-mingw32-windres windres)
```

Ensure that your `CMAKE_PREFIX_PATH` includes `C:\Libraries`. A minimal fix is to pass it as a CMake option when building:

```sh
cmake .. -G Ninja -DCMAKE_PREFIX_PATH=C:\Libraries
```

On Windows, you also need to copy `SDL3.dll` to your build directory so that the game can find it at runtime. You can use `find_program` in CMake to locate the DLL:

```cmake
find_program(SDL3_DLL NAMES SDL3.dll PATH_SUFFIXES bin REQUIRED)
```

Below is a minimal `CMakeLists.txt` file to build your game linked against a system SDL that was built with the MinGW-64 toolchain. See [README-cmake.md](README-cmake.md) for more details on including SDL in your CMake project.

```cmake
cmake_minimum_required(VERSION 3.18)
project(mygame)

# Find mingw64 toolchain.
find_program(CMAKE_C_COMPILER NAMES x86_64-w64-mingw32-gcc)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-w64-mingw32-g++)
find_program(CMAKE_RC_COMPILER NAMES x86_64-w64-mingw32-windres windres)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_executable(mygame WIN32 mygame.c)
target_link_libraries(mygame PRIVATE SDL3::SDL3)  # Links to the static SDL3 shim

# On Windows, copy the DLL to the build dir so it can be loaded by the shim
if(WIN32)
    find_program(SDL3_DLL NAMES SDL3.dll PATH_SUFFIXES bin REQUIRED)
    message(STATUS "Found SDL3.dll at: ${SDL3_DLL}")

    add_custom_command(TARGET mygame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_DLL}"
            $<TARGET_FILE_DIR:mygame>
            COMMENT "Copying SDL3.dll to the executable directory"
    )
endif()
```

## OpenGL ES 2.x support

SDL has support for OpenGL ES 2.x under Windows via two alternative
implementations.

The most straightforward method consists in running your app in a system with
a graphic card paired with a relatively recent (as of November of 2013) driver
which supports the WGL_EXT_create_context_es2_profile extension. Vendors known
to ship said extension on Windows currently include nVidia and Intel.

The other method involves using the
[ANGLE library](https://code.google.com/p/angleproject/). If an OpenGL ES 2.x
context is requested and no WGL_EXT_create_context_es2_profile extension is
found, SDL will try to load the libEGL.dll library provided by ANGLE.

To obtain the ANGLE binaries, you can either compile from source from
https://chromium.googlesource.com/angle/angle or copy the relevant binaries
from a recent Chrome/Chromium install for Windows. The files you need are:

- libEGL.dll
- libGLESv2.dll
- d3dcompiler_46.dll (supports Windows Vista or later, better shader
  compiler) *or* d3dcompiler_43.dll (supports Windows XP or later)

If you compile ANGLE from source, you can configure it so it does not need the
d3dcompiler_* DLL at all (for details on this, see their documentation).
However, by default SDL will try to preload the d3dcompiler_46.dll to
comply with ANGLE's requirements. If you wish SDL to preload
d3dcompiler_43.dll (to support Windows XP) or to skip this step at all, you
can use the SDL_HINT_VIDEO_WIN_D3DCOMPILER hint (see SDL_hints.h for more
details).

Known Bugs:

- SDL_GL_SetSwapInterval is currently a no op when using ANGLE. It appears
  that there's a bug in the library which prevents the window contents from
  refreshing if this is set to anything other than the default value.

## Vulkan Surface Support

Support for creating Vulkan surfaces is configured on by default. To disable
it change the value of `SDL_VIDEO_VULKAN` to 0 in `SDL_config_windows.h`. You
must install the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) in order to
use Vulkan graphics in your application.

## Transparent Window Support

SDL uses the Desktop Window Manager (DWM) to create transparent windows. DWM is
always enabled from Windows 8 and above. Windows 7 only enables DWM with Aero Glass
theme.

However, it cannot be guaranteed to work on all hardware configurations (an example
is hybrid GPU systems, such as NVIDIA Optimus laptops).
