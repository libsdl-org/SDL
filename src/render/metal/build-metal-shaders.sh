#!/bin/bash

set -x
set -e

cd `dirname "$0"`
rm -f sdl.air sdl.metalar sdl.metallib
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/usr/bin/metal -std=osx-metal1.1 -Wall -O3 -o ./sdl.air ./SDL_shaders_metal.metal
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/usr/bin/metal-ar rc sdl.metalar sdl.air
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/usr/bin/metallib -o sdl.metallib sdl.metalar
xxd -i sdl.metallib |perl -w -p -e 's/\Aunsigned /const unsigned /;' >./SDL_shaders_metal.c
rm -f sdl.air sdl.metalar sdl.metallib

