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

#if SDL_JOYSTICK_XINPUT

/* SDL_xinputjoystick.c implements an XInput-only joystick and game controller
   backend that is suitable for use on WinRT.  SDL's DirectInput backend, also
   XInput-capable, was not used as DirectInput is not available on WinRT (or,
   at least, it isn't a public API).  Some portions of this XInput backend
   may copy parts of the XInput-using code from the DirectInput backend.
   Refactoring the common parts into one location may be good to-do at some
   point.

   TODO, WinRT: add hotplug support for XInput based game controllers
*/

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL_events.h"
#include "../../events/SDL_events_c.h"
#include "SDL_timer.h"

#include <Windows.h>
#include <Xinput.h>

struct joystick_hwdata {
    //Uint8 bXInputHaptic; // Supports force feedback via XInput.
    DWORD userIndex;    // The XInput device index, in the range [0, XUSER_MAX_COUNT-1] (probably [0,3]).
    XINPUT_STATE XInputState;   // the last-read in XInputState, kept around to compare old and new values
    SDL_bool isDeviceConnected; // was the device connected (on the last detection-polling, or during backend-initialization)?
    SDL_bool isDeviceConnectionEventPending;    // was a device added, and is the associated add-event pending?
    SDL_bool isDeviceRemovalEventPending;   // was the device removed, and is the associated remove-event pending?
};

/* Keep track of data on all XInput devices, regardless of whether or not
   they've been opened (via SDL_JoystickOpen).
 */
static struct joystick_hwdata g_XInputData[XUSER_MAX_COUNT];

/* Device detection can be extremely costly performance-wise, in some cases.
   In particular, if no devices are connected, calls to detect a single device,
   via either XInputGetState() or XInputGetCapabilities(), can take upwards of
   20 ms on a 1st generation Surface RT, more if devices are detected across
   all of of XInput's four device slots.  WinRT and XInput do not appear to
   have callback-based APIs to notify an app when a device is connected, at
   least as of Windows 8.1.  The synchronous XInput calls must be used.

   Once a device is connected, calling XInputGetState() is a much less costly
   operation, with individual calls costing well under 1 ms, and often under
   0.1 ms [on a 1st gen Surface RT].

   With XInput's performance limitations in mind, a separate device-detection
   thread will be utilized (by SDL) to try to move costly XInput calls off the
   main thread.  Polling of active devices still, however, occurs on the main
   thread.
 */
static SDL_Thread * g_DeviceDetectionThread = NULL;
static SDL_mutex * g_DeviceInfoLock = NULL;
static SDL_bool g_DeviceDetectionQuit = SDL_FALSE;

/* Main function for the device-detection thread.
 */
static int
DeviceDetectionThreadMain(void * _data)
{
    DWORD result;
    XINPUT_CAPABILITIES tempXInputCaps;
    int i;

    while (1) {
        /* See if the device-detection thread is being asked to shutdown.
         */
        SDL_LockMutex(g_DeviceInfoLock);
        if (g_DeviceDetectionQuit) {
            SDL_UnlockMutex(g_DeviceInfoLock);
            break;
        }
        SDL_UnlockMutex(g_DeviceInfoLock);

        /* Add a short delay to prevent the device-detection thread from eating
           up too much CPU time:
         */
        SDL_Delay(300);

        /* TODO, WinRT: try making the device-detection thread wakeup sooner from its CPU-preserving SDL_Delay, if the thread was asked to quit.
         */

        /* See if any new devices are connected. */
        SDL_LockMutex(g_DeviceInfoLock);
        for (i = 0; i < XUSER_MAX_COUNT; ++i) {
            if (!g_XInputData[i].isDeviceConnected &&
                !g_XInputData[i].isDeviceConnectionEventPending &&
                !g_XInputData[i].isDeviceRemovalEventPending)
            {
                SDL_UnlockMutex(g_DeviceInfoLock);
                result = XInputGetCapabilities(i, 0, &tempXInputCaps);
                SDL_LockMutex(g_DeviceInfoLock);
                if (result == ERROR_SUCCESS) {
                    /* Yes, a device is connected.  Mark it as such.
                       Others will be told about this (via an
                       SDL_JOYDEVICEADDED event) in the next call to
                       SDL_SYS_JoystickDetect.
                     */
                    g_XInputData[i].isDeviceConnected = SDL_TRUE;
                    g_XInputData[i].isDeviceConnectionEventPending = SDL_TRUE;
                }
            }
        }
        SDL_UnlockMutex(g_DeviceInfoLock);
    }

    return 0;
}

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    HRESULT result = S_OK;
    XINPUT_STATE tempXInputState;
    int i;

    SDL_zero(g_XInputData);

    /* Make initial notes on whether or not devices are connected (or not).
     */
    for (i = 0; i < XUSER_MAX_COUNT; ++i) {
        result = XInputGetState(i, &tempXInputState);
        if (result == ERROR_SUCCESS) {
            g_XInputData[i].isDeviceConnected = SDL_TRUE;
        }
    }

    /* Start up the device-detection thread.
     */
    g_DeviceDetectionQuit = SDL_FALSE;
    g_DeviceInfoLock = SDL_CreateMutex();
    g_DeviceDetectionThread = SDL_CreateThread(DeviceDetectionThreadMain, "SDL_joystick", NULL);

    return (0);
}

int SDL_SYS_NumJoysticks()
{
    int joystickCount = 0;
    DWORD i;

    /* Iterate through each possible XInput device and see if something
       was connected (at joystick init, or during the last polling).
     */
    SDL_LockMutex(g_DeviceInfoLock);
    for (i = 0; i < XUSER_MAX_COUNT; ++i) {
        if (g_XInputData[i].isDeviceConnected) {
            ++joystickCount;
        }
    }
    SDL_UnlockMutex(g_DeviceInfoLock);

    return joystickCount;
}

void SDL_SYS_JoystickDetect()
{
    DWORD i;
    SDL_Event event;

    /* Iterate through each possible XInput device, seeing if any devices
       have been connected, or if they were removed.
     */
    SDL_LockMutex(g_DeviceInfoLock);
    for (i = 0; i < XUSER_MAX_COUNT; ++i) {
        /* See if any new devices are connected. */
        if (g_XInputData[i].isDeviceConnectionEventPending) {
#if !SDL_EVENTS_DISABLED
            SDL_zero(event);
            event.type = SDL_JOYDEVICEADDED;
                
            if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                event.jdevice.which = i;
                if ((SDL_EventOK == NULL)
                    || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                    SDL_PushEvent(&event);
                }
            }
#endif
            g_XInputData[i].isDeviceConnectionEventPending = SDL_FALSE;
        } else if (g_XInputData[i].isDeviceRemovalEventPending) {
            /* A device was previously marked as removed (by
               SDL_SYS_JoystickUpdate).  Tell others about the device removal.
            */

            g_XInputData[i].isDeviceRemovalEventPending = SDL_FALSE;

#if !SDL_EVENTS_DISABLED
            SDL_zero(event);
            event.type = SDL_JOYDEVICEREMOVED;
                
            if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                event.jdevice.which = i; //joystick->hwdata->userIndex;
                if ((SDL_EventOK == NULL)
                    || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                    SDL_PushEvent(&event);
                }
            }
#endif
        }
    }
    SDL_UnlockMutex(g_DeviceInfoLock);
}

SDL_bool SDL_SYS_JoystickNeedsPolling()
{
    /* Since XInput, or WinRT, provides any events to indicate when a game
       controller gets connected, and instead indicates device availability
       solely through polling, we'll poll (for new devices).
     */
    return SDL_TRUE;
}

/* Internal function to retreive device capabilities.
   This function will return an SDL-standard value of 0 on success
   (a device is connected, and data on it was retrieved), or -1
   on failure (no device was connected, or some other error
   occurred.  SDL_SetError() will be invoked to set an appropriate
   error message.
 */
static int
SDL_XInput_GetDeviceCapabilities(int device_index, XINPUT_CAPABILITIES * pDeviceCaps)
{
    HRESULT dwResult;

    /* Make sure that the device index is a valid one.  If not, return to the
       caller with an error.
     */
    if (device_index < 0 || device_index >= XUSER_MAX_COUNT) {
        return SDL_SetError("invalid/unavailable device index");
    }

    /* See if a device exists, and if so, what its capabilities are.  If a
       device is not available, return to the caller with an error.
     */
    switch ((dwResult = XInputGetCapabilities(device_index, 0, pDeviceCaps))) {
        case ERROR_SUCCESS:
            /* A device is available, and its capabilities were retrieved! */
            return 0;
        case ERROR_DEVICE_NOT_CONNECTED:
            return SDL_SetError("no device is connected at joystick index, %d", device_index);
        default:
            return SDL_SetError("an unknown error occurred when retrieving info on a device at joystick index, %d", device_index);
    }
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    XINPUT_CAPABILITIES deviceCaps;

    if (SDL_XInput_GetDeviceCapabilities(device_index, &deviceCaps) != 0) {
        /* Uh oh.  Device capabilities couldn't be retrieved.  Return to the
           caller.  SDL_SetError() has already been invoked (with relevant
           information).
         */
        return NULL;
    }

    switch (deviceCaps.SubType) {
        default:
            if (deviceCaps.Type == XINPUT_DEVTYPE_GAMEPAD) {
                return "Undefined game controller";
            } else {
                return "Undefined controller";
            }
        case XINPUT_DEVSUBTYPE_UNKNOWN:
            if (deviceCaps.Type == XINPUT_DEVTYPE_GAMEPAD) {
                return "Unknown game controller";
            } else {
                return "Unknown controller";
            }
        case XINPUT_DEVSUBTYPE_GAMEPAD:
            return "Gamepad controller";
        case XINPUT_DEVSUBTYPE_WHEEL:
            return "Racing wheel controller";
        case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            return "Arcade stick controller";
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            return "Flight stick controller";
        case XINPUT_DEVSUBTYPE_DANCE_PAD:
            return "Dance pad controller";
        case XINPUT_DEVSUBTYPE_GUITAR:
            return "Guitar controller";
        case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
            return "Guitar controller, Alternate";
        case XINPUT_DEVSUBTYPE_GUITAR_BASS:
            return "Guitar controller, Bass";
        case XINPUT_DEVSUBTYPE_DRUM_KIT:
            return "Drum controller";
        case XINPUT_DEVSUBTYPE_ARCADE_PAD:
            return "Arcade pad controller";
    }
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
    XINPUT_CAPABILITIES deviceCaps;

    if (SDL_XInput_GetDeviceCapabilities(device_index, &deviceCaps) != 0) {
        /* Uh oh.  Device capabilities couldn't be retrieved.  Return to the
           caller.  SDL_SetError() has already been invoked (with relevant
           information).
         */
        return -1;
    }

    /* For now, only game pads are supported.  If the device is something other
       than that, return an error to the caller.
     */
    if (deviceCaps.Type != XINPUT_DEVTYPE_GAMEPAD) {
        return SDL_SetError("a device is connected (at joystick index, %d), but it is of an unknown device type (deviceCaps.Flags=%ul)",
            device_index, (unsigned int)deviceCaps.Flags);
    }

    /* Create the joystick data structure */
    joystick->instance_id = device_index;
    joystick->hwdata = &g_XInputData[device_index];

    // The XInput API has a hard coded button/axis mapping, so we just match it
    joystick->naxes = 6;
    joystick->nbuttons = 15;
    joystick->nballs = 0;
    joystick->nhats = 0;

    /* We're done! */
    return (0);
}

/* Function to determine is this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    SDL_bool isDeviceConnected;
    SDL_LockMutex(g_DeviceInfoLock);
    isDeviceConnected = joystick->hwdata->isDeviceConnected;
    SDL_UnlockMutex(g_DeviceInfoLock);
    return isDeviceConnected;
}

/* Function to return > 0 if a bit array of buttons differs after applying a mask
*/
static int ButtonChanged( int ButtonsNow, int ButtonsPrev, int ButtonMask )
{
    return ( ButtonsNow & ButtonMask ) != ( ButtonsPrev & ButtonMask );
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    HRESULT result;
    XINPUT_STATE prevXInputState;

    SDL_LockMutex(g_DeviceInfoLock);

    /* Before polling for new data, make note of the old data */
    prevXInputState = joystick->hwdata->XInputState;

    /* Poll for new data */
    result = XInputGetState(joystick->hwdata->userIndex, &joystick->hwdata->XInputState);
    if (result == ERROR_DEVICE_NOT_CONNECTED) {
        if (joystick->hwdata->isDeviceConnected) {
            joystick->hwdata->isDeviceConnected = SDL_FALSE;
            joystick->hwdata->isDeviceRemovalEventPending = SDL_TRUE;
            /* TODO, WinRT: make sure isDeviceRemovalEventPending gets cleared as appropriate, and that quick re-plugs don't cause trouble */
        }
        SDL_UnlockMutex(g_DeviceInfoLock);
        return;
    }

    /* Make sure the device is marked as connected */
    joystick->hwdata->isDeviceConnected = SDL_TRUE;

    // only fire events if the data changed from last time
    if ( joystick->hwdata->XInputState.dwPacketNumber != 0 
        && joystick->hwdata->XInputState.dwPacketNumber != prevXInputState.dwPacketNumber )
    {
        XINPUT_STATE *pXInputState = &joystick->hwdata->XInputState;
        XINPUT_STATE *pXInputStatePrev = &prevXInputState;

        SDL_PrivateJoystickAxis(joystick, 0, (Sint16)pXInputState->Gamepad.sThumbLX );
        SDL_PrivateJoystickAxis(joystick, 1, (Sint16)(-1*pXInputState->Gamepad.sThumbLY-1) );
        SDL_PrivateJoystickAxis(joystick, 2, (Sint16)pXInputState->Gamepad.sThumbRX );
        SDL_PrivateJoystickAxis(joystick, 3, (Sint16)(-1*pXInputState->Gamepad.sThumbRY-1) );
        SDL_PrivateJoystickAxis(joystick, 4, (Sint16)((int)pXInputState->Gamepad.bLeftTrigger*32767/255) );
        SDL_PrivateJoystickAxis(joystick, 5, (Sint16)((int)pXInputState->Gamepad.bRightTrigger*32767/255) );

        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_UP ) )
            SDL_PrivateJoystickButton(joystick, 0, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_DOWN ) )
            SDL_PrivateJoystickButton(joystick, 1, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_LEFT ) )
            SDL_PrivateJoystickButton(joystick, 2, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_RIGHT ) )
            SDL_PrivateJoystickButton(joystick, 3, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_START ) )
            SDL_PrivateJoystickButton(joystick, 4, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_START ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_BACK ) )
            SDL_PrivateJoystickButton(joystick, 5, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_BACK ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_THUMB ) )
            SDL_PrivateJoystickButton(joystick, 6, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_THUMB ) )
            SDL_PrivateJoystickButton(joystick, 7, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER ) )
            SDL_PrivateJoystickButton(joystick, 8, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER ) )
            SDL_PrivateJoystickButton(joystick, 9, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_A ) )
            SDL_PrivateJoystickButton(joystick, 10, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_A ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_B ) )
            SDL_PrivateJoystickButton(joystick, 11, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_B ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_X ) )
            SDL_PrivateJoystickButton(joystick, 12, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_X ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons, XINPUT_GAMEPAD_Y ) )
            SDL_PrivateJoystickButton(joystick, 13, pXInputState->Gamepad.wButtons & XINPUT_GAMEPAD_Y ? SDL_PRESSED :	SDL_RELEASED );
        if ( ButtonChanged( pXInputState->Gamepad.wButtons, pXInputStatePrev->Gamepad.wButtons,  0x400 ) )
            SDL_PrivateJoystickButton(joystick, 14, pXInputState->Gamepad.wButtons & 0x400 ? SDL_PRESSED :	SDL_RELEASED ); // 0x400 is the undocumented code for the guide button
    }

    SDL_UnlockMutex(g_DeviceInfoLock);
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    /* Clear cached button data on the joystick */
    SDL_LockMutex(g_DeviceInfoLock);
    SDL_zero(joystick->hwdata->XInputState);
    SDL_UnlockMutex(g_DeviceInfoLock);

    /* There's need to free 'hwdata', as it's a pointer to a global array.
       The field will be cleared anyways, just to indicate that it's not
       currently needed.
     */
    joystick->hwdata = NULL;
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    /* Tell the joystick detection thread to stop, then wait for it to finish */
    SDL_LockMutex(g_DeviceInfoLock);
    g_DeviceDetectionQuit = SDL_TRUE;
    SDL_UnlockMutex(g_DeviceInfoLock);
    SDL_WaitThread(g_DeviceDetectionThread, NULL);

    /* Clean up device-detection stuff */
    SDL_DestroyMutex(g_DeviceInfoLock);
    g_DeviceInfoLock = NULL;
    g_DeviceDetectionThread = NULL;
    g_DeviceDetectionQuit = SDL_FALSE;

    return;
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
    SDL_JoystickGUID guid;
    // the GUID is just the first 16 chars of the name for now
    const char *name = SDL_SYS_JoystickNameForDeviceIndex( device_index );
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}


SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    SDL_JoystickGUID guid;
    // the GUID is just the first 16 chars of the name for now
    const char *name = joystick->name;
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

SDL_bool SDL_SYS_IsXInputDeviceIndex(int device_index)
{
    /* The XInput-capable DirectInput joystick backend implements the same
       function (SDL_SYS_IsXInputDeviceIndex), however in that case, not all
       joystick devices are XInput devices.  In this case, with the
       WinRT-enabled XInput-only backend, all "joystick" devices are XInput
       devices.
     */
    return SDL_TRUE;
}

#endif /* SDL_JOYSTICK_XINPUT */

/* vi: set ts=4 sw=4 expandtab: */
