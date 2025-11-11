PS2
======
SDL port for the Sony Playstation 2 contributed by:
- Francisco Javier Trujillo Mata


Credit to
   - The guys that ported SDL to PSP & Vita because I'm taking them as reference.
   - David G. F. for helping me with several issues and tests.

## Building
To build SDL library for the PS2, make sure you have the latest PS2Dev status and run:
```bash
export PS2DEV=/usr/local/ps2dev # or wherever your ps2dev installation is
export PS2SDK=$PS2DEV/ps2sdk
export PATH=$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$PS2DEV/share/ps2dev.cmake
cmake --build build
cmake --install build
```

## Hints
- `SDL_HINT_PS2_GS_WIDTH`: Width of the framebuffer. Defaults to 640.
- `SDL_HINT_PS2_GS_HEIGHT`: Height of the framebuffer. Defaults to 448.
- `SDL_HINT_PS2_GS_PROGRESSIVE`: Whether to use progressive, instead of interlaced. Defaults to 0.
- `SDL_HINT_PS2_GS_MODE`: Regional standard of the signal. "NTSC" (60hz), "PAL" (50hz) or "" (the console's region, default).

## Notes
If you trying to debug a SDL app through [ps2client](https://github.com/ps2dev/ps2client) you need to avoid the IOP reset, otherwise you will lose the connection with your computer.
So to avoid the reset of the IOP CPU, you need to call to the macro `SDL_PS2_SKIP_IOP_RESET();`.
It could be something similar as:
```c
.....

SDL_PS2_SKIP_IOP_RESET();

int main(int argc, char *argv[])
{
.....
```
For a release binary is recommendable to reset the IOP always.

Remember to do a clean compilation every time you enable or disable the `SDL_PS2_SKIP_IOP_RESET` otherwise the change won't be reflected.

## Getting PS2 Dev
[Installing PS2 Dev](https://github.com/ps2dev/ps2dev)

## Running on PCSX2 Emulator
[PCSX2](https://github.com/PCSX2/pcsx2)

[More PCSX2 information](https://pcsx2.net/)

## To Do
- PS2 Screen Keyboard
- Dialogs
- Others
