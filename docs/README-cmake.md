# CMake

[www.cmake.org](https://www.cmake.org/)

The CMake build system is supported on the following platforms:

* FreeBSD
* Linux
* Microsoft Visual C
* MinGW and Msys
* macOS, iOS, and tvOS, with support for XCode
* Android
* Emscripten
* FreeBSD
* Haiku
* Nintendo 3DS
* Playstation 2
* Playstation Vita
* QNX 7.x/8.x
* RiscOS

## Building SDL

Assuming the source tree of SDL is located at `~/sdl`,
this will configure and build SDL in the `~/build` directory:
```sh
cmake -S ~/sdl -B ~/build
cmake --build ~/build
```

Installation can be done using:
```sh
cmake --install ~/build --prefix /usr/local        # '--install' requires CMake 3.15, or newer
```

This will install SDL to /usr/local.

### Building SDL tests

You can build the SDL test programs by adding `-DSDL_TESTS=ON` to the first cmake command above:
```sh
cmake -S ~/sdl -B ~/build -DSDL_TEST_LIBRARY=ON -DSDL_TESTS=ON
```
and then building normally. In this example, the test programs will be built and can be run from `~/build/tests/`.

## Including SDL in your project

SDL can be included in your project in 2 major ways:
- using a system SDL library, provided by your (*nix) distribution or a package manager
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

By default, only a shared SDL library is built and installed.
The options `-DSDL_SHARED=` and `-DSDL_STATIC=` accept boolean values to change this.

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

## Help, it doesn't work!

Below, a SDL3 CMake project can be found that builds 99.9% of time (assuming you have internet connectivity).
When you have a problem with building or using SDL, please modify it until it reproduces your issue.

```cmake
cmake_minimum_required(VERSION 3.16)
project(sdl_issue)

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!!!!!                                                                            !!!!!!
# !!!!!!     This CMake script is not using "CMake best practices".                 !!!!!!
# !!!!!!                 Don't use it in your project.                              !!!!!!
# !!!!!!                                                                            !!!!!!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

# 1. Try system SDL3 package first
find_package(SDL3 QUIET)
if(SDL3_FOUND)
    message(STATUS "Using SDL3 via find_package")
endif()

# 2. Try using a vendored SDL library
if(NOT SDL3_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/SDL/CMakeLists.txt")
    add_subdirectory(SDL)
    message(STATUS "Using SDL3 via add_subdirectory")
    set(SDL3_FOUND TRUE)
endif()

# 3. Download SDL, and use that.
if(NOT SDL3_FOUND)
    include(FetchContent)
    set(SDL_SHARED TRUE CACHE BOOL "Build a SDL shared library (if available)")
    set(SDL_STATIC TRUE CACHE BOOL "Build a SDL static library (if available)")
    FetchContent_Declare(
        SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG main  # Replace this with a particular git tag or git hash
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    message(STATUS "Using SDL3 via FetchContent")
    FetchContent_MakeAvailable(SDL)
    set_property(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_deps/sdl-src" PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

file(WRITE main.c [===========================================[
/**
 * Modify this source such that it reproduces your problem.
 */

/* START of source modifications */

#include <SDL3/SDL.h>

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;

    if (SDL_CreateWindowAndRenderer(640, 480, 0, &window, &renderer) < 0) {
        SDL_Log("SDL_CreateWindowAndRenderer failed (%s)", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowTitle(window, "SDL issue");

    while (1) {
        int finished = 0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                finished = 1;
                break;
            }
        }
        if (finished) {
            break;
        }

        SDL_SetRenderDrawColor(renderer, 80, 80, 80, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

/* END of source modifications */

]===========================================])

add_executable(sdl_issue main.c)

target_link_libraries(sdl_issue PRIVATE SDL3::SDL3)
# target_link_libraries(sdl_issue PRIVATE SDL3::SDL3-shared)
# target_link_libraries(sdl_issue PRIVATE SDL3::SDL3-static)
```
