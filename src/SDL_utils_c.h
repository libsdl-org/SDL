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

#ifndef SDL_utils_h_
#define SDL_utils_h_

/* Common utility functions that aren't in the public API */

/* Return the smallest power of 2 greater than or equal to 'x' */
extern int SDL_powerof2(int x);

extern void SDL_CalculateFraction(float x, int *numerator, int *denominator);

extern SDL_bool SDL_endswith(const char *string, const char *suffix);

typedef enum
{
    SDL_OBJECT_TYPE_UNKNOWN,
    SDL_OBJECT_TYPE_WINDOW,
    SDL_OBJECT_TYPE_RENDERER,
    SDL_OBJECT_TYPE_TEXTURE,
    SDL_OBJECT_TYPE_JOYSTICK,
    SDL_OBJECT_TYPE_GAMEPAD,
    SDL_OBJECT_TYPE_HAPTIC,
    SDL_OBJECT_TYPE_SENSOR,
    SDL_OBJECT_TYPE_HIDAPI_DEVICE,
    SDL_OBJECT_TYPE_HIDAPI_JOYSTICK,

} SDL_ObjectType;

extern Uint32 SDL_GetNextObjectID(void);
extern void SDL_SetObjectValid(void *object, SDL_ObjectType type, SDL_bool valid);
extern SDL_bool SDL_ObjectValid(void *object, SDL_ObjectType type);
extern void SDL_SetObjectsInvalid(void);

#endif /* SDL_utils_h_ */
