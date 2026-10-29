/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_ANDROID
#include "core/android/SDL_android.h"
#endif

typedef struct SDL_HintWatch
{
    SDL_HintCallback callback;
    void *userdata;
    struct SDL_HintWatch *next;
} SDL_HintWatch;

typedef struct SDL_Hint
{
    char *value;
    SDL_HintPriority priority;
    SDL_HintWatch *callbacks;
} SDL_Hint;

/* The hint properties lock protects the values and the callback lists, and is never held while callbacks are
   called, so reading hints never waits for a callback. The callback lock is held while callbacks are called,
   and while callbacks are added and removed, so callbacks are called one at a time, in the order the values
   were set, and a callback isn't running on another thread once it has been removed.

   Callbacks can set hints and add or remove callbacks themselves, both locks are recursive. Callbacks that are
   removed while callbacks are being called are only cleared, and freed later, so the list being walked stays
   valid. Callbacks are added at the head of the list, so they aren't called by a walk that's in progress.
 */
static SDL_AtomicU32 SDL_hint_props;
static void *SDL_hint_callback_lock; // Accessed with the atomic pointer functions
static int SDL_hint_dispatching;     // Callback calls in progress, protected by the callback lock


void SDL_InitHints(void)
{
}

void SDL_QuitHints(void)
{
    SDL_PropertiesID props;
    do {
        props = SDL_GetAtomicU32(&SDL_hint_props);
    } while (!SDL_CompareAndSwapAtomicU32(&SDL_hint_props, props, 0));

    if (props) {
        SDL_DestroyProperties(props);
    }

    SDL_Mutex *callback_lock = (SDL_Mutex *)SDL_SetAtomicPointer(&SDL_hint_callback_lock, NULL);
    SDL_DestroyMutex(callback_lock);
}

static SDL_Mutex *GetHintCallbackLock(void)
{
    SDL_Mutex *lock = (SDL_Mutex *)SDL_GetAtomicPointer(&SDL_hint_callback_lock);
    if (!lock) {
        lock = SDL_CreateMutex();
        if (!SDL_CompareAndSwapAtomicPointer(&SDL_hint_callback_lock, NULL, lock)) {
            // Somebody else created the lock before us, just use that
            SDL_DestroyMutex(lock);
            lock = (SDL_Mutex *)SDL_GetAtomicPointer(&SDL_hint_callback_lock);
        }
    }
    return lock;
}

static SDL_PropertiesID GetHintProperties(bool create)
{
    SDL_PropertiesID props = SDL_GetAtomicU32(&SDL_hint_props);
    if (!props && create) {
        props = SDL_CreateProperties();
        if (!SDL_CompareAndSwapAtomicU32(&SDL_hint_props, 0, props)) {
            // Somebody else created hint properties before us, just use those
            SDL_DestroyProperties(props);
            props = SDL_GetAtomicU32(&SDL_hint_props);
        }
    }
    return props;
}

static void SDLCALL CleanupHintProperty(void *userdata, void *value)
{
    SDL_Hint *hint = (SDL_Hint *) value;
    SDL_free(hint->value);

    SDL_HintWatch *entry = hint->callbacks;
    while (entry) {
        SDL_HintWatch *freeable = entry;
        entry = entry->next;
        SDL_free(freeable);
    }
    SDL_free(hint);
}

static const char *GetHintEnvironmentVariable(const char *name)
{
    const char *result = SDL_getenv(name);
    if (!result && name && *name) {
        // fall back to old (SDL2) names of environment variables that
        // are important to users (e.g. many use SDL_VIDEODRIVER=wayland)
        if (SDL_strcmp(name, SDL_HINT_VIDEO_DRIVER) == 0) {
            result = SDL_getenv("SDL_VIDEODRIVER");
        } else if (SDL_strcmp(name, SDL_HINT_AUDIO_DRIVER) == 0) {
            result = SDL_getenv("SDL_AUDIODRIVER");
        }
    }
    return result;
}

// Find a hint, adding it if it doesn't exist yet. Called holding the hint properties lock.
static SDL_Hint *GetOrAddHint(SDL_PropertiesID hints, const char *name)
{
    SDL_Hint *hint = (SDL_Hint *)SDL_GetPointerProperty(hints, name, NULL);
    if (!hint) {
        hint = (SDL_Hint *)SDL_calloc(1, sizeof(*hint));
        if (!hint) {
            return NULL;
        }
        hint->priority = SDL_HINT_DEFAULT;
        if (!SDL_SetPointerPropertyWithCleanup(hints, name, hint, CleanupHintProperty, NULL)) {
            // SDL_SetPointerPropertyWithCleanup() cleans up the hint if it fails
            return NULL;
        }
    }
    return hint;
}

// Call a hint's callbacks, holding the callback lock and not the hint properties lock
static void CallHintCallbacks(SDL_Hint *hint, const char *name, const char *old_value, const char *new_value)
{
    ++SDL_hint_dispatching;
    for (SDL_HintWatch *entry = hint->callbacks; entry; entry = entry->next) {
        if (entry->callback) {
            entry->callback(entry->userdata, name, old_value, new_value);
        }
    }
    --SDL_hint_dispatching;
}

// Remove a callback, holding the callback lock and the hint properties lock
static void RemoveHintCallback(SDL_Hint *hint, SDL_HintCallback callback, void *userdata)
{
    SDL_HintWatch *entry, *prev = NULL, *next;

    for (entry = hint->callbacks; entry; entry = entry->next) {
        if ((callback == entry->callback) && (userdata == entry->userdata)) {
            entry->callback = NULL;
            break;
        }
    }

    if (SDL_hint_dispatching) {
        // Freed when callbacks aren't being called
        return;
    }

    for (entry = hint->callbacks; entry; entry = next) {
        next = entry->next;
        if (!entry->callback) {
            if (prev) {
                prev->next = next;
            } else {
                hint->callbacks = next;
            }
            SDL_free(entry);
        } else {
            prev = entry;
        }
    }
}

bool SDL_SetHintWithPriority(const char *name, const char *value, SDL_HintPriority priority)
{
    CHECK_PARAM(!name || !*name) {
        return SDL_InvalidParamError("name");
    }

    const char *env = GetHintEnvironmentVariable(name);
    if (env && (priority < SDL_HINT_OVERRIDE)) {
        return SDL_SetError("An environment variable is taking priority");
    }

    const SDL_PropertiesID hints = GetHintProperties(true);
    if (!hints) {
        return false;
    }

    bool result = false;
    char *old_value = NULL;
    bool changed = false;

    SDL_Mutex *callback_lock = GetHintCallbackLock();

    SDL_LockMutex(callback_lock);
    SDL_LockProperties(hints);

    SDL_Hint *hint = GetOrAddHint(hints, name);
    if (hint && priority >= hint->priority) {
        if (hint->value != value && (!value || !hint->value || SDL_strcmp(hint->value, value) != 0)) {
            old_value = hint->value;
            hint->value = value ? SDL_strdup(value) : NULL;
            changed = true;
        }
        hint->priority = priority;
        result = true;
    }

#ifdef SDL_PLATFORM_ANDROID
    if (SDL_strcmp(name, SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY) == 0) {
        // Special handling for this hint, which needs to persist outside the normal application flow
        Android_SetAllowRecreateActivity(SDL_GetStringBoolean(value, false));
    }
#endif // SDL_PLATFORM_ANDROID

    SDL_UnlockProperties(hints);

    if (changed) {
        CallHintCallbacks(hint, name, old_value, value);
        SDL_free(old_value);
    }

    SDL_UnlockMutex(callback_lock);

    return result;
}

bool SDL_ResetHint(const char *name)
{
    CHECK_PARAM(!name || !*name) {
        return SDL_InvalidParamError("name");
    }

    const char *env = GetHintEnvironmentVariable(name);

    const SDL_PropertiesID hints = GetHintProperties(false);
    if (!hints) {
        return false;
    }

    char *old_value = NULL;
    bool changed = false;

    SDL_Mutex *callback_lock = GetHintCallbackLock();

    SDL_LockMutex(callback_lock);
    SDL_LockProperties(hints);

    SDL_Hint *hint = (SDL_Hint *)SDL_GetPointerProperty(hints, name, NULL);
    if (!hint) {
        SDL_UnlockProperties(hints);
        SDL_UnlockMutex(callback_lock);
        return false;
    }

    if ((!env && hint->value) || (env && !hint->value) || (env && SDL_strcmp(env, hint->value) != 0)) {
        changed = true;
    }
    old_value = hint->value;
    hint->value = NULL;
    hint->priority = SDL_HINT_DEFAULT;

#ifdef SDL_PLATFORM_ANDROID
    if (SDL_strcmp(name, SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY) == 0) {
        // Special handling for this hint, which needs to persist outside the normal application flow
        if (env) {
            Android_SetAllowRecreateActivity(SDL_GetStringBoolean(env, false));
        } else {
            Android_SetAllowRecreateActivity(false);
        }
    }
#endif // SDL_PLATFORM_ANDROID

    SDL_UnlockProperties(hints);

    if (changed) {
        CallHintCallbacks(hint, name, old_value, env);
    }
    SDL_free(old_value);

    SDL_UnlockMutex(callback_lock);

    return true;
}

typedef struct ResetHintsData
{
    char **names;
    int count;
    int max;
} ResetHintsData;

static void SDLCALL CollectHintNamesCallback(void *userdata, SDL_PropertiesID hints, const char *name)
{
    ResetHintsData *data = (ResetHintsData *)userdata;

    if (data->count == data->max) {
        int max = data->max ? data->max * 2 : 32;
        char **names = (char **)SDL_realloc(data->names, max * sizeof(*names));
        if (!names) {
            return;
        }
        data->names = names;
        data->max = max;
    }
    data->names[data->count] = SDL_strdup(name);
    if (data->names[data->count]) {
        ++data->count;
    }
}

void SDL_ResetHints(void)
{
    ResetHintsData data;
    int i;

    // Collect the names first, callbacks can't be called while the properties are being enumerated
    SDL_zero(data);
    SDL_EnumerateProperties(GetHintProperties(false), CollectHintNamesCallback, &data);

    for (i = 0; i < data.count; ++i) {
        SDL_ResetHint(data.names[i]);
        SDL_free(data.names[i]);
    }
    SDL_free(data.names);
}

bool SDL_SetHint(const char *name, const char *value)
{
    return SDL_SetHintWithPriority(name, value, SDL_HINT_NORMAL);
}

const char *SDL_GetHint(const char *name)
{
    if (!name) {
        return NULL;
    }

    const char *result = GetHintEnvironmentVariable(name);

    const SDL_PropertiesID hints = GetHintProperties(false);
    if (hints) {
        SDL_LockProperties(hints);

        SDL_Hint *hint = (SDL_Hint *)SDL_GetPointerProperty(hints, name, NULL);
        if (hint) {
            if (!result || hint->priority == SDL_HINT_OVERRIDE) {
                result = SDL_GetPersistentString(hint->value);
            }
        }

        SDL_UnlockProperties(hints);
    }

    return result;
}

int SDL_GetStringInteger(const char *value, int default_value)
{
    if (!value || !*value) {
        return default_value;
    }
    if (SDL_strcasecmp(value, "false") == 0) {
        return 0;
    }
    if (SDL_strcasecmp(value, "true") == 0) {
        return 1;
    }
    if (*value == '-' || SDL_isdigit(*value)) {
        return SDL_atoi(value);
    }
    return default_value;
}

bool SDL_GetStringBoolean(const char *value, bool default_value)
{
    if (!value || !*value) {
        return default_value;
    }
    if (*value == '0' || SDL_strcasecmp(value, "false") == 0) {
        return false;
    }
    return true;
}

bool SDL_GetHintBoolean(const char *name, bool default_value)
{
    const char *hint = SDL_GetHint(name);
    return SDL_GetStringBoolean(hint, default_value);
}

bool SDL_AddHintCallback(const char *name, SDL_HintCallback callback, void *userdata)
{
    CHECK_PARAM(!name || !*name) {
        return SDL_InvalidParamError("name");
    }
    CHECK_PARAM(!callback) {
        return SDL_InvalidParamError("callback");
    }

    const SDL_PropertiesID hints = GetHintProperties(true);
    if (!hints) {
        return false;
    }

    SDL_HintWatch *entry = (SDL_HintWatch *)SDL_malloc(sizeof(*entry));
    if (!entry) {
        return false;
    }
    entry->callback = callback;
    entry->userdata = userdata;

    SDL_Mutex *callback_lock = GetHintCallbackLock();

    SDL_LockMutex(callback_lock);
    SDL_LockProperties(hints);

    SDL_Hint *hint = GetOrAddHint(hints, name);
    if (!hint) {
        SDL_UnlockProperties(hints);
        SDL_UnlockMutex(callback_lock);
        SDL_free(entry);
        return false;
    }

    RemoveHintCallback(hint, callback, userdata);

    // Add it to the callbacks for this hint
    entry->next = hint->callbacks;
    hint->callbacks = entry;

    // Get the current value, it's a persistent copy so it can be used after unlocking
    const char *value = SDL_GetHint(name);

    SDL_UnlockProperties(hints);

    // Now call it with the current value
    callback(userdata, name, value, value);

    SDL_UnlockMutex(callback_lock);

    return true;
}

void SDL_RemoveHintCallback(const char *name, SDL_HintCallback callback, void *userdata)
{
    if (!name || !*name) {
        return;
    }

    const SDL_PropertiesID hints = GetHintProperties(false);
    if (!hints) {
        return;
    }

    SDL_Mutex *callback_lock = GetHintCallbackLock();

    // Wait for callbacks to finish on other threads, so this one isn't called once it's removed
    SDL_LockMutex(callback_lock);
    SDL_LockProperties(hints);
    SDL_Hint *hint = (SDL_Hint *)SDL_GetPointerProperty(hints, name, NULL);
    if (hint) {
        RemoveHintCallback(hint, callback, userdata);
    }
    SDL_UnlockProperties(hints);
    SDL_UnlockMutex(callback_lock);
}
