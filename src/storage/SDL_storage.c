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

#include "SDL_sysstorage.h"

/* Available title storage drivers */
static TitleStorageBootStrap *titlebootstrap[] = {
    &GENERIC_titlebootstrap,
};

/* Available user storage drivers */
static UserStorageBootStrap *userbootstrap[] = {
#ifdef SDL_STORAGE_STEAM
    &STEAM_userbootstrap,
#endif
    &GENERIC_userbootstrap,
};

struct SDL_Storage
{
    SDL_StorageInterface iface;
    void *userdata;
};

#define CHECK_STORAGE_MAGIC()                             \
    if (!storage) {                                       \
        return SDL_SetError("Invalid storage container"); \
    }

#define CHECK_STORAGE_MAGIC_RET(retval)            \
    if (!storage) {                                \
        SDL_SetError("Invalid storage container"); \
        return retval;                             \
    }

SDL_Storage *SDL_OpenTitleStorage(const char *override, SDL_PropertiesID props)
{
    SDL_Storage *storage = NULL;
    int i = 0;

    /* Select the proper storage driver */
    const char *driver_name = SDL_GetHint(SDL_HINT_STORAGE_TITLE_DRIVER);
    if (driver_name && *driver_name != 0) {
        const char *driver_attempt = driver_name;
        while (driver_attempt && *driver_attempt != 0 && !storage) {
            const char *driver_attempt_end = SDL_strchr(driver_attempt, ',');
            size_t driver_attempt_len = (driver_attempt_end) ? (driver_attempt_end - driver_attempt)
                                                                     : SDL_strlen(driver_attempt);

            for (i = 0; titlebootstrap[i]; ++i) {
                if ((driver_attempt_len == SDL_strlen(titlebootstrap[i]->name)) &&
                    (SDL_strncasecmp(titlebootstrap[i]->name, driver_attempt, driver_attempt_len) == 0)) {
                    storage = titlebootstrap[i]->create(override, props);
                    break;
                }
            }

            driver_attempt = (driver_attempt_end) ? (driver_attempt_end + 1) : NULL;
        }
    } else {
        for (i = 0; titlebootstrap[i]; ++i) {
            storage = titlebootstrap[i]->create(override, props);
            if (storage) {
                break;
            }
        }
    }
    if (!storage) {
        if (driver_name) {
            SDL_SetError("%s not available", driver_name);
        } else {
            SDL_SetError("No available title storage driver");
        }
    }
    return storage;
}

SDL_Storage *SDL_OpenUserStorage(const char *org, const char *app, SDL_PropertiesID props)
{
    SDL_Storage *storage = NULL;
    int i = 0;

    /* Select the proper storage driver */
    const char *driver_name = SDL_GetHint(SDL_HINT_STORAGE_USER_DRIVER);
    if (driver_name && *driver_name != 0) {
        const char *driver_attempt = driver_name;
        while (driver_attempt && *driver_attempt != 0 && !storage) {
            const char *driver_attempt_end = SDL_strchr(driver_attempt, ',');
            size_t driver_attempt_len = (driver_attempt_end) ? (driver_attempt_end - driver_attempt)
                                                                     : SDL_strlen(driver_attempt);

            for (i = 0; userbootstrap[i]; ++i) {
                if ((driver_attempt_len == SDL_strlen(userbootstrap[i]->name)) &&
                    (SDL_strncasecmp(userbootstrap[i]->name, driver_attempt, driver_attempt_len) == 0)) {
                    storage = userbootstrap[i]->create(org, app, props);
                    break;
                }
            }

            driver_attempt = (driver_attempt_end) ? (driver_attempt_end + 1) : NULL;
        }
    } else {
        for (i = 0; userbootstrap[i]; ++i) {
            storage = userbootstrap[i]->create(org, app, props);
            if (storage) {
                break;
            }
        }
    }
    if (!storage) {
        if (driver_name) {
            SDL_SetError("%s not available", driver_name);
        } else {
            SDL_SetError("No available user storage driver");
        }
    }
    return storage;
}

SDL_Storage *SDL_OpenFileStorage(const char *path)
{
    return GENERIC_OpenFileStorage(path);
}

SDL_Storage *SDL_OpenStorage(const SDL_StorageInterface *iface, void *userdata)
{
    SDL_Storage *storage;

    if (!iface) {
        SDL_InvalidParamError("iface");
        return NULL;
    }

    storage = (SDL_Storage *)SDL_calloc(1, sizeof(*storage));
    if (storage) {
        SDL_copyp(&storage->iface, iface);
        storage->userdata = userdata;
    }
    return storage;
}

int SDL_CloseStorage(SDL_Storage *storage)
{
    int retval = 0;

    CHECK_STORAGE_MAGIC()

    if (storage->iface.close) {
        retval = storage->iface.close(storage->userdata);
    }
    SDL_free(storage);
    return retval;
}

SDL_bool SDL_StorageReady(SDL_Storage *storage)
{
    CHECK_STORAGE_MAGIC_RET(SDL_FALSE)

    if (storage->iface.ready) {
        return storage->iface.ready(storage->userdata);
    }
    return SDL_TRUE;
}

int SDL_GetStorageFileSize(SDL_Storage *storage, const char *path, Uint64 *length)
{
    SDL_PathInfo info;

    if (SDL_GetStoragePathInfo(storage, path, &info) < 0) {
        return -1;
    }
    if (length) {
        *length = info.size;
    }
    return 0;
}

int SDL_ReadStorageFile(SDL_Storage *storage, const char *path, void *destination, Uint64 length)
{
    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.read_file) {
        return SDL_Unsupported();
    }

    return storage->iface.read_file(storage->userdata, path, destination, length);
}

int SDL_WriteStorageFile(SDL_Storage *storage, const char *path, const void *source, Uint64 length)
{
    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.write_file) {
        return SDL_Unsupported();
    }

    return storage->iface.write_file(storage->userdata, path, source, length);
}

int SDL_CreateStorageDirectory(SDL_Storage *storage, const char *path)
{
    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.mkdir) {
        return SDL_Unsupported();
    }

    return storage->iface.mkdir(storage->userdata, path);
}

int SDL_EnumerateStorageDirectory(SDL_Storage *storage, const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata)
{
    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.enumerate) {
        return SDL_Unsupported();
    }

    return storage->iface.enumerate(storage->userdata, path, callback, userdata);
}

int SDL_RemoveStoragePath(SDL_Storage *storage, const char *path)
{
    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.remove) {
        return SDL_Unsupported();
    }

    return storage->iface.remove(storage->userdata, path);
}

int SDL_RenameStoragePath(SDL_Storage *storage, const char *oldpath, const char *newpath)
{
    CHECK_STORAGE_MAGIC()

    if (!oldpath) {
        return SDL_InvalidParamError("oldpath");
    }
    if (!newpath) {
        return SDL_InvalidParamError("newpath");
    }

    if (!storage->iface.rename) {
        return SDL_Unsupported();
    }

    return storage->iface.rename(storage->userdata, oldpath, newpath);
}

int SDL_GetStoragePathInfo(SDL_Storage *storage, const char *path, SDL_PathInfo *info)
{
    SDL_PathInfo dummy;

    if (!info) {
        info = &dummy;
    }
    SDL_zerop(info);

    CHECK_STORAGE_MAGIC()

    if (!path) {
        return SDL_InvalidParamError("path");
    }

    if (!storage->iface.info) {
        return SDL_Unsupported();
    }

    return storage->iface.info(storage->userdata, path, info);
}

Uint64 SDL_GetStorageSpaceRemaining(SDL_Storage *storage)
{
    CHECK_STORAGE_MAGIC_RET(0)

    if (!storage->iface.space_remaining) {
        SDL_Unsupported();
        return 0;
    }

    return storage->iface.space_remaining(storage->userdata);
}
