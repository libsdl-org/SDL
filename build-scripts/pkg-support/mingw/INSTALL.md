
# Using this package

This package contains SDL built for the mingw-w64 toolchain.

The files for 32-bit architecture are in i686-w64-mingw32
The files for 64-bit architecture are in x86_64-w64-mingw32

You can install them to another location, just type `make` for help.

To use this package, point your include path at _arch_/include and your library path at _arch_/lib, link with the SDL3 library and copy _arch_/bin/SDL3.dll next to your executable.

e.g.
```sh
gcc -o hello.exe hello.c -Ix86_64-w64-mingw32/include -Lx86_64-w64-mingw32/lib -lSDL3
cp x86_64-w64-mingw32/bin/SDL3.dll .
./hello.exe
```

# Documentation

An API reference, tutorials, and additional documentation is available at:

https://wiki.libsdl.org/SDL3

# Example code

There are simple example programs available at:

https://examples.libsdl.org/SDL3

# Discussions

## Discord

You can join the official Discord server at:

https://discord.com/invite/BwpFGBWsv8

## Forums/mailing lists

You can join SDL development discussions at:

https://discourse.libsdl.org/

Once you sign up, you can use the forum through the website or as a mailing list from your email client.

## Announcement list

You can sign up for the low traffic announcement list at:

https://www.libsdl.org/mailing-list.php

