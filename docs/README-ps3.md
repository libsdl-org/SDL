PS3
======
SDL port for the Sony Playstation 3 contributed by:
- 16rom.com


Credit to
   - Developers of the PSL1GHT library.
   - Developers of PSL1GHT ports for SDL2.
   - Developers of ps3toolchain.
   - Developers of ps3libraries. 

## Building

**ATTENTION**: repositories in the links are prive and not merged yet into official ps3dev repos. Something could not work from the first time. 

First you need to setup [ps3toolchain](https://github.com/onesixromcom/ps3toolchain). New fork was created to fix build on the latest Linux system.

There's also referenced [ps3libraries](https://github.com/onesixromcom/ps3libraries) from ps3toolchain which builds SDL3, SDL3_mixer and SDL3_ttf.

Example PS3 program that builds with cmake [https://github.com/onesixromcom/sdl3-ps3-example](https://github.com/onesixromcom/sdl3-ps3-example)

Add variables to .bashrc
```bash
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin
export PATH=$PATH:$PS3DEV/ppu/bin
export PATH=$PATH:$PS3DEV/spu/bin
````

More information could be found in ps3toolchain for setup guide.

## Notes

Use ps3loadx installed on PS3 and ps3load compiled from ps3toolchain to run and debug code on real PS3.

## Getting PS3 Dev

[Installing PS3 Dev](https://github.com/ps3dev/ps3dev)

[Official PS3 libraries](https://github.com/ps3dev/ps3libraries)

[Official PS3 toolchain](https://github.com/ps3dev/ps3toolchain)

## Running on RPCS3 Emulator

[RPCS3](https://github.com/RPCS3/rpcs3)

## To Do
- Handle video mode/resolution change
- Filesystem
- Handle PS3 joystick via SDL_hidapi_ps3
- Add blend modes support
- Implement PS3_SetRenderTarget
