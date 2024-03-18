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

static const Sint64 delta_1601_epoch_100ns = 11644473600ll * 10000000ll; // [100 ns] (100 ns units between 1/1/1601 and 1/1/1970, 11644473600 seconds)

void SDL_FileTimeToWindows(SDL_FileTime ftime, Uint32 *dwLowDateTime, Uint32 *dwHighDateTime)
{
    Uint64 wtime;

    // Convert ftime to 100ns units
    Sint64 ftime_100ns = (ftime / 100);

    if (ftime_100ns < 0 && -ftime_100ns > delta_1601_epoch_100ns) {
        // If we're trying to show a timestamp from before before the Windows epoch, (Jan 1, 1601), clamp it to zero
        wtime = 0;
    } else {
        wtime = (Uint64)(delta_1601_epoch_100ns + ftime_100ns);
    }

    if (dwLowDateTime) {
        *dwLowDateTime = (Uint32)wtime;
    }

    if (dwHighDateTime) {
        *dwHighDateTime = (Uint32)(wtime >> 32);
    }
}

SDL_FileTime SDL_FileTimeFromWindows(Uint32 dwLowDateTime, Uint32 dwHighDateTime)
{
    Uint64 wtime = (((Uint64)dwHighDateTime << 32) | dwLowDateTime);

    return (Sint64)(wtime - delta_1601_epoch_100ns) * 100;
}

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
    return (SDL_SYS_EnumerateDirectory(path, path, callback, userdata) < 0) ? -1 : 0;
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

static SDL_bool WildcardMatch(const char *pattern, const char *str)
{
    SDL_assert(str != NULL);

    if (!pattern) {
        return SDL_TRUE;  // no pattern? Everything matches.
    }

    while (SDL_TRUE) {
        const char pch = *(pattern++);
        if (pch == '?') {
            if (*(str++) == '\0') {
                return SDL_FALSE;
            }
        } else if (pch == '*') {
            const char nextch = *(pattern++);
            while (SDL_TRUE) {
                const char nextsch = *(str++);
                if (nextsch == nextch) {
                    break;
                } else if (nextsch == '\0') {
                    return SDL_FALSE;  // hit end of string without matching next pattern character.
                }
            }
        } else if (pch != *(str++)) {
            return SDL_FALSE;  // didn't match the next character.
        } else if (pch == '\0') {
            break;   // matched to the end of the string!
        }
    }

    return SDL_TRUE;   // matched to the end of the string!
}


int SDL_GlobDirectoryCallback(void *userdata, const char *dirname, const char *fname)
{
    SDL_GlobDirCallbackData *data = (SDL_GlobDirCallbackData *) userdata;

    if (WildcardMatch(data->pattern, fname)) {
        const size_t slen = SDL_strlen(fname) + 1;
        if (SDL_WriteIO(data->string_stream, fname, slen) != slen) {
            return -1;  // stop enumerating, return failure to the app.
        }
        data->num_entries++;
    }

    return 1;  // keep enumerating.
}

int SDL_InitGlobDirectoryCallback(const char *path, const char *pattern, SDL_GlobDirCallbackData *data)
{
    SDL_assert(data != NULL);

    if (!path) {
        SDL_InvalidParamError("path");
        return -1;
    }

    SDL_zerop(data);
    data->pattern = pattern;
    data->string_stream = SDL_IOFromDynamicMem();
    if (!data->string_stream) {
        return -1;
    }

    return 0;
}

char **SDL_ProcessGlobDirectoryCallback(SDL_GlobDirCallbackData *data, int *count)
{
    const size_t streamlen = (size_t) SDL_GetIOSize(data->string_stream);
    const size_t buflen = streamlen + ((data->num_entries + 1) * sizeof (char *));  // +1 for NULL terminator at end of array.
    char **retval = (char **) SDL_malloc(buflen);
    if (retval) {
        if (data->num_entries > 0) {
            Sint64 iorc = SDL_SeekIO(data->string_stream, 0, SDL_IO_SEEK_SET);
            SDL_assert(iorc == 0);  // this should never fail for a memory stream!
            char *ptr = (char *) (retval + (data->num_entries + 1));
            iorc = SDL_ReadIO(data->string_stream, ptr, streamlen);
            SDL_assert(iorc == (Sint64) streamlen);  // this should never fail for a memory stream!
            for (int i = 0; i < data->num_entries; i++) {
                retval[i] = ptr;
                ptr += SDL_strlen(ptr) + 1;
            }
        }
        retval[data->num_entries] = NULL;  // NULL terminate the list.
        *count = data->num_entries;
    }
    return retval;
}


void SDL_QuitGlobDirectoryCallback(SDL_GlobDirCallbackData *data)
{
    if (data->string_stream) {
        SDL_CloseIO(data->string_stream);
    }
}

char **SDL_GlobDirectory(const char *path, const char *pattern, int *count)
{
    int dummycount;
    if (!count) {
        count = &dummycount;
    }
    *count = 0;

    char **retval = NULL;
    SDL_GlobDirCallbackData data;

    if (SDL_InitGlobDirectoryCallback(path, pattern, &data) < 0) {
        return NULL;
    } else if (SDL_EnumerateDirectory(path, SDL_GlobDirectoryCallback, &data) == 0) {
        retval = SDL_ProcessGlobDirectoryCallback(&data, count);
    }
    SDL_QuitGlobDirectoryCallback(&data);
    return retval;
}

