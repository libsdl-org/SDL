# Emscripten

## The state of things

(As of September 2023, but things move quickly and we don't update this
document often.)

In modern times, all the browsers you probably care about (Chrome, Firefox,
Edge, and Safari, on Windows, macOS, Linux, iOS and Android), support some
reasonable base configurations:

- WebAssembly (don't bother with asm.js any more)
- WebGL (which will look like OpenGL ES 2 or 3 to your app).
- Threads (see caveats, though!)
- Game controllers
- Autoupdating (so you can assume they have a recent version of the browser)

All this to say we're at the point where you don't have to make a lot of
concessions to get even a fairly complex SDL-based game up and running.


## RTFM

This document is a quick rundown of some high-level details. The
documentation at [emscripten.org](https://emscripten.org/) is vast
and extremely detailed for a wide variety of topics, and you should at
least skim through it at some point.


## Porting your app to Emscripten

Many many things just need some simple adjustments and they'll compile
like any other C/C++ code, as long as SDL was handling the platform-specific
work for your program.

First, you probably need this in at least one of your source files:

```c
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
```

Second: assembly language code has to go. Replace it with C. You can even use
[x86 SIMD intrinsic functions in Emscripten](https://emscripten.org/docs/porting/simd.html)!

Third: Middleware has to go. If you have a third-party library you link
against, you either need an Emscripten port of it, or the source code to it
to compile yourself, or you need to remove it.

Fourth: You still start in a function called main(), but you need to get out of
it and into a function that gets called repeatedly, and returns quickly,
called a mainloop.

Somewhere in your program, you probably have something that looks like a more
complicated version of this:

```c
void main(void)
{
    initialize_the_game();
    while (game_is_still_running) {
        check_for_new_input();
        think_about_stuff();
        draw_the_next_frame();
    }
    deinitialize_the_game();
}
```

This will not work on Emscripten, because the main thread needs to be free
to do stuff and can't sit in this loop forever. So Emscripten lets you set up
a [mainloop](https://emscripten.org/docs/porting/emscripten-runtime-environment.html#browser-main-loop).

```c
static void mainloop(void)   /* this will run often, possibly at the monitor's refresh rate */
{
    if (!game_is_still_running) {
        deinitialize_the_game();
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();  /* this should "kill" the app. */
        #else
        exit(0);
        #endif
    }

    check_for_new_input();
    think_about_stuff();
    draw_the_next_frame();
}

void main(void)
{
    initialize_the_game();
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainloop, 0, 1);
    #else
    while (1) { mainloop(); }
    #endif
}
```

Basically, `emscripten_set_main_loop(mainloop, 0, 1);` says "run
`mainloop` over and over until I end the program." The function will
run, and return, freeing the main thread for other tasks, and then
run again when it's time. The `1` parameter does some magic to make
your main() function end immediately; this is useful because you
don't want any shutdown code that might be sitting below this code
to actually run if main() were to continue on, since we're just
getting started.

There's a lot of little details that are beyond the scope of this
document, but that's the biggest intial set of hurdles to porting
your app to the web.


## Do you need threads?

If you plan to use threads, they work on all major browsers now. HOWEVER,
they bring with them a lot of careful considerations. Rendering _must_
be done on the main thread. This is a general guideline for many
platforms, but a hard requirement on the web.

Many other things also must happen on the main thread; often times SDL
and Emscripten make efforts to "proxy" work to the main thread that
must be there, but you have to be careful (and read more detailed
documentation than this for the finer points).

Even when using threads, your main thread needs to set an Emscripten
mainloop that runs quickly and returns, or things will fail to work
correctly.

You should definitely read [Emscripten's pthreads docs](https://emscripten.org/docs/porting/pthreads.html)
for all the finer points. Mostly SDL's thread API will work as expected,
but is built on pthreads, so it shares the same little incompatibilities
that are documented there, such as where you can use a mutex, and when
a thread will start running, etc.


IMPORTANT: You have to decide to either build something that uses
threads or something that doesn't; you can't have one build
that works everywhere. This is an Emscripten (or maybe WebAssembly?
Or just web browsers in general?) limitation. If you aren't using
threads, it's easier to not enable them at all, at build time.

If you use threads, you _have to_ run from a web server that has
[COOP/COEP headers set correctly](https://web.dev/why-coop-coep/)
or your program will fail to start at all.

If building with threads, `__EMSCRIPTEN_PTHREADS__` will be defined
for checking with the C preprocessor, so you can build something
different depending on what sort of build you're compiling.


## Audio

Audio works as expected at the API level, but not exactly like other
platforms.

You'll only see a single default audio device. Audio capture also works;
if the browser pops up a prompt to ask for permission to access the
microphone, the SDL_OpenAudioDevice call will succeed and start producing
silence at a regular interval. Once the user approves the request, real
audio data will flow. If the user denies it, the app is not informed and
will just continue to receive silence.

SDL3 allows you to open the audio device at any time; it'll just throw away
audio data until the user interacts with the page, and then start feeding
the browser seamlessly, but for SDL2, you need to manage this yourself.

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

SDL3 allows you to open the audio device at any time; it'll just throw away
audio data until the user interacts with the page, and then start feeding
the browser seamlessly, but for SDL2, you need to manage this yourself.


## Rendering

If you use SDL's 2D render API, it will use GLES2 internally, which
Emscripten will turn into WebGL calls. You can also use OpenGL ES 2
directly by creating a GL context and drawing into it.

Calling SDL_RenderPresent (or SDL_GL_SwapWindow) will not actually
present anything on the screen until your return from your mainloop
function.


## Building SDL/emscripten

First: do you _really_ need to build SDL from source?

If you aren't developing SDL itself, have a desire to mess with its source
code, or need something on the bleeding edge, don't build SDL. Just use
Emscripten's packaged version!

Compile and link your app with `-sUSE_SDL=2` and it'll use a build of
SDL packaged with Emscripten. This comes from the same source code and
fixes the Emscripten project makes to SDL are generally merged into SDL's
revision control, so often this is much easier for app developers.

`-sUSE_SDL=1` will select Emscripten's JavaScript reimplementation of SDL
1.2 instead; if you need SDL 1.2, this might be fine, but we generally
recommend you don't use SDL 1.2 in modern times.


If you want to build SDL, though...

SDL currently requires at least Emscripten 3.1.35 to build. Newer versions
are likely to work, as well.


Build:

This works on Linux/Unix and macOS. Please send comments about Windows.

Make sure you've [installed emsdk](https://emscripten.org/docs/getting_started/downloads.html)
first, and run `source emsdk_env.sh` at the command line so it finds the
tools.

(These configure options might be overkill, but this has worked for me.)

```bash
cd SDL
mkdir build
cd build
emconfigure ../configure --host=wasm32-unknown-emscripten --disable-pthreads --disable-assembly --disable-cpuinfo CFLAGS="-sUSE_SDL=0 -O3"
emmake make -j4
```

If you want to build with thread support, something like this works:

```bash
emconfigure ../configure --host=wasm32-unknown-emscripten --enable-pthreads --disable-assembly --disable-cpuinfo CFLAGS="-sUSE_SDL=0 -O3 -pthread" LDFLAGS="-pthread"
```

Or with cmake:

```bash
mkdir build
cd build
emcmake cmake ..
emmake make -j4
```

To build one of the tests:

```bash
cd test/
emcc -O2 --js-opts 0 -g4 testdraw2.c -I../include ../build/.libs/libSDL2.a ../build/libSDL2_test.a -o a.html
```

## Building your app

You need to compile with `emcc` instead of `gcc` or `clang` or whatever, but
mostly it uses the same command line arguments as Clang.

Link against the SDL/build/.libs/libSDL2.a file you generated by building SDL,
link with `-sUSE_SDL=2` to use Emscripten's prepackaged SDL2 build.

Usually you would produce a binary like this:

```bash
gcc -o mygame mygame.c  # or whatever
```

But for Emscripten, you want to output something else:

```bash
emcc -o index.html mygame.c
```

This will produce several files...support Javascript and WebAssembly (.wasm)
files. The `-o index.html` will produce a simple HTML page that loads and
runs your app. You will (probably) eventually want to replace or customize
that file and do `-o index.js` instead to just build the code pieces.

If you're working on a program of any serious size, you'll likely need to
link with `-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1gb` to get access
to more memory. If using pthreads, you'll need the `-sMAXIMUM_MEMORY=1gb`
or the app will fail to start on iOS browsers, but this might be a bug that
goes away in the future.



## Debugging

Debugging web apps is a mixed bag. You should compile and link with
`-gsource-map`, which embeds a ton of source-level debugging information into
the build, and make sure _the app source code is available on the web server_,
which is often a scary proposition for various reasons.

When you debug from the browser's tools and hit a breakpoint, you can step
through the actual C/C++ source code, though, which can be nice.

If you try debugging in Firefox and it doesn't work well for no apparent
reason, try Chrome, and vice-versa. Sometimes it's just like that.

SDL_Log() will write to the Javascript console, and honestly I find
printf-style debugging to be easier than setting up a build for proper
debugging, so use whatever tools work best for you.


## Questions?

Please give us feedback on this document at [the SDL bug tracker](https://github.com/libsdl-org/SDL/issues).
If something is wrong or unclear, we want to know!



