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
#include "SDL_hints_c.h"
#include "SDL_properties_c.h"


typedef struct
{
    SDL_PropertyType type;

    union {
        void *pointer_value;
        char *string_value;
        Sint64 number_value;
        float float_value;
        SDL_bool boolean_value;
    } value;

    char *string_storage;

    void (SDLCALL *cleanup)(void *userdata, void *value);
    void *userdata;
} SDL_Property;

typedef struct
{
    SDL_HashTable *props;
    SDL_Mutex *lock;
} SDL_Properties;

static SDL_HashTable *SDL_properties;
static SDL_Mutex *SDL_properties_lock;
static SDL_PropertiesID SDL_last_properties_id;
static SDL_PropertiesID SDL_global_properties;


static void SDL_FreePropertyWithCleanup(const void *key, const void *value, void *data, SDL_bool cleanup)
{
    SDL_Property *property = (SDL_Property *)value;
    if (property) {
        switch (property->type) {
        case SDL_PROPERTY_TYPE_POINTER:
            if (property->cleanup && cleanup) {
                property->cleanup(property->userdata, property->value.pointer_value);
            }
            break;
        case SDL_PROPERTY_TYPE_STRING:
            SDL_free(property->value.string_value);
            break;
        default:
            break;
        }
        if (property->string_storage) {
            SDL_free(property->string_storage);
        }
    }
    SDL_free((void *)key);
    SDL_free((void *)value);
}

static void SDL_FreeProperty(const void *key, const void *value, void *data)
{
    SDL_FreePropertyWithCleanup(key, value, data, SDL_TRUE);
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
        SDL_properties_lock = SDL_CreateMutex();
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
    if (SDL_global_properties) {
        SDL_DestroyProperties(SDL_global_properties);
        SDL_global_properties = 0;
    }
    if (SDL_properties) {
        SDL_DestroyHashTable(SDL_properties);
        SDL_properties = NULL;
    }
    if (SDL_properties_lock) {
        SDL_DestroyMutex(SDL_properties_lock);
        SDL_properties_lock = NULL;
    }
}

SDL_PropertiesID SDL_GetGlobalProperties(void)
{
    if (!SDL_global_properties) {
        SDL_global_properties = SDL_CreateProperties();
    }
    return SDL_global_properties;
}

SDL_PropertiesID SDL_CreateProperties(void)
{
    SDL_PropertiesID props = 0;
    SDL_Properties *properties = NULL;
    SDL_bool inserted = SDL_FALSE;

    if (!SDL_properties && SDL_InitProperties() < 0) {
        return 0;
    }

    properties = SDL_calloc(1, sizeof(*properties));
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

    SDL_LockMutex(SDL_properties_lock);
    ++SDL_last_properties_id;
    if (SDL_last_properties_id == 0) {
        ++SDL_last_properties_id;
    }
    props = SDL_last_properties_id;
    if (SDL_InsertIntoHashTable(SDL_properties, (const void *)(uintptr_t)props, properties)) {
        inserted = SDL_TRUE;
    }
    SDL_UnlockMutex(SDL_properties_lock);

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

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

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

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        return;
    }

    SDL_UnlockMutex(properties->lock);
}

static int SDL_PrivateSetProperty(SDL_PropertiesID props, const char *name, SDL_Property *property)
{
    SDL_Properties *properties = NULL;
    int result = 0;

    if (!props) {
        SDL_FreePropertyWithCleanup(NULL, property, NULL, SDL_FALSE);
        return SDL_InvalidParamError("props");
    }
    if (!name || !*name) {
        SDL_FreePropertyWithCleanup(NULL, property, NULL, SDL_FALSE);
        return SDL_InvalidParamError("name");
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_FreePropertyWithCleanup(NULL, property, NULL, SDL_FALSE);
        return SDL_InvalidParamError("props");
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_RemoveFromHashTable(properties->props, name);
        if (property) {
            char *key = SDL_strdup(name);
            if (!SDL_InsertIntoHashTable(properties->props, key, property)) {
                SDL_FreePropertyWithCleanup(key, property, NULL, SDL_FALSE);
                result = -1;
            }
        }
    }
    SDL_UnlockMutex(properties->lock);

    return result;
}

int SDL_SetPropertyWithCleanup(SDL_PropertiesID props, const char *name, void *value, void (SDLCALL *cleanup)(void *userdata, void *value), void *userdata)
{
    SDL_Property *property;

    if (!value) {
        return SDL_ClearProperty(props, name);
    }

    property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_POINTER;
    property->value.pointer_value = value;
    property->cleanup = cleanup;
    property->userdata = userdata;
    return SDL_PrivateSetProperty(props, name, property);
}

int SDL_SetProperty(SDL_PropertiesID props, const char *name, void *value)
{
    SDL_Property *property;

    if (!value) {
        return SDL_ClearProperty(props, name);
    }

    property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_POINTER;
    property->value.pointer_value = value;
    return SDL_PrivateSetProperty(props, name, property);
}


int SDL_SetStringProperty(SDL_PropertiesID props, const char *name, const char *value)
{
    SDL_Property *property;

    if (!value) {
        return SDL_ClearProperty(props, name);
    }

    property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_STRING;
    property->value.string_value = SDL_strdup(value);
    if (!property->value.string_value) {
        SDL_free(property);
        return -1;
    }
    return SDL_PrivateSetProperty(props, name, property);
}

int SDL_SetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 value)
{
    SDL_Property *property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_NUMBER;
    property->value.number_value = value;
    return SDL_PrivateSetProperty(props, name, property);
}

int SDL_SetFloatProperty(SDL_PropertiesID props, const char *name, float value)
{
    SDL_Property *property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_FLOAT;
    property->value.float_value = value;
    return SDL_PrivateSetProperty(props, name, property);
}

int SDL_SetBooleanProperty(SDL_PropertiesID props, const char *name, SDL_bool value)
{
    SDL_Property *property = (SDL_Property *)SDL_calloc(1, sizeof(*property));
    if (!property) {
        return -1;
    }
    property->type = SDL_PROPERTY_TYPE_BOOLEAN;
    property->value.boolean_value = value ? SDL_TRUE : SDL_FALSE;
    return SDL_PrivateSetProperty(props, name, property);
}

SDL_PropertyType SDL_GetPropertyType(SDL_PropertiesID props, const char *name)
{
    SDL_Properties *properties = NULL;
    SDL_PropertyType type = SDL_PROPERTY_TYPE_INVALID;

    if (!props) {
        SDL_InvalidParamError("props");
        return SDL_PROPERTY_TYPE_INVALID;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return SDL_PROPERTY_TYPE_INVALID;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return SDL_PROPERTY_TYPE_INVALID;
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            type = property->type;
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return type;
}

void *SDL_GetProperty(SDL_PropertiesID props, const char *name, void *default_value)
{
    SDL_Properties *properties = NULL;
    void *value = default_value;

    if (!props) {
        SDL_InvalidParamError("props");
        return value;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return value;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return value;
    }

    /* Note that taking the lock here only guarantees that we won't read the
     * hashtable while it's being modified. The value itself can easily be
     * freed from another thread after it is returned here.
     */
    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            if (property->type == SDL_PROPERTY_TYPE_POINTER) {
                value = property->value.pointer_value;
            } else {
                SDL_SetError("Property %s isn't a pointer value", name);
            }
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

const char *SDL_GetStringProperty(SDL_PropertiesID props, const char *name, const char *default_value)
{
    SDL_Properties *properties = NULL;
    const char *value = default_value;

    if (!props) {
        SDL_InvalidParamError("props");
        return value;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return value;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return value;
    }

    /* Note that taking the lock here only guarantees that we won't read the
     * hashtable while it's being modified. The value itself can easily be
     * freed from another thread after it is returned here.
     *
     * FIXME: Should we SDL_strdup() the return value to avoid this?
     */
    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            switch (property->type) {
            case SDL_PROPERTY_TYPE_STRING:
                value = property->value.string_value;
                break;
            case SDL_PROPERTY_TYPE_NUMBER:
                if (property->string_storage) {
                    value = property->string_storage;
                } else {
                    SDL_asprintf(&property->string_storage, "%" SDL_PRIs64 "", property->value.number_value);
                    if (property->string_storage) {
                        value = property->string_storage;
                    }
                }
                break;
            case SDL_PROPERTY_TYPE_FLOAT:
                if (property->string_storage) {
                    value = property->string_storage;
                } else {
                    SDL_asprintf(&property->string_storage, "%f", property->value.float_value);
                    if (property->string_storage) {
                        value = property->string_storage;
                    }
                }
                break;
            case SDL_PROPERTY_TYPE_BOOLEAN:
                value = property->value.boolean_value ? "true" : "false";
                break;
            default:
                SDL_SetError("Property %s isn't a string value", name);
                break;
            }
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

Sint64 SDL_GetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 default_value)
{
    SDL_Properties *properties = NULL;
    Sint64 value = default_value;

    if (!props) {
        SDL_InvalidParamError("props");
        return value;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return value;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return value;
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            switch (property->type) {
            case SDL_PROPERTY_TYPE_STRING:
                value = SDL_strtoll(property->value.string_value, NULL, 0);
                break;
            case SDL_PROPERTY_TYPE_NUMBER:
                value = property->value.number_value;
                break;
            case SDL_PROPERTY_TYPE_FLOAT:
                value = (Sint64)SDL_round((double)property->value.float_value);
                break;
            case SDL_PROPERTY_TYPE_BOOLEAN:
                value = property->value.boolean_value;
                break;
            default:
                SDL_SetError("Property %s isn't a number value", name);
                break;
            }
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

float SDL_GetFloatProperty(SDL_PropertiesID props, const char *name, float default_value)
{
    SDL_Properties *properties = NULL;
    float value = default_value;

    if (!props) {
        SDL_InvalidParamError("props");
        return value;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return value;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return value;
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            switch (property->type) {
            case SDL_PROPERTY_TYPE_STRING:
                value = (float)SDL_atof(property->value.string_value);
                break;
            case SDL_PROPERTY_TYPE_NUMBER:
                value = (float)property->value.number_value;
                break;
            case SDL_PROPERTY_TYPE_FLOAT:
                value = property->value.float_value;
                break;
            case SDL_PROPERTY_TYPE_BOOLEAN:
                value = (float)property->value.boolean_value;
                break;
            default:
                SDL_SetError("Property %s isn't a float value", name);
                break;
            }
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

SDL_bool SDL_GetBooleanProperty(SDL_PropertiesID props, const char *name, SDL_bool default_value)
{
    SDL_Properties *properties = NULL;
    SDL_bool value = default_value;

    if (!props) {
        SDL_InvalidParamError("props");
        return value;
    }
    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return value;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        SDL_InvalidParamError("props");
        return value;
    }

    SDL_LockMutex(properties->lock);
    {
        SDL_Property *property = NULL;
        if (SDL_FindInHashTable(properties->props, name, (const void **)&property)) {
            switch (property->type) {
            case SDL_PROPERTY_TYPE_STRING:
                value = SDL_GetStringBoolean(property->value.string_value, default_value);
                break;
            case SDL_PROPERTY_TYPE_NUMBER:
                value = (property->value.number_value != 0);
                break;
            case SDL_PROPERTY_TYPE_FLOAT:
                value = (property->value.float_value != 0.0f);
                break;
            case SDL_PROPERTY_TYPE_BOOLEAN:
                value = property->value.boolean_value;
                break;
            default:
                SDL_SetError("Property %s isn't a boolean value", name);
                break;
            }
        } else {
            SDL_SetError("Couldn't find property named %s", name);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return value;
}

int SDL_ClearProperty(SDL_PropertiesID props, const char *name)
{
    return SDL_PrivateSetProperty(props, name, NULL);
}

int SDL_EnumerateProperties(SDL_PropertiesID props, SDL_EnumeratePropertiesCallback callback, void *userdata)
{
    SDL_Properties *properties = NULL;

    if (!props) {
        return SDL_InvalidParamError("props");
    }
    if (!callback) {
        return SDL_InvalidParamError("callback");
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_FindInHashTable(SDL_properties, (const void *)(uintptr_t)props, (const void **)&properties);
    SDL_UnlockMutex(SDL_properties_lock);

    if (!properties) {
        return SDL_InvalidParamError("props");
    }

    SDL_LockMutex(properties->lock);
    {
        void *iter;
        const void *key, *value;

        iter = NULL;
        while (SDL_IterateHashTable(properties->props, &key, &value, &iter)) {
            callback(userdata, props, (const char *)key);
        }
    }
    SDL_UnlockMutex(properties->lock);

    return 0;
}

void SDL_DestroyProperties(SDL_PropertiesID props)
{
    if (!props) {
        return;
    }

    SDL_LockMutex(SDL_properties_lock);
    SDL_RemoveFromHashTable(SDL_properties, (const void *)(uintptr_t)props);
    SDL_UnlockMutex(SDL_properties_lock);
}
