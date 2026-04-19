# DOS

SDL port for MS-DOS using the [DJGPP](https://www.delorie.com/djgpp/) GCC cross-compiler.

## Building

To build for DOS, make sure you have a DJGPP cross-compiler in your PATH and run:

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=build-scripts/i586-pc-msdosdjgpp.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The toolchain file looks for `i586-pc-msdosdjgpp-gcc` or `i386-pc-msdosdjgpp-gcc` in PATH.

## Running

DOS executables require a DPMI host. Place `CWSDPMI.EXE` next to your executable.

To run in DOSBox:

```bash
dosbox myapp.exe
```

## Notes

### Memory Model

The DOS port produces 32-bit protected-mode DPMI executables. It uses the "fat DS" nearptr trick (`__djgpp_nearptr_enable()`) to convert physical addresses to usable C pointers. This allows direct access to the VESA linear framebuffer and DMA buffers from C code without segment descriptor manipulation.

SDL on DOS requires the fat DS trick. `SDL_RunApp()` enables it automatically via `SDL_main.h`. If you define `SDL_MAIN_HANDLED`, you must call `__djgpp_nearptr_enable()` yourself before initializing SDL, or video initialization will fail with a clear error message.

### Threading

DOS has no OS-level threads. SDL on DOS implements cooperative threading via a mini-scheduler that uses `setjmp`/`longjmp` for context switching. Threads are never preempted mid-instruction. Context switches occur only at explicit yield points such as `SDL_Delay` and the event pump, so make sure your main loop calls `SDL_PumpEvents` or `SDL_Delay` regularly. `SDL_Delay(0)` yields to other threads without sleeping and is safe to call in tight loops. In some cases, like a load screen, a longer delay (e.g. `SDL_Delay(16)`) may be needed to give background threads enough CPU time.

### Video

The video driver uses VESA BIOS Extensions (VBE). Both linear framebuffer (VBE 2.0+) and banked framebuffer (VBE 1.2+) modes are supported. Hardware page-flipping is used for tear-free rendering when available.

Only software rendering is supported. There is no GPU renderer.

All video modes are effectively fullscreen. When creating a window, the driver selects the closest available VESA mode to the requested size.

8-bit indexed color (INDEX8) modes with programmable VGA DAC palettes are supported but will only be used when explicitly requested via the `SDL_PIXELFORMAT` window creation property.

### Audio

Sound Blaster support is available for SB16 (16-bit stereo), SB Pro (8-bit stereo), and SB 2.0/1.x (8-bit mono). Configured automatically from the `BLASTER` environment variable.

A ring buffer sits between the SDL audio pipeline and the DMA hardware. The audio thread fills the ring buffer cooperatively, and the Sound Blaster IRQ handler copies data from the ring buffer to the DMA buffer directly. This gives roughly 45 ms of cushion before audio would stutter. If your game runs at 22 fps or above, audio will be glitch-free with no extra effort. Below 20 fps, adding a `SDL_Delay(0)` in the middle of your game loop should be enough to keep the ring buffer fed.

Audio recording is not implemented.

### Input

- Keyboard: IRQ1-driven with full extended scancode (0xE0 prefix) support.
- Mouse: INT 33h mouse driver with relative motion via mickeys.
- Joystick: gameport joystick via BIOS INT 15h (axes) and direct port 0x201 reads (buttons) with software calibration.

### Limitations

- No shared library / dynamic loading support (no `SDL_LoadObject`). DXE support may be added in the future.
