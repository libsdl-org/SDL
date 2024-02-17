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

#ifdef SDL_JOYSTICK_XINPUT

#include "SDL_windowsjoystick_c.h"
#include "SDL_xinputjoystick_c.h"
#include "SDL_rawinputjoystick_c.h"
#include "../hidapi/SDL_hidapijoystick_c.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal stuff.
 */
static SDL_bool s_bXInputEnabled = SDL_TRUE;

SDL_bool SDL_XINPUT_Enabled(void)
{
    return s_bXInputEnabled;
}

int SDL_XINPUT_JoystickInit(void)
{
    s_bXInputEnabled = SDL_GetHintBoolean(SDL_HINT_XINPUT_ENABLED, SDL_TRUE);

    if (s_bXInputEnabled && WIN_LoadXInputDLL() < 0) {
        s_bXInputEnabled = SDL_FALSE; /* oh well. */
    }
    return 0;
}

static const char *GetXInputName(const Uint8 userid, BYTE SubType)
{
    static char name[32];

    switch (SubType) {
    case XINPUT_DEVSUBTYPE_GAMEPAD:
        (void)SDL_snprintf(name, sizeof(name), "XInput Controller #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_WHEEL:
        (void)SDL_snprintf(name, sizeof(name), "XInput Wheel #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_ARCADE_STICK:
        (void)SDL_snprintf(name, sizeof(name), "XInput ArcadeStick #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
        (void)SDL_snprintf(name, sizeof(name), "XInput FlightStick #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_DANCE_PAD:
        (void)SDL_snprintf(name, sizeof(name), "XInput DancePad #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_GUITAR:
    case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
    case XINPUT_DEVSUBTYPE_GUITAR_BASS:
        (void)SDL_snprintf(name, sizeof(name), "XInput Guitar #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_DRUM_KIT:
        (void)SDL_snprintf(name, sizeof(name), "XInput DrumKit #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_ARCADE_PAD:
        (void)SDL_snprintf(name, sizeof(name), "XInput ArcadePad #%d", 1 + userid);
        break;
    default:
        (void)SDL_snprintf(name, sizeof(name), "XInput Device #%d", 1 + userid);
        break;
    }
    return name;
}

static SDL_bool GetXInputDeviceInfo(Uint8 userid, Uint16 *pVID, Uint16 *pPID, Uint16 *pVersion)
{
    SDL_XINPUT_CAPABILITIES_EX capabilities;

    if (!XINPUTGETCAPABILITIESEX || XINPUTGETCAPABILITIESEX(1, userid, 0, &capabilities) != ERROR_SUCCESS) {
        /* Use a generic VID/PID representing an XInput controller */
        if (pVID) {
            *pVID = USB_VENDOR_MICROSOFT;
        }
        if (pPID) {
            *pPID = USB_PRODUCT_XBOX360_XUSB_CONTROLLER;
        }
        return SDL_FALSE;
    }

    /* Fixup for Wireless Xbox 360 Controller */
    if (capabilities.ProductId == 0 && capabilities.Capabilities.Flags & XINPUT_CAPS_WIRELESS) {
        capabilities.VendorId = USB_VENDOR_MICROSOFT;
        capabilities.ProductId = USB_PRODUCT_XBOX360_XUSB_CONTROLLER;
    }

    if (pVID) {
        *pVID = capabilities.VendorId;
    }
    if (pPID) {
        *pPID = capabilities.ProductId;
    }
    if (pVersion) {
        *pVersion = capabilities.ProductVersion;
    }
    return SDL_TRUE;
}

int SDL_XINPUT_GetSteamVirtualGamepadSlot(Uint8 userid)
{
    SDL_XINPUT_CAPABILITIES_EX capabilities;

    if (XINPUTGETCAPABILITIESEX &&
        XINPUTGETCAPABILITIESEX(1, userid, 0, &capabilities) == ERROR_SUCCESS &&
        capabilities.VendorId == USB_VENDOR_VALVE &&
        capabilities.ProductId == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD) {
        return (int)capabilities.unk2;
    }
    return -1;
}

static void AddXInputDevice(Uint8 userid, BYTE SubType, JoyStick_DeviceData **pContext)
{
    const char *name = NULL;
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    JoyStick_DeviceData *pPrevJoystick = NULL;
    JoyStick_DeviceData *pNewJoystick = *pContext;

#ifdef SDL_JOYSTICK_RAWINPUT
    if (RAWINPUT_IsEnabled()) {
        /* The raw input driver handles more than 4 controllers, so prefer that when available */
        /* We do this check here rather than at the top of SDL_XINPUT_JoystickDetect() because
           we need to check XInput state before RAWINPUT gets a hold of the device, otherwise
           when a controller is connected via the wireless adapter, it will shut down at the
           first subsequent XInput call. This seems like a driver stack bug?

           Reference: https://github.com/libsdl-org/SDL/issues/3468
         */
        return;
    }
#endif

    if (SubType == XINPUT_DEVSUBTYPE_UNKNOWN) {
        return;
    }

    while (pNewJoystick) {
        if (pNewJoystick->bXInputDevice && (pNewJoystick->XInputUserId == userid) && (pNewJoystick->SubType == SubType)) {
            /* if we are replacing the front of the list then update it */
            if (pNewJoystick == *pContext) {
                *pContext = pNewJoystick->pNext;
            } else if (pPrevJoystick) {
                pPrevJoystick->pNext = pNewJoystick->pNext;
            }

            pNewJoystick->pNext = SYS_Joystick;
            SYS_Joystick = pNewJoystick;
            return; /* already in the list. */
        }

        pPrevJoystick = pNewJoystick;
        pNewJoystick = pNewJoystick->pNext;
    }

    pNewJoystick = (JoyStick_DeviceData *)SDL_calloc(1, sizeof(JoyStick_DeviceData));
    if (!pNewJoystick) {
        return; /* better luck next time? */
    }

    name = GetXInputName(userid, SubType);
    GetXInputDeviceInfo(userid, &vendor, &product, &version);
    pNewJoystick->bXInputDevice = SDL_TRUE;
    pNewJoystick->joystickname = SDL_CreateJoystickName(vendor, product, NULL, name);
    if (!pNewJoystick->joystickname) {
        SDL_free(pNewJoystick);
        return; /* better luck next time? */
    }
    (void)SDL_snprintf(pNewJoystick->path, sizeof(pNewJoystick->path), "XInput#%u", userid);
    pNewJoystick->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, vendor, product, version, NULL, name, 'x', SubType);
    pNewJoystick->SubType = SubType;
    pNewJoystick->XInputUserId = userid;

    if (SDL_ShouldIgnoreJoystick(pNewJoystick->joystickname, pNewJoystick->guid)) {
        SDL_free(pNewJoystick);
        return;
    }

    if (SDL_JoystickHandledByAnotherDriver(&SDL_WINDOWS_JoystickDriver, vendor, product, version, pNewJoystick->joystickname)) {
        SDL_free(pNewJoystick);
        return;
    }

    WINDOWS_AddJoystickDevice(pNewJoystick);
}

void SDL_XINPUT_JoystickDetect(JoyStick_DeviceData **pContext)
{
    int iuserid;

    if (!s_bXInputEnabled) {
        return;
    }

    /* iterate in reverse, so these are in the final list in ascending numeric order. */
    for (iuserid = XUSER_MAX_COUNT - 1; iuserid >= 0; iuserid--) {
        const Uint8 userid = (Uint8)iuserid;
        XINPUT_CAPABILITIES capabilities;
        if (XINPUTGETCAPABILITIES(userid, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS) {
            AddXInputDevice(userid, capabilities.SubType, pContext);
        }
    }
}

SDL_bool SDL_XINPUT_JoystickPresent(Uint16 vendor, Uint16 product, Uint16 version)
{
    int iuserid;

    if (!s_bXInputEnabled) {
        return SDL_FALSE;
    }

    /* iterate in reverse, so these are in the final list in ascending numeric order. */
    for (iuserid = 0; iuserid < XUSER_MAX_COUNT; ++iuserid) {
        const Uint8 userid = (Uint8)iuserid;
        Uint16 slot_vendor;
        Uint16 slot_product;
        Uint16 slot_version;
        if (GetXInputDeviceInfo(userid, &slot_vendor, &slot_product, &slot_version)) {
            if (vendor == slot_vendor && product == slot_product && version == slot_version) {
                return SDL_TRUE;
            }
        }
    }
    return SDL_FALSE;
}

int SDL_XINPUT_JoystickOpen(SDL_Joystick *joystick, JoyStick_DeviceData *joystickdevice)
{
    const Uint8 userId = joystickdevice->XInputUserId;
    XINPUT_CAPABILITIES capabilities;
    XINPUT_VIBRATION state;

    SDL_assert(s_bXInputEnabled);
    SDL_assert(XINPUTGETCAPABILITIES);
    SDL_assert(XINPUTSETSTATE);
    SDL_assert(userId < XUSER_MAX_COUNT);

    joystick->hwdata->bXInputDevice = SDL_TRUE;

    if (XINPUTGETCAPABILITIES(userId, XINPUT_FLAG_GAMEPAD, &capabilities) != ERROR_SUCCESS) {
        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
        return SDL_SetError("Failed to obtain XInput device capabilities. Device disconnected?");
    }
    SDL_zero(state);
    joystick->hwdata->bXInputHaptic = (XINPUTSETSTATE(userId, &state) == ERROR_SUCCESS);
    joystick->hwdata->userid = userId;

    /* The XInput API has a hard coded button/axis mapping, so we just match it */
    joystick->naxes = 6;
    joystick->nbuttons = 11;
    joystick->nhats = 1;

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, SDL_TRUE);

    return 0;
}

static void UpdateXInputJoystickBatteryInformation(SDL_Joystick *joystick, XINPUT_BATTERY_INFORMATION_EX *pBatteryInformation)
{
    if (pBatteryInformation->BatteryType != BATTERY_TYPE_UNKNOWN) {
        SDL_JoystickPowerLevel ePowerLevel = SDL_JOYSTICK_POWER_UNKNOWN;
        if (pBatteryInformation->BatteryType == BATTERY_TYPE_WIRED) {
            ePowerLevel = SDL_JOYSTICK_POWER_WIRED;
        } else {
            switch (pBatteryInformation->BatteryLevel) {
            case BATTERY_LEVEL_EMPTY:
                ePowerLevel = SDL_JOYSTICK_POWER_EMPTY;
                break;
            case BATTERY_LEVEL_LOW:
                ePowerLevel = SDL_JOYSTICK_POWER_LOW;
                break;
            case BATTERY_LEVEL_MEDIUM:
                ePowerLevel = SDL_JOYSTICK_POWER_MEDIUM;
                break;
            default:
            case BATTERY_LEVEL_FULL:
                ePowerLevel = SDL_JOYSTICK_POWER_FULL;
                break;
            }
        }

        SDL_SendJoystickBatteryLevel(joystick, ePowerLevel);
    }
}

static void UpdateXInputJoystickState(SDL_Joystick *joystick, XINPUT_STATE *pXInputState, XINPUT_BATTERY_INFORMATION_EX *pBatteryInformation)
{
    static WORD s_XInputButtons[] = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_GUIDE
    };
    WORD wButtons = pXInputState->Gamepad.wButtons;
    Uint8 button;
    Uint8 hat = SDL_HAT_CENTERED;
    Uint64 timestamp = SDL_GetTicksNS();

    SDL_SendJoystickAxis(timestamp, joystick, 0, pXInputState->Gamepad.sThumbLX);
    SDL_SendJoystickAxis(timestamp, joystick, 1, ~pXInputState->Gamepad.sThumbLY);
    SDL_SendJoystickAxis(timestamp, joystick, 2, ((int)pXInputState->Gamepad.bLeftTrigger * 257) - 32768);
    SDL_SendJoystickAxis(timestamp, joystick, 3, pXInputState->Gamepad.sThumbRX);
    SDL_SendJoystickAxis(timestamp, joystick, 4, ~pXInputState->Gamepad.sThumbRY);
    SDL_SendJoystickAxis(timestamp, joystick, 5, ((int)pXInputState->Gamepad.bRightTrigger * 257) - 32768);

    for (button = 0; button < (Uint8)SDL_arraysize(s_XInputButtons); ++button) {
        SDL_SendJoystickButton(timestamp, joystick, button, (wButtons & s_XInputButtons[button]) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        hat |= SDL_HAT_UP;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_SendJoystickHat(timestamp, joystick, 0, hat);

    UpdateXInputJoystickBatteryInformation(joystick, pBatteryInformation);
}

int SDL_XINPUT_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    XINPUT_VIBRATION XVibration;

    if (!XINPUTSETSTATE) {
        return SDL_Unsupported();
    }

    XVibration.wLeftMotorSpeed = low_frequency_rumble;
    XVibration.wRightMotorSpeed = high_frequency_rumble;
    if (XINPUTSETSTATE(joystick->hwdata->userid, &XVibration) != ERROR_SUCCESS) {
        return SDL_SetError("XInputSetState() failed");
    }
    return 0;
}

void SDL_XINPUT_JoystickUpdate(SDL_Joystick *joystick)
{
    DWORD result;
    XINPUT_STATE XInputState;
    XINPUT_BATTERY_INFORMATION_EX XBatteryInformation;

    if (!XINPUTGETSTATE) {
        return;
    }

    result = XINPUTGETSTATE(joystick->hwdata->userid, &XInputState);
    if (result == ERROR_DEVICE_NOT_CONNECTED) {
        return;
    }

    SDL_zero(XBatteryInformation);
    if (XINPUTGETBATTERYINFORMATION) {
        result = XINPUTGETBATTERYINFORMATION(joystick->hwdata->userid, BATTERY_DEVTYPE_GAMEPAD, &XBatteryInformation);
    }

#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    /* XInputOnGameInput doesn't ever change dwPacketNumber, so have to just update every frame */
    UpdateXInputJoystickState(joystick, &XInputState, &XBatteryInformation);
#else
    /* only fire events if the data changed from last time */
    if (XInputState.dwPacketNumber && XInputState.dwPacketNumber != joystick->hwdata->dwPacketNumber) {
        UpdateXInputJoystickState(joystick, &XInputState, &XBatteryInformation);
        joystick->hwdata->dwPacketNumber = XInputState.dwPacketNumber;
    }
#endif
}

void SDL_XINPUT_JoystickClose(SDL_Joystick *joystick)
{
}

void SDL_XINPUT_JoystickQuit(void)
{
    if (s_bXInputEnabled) {
        WIN_UnloadXInputDLL();
    }
}

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#else /* !SDL_JOYSTICK_XINPUT */

typedef struct JoyStick_DeviceData JoyStick_DeviceData;

SDL_bool SDL_XINPUT_Enabled(void)
{
    return SDL_FALSE;
}

int SDL_XINPUT_JoystickInit(void)
{
    return 0;
}

void SDL_XINPUT_JoystickDetect(JoyStick_DeviceData **pContext)
{
}

SDL_bool SDL_XINPUT_JoystickPresent(Uint16 vendor, Uint16 product, Uint16 version)
{
    return SDL_FALSE;
}

int SDL_XINPUT_JoystickOpen(SDL_Joystick *joystick, JoyStick_DeviceData *joystickdevice)
{
    return SDL_Unsupported();
}

int SDL_XINPUT_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

void SDL_XINPUT_JoystickUpdate(SDL_Joystick *joystick)
{
}

void SDL_XINPUT_JoystickClose(SDL_Joystick *joystick)
{
}

void SDL_XINPUT_JoystickQuit(void)
{
}

#endif /* SDL_JOYSTICK_XINPUT */
