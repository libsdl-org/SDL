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

#ifndef SDL_gamepad_c_h_
#define SDL_gamepad_c_h_

#include "SDL_internal.h"

/* Useful functions and variables from SDL_gamepad.c */

/* Initialization and shutdown functions */
extern int SDL_InitGamepadMappings(void);
extern void SDL_QuitGamepadMappings(void);
extern int SDL_InitGamepads(void);
extern void SDL_QuitGamepads(void);


/* Function to return whether a joystick name and GUID is a gamepad  */
extern SDL_bool SDL_IsGamepadNameAndGUID(const char *name, SDL_JoystickGUID guid);

/* Function to return whether a gamepad should be ignored */
extern SDL_bool SDL_ShouldIgnoreGamepad(const char *name, SDL_JoystickGUID guid);

/* Handle delayed guide button on a gamepad */
extern void SDL_GamepadHandleDelayedGuideButton(SDL_Joystick *joystick);

#endif /* SDL_gamepad_c_h_ */
