/*
  Simple DiretMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_hashtable.h"
#include "SDL_properties_c.h"


typedef struct
{
    void *value;
    void (SDLCALL *cleanup)(void *userdata, void *value);
    void *userdata;
} SDL_Property;

typedef struct
{
    SDL_HashTable *props;
    SDL_Mutex *lock;
} SDL_Properties;

static SDL_HashTable *SDL_properties;
static SDL_RWLock *SDL_properties_lock;
static SDL_PropertiesID SDL_last_properties_id;


static void SDL_FreeProperty(const void *key, const void *value, void *data)
{
    SDL_Property *property = (SDL_Property *)value;
    if (property->cleanup) {
        property->cleanup(property->userdata, property->value);
    }
    SDL_free((void *)key);
    SDL_free((void *)value);
}

static void SDL_FreeProperties(const void *key, const void *value, void *data)
{
    SDL_Properties *properties = (SDL_Properties *)value;
    if (properties) {
        if (properties->props) {
            SDL_DestroyHashTable(properties->props);
            properties->props = NULL;
        }
        if (properties->lock) {
            SDL_DestroyMutex(properties->lock);
            properties->lock = NULL;
        }
        SDL_free(properties);
    }
}

int SDL_InitProperties(void)
{
    if (!SDL_properties_lock) {
        SDL_properties_lock = SDL_CreateRWLock();
        if (!SDL_properties_lock) {
            return -1;
        }
    }
    if (!SDL_properties) {
        SDL_properties = SDL_CreateHashTable(NULL, 16, SDL_HashID, SDL_KeyMatchID, SDL_FreeProperties, SDL_FALSE);
        if (!SDL_properties) {
            return -1;
        }
    }
    return 0;
}

void SDL_QuitProperties(void)
{
    if (SDL_properties) {
        SDL_DestroyHashTable(SDL_properties);
        SDL_properties = NULL;
    }
    if (SDL_properties_lock) {
        SDL_DestroyRWLock(SDL_properties_lock);
        SDL_properties_lock = NULL;
    }
}

SDL_PropertiesID SDL_CreateProperties(void)
{
    SDL_PropertiesID props = 0;
    SDL_Properties *properties = NULL;
    SDL_bool inserted = SDL_FALSE;

    if (!SDL_properties && SDL_InitProperties() < 0) {
        return 0;
    }

    properties = SDL_malloc(sizeof(*properties));
    if (!properties) {
        goto error;
    }
    properties->props = SDL_CreateHashTable(NULL, 4, SDL_HashString, SDL_KeyMatchString, SDL_FreeProperty, SDL_FALSE);
    if (!properties->props) {
        goto error;
    }
    properties->lock = SDL_CreateMutex();
    if (!properties->lock) {
        goto error;
    }

    if (SDL_InitProperties() < 0) {
        goto error;
    }

    SDL_LockRWLockForWriting(SDL_properties_lock);
    ++SDL_last_properties_id;
    if (SDL_last_properties_id == 0) {
        ++SDL_last_properties_id;
    }
    props = SDL_last_properties_id;
    if (SDL_InsertIntoHashTable(SDL_properties, (const void *)(uintptr_t)props, properties)) {
        inserted = SDL_TRUE;
    }
    SDL_UnlockRWLock(SDL_properties_lock);

    if (inserted) {
        /* All done! */
        return props;
    }

error:
    SDL_FreeProperties(NULL, properties, NULL);
    return 0;
}

int SDL_LockProperties(SDL_PropertiesID props)
{
    SDL_Properties *properties = NULL;

    if (!props) {
        return SDL_InvalidParamError("props");
    }

    SDL_LockRWLockForReading(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockRWLock(SDL_properties_lock);

    if (!properties) {
        return SDL_InvalidParamError("props");
    }

    SDL_LockMutex(properties->lock);
    return 0;
}

void SDL_UnlockProperties(SDL_PropertiesID props)
{
    SDL_Properties *properties = NULL;

    if (!props) {
        return;
    }

    SDL_LockRWLockForReading(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockRWLock(SDL_properties_lock);

    if (!properties) {
        return;
    }

    SDL_UnlockMutex(properties->lock);
}

int SDL_SetProperty(SDL_PropertiesID props, const char *name, void *value, void (SDLCALL *cleanup)(void *userdata, void *value), void *userdata)
{
    SDL_Properties *properties = NULL;
    SDL_Property *property = NULL;
    int result = 0;

    if (!props) {
        return SDL_InvalidParamError("props");
    }
    if (!name || !*name) {
        return SDL_InvalidParamError("name");
    }

    SDL_LockRWLockForReading(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockRWLock(SDL_properties_lock);

    if (!properties) {
        return SDL_InvalidParamError("props");
    }

    if (value) {
        property = (SDL_Property *)SDL_malloc(sizeof(*property));
        if (!property) {
            return SDL_OutOfMemory();
        }

        property->value = value;
        property->cleanup = cleanup;
        property->userdata = userdata;
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_RemoveFromHashTable(properties->props, name);
        if (property) {
            char *key = SDL_strdup(name);
            if (!SDL_InsertIntoHashTable(properties->props, key, property)) {
                SDL_FreeProperty(key, property, NULL);
                result = -1;
            }
        }
    }
    SDL_UnlockMutex(properties->lock);

    return result;
}

void *SDL_GetProperty(SDL_PropertiesID props, const char *name)
{
    SDL_Properties *properties = NULL;
    void *value = NULL;

    if (!props) {
        SDL_InvalidParamError("props");
        return NULL;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return NULL;
    }

    SDL_LockRWLockForReading(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockRWLock(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return NULL;
    }

    /* Note that taking the lock here only guarantees that we won't read the
     * hashtable while it's being modified. The value itself can easily be
     * freed from another thread after it is returned here.
     */
    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            value = property->value;
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

int SDL_ClearProperty(SDL_PropertiesID props, const char *name)
{
    return SDL_SetProperty(props, name, NULL, NULL, NULL);
}

void SDL_DestroyProperties(SDL_PropertiesID props)
{
    if (!props) {
        return;
    }

    SDL_LockRWLockForWriting(SDL_properties_lock);
    SDL_RemoveFromHashTable(SDL_properties, (const void *)(uintptr_t)props);
    SDL_UnlockRWLock(SDL_properties_lock);
}
