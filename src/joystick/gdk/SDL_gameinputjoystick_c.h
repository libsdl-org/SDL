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
#include "../SDL_sysjoystick.h"

#if defined(SDL_JOYSTICK_GAMEINPUT) && SDL_JOYSTICK_GAMEINPUT

/* include this file in C++ */
#include <GameInput.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum GAMEINPUT_JoystickEffectDataType
{
    GAMEINPUT_JoystickEffectDataType_HapticFeedback
} GAMEINPUT_JoystickEffectDataType;

typedef struct GAMEINPUT_JoystickEffectData
{
    GAMEINPUT_JoystickEffectDataType type;

    union
    {
        struct /* type == GAMEINPUT_JoystickEffectDataType_HapticFeedback */
        {
            uint32_t hapticFeedbackMotorIndex;
            GameInputHapticFeedbackParams hapticFeedbackParams;
        };
    };
} GAMEINPUT_JoystickEffectData;

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* defined(SDL_JOYSTICK_GAMEINPUT) && SDL_JOYSTICK_GAMEINPUT */
