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

/**
 *  \file SDL_touch.h
 *
 *  Include file for SDL touch event handling.
 */

#ifndef SDL_touch_h_
#define SDL_touch_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef Sint64 SDL_TouchID;
typedef Sint64 SDL_FingerID;

typedef enum
{
    SDL_TOUCH_DEVICE_INVALID = -1,
    SDL_TOUCH_DEVICE_DIRECT,            /* touch screen with window-relative coordinates */
    SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE, /* trackpad with absolute device coordinates */
    SDL_TOUCH_DEVICE_INDIRECT_RELATIVE  /* trackpad with screen cursor-relative coordinates */
} SDL_TouchDeviceType;

typedef struct SDL_Finger
{
    SDL_FingerID id;
    float x;
    float y;
    float pressure;
} SDL_Finger;

/* Used as the device ID for mouse events simulated with touch input */
#define SDL_TOUCH_MOUSEID ((Uint32)-1)

/* Used as the SDL_TouchID for touch events simulated with mouse input */
#define SDL_MOUSE_TOUCHID ((Sint64)-1)


/**
 * Get a list of registered touch devices.
 *
 * On some platforms SDL first sees the touch device if it was actually used.
 * Therefore the returned list might be empty, although devices are available.
 * After using all devices at least once the number will be correct.
 *
 * This was fixed for Android in SDL 2.0.1.
 *
 * \param count a pointer filled in with the number of devices returned, can
 *              be NULL.
 * \returns a 0 terminated array of touch device IDs which should be freed
 *          with SDL_free(), or NULL on error; call SDL_GetError() for more
 *          details.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_TouchID *SDLCALL SDL_GetTouchDevices(int *count);

/**
 * Get the touch device name as reported from the driver.
 *
 * You do not own the returned string, do not modify or free it.
 *
 * \param touchID the touch device instance ID.
 * \returns touch device name, or NULL on error; call SDL_GetError() for more
 *          details.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char* SDLCALL SDL_GetTouchDeviceName(SDL_TouchID touchID);

/**
 * Get the type of the given touch device.
 *
 * \param touchID the ID of a touch device
 * \returns touch device type
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_TouchDeviceType SDLCALL SDL_GetTouchDeviceType(SDL_TouchID touchID);

/**
 * Get the number of active fingers for a given touch device.
 *
 * \param touchID the ID of a touch device
 * \returns the number of active fingers for a given touch device on success
 *          or a negative error code on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTouchFinger
 */
extern DECLSPEC int SDLCALL SDL_GetNumTouchFingers(SDL_TouchID touchID);

/**
 * Get the finger object for specified touch device ID and finger index.
 *
 * The returned resource is owned by SDL and should not be deallocated.
 *
 * \param touchID the ID of the requested touch device
 * \param index the index of the requested finger
 * \returns a pointer to the SDL_Finger object or NULL if no object at the
 *          given ID and index could be found.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Finger * SDLCALL SDL_GetTouchFinger(SDL_TouchID touchID, int index);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_touch_h_ */
