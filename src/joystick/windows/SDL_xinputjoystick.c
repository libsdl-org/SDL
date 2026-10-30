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

typedef struct JoyStick_DeviceData
{
    SDL_GUID guid;
    char *joystickname;
    Uint8 send_add_event;
    SDL_JoystickID nInstanceID;
    bool bXInputDevice;
    BYTE SubType;
    Uint8 XInputUserId;
    char path[MAX_PATH];
    int steam_virtual_gamepad_slot;
    struct JoyStick_DeviceData *pNext;
} JoyStick_DeviceData;

// The private structure used to keep track of a joystick
struct joystick_hwdata
{
    SDL_GUID guid;
    bool bXInputDevice; // true if this device supports using the xinput API rather than DirectInput
    bool bXInputHaptic; // Supports force feedback via XInput.
    Uint8 userid;       // XInput userid index for this joystick
    DWORD dwPacketNumber;
};

// local variables
static bool s_bXInputEnabled = false;
static Uint64 s_nLastDetect;
static JoyStick_DeviceData *SYS_Joystick; // array to hold joystick ID values

static void XINPUT_JoystickDetect(void);

bool SDL_XINPUT_Enabled(void)
{
    return s_bXInputEnabled;
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

static void AddXInputDevice(Uint8 userid, BYTE SubType, JoyStick_DeviceData **pContext)
{
    const char *name = NULL;
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    JoyStick_DeviceData *pPrevJoystick = NULL;
    JoyStick_DeviceData *pNewJoystick = *pContext;

    if (SubType == XINPUT_DEVSUBTYPE_UNKNOWN) {
        return;
    }

    while (pNewJoystick) {
        if ((pNewJoystick->XInputUserId == userid) && (pNewJoystick->SubType == SubType)) {
            // if we are replacing the front of the list then update it
            if (pNewJoystick == *pContext) {
                *pContext = pNewJoystick->pNext;
            } else if (pPrevJoystick) {
                pPrevJoystick->pNext = pNewJoystick->pNext;
            }

            pNewJoystick->pNext = SYS_Joystick;
            SYS_Joystick = pNewJoystick;
            return; // already in the list.
        }

        pPrevJoystick = pNewJoystick;
        pNewJoystick = pNewJoystick->pNext;
    }

    name = GetXInputName(userid, SubType);
    GetXInputDeviceInfo(userid, &vendor, &product, &version);
    if (SDL_ShouldIgnoreJoystick(vendor, product, version, name) ||
        SDL_JoystickHandledByAnotherDriver(&SDL_XINPUT_JoystickDriver, vendor, product, version, name)) {
        return;
    }

    pNewJoystick = (JoyStick_DeviceData *)SDL_calloc(1, sizeof(JoyStick_DeviceData));
    if (!pNewJoystick) {
        return; // better luck next time?
    }

    pNewJoystick->bXInputDevice = true;
    pNewJoystick->joystickname = SDL_CreateJoystickName(vendor, product, NULL, name);
    if (!pNewJoystick->joystickname) {
        SDL_free(pNewJoystick);
        return; // better luck next time?
    }
    (void)SDL_snprintf(pNewJoystick->path, sizeof(pNewJoystick->path), "XInput#%u", userid);
    pNewJoystick->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, vendor, product, version, NULL, name, 'x', SubType);
    pNewJoystick->SubType = SubType;
    pNewJoystick->XInputUserId = userid;

    pNewJoystick->send_add_event = true;
    pNewJoystick->nInstanceID = SDL_GetNextObjectID();
    pNewJoystick->pNext = SYS_Joystick;
    SYS_Joystick = pNewJoystick;
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

    if (enabled) {
        XINPUT_JoystickDetect();
    }

    return true;
}

static int XINPUT_JoystickGetCount(void)
{
    int nJoysticks = 0;
    JoyStick_DeviceData *device = SYS_Joystick;
    while (device) {
        nJoysticks++;
        device = device->pNext;
    }

    return nJoysticks;
}

static void XINPUT_JoystickDetect(void)
{
    JoyStick_DeviceData *pCurList = NULL;

    if (!s_bXInputEnabled) {
        return;
    }

    // We'll check every 3 seconds to avoid hammering open slots
    const Uint32 XINPUT_DETECT_INTERVAL_MS = 3000;
    Uint64 now = SDL_GetTicks();
    if (s_nLastDetect && now < (s_nLastDetect + XINPUT_DETECT_INTERVAL_MS)) {
        return;
    }
    s_nLastDetect = now;

    pCurList = SYS_Joystick;
    SYS_Joystick = NULL;

    // iterate in reverse, so these are in the final list in ascending numeric order.
    for (int iuserid = XUSER_MAX_COUNT - 1; iuserid >= 0; iuserid--) {
        const Uint8 userid = (Uint8)iuserid;
        XINPUT_CAPABILITIES capabilities;
        if (XINPUTGETCAPABILITIES(userid, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS) {
            AddXInputDevice(userid, capabilities.SubType, &pCurList);
        }
    }

    while (pCurList) {
        JoyStick_DeviceData *pListNext = NULL;

        SDL_PrivateJoystickRemoved(pCurList->nInstanceID);

        pListNext = pCurList->pNext;
        SDL_free(pCurList->joystickname);
        SDL_free(pCurList);
        pCurList = pListNext;
    }

    for (pCurList = SYS_Joystick; pCurList; pCurList = pCurList->pNext) {
        if (pCurList->send_add_event) {
            SDL_PrivateJoystickAdded(pCurList->nInstanceID);
            pCurList->send_add_event = false;
        }
    }
}

static bool XINPUT_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    if (!s_bXInputEnabled) {
        return false;
    }

    for (int iuserid = 0; iuserid < XUSER_MAX_COUNT; ++iuserid) {
        const Uint8 userid = (Uint8)iuserid;
        Uint16 slot_vendor;
        Uint16 slot_product;
        Uint16 slot_version;
        if (GetXInputDeviceInfo(userid, &slot_vendor, &slot_product, &slot_version)) {
            if (vendor_id == slot_vendor && product_id == slot_product && version == slot_version) {
                return true;
            }
        }
    }
    return false;
}

static JoyStick_DeviceData *XINPUT_GetDeviceByIndex(int device_index)
{
    JoyStick_DeviceData *device = SYS_Joystick;
    int index;

    for (index = device_index; index > 0; index--) {
        device = device->pNext;
    }

    return device;
}

static const char *XINPUT_JoystickGetDeviceName(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    return device->joystickname;
}

static const char *XINPUT_JoystickGetDevicePath(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    return device->path;
}

static int XINPUT_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    // The slot for XInput devices can change as controllers are seated
    SDL_XINPUT_CAPABILITIES_EX capabilities;
    if (XINPUTGETCAPABILITIESEX &&
        XINPUTGETCAPABILITIESEX(1, device->XInputUserId, 0, &capabilities) == ERROR_SUCCESS &&
        capabilities.VendorId == USB_VENDOR_VALVE &&
        capabilities.ProductId == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD) {
        return (int)capabilities.unk2;
    }
    return -1;
}

static int XINPUT_JoystickGetDevicePlayerIndex(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    return device->XInputUserId;
}

static SDL_GUID XINPUT_JoystickGetDeviceGUID(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    return device->guid;
}

static SDL_JoystickID XINPUT_JoystickGetDeviceInstanceID(int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    return device->nInstanceID;
}

static bool XINPUT_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    JoyStick_DeviceData *device = XINPUT_GetDeviceByIndex(device_index);

    // allocate memory for system specific hardware data
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(struct joystick_hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->hwdata->guid = device->guid;

    const Uint8 userId = device->XInputUserId;
    XINPUT_CAPABILITIES capabilities;
    XINPUT_VIBRATION state;

    SDL_assert(s_bXInputEnabled);
    SDL_assert(XINPUTGETCAPABILITIES);
    SDL_assert(XINPUTSETSTATE);
    SDL_assert(userId < XUSER_MAX_COUNT);

    joystick->hwdata->bXInputDevice = true;

    if (XINPUTGETCAPABILITIES(userId, XINPUT_FLAG_GAMEPAD, &capabilities) != ERROR_SUCCESS) {
        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
        return SDL_SetError("Failed to obtain XInput device capabilities. Device disconnected?");
    }
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
        s_nLastDetect = 0;
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
    JoyStick_DeviceData *device = SYS_Joystick;

    while (device) {
        JoyStick_DeviceData *device_next = device->pNext;
        SDL_free(device->joystickname);
        SDL_free(device);
        device = device_next;
    }
    SYS_Joystick = NULL;

    s_nLastDetect = 0;

    if (s_bXInputEnabled) {
        s_bXInputEnabled = false;
        WIN_UnloadXInputDLL();
    }
}

SDL_JoystickDriver SDL_XINPUT_JoystickDriver = {
    XINPUT_JoystickInit,
    XINPUT_JoystickGetCount,
    XINPUT_JoystickDetect,
    XINPUT_JoystickIsDevicePresent,
    XINPUT_JoystickGetDeviceName,
    XINPUT_JoystickGetDevicePath,
    XINPUT_JoystickGetDeviceSteamVirtualGamepadSlot,
    XINPUT_JoystickGetDevicePlayerIndex,
    NULL, // SetDevicePlayerIndex
    XINPUT_JoystickGetDeviceGUID,
    XINPUT_JoystickGetDeviceInstanceID,
    XINPUT_JoystickOpen,
    XINPUT_JoystickRumble,
    NULL, // RumbleTriggers
    NULL, // SetLED
    NULL, // SendEffect
    NULL, // SetSensorsEnabled
    XINPUT_JoystickUpdate,
    XINPUT_JoystickClose,
    XINPUT_JoystickQuit,
    NULL // GetGamepadMapping
};

#else // !SDL_JOYSTICK_XINPUT

bool SDL_XINPUT_Enabled(void)
{
    return false;
}

#endif // SDL_JOYSTICK_XINPUT
