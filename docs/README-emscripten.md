# Emscripten

(This documentation is not very robust; we will update and expand this later.)

## A quick note about audio

Modern web browsers will not permit web pages to produce sound before the
user has interacted with them; this is for several reasons, not the least
of which being that no one likes when a random browser tab suddenly starts
making noise and the user has to scramble to figure out which and silence
it.

To solve this, most browsers will refuse to let a web app use the audio
subsystem at all before the user has interacted with (clicked on) the page
in a meaningful way. SDL-based apps also have to deal with this problem; if
the user hasn't interacted with the page, SDL_OpenAudioDevice will fail.

There are two reasonable ways to deal with this: if you are writing some
sort of media player thing, where the user expects there to be a volume
control when you mouseover the canvas, just default that control to a muted
state; if the user clicks on the control to unmute it, on this first click,
open the audio device. This allows the media to play at start, the user can
reasonably opt-in to listening, and you never get access denied to the audio
device.

Many games do not have this sort of UI, and are more rigid about starting
audio along with everything else at the start of the process. For these, your
best bet is to write a little Javascript that puts up a "Click here to play!"
UI, and upon the user clicking, remove that UI and then call the Emscripten
app's main() function. As far as the application knows, the audio device was
available to be opened as soon as the program started, and since this magic
happens in a little Javascript, you don't have to change your C/C++ code at
all to make it happen.

Please see the discussion at https://github.com/libsdl-org/SDL/issues/6385
for some Javascript code to steal for this approach.


## Building SDL/emscripten

SDL currently requires at least Emscripten 2.0.32 to build. Newer versions
are likely to work, as well.


Build:

    $ mkdir build
    $ cd build
    $ emconfigure ../configure --host=asmjs-unknown-emscripten --disable-assembly --disable-threads --disable-cpuinfo CFLAGS="-O2"
    $ emmake make

Or with cmake:

    $ mkdir build
    $ cd build
    $ emcmake cmake ..
    $ emmake make

To build one of the tests:

    $ cd test/
    $ emcc -O2 --js-opts 0 -g4 testdraw2.c -I../include ../build/.libs/libSDL2.a ../build/libSDL2_test.a -o a.html

Uses GLES2 renderer or software

Some other SDL2 libraries can be easily built (assuming SDL2 is installed somewhere):

SDL_mixer (http://www.libsdl.org/projects/SDL_mixer/):

    $ EMCONFIGURE_JS=1 emconfigure ../configure
    build as usual...

SDL_gfx (http://cms.ferzkopp.net/index.php/software/13-sdl-gfx):

    $ EMCONFIGURE_JS=1 emconfigure ../configure --disable-mmx
    build as usual...
