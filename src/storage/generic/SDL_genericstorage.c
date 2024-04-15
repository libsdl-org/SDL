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
    if (!base) {
        return SDL_strdup(relative);
    }

    size_t len = SDL_strlen(base) + SDL_strlen(relative) + 1;
    char *result = (char*)SDL_malloc(len);
    if (result != NULL) {
        SDL_snprintf(result, len, "%s%s", base, relative);
    }
    return result;
}

static int GENERIC_CloseStorage(void *userdata)
{
    SDL_free(userdata);
    return 0;
}

static int GENERIC_EnumerateStorageDirectory(void *userdata, const char *path, SDL_EnumerateDirectoryCallback callback, void *callback_userdata)
{
    int result = -1;

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        result = SDL_EnumerateDirectory(fullpath, callback, callback_userdata);

        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_GetStoragePathInfo(void *userdata, const char *path, SDL_PathInfo *info)
{
    int result = -1;

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        result = SDL_GetPathInfo(fullpath, info);

        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_ReadStorageFile(void *userdata, const char *path, void *destination, Uint64 length)
{
    int result = -1;

    if (length > SDL_SIZE_MAX) {
        return SDL_SetError("Read size exceeds SDL_SIZE_MAX");
    }

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        SDL_IOStream *stream = SDL_IOFromFile(fullpath, "rb");
        if (stream) {
            /* FIXME: Should SDL_ReadIO use u64 now...? */
            if (SDL_ReadIO(stream, destination, (size_t)length) == length) {
                result = 0;
            }
            SDL_CloseIO(stream);
        }
        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_WriteStorageFile(void *userdata, const char *path, const void *source, Uint64 length)
{
    /* TODO: Recursively create subdirectories with SDL_CreateDirectory */
    int result = -1;

    if (length > SDL_SIZE_MAX) {
        return SDL_SetError("Write size exceeds SDL_SIZE_MAX");
    }

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        SDL_IOStream *stream = SDL_IOFromFile(fullpath, "wb");

        if (stream) {
            /* FIXME: Should SDL_WriteIO use u64 now...? */
            if (SDL_WriteIO(stream, source, (size_t)length) == length) {
                result = 0;
            }
            SDL_CloseIO(stream);
        }
        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_CreateStorageDirectory(void *userdata, const char *path)
{
    /* TODO: Recursively create subdirectories with SDL_CreateDirectory */
    int result = -1;

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        result = SDL_CreateDirectory(fullpath);

        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_RemoveStoragePath(void *userdata, const char *path)
{
    int result = -1;

    char *fullpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, path);
    if (fullpath) {
        result = SDL_RemovePath(fullpath);

        SDL_free(fullpath);
    }
    return result;
}

static int GENERIC_RenameStoragePath(void *userdata, const char *oldpath, const char *newpath)
{
    int result = -1;

    char *fulloldpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, oldpath);
    char *fullnewpath = GENERIC_INTERNAL_CreateFullPath((char *)userdata, newpath);
    if (fulloldpath && fullnewpath) {
        result = SDL_RenamePath(fulloldpath, fullnewpath);
    }
    SDL_free(fulloldpath);
    SDL_free(fullnewpath);

    return result;
}

static Uint64 GENERIC_GetStorageSpaceRemaining(void *userdata)
{
    /* TODO: There's totally a way to query a folder root's quota... */
    return SDL_MAX_UINT64;
}

static const SDL_StorageInterface GENERIC_title_iface = {
    GENERIC_CloseStorage,
    NULL,   /* ready */
    GENERIC_EnumerateStorageDirectory,
    GENERIC_GetStoragePathInfo,
    GENERIC_ReadStorageFile,
    NULL,   /* write_file */
    NULL,   /* mkdir */
    NULL,   /* remove */
    NULL,   /* rename */
    NULL    /* space_remaining */
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
    GENERIC_CloseStorage,
    NULL,   /* ready */
    GENERIC_EnumerateStorageDirectory,
    GENERIC_GetStoragePathInfo,
    GENERIC_ReadStorageFile,
    GENERIC_WriteStorageFile,
    GENERIC_CreateStorageDirectory,
    GENERIC_RemoveStoragePath,
    GENERIC_RenameStoragePath,
    GENERIC_GetStorageSpaceRemaining
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
    GENERIC_CloseStorage,
    NULL,   /* ready */
    GENERIC_EnumerateStorageDirectory,
    GENERIC_GetStoragePathInfo,
    GENERIC_ReadStorageFile,
    GENERIC_WriteStorageFile,
    GENERIC_CreateStorageDirectory,
    GENERIC_RemoveStoragePath,
    GENERIC_RenameStoragePath,
    GENERIC_GetStorageSpaceRemaining
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
