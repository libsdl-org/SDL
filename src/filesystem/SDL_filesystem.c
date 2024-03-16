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

void SDL_FileTimeToWindows(Sint64 ftime, Uint32 *low, Uint32 *high)
{
    const Sint64 delta_1601_epoch_s = 11644473600ull; // [seconds] (seconds between 1/1/1601 and 1/1/1970, 11644473600 seconds)

    Sint64 cvt = (ftime + delta_1601_epoch_s) * (SDL_NS_PER_SECOND / 100ull); // [100ns] (adjust to epoch and convert nanoseconds to 1/100th nanosecond units).

    // Windows FILETIME is unsigned, so if we're trying to show a timestamp from before before the
    // Windows epoch, (Jan 1, 1601), clamp it to zero so it doesn't go way into the future.
    if (cvt < 0) {
        cvt = 0;
    }

    if (low) {
        *low = (Uint32) cvt;
    }

    if (high) {
        *high = (Uint32) (cvt >> 32);
    }
}

int SDL_RemovePath(const char *path)
{
    if (!path) {
        return SDL_InvalidParamError("path");
    }
    return SDL_SYS_FSremove(path);
}

int SDL_RenamePath(const char *oldpath, const char *newpath)
{
    if (!oldpath) {
        return SDL_InvalidParamError("oldpath");
    } else if (!newpath) {
        return SDL_InvalidParamError("newpath");
    }
    return SDL_SYS_FSrename(oldpath, newpath);
}

int SDL_CreateDirectory(const char *path)
{
    /* TODO: Recursively create subdirectories */
    if (!path) {
        return SDL_InvalidParamError("path");
    }
    return SDL_SYS_FSmkdir(path);
}

int SDL_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata)
{
    if (!path) {
        return SDL_InvalidParamError("path");
    } else if (!callback) {
        return SDL_InvalidParamError("callback");
    }
    return SDL_SYS_FSenumerate(path, path, callback, userdata);
}

int SDL_GetPathInfo(const char *path, SDL_PathInfo *stat)
{
    if (!path) {
        return SDL_InvalidParamError("path");
    } else if (!stat) {
        return SDL_InvalidParamError("stat");
    }
    return SDL_SYS_FSstat(path, stat);
}
