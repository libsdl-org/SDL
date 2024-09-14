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

#include "../SDL_sysurl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#ifdef USE_POSIX_SPAWN
#include <spawn.h>
extern char **environ;
#endif

bool SDL_SYS_OpenURL(const char *url)
{
    SDL_Environment *env = NULL;
    char **process_env = NULL;
    const char *process_args[] = { "xdg-open", url, NULL };
    SDL_Process *process = NULL;
    bool result = false;

    env = SDL_CreateEnvironment(SDL_FALSE);
    if (!env) {
        goto done;
    }

    // Clear LD_PRELOAD so Chrome opens correctly when this application is launched by Steam
    SDL_UnsetEnvironmentVariable(env, "LD_PRELOAD");

    process_env = SDL_GetEnvironmentVariables(env);
    if (!process_env) {
        goto done;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        goto done;
    }
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, process_args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, process_env);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        goto done;
    }

    result = true;

done:
    SDL_free(process_env);
    SDL_DestroyEnvironment(env);
    SDL_DestroyProcess(process);

    return result;
}
