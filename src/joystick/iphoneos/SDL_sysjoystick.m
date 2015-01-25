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
#include "../../SDL_internal.h"

/* This is the iOS implementation of the SDL joystick API */

#include "SDL_joystick.h"
#include "SDL_hints.h"
#include "SDL_stdinc.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#import <CoreMotion/CoreMotion.h>

/* needed for SDL_IPHONE_MAX_GFORCE macro */
#import "SDL_config_iphoneos.h"

const char *accelerometerName = "iOS Accelerometer";

static CMMotionManager *motionManager = nil;
static int numjoysticks = 0;

/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    const char *hint = SDL_GetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK);
    if (!hint || SDL_atoi(hint)) {
        /* Default behavior, accelerometer as joystick */
        numjoysticks = 1;
    }

    return numjoysticks;
}

int SDL_SYS_NumJoysticks()
{
    return numjoysticks;
}

void SDL_SYS_JoystickDetect()
{
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    return accelerometerName;
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
    joystick->naxes = 3;
    joystick->nhats = 0;
    joystick->nballs = 0;
    joystick->nbuttons = 0;

    @autoreleasepool {
        if (motionManager == nil) {
            motionManager = [[CMMotionManager alloc] init];
        }

        /* Shorter times between updates can significantly increase CPU usage. */
        motionManager.accelerometerUpdateInterval = 0.1;
        [motionManager startAccelerometerUpdates];
    }

    return 0;
}

/* Function to determine is this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    return SDL_TRUE;
}

static void SDL_SYS_AccelerometerUpdate(SDL_Joystick * joystick)
{
    const float maxgforce = SDL_IPHONE_MAX_GFORCE;
    const SInt16 maxsint16 = 0x7FFF;
    CMAcceleration accel;

    @autoreleasepool {
        if (!motionManager.accelerometerActive) {
            return;
        }

        accel = motionManager.accelerometerData.acceleration;
    }

    /*
     Convert accelerometer data from floating point to Sint16, which is what
     the joystick system expects.

     To do the conversion, the data is first clamped onto the interval
     [-SDL_IPHONE_MAX_G_FORCE, SDL_IPHONE_MAX_G_FORCE], then the data is multiplied
     by MAX_SINT16 so that it is mapped to the full range of an Sint16.

     You can customize the clamped range of this function by modifying the
     SDL_IPHONE_MAX_GFORCE macro in SDL_config_iphoneos.h.

     Once converted to Sint16, the accelerometer data no longer has coherent
     units. You can convert the data back to units of g-force by multiplying
     it in your application's code by SDL_IPHONE_MAX_GFORCE / 0x7FFF.
     */

    /* clamp the data */
    accel.x = SDL_min(SDL_max(accel.x, -maxgforce), maxgforce);
    accel.y = SDL_min(SDL_max(accel.y, -maxgforce), maxgforce);
    accel.z = SDL_min(SDL_max(accel.z, -maxgforce), maxgforce);

    /* pass in data mapped to range of SInt16 */
    SDL_PrivateJoystickAxis(joystick, 0, (accel.x / maxgforce) * maxsint16);
    SDL_PrivateJoystickAxis(joystick, 1, -(accel.y / maxgforce) * maxsint16);
    SDL_PrivateJoystickAxis(joystick, 2, (accel.z / maxgforce) * maxsint16);
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    SDL_SYS_AccelerometerUpdate(joystick);
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    @autoreleasepool {
        [motionManager stopAccelerometerUpdates];
    }
    joystick->closed = 1;
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    @autoreleasepool {
        motionManager = nil;
    }

    numjoysticks = 0;
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

/* vi: set ts=4 sw=4 expandtab: */
