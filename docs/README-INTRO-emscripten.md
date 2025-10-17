
# Introduction to SDL with Emscripten

The easiest way to use SDL is to include it as a subproject in your project.

We'll start by creating a simple project to build and run [hello.c](hello.c)

First, you should have the Emscripten SDK installed from:

https://emscripten.org/docs/getting_started/downloads.html

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

# on Web targets, we need CMake to generate a HTML webpage. 
if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

# Create your game executable target as usual
add_executable(hello WIN32 hello.c)

# Link to the actual SDL3 library.
target_link_libraries(hello PRIVATE SDL3::SDL3)
```

Build:
```sh
emcmake cmake -S . -B build
cd build
emmake make
```

You can now run your app by pointing a webserver at your build directory and connecting a web browser to it, opening hello.html

A more complete example is available at:

https://github.com/Ravbug/sdl3-sample

Additional information and troubleshooting is available in [README-emscripten.md](README-emscripten.md)
