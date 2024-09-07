# Examples

## What is this?

In here are a collection of standalone SDL application examples. Unless
otherwise stated, they should work on all supported platforms out of the box.
If they don't [please file a bug to let us know](https://github.com/libsdl-org/SDL/issues/new).


## What is this SDL_AppIterate thing?

SDL can optionally build apps as a collection of callbacks instead of the
usual program structure that starts and ends in a function called `main`.
The examples use this format for two reasons.

First, it allows the examples to work when built as web applications without
a pile of ugly `#ifdef`s, and all of these examples are published on the web
at [examples.libsdl.org](https://examples.libsdl.org/), so you can easily see
them in action.

Second, it's example code! The callbacks let us cleanly break the program up
into the four logical pieces most apps care about:

- Program startup
- Event handling
- What the program actually does in a single frame
- Program shutdown

A detailed technical explanation of these callbacks is in
docs/README-main-functions.md (or view that page on the web on
[the wiki](https://wiki.libsdl.org/SDL3/README/main-functions#main-callbacks-in-sdl3)).


## I would like to build and run these examples myself.

When you build SDL with CMake, you can add `-DSDL_EXAMPLES=On` to the
CMake command line. When you build SDL, these examples will be built with it.

But most of these can just be built as a single .c file, as long as you point
your compiler at SDL3's headers and link against SDL.


## What is the license on the example code? Can I paste this into my project?

All code in the examples directory is considered public domain! You can do
anything you like with it, including copy/paste it into your closed-source
project, sell it, and pretend you wrote it yourself. We do not require you to
give us credit for this code (but we always appreciate if you do!).

This is only true for the examples directory. The rest of SDL falls under the
[zlib license](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt).


## What is template.html and highlight-plugin.lua in this directory?

This is what [examples.libsdl.org](https://examples.libsdl.org/) uses when
generating the web versions of these example programs. You can ignore this,
unless you are improving it, in which case we definitely would love to hear
from you!


## What is template.c in this directory?

If writing new examples, this is the skeleton code we start from, to keep
everything consistent. You can ignore it.


