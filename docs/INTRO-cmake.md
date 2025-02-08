
# Introduction to SDL with CMake

The easiest way to use SDL is to include it as a subproject in your project.

We'll start by creating a simple project to build and run [hello.c](hello.c)

Create the file CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(hello)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(hello WIN32 hello.c)

# Link to the actual SDL3 library.
target_link_libraries(hello PRIVATE SDL3::SDL3)
```

Build:
```sh
cmake -S . -B build
cmake --build build
```

Run:
- On Windows the executable is in the build Debug directory:
```sh
cd build/Debug
./hello
``` 
- On other platforms the executable is in the build directory:
```sh
cd build
./hello
```

A more complete example is available at:

https://github.com/Ravbug/sdl3-sample

Additional information and troubleshooting is available in [README-cmake.md](README-cmake.md)
