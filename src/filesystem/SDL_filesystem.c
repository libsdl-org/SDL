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
    } else if (fs->open == NULL) {
        SDL_Unsupported();
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
        return SDL_InvalidParamError("path");
    } else if (!cb) {
        return SDL_InvalidParamError("cb");
    } else if (fs->enumerate == NULL) {
        return SDL_Unsupported();
    }
    return fs->enumerate(fs, path, cb, userdata);
}

int SDL_FSremove(SDL_FSops *fs, const char *path)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        return SDL_InvalidParamError("path");
    } else if (fs->remove == NULL) {
        return SDL_Unsupported();
    }
    return fs->remove(fs, path);
}

int SDL_FSrename(SDL_FSops *fs, const char *oldpath, const char *newpath)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!oldpath) {
        return SDL_InvalidParamError("oldpath");
    } else if (!newpath) {
        return SDL_InvalidParamError("newpath");
    } else if (fs->rename == NULL) {
        return SDL_Unsupported();
    }
    return fs->rename(fs, oldpath, newpath);
}

int SDL_FSmkdir(SDL_FSops *fs, const char *path)
{
    if (!fs) {
        return SDL_InvalidParamError("fs");
    } else if (!path) {
        return SDL_InvalidParamError("path");
    } else if (fs->mkdir == NULL) {
        return SDL_Unsupported();
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
    } else if (fs->stat == NULL) {
        return SDL_Unsupported();
    }
    return fs->stat(fs, path, stat);
}

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

Sint64 SDL_FileTimeToUnix(Sint64 ftime)
{
    // SDL time uses seconds since the unix epoch, so we're already there.
    return ftime;
}


// this assumes `sanitized` is as large a buffer as `path`!
static SDL_bool SanitizePath(char *sanitized, const char *path, const size_t pathlen)
{
    // there is probably a more efficient way to do this, that parses `path` as it
    // copies to the destination, but it's going to be just as complicated and
    // twice as fragile.

    // skip past any path separators at the start of the string.
    #ifdef SDL_PLATFORM_WINDOWS
    while ((*path == '\\') || (*path == '/')) {
        path++;
    }
    #else
    while (*path == '/') {
        path++;
    }
    #endif

    // copy `path` to `sanitized`, flipping any Windows path seps to '/' and counting pieces of the path.
    int num_pieces = 1;
    int i = 0;
    while (path[i]) {
        const char ch = path[i];

        #ifdef SDL_PLATFORM_WINDOWS
        const SDL_bool issep = (ch == '\\') || (ch == '/');
        sanitized[i++] = issep ? '/' : ch;
        #else
        const SDL_bool issep = (ch == '/');
        sanitized[i++] = ch;
        #endif

        if (issep) {
            num_pieces++;
        }
    }

    SDL_assert(i < pathlen);
    sanitized[i] = '\0';

    // `sanitized` is now a copy of path where all path separators are '/'

    if (*sanitized == '\0') {
        return SDL_TRUE;  // it's an empty string, we're already done.
    }

    // Allocate a list of pointers to individual pieces of the path.
    SDL_bool isstack;
    char **pieces = SDL_small_alloc(char *, num_pieces, &isstack);
    if (!pieces) {
        return SDL_FALSE;
    }

    // build the list, null-terminate the pieces.
    char *start = sanitized;
    char *ptr;
    i = 0;
    while ((ptr = SDL_strchr(start, '/')) != NULL) {
        SDL_assert(i < num_pieces);
        pieces[i++] = start;
        *ptr = '\0';
        start = ptr + 1;
    }

    SDL_assert(i == (num_pieces - 1));
    pieces[i] = start;  // get the last piece that didn't have a separator after it.

    for (i = 0; i < num_pieces; i++) {
        char *piece = pieces[i];
        if (*piece == '\0') {
            pieces[i] = NULL;  // empty path, dump it ("a/b//.." will end up in a, not b, so clear these empties out up front).
        } else if (piece[0] == '.') {
            if (piece[1] == '\0') {
                pieces[i] = NULL;  // a "." path, kill it.
            } else if ((piece[1] == '.') && (piece[2] == '\0')) {
                pieces[i] = NULL;  // a ".." path, kill it.
                for (int j = i - 1; j >= 0; j--) {
                    if (pieces[j]) {
                        pieces[j] = NULL;  // kill next parent directory, too.
                        break;
                    }
                }
            }
        }
    }

    char *dst = sanitized;
    for (i = 0; i < num_pieces; i++) {
        char *piece = pieces[i];
        if (piece) {
            const size_t slen = SDL_strlen(piece);
            if (dst > sanitized) {
                #ifdef SDL_PLATFORM_WINDOWS  // might as well give the "blessed" path separator here, even though '/' also works.
                *(dst++) = '\\';
                #else
                *(dst++) = '/';
                #endif
            }
            if (piece != dst) {  // if already in the right place, don't copy it.
                SDL_memmove(dst, piece, slen);
            }
            dst += slen;
        }
    }

    SDL_assert(dst <= (sanitized + pathlen));
    *dst = '\0';

    SDL_small_free(pieces, isstack);

    return SDL_TRUE;
}

static char *AssembleNativeFsPath(SDL_FSops *fs, const char *path)
{
    const char *base = (const char *) fs->opaque;
    const size_t baselen = base ? SDL_strlen(base) : 0;
    const size_t pathlen = path ? SDL_strlen(path) : 0;
    SDL_bool isstack = SDL_FALSE;
    char *sanitized = path ? SDL_small_alloc(char, pathlen + 1, &isstack) : NULL;
    if (path) {
        if (!sanitized) {
            return NULL;
        } else if (!SanitizePath(sanitized, path, pathlen + 1)) {
            SDL_small_free(sanitized, isstack);
            return NULL;
        }
    }

    const size_t sanitizedlen = sanitized ? SDL_strlen(sanitized) : 0;
    const size_t fullpathlen = baselen + sanitizedlen + 2;
    char *fullpath = (char *) SDL_malloc(fullpathlen);
    if (!fullpath) {
        SDL_small_free(sanitized, isstack);
        return NULL;
    }

    const size_t slen = SDL_snprintf(fullpath, fullpathlen, "%s%s%s", base ? base : "", base ? PLATFORM_PATH_SEPARATOR : PLATFORM_ROOT_PATH, sanitized ? sanitized : "");
    SDL_assert(slen < fullpathlen);

    if (sanitized) {
        SDL_small_free(sanitized, isstack);
    }

    return fullpath;
}


static SDL_RWops * SDLCALL nativefs_open(SDL_FSops *fs, const char *path, const char *mode)
{
    SDL_RWops *rwops = NULL;
    char *fullpath = AssembleNativeFsPath(fs, path);
    if (fullpath) {
        rwops = SDL_RWFromFile(fullpath, mode);
        SDL_free(fullpath);
    }
    return rwops;
}

static int nativefs_enumerate(SDL_FSops *fs, const char *path, SDL_EnumerateCallback cb, void *userdata)
{
    int retval = -1;
    char *fullpath = AssembleNativeFsPath(fs, path);
    if (fullpath) {
        retval = SDL_SYS_FSenumerate(fs, fullpath, path, cb, userdata);
        SDL_free(fullpath);
    }
    return retval;
}

static int SDLCALL nativefs_remove(SDL_FSops *fs, const char *path)
{
    int retval = -1;
    char *fullpath = AssembleNativeFsPath(fs, path);
    if (fullpath) {
        retval = SDL_SYS_FSremove(fs, fullpath);
        SDL_free(fullpath);
    }
    return retval;
}

static int SDLCALL nativefs_rename(SDL_FSops *fs, const char *oldpath, const char *newpath)
{
    int retval = -1;
    char *oldfullpath = AssembleNativeFsPath(fs, oldpath);
    if (oldfullpath) {
        char *newfullpath = AssembleNativeFsPath(fs, newpath);
        if (newfullpath) {
            retval = SDL_SYS_FSrename(fs, oldfullpath, newfullpath);
            SDL_free(newfullpath);
        }
        SDL_free(oldfullpath);
    }
    return retval;
}

static int SDLCALL nativefs_mkdir(SDL_FSops *fs, const char *path)
{
    int retval = -1;
    char *fullpath = AssembleNativeFsPath(fs, path);
    if (fullpath) {
        retval = SDL_SYS_FSmkdir(fs, fullpath);
        SDL_free(fullpath);
    }
    return retval;
}

static int SDLCALL nativefs_stat(SDL_FSops *fs, const char *path, SDL_Stat *stat)
{
    int retval = -1;
    char *fullpath = AssembleNativeFsPath(fs, path);
    if (fullpath) {
        retval = SDL_SYS_FSstat(fs, fullpath, stat);
        SDL_free(fullpath);
    }
    return retval;
}

static void SDLCALL nativefs_closefs(SDL_FSops *fs)
{
    if (fs) {
        SDL_free(fs->opaque);
    }
}

SDL_FSops *SDL_CreateFilesystem(const char *basedir)
{
    SDL_FSops *fs = (SDL_FSops *) SDL_calloc(1, sizeof (SDL_FSops));
    if (fs) {
        if (basedir) {
            const size_t buflen = SDL_strlen(basedir) + 1;
            char *sanitized = (char *) SDL_malloc(buflen);

            // save an absolute path char that SanitizePath will trim off...
            #ifdef SDL_PLATFORM_WINDOWS
            const int absolute_path = ((*basedir == '\\') || (*basedir == '/')) ? 1 : 0;
            #else
            const int absolute_path = (*basedir == '/') ? 1 : 0;
            #endif

            if (!sanitized) {
                SDL_free(fs);
                return NULL;
            } else if (!SanitizePath(sanitized + absolute_path, basedir + absolute_path, buflen - absolute_path)) {
                SDL_free(sanitized);
                SDL_free(fs);
                return NULL;
            }

            if (absolute_path) {
                #ifdef SDL_PLATFORM_WINDOWS
                *sanitized = '\\';
                #else
                *sanitized = '/';
                #endif
            } else if (*sanitized == '\0') {
                SDL_free(sanitized);
                sanitized = NULL;
            }

            fs->opaque = sanitized;
        }

        fs->open = nativefs_open;
        fs->enumerate = nativefs_enumerate;
        fs->remove = nativefs_remove;
        fs->rename = nativefs_rename;
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

