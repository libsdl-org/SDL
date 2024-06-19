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

#include "SDL_hashtable.h"

/* Common utility functions that aren't in the public API */

int SDL_powerof2(int x)
{
    int value;

    if (x <= 0) {
        /* Return some sane value - we shouldn't hit this in our use cases */
        return 1;
    }

    /* This trick works for 32-bit values */
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

SDL_bool SDL_endswith(const char *string, const char *suffix)
{
    size_t string_length = string ? SDL_strlen(string) : 0;
    size_t suffix_length = suffix ? SDL_strlen(suffix) : 0;

    if (suffix_length > 0 && suffix_length <= string_length) {
        if (SDL_memcmp(string + string_length - suffix_length, suffix, suffix_length) == 0) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

char *SDL_UCS4ToUTF8(Uint32 ch, char *dst)
{
    Uint8 *p = (Uint8 *)dst;
    if (ch <= 0x7F) {
        *p = (Uint8)ch;
        ++dst;
    } else if (ch <= 0x7FF) {
        p[0] = 0xC0 | (Uint8)((ch >> 6) & 0x1F);
        p[1] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 2;
    } else if (ch <= 0xFFFF) {
        p[0] = 0xE0 | (Uint8)((ch >> 12) & 0x0F);
        p[1] = 0x80 | (Uint8)((ch >> 6) & 0x3F);
        p[2] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 3;
    } else {
        p[0] = 0xF0 | (Uint8)((ch >> 18) & 0x07);
        p[1] = 0x80 | (Uint8)((ch >> 12) & 0x3F);
        p[2] = 0x80 | (Uint8)((ch >> 6) & 0x3F);
        p[3] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 4;
    }
    return dst;
}

/* Assume we can wrap SDL_AtomicInt values and cast to Uint32 */
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

static SDL_HashTable *SDL_objects;

static Uint32 SDL_HashObject(const void *key, void *unused)
{
    return (Uint32)(uintptr_t)key;
}

static SDL_bool SDL_KeyMatchObject(const void *a, const void *b, void *unused)
{
    return (a == b);
}

void SDL_SetObjectValid(void *object, SDL_ObjectType type, SDL_bool valid)
{
    SDL_assert(object != NULL);

    if (valid) {
        if (!SDL_objects) {
            SDL_objects = SDL_CreateHashTable(NULL, 32, SDL_HashObject, SDL_KeyMatchObject, NULL, SDL_FALSE);
        }

        SDL_InsertIntoHashTable(SDL_objects, object, (void *)(uintptr_t)type);
    } else {
        if (SDL_objects) {
            SDL_RemoveFromHashTable(SDL_objects, object);
        }
    }
}

SDL_bool SDL_ObjectValid(void *object, SDL_ObjectType type)
{
    if (!object) {
        return SDL_FALSE;
    }

    const void *object_type;
    if (!SDL_FindInHashTable(SDL_objects, object, &object_type)) {
        return SDL_FALSE;
    }

    return (((SDL_ObjectType)(uintptr_t)object_type) == type);
}

void SDL_SetObjectsInvalid(void)
{
    if (SDL_objects) {
        /* Log any leaked objects */
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
            default:
                type = "unknown object";
                break;
            }
            SDL_Log("Leaked %s (%p)\n", type, object);
        }
        SDL_assert(SDL_HashTableEmpty(SDL_objects));

        SDL_DestroyHashTable(SDL_objects);
        SDL_objects = NULL;
    }
}
