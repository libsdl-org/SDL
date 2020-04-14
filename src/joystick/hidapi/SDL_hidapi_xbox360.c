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
typedef struct WindowsGamingInputGamepadState WindowsGamingInputGamepadState;
#define GamepadButtons_GUIDE 0x40000000
#define COBJMACROS
#include "windows.gaming.input.h"
#endif

#if defined(SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT)
#define SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
#endif

typedef struct {
    Uint8 last_state[USB_PACKET_LENGTH];
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    Uint32 match_state; /* Low 16 bits for button states, high 16 for 4 4bit axes */
    Uint32 last_state_packet;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    SDL_bool xinput_enabled;
    SDL_bool xinput_correlated;
    Uint8 xinput_correlation_id;
    Uint8 xinput_correlation_count;
    Uint8 xinput_uncorrelate_count;
    Uint8 xinput_slot;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    SDL_bool wgi_correlated;
    Uint8 wgi_correlation_id;
    Uint8 wgi_correlation_count;
    Uint8 wgi_uncorrelate_count;
    WindowsGamingInputGamepadState *wgi_slot;
#endif
} SDL_DriverXbox360_Context;

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
static struct {
    Uint32 last_state_packet;
    SDL_Joystick *joystick;
    SDL_Joystick *last_joystick;
} guide_button_candidate;

typedef struct WindowsMatchState {
    SHORT match_axes[4];
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    WORD xinput_buttons;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    Uint32 wgi_buttons;
#endif
    SDL_bool any_data;
} WindowsMatchState;

static void HIDAPI_DriverXbox360_FillMatchState(WindowsMatchState *state, Uint32 match_state)
{
    int ii;
    state->any_data = SDL_FALSE;
    /*  SHORT state->match_axes[4] = {
            (match_state & 0x000F0000) >> 4,
            (match_state & 0x00F00000) >> 8,
            (match_state & 0x0F000000) >> 12,
            (match_state & 0xF0000000) >> 16,
        }; */
    for (ii = 0; ii < 4; ii++) {
        state->match_axes[ii] = (match_state & (0x000F0000 << (ii * 4))) >> (4 + ii * 4);
        if ((Uint32)(state->match_axes[ii] + 0x1000) > 0x2000) { /* match_state bit is not 0xF, 0x1, or 0x2 */
            state->any_data = SDL_TRUE;
        }
    }

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    /* Match axes by checking if the distance between the high 4 bits of axis and the 4 bits from match_state is 1 or less */
#define XInputAxesMatch(gamepad) (\
   (Uint32)(gamepad.sThumbLX - state->match_axes[0] + 0x1000) <= 0x2fff && \
   (Uint32)(~gamepad.sThumbLY - state->match_axes[1] + 0x1000) <= 0x2fff && \
   (Uint32)(gamepad.sThumbRX - state->match_axes[2] + 0x1000) <= 0x2fff && \
   (Uint32)(~gamepad.sThumbRY - state->match_axes[3] + 0x1000) <= 0x2fff)
    /* Explicit
#define XInputAxesMatch(gamepad) (\
    SDL_abs((Sint8)((gamepad.sThumbLX & 0xF000) >> 8) - ((match_state & 0x000F0000) >> 12)) <= 0x10 && \
    SDL_abs((Sint8)((~gamepad.sThumbLY & 0xF000) >> 8) - ((match_state & 0x00F00000) >> 16)) <= 0x10 && \
    SDL_abs((Sint8)((gamepad.sThumbRX & 0xF000) >> 8) - ((match_state & 0x0F000000) >> 20)) <= 0x10 && \
    SDL_abs((Sint8)((~gamepad.sThumbRY & 0xF000) >> 8) - ((match_state & 0xF0000000) >> 24)) <= 0x10) */


    state->xinput_buttons =
        /* Bitwise map .RLDUWVQTS.KYXBA -> YXBA..WVQTKSRLDU */
        match_state << 12 | (match_state & 0x0780) >> 1 | (match_state & 0x0010) << 1 | (match_state & 0x0040) >> 2 | (match_state & 0x7800) >> 11;
    /*  Explicit
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_A)) ? XINPUT_GAMEPAD_A : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_B)) ? XINPUT_GAMEPAD_B : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_X)) ? XINPUT_GAMEPAD_X : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_Y)) ? XINPUT_GAMEPAD_Y : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_BACK)) ? XINPUT_GAMEPAD_BACK : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_START)) ? XINPUT_GAMEPAD_START : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_LEFTSTICK)) ? XINPUT_GAMEPAD_LEFT_THUMB : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_RIGHTSTICK)) ? XINPUT_GAMEPAD_RIGHT_THUMB: 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) ? XINPUT_GAMEPAD_LEFT_SHOULDER : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) ? XINPUT_GAMEPAD_RIGHT_SHOULDER : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_UP)) ? XINPUT_GAMEPAD_DPAD_UP : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_DOWN)) ? XINPUT_GAMEPAD_DPAD_DOWN : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_LEFT)) ? XINPUT_GAMEPAD_DPAD_LEFT : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) ? XINPUT_GAMEPAD_DPAD_RIGHT : 0);
    */

    if (state->xinput_buttons)
        state->any_data = SDL_TRUE;
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    /* Match axes by checking if the distance between the high 4 bits of axis and the 4 bits from match_state is 1 or less */
#define WindowsGamingInputAxesMatch(gamepad) (\
    (Uint16)(((Sint16)(gamepad.LeftThumbstickX * SDL_MAX_SINT16) & 0xF000) - state->match_axes[0] + 0x1000) <= 0x2fff && \
    (Uint16)((~(Sint16)(gamepad.LeftThumbstickY * SDL_MAX_SINT16) & 0xF000) - state->match_axes[1] + 0x1000) <= 0x2fff && \
    (Uint16)(((Sint16)(gamepad.RightThumbstickX * SDL_MAX_SINT16) & 0xF000) - state->match_axes[2] + 0x1000) <= 0x2fff && \
    (Uint16)((~(Sint16)(gamepad.RightThumbstickY * SDL_MAX_SINT16) & 0xF000) - state->match_axes[3] + 0x1000) <= 0x2fff)


    state->wgi_buttons =
        /* Bitwise map .RLD UWVQ TS.K YXBA -> ..QT WVRL DUYX BAKS */
        /*  RStick/LStick (QT)         RShould/LShould  (WV)                 DPad R/L/D/U                          YXBA                         bac(K)                      (S)tart */
        (match_state & 0x0180) << 5 | (match_state & 0x0600) << 1 | (match_state & 0x7800) >> 5 | (match_state & 0x000F) << 2 | (match_state & 0x0010) >> 3 | (match_state & 0x0040) >> 6;
    /*  Explicit
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_A)) ? GamepadButtons_A : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_B)) ? GamepadButtons_B : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_X)) ? GamepadButtons_X : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_Y)) ? GamepadButtons_Y : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_BACK)) ? GamepadButtons_View : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_START)) ? GamepadButtons_Menu : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_LEFTSTICK)) ? GamepadButtons_LeftThumbstick : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_RIGHTSTICK)) ? GamepadButtons_RightThumbstick: 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) ? GamepadButtons_LeftShoulder: 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) ? GamepadButtons_RightShoulder: 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_UP)) ? GamepadButtons_DPadUp : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_DOWN)) ? GamepadButtons_DPadDown : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_LEFT)) ? GamepadButtons_DPadLeft : 0) |
        ((match_state & (1<<SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) ? GamepadButtons_DPadRight : 0); */

    if (state->wgi_buttons)
        state->any_data = SDL_TRUE;
#endif

}


#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
static struct {
    XINPUT_STATE_EX state;
    SDL_bool connected; /* Currently has an active XInput device */
    SDL_bool used; /* Is currently mapped to an SDL device */
    Uint8 correlation_id;
} xinput_state[XUSER_MAX_COUNT];
static SDL_bool xinput_device_change = SDL_TRUE;
static SDL_bool xinput_state_dirty = SDL_TRUE;

static void
HIDAPI_DriverXbox360_UpdateXInput()
{
    DWORD user_index;
    if (xinput_device_change) {
        for (user_index = 0; user_index < XUSER_MAX_COUNT; user_index++) {
            XINPUT_CAPABILITIES capabilities;
            xinput_state[user_index].connected = XINPUTGETCAPABILITIES(user_index, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS;
        }
        xinput_device_change = SDL_FALSE;
        xinput_state_dirty = SDL_TRUE;
    }
    if (xinput_state_dirty) {
        xinput_state_dirty = SDL_FALSE;
        for (user_index = 0; user_index < SDL_arraysize(xinput_state); ++user_index) {
            if (xinput_state[user_index].connected) {
                if (XINPUTGETSTATE(user_index, &xinput_state[user_index].state) != ERROR_SUCCESS) {
                    xinput_state[user_index].connected = SDL_FALSE;
                }
            }
        }
    }
}

static void
HIDAPI_DriverXbox360_MarkXInputSlotUsed(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_state[xinput_slot].used = SDL_TRUE;
    }
}

static void
HIDAPI_DriverXbox360_MarkXInputSlotFree(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_state[xinput_slot].used = SDL_FALSE;
    }
}
static SDL_bool
HIDAPI_DriverXbox360_MissingXInputSlot()
{
    int ii;
    for (ii = 0; ii < SDL_arraysize(xinput_state); ii++) {
        if (xinput_state[ii].connected && !xinput_state[ii].used) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_bool
HIDAPI_DriverXbox360_XInputSlotMatches(const WindowsMatchState *state, Uint8 slot_idx)
{
    if (xinput_state[slot_idx].connected) {
        WORD xinput_buttons = xinput_state[slot_idx].state.Gamepad.wButtons;
        if ((xinput_buttons & ~XINPUT_GAMEPAD_GUIDE) == state->xinput_buttons && XInputAxesMatch(xinput_state[slot_idx].state.Gamepad)) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}


static SDL_bool
HIDAPI_DriverXbox360_GuessXInputSlot(const WindowsMatchState *state, Uint8 *correlation_id, Uint8 *slot_idx)
{
    int user_index;
    int match_count;

    match_count = 0;
    for (user_index = 0; user_index < XUSER_MAX_COUNT; ++user_index) {
        if (!xinput_state[user_index].used && HIDAPI_DriverXbox360_XInputSlotMatches(state, user_index)) {
            ++match_count;
            *slot_idx = (Uint8)user_index;
            /* Incrementing correlation_id for any match, as negative evidence for others being correlated */
            *correlation_id = ++xinput_state[user_index].correlation_id;
        }
    }
    /* Only return a match if we match exactly one, and we have some non-zero data (buttons or axes) that matched.
       Note that we're still invalidating *other* potential correlations if we have more than one match or we have no
       data. */
    if (match_count == 1 && state->any_data) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT

typedef struct WindowsGamingInputGamepadState {
    __x_ABI_CWindows_CGaming_CInput_CIGamepad *gamepad;
    struct __x_ABI_CWindows_CGaming_CInput_CGamepadReading state;
    SDL_DriverXbox360_Context *correlated_context;
    SDL_bool used; /* Is currently mapped to an SDL device */
    SDL_bool connected; /* Just used during update to track disconnected */
    Uint8 correlation_id;
    struct __x_ABI_CWindows_CGaming_CInput_CGamepadVibration vibration;
} WindowsGamingInputGamepadState;

static struct {
    WindowsGamingInputGamepadState **per_gamepad;
    int per_gamepad_count;
    SDL_bool initialized;
    SDL_bool dirty;
    SDL_bool need_device_list_update;
    int ref_count;
    __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics *gamepad_statics;
} wgi_state;

static void
HIDAPI_DriverXbox360_MarkWindowsGamingInputSlotUsed(WindowsGamingInputGamepadState *wgi_slot, SDL_DriverXbox360_Context *ctx)
{
    wgi_slot->used = SDL_TRUE;
    wgi_slot->correlated_context = ctx;
}

static void
HIDAPI_DriverXbox360_MarkWindowsGamingInputSlotFree(WindowsGamingInputGamepadState *wgi_slot)
{
    wgi_slot->used = SDL_FALSE;
    wgi_slot->correlated_context = NULL;
}

static SDL_bool
HIDAPI_DriverXbox360_MissingWindowsGamingInputSlot()
{
    int ii;
    for (ii = 0; ii < wgi_state.per_gamepad_count; ii++) {
        if (!wgi_state.per_gamepad[ii]->used) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static void
HIDAPI_DriverXbox360_UpdateWindowsGamingInput()
{
    int ii;
    if (!wgi_state.gamepad_statics)
        return;

    if (!wgi_state.dirty)
        return;
    wgi_state.dirty = SDL_FALSE;

    if (wgi_state.need_device_list_update) {
        wgi_state.need_device_list_update = SDL_FALSE;
        for (ii = 0; ii < wgi_state.per_gamepad_count; ii++) {
            wgi_state.per_gamepad[ii]->connected = SDL_FALSE;
        }
        HRESULT hr;
        __FIVectorView_1_Windows__CGaming__CInput__CGamepad *gamepads;

        hr = __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_get_Gamepads(wgi_state.gamepad_statics, &gamepads);
        if (SUCCEEDED(hr)) {
            unsigned int num_gamepads;

            hr = __FIVectorView_1_Windows__CGaming__CInput__CGamepad_get_Size(gamepads, &num_gamepads);
            if (SUCCEEDED(hr)) {
                unsigned int i;
                for (i = 0; i < num_gamepads; ++i) {
                    __x_ABI_CWindows_CGaming_CInput_CIGamepad *gamepad;

                    hr = __FIVectorView_1_Windows__CGaming__CInput__CGamepad_GetAt(gamepads, i, &gamepad);
                    if (SUCCEEDED(hr)) {
                        SDL_bool found = SDL_FALSE;
                        int jj;
                        for (jj = 0; jj < wgi_state.per_gamepad_count ; jj++) {
                            if (wgi_state.per_gamepad[jj]->gamepad == gamepad) {
                                found = SDL_TRUE;
                                wgi_state.per_gamepad[jj]->connected = SDL_TRUE;
                                break;
                            }
                        }
                        if (!found) {
                            /* New device, add it */
                            wgi_state.per_gamepad_count++;
                            wgi_state.per_gamepad = SDL_realloc(wgi_state.per_gamepad, sizeof(wgi_state.per_gamepad[0]) * wgi_state.per_gamepad_count);
                            if (!wgi_state.per_gamepad) {
                                SDL_OutOfMemory();
                                return;
                            }
                            WindowsGamingInputGamepadState *gamepad_state = SDL_calloc(1, sizeof(*gamepad_state));
                            if (!gamepad_state) {
                                SDL_OutOfMemory();
                                return;
                            }
                            wgi_state.per_gamepad[wgi_state.per_gamepad_count - 1] = gamepad_state;
                            gamepad_state->gamepad = gamepad;
                            gamepad_state->connected = SDL_TRUE;
                        } else {
                            /* Already tracked */
                            __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(gamepad);
                        }
                    }
                }
                for (ii = wgi_state.per_gamepad_count - 1; ii >= 0; ii--) {
                    WindowsGamingInputGamepadState *gamepad_state = wgi_state.per_gamepad[ii];
                    if (!gamepad_state->connected) {
                        /* Device missing, must be disconnected */
                        if (gamepad_state->correlated_context) {
                            gamepad_state->correlated_context->wgi_correlated = SDL_FALSE;
                            gamepad_state->correlated_context->wgi_slot = NULL;
                        }
                        __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(gamepad_state->gamepad);
                        SDL_free(gamepad_state);
                        wgi_state.per_gamepad[ii] = wgi_state.per_gamepad[wgi_state.per_gamepad_count - 1];
                        --wgi_state.per_gamepad_count;
                    }
                }
            }
            __FIVectorView_1_Windows__CGaming__CInput__CGamepad_Release(gamepads);
        }
    } /* need_device_list_update */

    for (ii = 0; ii < wgi_state.per_gamepad_count; ii++) {
        HRESULT hr = __x_ABI_CWindows_CGaming_CInput_CIGamepad_GetCurrentReading(wgi_state.per_gamepad[ii]->gamepad, &wgi_state.per_gamepad[ii]->state);
        if (!SUCCEEDED(hr)) {
            wgi_state.per_gamepad[ii]->connected = SDL_FALSE; /* Not used by anything, currently */
        }
    }
}
static void
HIDAPI_DriverXbox360_InitWindowsGamingInput(SDL_DriverXbox360_Context *ctx)
{
    wgi_state.need_device_list_update = SDL_TRUE;
    wgi_state.ref_count++;
    if (!wgi_state.initialized) {
        /* I think this takes care of RoInitialize() in a way that is compatible with the rest of SDL */
        if (FAILED(WIN_CoInitialize())) {
            return;
        }
        wgi_state.initialized = SDL_TRUE;
        wgi_state.dirty = SDL_TRUE;

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
                    RoGetActivationFactoryFunc(hNamespaceString, &SDL_IID_IGamepadStatics, &wgi_state.gamepad_statics);
                    WindowsDeleteStringFunc(hNamespaceString);
                }
            }
            FreeLibrary(hModule);
        }
    }
}

static SDL_bool
HIDAPI_DriverXbox360_WindowsGamingInputSlotMatches(const WindowsMatchState *state, WindowsGamingInputGamepadState *slot)
{
    Uint32 wgi_buttons = slot->state.Buttons;
    if ((wgi_buttons & 0x3FFF) == state->wgi_buttons && WindowsGamingInputAxesMatch(slot->state)) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static SDL_bool
HIDAPI_DriverXbox360_GuessWindowsGamingInputSlot(const WindowsMatchState *state, Uint8 *correlation_id, WindowsGamingInputGamepadState **slot)
{
    int match_count;

    match_count = 0;
    for (int user_index = 0; user_index < wgi_state.per_gamepad_count; ++user_index) {
        WindowsGamingInputGamepadState *gamepad_state = wgi_state.per_gamepad[user_index];
        if (HIDAPI_DriverXbox360_WindowsGamingInputSlotMatches(state, gamepad_state)) {
            ++match_count;
            *slot = gamepad_state;
            /* Incrementing correlation_id for any match, as negative evidence for others being correlated */
            *correlation_id = ++gamepad_state->correlation_id;
        }
    }
    /* Only return a match if we match exactly one, and we have some non-zero data (buttons or axes) that matched.
       Note that we're still invalidating *other* potential correlations if we have more than one match or we have no
       data. */
    if (match_count == 1 && state->any_data) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static void
HIDAPI_DriverXbox360_QuitWindowsGamingInput(SDL_DriverXbox360_Context *ctx)
{
    wgi_state.need_device_list_update = SDL_TRUE;
    --wgi_state.ref_count;
    if (!wgi_state.ref_count && wgi_state.initialized) {
        for (int ii = 0; ii < wgi_state.per_gamepad_count; ii++) {
            __x_ABI_CWindows_CGaming_CInput_CIGamepad_Release(wgi_state.per_gamepad[ii]->gamepad);
        }
        if (wgi_state.per_gamepad) {
            SDL_free(wgi_state.per_gamepad);
            wgi_state.per_gamepad = NULL;
        }
        wgi_state.per_gamepad_count = 0;
        if (wgi_state.gamepad_statics) {
            __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics_Release(wgi_state.gamepad_statics);
            wgi_state.gamepad_statics = NULL;
        }
        WIN_CoUninitialize();
        wgi_state.initialized = SDL_FALSE;
    }
}

#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT */

static void
HIDAPI_DriverXbox360_PostUpdate(void)
{
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    SDL_bool unmapped_guide_pressed = SDL_FALSE;

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    if (!wgi_state.dirty) {
        int ii;
        for (ii = 0; ii < wgi_state.per_gamepad_count; ii++) {
            WindowsGamingInputGamepadState *gamepad_state = wgi_state.per_gamepad[ii];
            if (!gamepad_state->used && (gamepad_state->state.Buttons & GamepadButtons_GUIDE)) {
                unmapped_guide_pressed = SDL_TRUE;
                break;
            }
        }
    }
    wgi_state.dirty = SDL_TRUE;
#endif


#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    if (!xinput_state_dirty) {
        int ii;
        for (ii = 0; ii < SDL_arraysize(xinput_state); ii++) {
            if (xinput_state[ii].connected && !xinput_state[ii].used && (xinput_state[ii].state.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE)) {
                unmapped_guide_pressed = SDL_TRUE;
                break;
            }
        }
    }
    xinput_state_dirty = SDL_TRUE;
#endif

    if (unmapped_guide_pressed) {
        if (guide_button_candidate.joystick && !guide_button_candidate.last_joystick) {
            SDL_PrivateJoystickButton(guide_button_candidate.joystick, SDL_CONTROLLER_BUTTON_GUIDE, SDL_PRESSED);
            guide_button_candidate.last_joystick = guide_button_candidate.joystick;
        }
    } else if (guide_button_candidate.last_joystick) {
        SDL_PrivateJoystickButton(guide_button_candidate.last_joystick, SDL_CONTROLLER_BUTTON_GUIDE, SDL_RELEASED);
        guide_button_candidate.last_joystick = NULL;
    }
    guide_button_candidate.joystick = NULL;
#endif
}

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
    return HIDAPI_JoystickConnected(device, NULL, SDL_FALSE);
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

    if (device->path) { /* else opened for RAWINPUT driver */
        device->dev = hid_open_path(device->path, 0);
        if (!device->dev) {
            SDL_SetError("Couldn't open %s", device->path);
            SDL_free(ctx);
            return SDL_FALSE;
        }
    }
    device->context = ctx;

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    xinput_device_change = SDL_TRUE;
    ctx->xinput_enabled = SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_CORRELATE_XINPUT, SDL_TRUE);
    if (ctx->xinput_enabled && (WIN_LoadXInputDLL() < 0 || !XINPUTGETSTATE)) {
        ctx->xinput_enabled = SDL_FALSE;
    }
    ctx->xinput_slot = XUSER_INDEX_ANY;
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    HIDAPI_DriverXbox360_InitWindowsGamingInput(ctx);
#endif

    /* Set the controller LED */
    player_index = SDL_JoystickGetPlayerIndex(joystick);
    if (player_index >= 0 && device->dev) {
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
    if (!rumbled && ctx->wgi_correlated) {
        WindowsGamingInputGamepadState *gamepad_state = ctx->wgi_slot;
        HRESULT hr;
        gamepad_state->vibration.LeftMotor = (DOUBLE)low_frequency_rumble / SDL_MAX_UINT16;
        gamepad_state->vibration.RightMotor = (DOUBLE)high_frequency_rumble / SDL_MAX_UINT16;
        hr = __x_ABI_CWindows_CGaming_CInput_CIGamepad_put_Vibration(gamepad_state->gamepad, gamepad_state->vibration);
        if (SUCCEEDED(hr)) {
            rumbled = SDL_TRUE;
        }
    }
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    if (!rumbled && ctx->xinput_correlated) {
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
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    Uint32 match_state = ctx->match_state;
    /* Update match_state with button bit, then fall through */
#   define SDL_PrivateJoystickButton(joystick, button, state) if (state) match_state |= 1 << (button); else match_state &=~(1<<(button)); SDL_PrivateJoystickButton(joystick, button, state)
    /* Grab high 4 bits of value, then fall through */
#   define SDL_PrivateJoystickAxis(joystick, axis, value) if (axis < 4) match_state = (match_state & ~(0xF << (4 * axis + 16))) | ((value) & 0xF000) << (4 * axis + 4); SDL_PrivateJoystickAxis(joystick, axis, value)
#endif
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

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
#undef SDL_PrivateJoystickAxis
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    /* Prefer XInput over WindowsGamingInput, it continues to provide data in the background */
    if (!has_trigger_data && ctx->xinput_enabled && ctx->xinput_correlated) {
        has_trigger_data = SDL_TRUE;
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    if (!has_trigger_data && ctx->wgi_correlated) {
        has_trigger_data = SDL_TRUE;
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT */

    if (!has_trigger_data) {
        axis = (data[9] * 257) - 32768;
        if (data[9] < 0x80) {
            axis = -axis * 2 - 32769;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_MIN_SINT16);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
        } else if (data[9] > 0x80) {
            axis = axis * 2 - 32767;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_MIN_SINT16);
        } else {
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_MIN_SINT16);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_MIN_SINT16);
        }
    }

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    ctx->match_state = match_state;
    ctx->last_state_packet = SDL_GetTicks();
#undef SDL_PrivateJoystickButton
#endif
    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

#ifdef SDL_JOYSTICK_RAWINPUT
static void
HIDAPI_DriverXbox360_HandleStatePacketFromRAWINPUT(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 *data, int size)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
    HIDAPI_DriverXbox360_HandleStatePacket(joystick, NULL, ctx, data, size);
}
#endif

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

static void
HIDAPI_DriverXbox360_UpdateOtherAPIs(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
#if defined(SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT)
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
    SDL_bool has_trigger_data = SDL_FALSE;
    SDL_bool correlated = SDL_FALSE;
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    WindowsMatchState match_state_xinput;
#endif

    /* Poll for trigger data once (not per-state-packet) */
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    /* Prefer XInput over WindowsGamingInput, it continues to provide data in the background */
    if (!has_trigger_data && ctx->xinput_enabled && ctx->xinput_correlated) {
        HIDAPI_DriverXbox360_UpdateXInput();
        if (xinput_state[ctx->xinput_slot].connected) {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (xinput_state[ctx->xinput_slot].state.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ((int)xinput_state[ctx->xinput_slot].state.Gamepad.bLeftTrigger * 257) - 32768);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ((int)xinput_state[ctx->xinput_slot].state.Gamepad.bRightTrigger * 257) - 32768);
            has_trigger_data = SDL_TRUE;
        }
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    if (!has_trigger_data && ctx->wgi_correlated) {
        HIDAPI_DriverXbox360_UpdateWindowsGamingInput(); /* May detect disconnect / cause uncorrelation */
        if (ctx->wgi_correlated) { /* Still connected */
            struct __x_ABI_CWindows_CGaming_CInput_CGamepadReading *state = &ctx->wgi_slot->state;
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (state->Buttons & GamepadButtons_GUIDE) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ((int)(state->LeftTrigger * SDL_MAX_UINT16)) - 32768);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ((int)(state->RightTrigger * SDL_MAX_UINT16)) - 32768);
            has_trigger_data = SDL_TRUE;
        }
    }
#endif /* SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT */

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    HIDAPI_DriverXbox360_FillMatchState(&match_state_xinput, ctx->match_state);

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    /* Parallel logic to WINDOWS_XINPUT below */
    HIDAPI_DriverXbox360_UpdateWindowsGamingInput();
    if (ctx->wgi_correlated) {
        /* We have been previously correlated, ensure we are still matching, see comments in XINPUT section */
        if (HIDAPI_DriverXbox360_WindowsGamingInputSlotMatches(&match_state_xinput, ctx->wgi_slot)) {
            ctx->wgi_uncorrelate_count = 0;
        } else {
            ++ctx->wgi_uncorrelate_count;
            /* Only un-correlate if this is consistent over multiple Update() calls - the timing of polling/event
              pumping can easily cause this to uncorrelate for a frame.  2 seemed reliable in my testing, but
              let's set it to 3 to be safe.  An incorrect un-correlation will simply result in lower precision
              triggers for a frame. */
            if (ctx->wgi_uncorrelate_count >= 3) {
#ifdef DEBUG_JOYSTICK
                SDL_Log("UN-Correlated joystick %d to WindowsGamingInput device #%d\n", joystick->instance_id, ctx->wgi_slot);
#endif
                HIDAPI_DriverXbox360_MarkWindowsGamingInputSlotFree(ctx->wgi_slot);
                ctx->wgi_correlated = SDL_FALSE;
                ctx->wgi_correlation_count = 0;
                /* Force immediate update of triggers */
                HIDAPI_DriverXbox360_HandleStatePacket(joystick, NULL, ctx, ctx->last_state, sizeof(ctx->last_state));
                /* Force release of Guide button, it can't possibly be down on this device now. */
                /* It gets left down if we were actually correlated incorrectly and it was released on the WindowsGamingInput
                  device but we didn't get a state packet. */
                SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, SDL_RELEASED);
            }
        }
    }
    if (!ctx->wgi_correlated) {
        SDL_bool new_correlation_count = 0;
        if (HIDAPI_DriverXbox360_MissingWindowsGamingInputSlot()) {
            Uint8 correlation_id;
            WindowsGamingInputGamepadState *slot_idx;
            if (HIDAPI_DriverXbox360_GuessWindowsGamingInputSlot(&match_state_xinput, &correlation_id, &slot_idx)) {
                /* we match exactly one WindowsGamingInput device */
                /* Probably can do without wgi_correlation_count, just check and clear wgi_slot to NULL, unless we need
                   even more frames to be sure. */
                if (ctx->wgi_correlation_count && ctx->wgi_slot == slot_idx) {
                    /* was correlated previously, and still the same device */
                    if (ctx->wgi_correlation_id + 1 == correlation_id) {
                        /* no one else was correlated in the meantime */
                        new_correlation_count = ctx->wgi_correlation_count + 1;
                        if (new_correlation_count == 2) {
                            /* correlation stayed steady and uncontested across multiple frames, guaranteed match */
                            ctx->wgi_correlated = SDL_TRUE;
#ifdef DEBUG_JOYSTICK
                            SDL_Log("Correlated joystick %d to WindowsGamingInput device #%d\n", joystick->instance_id, slot_idx);
#endif
                            correlated = SDL_TRUE;
                            HIDAPI_DriverXbox360_MarkWindowsGamingInputSlotUsed(ctx->wgi_slot, ctx);
                            /* If the generalized Guide button was using us, it doesn't need to anymore */
                            if (guide_button_candidate.joystick == joystick)
                                guide_button_candidate.joystick = NULL;
                            if (guide_button_candidate.last_joystick == joystick)
                                guide_button_candidate.last_joystick = NULL;
                            /* Force immediate update of guide button / triggers */
                            HIDAPI_DriverXbox360_HandleStatePacket(joystick, NULL, ctx, ctx->last_state, sizeof(ctx->last_state));
                        }
                    } else {
                        /* someone else also possibly correlated to this device, start over */
                        new_correlation_count = 1;
                    }
                } else {
                    /* new possible correlation */
                    new_correlation_count = 1;
                    ctx->wgi_slot = slot_idx;
                }
                ctx->wgi_correlation_id = correlation_id;
            } else {
                /* Match multiple WindowsGamingInput devices, or none (possibly due to no buttons pressed) */
            }
        }
        ctx->wgi_correlation_count = new_correlation_count;
    } else {
        correlated = SDL_TRUE;
    }
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    /* Parallel logic to WINDOWS_GAMING_INPUT above */
    if (ctx->xinput_enabled) {
        HIDAPI_DriverXbox360_UpdateXInput();
        if (ctx->xinput_correlated) {
            /* We have been previously correlated, ensure we are still matching */
            /* This is required to deal with two (mostly) un-preventable mis-correlation situations:
              A) Since the HID data stream does not provide an initial state (but polling XInput does), if we open
                 5 controllers (#1-4 XInput mapped, #5 is not), and controller 1 had the A button down (and we don't
                 know), and the user presses A on controller #5, we'll see exactly 1 controller with A down (#5) and
                 exactly 1 XInput device with A down (#1), and incorrectly correlate.  This code will then un-correlate
                 when A is released from either controller #1 or #5.
              B) Since the app may not open all controllers, we could have a similar situation where only controller #5
                 is opened, and the user holds A on controllers #1 and #5 simultaneously - again we see only 1 controller
                 with A down and 1 XInput device with A down, and incorrectly correlate.  This should be very unusual
                 (only when apps do not open all controllers, yet are listening to Guide button presses, yet
                 for some reason want to ignore guide button presses on the un-opened controllers, yet users are
                 pressing buttons on the unopened controllers), and will resolve itself when either button is released
                 and we un-correlate.  We could prevent this by processing the state packets for *all* controllers,
                 even un-opened ones, as that would allow more precise correlation.
            */
            if (HIDAPI_DriverXbox360_XInputSlotMatches(&match_state_xinput, ctx->xinput_slot)) {
                ctx->xinput_uncorrelate_count = 0;
            } else {
                ++ctx->xinput_uncorrelate_count;
                /* Only un-correlate if this is consistent over multiple Update() calls - the timing of polling/event
                  pumping can easily cause this to uncorrelate for a frame.  2 seemed reliable in my testing, but
                  let's set it to 3 to be safe.  An incorrect un-correlation will simply result in lower precision
                  triggers for a frame. */
                if (ctx->xinput_uncorrelate_count >= 3) {
#ifdef DEBUG_JOYSTICK
                    SDL_Log("UN-Correlated joystick %d to XInput device #%d\n", joystick->instance_id, ctx->xinput_slot);
#endif
                    HIDAPI_DriverXbox360_MarkXInputSlotFree(ctx->xinput_slot);
                    ctx->xinput_correlated = SDL_FALSE;
                    ctx->xinput_correlation_count = 0;
                    /* Force immediate update of triggers */
                    HIDAPI_DriverXbox360_HandleStatePacket(joystick, NULL, ctx, ctx->last_state, sizeof(ctx->last_state));
                    /* Force release of Guide button, it can't possibly be down on this device now. */
                    /* It gets left down if we were actually correlated incorrectly and it was released on the XInput
                      device but we didn't get a state packet. */
                    SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, SDL_RELEASED);
                }
            }
        }
        if (!ctx->xinput_correlated) {
            SDL_bool new_correlation_count = 0;
            if (HIDAPI_DriverXbox360_MissingXInputSlot()) {
                Uint8 correlation_id;
                Uint8 slot_idx;
                if (HIDAPI_DriverXbox360_GuessXInputSlot(&match_state_xinput, &correlation_id, &slot_idx)) {
                    /* we match exactly one XInput device */
                    /* Probably can do without xinput_correlation_count, just check and clear xinput_slot to ANY, unless
                       we need even more frames to be sure */
                    if (ctx->xinput_correlation_count && ctx->xinput_slot == slot_idx) {
                        /* was correlated previously, and still the same device */
                        if (ctx->xinput_correlation_id + 1 == correlation_id) {
                            /* no one else was correlated in the meantime */
                            new_correlation_count = ctx->xinput_correlation_count + 1;
                            if (new_correlation_count == 2) {
                                /* correlation stayed steady and uncontested across multiple frames, guaranteed match */
                                ctx->xinput_correlated = SDL_TRUE;
#ifdef DEBUG_JOYSTICK
                                SDL_Log("Correlated joystick %d to XInput device #%d\n", joystick->instance_id, slot_idx);
#endif
                                correlated = SDL_TRUE;
                                HIDAPI_DriverXbox360_MarkXInputSlotUsed(ctx->xinput_slot);
                                /* If the generalized Guide button was using us, it doesn't need to anymore */
                                if (guide_button_candidate.joystick == joystick)
                                    guide_button_candidate.joystick = NULL;
                                if (guide_button_candidate.last_joystick == joystick)
                                    guide_button_candidate.last_joystick = NULL;
                                /* Force immediate update of guide button / triggers */
                                HIDAPI_DriverXbox360_HandleStatePacket(joystick, NULL, ctx, ctx->last_state, sizeof(ctx->last_state));
                            }
                        } else {
                            /* someone else also possibly correlated to this device, start over */
                            new_correlation_count = 1;
                        }
                    } else {
                        /* new possible correlation */
                        new_correlation_count = 1;
                        ctx->xinput_slot = slot_idx;
                    }
                    ctx->xinput_correlation_id = correlation_id;
                } else {
                    /* Match multiple XInput devices, or none (possibly due to no buttons pressed) */
                }
            }
            ctx->xinput_correlation_count = new_correlation_count;
        } else {
            correlated = SDL_TRUE;
        }
    }
#endif

    if (!correlated) {
        if (!guide_button_candidate.joystick ||
            (ctx->last_state_packet && (
                !guide_button_candidate.last_state_packet ||
                SDL_TICKS_PASSED(ctx->last_state_packet, guide_button_candidate.last_state_packet)
            ))
        ) {
            guide_button_candidate.joystick = joystick;
            guide_button_candidate.last_state_packet = ctx->last_state_packet;
        }
    }
#endif
#endif
}

static SDL_bool
HIDAPI_DriverXbox360_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
    }
    if (!joystick) {
        return SDL_FALSE;
    }

    while (device->dev && (size = hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
        HIDAPI_DriverXbox360_HandleStatePacket(joystick, device->dev, ctx, data, size);
    }

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, joystick->instance_id, SDL_FALSE);
    } else {
        HIDAPI_DriverXbox360_UpdateOtherAPIs(device, joystick);
    }
    
    return (size >= 0);
}

static void
HIDAPI_DriverXbox360_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
#if defined(SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT) || defined(SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT)
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)device->context;
#endif
    
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_MATCHING
    if (guide_button_candidate.joystick == joystick)
        guide_button_candidate.joystick = NULL;
    if (guide_button_candidate.last_joystick == joystick)
        guide_button_candidate.last_joystick = NULL;
#endif

#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_XINPUT
    xinput_device_change = SDL_TRUE;
    if (ctx->xinput_enabled) {
        if (ctx->xinput_correlated) {
            HIDAPI_DriverXbox360_MarkXInputSlotFree(ctx->xinput_slot);
        }
        WIN_UnloadXInputDLL();
    }
#endif
#ifdef SDL_JOYSTICK_HIDAPI_WINDOWS_GAMING_INPUT
    HIDAPI_DriverXbox360_QuitWindowsGamingInput(ctx);
#endif

    if (device->dev) {
        hid_close(device->dev);
        device->dev = NULL;
    }

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
    HIDAPI_DriverXbox360_FreeDevice,
    HIDAPI_DriverXbox360_PostUpdate,
#ifdef SDL_JOYSTICK_RAWINPUT
    HIDAPI_DriverXbox360_HandleStatePacketFromRAWINPUT,
#endif
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOX360 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
