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

#ifdef SDL_FILESYSTEM_DOS

#include <dir.h>
#include <sys/stat.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

char *SDL_SYS_GetBasePath(void)
{
    extern const char *SDL_argv0; // from src/main/dos/SDL_sysmain_runapp.c
    char *searched = searchpath(SDL_argv0);
    if (!searched) {
        SDL_SetError("argv[0] not found by searchpath");
        return NULL;
    }

    char *fullpath = SDL_strdup(searched);
    if (!fullpath) {
        return NULL;
    }

    // I don't know if this is a good idea. Drop DOS path separators, use Unix style instead.
    char *ptr;
    for (ptr = fullpath; *ptr; ptr++) {
        if (*ptr == '\\') {
            *ptr = '/';
        }
    }

    // drop the .exe name.
    ptr = SDL_strrchr(fullpath, '/');
    if (ptr) {
        ptr[1] = '\0';
    }

    return fullpath;
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    char *result = NULL;
    size_t len;
    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }

    const char *base = SDL_GetBasePath();
    if (!base) {
        return NULL;
    }

    if (!org) {
        org = "";
    }

    len = SDL_strlen(base) + SDL_strlen(org) + SDL_strlen(app) + 4;
    result = (char *)SDL_malloc(len);
    if (result) {
        if (*org) {
            SDL_snprintf(result, len, "%s%s", base, org);
            mkdir(result, 0755);
            SDL_snprintf(result, len, "%s%s/%s/", base, org, app);
        } else {
            SDL_snprintf(result, len, "%s%s/", base, app);
        }

        mkdir(result, 0755);
    }

    return result;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    SDL_Unsupported();
    return NULL;
}

#endif // SDL_FILESYSTEM_DOS
