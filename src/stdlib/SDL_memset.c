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

#include "../SDL_internal.h"

/* This file contains a portable memset manipulation function for SDL */

void *
SDL_memset(SDL_OUT_BYTECAP(len) void *dst, int c, size_t len)
{
#if defined(HAVE_MEMSET)
    return memset(dst, c, len);
#else
    size_t left;
    Uint32 *dstp4;
    Uint8 *dstp1 = (Uint8 *) dst;
    Uint8 value1;
    Uint32 value4;

    /* The value used in memset() is a byte, passed as an int */
    c &= 0xff;

    /* The destination pointer needs to be aligned on a 4-byte boundary to
     * execute a 32-bit set. Set first bytes manually if needed until it is
     * aligned. */
    value1 = (Uint8)c;
    while ((uintptr_t)dstp1 & 0x3) {
        if (len--) {
            *dstp1++ = value1;
        } else {
            return dst;
        }
    }

    value4 = ((Uint32)c | ((Uint32)c << 8) | ((Uint32)c << 16) | ((Uint32)c << 24));
    dstp4 = (Uint32 *) dstp1;
    left = (len % 4);
    len /= 4;
    while (len--) {
        *dstp4++ = value4;
    }

    dstp1 = (Uint8 *) dstp4;
    switch (left) {
        case 3:
            *dstp1++ = value1;
        case 2:
            *dstp1++ = value1;
        case 1:
            *dstp1++ = value1;
    }

    return dst;
#endif /* HAVE_MEMSET */
}

/* vi: set ts=4 sw=4 expandtab: */
