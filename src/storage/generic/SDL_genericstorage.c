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

#include "../SDL_sysstorage.h"

static char *GENERIC_INTERNAL_CreateFullPath(const char *base, const char *relative)
{
    size_t len = 0;

    if (base) {
        len += SDL_strlen(base);
    }
    len += SDL_strlen(relative) + 1;

    char *result = (char*) SDL_malloc(len);
    if (result != NULL) {
        SDL_snprintf(result, len, "%s%s", base, relative);
    }
    return result;
}

static int GENERIC_StorageClose(void *userdata)
{
    SDL_free(userdata);
    return 0;
}

static SDL_bool GENERIC_StorageReady(void *userdata)
{
    return SDL_TRUE;
}

static int GENERIC_StorageFileSize(void *userdata, const char *path, Uint64 *length)
{
    SDL_IOStream *stream;
    Sint64 result;

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char*) userdata, path);
    if (fullpath == NULL) {
        return SDL_OutOfMemory();
    }
    stream = SDL_IOFromFile(fullpath, "rb");
    SDL_free(fullpath);

    result = SDL_SizeIO(stream);
    SDL_CloseIO(stream);
    if (result < 0) {
        return result;
    }

    /* FIXME: Should SDL_SizeIO use u64 now...? */
    *length = (Uint64) result;
    return 0;
}

static int GENERIC_StorageReadFile(void *userdata, const char *path, void *destination, Uint64 length)
{
    SDL_IOStream *stream;
    char *fullpath;
    int fullread;

    if (length > SDL_SIZE_MAX) {
        return SDL_SetError("Read size exceeds SDL_SIZE_MAX");
    }

    fullpath = GENERIC_INTERNAL_CreateFullPath((char*) userdata, path);
    if (fullpath == NULL) {
        return SDL_OutOfMemory();
    }
    stream = SDL_IOFromFile(fullpath, "rb");
    SDL_free(fullpath);

    /* FIXME: Should SDL_ReadIO use u64 now...? */
    fullread = (SDL_ReadIO(stream, destination, (size_t) length) == length);
    SDL_CloseIO(stream);
    return fullread - 1;
}

static int GENERIC_StorageWriteFile(void *userdata, const char *path, const void *source, Uint64 length)
{
    /* TODO: Recursively create subdirectories with SDL_mkdir */
    SDL_IOStream *stream;
    char *fullpath;
    int fullwrite;

    if (length > SDL_SIZE_MAX) {
        return SDL_SetError("Write size exceeds SDL_SIZE_MAX");
    }

    fullpath = GENERIC_INTERNAL_CreateFullPath((char*) userdata, path);
    if (fullpath == NULL) {
        return SDL_OutOfMemory();
    }
    stream = SDL_IOFromFile(fullpath, "wb");
    SDL_free(fullpath);

    /* FIXME: Should SDL_WriteIO use u64 now...? */
    fullwrite = (SDL_WriteIO(stream, source, (size_t) length) == length);
    SDL_CloseIO(stream);
    return fullwrite - 1;
}

static Uint64 GENERIC_StorageSpaceRemaining(void *userdata)
{
    /* TODO: There's totally a way to query a folder root's quota... */
    return SDL_MAX_UINT64;
}

static const SDL_StorageInterface GENERIC_title_iface = {
    GENERIC_StorageClose,
    GENERIC_StorageReady,
    GENERIC_StorageFileSize,
    GENERIC_StorageReadFile,
    NULL,
    NULL
};

static SDL_Storage *GENERIC_Title_Create(const char *override, SDL_PropertiesID props)
{
    SDL_Storage *result;

    char *basepath;
    if (override != NULL) {
        basepath = SDL_strdup(override);
    } else {
        basepath = SDL_GetBasePath();
    }
    if (basepath == NULL) {
        return NULL;
    }

    result = SDL_OpenStorage(&GENERIC_title_iface, basepath);
    if (result == NULL) {
        SDL_free(basepath);
    }
    return result;
}

TitleStorageBootStrap GENERIC_titlebootstrap = {
    "generic",
    "SDL generic title storage driver",
    GENERIC_Title_Create
};

static const SDL_StorageInterface GENERIC_user_iface = {
    GENERIC_StorageClose,
    GENERIC_StorageReady,
    GENERIC_StorageFileSize,
    GENERIC_StorageReadFile,
    GENERIC_StorageWriteFile,
    GENERIC_StorageSpaceRemaining
};

static SDL_Storage *GENERIC_User_Create(const char *org, const char *app, SDL_PropertiesID props)
{
    SDL_Storage *result;

    char *prefpath = SDL_GetPrefPath(org, app);
    if (prefpath == NULL) {
        return NULL;
    }

    result = SDL_OpenStorage(&GENERIC_user_iface, prefpath);
    if (result == NULL) {
        SDL_free(prefpath);
    }
    return result;
}

UserStorageBootStrap GENERIC_userbootstrap = {
    "generic",
    "SDL generic user storage driver",
    GENERIC_User_Create
};

static const SDL_StorageInterface GENERIC_file_iface = {
    GENERIC_StorageClose,
    GENERIC_StorageReady,
    GENERIC_StorageFileSize,
    GENERIC_StorageReadFile,
    GENERIC_StorageWriteFile,
    GENERIC_StorageSpaceRemaining
};

SDL_Storage *GENERIC_OpenFileStorage(const char *path)
{
    SDL_Storage *result;
    size_t len = 0;
    char *basepath = NULL;

    if (path) {
        len += SDL_strlen(path);
    }
    if (len > 0) {
        if (path[len-1] == '/') {
            basepath = SDL_strdup(path);
            if (!basepath) {
                return NULL;
            }
        } else {
            if (SDL_asprintf(&basepath, "%s/", path) < 0) {
                return NULL;
            }
        }
    }
    result = SDL_OpenStorage(&GENERIC_file_iface, basepath);
    if (result == NULL) {
        SDL_free(basepath);
    }
    return result;
}
