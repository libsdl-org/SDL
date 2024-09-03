# Where an SDL program starts running.

## History

SDL has a long, complicated history with starting a program.

In most of the civilized world, an application starts in a C-callable
function named "main". You probably learned it a long time ago:

```c
int main(int argc, char **argv)
{
    printf("Hello world!\n");
    return 0;
}
```

But not all platforms work like this. Windows apps might want a different
function named "WinMain", for example, so SDL set out to paper over this
difference.

Generally how this would work is: your app would always use the "standard"
`main(argc, argv)` function as its entry point, and `#include` the proper
SDL header before that, which did some macro magic. On platforms that used
a standard `main`, it would do nothing and what you saw was what you got.

But those other platforms! If they needed something that _wasn't_ `main`,
SDL's macro magic would quietly rename your function to `SDL_main`, and
provide its own entry point that called it. Your app was none the wiser and
your code worked everywhere without changes.


## The main entry point in SDL3

Previous versions of SDL had a static library, SDLmain, that you would link
your app against. SDL3 still has the same macro tricks, but the static library
is gone. Now it's supplied by a "single-header library," which means you
`#include <SDL3/SDL_main.h>` and that header will insert a small amount of
code into the source file that included it, so you no longer have to worry
about linking against an extra library that you might need on some platforms.
You just build your app and it works.

You should _only_ include SDL_main.h from one file (the umbrella header,
SDL.h, does _not_ include it), and know that it will `#define main` to
something else, so if you use this symbol elsewhere as a variable name, etc,
it can cause you unexpected problems.

SDL_main.h will also include platform-specific code (WinMain or whatnot) that
calls your _actual_ main function. This is compiled directly into your
program.

If for some reason you need to include SDL_main.h in a file but also _don't_
want it to generate this platform-specific code, you should define a special
macro before including the header:


```c
#define SDL_MAIN_NOIMPL
```

If you are moving from SDL2, remove any references to the SDLmain static
library from your build system, and you should be done. Things should work as
they always have.

If you have never controlled your process's entry point (you are using SDL
as a module from a general-purpose scripting language interpreter, or you're
using SDL in a plugin for some otherwise-unrelated app), then there is nothing
required of you here; there is no startup code in SDL's entry point code that
is required, so using SDL_main.h is completely optional. Just start using
the SDL API when you are ready.


## Main callbacks in SDL3

There is a second option in SDL3 for how to structure your program. This is
completely optional and you can ignore it if you're happy using a standard
"main" function.

Some platforms would rather your program operate in chunks. Most of the time,
games tend to look like this at the highest level:

```c
int main(int argc, char **argv)
{
    initialize();
    while (keep_running()) {
        handle_new_events();
        do_one_frame_of_stuff();
    }
    deinitialize();
}
```

There are platforms that would rather be in charge of that `while` loop:
iOS would rather you return from main() immediately and then it will let you
know that it's time to update and draw the next frame of video. Emscripten
(programs that run on a web page) absolutely requires this to function at all.
Video targets like Wayland can notify the app when to draw a new frame, to
save battery life and cooperate with the compositor more closely.

In most cases, you can add special-case code to your program to deal with this
on different platforms, but SDL3 offers a system to handle this transparently on
the app's behalf.

To use this, you have to redesign the highest level of your app a little. Once
you do, it'll work on all supported SDL platforms without problems and
`#ifdef`s in your code.

Instead of providing a "main" function, under this system, you would provide
several functions that SDL will call as appropriate.

Using the callback entry points works on every platform, because on platforms
that don't require them, we can fake them with a simple loop in an internal
implementation of the usual SDL_main.

The primary way we expect people to write SDL apps is still with SDL_main, and
this is not intended to replace it. If the app chooses to use this, it just
removes some platform-specific details they might have to otherwise manage,
and maybe removes a barrier to entry on some future platform. And you might
find you enjoy structuring your program like this more!


## How to use main callbacks in SDL3

To enable the callback entry points, you include SDL_main.h with an extra define,
from a single source file in your project:

```c
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
```

Once you do this, you do not write a "main" function at all (and if you do,
the app will likely fail to link). Instead, you provide the following
functions:

First:

```c
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
```

This will be called _once_ before anything else. argc/argv work like they
always do. If this returns SDL_APP_CONTINUE, the app runs. If it returns
SDL_APP_FAILURE, the app calls SDL_AppQuit and terminates with an exit
code that reports an error to the platform. If it returns SDL_APP_SUCCESS,
the app calls SDL_AppQuit and terminates with an exit code that reports
success to the platform. This function should not go into an infinite
mainloop; it should do any one-time startup it requires and then return.

If you want to, you can assign a pointer to `*appstate`, and this pointer
will be made available to you in later functions calls in their `appstate`
parameter. This allows you to avoid global variables, but is totally
optional. If you don't set this, the pointer will be NULL in later function
calls.


Then:

```c
SDL_AppResult SDL_AppIterate(void *appstate);
```

This is called over and over, possibly at the refresh rate of the display or
some other metric that the platform dictates. This is where the heart of your
app runs. It should return as quickly as reasonably possible, but it's not a
"run one memcpy and that's all the time you have" sort of thing. The app
should do any game updates, and render a frame of video. If it returns
SDL_APP_FAILURE, SDL will call SDL_AppQuit and terminate the process with an
exit code that reports an error to the platform. If it returns
SDL_APP_SUCCESS, the app calls SDL_AppQuit and terminates with an exit code
that reports success to the platform. If it returns SDL_APP_CONTINUE, then
SDL_AppIterate will be called again at some regular frequency. The platform
may choose to run this more or less (perhaps less in the background, etc),
or it might just call this function in a loop as fast as possible. You do
not check the  event queue in this function (SDL_AppEvent exists for that).

Next:

```c
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
```

This will be called whenever an SDL event arrives. Your app should not call
SDL_PollEvent, SDL_PumpEvent, etc, as  SDL will manage all this for you. Return
values are the same as from SDL_AppIterate(), so you can terminate in response
to SDL_EVENT_QUIT, etc.


Finally:

```c
void SDL_AppQuit(void *appstate);
```

This is called once before terminating the app--assuming the app isn't being
forcibly killed or crashed--as a last chance to clean up. After this returns,
SDL will call SDL_Quit so the app doesn't have to (but it's safe for the app
to call it, too). Process termination proceeds as if the app returned normally
from main(), so atexit handles will run, if your platform supports that.

If you set `*appstate` during SDL_AppInit, this is where you should free that
data, as this pointer will not be provided to your app again.
