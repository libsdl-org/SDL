/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* Useful functions and variables from SDL_joystick.c */
#include "SDL_joystick.h"

/* Initialization and shutdown functions */
extern int SDL_JoystickInit(void);
extern void SDL_JoystickQuit(void);

/* Initialization and shutdown functions */
extern int SDL_GameControllerInit(void);
extern void SDL_GameControllerQuit(void);


/* Internal event queueing functions */
extern int SDL_PrivateJoystickAxis(SDL_Joystick * joystick,
                                   Uint8 axis, Sint16 value);
extern int SDL_PrivateJoystickBall(SDL_Joystick * joystick,
                                   Uint8 ball, Sint16 xrel, Sint16 yrel);
extern int SDL_PrivateJoystickHat(SDL_Joystick * joystick,
                                  Uint8 hat, Uint8 value);
extern int SDL_PrivateJoystickButton(SDL_Joystick * joystick,
                                     Uint8 button, Uint8 state);

/* Internal sanity checking functions */
extern int SDL_PrivateJoystickValid(SDL_Joystick * joystick);

/* vi: set ts=4 sw=4 expandtab: */
