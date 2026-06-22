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

#ifdef HAVE_DBUS_DBUS_H
#include "../../core/linux/SDL_dbus.h"
#endif

#ifdef SDL_VIDEO_DRIVER_WAYLAND
#include "../../video/wayland/SDL_waylandutil.h"
#endif

// Wayland requires an activation token for the browser to take focus.
static void GetActivationToken(char **token, char **window_id)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    SDL_VideoDevice *vid = SDL_GetVideoDevice();

    if (vid && SDL_strcmp(vid->name, "wayland") == 0) {
        Wayland_GetActivationTokenForExport(vid, token, window_id);
    }
#endif
}

bool SDL_SYS_OpenURL(const char *url)
{
    char *activation_token = NULL;
    char *window_id = NULL;

    GetActivationToken(&activation_token, &window_id);

    // Prefer the D-Bus portal, if available.
#ifdef HAVE_DBUS_DBUS_H
    if (SDL_DBus_OpenURI(url, window_id, activation_token)) {
        SDL_free(activation_token);
        SDL_free(window_id);
        return true;
    }
#endif

    const char *args[] = { "xdg-open", url, NULL };
    SDL_Environment *env = NULL;
    SDL_Process *process = NULL;
    bool result = false;

    env = SDL_CreateEnvironment(true);
    if (!env) {
        goto done;
    }

    // Clear LD_PRELOAD so Chrome opens correctly when this application is launched by Steam
    SDL_UnsetEnvironmentVariable(env, "LD_PRELOAD");
    if (activation_token) {
        SDL_SetEnvironmentVariable(env, "XDG_ACTIVATION_TOKEN", activation_token, false);
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        goto done;
    }
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        goto done;
    }

    result = true;

done:
    SDL_free(activation_token);
    SDL_free(window_id);
    SDL_DestroyEnvironment(env);
    SDL_DestroyProcess(process);

    return result;
}
