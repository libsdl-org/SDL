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

#ifdef SDL_PLATFORM_LINUX
#include <stdio.h>
#include <unistd.h>

bool SDL_IsUbuntuTouch(void)
{
    static bool checked = false;
    static bool is_ubuntu_touch = false;

    if (!checked) {
        const char *xdg_session_desktop = SDL_getenv("XDG_SESSION_DESKTOP");
        if (xdg_session_desktop && SDL_strcmp(xdg_session_desktop, "ubuntu-touch") == 0) {
            is_ubuntu_touch = true;
        }
        checked = true;
    }
    return is_ubuntu_touch;
}

bool SDL_SetupUbuntuTouchGlobalProperties(SDL_PropertiesID props)
{
    if (!SDL_IsUbuntuTouch()) {
        return true;
    }

    // Format is <appname>_<hookname>_<version>, like myapp.myname_myapp_1.0.0.
    // None of those are allowed to have underscores.
    const char *app_id = SDL_getenv("APP_ID");

    if (!app_id) {
        SDL_SetError("Missing APP_ID");
        return false;
    }

    char *buffer = SDL_strdup(app_id);

    if (!buffer) {
        return false;
    }

    char *it = SDL_strchr(buffer, '_');

    if (!*it) {
        SDL_SetError("Malformed APP_ID");
        SDL_free(buffer);
        return NULL;
    }

    char *it2 = SDL_strchr(it + 1, '_');

    if (!*it2) {
        SDL_SetError("Malformed APP_ID");
        SDL_free(buffer);
        return NULL;
    }

    *it++ = '\0';
    *it2++ = '\0';

    if (!SDL_SetStringProperty(props, SDL_PROP_GLOBAL_SYSTEM_UBUNTU_TOUCH_APPID_STRING, buffer) ||
        !SDL_SetStringProperty(props, SDL_PROP_GLOBAL_SYSTEM_UBUNTU_TOUCH_HOOK_STRING, it) ||
        !SDL_SetStringProperty(props, SDL_PROP_GLOBAL_SYSTEM_UBUNTU_TOUCH_APP_VERSION_STRING, it2)) {
        SDL_free(buffer);
        return false;
    }

    SDL_free(buffer);

    return true;
}

#endif
