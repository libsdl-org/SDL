Nokia N-Gage
============

SDL port for Symbian S60v1 and v2 with a main focus on the Nokia N-Gage
(Classic and QD) by [Michael Fitzmayer](https://github.com/mupfdev).

Compiling
---------

SDL is part of the [N-Gage SDK.](https://github.com/ngagesdk) project.
The library is included in the
[toolchain](https://github.com/ngagesdk/ngage-toolchain) as a
sub-module.

A complete example project based on SDL can be found in the GitHub
account of the SDK: [Wordle](https://github.com/ngagesdk/wordle).

Current level of implementation
-------------------------------

The video driver currently provides full screen video support with
keyboard input.

At the moment only the software renderer works.

Audio is not yet implemented.

Acknowledgements
----------------

Thanks to Hannu Viitala, Kimmo Kinnunen and Markus Mertama for the
valuable insight into Symbian programming.  Without the SDL 1.2 port
which was specially developed for CDoom (Doom for the Nokia 9210), this
adaptation would not have been possible.

I would like to thank my friends
[Razvan](https://twitter.com/bewarerazvan) and [Dan
Whelan](https://danwhelan.ie/), for their continuous support.  Without
you and the [N-Gage community](https://discord.gg/dbUzqJ26vs), I would
have lost my patience long ago.

Last but not least, I would like to thank the development team of
[EKA2L1](https://12z1.com/) (an experimental Symbian OS emulator). Your
patience and support in troubleshooting helped me a lot.
