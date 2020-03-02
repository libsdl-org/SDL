/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"


#ifdef SDL_JOYSTICK_HIDAPI_XBOX360

#ifdef __WIN32__
#define SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
/* This requires the Windows 10 SDK to build */
/*#define SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT*/
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
#include "../../core/windows/SDL_xinput.h"
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
#include "../../core/windows/SDL_windows.h"
#define COBJMACROS
#include "windows.gaming.input.h"
#endif


typedef struct {
    Uint8 last_state[USB_PACKET_LENGTH];
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    SDL_bool xinput_enabled;
    Uint8 xinput_slot;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    SDL_bool coinitialized;
    __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics *gamepad_statics;
    __x_ABI_CWindows_CGaming_CInput_CIGamepad *gamepad;
    struct __x_ABI_CWindows_CGaming_CInput_CGamepadVibration vibration;
#endif
} SDL_DriverXbox360_Context;


#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
static Uint8 xinput_slots;

static void
HIDAPI_DriverXbox360_MarkXInputSlotUsed(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_slots |= (0x01 << xinput_slot);
    }
}

static void
HIDAPI_DriverXbox360_MarkXInputSlotFree(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_slots &= ~(0x01 << xinput_slot);
    }
}

static SDL_bool
HIDAPI_DriverXbox360_MissingXInputSlot()
{
    return xinput_slots != 0x0F;
}

static Uint8
HIDAPI_DriverXbox360_GuessXInputSlot(WORD wButtons)
{
    DWORD user_index;
    int match_count;
    Uint8 match_slot;

    if (!XINPUTGETSTATE) {
        return XUSER_INDEX_ANY;
    }

    match_count = 0;
    for (user_index = 0; user_index < XUSER_MAX_COUNT; ++user_index) {
        XINPUT_STATE_EX xinput_state;

        if (XINPUTGETSTATE(user_index, &xinput_state) == ERROR_SUCCESS) {
            if (xinput_state.Gamepad.wButtons == wButtons) {
                ++match_count;
                match_slot = (Uint8)user_index;
            }
        }
    }
    if (match_count == 1) {
        return match_slot;
    }
    return XUSER_INDEX_ANY;
}

#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT

static void
HIDAPI_DriverXbox360_InitWindowsGamingInput(SDL_DriverXbox360_Context *ctx)
{
    /* I think this takes care of RoInitialize() in a way that is compatible with the rest of SDL */
    if (FAILED(WIN_CoInitialize())) {
        return;
    }
    ctx->coinitialized = SDL_TRUE;

    {
        static const IID SDL_IID_IGamepadStatics = { 0x8BBCE529, 0xD49C, 0x39E9, { 0x95, 0x60, 0xE4, 0x7D, 0xDE, 0x96, 0xB7, 0xC8 } };
        HRESULT hr;
        HMODULE hModule = LoadLibraryA("combase.dll");
        if (hModule != NULL) {
            typedef HRESULT (WINAPI *WindowsCreateString_t)(PCNZWCH sourceString, UINT32 length, HSTRING* string);
            typedef HRESULT (WINAPI *WindowsDeleteString_t)(HSTRING string);
            typedef HRESULT (WINAPI *RoGetActivationFactory_t)(HSTRING activatableClassId, REFIID iid, void** factory);

            WindowsCreateString_t WindowsCreateStringFunc = (WindowsCreateString_t)GetProcAddress(hModule, "WindowsCreateString");
            WindowsDeleteString_t WindowsDeleteStringFunc = (WindowsDeleteString_t)GetProcAddress(hModule, "WindowsDeleteString");
            RoGetActivationFactory_t RoGetActivationFactoryFunc = (RoGetActivationFactory_t)GetProcAddress(hModule, "RoGetActivationFactory");
            if (WindowsCreateStringFunc && WindowsDeleteStringFunc && RoGetActivationFactoryFunc) {
                LPTSTR pNamespace = L"Windows.Gaming.Input.Gamepad";
                HSTRING hNamespaceString;

                hr = WindowsCreateStringFunc(pNamespace, SDL_wcslen(pNamespace), &hNamespaceString);
                if (SUCCEEDED(hr)) {
                    RoGetActivationFactoryFunc(hNamespaceString, &SDL_IID_IGamepadStatics, &ctx->gamepad_statics);
                    WindowsDeleteStringFunc(hNamespaceString);
                }
            }
            FreeLibrary(hModule);
        }
    }
}

static Uint8
HIDAPI_DriverXbox360_GetGamepadButtonsForMatch(__x_ABI_CWindows_CGaming_CInput_CIGamepad *gamepad)
{
    HRESULT hr;
    struct __x_ABI_CWindows_CGaming_CInput_CGamepadReading state;
    Uint8 buttons = 0;

    hr = __x_ABI_CWindows_CGaming_CInput_CIGamepad_GetCurrentReading(gamepad, &state);
    if (SUCCEEDED(hr)) {
        if (state.Buttons & GamepadButtons_A) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_A);
        }
        if (state.Buttons & GamepadButtons_B) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_B);
        }
        if (state.Buttons & GamepadButtons_X) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_X);
        }
        if (state.Buttons & GamepadButtons_Y) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_Y);
        }
    }
    return buttons;
}

static void
HIDAPI_DriverXbox360_GuessGamepad(SDL_DriverXbox360_Context *ctx, Uint8 buttons)
{
    HRESULT hr;
    __FIVectorView_1_Windows__CGaming__CInput__CGamepad *gamepads;

    hr = __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_get_Gamepads(ctx->gamepad_statics, &gamepads);
    if (SUCCEEDED(hr)) {
        unsigned int i, num_gamepads;

        hr = __FIVectorView_1_Windows__CGaming__CInput__CGamepad_get_Size(gamepads, &num_gamepads);
        if (SUCCEEDED(hr)) {
            int match_count;
            unsigned int match_slot;

            match_count = 0;
            for (i = 0; i < num_gamepads; ++i) {
                __x_ABI_CWindows_CGaming_CInput_CIGamepad *gamepad;

                hr = __FIVectorView_1_Windows__CGaming__CInput__CGamepad_GetAt(gamepads, i, &gamepad);
                if (SUCCEEDED(hr)) {
                    Uint8 gamepad_buttons = HIDAPI_DriverXbox360_GetGamepadButtonsForMatch(gamepad);
                    if (buttons == gamepad_buttons) {
                        ++match_count;
                        match_slot = i;
                    }
                    __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(gamepad);
                }
            }
            if (match_count == 1) {
                hr = __FIVectorView_1_Windows__CGaming__CInput__CGamepad_GetAt(gamepads, match_slot, &ctx->gamepad);
                if (SUCCEEDED(hr)) {
                }
            }
        }
        __FIVectorView_1_Windows__CGaming__CInput__CGamepad_Release(gamepads);
    }
}

static void
HIDAPI_DriverXbox360_QuitWindowsGamingInput(SDL_DriverXbox360_Context *ctx)
{
    if (ctx->gamepad_statics) {
        __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_Release(ctx->gamepad_statics);
        ctx->gamepad_statics = NULL;
    }
    if (ctx->gamepad) {
        __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(ctx->gamepad);
        ctx->gamepad = NULL;
    }

    if (ctx->coinitialized) {
        WIN_CoUninitialize();
        ctx->coinitialized = SDL_FALSE;
    }
}

#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT */

#if defined(__MACOSX__)
static SDL_bool
IsBluetoothXboxOneController(Uint16 vendor_id, Uint16 product_id)
{
    /* Check to see if it's the Xbox One S or Xbox One Elite Series 2 in Bluetooth mode */
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV2_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}
#endif

static SDL_bool
HIDAPI_DriverXbox360_IsSupportedDevice(const char *name, SDL_GameControllerType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    const int XB360W_IFACE_PROTOCOL = 129; /* Wireless */

    if (vendor_id == USB_VENDOR_NVIDIA) {
        /* This is the NVIDIA Shield controller which doesn't talk Xbox controller protocol */
        return SDL_FALSE;
    }
    if ((vendor_id == USB_VENDOR_MICROSOFT && (product_id == 0x0291 || product_id == 0x0719)) ||
        (type == SDL_CONTROLLER_TYPE_XBOX360 && interface_protocol == XB360W_IFACE_PROTOCOL)) {
        /* This is the wireless dongle, which talks a different protocol */
        return SDL_FALSE;
    }
    if (interface_number > 0) {
        /* This is the chatpad or other input interface, not the Xbox 360 interface */
        return SDL_FALSE;
    }
#if defined(__MACOSX__) || defined(__WIN32__)
    if (vendor_id == USB_VENDOR_MICROSOFT && product_id == 0x028e && version == 1) {
        /* This is the Steam Virtual Gamepad, which isn't supported by this driver */
        return SDL_FALSE;
    }
#if defined(__MACOSX__)
    /* Wired Xbox One controllers are handled by this driver, interfacing with
       the 360Controller driver available from:
       https://github.com/360Controller/360Controller/releases

       Bluetooth Xbox One controllers are handled by the SDL Xbox One driver
    */
    if (IsBluetoothXboxOneController(vendor_id, product_id)) {
        return SDL_FALSE;
    }
#endif
    return (type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE);
#else
    return (type == SDL_CONTROLLER_TYPE_XBOX360);
#endif
}

static const char *
HIDAPI_DriverXbox360_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    return NULL;
}

static SDL_bool SetSlotLED(hid_device *dev, Uint8 slot)
{
    Uint8 mode = 0x02 + slot;
    const Uint8 led_packet[] = { 0x01, 0x03, mode };

    if (hid_write(dev, led_packet, sizeof(led_packet)) != sizeof(led_packet)) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverXbox360_InitDevice(SDL_HIDAPI_Device *device)
{
    return HIDAPI_JoystickConnected(device, NULL);
}

static int
HIDAPI_DriverXbox360_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void
HIDAPI_DriverXbox360_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    if (device->dev) {
        SetSlotLED(device->dev, (player_index % 4));
    }
}

static SDL_bool
HIDAPI_DriverXbox360_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXbox360_Context *ctx;
    int player_index;

    ctx = (SDL_DriverXbox360_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }

    device->dev = hid_open_path(device->path, 0);
    if (!device->dev) {
        SDL_free(ctx);
        SDL_SetError("Couldn't open %s", device->path);
        return SDL_FALSE;
    }
    device->context = ctx;

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    ctx->xinput_enabled = SDL_GetHintBoolean(SDL_HINT_XINPUT_ENABLED, SDL_TRUE);
    if (ctx->xinput_enabled && WIN_LoadXInputDLL() < 0) {
        ctx->xinput_enabled = SDL_FALSE;
    }
    ctx->xinput_slot = XUSER_INDEX_ANY;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    HIDAPI_DriverXbox360_InitWindowsGamingInput(ctx);
#endif

    /* Set the controller LED */
    player_index = SDL_JoystickGetPlayerIndex(joystick);
    if (player_index >= 0) {
        SetSlotLED(device->dev, (player_index % 4));
    }

    /* Initialize the joystick capabilities */
    joystick->nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverXbox360_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
#if defined(SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT)
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
#endif

#ifdef __WIN32__
    SDL_bool rumbled = SDL_FALSE;

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    if (!rumbled && ctx->gamepad) {
        HRESULT hr;

        ctx->vibration.LeftMotor = (DOUBLE)low_frequency_rumble / SDL_MAX_UINT16;
        ctx->vibration.RightMotor = (DOUBLE)high_frequency_rumble / SDL_MAX_UINT16;
        hr = __x_ABI_CWindows_CGaming_CInput_CIGamepad_put_Vibration(ctx->gamepad, ctx->vibration);
        if (SUCCEEDED(hr)) {
            rumbled = SDL_TRUE;
        }
    }
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    if (!rumbled && ctx->xinput_slot != XUSER_INDEX_ANY) {
        XINPUT_VIBRATION XVibration;

        if (!XINPUTSETSTATE) {
            return SDL_Unsupported();
        }

        XVibration.wLeftMotorSpeed = low_frequency_rumble;
        XVibration.wRightMotorSpeed = high_frequency_rumble;
        if (XINPUTSETSTATE(ctx->xinput_slot, &XVibration) == ERROR_SUCCESS) {
            rumbled = SDL_TRUE;
        } else {
            return SDL_SetError("XInputSetState() failed");
        }
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

#else /* !__WIN32__ */

#ifdef __MACOSX__
    if (IsBluetoothXboxOneController(device->vendor_id, device->product_id)) {
        Uint8 rumble_packet[] = { 0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00 };

        rumble_packet[4] = (low_frequency_rumble >> 8);
        rumble_packet[5] = (high_frequency_rumble >> 8);

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    } else {
        /* On Mac OS X the 360Controller driver uses this short report,
           and we need to prefix it with a magic token so hidapi passes it through untouched
         */
        Uint8 rumble_packet[] = { 'M', 'A', 'G', 'I', 'C', '0', 0x00, 0x04, 0x00, 0x00 };

        rumble_packet[6+2] = (low_frequency_rumble >> 8);
        rumble_packet[6+3] = (high_frequency_rumble >> 8);

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    }
#else
    Uint8 rumble_packet[] = { 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    rumble_packet[3] = (low_frequency_rumble >> 8);
    rumble_packet[4] = (high_frequency_rumble >> 8);

    if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }
#endif
#endif /* __WIN32__ */

    return 0;
}

#ifdef __WIN32__
 /* This is the packet format for Xbox 360 and Xbox One controllers on Windows,
    however with this interface there is no rumble support, no guide button,
    and the left and right triggers are tied together as a single axis.

    We use XInput and Windows.Gaming.Input to make up for these shortcomings.
  */
static void
HIDAPI_DriverXbox360_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    SDL_bool has_trigger_data = SDL_FALSE;

    if (ctx->last_state[10] != data[10]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[10] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[10] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[10] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[10] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[10] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[10] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[10] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[10] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[11] != data[11]) {
        SDL_bool dpad_up = SDL_FALSE;
        SDL_bool dpad_down = SDL_FALSE;
        SDL_bool dpad_left = SDL_FALSE;
        SDL_bool dpad_right = SDL_FALSE;

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[11] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[11] & 0x02) ? SDL_PRESSED : SDL_RELEASED);

        switch (data[11] & 0x3C) {
        case 4:
            dpad_up = SDL_TRUE;
            break;
        case 8:
            dpad_up = SDL_TRUE;
            dpad_right = SDL_TRUE;
            break;
        case 12:
            dpad_right = SDL_TRUE;
            break;
        case 16:
            dpad_right = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 20:
            dpad_down = SDL_TRUE;
            break;
        case 24:
            dpad_left = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 28:
            dpad_left = SDL_TRUE;
            break;
        case 32:
            dpad_up = SDL_TRUE;
            dpad_left = SDL_TRUE;
            break;
        default:
            break;
        }
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, dpad_down);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, dpad_up);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, dpad_right);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, dpad_left);
    }

    axis = (int)*(Uint16*)(&data[0]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = (int)*(Uint16*)(&data[2]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = (int)*(Uint16*)(&data[4]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = (int)*(Uint16*)(&data[6]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    if (ctx->gamepad_statics && !ctx->gamepad) {
        Uint8 buttons = 0;

        if (data[10] & 0x01) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_A);
        }
        if (data[10] & 0x02) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_B);
        }
        if (data[10] & 0x04) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_X);
        }
        if (data[10] & 0x08) {
            buttons |= (1 << SDL_CONTROLLER_BUTTON_Y);
        }
        if (buttons != 0) {
            HIDAPI_DriverXbox360_GuessGamepad(ctx, buttons);
        }
    }

    if (ctx->gamepad) {
        HRESULT hr;
        struct __x_ABI_CWindows_CGaming_CInput_CGamepadReading state;
        
        hr = __x_ABI_CWindows_CGaming_CInput_CIGamepad_GetCurrentReading(ctx->gamepad, &state);
        if (SUCCEEDED(hr)) {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (state.Buttons & 0x40000000) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ((int)(state.LeftTrigger * SDL_MAX_UINT16)) - 32768);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ((int)(state.RightTrigger * SDL_MAX_UINT16)) - 32768);
            has_trigger_data = SDL_TRUE;
        }
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    if (ctx->xinput_enabled) {
        if (ctx->xinput_slot == XUSER_INDEX_ANY && HIDAPI_DriverXbox360_MissingXInputSlot()) {
            WORD wButtons = 0;

            if (data[10] & 0x01) {
                wButtons |= XINPUT_GAMEPAD_A;
            }
            if (data[10] & 0x02) {
                wButtons |= XINPUT_GAMEPAD_B;
            }
            if (data[10] & 0x04) {
                wButtons |= XINPUT_GAMEPAD_X;
            }
            if (data[10] & 0x08) {
                wButtons |= XINPUT_GAMEPAD_Y;
            }
            if (wButtons != 0) {
                Uint8 xinput_slot = HIDAPI_DriverXbox360_GuessXInputSlot(wButtons);
                if (xinput_slot != XUSER_INDEX_ANY) {
                    HIDAPI_DriverXbox360_MarkXInputSlotUsed(xinput_slot);
                    ctx->xinput_slot = xinput_slot;
                }
            }
        }

        if (!has_trigger_data && ctx->xinput_slot != XUSER_INDEX_ANY) {
            XINPUT_STATE_EX xinput_state;

            if (XINPUTGETSTATE(ctx->xinput_slot, &xinput_state) == ERROR_SUCCESS) {
                SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) ? SDL_PRESSED : SDL_RELEASED);
                SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ((int)xinput_state.Gamepad.bLeftTrigger * 257) - 32768);
                SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ((int)xinput_state.Gamepad.bRightTrigger * 257) - 32768);
                has_trigger_data = SDL_TRUE;
            }
        }
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

    if (!has_trigger_data) {
        axis = (data[9] * 257) - 32768;
        if (data[9] < 0x80) {
            axis = -axis * 2 - 32769;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
        } else if (data[9] > 0x80) {
            axis = axis * 2 - 32767;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
        } else {
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_MIN_SINT16);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_MIN_SINT16);
        }
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}
#else

static void
HIDAPI_DriverXbox360_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
#ifdef __MACOSX__
    const SDL_bool invert_y_axes = SDL_FALSE;
#else
    const SDL_bool invert_y_axes = SDL_TRUE;
#endif

    if (ctx->last_state[2] != data[2]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, (data[2] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, (data[2] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, (data[2] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (data[2] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[2] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[2] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[2] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[2] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[3] != data[3]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[3] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[3] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[3] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[3] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[3] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[3] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[3] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    axis = ((int)data[4] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)data[5] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = *(Sint16*)(&data[6]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = *(Sint16*)(&data[8]);
    if (invert_y_axes) {
        axis = ~axis;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = *(Sint16*)(&data[10]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = *(Sint16*)(&data[12]);
    if (invert_y_axes) {
        axis = ~axis;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}
#endif /* __WIN32__ */

static SDL_bool
HIDAPI_DriverXbox360_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
    }
    if (!joystick) {
        return SDL_FALSE;
    }

    while ((size = hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
        HIDAPI_DriverXbox360_HandleStatePacket(joystick, device->dev, ctx, data, size);
    }

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, joystick->instance_id);
    }
    return (size >= 0);
}

static void
HIDAPI_DriverXbox360_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
#if defined(SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT)
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    if (ctx->xinput_enabled) {
        HIDAPI_DriverXbox360_MarkXInputSlotFree(ctx->xinput_slot);
        WIN_UnloadXInputDLL();
    }
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    HIDAPI_DriverXbox360_QuitWindowsGamingInput(ctx);
#endif

    hid_close(device->dev);
    device->dev = NULL;

    SDL_free(device->context);
    device->context = NULL;
}

static void
HIDAPI_DriverXbox360_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360 =
{
    SDL_HINT_JOYSTICK_HIDAPI_XBOX,
    SDL_TRUE,
    HIDAPI_DriverXbox360_IsSupportedDevice,
    HIDAPI_DriverXbox360_GetDeviceName,
    HIDAPI_DriverXbox360_InitDevice,
    HIDAPI_DriverXbox360_GetDevicePlayerIndex,
    HIDAPI_DriverXbox360_SetDevicePlayerIndex,
    HIDAPI_DriverXbox360_UpdateDevice,
    HIDAPI_DriverXbox360_OpenJoystick,
    HIDAPI_DriverXbox360_RumbleJoystick,
    HIDAPI_DriverXbox360_CloseJoystick,
    HIDAPI_DriverXbox360_FreeDevice
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOX360 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
