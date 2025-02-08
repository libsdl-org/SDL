/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#if defined(HAVE_GETHOSTNAME) && !defined(SDL_PLATFORM_WINDOWS)
#include <unistd.h>
#endif

// Common utility functions that aren't in the public API

int SDL_powerof2(int x)
{
    int value;

    if (x <= 0) {
        // Return some sane value - we shouldn't hit this in our use cases
        return 1;
    }

    // This trick works for 32-bit values
    {
        SDL_COMPILE_TIME_ASSERT(SDL_powerof2, sizeof(x) == sizeof(Uint32));
    }
    value = x;
    value -= 1;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value += 1;

    return value;
}

Uint32 SDL_CalculateGCD(Uint32 a, Uint32 b)
{
    if (b == 0) {
        return a;
    }
    return SDL_CalculateGCD(b, (a % b));
}

// Algorithm adapted with thanks from John Cook's blog post:
// http://www.johndcook.com/blog/2010/10/20/best-rational-approximation
void SDL_CalculateFraction(float x, int *numerator, int *denominator)
{
    const int N = 1000;
    int a = 0, b = 1;
    int c = 1, d = 0;

    while (b <= N && d <= N) {
        float mediant = (float)(a + c) / (b + d);
        if (x == mediant) {
            if (b + d <= N) {
                *numerator = a + c;
                *denominator = b + d;
            } else if (d > b) {
                *numerator = c;
                *denominator = d;
            } else {
                *numerator = a;
                *denominator = b;
            }
            return;
        } else if (x > mediant) {
            a = a + c;
            b = b + d;
        } else {
            c = a + c;
            d = b + d;
        }
    }
    if (b > N) {
        *numerator = c;
        *denominator = d;
    } else {
        *numerator = a;
        *denominator = b;
    }
}

bool SDL_startswith(const char *string, const char *prefix)
{
    if (SDL_strncmp(string, prefix, SDL_strlen(prefix)) == 0) {
        return true;
    }
    return false;
}

bool SDL_endswith(const char *string, const char *suffix)
{
    size_t string_length = string ? SDL_strlen(string) : 0;
    size_t suffix_length = suffix ? SDL_strlen(suffix) : 0;

    if (suffix_length > 0 && suffix_length <= string_length) {
        if (SDL_memcmp(string + string_length - suffix_length, suffix, suffix_length) == 0) {
            return true;
        }
    }
    return false;
}

SDL_COMPILE_TIME_ASSERT(sizeof_object_id, sizeof(int) == sizeof(Uint32));

Uint32 SDL_GetNextObjectID(void)
{
    static SDL_AtomicInt last_id;

    Uint32 id = (Uint32)SDL_AtomicIncRef(&last_id) + 1;
    if (id == 0) {
        id = (Uint32)SDL_AtomicIncRef(&last_id) + 1;
    }
    return id;
}

static SDL_InitState SDL_objects_init;
static SDL_HashTable *SDL_objects;

static Uint32 SDL_HashObject(const void *key, void *unused)
{
    return (Uint32)(uintptr_t)key;
}

static bool SDL_KeyMatchObject(const void *a, const void *b, void *unused)
{
    return (a == b);
}

void SDL_SetObjectValid(void *object, SDL_ObjectType type, bool valid)
{
    SDL_assert(object != NULL);

    if (valid && SDL_ShouldInit(&SDL_objects_init)) {
        SDL_objects = SDL_CreateHashTable(NULL, 32, SDL_HashObject, SDL_KeyMatchObject, NULL, true, false);
        if (!SDL_objects) {
            SDL_SetInitialized(&SDL_objects_init, false);
        }
        SDL_SetInitialized(&SDL_objects_init, true);
    }

    if (valid) {
        SDL_InsertIntoHashTable(SDL_objects, object, (void *)(uintptr_t)type);
    } else {
        SDL_RemoveFromHashTable(SDL_objects, object);
    }
}

bool SDL_ObjectValid(void *object, SDL_ObjectType type)
{
    if (!object) {
        return false;
    }

    const void *object_type;
    if (!SDL_FindInHashTable(SDL_objects, object, &object_type)) {
        return false;
    }

    return (((SDL_ObjectType)(uintptr_t)object_type) == type);
}

int SDL_GetObjects(SDL_ObjectType type, void **objects, int count)
{
    const void *object, *object_type;
    void *iter = NULL;
    int num_objects = 0;
    while (SDL_IterateHashTable(SDL_objects, &object, &object_type, &iter)) {
        if ((SDL_ObjectType)(uintptr_t)object_type == type) {
            if (num_objects < count) {
                objects[num_objects] = (void *)object;
            }
            ++num_objects;
        }
    }
    return num_objects;
}

void SDL_SetObjectsInvalid(void)
{
    if (SDL_ShouldQuit(&SDL_objects_init)) {
        // Log any leaked objects
        const void *object, *object_type;
        void *iter = NULL;
        while (SDL_IterateHashTable(SDL_objects, &object, &object_type, &iter)) {
            const char *type;
            switch ((SDL_ObjectType)(uintptr_t)object_type) {
            case SDL_OBJECT_TYPE_WINDOW:
                type = "SDL_Window";
                break;
            case SDL_OBJECT_TYPE_RENDERER:
                type = "SDL_Renderer";
                break;
            case SDL_OBJECT_TYPE_TEXTURE:
                type = "SDL_Texture";
                break;
            case SDL_OBJECT_TYPE_JOYSTICK:
                type = "SDL_Joystick";
                break;
            case SDL_OBJECT_TYPE_GAMEPAD:
                type = "SDL_Gamepad";
                break;
            case SDL_OBJECT_TYPE_HAPTIC:
                type = "SDL_Haptic";
                break;
            case SDL_OBJECT_TYPE_SENSOR:
                type = "SDL_Sensor";
                break;
            case SDL_OBJECT_TYPE_HIDAPI_DEVICE:
                type = "hidapi device";
                break;
            case SDL_OBJECT_TYPE_HIDAPI_JOYSTICK:
                type = "hidapi joystick";
                break;
            case SDL_OBJECT_TYPE_THREAD:
                type = "thread";
                break;
            case SDL_OBJECT_TYPE_TRAY:
                type = "SDL_Tray";
                break;
            default:
                type = "unknown object";
                break;
            }
            SDL_Log("Leaked %s (%p)", type, object);
        }
        SDL_assert(SDL_HashTableEmpty(SDL_objects));

        SDL_DestroyHashTable(SDL_objects);
        SDL_objects = NULL;

        SDL_SetInitialized(&SDL_objects_init, false);
    }
}

static int SDL_URIDecode(const char *src, char *dst, int len)
{
    int ri, wi, di;
    char decode = '\0';
    if (!src || !dst || len < 0) {
        return -1;
    }
    if (len == 0) {
        len = (int)SDL_strlen(src);
    }
    for (ri = 0, wi = 0, di = 0; ri < len && wi < len; ri += 1) {
        if (di == 0) {
            // start decoding
            if (src[ri] == '%') {
                decode = '\0';
                di += 1;
                continue;
            }
            // normal write
            dst[wi] = src[ri];
            wi += 1;
        } else if (di == 1 || di == 2) {
            char off = '\0';
            char isa = src[ri] >= 'a' && src[ri] <= 'f';
            char isA = src[ri] >= 'A' && src[ri] <= 'F';
            char isn = src[ri] >= '0' && src[ri] <= '9';
            if (!(isa || isA || isn)) {
                // not a hexadecimal
                int sri;
                for (sri = ri - di; sri <= ri; sri += 1) {
                    dst[wi] = src[sri];
                    wi += 1;
                }
                di = 0;
                continue;
            }
            // itsy bitsy magicsy
            if (isn) {
                off = 0 - '0';
            } else if (isa) {
                off = 10 - 'a';
            } else if (isA) {
                off = 10 - 'A';
            }
            decode |= (src[ri] + off) << (2 - di) * 4;
            if (di == 2) {
                dst[wi] = decode;
                wi += 1;
                di = 0;
            } else {
                di += 1;
            }
        }
    }
    dst[wi] = '\0';
    return wi;
}

int SDL_URIToLocal(const char *src, char *dst)
{
    if (SDL_memcmp(src, "file:/", 6) == 0) {
        src += 6; // local file?
    } else if (SDL_strstr(src, ":/") != NULL) {
        return -1; // wrong scheme
    }

    bool local = src[0] != '/' || (src[0] != '\0' && src[1] == '/');

    // Check the hostname, if present. RFC 3986 states that the hostname component of a URI is not case-sensitive.
    if (!local && src[0] == '/' && src[2] != '/') {
        char *hostname_end = SDL_strchr(src + 1, '/');
        if (hostname_end) {
            const size_t src_len = hostname_end - (src + 1);
            size_t hostname_len;

#if defined(HAVE_GETHOSTNAME) && !defined(SDL_PLATFORM_WINDOWS)
            char hostname[257];
            if (gethostname(hostname, 255) == 0) {
                hostname[256] = '\0';
                hostname_len = SDL_strlen(hostname);
                if (hostname_len == src_len && SDL_strncasecmp(src + 1, hostname, src_len) == 0) {
                    src = hostname_end + 1;
                    local = true;
                }
            }
#endif

            if (!local) {
                static const char *localhost = "localhost";
                hostname_len = SDL_strlen(localhost);
                if (hostname_len == src_len && SDL_strncasecmp(src + 1, localhost, src_len) == 0) {
                    src = hostname_end + 1;
                    local = true;
                }
            }
        }
    }

    if (local) {
        // Convert URI escape sequences to real characters
        if (src[0] == '/') {
            src++;
        } else {
            src--;
        }
        return SDL_URIDecode(src, dst, 0);
    }
    return -1;
}

// This is a set of per-thread persistent strings that we can return from the SDL API.
// This is used for short strings that might persist past the lifetime of the object
// they are related to.

static SDL_TLSID SDL_string_storage;

static void SDL_FreePersistentStrings( void *value )
{
    SDL_HashTable *strings = (SDL_HashTable *)value;
    SDL_DestroyHashTable(strings);
}

const char *SDL_GetPersistentString(const char *string)
{
    if (!string) {
        return NULL;
    }
    if (!*string) {
        return "";
    }

    SDL_HashTable *strings = (SDL_HashTable *)SDL_GetTLS(&SDL_string_storage);
    if (!strings) {
        strings = SDL_CreateHashTable(NULL, 32, SDL_HashString, SDL_KeyMatchString, SDL_NukeFreeValue, false, false);
        if (!strings) {
            return NULL;
        }

        SDL_SetTLS(&SDL_string_storage, strings, SDL_FreePersistentStrings);
    }

    const char *result;
    if (!SDL_FindInHashTable(strings, string, (const void **)&result)) {
        char *new_string = SDL_strdup(string);
        if (!new_string) {
            return NULL;
        }

        // If the hash table insert fails, at least we can return the string we allocated
        SDL_InsertIntoHashTable(strings, new_string, new_string);
        result = new_string;
    }
    return result;
}
