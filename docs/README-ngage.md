# Nokia N-Gage

SDL port for the Nokia N-Gage
[Homebrew toolchain](https://github.com/ngagesdk/ngage-toolchain)
contributed by:

- [Michael Fitzmayer](https://github.com/mupfdev)

- [Anonymous Maarten](https://github.com/madebr)

Many thanks to:

- icculus and slouken for always making room for us, even when we show up in 2025
 still waving the N-Gage flag.

- The Nokia N-Gage [Discord community](https://discord.gg/dbUzqJ26vs)
 who keeps the platform alive.

- The staff and supporters of the
 [Suomen pelimuseo](https://www.vapriikki.fi/nayttelyt/fantastinen-floppi/), and
 to Heikki Jungmann, for their ongoing love and dedication for the Nokia N-Gage --
 you guys are awesome!

## History

When SDL support was discontinued due to the lack of C99 support at the time,
this version was rebuilt from the ground up after resolving the compiler issues.

In contrast to the earlier SDL2 port, this version features a dedicated rendering
backend and a functional, albeit limited, audio interface.  Support for the
software renderer has been removed.

The outcome is a significantly leaner and more efficient SDL port, which we hope
will breathe new life into this beloved yet obscure platform.

## To the Stubborn Legends of the DC Scene

This port is lovingly dedicated to the ever-nostalgic Dreamcast homebrew scene --
because if we managed to pull this off for the N-Gage (yes, the N-Gage), surely
you guys can stop clinging to SDL2 like it's a rare Shenmue prototype and finally
make the leap to SDL3.  It's 2025, not 1999 -- and let's be honest, you're rocking
a state-of-the-art C23 compiler.  The irony writes itself.

## Existing Issues and Limitations

- For now, the new
 [SDL3 main callbacks](https://wiki.libsdl.org/SDL3/README/main-functions#how-to-use-main-callbacks-in-sdl3)
 are not optional and must be used. This is important as the callbacks
 are optional on other platforms.

- If the application is put in the background while sound is playing,
 some of the audio is looped until the app is back in focus.

- It is recommended initialising SDLs audio sub-system even when it
 is not required. The backend is started at a higher level.  Initialising
 SDLs audio sub-system ensures that the backend is properly deinitialised.

- Because the audio sample rate can change during phone calls, the sample
 rate is currently fixed at 8kHz to ensure stable behavior.  Although
 dynamically adjusting the sample rate is theoretically possible, the
 current implementation doesn't support it yet.  This limitation is
 expected to be resolved in a future update.

- Dependency tracking is currently non-functional.
