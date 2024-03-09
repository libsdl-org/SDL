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

#ifdef SDL_FILESYSTEM_HAIKU

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include <kernel/image.h>
#include <storage/Directory.h>
#include <storage/Entry.h>
#include <storage/FindDirectory.h>
#include <storage/Path.h>


char *SDL_GetBasePath(void)
{
    char name[MAXPATHLEN];

    if (find_path(B_APP_IMAGE_SYMBOL, B_FIND_PATH_IMAGE_PATH, NULL, name, sizeof(name)) != B_OK) {
        return NULL;
    }

    BEntry entry(name, true);
    BPath path;
    status_t rc = entry.GetPath(&path);  /* (path) now has binary's path. */
    SDL_assert(rc == B_OK);
    rc = path.GetParent(&path); /* chop filename, keep directory. */
    SDL_assert(rc == B_OK);
    const char *str = path.Path();
    SDL_assert(str != NULL);

    const size_t len = SDL_strlen(str);
    char *retval = (char *) SDL_malloc(len + 2);
    if (!retval) {
        return NULL;
    }

    SDL_memcpy(retval, str, len);
    retval[len] = '/';
    retval[len+1] = '\0';
    return retval;
}


char *SDL_GetPrefPath(const char *org, const char *app)
{
    // !!! FIXME: is there a better way to do this?
    const char *home = SDL_getenv("HOME");
    const char *append = "/config/settings/";
    size_t len = SDL_strlen(home);

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }

    if (!len || (home[len - 1] == '/')) {
        ++append; // home empty or ends with separator, skip the one from append
    }
    len += SDL_strlen(append) + SDL_strlen(org) + SDL_strlen(app) + 3;
    char *retval = (char *) SDL_malloc(len);
    if (retval) {
        if (*org) {
            SDL_snprintf(retval, len, "%s%s%s/%s/", home, append, org, app);
        } else {
            SDL_snprintf(retval, len, "%s%s%s/", home, append, app);
        }
        create_directory(retval, 0700);  // Haiku api: creates missing dirs
    }

    return retval;
}

char *SDL_GetUserFolder(SDL_Folder folder)
{
    const char *home = NULL;
    char *retval;

    home = SDL_getenv("HOME");
    if (!home) {
        SDL_SetError("No $HOME environment variable available");
        return NULL;
    }

    switch (folder) {
    case SDL_FOLDER_HOME:
        return SDL_strdup(home);

        /* TODO: Is Haiku's desktop folder always ~/Desktop/ ? */
    case SDL_FOLDER_DESKTOP:
        retval = (char *) SDL_malloc(SDL_strlen(home) + 10);

        if (retval) {
            SDL_strlcpy(retval, home, SDL_strlen(home) + 10);
            SDL_strlcat(retval, "/Desktop/", SDL_strlen(home) + 10);
        }

        return retval;

    case SDL_FOLDER_DOCUMENTS:
    case SDL_FOLDER_DOWNLOADS:
    case SDL_FOLDER_MUSIC:
    case SDL_FOLDER_PICTURES:
    case SDL_FOLDER_PUBLICSHARE:
    case SDL_FOLDER_SAVEDGAMES:
    case SDL_FOLDER_SCREENSHOTS:
    case SDL_FOLDER_TEMPLATES:
    case SDL_FOLDER_VIDEOS:
    default:
        SDL_SetError("Only HOME and DESKTOP available on Haiku");
        return NULL;
    }
}

#endif /* SDL_FILESYSTEM_HAIKU */
