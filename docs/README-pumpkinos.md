# PumpkinOS

[PumpkinOS](https://github.com/migueletto/PumpkinOS) is a re-implementation
of PalmOS that runs on Linux, macOS, Windows and a few embedded targets.
Applications are PalmOS programs: on the desktop they are shared libraries
(`.dlib`) with a `PilotMain()` entry point, packaged in a `.prc` and loaded
into the PumpkinOS process, where each application runs as a task with its
own screen.

SDL on PumpkinOS is such an application library. SDL keeps using the host
system for threads, timers, files and dynamic loading, and adds three
PumpkinOS-specific drivers:

- **Video** (`pumpkinos`): the task screen is the SDL window. Applications
  draw through `SDL_GetWindowSurface()` / `SDL_UpdateWindowSurface()` or the
  software renderer; the dirty rectangles are copied into the task screen and
  uploaded by PumpkinOS. There is no OpenGL or Vulkan.
- **Audio** (`pumpkinos`): playback through the PalmOS sound stream API
  (`SndStreamCreate`), in 8-bit, 16-bit or 32-bit PCM, mono or stereo.
  Recording is not available.
- **Joystick** (`PumpkinOS Hard Keys`): the PalmOS hard keys as one gamepad.
  The four application buttons are A/B/X/Y, the power key is Start and the
  up/down/left/right keys form the d-pad. On the desktop these are F1..F4,
  End and the arrow keys.

Keyboard and pen input come through the video driver: the pen is the mouse
(a right click is `penDownRightEvent`), keys are reported with scancodes of
a US layout and printable characters are delivered as text input.

## Building SDL

You need a PumpkinOS source tree, built for your host, so that
`bin/libpumpkin` and `bin/libpit` exist. Then:

```sh
cmake -S . -B build -DSDL_PUMPKINOS=ON -DSDL_PUMPKINOS_DIR=/path/to/PumpkinOS
cmake --build build
```

`SDL_PUMPKINOS=ON` adds the three drivers to an otherwise normal build for
the host platform and defines `__PUMPKINOS__` for SDL and for everything
that links against it. Because a PumpkinOS application is itself a shared
library, SDL is built as a static library by default; pass `-DSDL_SHARED=ON`
if you want `libSDL3` as a separate shared library as well. The host window
system drivers (Cocoa, Metal, Vulkan) are left out, since the task screen is
the only window. The PumpkinOS drivers are registered first, so they are
chosen over other host drivers (X11, CoreAudio, ...) that may also be
compiled in; you can turn those off with the usual options
(`-DSDL_X11=OFF`, `-DSDL_COREAUDIO=OFF`, and so on) to keep the library
small.

With `SDL_TESTS=ON` the test programs are built as PumpkinOS applications
too: each one is a `<test>.dlib` with a `PilotMain()` entry point, and when
`tools/prcbuild` exists in the PumpkinOS tree it is packaged as
`build/test/<test>.prc`, ready to be copied into `vfs/app_install`. CTest
cannot run them, so no tests are registered with it. The test programs
expect their data files (`icon.bmp`, `sample.wav`, ...) next to the running
PumpkinOS binary, where `SDL_GetBasePath()` points.

## Building an application

Compile your program with `__PUMPKINOS__` defined (CMake does this for you
when you link `SDL3::SDL3`). `SDL_main.h` then provides `PilotMain()`, which
runs your `main()` on a normal launch (`SDL_MAIN_USE_CALLBACKS` works the
same way). There is no command line: `main()` receives `argc` of 1 and the
application name in `argv[0]`. Link the program as a shared library
together with SDL, `libpumpkin` and `libpit`, and package it as a `.prc`
with the PumpkinOS tools, exactly like the applications in the PumpkinOS
tree (see `src/app.mak` and the `Doom` or `gnuboy` ports):

```sh
cc -shared -fvisibility=hidden -D__PUMPKINOS__ -I/path/to/SDL/include \
   -o MyGame.dlib *.c -L/path/to/SDL/build -lSDL3 \
   -L/path/to/PumpkinOS/bin -lpumpkin -lpit
cp MyGame.dlib dlib004a.bin
prcbuild -f MyGame.prc -t appl -c MyGm -n "MyGame" dlib004a.bin resources/*
cp MyGame.prc /path/to/PumpkinOS/vfs/app_install/
```

The name of the dlib resource encodes the host: `dlibNNNN.bin` with
`NNNN` the hexadecimal value of `os * 64 + cpu * 8 + size`, where `os` is 1
for Linux and macOS, `cpu` is 1 for ARM, 2 for x86 and 3 for RISC-V, and
`size` is 1 for 32-bit and 2 for 64-bit. So macOS or Linux on ARM64 is
`dlib004a.bin` and Linux on x86_64 is `dlib0052.bin`; Windows uses fixed
numbers, see `src/commonp.mak` in the PumpkinOS tree. The PumpkinOS
launcher installs the `.prc` from `vfs/app_install` on the next start.

If you provide your own `PilotMain()`, define `SDL_MAIN_HANDLED` and call
`SDL_RunApp()` yourself.

## Notes and limitations

- One window per application. Its size is the size of the task screen.
  `SDL_CreateWindow()` and `SDL_SetWindowSize()` ask PumpkinOS to resize the
  task screen and wait briefly for it; when the request is not honored
  (single-application mode, embedded targets) the window keeps the screen
  size and a `SDL_EVENT_WINDOW_RESIZED` event reports it.
- PalmOS API calls are only valid on the task thread. `SDL_Init()`,
  window creation, event pumping and `SDL_OpenAudioDevice()` /
  `SDL_CloseAudioDevice()` must be called from the thread that runs
  `PilotMain()`. SDL's own threads (audio mixing, timers) do not call into
  PalmOS.
- `SDL_EVENT_QUIT` is sent when PumpkinOS stops the application (window
  closed, launcher key, shutdown). The application should return from
  `main()` promptly after that.
- Pen position is polled from PumpkinOS, so mouse motion is reported at the
  rate the application pumps events.
- The screen pixel format follows the PumpkinOS configuration: RGB565 for a
  16-bit display, ARGB or ABGR for a 32-bit one. Palette, gray and mono
  screens are converted pixel by pixel.
- Clipboard text is shared with the PumpkinOS host window.
