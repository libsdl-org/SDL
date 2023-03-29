# CMake

(www.cmake.org)

The CMake build system is supported on the following platforms:

* FreeBSD
* Linux
* Microsoft Visual C
* MinGW and Msys
* macOS, iOS, and tvOS, with support for XCode
* Android
* Emscripten
* RiscOS
* Playstation 2
* Playstation Vita
* Nintendo 3DS
* QNX 7.x/8.x

## Building SDL

Assuming the source for SDL is located at `~/sdl`.
```sh
cmake -S ~/sdl -B ~/build
cmake --build ~/build
```

This will build SDL in the `~/build` directory.
Installation can be done using:

```sh
cmake --install ~/build --prefix /usr/local        # '--install' requires CMake 3.15, or newer
```

This will install SDL to /usr/local.

## Including SDL in your project

SDL can be included in your project in 2 major ways:
- using a system SDL library, provided by your (*nix) distribution or a package manager
- using a vendored SDL library: this is SDL copied or symlinked in a subfolder.

The following CMake script supports both, depending on the value of `MYGAME_VENDORED`.

```cmake
cmake_minimum_required(VERSION 3.0)
project(mygame)

# Create an option to switch between a system sdl library and a vendored sdl library
option(MYGAME_VENDORED "Use vendored libraries" OFF)

if(MYGAME_VENDORED)
    add_subdirectory(vendored/sdl EXCLUDE_FROM_ALL)
else()
    # 1. Look for a SDL3 package, 2. look for the SDL3 component and 3. fail if none can be found
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
endif()

# Create your game executable target as usual
add_executable(mygame WIN32 mygame.c)

# Link to the actual SDL3 library. SDL3::SDL3 is the shared SDL library, SDL3::SDL3-static is the static SDL libarary.
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


### Using a vendored SDL

This only requires a copy of SDL in a subdirectory.

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

### Pass custom compile options to the compiler

- Use [`CMAKE_<LANG>_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html) to pass extra
flags to the compiler.
- Use [`CMAKE_EXE_LINKER_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_EXE_LINKER_FLAGS.html) to pass extra option to the linker for executables.
- Use [`CMAKE_SHARED_LINKER_FLAGS`](https://cmake.org/cmake/help/latest/variable/CMAKE_SHARED_LINKER_FLAGS.html) to pass extra options to the linker for shared libraries.

#### Examples

- build a SDL library optimized for (more) modern x64 microprocessor architectures.

  With gcc or clang:
    ```sh
    cmake ~/sdl -DCMAKE_C_FLAGS="-march=x86-64-v3" -DCMAKE_CXX_FLAGS="-march=x86-64-v3"
    ```
  With Visual C:
    ```sh
    cmake .. -DCMAKE_C_FLAGS="/ARCH:AVX2" -DCMAKE_CXX_FLAGS="/ARCH:AVX2"
    ```

### iOS/tvOS

CMake 3.14+ natively includes support for iOS and tvOS.  SDL binaries may be built
using Xcode or Make, possibly among other build-systems.

When using a recent version of CMake (3.14+), it should be possible to:

- build SDL for iOS, both static and dynamic
- build SDL test apps (as iOS/tvOS .app bundles)
- generate a working SDL_build_config.h for iOS (using SDL_build_config.h.cmake as a basis)

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

- for QNX/aarch64, using the latest, installed SDK:

    ```cmake
    cmake ~/sdl -DCMAKE_TOOLCHAIN_FILE=~/sdl/build-scripts/cmake-toolchain-qnx-aarch64le.cmake -DSDL_X11=0
    ```
