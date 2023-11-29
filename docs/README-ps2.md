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
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$PS2DEV/ps2sdk/ps2dev.cmake
cmake --build build
cmake --install build
```

## Hints
The PS2 port has a special Hint for having a dynamic VSYNC. The Hint is `SDL_HINT_PS2_DYNAMIC_VSYNC`.
If you enabled the dynamic vsync having as well `SDL_RENDERER_PRESENTVSYNC` enabled, then if the app is not able to run at 60 FPS, automatically the `vsync` will be disabled having a better performance, instead of dropping FPS to 30.

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

Remember to do a clean compilation everytime you enable or disable the `SDL_PS2_SKIP_IOP_RESET` otherwise the change won't be reflected.

## Getting PS2 Dev
[Installing PS2 Dev](https://github.com/ps2dev/ps2dev)

## Running on PCSX2 Emulator
[PCSX2](https://github.com/PCSX2/pcsx2)

[More PCSX2 information](https://pcsx2.net/)

## To Do
- PS2 Screen Keyboard
- Dialogs
- Others
