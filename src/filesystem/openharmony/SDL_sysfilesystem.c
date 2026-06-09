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

#ifdef SDL_FILESYSTEM_OPENHARMONY


// !!! FIXME: HACK to prevent <AbilityKit/ability_runtime/start_options.h> from including. It has '&' instead of '*' for some args, which suggests it's only been tested with C++.
#define ABILITY_RUNTIME_START_OPTIONS_H
typedef enum AbilityRuntime_StartOptions AbilityRuntime_StartOptions;


#include <sys/stat.h>
#include <AbilityKit/ability_runtime/application_context.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

char *SDL_SYS_GetBasePath(void)
{
    return SDL_strdup("assets://");
}

char *SDL_SYS_GetExeName(void)
{
    char buffer[128];
    int32_t writelen = 0;
    const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetBundleName(buffer, (int32_t) sizeof (buffer), &writelen);
    if (rc != ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
        SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleName failed: %d", (int) rc);
        return NULL;
    }
    return SDL_strdup(buffer);
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    const char *prefdir = SDL_GetOpenHarmonyInternalStoragePath();
    char *retval = NULL;
    if (prefdir) {
        if (SDL_asprintf(&retval, "%s/%s/", prefdir, app) < 0) {
            retval = NULL;
        }
    }

    mkdir(prefdir, 0755);
    mkdir(retval, 0755);

    return retval;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    SDL_Unsupported();  // !!! FIXME: Can we support _any_ of this?
    return NULL;
}

#endif // SDL_FILESYSTEM_OPENHARMONY
