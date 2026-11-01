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
#include "SDL_xinputjoystick_c.h"

#ifdef SDL_JOYSTICK_XINPUT

#include "../../core/windows/SDL_xinput.h"
#include "../../core/windows/SDL_gameinput.h"
#include "../hidapi/SDL_hidapijoystick_c.h"

// The XInput slot and subtype, also used as the unique ID for a device
struct SDL_JoystickDeviceData
{
    Uint8 userid;
    BYTE SubType;
};

// The private structure used to keep track of a joystick
struct joystick_hwdata
{
    Uint8 userid;       // XInput userid index for this joystick
    DWORD dwPacketNumber;
};

// local variables
static bool s_bXInputEnabled = false;

bool SDL_XINPUT_Enabled(void)
{
    return s_bXInputEnabled;
}

static const char *GetXInputName(const Uint8 userid, BYTE SubType, char *name, size_t maxlen)
{
    switch (SubType) {
    case XINPUT_DEVSUBTYPE_GAMEPAD:
        (void)SDL_snprintf(name, maxlen, "XInput Controller #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_WHEEL:
        (void)SDL_snprintf(name, maxlen, "XInput Wheel #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_ARCADE_STICK:
        (void)SDL_snprintf(name, maxlen, "XInput ArcadeStick #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
        (void)SDL_snprintf(name, maxlen, "XInput FlightStick #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_DANCE_PAD:
        (void)SDL_snprintf(name, maxlen, "XInput DancePad #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_GUITAR:
    case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:
    case XINPUT_DEVSUBTYPE_GUITAR_BASS:
        (void)SDL_snprintf(name, maxlen, "XInput Guitar #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_DRUM_KIT:
        (void)SDL_snprintf(name, maxlen, "XInput DrumKit #%d", 1 + userid);
        break;
    case XINPUT_DEVSUBTYPE_ARCADE_PAD:
        (void)SDL_snprintf(name, maxlen, "XInput ArcadePad #%d", 1 + userid);
        break;
    default:
        (void)SDL_snprintf(name, maxlen, "XInput Device #%d", 1 + userid);
        break;
    }
    return name;
}

static bool GetXInputDeviceInfo(Uint8 userid, Uint16 *pVID, Uint16 *pPID, Uint16 *pVersion, int *pSteamVirtualGamepadSlot)
{
    SDL_XINPUT_CAPABILITIES_EX capabilities;

    if (pSteamVirtualGamepadSlot) {
        *pSteamVirtualGamepadSlot = -1;
    }

    if (!XINPUTGETCAPABILITIESEX || XINPUTGETCAPABILITIESEX(1, userid, 0, &capabilities) != ERROR_SUCCESS) {
        // Use a generic VID/PID representing an XInput controller
        if (pVID) {
            *pVID = USB_VENDOR_MICROSOFT;
        }
        if (pPID) {
            *pPID = USB_PRODUCT_XBOX360_XUSB_CONTROLLER;
        }
        return false;
    }

    if (pSteamVirtualGamepadSlot &&
        capabilities.VendorId == USB_VENDOR_VALVE &&
        capabilities.ProductId == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD) {
        *pSteamVirtualGamepadSlot = (int)capabilities.unk2;
    }

    // Fixup for Wireless Xbox 360 Controller
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
    return true;
}

static SDL_JoystickDevice *CreateXInputDevice(Uint8 userid, BYTE SubType)
{
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    int steam_virtual_gamepad_slot = -1;

    if (SubType == XINPUT_DEVSUBTYPE_UNKNOWN) {
        return NULL;
    }

    char name[32];
    GetXInputDeviceInfo(userid, &vendor, &product, &version, &steam_virtual_gamepad_slot);
    if (SDL_ShouldIgnoreJoystick(vendor, product, version, GetXInputName(userid, SubType, name, sizeof(name)))) {
        return NULL;
    }

    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_XINPUT_JoystickDriver, sizeof(SDL_JoystickDeviceData));
    if (!device) {
        return NULL;
    }

    SDL_JoystickDeviceData *data = (SDL_JoystickDeviceData *)(device + 1);
    data->userid = userid;
    data->SubType = SubType;
    device->internal = data;
    device->unique_id = data;
    device->unique_id_len = sizeof(*data);

    device->name = SDL_CreateJoystickName(vendor, product, NULL, name);
    if (!device->name) {
        SDL_DestroyJoystickDevice(device);
        return NULL;
    }

    if (SDL_asprintf(&device->path, "XInput#%u", userid) < 0) {
        SDL_DestroyJoystickDevice(device);
        return NULL;
    }

    device->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, vendor, product, version, NULL, name, 'x', SubType);
    device->player_index = userid;
    device->steam_virtual_gamepad_slot = steam_virtual_gamepad_slot;

    SDL_AddJoystickDeviceMatchXInput(device, userid);
    SDL_AddJoystickDeviceMatchVIDPID(device, vendor, product);

    return device;
}

static void UpdateXInputJoystickBatteryInformation(SDL_Joystick *joystick, XINPUT_BATTERY_INFORMATION_EX *pBatteryInformation)
{
    SDL_PowerState state;
    int percent;
    switch (pBatteryInformation->BatteryType) {
    case BATTERY_TYPE_WIRED:
        state = SDL_POWERSTATE_CHARGING;
        break;
    case BATTERY_TYPE_UNKNOWN:
    case BATTERY_TYPE_DISCONNECTED:
        state = SDL_POWERSTATE_UNKNOWN;
        break;
    default:
        state = SDL_POWERSTATE_ON_BATTERY;
        break;
    }
    if (state == SDL_POWERSTATE_ON_BATTERY || state == SDL_POWERSTATE_CHARGING) {
        switch (pBatteryInformation->BatteryLevel) {
        case BATTERY_LEVEL_EMPTY:
            percent = 10;
            break;
        case BATTERY_LEVEL_LOW:
            percent = 40;
            break;
        case BATTERY_LEVEL_MEDIUM:
            percent = 70;
            break;
        default:
        case BATTERY_LEVEL_FULL:
            percent = 100;
            break;
        }
    } else {
        percent = -1;
    }
    SDL_SendJoystickPowerInfo(joystick, state, percent);
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
        bool down = ((wButtons & s_XInputButtons[button]) != 0);
        SDL_SendJoystickButton(timestamp, joystick, button, down);
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

static bool XINPUT_JoystickInit(void)
{
    bool enabled = true;

    if (!SDL_GetHintBoolean(SDL_HINT_XINPUT_ENABLED, true) ||
        SDL_UsingGameInputForXInputControllers()) {
        enabled = false;
    }

    if (enabled && !WIN_LoadXInputDLL()) {
        enabled = false; // oh well.
    }
    s_bXInputEnabled = enabled;

    return true;
}

static SDL_JoystickDevice *XINPUT_JoystickQueryDevices(void)
{
    SDL_JoystickDevice *devices = NULL;

    if (!s_bXInputEnabled) {
        return NULL;
    }

    // iterate in reverse, so these are in the final list in ascending numeric order.
    for (int iuserid = XUSER_MAX_COUNT - 1; iuserid >= 0; iuserid--) {
        const Uint8 userid = (Uint8)iuserid;
        XINPUT_CAPABILITIES capabilities;
        if (XINPUTGETCAPABILITIES(userid, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS) {
            SDL_JoystickDevice *device = CreateXInputDevice(userid, capabilities.SubType);
            if (device) {
                device->next = devices;
                devices = device;
            }
        }
    }
    return devices;
}

static void XINPUT_JoystickUpdateSteamVirtualGamepadSlot(SDL_JoystickDevice *device)
{
    const SDL_JoystickDeviceData *data = device->internal;
    Uint16 vendor, product;
    int steam_virtual_gamepad_slot;
    char name[32];

    // Steam can move a different controller into this XInput slot
    if (GetXInputDeviceInfo(data->userid, &vendor, &product, NULL, &steam_virtual_gamepad_slot)) {
        char *joystick_name = SDL_CreateJoystickName(vendor, product, NULL, GetXInputName(data->userid, data->SubType, name, sizeof(name)));
        if (joystick_name) {
            SDL_free(device->name);
            device->name = joystick_name;
        }
    }
    device->steam_virtual_gamepad_slot = steam_virtual_gamepad_slot;
}

static bool XINPUT_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    // allocate memory for system specific hardware data
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(struct joystick_hwdata));
    if (!joystick->hwdata) {
        return false;
    }

    const Uint8 userId = device->internal->userid;
    XINPUT_CAPABILITIES capabilities;

    SDL_assert(s_bXInputEnabled);
    SDL_assert(XINPUTGETCAPABILITIES);
    SDL_assert(userId < XUSER_MAX_COUNT);

    if (XINPUTGETCAPABILITIES(userId, XINPUT_FLAG_GAMEPAD, &capabilities) != ERROR_SUCCESS) {
        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
        return SDL_SetError("Failed to obtain XInput device capabilities. Device disconnected?");
    }
    joystick->hwdata->userid = userId;

    // The XInput API has a hard coded button/axis mapping, so we just match it
    joystick->naxes = 6;
    joystick->nbuttons = 11;
    joystick->nhats = 1;

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);

    return true;
}

static bool XINPUT_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
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
    return true;
}

static void XINPUT_JoystickUpdate(SDL_Joystick *joystick)
{
    if (!joystick->hwdata) {
        return;
    }

    DWORD result;
    XINPUT_STATE XInputState;
    XINPUT_BATTERY_INFORMATION_EX XBatteryInformation;

    if (!XINPUTGETSTATE) {
        return;
    }

    result = XINPUTGETSTATE(joystick->hwdata->userid, &XInputState);
    if (result == ERROR_DEVICE_NOT_CONNECTED) {
        SDL_RequestJoystickDeviceUpdate(true);
        return;
    }

    // FIXME: This does end up making a device ioctl() to query data, we shouldn't do this every update.
    SDL_zero(XBatteryInformation);
    if (XINPUTGETBATTERYINFORMATION) {
        result = XINPUTGETBATTERYINFORMATION(joystick->hwdata->userid, BATTERY_DEVTYPE_GAMEPAD, &XBatteryInformation);
    }

#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    // XInputOnGameInput doesn't ever change dwPacketNumber, so have to just update every frame
    UpdateXInputJoystickState(joystick, &XInputState, &XBatteryInformation);
#else
    // only fire events if the data changed from last time
    if (XInputState.dwPacketNumber && XInputState.dwPacketNumber != joystick->hwdata->dwPacketNumber) {
        UpdateXInputJoystickState(joystick, &XInputState, &XBatteryInformation);
        joystick->hwdata->dwPacketNumber = XInputState.dwPacketNumber;
    }
#endif
}

static void XINPUT_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
}

static void XINPUT_JoystickQuit(void)
{
    if (s_bXInputEnabled) {
        s_bXInputEnabled = false;
        WIN_UnloadXInputDLL();
    }
}

// This file may be compiled as C++, so don't use designated initializers here
SDL_JoystickDriver SDL_XINPUT_JoystickDriver =
{
    XINPUT_JoystickInit,
    XINPUT_JoystickQueryDevices,
    NULL, // DetectDevices
    NULL, // PrepareDevice
    XINPUT_JoystickUpdateSteamVirtualGamepadSlot,
    NULL, // GetGamepadMapping
    XINPUT_JoystickOpen,
    XINPUT_JoystickRumble,
    NULL, // RumbleTriggers
    NULL, // SetLED
    NULL, // SendEffect
    NULL, // SetSensorsEnabled
    NULL, // SetPlayerIndex
    XINPUT_JoystickUpdate,
    XINPUT_JoystickClose,
    XINPUT_JoystickQuit
};

#else // !SDL_JOYSTICK_XINPUT

bool SDL_XINPUT_Enabled(void)
{
    return false;
}

#endif // SDL_JOYSTICK_XINPUT
