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
#include "../stdlib/SDL_sysstdlib.h"

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

static SDL_bool EverythingMatch(const char *pattern, const char *str, SDL_bool *matched_to_dir)
{
    SDL_assert(pattern == NULL);
    SDL_assert(str != NULL);
    SDL_assert(matched_to_dir != NULL);

    *matched_to_dir = SDL_TRUE;
    return SDL_TRUE;  // everything matches!
}

// this is just '*' and '?', with '/' matching nothing.
static SDL_bool WildcardMatch(const char *pattern, const char *str, SDL_bool *matched_to_dir)
{
    SDL_assert(pattern != NULL);
    SDL_assert(str != NULL);
    SDL_assert(matched_to_dir != NULL);

    const char *str_backtrack = NULL;
    const char *pattern_backtrack = NULL;
    char sch_backtrack = 0;
    char sch = *str;
    char pch = *pattern;

    while (sch) {
        if (pch == '*') {
            str_backtrack = str;
            pattern_backtrack = ++pattern;
            sch_backtrack = sch;
            pch = *pattern;
        } else if (pch == sch) {
            if (pch == '/') {
                str_backtrack = pattern_backtrack = NULL;
            }
            sch = *(++str);
            pch = *(++pattern);
        } else if ((pch == '?') && (sch != '/')) {  // end of string (checked at `while`) or path separator do not match '?'.
            sch = *(++str);
            pch = *(++pattern);
        } else if (!pattern_backtrack || (sch_backtrack == '/')) { // we didn't have a match. Are we in a '*' and NOT on a path separator? Keep going. Otherwise, fail.
            *matched_to_dir = SDL_FALSE;
            return SDL_FALSE;
        } else {  // still here? Wasn't a match, but we're definitely in a '*' pattern.
            str = ++str_backtrack;
            pattern = pattern_backtrack;
            sch_backtrack = sch;
            sch = *str;
            pch = *pattern;
        }
    }

    // '*' at the end can be ignored, they are allowed to match nothing.
    while (pch == '*') {
        pch = *(++pattern);
    }

    *matched_to_dir = ((pch == '/') || (pch == '\0'));  // end of string and the pattern is complete or failed at a '/'? We should descend into this directory.

    return (pch == '\0');  // survived the whole pattern? That's a match!
}


// Note that this will currently encode illegal codepoints: UTF-16 surrogates, 0xFFFE, and 0xFFFF.
// and a codepoint > 0x10FFFF will fail the same as if there wasn't enough memory.
// clean this up if you want to move this to SDL_string.c.
static size_t EncodeCodepointToUtf8(char *ptr, Uint32 cp, size_t remaining)
{
    if (cp < 0x80) {  // fits in a single UTF-8 byte.
        if (remaining) {
            *ptr = (char) cp;
            return 1;
        }
    } else if (cp < 0x800) {  // fits in 2 bytes.
        if (remaining >= 2) {
            ptr[0] = (char) ((cp >> 6) | 128 | 64);
            ptr[1] = (char) (cp & 0x3F) | 128;
            return 2;
        }
    } else if (cp < 0x10000) { // fits in 3 bytes.
        if (remaining >= 3) {
            ptr[0] = (char) ((cp >> 12) | 128 | 64 | 32);
            ptr[1] = (char) ((cp >> 6) & 0x3F) | 128;
            ptr[2] = (char) (cp & 0x3F) | 128;
            return 3;
        }
    } else if (cp <= 0x10FFFF) {  // fits in 4 bytes.
        if (remaining >= 4) {
            ptr[0] = (char) ((cp >> 18) | 128 | 64 | 32 | 16);
            ptr[1] = (char) ((cp >> 12) & 0x3F) | 128;
            ptr[2] = (char) ((cp >> 6) & 0x3F) | 128;
            ptr[3] = (char) (cp & 0x3F) | 128;
            return 4;
        }
    }

    return 0;
}

static char *CaseFoldUtf8String(const char *fname)
{
    SDL_assert(fname != NULL);
    const size_t allocation = (SDL_strlen(fname) + 1) * 3 * 4;
    char *retval = (char *) SDL_malloc(allocation);  // lazy: just allocating the max needed.
    if (!retval) {
        return NULL;
    }

    Uint32 codepoint;
    char *ptr = retval;
    size_t remaining = allocation;
    while ((codepoint = SDL_StepUTF8(&fname, NULL)) != 0) {
        Uint32 folded[3];
        const int num_folded = SDL_CaseFoldUnicode(codepoint, folded);
        SDL_assert(num_folded > 0);
        SDL_assert(num_folded <= SDL_arraysize(folded));
        for (int i = 0; i < num_folded; i++) {
            SDL_assert(remaining > 0);
            const size_t rc = EncodeCodepointToUtf8(ptr, folded[i], remaining);
            SDL_assert(rc > 0);
            SDL_assert(rc < remaining);
            remaining -= rc;
            ptr += rc;
        }
    }

    SDL_assert(remaining > 0);
    remaining--;
    *ptr = '\0';

    if (remaining > 0) {
        SDL_assert(allocation > remaining);
        ptr = SDL_realloc(retval, allocation - remaining);  // shrink it down.
        if (ptr) {  // shouldn't fail, but if it does, `retval` is still valid.
            retval = ptr;
        }
    }

    return retval;
}


typedef struct GlobDirCallbackData
{
    SDL_bool (*matcher)(const char *pattern, const char *str, SDL_bool *matched_to_dir);
    const char *pattern;
    int num_entries;
    SDL_GlobFlags flags;
    SDL_GlobEnumeratorFunc enumerator;
    SDL_GlobGetPathInfoFunc getpathinfo;
    void *fsuserdata;
    size_t basedirlen;
    SDL_IOStream *string_stream;
} GlobDirCallbackData;

static int SDLCALL GlobDirectoryCallback(void *userdata, const char *dirname, const char *fname)
{
    SDL_assert(userdata != NULL);
    SDL_assert(dirname != NULL);
    SDL_assert(fname != NULL);

    //SDL_Log("GlobDirectoryCallback('%s', '%s')", dirname, fname);

    GlobDirCallbackData *data = (GlobDirCallbackData *) userdata;

    // !!! FIXME: if we're careful, we can keep a single buffer in `data` that we push and pop paths off the end of as we walk the tree,
    // !!! FIXME: and only casefold the new pieces instead of allocating and folding full paths for all of this.

    char *fullpath = NULL;
    if (SDL_asprintf(&fullpath, "%s/%s", dirname, fname) < 0) {
        return -1;
    }

    char *folded = NULL;
    if (data->flags & SDL_GLOB_CASEINSENSITIVE) {
        folded = CaseFoldUtf8String(fullpath);
        if (!folded) {
            return -1;
        }
    }

    SDL_bool matched_to_dir = SDL_FALSE;
    const SDL_bool matched = data->matcher(data->pattern, (folded ? folded : fullpath) + data->basedirlen, &matched_to_dir);
    //SDL_Log("GlobDirectoryCallback: Considered %spath='%s' vs pattern='%s': %smatched (matched_to_dir=%s)", folded ? "(folded) " : "", (folded ? folded : fullpath) + data->basedirlen, data->pattern, matched ? "" : "NOT ", matched_to_dir ? "TRUE" : "FALSE");
    SDL_free(folded);

    if (matched) {
        const char *subpath = fullpath + data->basedirlen;
        const size_t slen = SDL_strlen(subpath) + 1;
        if (SDL_WriteIO(data->string_stream, subpath, slen) != slen) {
            SDL_free(fullpath);
            return -1;  // stop enumerating, return failure to the app.
        }
        data->num_entries++;
    }

    int retval = 1;  // keep enumerating by default.
    if (matched_to_dir) {
        SDL_PathInfo info;
        if ((data->getpathinfo(fullpath, &info, data->fsuserdata) == 0) && (info.type == SDL_PATHTYPE_DIRECTORY)) {
            //SDL_Log("GlobDirectoryCallback: Descending into subdir '%s'", fname);
            if (data->enumerator(fullpath, GlobDirectoryCallback, data, data->fsuserdata) < 0) {
                retval = -1;
            }
        }
    }

    SDL_free(fullpath);

    return retval;
}

char **SDL_InternalGlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count, SDL_GlobEnumeratorFunc enumerator, SDL_GlobGetPathInfoFunc getpathinfo, void *userdata)
{
    int dummycount;
    if (!count) {
        count = &dummycount;
    }
    *count = 0;

    if (!path) {
        SDL_InvalidParamError("path");
        return NULL;
    }

    // if path ends with any '/', chop them off, so we don't confuse the pattern matcher later.
    char *pathcpy = NULL;
    size_t pathlen = SDL_strlen(path);
    if (pathlen && (path[pathlen-1] == '/')) {
        pathcpy = SDL_strdup(path);
        if (!pathcpy) {
            return NULL;
        }
        char *ptr = &pathcpy[pathlen-1];
        while ((ptr >= pathcpy) && (*ptr == '/')) {
            *(ptr--) = '\0';
        }
        path = pathcpy;
    }

    if (!pattern) {
        flags &= ~SDL_GLOB_CASEINSENSITIVE;  // avoid some unnecessary allocations and work later.
    }

    char *folded = NULL;
    if (flags & SDL_GLOB_CASEINSENSITIVE) {
        SDL_assert(pattern != NULL);
        folded = CaseFoldUtf8String(pattern);
        if (!folded) {
            SDL_free(pathcpy);
            return NULL;
        }
    }

    GlobDirCallbackData data;
    SDL_zero(data);
    data.string_stream = SDL_IOFromDynamicMem();
    if (!data.string_stream) {
        SDL_free(folded);
        SDL_free(pathcpy);
        return NULL;
    }

    if (!pattern) {
        data.matcher = EverythingMatch;  // no pattern? Everything matches.

    // !!! FIXME
    //} else if (flags & SDL_GLOB_GITIGNORE) {
    //    data.matcher = GitIgnoreMatch;

    } else {
        data.matcher = WildcardMatch;
    }

    data.pattern = folded ? folded : pattern;
    data.flags = flags;
    data.enumerator = enumerator;
    data.getpathinfo = getpathinfo;
    data.fsuserdata = userdata;
    data.basedirlen = SDL_strlen(path) + 1;  // +1 for the '/' we'll be adding.

    char **retval = NULL;
    if (data.enumerator(path, GlobDirectoryCallback, &data, data.fsuserdata) == 0) {
        const size_t streamlen = (size_t) SDL_GetIOSize(data.string_stream);
        const size_t buflen = streamlen + ((data.num_entries + 1) * sizeof (char *));  // +1 for NULL terminator at end of array.
        retval = (char **) SDL_malloc(buflen);
        if (retval) {
            if (data.num_entries > 0) {
                Sint64 iorc = SDL_SeekIO(data.string_stream, 0, SDL_IO_SEEK_SET);
                SDL_assert(iorc == 0);  // this should never fail for a memory stream!
                char *ptr = (char *) (retval + (data.num_entries + 1));
                iorc = SDL_ReadIO(data.string_stream, ptr, streamlen);
                SDL_assert(iorc == (Sint64) streamlen);  // this should never fail for a memory stream!
                for (int i = 0; i < data.num_entries; i++) {
                    retval[i] = ptr;
                    ptr += SDL_strlen(ptr) + 1;
                }
            }
            retval[data.num_entries] = NULL;  // NULL terminate the list.
            *count = data.num_entries;
        }
    }

    SDL_CloseIO(data.string_stream);
    SDL_free(folded);
    SDL_free(pathcpy);

    return retval;
}

static int GlobDirectoryGetPathInfo(const char *path, SDL_PathInfo *info, void *userdata)
{
    return SDL_GetPathInfo(path, info);
}

static int GlobDirectoryEnumerator(const char *path, SDL_EnumerateDirectoryCallback cb, void *cbuserdata, void *userdata)
{
    return SDL_EnumerateDirectory(path, cb, cbuserdata);
}

char **SDL_GlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count)
{
    //SDL_Log("SDL_GlobDirectory('%s', '%s') ...", path, pattern);
    return SDL_InternalGlobDirectory(path, pattern, flags, count, GlobDirectoryEnumerator, GlobDirectoryGetPathInfo, NULL);
}

