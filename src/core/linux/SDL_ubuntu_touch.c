/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
    const char *xdg_session_desktop = SDL_getenv("XDG_SESSION_DESKTOP");

    if (!xdg_session_desktop) {
        return false;
    }

    return SDL_strcmp(xdg_session_desktop, "ubuntu-touch") == 0;
}

static char **SDL_ubuntu_touch_get_app_info(void)
{
    static char appname[257] = "";
    static char hookname[257] = "";
    static char appversion[257] = "";
    static char *info[3] = {
      appname,
      hookname,
      appversion
    };
    static bool set = false;
    char *it, *it2;

    if (set) {
        return info;
    }

    if (!SDL_IsUbuntuTouch()) {
        SDL_SetError("Not running under Ubuntu Touch");
        return NULL;
    }

    // Format is <appname>_<hookname>_<version>, like myapp.myname_myapp_1.0.0.
    // None of those are allowed to have underscores.
    const char *app_id = SDL_getenv("APP_ID");

    if (!app_id) {
        SDL_SetError("Environment variable 'APP_ID' not set by the OS");
        return NULL;
    }

    it = SDL_strchr(app_id, '_');

    if (!*it) {
        SDL_SetError("Malformed APP_ID");
        return NULL;
    }

    it2 = SDL_strchr(it + 1, '_');

    if (!*it2) {
        SDL_SetError("Malformed APP_ID");
        return NULL;
    }

    if (SDL_snprintf(appname, sizeof(appname), "%.*s", (int) (it - app_id), app_id) > 255) {
        SDL_SetError("APP_ID appname too long");
        return NULL;
    }

    if (SDL_snprintf(hookname, sizeof(hookname), "%.*s", (int) (it2 - it), it + 1) > 255) {
        SDL_SetError("APP_ID hook name too long");
        return NULL;
    }

    if (SDL_snprintf(appversion, sizeof(appversion), "%s", it2 + 1) > 255) {
        SDL_SetError("APP_ID version too long");
        return NULL;
    }

    set = true;
    return info;
}

const char *SDL_GetUbuntuTouchAppID(void)
{
    char **info = SDL_ubuntu_touch_get_app_info();

    if (!info) {
        return NULL;
    }

    // On Ubuntu Touch, the ID of the app is called the "app name", whereas the
    // human-readable name is called the "app title".
    return info[0];
}

const char *SDL_GetUbuntuTouchAppHook(void)
{
    char **info = SDL_ubuntu_touch_get_app_info();

    if (!info) {
        return NULL;
    }

    return info[1];
}

const char *SDL_GetUbuntuTouchAppVersion(void)
{
    char **info = SDL_ubuntu_touch_get_app_info();

    if (!info) {
        return NULL;
    }

    return info[2];
}

#endif
