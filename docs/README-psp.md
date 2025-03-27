PSP
======
SDL port for the Sony PSP contributed by:
- Captian Lex
- Francisco Javier Trujillo Mata
- Wouter Wijsman


Credit to
   Marcus R.Brown,Jim Paris,Matthew H for the original SDL 1.2 for PSP
   Geecko for his PSP GU lib "Glib2d"

## Building
To build SDL library for the PSP, make sure you have the latest PSPDev status and run:
```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$PSPDEV/psp/share/pspdev.cmake
cmake --build build
cmake --install build
```


## Getting PSP Dev
[Installing PSP Dev](https://github.com/pspdev/pspdev)

## Running on PPSSPP Emulator
[PPSSPP](https://github.com/hrydgard/ppsspp)

[Build Instructions](https://github.com/hrydgard/ppsspp/wiki/Build-instructions)


## Compiling a HelloWorld
[PSP Hello World](https://pspdev.github.io/basic_programs.html#hello-world)

## To Do
- PSP Screen Keyboard
- Dialogs
