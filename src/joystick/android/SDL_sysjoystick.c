/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_config.h"

#ifdef SDL_JOYSTICK_ANDROID

/* This is the system specific header for the SDL joystick API */
#include <stdio.h>              /* For the definition of NULL */

#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "SDL_hints.h"
#include "SDL_assert.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../core/android/SDL_android.h"
#include "android/keycodes.h"

/* As of platform android-14, android/keycodes.h is missing these defines */
#ifndef AKEYCODE_BUTTON_1
#define AKEYCODE_BUTTON_1 188
#define AKEYCODE_BUTTON_2 189
#define AKEYCODE_BUTTON_3 190
#define AKEYCODE_BUTTON_4 191
#define AKEYCODE_BUTTON_5 192
#define AKEYCODE_BUTTON_6 193
#define AKEYCODE_BUTTON_7 194
#define AKEYCODE_BUTTON_8 195
#define AKEYCODE_BUTTON_9 196
#define AKEYCODE_BUTTON_10 197
#define AKEYCODE_BUTTON_11 198
#define AKEYCODE_BUTTON_12 199
#define AKEYCODE_BUTTON_13 200
#define AKEYCODE_BUTTON_14 201
#define AKEYCODE_BUTTON_15 202
#define AKEYCODE_BUTTON_16 203
#endif

#define ANDROID_ACCELEROMETER_INDEX (SYS_numjoysticks - 1)
#define ANDROID_ACCELEROMETER_NAME "Android Accelerometer"
#define ANDROID_MAX_NBUTTONS 36

static SDL_Joystick **SYS_Joysticks;
static char **SYS_JoystickNames;
static int SYS_numjoysticks;
static SDL_bool SYS_accelAsJoy;

/* Function to convert Android keyCodes into SDL ones.
 * This code manipulation is done to get a sequential list of codes.
 * FIXME: This is only suited for the case where we use a fixed number of buttons determined by ANDROID_MAX_NBUTTONS
 */
static int
keycode_to_SDL(int keycode)
{
    /* FIXME: If this function gets too unwiedly in the future, replace with a lookup table */
    int button = 0;
    switch(keycode) 
    {
        /* D-Pad key codes (API 1), these get mapped to 0...4 */
        case AKEYCODE_DPAD_UP:
        case AKEYCODE_DPAD_DOWN:
        case AKEYCODE_DPAD_LEFT:
        case AKEYCODE_DPAD_RIGHT:
        case AKEYCODE_DPAD_CENTER:
            button = keycode - AKEYCODE_DPAD_UP;
            break;
        
        /* Some gamepad buttons (API 9), these get mapped to 5...19*/
        case AKEYCODE_BUTTON_A:
        case AKEYCODE_BUTTON_B:
        case AKEYCODE_BUTTON_C:
        case AKEYCODE_BUTTON_X:
        case AKEYCODE_BUTTON_Y:
        case AKEYCODE_BUTTON_Z:
        case AKEYCODE_BUTTON_L1:
        case AKEYCODE_BUTTON_L2:
        case AKEYCODE_BUTTON_R1:
        case AKEYCODE_BUTTON_R2:
        case AKEYCODE_BUTTON_THUMBL:
        case AKEYCODE_BUTTON_THUMBR:
        case AKEYCODE_BUTTON_START:
        case AKEYCODE_BUTTON_SELECT:
        case AKEYCODE_BUTTON_MODE:
            button = keycode - AKEYCODE_BUTTON_A + 5;
            break;
            
        
        /* More gamepad buttons (API 12), these get mapped to 20...35*/
        case AKEYCODE_BUTTON_1:
        case AKEYCODE_BUTTON_2:
        case AKEYCODE_BUTTON_3:
        case AKEYCODE_BUTTON_4:
        case AKEYCODE_BUTTON_5:
        case AKEYCODE_BUTTON_6:
        case AKEYCODE_BUTTON_7:
        case AKEYCODE_BUTTON_8:
        case AKEYCODE_BUTTON_9:
        case AKEYCODE_BUTTON_10:
        case AKEYCODE_BUTTON_11:
        case AKEYCODE_BUTTON_12:
        case AKEYCODE_BUTTON_13:
        case AKEYCODE_BUTTON_14:
        case AKEYCODE_BUTTON_15:
        case AKEYCODE_BUTTON_16:
            button = keycode - AKEYCODE_BUTTON_1 + 20;
            break;
            
        default:
            SDL_Log("The button you just pressed is not recognized by SDL. To help get this fixed, please report this to the SDL mailing list <sdl@libsdl.org> Android KeyCode %d", keycode);
            return -1;
            break;
    }
    
    /* This is here in case future generations, probably with six fingers per hand, 
     * happily add new cases up above and forget to update the max number of buttons. 
     */
    SDL_assert(button < ANDROID_MAX_NBUTTONS);
    return button;
    
}

/* Function to scan the system for joysticks.
 * This function should set SDL_numjoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    int i = 0;
    const char *env;
    
    env = SDL_GetHint(SDL_HINT_ACCEL_AS_JOY);
    if (env && !SDL_atoi(env))
        SYS_accelAsJoy = SDL_FALSE;
    else
        SYS_accelAsJoy = SDL_TRUE; /* Default behavior */
    
    SYS_numjoysticks = Android_JNI_GetNumJoysticks();
    if (SYS_accelAsJoy) {
        SYS_numjoysticks++;
    }
    SYS_Joysticks = (SDL_Joystick **)SDL_malloc(SYS_numjoysticks*sizeof(SDL_Joystick *));
    if (SYS_Joysticks == NULL)
    {
        return SDL_OutOfMemory();
    }
    SYS_JoystickNames = (char **)SDL_malloc(SYS_numjoysticks*sizeof(char *));
    if (SYS_JoystickNames == NULL)
    {
        SDL_free(SYS_Joysticks);
        SYS_Joysticks = NULL;
        return SDL_OutOfMemory();
    }
    SDL_memset(SYS_JoystickNames, 0, (SYS_numjoysticks*sizeof(char *)));
    SDL_memset(SYS_Joysticks, 0, (SYS_numjoysticks*sizeof(SDL_Joystick *)));
    
    for (i = 0; i < SYS_numjoysticks; i++)
    {
        if ( SYS_accelAsJoy && i == ANDROID_ACCELEROMETER_INDEX ) {
            SYS_JoystickNames[i] = ANDROID_ACCELEROMETER_NAME;
        } else {
            SYS_JoystickNames[i] = Android_JNI_GetJoystickName(i);
        }
    }
   
    return (SYS_numjoysticks);
}

int SDL_SYS_NumJoysticks()
{
    return SYS_numjoysticks;
}

void SDL_SYS_JoystickDetect()
{
}

/* TODO: Hotplugging support */
SDL_bool SDL_SYS_JoystickNeedsPolling()
{
    return SDL_FALSE;
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    return SYS_JoystickNames[device_index];
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    return device_index;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    if (device_index < SYS_numjoysticks) {
        joystick->nhats = 0;
        joystick->nballs = 0;
        if (SYS_accelAsJoy && device_index == ANDROID_ACCELEROMETER_INDEX) {
            joystick->nbuttons = 0;
            joystick->naxes = 3;
        } else {
            /* FIXME: Get the real number of buttons in the device? */
            joystick->nbuttons = ANDROID_MAX_NBUTTONS;
            joystick->naxes = Android_JNI_GetJoystickAxes(device_index);
        }
        
        SYS_Joysticks[device_index] = joystick;
        return 0;
    } else {
        return SDL_SetError("No joystick available with that index");
    }
}

/* Function to determine is this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    return SDL_TRUE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    int i;
    Sint16 value;
    float values[3];

    if (SYS_accelAsJoy && Android_JNI_GetAccelerometerValues(values) &&
        joystick->instance_id == ANDROID_ACCELEROMETER_INDEX) {
        for ( i = 0; i < 3; i++ ) {
            value = (Sint16)(values[i] * 32767.0f);
            SDL_PrivateJoystickAxis(joystick, i, value);
        }
    }
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    SDL_free(SYS_JoystickNames);
    SDL_free(SYS_Joysticks);
    SYS_JoystickNames = NULL;
    SYS_Joysticks = NULL;
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SDL_SYS_JoystickNameForDeviceIndex( device_index );
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = joystick->name;
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

int
Android_OnPadDown(int padId, int keycode)
{
    SDL_PrivateJoystickButton(SYS_Joysticks[padId], keycode_to_SDL(keycode), SDL_PRESSED);
    
    return 0;
}

int
Android_OnPadUp(int padId, int keycode)
{
    SDL_PrivateJoystickButton(SYS_Joysticks[padId], keycode_to_SDL(keycode), SDL_RELEASED);
    
    return 0;
}

int
Android_OnJoy(int joyId, int axis, float value)
{
    /* Android gives joy info normalized as [-1.0, 1.0] or [0.0, 1.0] */
    /* TODO: Are the reported values right? */
    SDL_PrivateJoystickAxis(SYS_Joysticks[joyId], axis, (Sint16) (32767.*value) );
    
    return 0;
}

#endif /* SDL_JOYSTICK_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
