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

/* This file contains a portable memcpy manipulation function for SDL */

void *
SDL_memcpy(SDL_OUT_BYTECAP(len) void *dst, SDL_IN_BYTECAP(len) const void *src, size_t len)
{
#ifdef __GNUC__
    /* Presumably this is well tuned for speed.
       On my machine this is twice as fast as the C code below.
     */
    return __builtin_memcpy(dst, src, len);
#elif defined(HAVE_MEMCPY)
    return memcpy(dst, src, len);
#elif defined(HAVE_BCOPY)
    bcopy(src, dst, len);
    return dst;
#else
    /* GCC 4.9.0 with -O3 will generate movaps instructions with the loop
       using Uint32* pointers, so we need to make sure the pointers are
       aligned before we loop using them.
     */
    if (((uintptr_t)src & 0x3) || ((uintptr_t)dst & 0x3)) {
        /* Do an unaligned byte copy */
        Uint8 *srcp1 = (Uint8 *)src;
        Uint8 *dstp1 = (Uint8 *)dst;

        while (len--) {
            *dstp1++ = *srcp1++;
        }
    } else {
        size_t left = (len % 4);
        Uint32 *srcp4, *dstp4;
        Uint8 *srcp1, *dstp1;

        srcp4 = (Uint32 *) src;
        dstp4 = (Uint32 *) dst;
        len /= 4;
        while (len--) {
            *dstp4++ = *srcp4++;
        }

        srcp1 = (Uint8 *) srcp4;
        dstp1 = (Uint8 *) dstp4;
        switch (left) {
            case 3:
                *dstp1++ = *srcp1++;
            case 2:
                *dstp1++ = *srcp1++;
            case 1:
                *dstp1++ = *srcp1++;
        }
    }
    return dst;
#endif /* __GNUC__ */
}

/* vi: set ts=4 sw=4 expandtab: */
