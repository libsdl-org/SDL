# CMake

(www.cmake.org)

SDL's build system was traditionally based on autotools. Over time, this
approach has suffered from several issues across the different supported
platforms.
To solve these problems, a new build system based on CMake was introduced.
It is developed in parallel to the legacy autotools build system, so users
can experiment with it without complication.

The CMake build system is supported on the following platforms:

* FreeBSD
* Linux
* Microsoft Visual C
* MinGW and Msys
* macOS, iOS, and tvOS, with support for XCode
* Android
* Emscripten
* RiscOS
* Playstation Vita

## Building SDL

Assuming the source for SDL is located at `~/sdl`
```sh
cd ~
mkdir build
cd build
cmake ~/sdl
cmake --build .
```

This will build the static and dynamic versions of SDL in the `~/build` directory.
Installation can be done using:

```sh
cmake --install .        # '--install' requires CMake 3.15, or newer
```

## Including SDL in your project

SDL can be included in your project in 2 major ways:
- using a system SDL library, provided by your (*nix) distribution or a package manager
- using a vendored SDL library: this is SDL copied or symlinked in a subfolder.

The following CMake script supports both, depending on the value of `MYGAME_VENDORED`.

```cmake
cmake_minimum_required(VERSION 3.5)
project(mygame)

# Create an option to switch between a system sdl library and a vendored sdl library
option(MYGAME_VENDORED "Use vendored libraries" OFF)

if(MYGAME_VENDORED)
    add_subdirectory(vendored/sdl EXCLUDE_FROM_ALL)
else()
    # 1. Look for a SDL2 package, 2. look for the SDL2 component and 3. fail if none can be found
    find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

    # 1. Look for a SDL2 package, 2. Look for the SDL2maincomponent and 3. DO NOT fail when SDL2main is not available
    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
endif()

# Create your game executable target as usual
add_executable(mygame WIN32 mygame.c)

# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
if(TARGET SDL2::SDL2main)
    # It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
    target_link_libraries(mygame PRIVATE SDL2::SDL2main)
endif()

# Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library, SDL2::SDL2-static is the static SDL libarary.
target_link_libraries(mygame PRIVATE SDL2::SDL2)
```

### A system SDL library

For CMake to find SDL, it must be installed in [a default location CMake is looking for](https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure).

The following components are available, to be used as an argument of `find_package`.

| Component name | Description                                                                                |
|----------------|--------------------------------------------------------------------------------------------|
| SDL2           | The SDL2 shared library, available through the `SDL2::SDL2` target [^SDL_TARGET_EXCEPTION] |
| SDL2-static    | The SDL2 static library, available through the `SDL2::SDL2-static` target                  |
| SDL2main       | The SDL2main static library, available through the `SDL2::SDL2main` target                 |
| SDL2test       | The SDL2test static library, available through the `SDL2::SDL2test` target                 |

### Using a vendored SDL

This only requires a copy of SDL in a subdirectory.

## CMake configuration options for platforms

### iOS/tvOS

CMake 3.14+ natively includes support for iOS and tvOS.  SDL binaries may be built
using Xcode or Make, possibly among other build-systems.

When using a recent version of CMake (3.14+), it should be possible to:

- build SDL for iOS, both static and dynamic
- build SDL test apps (as iOS/tvOS .app bundles)
- generate a working SDL_config.h for iOS (using SDL_config.h.cmake as a basis)

To use, set the following CMake variables when running CMake's configuration stage:

- `CMAKE_SYSTEM_NAME=<OS>`   (either `iOS` or `tvOS`)
- `CMAKE_OSX_SYSROOT=<SDK>`  (examples: `iphoneos`, `iphonesimulator`, `iphoneos12.4`, `/full/path/to/iPhoneOS.sdk`,
                              `appletvos`, `appletvsimulator`, `appletvos12.4`, `/full/path/to/AppleTVOS.sdk`, etc.)
- `CMAKE_OSX_ARCHITECTURES=<semicolon-separated list of CPU architectures>` (example: "arm64;armv7s;x86_64")


#### Examples

- for iOS-Simulator, using the latest, installed SDK:

    ```bash
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=x86_64
    ```

- for iOS-Device, using the latest, installed SDK, 64-bit only

    ```bash
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64
    ```

- for iOS-Device, using the latest, installed SDK, mixed 32/64 bit

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES="arm64;armv7s"
    ```

- for iOS-Device, using a specific SDK revision (iOS 12.4, in this example):

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos12.4 -DCMAKE_OSX_ARCHITECTURES=arm64
    ```

- for iOS-Simulator, using the latest, installed SDK, and building SDL test apps (as .app bundles):

    ```cmake
    cmake ~/sdl -DSDL_TESTS=1 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=x86_64
    ```

- for tvOS-Simulator, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT=appletvsimulator -DCMAKE_OSX_ARCHITECTURES=x86_64
    ```

- for tvOS-Device, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_SYSROOT=appletvos -DCMAKE_OSX_ARCHITECTURES=arm64`
    ```


[^SDL_TARGET_EXCEPTION]: `SDL2::SDL2` can be an ALIAS to a static `SDL2::SDL2-static` target for multiple reasons.
