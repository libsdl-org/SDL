/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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


#ifdef SDL_memset
#undef SDL_memset
#endif
#if SDL_DYNAMIC_API
#define SDL_memset SDL_memset_REAL
#endif
void *SDL_memset(SDL_OUT_BYTECAP(len) void *dst, int c, size_t len)
{
#if defined(__GNUC__) && (defined(HAVE_LIBC) && HAVE_LIBC)
    return __builtin_memset(dst, c, len);
#elif defined(HAVE_MEMSET)
    return memset(dst, c, len);
#else
    size_t left;
    uint32_t *dstp4;
    uint8_t *dstp1 = (uint8_t *)dst;
    uint8_t value1;
    uint32_t value4;

    // The value used in memset() is a byte, passed as an int
    c &= 0xff;

    /* The destination pointer needs to be aligned on a 4-byte boundary to
     * execute a 32-bit set. Set first bytes manually if needed until it is
     * aligned. */
    value1 = (uint8_t)c;
    while ((uintptr_t)dstp1 & 0x3) {
        if (len--) {
            *dstp1++ = value1;
        } else {
            return dst;
        }
    }

    value4 = ((uint32_t)c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24));
    dstp4 = (uint32_t *)dstp1;
    left = (len % 4);
    len /= 4;
    while (len--) {
        *dstp4++ = value4;
    }

    dstp1 = (uint8_t *)dstp4;
    switch (left) {
    case 3:
        *dstp1++ = value1;
        SDL_FALLTHROUGH;
    case 2:
        *dstp1++ = value1;
        SDL_FALLTHROUGH;
    case 1:
        *dstp1++ = value1;
    }

    return dst;
#endif // HAVE_MEMSET
}

// Note that memset() is a byte assignment and this is a 32-bit assignment, so they're not directly equivalent.
void *SDL_memset4(void *dst, uint32_t val, size_t dwords)
{
#if defined(__APPLE__) && defined(HAVE_STRING_H)
    memset_pattern4(dst, &val, dwords * 4);
#elif defined(__GNUC__) && defined(__i386__)
    int u0, u1, u2;
    __asm__ __volatile__(
        "cld \n\t"
        "rep ; stosl \n\t"
        : "=&D"(u0), "=&a"(u1), "=&c"(u2)
        : "0"(dst), "1"(val), "2"(SDL_static_cast(uint32_t, dwords))
        : "memory");
#else
    size_t _n = (dwords + 3) / 4;
    uint32_t *_p = SDL_static_cast(uint32_t *, dst);
    uint32_t _val = (val);
    if (dwords == 0) {
        return dst;
    }
    switch (dwords % 4) {
    case 0:
        do {
            *_p++ = _val;
            SDL_FALLTHROUGH;
        case 3:
            *_p++ = _val;
            SDL_FALLTHROUGH;
        case 2:
            *_p++ = _val;
            SDL_FALLTHROUGH;
        case 1:
            *_p++ = _val;
        } while (--_n);
    }
#endif
    return dst;
}

/* The optimizer on Visual Studio 2005 and later generates memcpy() and memset() calls.
   We will provide our own implementation if we're not building with a C runtime. */
#ifndef HAVE_LIBC
// NOLINTNEXTLINE(readability-redundant-declaration)
extern void *memset(void *dst, int c, size_t len);
#if defined(_MSC_VER) && !defined(__INTEL_LLVM_COMPILER)
#pragma intrinsic(memset)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma function(memset)
#endif
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void *memset(void *dst, int c, size_t len)
{
    return SDL_memset(dst, c, len);
}
#endif // !HAVE_LIBC

