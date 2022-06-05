PS2
======
SDL2 port for the Sony Playstation 2 contributed by:
- Francisco Javier Trujillo Mata


Credit to
   - The guys that ported SDL to PSP & Vita because I'm taking them as reference.
   - David G. F. for helping me with several issues and tests.

## Building
To build SDL2 library for the PS2, make sure you have the latest PS2Dev status and run:
```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$PS2DEV/ps2sdk/ps2dev.cmake
cmake --build build
cmake --install build
```


## Getting PS2 Dev
[Installing PS2 Dev](https://github.com/ps2dev/ps2dev)

## Running on PCSX2 Emulator
[PCSX2](https://github.com/PCSX2/pcsx2)

[More PCSX2 information](https://pcsx2.net/)

## To Do
- PS2 Screen Keyboard
- Dialogs
- Audio
- Video
- Controllers
- Others