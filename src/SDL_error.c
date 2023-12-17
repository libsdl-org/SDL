/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/* Simple error handling in SDL */

#include "SDL_error_c.h"

int SDL_SetError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    /* Ignore call if invalid format pointer was passed */
    if (fmt) {
        va_list ap;
        int result;
        SDL_error *error = SDL_GetErrBuf(SDL_TRUE);

        error->error = SDL_ErrorCodeGeneric;

        va_start(ap, fmt);
        result = SDL_vsnprintf(error->str, error->len, fmt, ap);
        va_end(ap);

        if (result >= 0 && (size_t)result >= error->len && error->realloc_func) {
            size_t len = (size_t)result + 1;
            char *str = (char *)error->realloc_func(error->str, len);
            if (str) {
                error->str = str;
                error->len = len;
                va_start(ap, fmt);
                (void)SDL_vsnprintf(error->str, error->len, fmt, ap);
                va_end(ap);
            }
        }

        if (SDL_LogGetPriority(SDL_LOG_CATEGORY_ERROR) <= SDL_LOG_PRIORITY_DEBUG) {
            /* If we are in debug mode, print out the error message */
            SDL_LogDebug(SDL_LOG_CATEGORY_ERROR, "%s", error->str);
        }
    }

    return -1;
}

/* Available for backwards compatibility */
const char *SDL_GetError(void)
{
    const SDL_error *error = SDL_GetErrBuf(SDL_FALSE);

    if (!error) {
        return "";
    }

    switch (error->error) {
    case SDL_ErrorCodeGeneric:
        return error->str;
    case SDL_ErrorCodeOutOfMemory:
        return "Out of memory";
    default:
        return "";
    }
}

void SDL_ClearError(void)
{
    SDL_error *error = SDL_GetErrBuf(SDL_FALSE);

    if (error) {
        error->error = SDL_ErrorCodeNone;
    }
}

/* Very common errors go here */
int SDL_Error(SDL_errorcode code)
{
    switch (code) {
    case SDL_ENOMEM:
        SDL_GetErrBuf(SDL_TRUE)->error = SDL_ErrorCodeOutOfMemory;
        return -1;
    case SDL_EFREAD:
        return SDL_SetError("Error reading from datastream");
    case SDL_EFWRITE:
        return SDL_SetError("Error writing to datastream");
    case SDL_EFSEEK:
        return SDL_SetError("Error seeking in datastream");
    case SDL_UNSUPPORTED:
        return SDL_SetError("That operation is not supported");
    default:
        return SDL_SetError("Unknown SDL error");
    }
}
