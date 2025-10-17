# Introduction to SDL with MinGW

Without getting deep into the history, MinGW is a long running project that aims to bring gcc to Windows. That said, there's many distributions, versions, and forks floating around. We recommend installing [MSYS2](https://www.msys2.org/), as it's the easiest way to get a modern toolchain with a package manager to help with dependency management. This would allow you to follow the MSYS2 section below. 

Otherwise you'll want to follow the "Other Distributions" section below.

We'll start by creating a simple project to build and run [hello.c](hello.c).

# MSYS2

Open the `MSYS2 UCRT64` prompt and then ensure you've installed the following packages. This will get you working toolchain, CMake, Ninja, and of course SDL3.

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-sdl3
```

## Create the file CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.26)
project(hello C CXX)

find_package(SDL3 REQUIRED)

add_executable(hello)

target_sources(hello
PRIVATE
    hello.c
)

target_link_libraries(hello SDL3::SDL3)
```

## Configure and Build:
```sh
cmake -S . -B build
cmake --build build
```

## Run:

The executable is in the `build` directory:
```sh
cd build
./hello
```

# Other Distributions

Things can get quite complicated with other distributions of MinGW. If you can't follow [the cmake intro](INTRO-cmake.md), perhaps due to issues getting cmake to understand your toolchain, this section should work.

## Acquire SDL

Download the `SDL3-devel-<version>-mingw.zip` asset from [the latest release.](https://github.com/libsdl-org/SDL/releases/latest) Then extract it inside your project folder such that the output of `ls SDL3-<version>` looks like `INSTALL.md  LICENSE.txt  Makefile  README.md  cmake  i686-w64-mingw32  x86_64-w64-mingw32`.

## Know your Target Architecture

It is not uncommon for folks to not realize their distribution is targeting 32bit Windows despite things like the name of the toolchain, or the fact that they're running on a 64bit system. We'll ensure we know up front what we need:

Create a file named `arch.c` with the following contents:
```c
#include <stddef.h>
#include <stdio.h>
int main() {
    #if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
        size_t ptr_size = sizeof(int*);
        if (4 == ptr_size) puts("i686-w64-mingw32");
        else if (8 == ptr_size) puts("x86_64-w64-mingw32");
        else puts("Unknown Architecture");
    #else
        puts("Unknown Architecture");
    #endif
    return 0;
}
```

Then run

```sh
gcc arch.c
./a.exe
```

This should print out which library directory we'll need to use when compiling, keep this value in mind, you'll need to use it when compiling in the next section as `<arch>`. If you get "Unknown Architecture" please [report a bug](https://github.com/libsdl-org/SDL/issues).


## Build and Run

Now we should have everything needed to compile and run our program. You'll need to ensure to replace `<version>` with the version of the release of SDL3 you downloaded, as well as use the `<arch>` we learned in the previous section.

```sh
gcc hello.c -o hello.exe -I SDL3-<version>/<arch>/include -L SDL3-<version>/<arch>/lib -lSDL3 -mwindows 
cp SDL3-<version>/<arch>/bin/SDL3.dll SDL3.dll
./hello.exe
```
