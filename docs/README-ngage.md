Nokia N-Gage
============

SDL2 port for Symbian S60v1/2 with a main focus on the Nokia N-Gage
(Classic and QD) by [Michael Fitzmayer](https://github.com/mupfdev).

Compiling
---------

SDL is part of the [N-Gage SDK.](https://github.com/ngagesdk) project.
The library is included in the
[toolchain](https://github.com/ngagesdk/ngage-toolchain) as a
sub-module.

A complete example project based on SDL2 can be found in the GitHub
account of the SDK: [Example
project](https://github.com/ngagesdk/wordle).

Current level of implementation
-------------------------------

The video driver currently provides full screen video support with
keyboard input.

At the moment only the software renderer works, but clipping had to be
explicitly deactivated for the platform.  This means that functions such
as `SDL_RenderSetLogicalSize()` do not work yet.

Audio is not yet implemented.

Acknowledgements
----------------

Thanks to Hannu Viitala, Kimmo Kinnunen and Markus Mertama for the
valuable insight into Symbian programming.  Without the SDL 1.2 port for
CDoom, this adaptation would not have been possible.

I would like to thank my friends
[Razvan](https://twitter.com/bewarerazvan) and [Dan
Whelan](https://danwhelan.ie/), for their continuous support.  Without
you and the [N-Gage community](https://discord.gg/dbUzqJ26vs), I would
have lost my patience long ago.

Last but not least, I would like to say a special thank you to the
[EKA2L1](https://12z1.com/) team.  Thank you for all your patience and
support in troubleshooting.
