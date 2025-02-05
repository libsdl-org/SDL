# Nokia N-Gage

SDL port for the Nokia N-Gage [Homebrew toolchain](https://github.com/ngagesdk/ngage-toolchain) contributed by:

- [Michael Fitzmayer](https://github.com/mupfdev)

Many thanks to:

- icculus and slouken, who faithfully kept us a place by the fireside!
- The awesome people who ported SDL to other homebrew platforms.
- The Nokia N-Gage [Discord community](https://discord.gg/dbUzqJ26vs)
  who keeps the platform alive.
- To all the staff and supporters of the
  [Suomen pelimuseo](https://www.vapriikki.fi/nayttelyt/fantastinen-floppi/)
  and Heikki Jungmann. You guys are great!

## History

After SDL support was removed because there was no support for C99 at that
time, this version was developed from scratch after the compiler problems
were resolved.

Unlike the previous SDL2 port, this version has a dedicated rendering
backend and a limited audio interface that works, even if it is limited.
Support for the software renderer has been removed.

The result is a much leaner, and much more performant port of SDL that
will hopefully breathe some new life into this obscure platform - that
we love so much.

## Future prospects

The revised toolchain includes EGL 1.3 and OpenGL ES 1.1. A native
integration with SDL is being considered.

## Existing Issues

- If the application is put in the background while sound is playing,
  some of the audio is looped until the app is back in focus.

- It is recommended initialising SDLs audio sub-system even when it
  is not required. The backend is started at a higher level.
  Initialising SDLs audio sub-system ensures that the backend is
  properly deinitialised.
