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
#include "SDL_sysfilesystem.h"

int SDL_RemovePath(const char *path)
{
    if (!path) {
        return SDL_InvalidParamError("path");
    }
    return SDL_SYS_RemovePath(path);
}

int SDL_RenamePath(const char *oldpath, const char *newpath)
{
    if (!oldpath) {
        return SDL_InvalidParamError("oldpath");
    } else if (!newpath) {
        return SDL_InvalidParamError("newpath");
    }
    return SDL_SYS_RenamePath(oldpath, newpath);
}

int SDL_CreateDirectory(const char *path)
{
    /* TODO: Recursively create subdirectories */
    if (!path) {
        return SDL_InvalidParamError("path");
    }
    return SDL_SYS_CreateDirectory(path);
}

int SDL_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata)
{
    if (!path) {
        return SDL_InvalidParamError("path");
    } else if (!callback) {
        return SDL_InvalidParamError("callback");
    }
    return SDL_SYS_EnumerateDirectory(path, path, callback, userdata);
}

int SDL_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    SDL_PathInfo dummy;

    if (!info) {
        info = &dummy;
    }
    SDL_zerop(info);

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    return SDL_SYS_GetPathInfo(path, info);
}
