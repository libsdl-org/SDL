# CMake

[www.cmake.org](https://www.cmake.org/)

The CMake build system is supported with the following environments:

* Android
* Emscripten
* FreeBSD
* Haiku
* Linux
* macOS, iOS, tvOS, and visionOS with support for XCode
* Microsoft Visual Studio
* MinGW and Msys
* NetBSD
* Nintendo 3DS
* PlayStation 2
* PlayStation Portable
* PlayStation Vita
* RISC OS

## Building SDL on Windows

Assuming you're in the SDL source directory, building and installing to C:/SDL can be done with:
```sh
cmake -S . -B build
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo --prefix C:/SDL
```

## Building SDL on UNIX

SDL will build with very few dependencies, but for full functionality you should install the packages detailed in [README-linux.md](README-linux.md).

Assuming you're in the SDL source directory, building and installing to /usr/local can be done with:
```sh
cmake -S . -B build
cmake --build build
sudo cmake --install build --prefix /usr/local
```

## Building SDL on macOS

Assuming you're in the SDL source directory, building and installing to ~/SDL can be done with:
```sh
cmake -S . -B build -DSDL_FRAMEWORK=ON -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build
cmake --install build --prefix ~/SDL
```

## Building SDL tests

You can build the SDL test programs by adding `-DSDL_TESTS=ON` to the first cmake command above:
```sh
cmake -S . -B build -DSDL_TESTS=ON
```
and then building normally. The test programs will be built and can be run from `build/test/`.

## Building SDL examples

You can build the SDL example programs by adding `-DSDL_EXAMPLES=ON` to the first cmake command above:
```sh
cmake -S . -B build -DSDL_EXAMPLES=ON
```
and then building normally. The example programs will be built and can be run from `build/examples/`.

## Including SDL in your project

SDL can be included in your project in 2 major ways:
- using a system SDL library, provided by your (UNIX) distribution or a package manager
- using a vendored SDL library: this is SDL copied or symlinked in a subfolder.

The following CMake script supports both, depending on the value of `MYGAME_VENDORED`.

```cmake
cmake_minimum_required(VERSION 3.5)
project(mygame)

# Create an option to switch between a system sdl library and a vendored SDL library
option(MYGAME_VENDORED "Use vendored libraries" OFF)

if(MYGAME_VENDORED)
    # This assumes you have added SDL as a submodule in vendored/SDL
    add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
else()
    # 1. Look for a SDL3 package,
    # 2. look for the SDL3-shared component, and
    # 3. fail if the shared component cannot be found.
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif()

# Create your game executable target as usual
add_executable(mygame WIN32 mygame.c)

# Link to the actual SDL3 library.
target_link_libraries(mygame PRIVATE SDL3::SDL3)
```

### A system SDL library

For CMake to find SDL, it must be installed in [a default location CMake is looking for](https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure).

The following components are available, to be used as an argument of `find_package`.

| Component name | Description                                                                                                                                                      |
|----------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SDL3-shared    | The SDL3 shared library, available through the `SDL3::SDL3-shared` target                                                                                        |
| SDL3-static    | The SDL3 static library, available through the `SDL3::SDL3-static` target                                                                                        |
| SDL3_test      | The SDL3_test static library, available through the `SDL3::SDL3_test` target                                                                                     |
| SDL3           | The SDL3 library, available through the `SDL3::SDL3` target. This is an alias of `SDL3::SDL3-shared` or `SDL3::SDL3-static`. This component is always available. |
| Headers        | The SDL3 headers, available through the `SDL3::Headers` target. This component is always available.                                                              |

SDL's CMake support guarantees a `SDL3::SDL3` target.
Neither `SDL3::SDL3-shared` nor `SDL3::SDL3-static` are guaranteed to exist.

### Using a vendored SDL

This only requires a copy of SDL in a subdirectory + `add_subdirectory`.
Alternatively, use [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html).
Depending on the configuration, the same targets as a system SDL package are available.

## CMake configuration options

### Build optimized library

By default, CMake provides 4 build types: `Debug`, `Release`, `RelWithDebInfo` and `MinSizeRel`.
The main difference(s) between these are the optimization options and the generation of debug info.
To configure SDL as an optimized `Release` library, configure SDL with:
```sh
cmake ~/SDL -DCMAKE_BUILD_TYPE=Release
```
To build it, run:
```sh
cmake --build . --config Release
```

### Shared or static

By default, only a dynamic (=shared) SDL library is built and installed.
The options `-DSDL_SHARED=` and `-DSDL_STATIC=` accept boolean values to change this.

Exceptions exist:
- some platforms don't support dynamic libraries, so only `-DSDL_STATIC=ON` makes sense.
- a static Apple framework is not supported

### Man pages

Configuring with `-DSDL_INSTALL_DOCS=TRUE` installs man pages.

We recommend package managers of unix distributions to install SDL3's man pages.
This adds an extra build-time dependency on Perl.

### Pass custom compile options to the compiler

- Use [`CMAKE_<LANG>_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html) to pass extra
flags to the compiler.
- Use [`CMAKE_EXE_LINKER_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_EXE_LINKER_FLAGS.html) to pass extra option to the linker for executables.
- Use [`CMAKE_SHARED_LINKER_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_SHARED_LINKER_FLAGS.html) to pass extra options to the linker for shared libraries.

#### Compile Options Examples

- build a SDL library optimized for (more) modern x64 microprocessor architectures.

  With gcc or clang:
    ```sh
    cmake ~/sdl -DCMAKE_C_FLAGS="-march=x86-64-v3" -DCMAKE_CXX_FLAGS="-march=x86-64-v3"
    ```
  With Visual C:
    ```sh
    cmake .. -DCMAKE_C_FLAGS="/ARCH:AVX2" -DCMAKE_CXX_FLAGS="/ARCH:AVX2"
    ```

### Apple

CMake documentation for cross building for Apple:
[link](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-visionos-or-watchos)

#### iOS/tvOS/visionOS

CMake 3.14+ natively includes support for iOS, tvOS and watchOS. visionOS requires CMake 3.28+.
SDL binaries may be built using Xcode or Make, possibly among other build-systems.

When using a compatible version of CMake, it should be possible to:

- build SDL dylibs, both static and dynamic dylibs
- build SDL frameworks, only shared
- build SDL test apps

#### Frameworks

Configure with `-DSDL_FRAMEWORK=ON` to build a SDL framework instead of a dylib shared library.
Only shared frameworks are supported, no static ones.

#### Platforms

Use `-DCMAKE_SYSTEM_NAME=<value>` to configure the platform. CMake can target only one platform at a time.

| Apple platform  | `CMAKE_SYSTEM_NAME` value |
|-----------------|---------------------------|
| macOS (MacOS X) | `Darwin`                  |
| iOS             | `iOS`                     |
| tvOS            | `tvOS`                    |
| visionOS        | `visionOS`                |
| watchOS         | `watchOS`                 |

#### Universal binaries

A universal binaries, can be built by configuring CMake with
`-DCMAKE_OSX_ARCHITECTURES=<semicolon-separated list of CPU architectures>`.

For example `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` will build binaries that run on both Intel cpus and Apple silicon.

SDL supports following Apple architectures:

| Platform                   | `CMAKE_OSX_ARCHITECTURES` value |
|----------------------------|---------------------------------|
| 64-bit ARM (Apple Silicon) | `arm64`                         |
| x86_64                     | `x86_64`                        |
| 32-bit ARM                 | `armv7s`                        |

CMake documentation: [link](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_ARCHITECTURES.html)

#### Simulators and/or non-default macOS platform SDK

Use `-DCMAKE_OSX_SYSROOT=<value>` to configure a different platform SDK.
The value can be either the name of the SDK, or a full path to the sdk (e.g. `/full/path/to/iPhoneOS.sdk`).

| SDK                  | `CMAKE_OSX_SYSROOT` value |
|----------------------|---------------------------|
| iphone               | `iphoneos`                |
| iphonesimulator      | `iphonesimulator`         |
| appleTV              | `appletvos`               |
| appleTV simulator    | `appletvsimulator`        |
| visionOS             | `xr`                      |
| visionOS simulator   | `xrsimulator`             |
| watchOS              | `watchos`                 |
| watchOS simulator    | `watchsimulator`          |

Append with a version number to target a specific SDK revision: e.g. `iphoneos12.4`, `appletvos12.4`.

CMake documentation: [link](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_SYSROOT.html)

#### Apple Examples

- for macOS, building a dylib and/or static library for x86_64 and arm64:

    ```bash
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11

- for macOS, building an universal framework for x86_64 and arm64:

    ```bash
    cmake ~/sdl -DSDL_FRAMEWORK=ON -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11

- for iOS-Simulator, using the latest, installed SDK:

    ```bash
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for iOS-Device, using the latest, installed SDK, 64-bit only

    ```bash
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for iOS-Device, using the latest, installed SDK, mixed 32/64 bit

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES="arm64;armv7s" -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for iOS-Device, using a specific SDK revision (iOS 12.4, in this example):

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos12.4 -DCMAKE_OSX_ARCHITECTURES=arm64
    ```

- for iOS-Simulator, using the latest, installed SDK, and building SDL test apps (as .app bundles):

    ```cmake
    cmake ~/sdl -DSDL_TESTS=1 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for tvOS-Simulator, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT=appletvsimulator -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for tvOS-Device, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT=appletvos -DCMAKE_OSX_ARCHITECTURES=arm64` -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0
    ```

- for QNX/aarch64, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_TOOLCHAIN_FILE=~/sdl/build-scripts/cmake-toolchain-qnx-aarch64le.cmake -DSDL_X11=0
    ```

## SDL-specific CMake options

SDL can be customized through (platform-specific) CMake options.
The following table shows generic options that are available for most platforms.
At the end of SDL CMake configuration, a table shows all CMake options along with its detected value.

| CMake option                  | Valid values | Description                                                                                         |
|-------------------------------|--------------|-----------------------------------------------------------------------------------------------------|
| `-DSDL_SHARED=`               | `ON`/`OFF`   | Build SDL shared library (not all platforms support this) (`libSDL3.so`/`libSDL3.dylib`/`SDL3.dll`) |
| `-DSDL_STATIC=`               | `ON`/`OFF`   | Build SDL static library (`libSDL3.a`/`SDL3-static.lib`)                                            |
| `-DSDL_TEST_LIBRARY=`         | `ON`/`OFF`   | Build SDL test library (`libSDL3_test.a`/`SDL3_test.lib`)                                           |
| `-DSDL_TESTS=`                | `ON`/`OFF`   | Build SDL test programs (**requires `-DSDL_TEST_LIBRARY=ON`**)                                      |
| `-DSDL_DISABLE_INSTALL=`      | `ON`/`OFF`   | Don't create a SDL install target                                                                   |
| `-DSDL_DISABLE_INSTALL_DOCS=` | `ON`/`OFF`   | Don't install the SDL documentation                                                                 |
| `-DSDL_INSTALL_TESTS=`        | `ON`/`OFF`   | Install the SDL test programs                                                                       |

### Incompatibilities

#### `SDL_LIBC=OFF` and sanitizers

Building with `-DSDL_LIBC=OFF` will make it impossible to use the sanitizer, such as the address sanitizer.
Configure your project with `-DSDL_LIBC=ON` to make use of sanitizers.

## CMake FAQ

### CMake fails to build without X11 or Wayland support

Install the required system packages prior to running CMake.
See [README-linux.md](README-linux.md#build-dependencies) for the list of dependencies on Linux.
Other unix operating systems should provide similar packages.

If you **really** don't need to show windows, add `-DSDL_UNIX_CONSOLE_BUILD=ON` to the CMake configure command.

### How do I copy a SDL3 dynamic library to another location?

Use [CMake generator expressions](https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html#target-dependent-expressions).
Generator expressions support multiple configurations, and are evaluated during build system generation time.

On Windows, the following example copies `SDL3.dll` to the directory where `mygame.exe` is built.
```cmake
if(WIN32)
    add_custom_command(
        TARGET mygame POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:mygame>
        VERBATIM
    )
endif()
```
On Unix systems, `$<TARGET_FILE:...>` will refer to the dynamic library (or framework),
and you might need to use `$<TARGET_SONAME_FILE:tgt>` instead.

Most often, you can avoid copying libraries by configuring your project with absolute [`CMAKE_LIBRARY_OUTPUT_DIRECTORY`](https://cmake.org/cmake/help/latest/variable/CMAKE_LIBRARY_OUTPUT_DIRECTORY.html)
and [`CMAKE_RUNTIME_OUTPUT_DIRECTORY`](https://cmake.org/cmake/help/latest/variable/CMAKE_RUNTIME_OUTPUT_DIRECTORY.html) paths.
When using a multi-config generator (such as Visual Studio or Ninja Multi-Config), eventually add `/$<CONFIG>` to both paths.

### Linking against a static SDL library fails due to relocation errors

On unix platforms, all code that ends up in shared libraries needs to be built as relocatable (=position independent) code.
However, by default CMake builds static libraries as non-relocatable.
Configuring SDL with `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` will result in a static `libSDL3.a` library
which you can link against to create a shared library.

