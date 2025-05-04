# Nokia N-Gage

SDL port for the Nokia N-Gage
[Homebrew toolchain](https://github.com/ngagesdk/ngage-toolchain)
contributed by:

- [Michael Fitzmayer](https://github.com/mupfdev)

- [Anonymous Maarten](https://github.com/madebr)

Many thanks to:

- icculus and slouken, who faithfully kept us a place by the fireside!

- The awesome people who ported SDL to other homebrew platforms.

- The Nokia N-Gage [Discord community](https://discord.gg/dbUzqJ26vs)
 who keeps the platform alive.

- To all the staff and supporters of the
 [Suomen pelimuseo](https://www.vapriikki.fi/nayttelyt/fantastinen-floppi/)
 and Heikki Jungmann.  You guys are great!

## History

When SDL support was discontinued due to the lack of C99 support at the time,
this version was rebuilt from the ground up after resolving the compiler issues.

In contrast to the earlier SDL2 port, this version features a dedicated rendering
backend and a functional, albeit limited, audio interface.  Support for the
software renderer has been removed.

The outcome is a significantly leaner and more efficient SDL port, which we hope
will breathe new life into this beloved yet obscure platform.

## Existing Issues  

- For now, the new
 [SDL3 main callbacks](https://wiki.libsdl.org/SDL3/README/main-functions#how-to-use-main-callbacks-in-sdl3)
 are not optional and must be used. This is important as the callbacks
 are optional on other platforms.

- If the application is put in the background while sound is playing,
 some of the audio is looped until the app is back in focus.

- It is recommended initialising SDLs audio sub-system even when it
 is not required. The backend is started at a higher level.  Initialising
 SDLs audio sub-system ensures that the backend is properly deinitialised.
