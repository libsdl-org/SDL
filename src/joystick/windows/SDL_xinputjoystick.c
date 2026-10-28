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

#ifdef SDL_JOYSTICK_XINPUT

#include "SDL_windowsjoystick_c.h"
#include "SDL_xinputjoystick_c.h"
#include "../../core/windows/SDL_gameinput.h"
#include "../hidapi/SDL_hidapijoystick_c.h"

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

// Internal stuff

static bool s_bXInputEnabled = false;

// The controller in each XInput slot. Only used on the joystick device thread, and by Quit after it stops.
typedef struct XINPUT_Slot
{
    SDL_JoystickID instance_id; // Driver instance ID, not the public joystick ID (see SDL_JoystickDevice)
    BYTE SubType;
    char *name;
    char path[16];
    SDL_GUID guid;
    int steam_virtual_gamepad_slot;
} XINPUT_Slot;

static XINPUT_Slot s_XInputSlots[XUSER_MAX_COUNT];
static WINDOWS_JoystickRescan s_XInputRescan;

// SDL_JoystickDevice driver data
typedef struct XINPUT_DeviceData
{
    Uint8 userid;
} XINPUT_DeviceData;

bool SDL_XINPUT_Enabled(void)
{
    return s_bXInputEnabled;
}

// GameInput's PreInit() runs first, so this can see whether GameInput handles XInput controllers
static bool XINPUT_JoystickPreInit(void)
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

    WINDOWS_InitJoystickDeviceNotification();

    return true;
}

static bool XINPUT_JoystickInit(void)
{
    return true;
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

static bool GetXInputDeviceInfo(Uint8 userid, Uint16 *pVID, Uint16 *pPID, Uint16 *pVersion)
{
    SDL_XINPUT_CAPABILITIES_EX capabilities;

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

static int XINPUT_GetSteamVirtualGamepadSlot(Uint8 userid)
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

static void XINPUT_AddSlot(Uint8 userid, BYTE SubType)
{
    XINPUT_Slot *slot = &s_XInputSlots[userid];
    const char *name = GetXInputName(userid, SubType);
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    char *joystickname;
    XINPUT_DeviceData *data;
    SDL_JoystickDevice *device;

    GetXInputDeviceInfo(userid, &vendor, &product, &version);
    if (SDL_ShouldIgnoreJoystick(vendor, product, version, name)) {
        return;
    }

    joystickname = SDL_CreateJoystickName(vendor, product, NULL, name);
    if (!joystickname) {
        return; // better luck next time?
    }

    data = (XINPUT_DeviceData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_free(joystickname);
        return;
    }
    data->userid = userid;

    device = SDL_CreateJoystickDevice(&SDL_XINPUT_JoystickDriver);
    if (!device) {
        SDL_free(data);
        SDL_free(joystickname);
        return;
    }
    device->driver_data = data;

    SDL_free(slot->name);
    slot->name = joystickname;
    slot->SubType = SubType;
    (void)SDL_snprintf(slot->path, sizeof(slot->path), "XInput#%u", userid);
    slot->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, vendor, product, version, NULL, name, 'x', SubType);
    slot->steam_virtual_gamepad_slot = XINPUT_GetSteamVirtualGamepadSlot(userid);
    slot->instance_id = device->driver_instance_id;

    SDL_SetJoystickDeviceDescription(device, slot->name, slot->path, slot->guid, slot->steam_virtual_gamepad_slot);
    device->vendor_id = vendor;
    device->product_id = product;
    device->type = SDL_GetGamepadTypeFromVIDPID(vendor, product, joystickname, false);
    device->player_index = userid;

    SDL_ReportJoystickDevice(device);
}

static void XINPUT_RemoveSlot(Uint8 userid)
{
    XINPUT_Slot *slot = &s_XInputSlots[userid];

    if (slot->instance_id) {
        SDL_ReportJoystickRemoved(slot->instance_id);
        slot->instance_id = 0;
    }
    SDL_free(slot->name);
    slot->name = NULL;
}

static Sint32 XINPUT_JoystickDiscover(void)
{
    // Without device notifications, look for controllers every second
    const Uint32 XINPUT_POLL_INTERVAL_MS = 1000;
    Sint32 timeout;

    if (!s_bXInputEnabled) {
        return -1;
    }

    // Checking the 4 slots is cheap, so look as soon as something changes
    if (!WINDOWS_JoystickRescanNeeded(&s_XInputRescan, true, XINPUT_POLL_INTERVAL_MS, &timeout)) {
        return timeout;
    }

    for (Uint8 userid = 0; userid < XUSER_MAX_COUNT; ++userid) {
        XINPUT_Slot *slot = &s_XInputSlots[userid];
        XINPUT_CAPABILITIES capabilities;
        BYTE SubType = XINPUT_DEVSUBTYPE_UNKNOWN;

        if (XINPUTGETCAPABILITIES(userid, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS) {
            SubType = capabilities.SubType;
        }

        if (slot->instance_id && SubType != slot->SubType) {
            XINPUT_RemoveSlot(userid);
        }

        if (!slot->instance_id) {
            if (SubType != XINPUT_DEVSUBTYPE_UNKNOWN) {
                XINPUT_AddSlot(userid, SubType);
            }
        } else {
            // The Steam virtual gamepad slot can change as controllers are seated
            int steam_virtual_gamepad_slot = XINPUT_GetSteamVirtualGamepadSlot(userid);
            if (steam_virtual_gamepad_slot != slot->steam_virtual_gamepad_slot) {
                slot->steam_virtual_gamepad_slot = steam_virtual_gamepad_slot;
                SDL_UpdateJoystickDeviceDescription(slot->instance_id, slot->name, slot->path, slot->guid, steam_virtual_gamepad_slot);
            }
        }
    }
    return timeout;
}

static bool XINPUT_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    const XINPUT_DeviceData *data = (const XINPUT_DeviceData *)device->driver_data;
    const Uint8 userId = data->userid;
    XINPUT_CAPABILITIES capabilities;
    XINPUT_VIBRATION state;

    SDL_assert(s_bXInputEnabled);
    SDL_assert(XINPUTGETCAPABILITIES);
    SDL_assert(XINPUTSETSTATE);
    SDL_assert(userId < XUSER_MAX_COUNT);

    if (XINPUTGETCAPABILITIES(userId, XINPUT_FLAG_GAMEPAD, &capabilities) != ERROR_SUCCESS) {
        return SDL_SetError("Failed to obtain XInput device capabilities. Device disconnected?");
    }

    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(struct joystick_hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->hwdata->guid = device->guid;
    joystick->hwdata->bXInputDevice = true;

    SDL_zero(state);
    joystick->hwdata->bXInputHaptic = (XINPUTSETSTATE(userId, &state) == ERROR_SUCCESS);
    joystick->hwdata->userid = userId;

    // The XInput API has a hard coded button/axis mapping, so we just match it
    joystick->naxes = 6;
    joystick->nbuttons = 11;
    joystick->nhats = 1;

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);

    return true;
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
    DWORD result;
    XINPUT_STATE XInputState;
    XINPUT_BATTERY_INFORMATION_EX XBatteryInformation;

    if (!XINPUTGETSTATE || !joystick->hwdata) {
        return;
    }

    result = XINPUTGETSTATE(joystick->hwdata->userid, &XInputState);
    if (result == ERROR_DEVICE_NOT_CONNECTED) {
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
    joystick->hwdata = NULL;
}

static void XINPUT_JoystickQuit(void)
{
    // The joystick device thread has stopped
    for (Uint8 userid = 0; userid < XUSER_MAX_COUNT; ++userid) {
        XINPUT_RemoveSlot(userid);
    }
    SDL_zero(s_XInputRescan);

    WINDOWS_QuitJoystickDeviceNotification();

    if (s_bXInputEnabled) {
        s_bXInputEnabled = false;
        WIN_UnloadXInputDLL();
    }
}

static void XINPUT_JoystickSetPlayerIndex(SDL_JoystickDevice *device, int player_index)
{
}

static bool XINPUT_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    return false;
}

SDL_JoystickDriver SDL_XINPUT_JoystickDriver = {
    .PreInit = XINPUT_JoystickPreInit,
    .Init = XINPUT_JoystickInit,
    .Discover = XINPUT_JoystickDiscover,
    .SetPlayerIndex = XINPUT_JoystickSetPlayerIndex,
    .GetGamepadMapping = XINPUT_JoystickGetGamepadMapping,
    .Open = XINPUT_JoystickOpen,
    .Rumble = XINPUT_JoystickRumble,
    .Update = XINPUT_JoystickUpdate,
    .Close = XINPUT_JoystickClose,
    .Quit = XINPUT_JoystickQuit,
};

// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif

#else // !SDL_JOYSTICK_XINPUT

bool SDL_XINPUT_Enabled(void)
{
    return false;
}

#endif // SDL_JOYSTICK_XINPUT
