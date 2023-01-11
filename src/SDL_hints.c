/*
  Simple DirectMedia Layer
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

#include "SDL_hints_c.h"

/* Assuming there aren't many hints set and they aren't being queried in
   critical performance paths, we'll just use linked lists here.
   !!! FIXME: there are like 400+ places we lookup hints in SDL...maybe
   !!! FIXME: a small hashtable isn't the worst idea. When the GPU API
   !!! FIXME: branch merges, there will be an SDL_HashTable we can
   !!! FIXME: plug in here.
 */
typedef struct SDL_HintWatch
{
    SDL_HintCallback callback;
    void *userdata;
    struct SDL_HintWatch *next;
} SDL_HintWatch;

typedef struct SDL_Hint
{
    char *name;
    char *value;
    char *default_value;
    SDL_HintPriority priority;
    SDL_HintWatch *callbacks;
    struct SDL_Hint *next;
} SDL_Hint;

static SDL_Hint *SDL_hints;

static SDL_bool
SDL_SetHintWithPriorityAndDefault(const char *name, const char *value,
                                  SDL_HintPriority priority, SDL_bool isdeflt)
{
    const char *env;
    SDL_Hint *hint;
    SDL_HintWatch *entry;

    if (name == NULL) {
        SDL_InvalidParamError("name");
        return SDL_FALSE;
    }

    env = SDL_getenv(name);
    if (!isdeflt && env && priority < SDL_HINT_OVERRIDE) {  /* early out */
        SDL_SetError("higher priority hint already in place");
        return SDL_FALSE;
    }

    for (hint = SDL_hints; hint; hint = hint->next) {
        if (SDL_strcmp(name, hint->name) == 0) {
            if (isdeflt) {   /* make sure this gets set, in case we made this variable without a default originally. */
                char *newval = NULL;
                if (value) {
                    newval = SDL_strdup(value);
                    if (newval == NULL) {
                        SDL_OutOfMemory();
                        return SDL_FALSE;
                    }
                }
                SDL_free(hint->default_value);  /* you probably shouldn't double-set a default, though! */
                hint->default_value = newval;
            }

            if (env && priority < SDL_HINT_OVERRIDE) {
                SDL_SetError("higher priority hint already in place");
                return SDL_FALSE;
            } else if (priority < hint->priority) {
                SDL_SetError("higher priority hint already in place");
                return SDL_FALSE;
            }
            if (hint->value != value &&
                (value == NULL || !hint->value || SDL_strcmp(hint->value, value) != 0)) {
                for (entry = hint->callbacks; entry;) {
                    /* Save the next entry in case this one is deleted */
                    SDL_HintWatch *next = entry->next;
                    entry->callback(entry->userdata, name, hint->value, value);
                    entry = next;
                }
                if (hint->value != hint->default_value) {
                    SDL_free(hint->value);
                }
                hint->value = value ? SDL_strdup(value) : hint->default_value;
            }
            hint->priority = priority;
            return SDL_TRUE;
        }
    }

    /* Couldn't find the hint, add a new one */
    hint = (SDL_Hint *)SDL_malloc(sizeof(*hint));
    if (hint == NULL) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    hint->name = SDL_strdup(name);
    if (hint->name == NULL) {
        SDL_free(hint);
        SDL_OutOfMemory();
        return SDL_FALSE;
    }

    hint->value = NULL;
    if (value != NULL) {
        hint->value = SDL_strdup(value);
        if (hint->value == NULL) {
            SDL_free(hint->name);
            SDL_free(hint);
            SDL_OutOfMemory();
            return SDL_FALSE;
        }
    }

    hint->default_value = isdeflt ? hint->value : NULL;
    hint->priority = priority;
    hint->callbacks = NULL;
    hint->next = SDL_hints;
    SDL_hints = hint;
    return SDL_TRUE;
}

SDL_bool
SDL_SetHintWithPriority(const char *name, const char *value,
                        SDL_HintPriority priority)
{
    return SDL_SetHintWithPriorityAndDefault(name, value, priority, SDL_FALSE);
}

SDL_bool
SDL_ResetHint(const char *name)
{
    const char *origenv;
    SDL_Hint *hint;
    SDL_HintWatch *entry;

    if (name == NULL) {
        return SDL_FALSE;
    }

    origenv = SDL_getenv(name);

    for (hint = SDL_hints; hint; hint = hint->next) {
        if (SDL_strcmp(name, hint->name) == 0) {
            const char *env = origenv ? origenv : hint->default_value;
            if ((env == NULL && hint->value != NULL) ||
                (env != NULL && hint->value == NULL) ||
                (env != NULL && SDL_strcmp(env, hint->value) != 0)) {
                for (entry = hint->callbacks; entry;) {
                    /* Save the next entry in case this one is deleted */
                    SDL_HintWatch *next = entry->next;
                    entry->callback(entry->userdata, name, hint->value, env);
                    entry = next;
                }
            }
            if (hint->value != hint->default_value) {
                SDL_free(hint->value);
                hint->value = hint->default_value;
            }
            hint->priority = SDL_HINT_DEFAULT;
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

void SDL_ResetHints(void)
{
    const char *env;
    SDL_Hint *hint;
    SDL_HintWatch *entry;

    for (hint = SDL_hints; hint; hint = hint->next) {
        env = SDL_getenv(hint->name);
        if (!env) {
            env = hint->default_value;
        }

        if ((env == NULL && hint->value != NULL) ||
            (env != NULL && hint->value == NULL) ||
            (env != NULL && SDL_strcmp(env, hint->value) != 0)) {
            for (entry = hint->callbacks; entry;) {
                /* Save the next entry in case this one is deleted */
                SDL_HintWatch *next = entry->next;
                entry->callback(entry->userdata, hint->name, hint->value, env);
                entry = next;
            }
        }
        if (hint->value != hint->default_value) {
            SDL_free(hint->value);
            hint->value = hint->default_value;
        }
        hint->priority = SDL_HINT_DEFAULT;
    }
}

SDL_bool
SDL_SetHint(const char *name, const char *value)
{
    return SDL_SetHintWithPriority(name, value, SDL_HINT_NORMAL);
}

const char *
SDL_GetHint(const char *name)
{
    const char *env;
    SDL_Hint *hint;

    env = SDL_getenv(name);
    for (hint = SDL_hints; hint; hint = hint->next) {
        if (SDL_strcmp(name, hint->name) == 0) {
            if (env == NULL || hint->priority == SDL_HINT_OVERRIDE) {
                return hint->value;
            }
            break;
        }
    }
    return env;
}

SDL_bool
SDL_GetStringBoolean(const char *value, SDL_bool default_value)
{
    if (value == NULL || !*value) {
        return default_value;
    }
    if (*value == '0' || SDL_strcasecmp(value, "false") == 0) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

SDL_bool
SDL_GetHintBoolean(const char *name, SDL_bool default_value)
{
    const char *hint = SDL_GetHint(name);
    return SDL_GetStringBoolean(hint, default_value);
}

int SDL_AddHintCallback(const char *name, SDL_HintCallback callback, void *userdata)
{
    SDL_Hint *hint;
    SDL_HintWatch *entry;
    const char *value;

    if (name == NULL || !*name) {
        return SDL_InvalidParamError("name");
    }
    if (!callback) {
        return SDL_InvalidParamError("callback");
    }

    SDL_DelHintCallback(name, callback, userdata);

    entry = (SDL_HintWatch *)SDL_malloc(sizeof(*entry));
    if (entry == NULL) {
        return SDL_OutOfMemory();
    }
    entry->callback = callback;
    entry->userdata = userdata;

    for (hint = SDL_hints; hint; hint = hint->next) {
        if (SDL_strcmp(name, hint->name) == 0) {
            break;
        }
    }
    if (hint == NULL) {
        /* Need to add a hint entry for this watcher */
        hint = (SDL_Hint *)SDL_malloc(sizeof(*hint));
        if (hint == NULL) {
            SDL_free(entry);
            return SDL_OutOfMemory();
        }
        hint->name = SDL_strdup(name);
        if (!hint->name) {
            SDL_free(entry);
            SDL_free(hint);
            return SDL_OutOfMemory();
        }
        hint->default_value = NULL;
        hint->value = NULL;
        hint->priority = SDL_HINT_DEFAULT;
        hint->callbacks = NULL;
        hint->next = SDL_hints;
        SDL_hints = hint;
    }

    /* Add it to the callbacks for this hint */
    entry->next = hint->callbacks;
    hint->callbacks = entry;

    /* Now call it with the current value */
    value = SDL_GetHint(name);
    callback(userdata, name, value, value);
    return 0;
}

void SDL_DelHintCallback(const char *name, SDL_HintCallback callback, void *userdata)
{
    SDL_Hint *hint;
    SDL_HintWatch *entry, *prev;

    for (hint = SDL_hints; hint; hint = hint->next) {
        if (SDL_strcmp(name, hint->name) == 0) {
            prev = NULL;
            for (entry = hint->callbacks; entry; entry = entry->next) {
                if (callback == entry->callback && userdata == entry->userdata) {
                    if (prev) {
                        prev->next = entry->next;
                    } else {
                        hint->callbacks = entry->next;
                    }
                    SDL_free(entry);
                    break;
                }
                prev = entry;
            }
            return;
        }
    }
}

void SDL_ClearHints(void)
{
    SDL_Hint *hint;
    SDL_HintWatch *entry;

    while (SDL_hints) {
        hint = SDL_hints;
        SDL_hints = hint->next;

        SDL_free(hint->name);
        SDL_free(hint->value);

        if (hint->value != hint->default_value) {
            SDL_free(hint->default_value);
        }

        for (entry = hint->callbacks; entry;) {
            SDL_HintWatch *freeable = entry;
            entry = entry->next;
            SDL_free(freeable);
        }
        SDL_free(hint);
    }
}


static void SDLCALL UpdateRegisteredIntHintCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    *((int *) userdata) = newValue ? SDL_atoi(newValue) : 0;
}

SDL_bool SDL_RegisterIntHint(const char *name, int *addr, int default_value)
{
    char defaultstr[64];
    SDL_snprintf(defaultstr, sizeof (defaultstr), "%d", default_value);
    (void) SDL_SetHintWithPriorityAndDefault(name, defaultstr, SDL_HINT_DEFAULT, SDL_TRUE);  /* carry on even if this fails. */
    return SDL_AddHintCallback(name, UpdateRegisteredIntHintCallback, addr);
}

void SDL_UnregisterIntHint(const char *name, int *addr)
{
    SDL_DelHintCallback(name, UpdateRegisteredIntHintCallback, addr);
}


static void SDLCALL UpdateRegisteredFloatHintCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    *((float *) userdata) = newValue ? ((float) SDL_atof(newValue)) : 0.0f;
}

SDL_bool SDL_RegisterFloatHint(const char *name, float *addr, float default_value)
{
    char defaultstr[64];
    SDL_snprintf(defaultstr, sizeof (defaultstr), "%f", default_value);
    (void) SDL_SetHintWithPriorityAndDefault(name, defaultstr, SDL_HINT_DEFAULT, SDL_TRUE);  /* carry on even if this fails. */
    return SDL_AddHintCallback(name, UpdateRegisteredFloatHintCallback, addr);
}

void SDL_UnregisterFloatHint(const char *name, float *addr)
{
    SDL_DelHintCallback(name, UpdateRegisteredFloatHintCallback, addr);
}


static void SDLCALL UpdateRegisteredBoolHintCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    *((SDL_bool *) userdata) = SDL_GetStringBoolean(newValue, SDL_FALSE);
}

SDL_bool SDL_RegisterBoolHint(const char *name, SDL_bool *addr, SDL_bool default_value)
{
    (void) SDL_SetHintWithPriorityAndDefault(name, default_value ? "1" : "0", SDL_HINT_DEFAULT, SDL_TRUE);  /* carry on even if this fails. */
    return SDL_AddHintCallback(name, UpdateRegisteredBoolHintCallback, addr);

}

void SDL_UnregisterBoolHint(const char *name, SDL_bool *addr)
{
    SDL_DelHintCallback(name, UpdateRegisteredBoolHintCallback, addr);
}


static void SDLCALL UpdateRegisteredStringHintCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    *((const char **) userdata) = newValue;
}

SDL_bool SDL_RegisterStringHint(const char *name, const char **addr, const char *default_value)
{
    (void) SDL_SetHintWithPriorityAndDefault(name, default_value, SDL_HINT_DEFAULT, SDL_TRUE);  /* carry on even if this fails. */
    return SDL_AddHintCallback(name, UpdateRegisteredStringHintCallback, addr);
}

void SDL_UnregisterStringHint(const char *name, const char **addr)
{
    SDL_DelHintCallback(name, UpdateRegisteredStringHintCallback, addr);
}

