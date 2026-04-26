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
## Getting PS2 Dev
[Installing PS2 Dev](https://github.com/ps2dev/ps2dev)

## Running on RPCS3 Emulator
[RPCS3](https://github.com/RPCS3/rpcs3)

## To Do
- PS3 Screen Keyboard
- PS3 Screen Mouse
- Handle video mode/resolution change.
- Render target Ex.
