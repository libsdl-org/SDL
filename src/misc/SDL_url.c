/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_sysurl.h"

bool SDL_OpenURL(const char *url)
{
    CHECK_PARAM(!url) {
        return SDL_InvalidParamError("url");
    }
    return SDL_SYS_OpenURL(url);
}

char *SDL_EncodeURL(const char *str, const char *no_encode_chars)
{
    const size_t slen = SDL_strlen(str) + 1;
    size_t allocation = slen + 64;   // at least this long plus a little more, in case one allocation covers it.
    size_t dsti = 0;
    char *retval = (char *) SDL_malloc(allocation);
    if (!retval) {
        return NULL;
    }

    for (size_t i = 0; i < slen; i++) {
        if (dsti >= (allocation - 4)) {
            allocation += 64;
            char *ptr = (char *) SDL_realloc(retval, allocation);
            if (!ptr) {
                SDL_free(retval);
                return NULL;
            }
            retval = ptr;
        }

        const char ch = str[i];

        if ( ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')) ||
             ((ch >= '0') && (ch <= '9')) ||
             ((ch == '=') || (ch == '.') || (ch == '_') || (ch == '~') || (ch == '\0')) ||
             (no_encode_chars && (SDL_strchr(no_encode_chars, ch) != NULL)) ) {
            retval[dsti++] = ch;  // unreserved char, null terminator char, or explicitly requested not to encode.
        } else {
            SDL_snprintf(&retval[dsti], 4, "%%%02X", (unsigned int) ch);
            dsti += 3;
        }
    }

    // shrink the allocation, if possible.
    char *ptr = (char *) SDL_realloc(retval, SDL_strlen(retval) + 1);
    if (ptr) {
        retval = ptr;
    }

    return retval;
}

