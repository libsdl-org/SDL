QNX
=======

SDL port for QNX, providing both screen and Wayland video backends.

This was originally contributed by Elad Lahav for QNX 7.0.

The port was later improved and adapted for QNX 8.0 by:
- Ethan Leir
- Roberto Speranza
- Darcy Phipps
- Jai Moraes
- Pierce McKinnon

Further changes to enable Wayland with the EGL backend were made by Felix Xing
and Aaron Bassett.


## Building

Building SDL3 for QNX requires Wayland to be built and installed. The commands
to build it are,
```bash
# Note, if you're cross-compiling, you will need to source qnxsdp-env.sh and
# provide the path to a cmake toolchain file with -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_DIR/qnx.nto.toolchain.cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSDL_X11=0
cmake --build build
cmake --install build
```

## QNX self-hosted

QNX provides a self-hosted environment, available with [the free license](https://www.qnx.com/products/everywhere/).
This is the easiest way to get your hands on SDL.

## QNX build-files

You can find the cross-compiled build tools at https://github.com/qnx-ports/build-files

## Notes - screen

- Currently, only software and OpenGLES2 rendering is supported.
- Unless your application is managed by a window manager capable of closing the application, you will need to quit it yourself.
- Restraining the mouse to a window or warping the mouse cursor will not work.
