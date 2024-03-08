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

SDL_Storage *SDL_OpenStorage(const SDL_StorageInterface *iface, void *userdata)
{
    SDL_Storage *storage;

    if (iface->close == NULL || iface->ready == NULL || iface->fileSize == NULL) {
        SDL_SetError("iface is missing required callbacks");
        return NULL;
    }

    if ((iface->writeFile != NULL) != (iface->spaceRemaining != NULL)) {
        SDL_SetError("Writeable containers must have both writeFile and spaceRemaining");
        return NULL;
    }

    storage = (SDL_Storage*) SDL_malloc(sizeof(SDL_Storage));
    if (storage == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_memcpy(&storage->iface, iface, sizeof(SDL_StorageInterface));
    storage->userdata = userdata;
    return storage;
}

int SDL_CloseStorage(SDL_Storage *storage)
{
    int retval;

    CHECK_STORAGE_MAGIC()

    retval = storage->iface.close(storage->userdata);
    SDL_free(storage);
    return retval;
}

SDL_bool SDL_StorageReady(SDL_Storage *storage)
{
    CHECK_STORAGE_MAGIC_RET(SDL_FALSE)

    return storage->iface.ready(storage->userdata);
}

int SDL_StorageFileSize(SDL_Storage *storage, const char *path, Uint64 *length)
{
    CHECK_STORAGE_MAGIC()

    return storage->iface.fileSize(storage->userdata, path, length);
}

int SDL_StorageReadFile(SDL_Storage *storage, const char *path, void *destination, Uint64 length)
{
    CHECK_STORAGE_MAGIC()

    if (storage->iface.readFile == NULL) {
        return SDL_SetError("Storage container does not have read capability");
    }

    return storage->iface.readFile(storage->userdata, path, destination, length);
}

int SDL_StorageWriteFile(SDL_Storage *storage, const char *path, const void *source, Uint64 length)
{
    CHECK_STORAGE_MAGIC()

    if (storage->iface.writeFile == NULL) {
        return SDL_SetError("Storage container does not have write capability");
    }

    return storage->iface.writeFile(storage->userdata, path, source, length);
}

Uint64 SDL_StorageSpaceRemaining(SDL_Storage *storage)
{
    CHECK_STORAGE_MAGIC_RET(0)

    if (storage->iface.spaceRemaining == NULL) {
        SDL_SetError("Storage container does not have write capability");
        return 0;
    }

    return storage->iface.spaceRemaining(storage->userdata);
}
