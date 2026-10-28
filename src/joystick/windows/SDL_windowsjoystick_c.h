/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
#include "../../core/windows/SDL_windows.h"
#include "../../core/windows/SDL_directx.h"

#define MAX_INPUTS 256 // each joystick can have up to 256 inputs

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

// When a driver should look for devices again, see SDL_windowsjoystick.c
typedef struct WINDOWS_JoystickRescan
{
    bool initialized;
    Uint32 change_count;
    Uint64 scan_ns[2];
    Uint64 last_scan_ns;
} WINDOWS_JoystickRescan;

extern void WINDOWS_InitJoystickDeviceNotification(void);
extern void WINDOWS_QuitJoystickDeviceNotification(void);

// Return true if the driver should look for devices now, and set the timeout until it should look again,
// or -1. Drivers with a cheap scan can set scan_immediately to also look as soon as a device change is
// reported, see SDL_windowsjoystick.c. Without device notifications, the driver looks every
// poll_interval_ms, if that's not 0.
extern bool WINDOWS_JoystickRescanNeeded(WINDOWS_JoystickRescan *rescan, bool scan_immediately, Uint32 poll_interval_ms, Sint32 *timeout);

typedef enum Type
{
    BUTTON,
    AXIS,
    HAT
} Type;

typedef struct input_t
{
    // DirectInput offset for this input type:
    DWORD ofs;

    // Button, axis or hat:
    Type type;

    // SDL input offset:
    Uint8 num;
} input_t;

// The private structure used to keep track of a joystick
struct joystick_hwdata
{
    SDL_GUID guid;

#ifdef SDL_JOYSTICK_DINPUT
    LPDIRECTINPUTDEVICE8 InputDevice;
    DIDEVCAPS Capabilities;
    bool buffered;
    bool first_update;
    input_t Inputs[MAX_INPUTS];
    int NumInputs;
    int NumSliders;
    bool ff_initialized;
    DIEFFECT *ffeffect;
    LPDIRECTINPUTEFFECT ffeffect_ref;
#endif

#ifdef SDL_JOYSTICK_XINPUT
    bool bXInputDevice; // true if this device supports using the xinput API rather than DirectInput
    bool bXInputHaptic; // Supports force feedback via XInput.
    Uint8 userid;           // XInput userid index for this joystick
    DWORD dwPacketNumber;
#endif
};

#ifdef SDL_JOYSTICK_DINPUT
extern const DIDATAFORMAT SDL_c_dfDIJoystick2;
#endif


// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif
