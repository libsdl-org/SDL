/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#ifdef SDL_FILESYSTEM_WINDOWS

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include "../../core/windows/SDL_windows.h"
#include <shlobj.h>

#include "SDL_assert.h"
#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "SDL_filesystem.h"

char *
SDL_GetBasePath(void)
{
    TCHAR path[MAX_PATH];
    const DWORD len = GetModuleFileName(NULL, path, SDL_arraysize(path));
    size_t i;

    SDL_assert(len < SDL_arraysize(path));

    if (len == 0) {
        WIN_SetError("Couldn't locate our .exe");
        return NULL;
    }

    for (i = len-1; i > 0; i--) {
        if (path[i] == '\\') {
            break;
        }
    }

    SDL_assert(i > 0); /* Should have been an absolute path. */
    path[i+1] = '\0';  /* chop off filename. */
    return WIN_StringToUTF8(path);
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    /*
     * Vista and later has a new API for this, but SHGetFolderPath works there,
     *  and apparently just wraps the new API. This is the new way to do it:
     *
     *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
     *                          NULL, &wszPath);
     */

    TCHAR path[MAX_PATH];
    char *utf8 = NULL;
    char *retval = NULL;

    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) {
        WIN_SetError("Couldn't locate our prefpath");
        return NULL;
    }

    utf8 = WIN_StringToUTF8(path);
    if (utf8) {
        const size_t len = SDL_strlen(utf8) + SDL_strlen(org) + SDL_strlen(app) + 4;
        retval = (char *) SDL_malloc(len);
        if (!retval) {
            SDL_free(utf8);
            SDL_OutOfMemory();
            return NULL;
        }
        SDL_snprintf(retval, len, "%s\\%s\\%s\\", utf8, org, app);
        SDL_free(utf8);
    }

    return retval;
}

#endif /* SDL_FILESYSTEM_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
