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
-   SDL2main should be used to ensure ROMFS is enabled.
-   By default, the extra L2 cache and higher clock speeds of the New 2/3DS lineup are enabled. If you wish to turn it off, use `osSetSpeedupEnable(false)` in your main function.
-   `SDL_GetBasePath` returns the romfs root instead of the executable's directory.
-   The Nintendo 3DS uses a cooperative threading model on a single core, meaning a thread will never yield unless done manually through the `SDL_Delay` functions, or blocking waits (`SDL_LockMutex`, `SDL_SemWait`, `SDL_CondWait`, `SDL_WaitThread`). To avoid starving other threads, `SDL_SemTryWait` and `SDL_SemWaitTimeout` will yield if they fail to acquire the semaphore, see https://github.com/libsdl-org/SDL/pull/6776 for more information.
