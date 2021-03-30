# Nintendo 3DS

SDL port for the Nintendo 3DS [Homebrew toolchain](https://devkitpro.org/) contributed by:

-   [Pierre Wendling](https://github.com/FtZPetruska)

Credits to:

-   The awesome people who ported SDL to other homebrew platforms.
-   The Devkitpro team for making all the tools necessary to achieve this.

## Building

To build for the Nintendo 3DS, make sure you have devkitARM and cmake installed and run:

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

## Notes

-   Currently only software rendering is supported.
-   Window are created on the top screen by default, use the `SDL_WINDOW_N3DS_BOTTOM` flag to put them on the bottom screen.
-   SDL2main should be used to ensure all the necessary services are initialised.
-   By default, the extra L2 cache and higher clock speeds of the New 2/3DS lineup are enabled. If you wish to turn it off, [use the PTMSYSM service](https://libctru.devkitpro.org/ptmsysm_8h.html#ae3a437bfd0de05fbc5ba9a460d148430) to turn it off in your program.
