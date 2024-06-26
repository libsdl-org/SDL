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

#include "SDL_uri_decode.h"
#include <errno.h>
#include <unistd.h>

int SDL_URIDecode(const char *src, char *dst, int len)
{
    int ri, wi, di;
    char decode = '\0';
    if (!src || !dst || len < 0) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        len = SDL_strlen(src);
    }
    for (ri = 0, wi = 0, di = 0; ri < len && wi < len; ri += 1) {
        if (di == 0) {
            /* start decoding */
            if (src[ri] == '%') {
                decode = '\0';
                di += 1;
                continue;
            }
            /* normal write */
            dst[wi] = src[ri];
            wi += 1;
            continue;
        } else if (di == 1 || di == 2) {
            char off = '\0';
            char isa = src[ri] >= 'a' && src[ri] <= 'f';
            char isA = src[ri] >= 'A' && src[ri] <= 'F';
            char isn = src[ri] >= '0' && src[ri] <= '9';
            if (!(isa || isA || isn)) {
                /* not a hexadecimal */
                int sri;
                for (sri = ri - di; sri <= ri; sri += 1) {
                    dst[wi] = src[sri];
                    wi += 1;
                }
                di = 0;
                continue;
            }
            /* itsy bitsy magicsy */
            if (isn) {
                off = 0 - '0';
            } else if (isa) {
                off = 10 - 'a';
            } else if (isA) {
                off = 10 - 'A';
            }
            decode |= (src[ri] + off) << (2 - di) * 4;
            if (di == 2) {
                dst[wi] = decode;
                wi += 1;
                di = 0;
            } else {
                di += 1;
            }
            continue;
        }
    }
    dst[wi] = '\0';
    return wi;
}

int SDL_URIToLocal(const char *src, char *dst)
{
    if (SDL_memcmp(src, "file:/", 6) == 0) {
        src += 6; /* local file? */
    } else if (SDL_strstr(src, ":/") != NULL) {
        return -1; /* wrong scheme */
    }

    SDL_bool local = src[0] != '/' || (src[0] != '\0' && src[1] == '/');

    /* got a hostname? */
    if (!local && src[0] == '/' && src[2] != '/') {
        char *hostname_end = SDL_strchr(src + 1, '/');
        if (hostname_end) {
            char hostname[257];
            if (gethostname(hostname, 255) == 0) {
                hostname[256] = '\0';
                if (SDL_memcmp(src + 1, hostname, hostname_end - (src + 1)) == 0) {
                    src = hostname_end + 1;
                    local = SDL_TRUE;
                }
            }
        }
    }
    if (local) {
        /* Convert URI escape sequences to real characters */
        if (src[0] == '/') {
            src++;
        } else {
            src--;
        }
        return SDL_URIDecode(src, dst, 0);
    }
    return -1;
}
