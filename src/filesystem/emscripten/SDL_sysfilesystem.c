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

#ifdef SDL_FILESYSTEM_EMSCRIPTEN

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */
#include <errno.h>
#include <sys/stat.h>

#include <emscripten/emscripten.h>

char *SDL_GetBasePath(void)
{
    char *retval = "/";
    return SDL_strdup(retval);
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
    const char *append = "/libsdl/";
    char *retval;
    char *ptr = NULL;
    size_t len = 0;

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }

    len = SDL_strlen(append) + SDL_strlen(org) + SDL_strlen(app) + 3;
    retval = (char *)SDL_malloc(len);
    if (!retval) {
        return NULL;
    }

    if (*org) {
        SDL_snprintf(retval, len, "%s%s/%s/", append, org, app);
    } else {
        SDL_snprintf(retval, len, "%s%s/", append, app);
    }

    for (ptr = retval + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';
            if (mkdir(retval, 0700) != 0 && errno != EEXIST) {
                goto error;
            }
            *ptr = '/';
        }
    }

    if (mkdir(retval, 0700) != 0 && errno != EEXIST) {
    error:
        SDL_SetError("Couldn't create directory '%s': '%s'", retval, strerror(errno));
        SDL_free(retval);
        return NULL;
    }

    return retval;
}

char *SDL_GetUserFolder(SDL_Folder folder)
{
    const char *home = NULL;

    if (folder != SDL_FOLDER_HOME) {
        SDL_SetError("Emscripten only supports the home folder");
        return NULL;
    }

    home = SDL_getenv("HOME");
    if (!home) {
        SDL_SetError("No $HOME environment variable available");
        return NULL;
    }

    return SDL_strdup(home);
}

#endif /* SDL_FILESYSTEM_EMSCRIPTEN */
