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

#ifndef SDL_JOYSTICK_DINPUT_H

/* DirectInput joystick driver; written by Glenn Maynard, based on Andrei de
 * A. Formiga's WINMM driver.
 *
 * Hats and sliders are completely untested; the app I'm writing this for mostly
 * doesn't use them and I don't own any joysticks with them.
 *
 * We don't bother to use event notification here.  It doesn't seem to work
 * with polled devices, and it's fine to call IDirectInputDevice2_GetDeviceData and
 * let it return 0 events. */

#include "../../core/windows/SDL_windows.h"

#define DIRECTINPUT_VERSION 0x0800      /* Need version 7 for force feedback. Need version 8 so IDirectInput8_EnumDevices doesn't leak like a sieve... */
#include <dinput.h>
#define COBJMACROS
#include <wbemcli.h>
#include <oleauto.h>
#include <xinput.h>
#include <devguid.h>
#include <dbt.h>


#ifndef XUSER_MAX_COUNT
#define XUSER_MAX_COUNT 4
#endif
#ifndef XUSER_INDEX_ANY
#define XUSER_INDEX_ANY     0x000000FF
#endif
#ifndef XINPUT_CAPS_FFB_SUPPORTED
#define XINPUT_CAPS_FFB_SUPPORTED 0x0001
#endif


/* typedef's for XInput structs we use */
typedef struct
{
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
    DWORD dwPaddingReserved;
} XINPUT_GAMEPAD_EX;

typedef struct
{
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_EX Gamepad;
} XINPUT_STATE_EX;

/* Forward decl's for XInput API's we load dynamically and use if available */
typedef DWORD (WINAPI *XInputGetState_t)
    (
    DWORD         dwUserIndex,  /* [in] Index of the gamer associated with the device */
    XINPUT_STATE_EX* pState     /* [out] Receives the current state */
    );

typedef DWORD (WINAPI *XInputSetState_t)
    (
    DWORD             dwUserIndex,  /* [in] Index of the gamer associated with the device */
    XINPUT_VIBRATION* pVibration    /* [in, out] The vibration information to send to the controller */
    );

typedef DWORD (WINAPI *XInputGetCapabilities_t)
    (
    DWORD                dwUserIndex,   /* [in] Index of the gamer associated with the device */
    DWORD                dwFlags,       /* [in] Input flags that identify the device type */
    XINPUT_CAPABILITIES* pCapabilities  /* [out] Receives the capabilities */
    );

extern int WIN_LoadXInputDLL(void);
extern void WIN_UnloadXInputDLL(void);

extern XInputGetState_t SDL_XInputGetState;
extern XInputSetState_t SDL_XInputSetState;
extern XInputGetCapabilities_t SDL_XInputGetCapabilities;
extern DWORD SDL_XInputVersion;  /* ((major << 16) & 0xFF00) | (minor & 0xFF) */

#define XINPUTGETSTATE          SDL_XInputGetState
#define XINPUTSETSTATE          SDL_XInputSetState
#define XINPUTGETCAPABILITIES   SDL_XInputGetCapabilities
#define INVALID_XINPUT_USERID   XUSER_INDEX_ANY
#define SDL_XINPUT_MAX_DEVICES  XUSER_MAX_COUNT

#define MAX_INPUTS  256     /* each joystick can have up to 256 inputs */


/* local types */
typedef enum Type
{ BUTTON, AXIS, HAT } Type;

typedef struct input_t
{
    /* DirectInput offset for this input type: */
    DWORD ofs;

    /* Button, axis or hat: */
    Type type;

    /* SDL input offset: */
    Uint8 num;
} input_t;

/* The private structure used to keep track of a joystick */
struct joystick_hwdata
{
    LPDIRECTINPUTDEVICE8 InputDevice;
    DIDEVCAPS Capabilities;
    int buffered;
    SDL_JoystickGUID guid;

    input_t Inputs[MAX_INPUTS];
    int NumInputs;
    int NumSliders;
    Uint8 removed;
    Uint8 send_remove_event;
    Uint8 bXInputDevice; /* 1 if this device supports using the xinput API rather than DirectInput */
    Uint8 bXInputHaptic; /* Supports force feedback via XInput. */
    Uint8 userid; /* XInput userid index for this joystick */
    Uint8 currentXInputSlot; /* the current position to write to in XInputState below, used so we can compare old and new values */
    XINPUT_STATE_EX XInputState[2];
};

#endif /* SDL_JOYSTICK_DINPUT_H */
