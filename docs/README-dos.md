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

## System Requirements

| Component | Minimum                        |
| --------- | ------------------------------ |
| CPU       | i386 or higher                 |
| RAM       | 4 MB                           |
| Video     | VGA (256-color mode 13h)       |
| Audio     | Sound Blaster                  |
| DPMI      | CWSDPMI.exe or compatible host |

Higher resolutions (640×480 and above) require a VESA VBE 1.2+ compatible video card.

## Notes

### Memory Model

The DOS port produces 32-bit protected-mode DPMI executables. It uses the "fat DS" nearptr trick (`__djgpp_nearptr_enable()`) to convert physical addresses to usable C pointers. This allows direct access to the VESA linear framebuffer and DMA buffers from C code without segment descriptor manipulation.

SDL on DOS requires the fat DS trick. `SDL_RunApp()` enables it automatically via `SDL_main.h`. If you define `SDL_MAIN_HANDLED`, you must call `__djgpp_nearptr_enable()` yourself before initializing SDL, or video initialization will fail with a clear error message.

### Threading

DOS has no OS-level threads. SDL on DOS implements cooperative threading via a mini-scheduler that uses `setjmp`/`longjmp` for context switching. Threads are never preempted mid-instruction. Context switches occur only at explicit yield points such as `SDL_Delay` and the event pump, so make sure your main loop calls `SDL_PumpEvents` or `SDL_Delay` regularly. `SDL_Delay(0)` yields to other threads without sleeping and is safe to call in tight loops. In some cases, like a load screen, a longer delay (e.g. `SDL_Delay(16)`) may be needed to give background threads enough CPU time.

### Video

The video driver supports VGA mode 13h (320×200×256) on any VGA card, and higher resolutions via VESA BIOS Extensions (VBE 1.2+). Both linear framebuffer (VBE 2.0+) and banked framebuffer modes are supported. Hardware page-flipping is used for tear-free rendering when available.

Only software rendering is supported. There is no GPU renderer.

All video modes are effectively fullscreen. When creating a window, the driver selects the closest available video mode to the requested size.

8-bit indexed color (INDEX8) modes with programmable VGA DAC palettes are supported but will only be used when explicitly requested via the `SDL_PIXELFORMAT` window creation property.

EGA and CGA cards are not supported. The driver detects VGA hardware at initialization and will fail with a clear error message if VGA is not present.

#### Direct Framebuffer Hint

Setting `SDL_HINT_DOS_ALLOW_DIRECT_FRAMEBUFFER` to `"1"` before calling `SDL_GetWindowSurface()` enables a fast path that skips the normal surface copy. `SDL_UpdateWindowSurface()` copies the system-RAM surface directly to VRAM via `dosmemput` and only programs the VGA DAC palette when it changes. Page flipping is done without waiting for vblank. No software cursor compositing is performed.

This mode is designed for applications like Quake that manage their own rendering and want maximum frame throughput. The trade-offs are:

- No vsync. Tearing is expected.
- No software cursor. The application must draw its own cursor if needed.
- Reading back the surface may be slow on real hardware (uncached VRAM).

The hint must be set before the first call to `SDL_GetWindowSurface()`. Changing it after that has no effect.

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
