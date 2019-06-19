/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_joystick_c_h_
#define SDL_joystick_c_h_

#include "../SDL_internal.h"

/* Useful functions and variables from SDL_joystick.c */
#include "SDL_joystick.h"

struct _SDL_JoystickDriver;

/* Initialization and shutdown functions */
extern int SDL_JoystickInit(void);
extern void SDL_JoystickQuit(void);

/* Function to get the next available joystick instance ID */
extern SDL_JoystickID SDL_GetNextJoystickInstanceID(void);

/* Initialization and shutdown functions */
extern int SDL_GameControllerInitMappings(void);
extern void SDL_GameControllerQuitMappings(void);
extern int SDL_GameControllerInit(void);
extern void SDL_GameControllerQuit(void);

/* Function to get the joystick driver and device index for an API device index */
extern SDL_bool SDL_GetDriverAndJoystickIndex(int device_index, struct _SDL_JoystickDriver **driver, int *driver_index);

/* Function to return the device index for a joystick ID, or -1 if not found */
extern int SDL_JoystickGetDeviceIndexFromInstanceID(SDL_JoystickID instance_id);

/* Function to extract information from an SDL joystick GUID */
extern void SDL_GetJoystickGUIDInfo(SDL_JoystickGUID guid, Uint16 *vendor, Uint16 *product, Uint16 *version);

/* Function to return whether a joystick is a PS4 controller */
extern SDL_bool SDL_IsJoystickPS4(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Nintendo Switch Pro controller */
extern SDL_bool SDL_IsJoystickNintendoSwitchPro(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Steam Controller */
extern SDL_bool SDL_IsJoystickSteamController(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Xbox 360 controller */
extern SDL_bool SDL_IsJoystickXbox360(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Xbox One controller */
extern SDL_bool SDL_IsJoystickXboxOne(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick guid comes from the XInput driver */
extern SDL_bool SDL_IsJoystickXInput(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the HIDAPI driver */
extern SDL_bool SDL_IsJoystickHIDAPI(SDL_JoystickGUID guid);

/* Function to return whether a joystick should be ignored */
extern SDL_bool SDL_ShouldIgnoreJoystick(const char *name, SDL_JoystickGUID guid);

/* Function to return whether a joystick name and GUID is a game controller  */
extern SDL_bool SDL_IsGameControllerNameAndGUID(const char *name, SDL_JoystickGUID guid);

/* Function to return whether a game controller should be ignored */
extern SDL_bool SDL_ShouldIgnoreGameController(const char *name, SDL_JoystickGUID guid);

/* Handle delayed guide button on a game controller */
extern void SDL_GameControllerHandleDelayedGuideButton(SDL_Joystick *joystick);

/* Internal event queueing functions */
extern void SDL_PrivateJoystickAdded(SDL_JoystickID device_instance);
extern void SDL_PrivateJoystickRemoved(SDL_JoystickID device_instance);
extern int SDL_PrivateJoystickAxis(SDL_Joystick * joystick,
                                   Uint8 axis, Sint16 value);
extern int SDL_PrivateJoystickBall(SDL_Joystick * joystick,
                                   Uint8 ball, Sint16 xrel, Sint16 yrel);
extern int SDL_PrivateJoystickHat(SDL_Joystick * joystick,
                                  Uint8 hat, Uint8 value);
extern int SDL_PrivateJoystickButton(SDL_Joystick * joystick,
                                     Uint8 button, Uint8 state);
extern void SDL_PrivateJoystickBatteryLevel(SDL_Joystick * joystick,
                                            SDL_JoystickPowerLevel ePowerLevel);

/* Internal sanity checking functions */
extern int SDL_PrivateJoystickValid(SDL_Joystick * joystick);

#endif /* SDL_joystick_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
