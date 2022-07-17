/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#include "SDL_utils_c.h"

/* Common utility functions that aren't in the public API */

int SDL_powerof2(int x)
{
    int value;

    /* We could use this trick for 32-bit values:
     * value = x;
     * value -= 1;
     * value |= value >> 1;
     * value |= value >> 2;
     * value |= value >> 4;
     * value |= value >> 8;
     * value |= value >> 16;
     * value += 1;
     *
     * ... but this is more readable:
     */
    value = 1;
    while (value < x) {
        value <<= 1;
    }
    return value;
}

/* vi: set ts=4 sw=4 expandtab: */
