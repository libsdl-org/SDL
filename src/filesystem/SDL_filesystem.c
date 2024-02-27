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

#ifdef SDL_PLATFORM_WINDOWS
    #define PLATFORM_PATH_SEPARATOR "\\"
    #define PLATFORM_ROOT_PATH ""
#else
    #define PLATFORM_PATH_SEPARATOR  "/"
    #define PLATFORM_ROOT_PATH "/"
#endif


SDL_PropertiesID SDL_GetFSProperties(SDL_FSops *context)
{
    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    }

    if (context->props == 0) {
        context->props = SDL_CreateProperties();
    }
    return context->props;
}

SDL_RWops *SDL_FSopen(SDL_FSops *fs, const char *path, const char *mode)
{
    SDL_RWops *retval = NULL;
    if (!fs) {
        SDL_InvalidParamError("fs");
    } else if (!path) {
        SDL_InvalidParamError("path");
    } else if (!mode) {
        SDL_InvalidParamError("mode");
    } else {
        retval = fs->open(fs, path, mode);
    }
    return retval;
}

int SDL_FSenumerate(SDL_FSops *fs, const char *path, SDL_EnumerateCallback cb, void *userdata)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        SDL_InvalidParamError("path");
    } else if (!cb) {
        return SDL_InvalidParamError("cb");
    }
    return fs->enumerate(fs, path, cb, userdata);
}

int SDL_FSremove(SDL_FSops *fs, const char *path)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        return SDL_InvalidParamError("path");
    }
    return fs->remove(fs, path);
}

int SDL_FSmkdir(SDL_FSops *fs, const char *path)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        return SDL_InvalidParamError("path");
    }
    return fs->mkdir(fs, path);
}

int SDL_FSstat(SDL_FSops *fs, const char *path, SDL_Stat *stat)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        return SDL_InvalidParamError("path");
    } else if (!stat) {
        return SDL_InvalidParamError("stat");
    }
    return fs->stat(fs, path, stat);
}

void SDL_FileTimeToWindows(Uint64 ftime, Uint32 *low, Uint32 *high)
{
    const Uint64 delta_1601_epoch_ns = 11644473600ull * SDL_NS_PER_SECOND; // [ns] (nanoseconds between 1/1/1601 and 1/1/1970, 11644473600 seconds * 1000000000)
    const Uint64 cvt = (ftime + delta_1601_epoch_ns) / 100ull; // [100ns] (adjust to epoch and convert nanoseconds to 1/100th nanosecond units).
    if (low) {
        *low = (Uint32) cvt;
    }
    if (high) {
        *high = (Uint32) (cvt >> 32);
    }
}

Uint64 SDL_FileTimeToUnix(Uint64 ftime)
{
    // SDL time uses the unix epoch, but in nanoseconds. Divide to convert to seconds.
    return ftime / SDL_NS_PER_SECOND;
}


#define MAKEFULLPATH() \
    const char *base = (const char *) fs->opaque; \
    SDL_bool isstack; \
    const size_t fullpathlen = (base ? SDL_strlen(base) : 0) + (path ? SDL_strlen(path) : 0) + 8; \
    char *fullpath = SDL_small_alloc(char, fullpathlen, &isstack); \
    if (fullpath) { SDL_snprintf(fullpath, fullpathlen, "%s%s%s", base ? base : "", base ? PLATFORM_PATH_SEPARATOR : PLATFORM_ROOT_PATH, path ? path : ""); }

static SDL_RWops * SDLCALL nativefs_open(SDL_FSops *fs, const char *path, const char *mode)
{
    SDL_RWops *rwops = NULL;
    MAKEFULLPATH();
    if (fullpath) {
        rwops = SDL_RWFromFile(fullpath, mode);
        SDL_small_free(fullpath, isstack);
    }
    return rwops;
}

static int nativefs_enumerate(SDL_FSops *fs, const char *path, SDL_EnumerateCallback cb, void *userdata)
{
    const char *pathsep = PLATFORM_PATH_SEPARATOR;
    const char pathsepchar = pathsep[0];
    int retval = -1;
    MAKEFULLPATH();
    if (fullpath) {
        for (int i = ((int) SDL_strlen(fullpath)) - 1; i >= 0; i++) {
            if (fullpath[i] != pathsepchar) {
                break;
            }
            fullpath[i] = '\0';
        }
        retval = SDL_SYS_FSenumerate(fs, fullpath, path, cb, userdata);
        SDL_small_free(fullpath, isstack);
    }
    return retval;
}

static int SDLCALL nativefs_remove(SDL_FSops *fs, const char *path)
{
    int retval = -1;
    MAKEFULLPATH();
    if (fullpath) {
        retval = SDL_SYS_FSremove(fs, fullpath);
        SDL_small_free(fullpath, isstack);
    }
    return retval;
}

static int SDLCALL nativefs_mkdir(SDL_FSops *fs, const char *path)
{
    int retval = -1;
    MAKEFULLPATH();
    if (fullpath) {
        retval = SDL_SYS_FSmkdir(fs, fullpath);
        SDL_small_free(fullpath, isstack);
    }
    return retval;
}

static int SDLCALL nativefs_stat(SDL_FSops *fs, const char *path, SDL_Stat *stat)
{
    int retval = -1;
    MAKEFULLPATH();
    if (fullpath) {
        retval = SDL_SYS_FSstat(fs, fullpath, stat);
        SDL_small_free(fullpath, isstack);
    }
    return retval;
}

static void SDLCALL nativefs_closefs(SDL_FSops *fs)
{
    if (fs) {
        SDL_free(fs->opaque);
    }
}

#undef MAKEFULLPATH


SDL_FSops *SDL_CreateFilesystem(const char *basedir)
{
    SDL_FSops *fs = (SDL_FSops *) SDL_calloc(1, sizeof (SDL_FSops));
    if (fs) {
        if (basedir) {
            fs->opaque = SDL_strdup(basedir);
            if (!fs->opaque) {
                SDL_free(fs);
                return NULL;
            }
        }

        fs->open = nativefs_open;
        fs->enumerate = nativefs_enumerate;
        fs->remove = nativefs_remove;
        fs->mkdir = nativefs_mkdir;
        fs->stat = nativefs_stat;
        fs->closefs = nativefs_closefs;
    }
    return fs;
}

void SDL_DestroyFilesystem(SDL_FSops *fs)
{
    if (fs) {
        if (fs->closefs) {
            fs->closefs(fs);
        }
        SDL_DestroyProperties(fs->props);
        SDL_free(fs);
    }
}

