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

## MinGW-w64 compiler support

SDL can be built with MinGW-w64 and CMake. Minimum tested MinGW-w64 version is 8.0.3.

On a Windows host, you first need to install and set up the MSYS2 environment, which provides the MinGW-w64 toolchain. Install MSYS2, typically to `C:\msys64`, and follow the instructions on the MSYS2 wiki to use the MinGW-w64 shell to update all components in the MSYS2 environment. This generally amounts to running `pacman -Syuu` from the mingw64 shell, but refer to MSYS2's documentation for more details. Once the MSYS2 environment has been updated, install the x86_64 MinGW toolchain from the mingw64 shell with the command `pacman -S mingw-w64-x86_64-toolchain`. (You can additionally install `mingw-w64-i686-toolchain` if you intend to build 32-bit binaries as well. The remainder of this section assumes you only want to build 64-bit binaries.)

To build and install SDL, you can use PowerShell or any CMake-compatible IDE. First, install CMake, Ninja, and Git. These tools can be installed using any number of tools, such as the MSYS2's `pacman`, `winget`, `Chocolatey`, or by manually downloading and running the installers. Clone SDL to an appropriate location with `git` and run the following commands from the root of the cloned repository:

```sh
mkdir build
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=build-scripts/cmake-toolchain-mingw64-x86_64.cmake
cmake --build build --parallel
cmake --install build --prefix C:/Libraries
```

This installs SDL to `C:\Libraries`. You can specify another directory of your choice as desired. Ensure that your `CMAKE_PREFIX_PATH` includes `C:\Libraries` when you want to build against this copy of SDL. The simplest way to do this is to pass it to CMake as an option at configuration time:

```sh
cmake .. -G Ninja -DCMAKE_PREFIX_PATH=C:/Libraries
```

You will also need to configure CMake to use the MinGW-w64 toolchain to build your own project. Here is a minimal toolchain file that you could use for this purpose:

```
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(CMAKE_C_COMPILER NAMES x86_64-w64-mingw32-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-w64-mingw32-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES x86_64-w64-mingw32-windres windres REQUIRED)
```

Save this in your project and refer to it at configuration time with the option `-DCMAKE_TOOLCHAIN_FILE`.

On Windows, you also need to copy `SDL3.dll` to an appropriate directory so that the game can find it at runtime. For guidance, see [README-cmake.md](README-cmake.md#how-do-i-copy-a-sdl3-dynamic-library-to-another-location).

Below is a minimal `CMakeLists.txt` file to build your game linked against a system SDL that was built with the MinGW-w64 toolchain. See [README-cmake.md](README-cmake.md) for more details on including SDL in your CMake project.

```cmake
cmake_minimum_required(VERSION 3.15)
project(mygame)

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared)

add_executable(mygame WIN32 mygame.c)
target_link_libraries(mygame PRIVATE SDL3::SDL3)

# On Windows, copy SDL3.dll to the build directory
if(WIN32)
    add_custom_command(
        TARGET mygame POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:mygame>
        VERBATIM
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
