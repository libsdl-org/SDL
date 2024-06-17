# String policies in SDL3.

## Encoding.

Unless otherwise specified, all strings in SDL, across all platforms, are
UTF-8 encoded and can represent the full range of [Unicode](https://unicode.org).


## The SDL Get String Rule.

Did you see 'SDL_GetStringRule' in the wiki or headers? Here are the details
that aren't worth copying across dozens of functions' documentation.

tl;dr: If an SDL function returns a `const char *` string, do not modify or
free it, and if you need to save it, make a copy right away.

In several cases, SDL wants to return a string to the app, and the question
in any library that does this is: _who owns this thing?_

The answer in almost all cases, is that SDL does, but not for long.

The pointer is only guaranteed to be valid until the next time the event
queue is updated, or SDL_Quit is called.

The reason for this is memory safety.

There are several strings that SDL provides that could be freed at
any moment. For example, an app calls SDL_GetAudioDeviceName(), which returns
a string that is part of the internal audio device structure. But, while this
function is returning, the user yanks the USB audio device out of the
computer, and SDL decides to deallocate the structure...and the string!
Now the app is holding a pointer that didn't live long enough to be useful,
and could crash if accessed.

To avoid this, instead of calling SDL_free on a string as soon as it's done
with it, SDL adds the pointer to a list. This list is freed at specific
points: when the event queue is run (for ongoing cleanup) and when SDL_Quit
is called (to catch things that are just hanging around). This allows the
app to use a string without worrying about it becoming bogus in the middle
of a printf() call. If the app needs it for longer, it should copy it.

When does "the event queue run"? There are several points:

- If the app calls SDL_PumpEvents() _from any thread_.
- SDL_PumpEvents is also called by several other APIs internally:
  SDL_PollEvent(), SDL_PeepEvents(), SDL_WaitEvent(),
  SDL_WaitEventTimeout(), and maybe others.
- If you are using [the main callbacks](main-functions#main-callbacks-in-sdl3),
  the event queue can run immediately after any of the callback functions
  return.

Note that these are just guaranteed minimum lifespans; any given string
might live much longer--some might even be static memory that is _never_
deallocated--but this rule promises that the app has a safe window.

Note that none of this applies if the return value is `char *` instead of
`const char *`. Please see the specific function's documentation for how
to handle those pointers.

