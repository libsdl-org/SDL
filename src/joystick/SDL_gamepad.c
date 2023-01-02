/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/* This is the gamepad API for Simple DirectMedia Layer */

#include "SDL_sysjoystick.h"
#include "SDL_joystick_c.h"
#include "SDL_gamepad_c.h"
#include "SDL_gamepad_db.h"
#include "controller_type.h"
#include "usb_ids.h"
#include "hidapi/SDL_hidapi_nintendo.h"

#if !SDL_EVENTS_DISABLED
#include "../events/SDL_events_c.h"
#endif

#if defined(__ANDROID__)
#endif

/* Many gamepads turn the center button into an instantaneous button press */
#define SDL_MINIMUM_GUIDE_BUTTON_DELAY_MS 250

#define SDL_GAMEPAD_CRC_FIELD           "crc:"
#define SDL_GAMEPAD_CRC_FIELD_SIZE      4 /* hard-coded for speed */
#define SDL_GAMEPAD_PLATFORM_FIELD      "platform:"
#define SDL_GAMEPAD_PLATFORM_FIELD_SIZE SDL_strlen(SDL_GAMEPAD_PLATFORM_FIELD)
#define SDL_GAMEPAD_HINT_FIELD          "hint:"
#define SDL_GAMEPAD_HINT_FIELD_SIZE     SDL_strlen(SDL_GAMEPAD_HINT_FIELD)
#define SDL_GAMEPAD_SDKGE_FIELD         "sdk>=:"
#define SDL_GAMEPAD_SDKGE_FIELD_SIZE    SDL_strlen(SDL_GAMEPAD_SDKGE_FIELD)
#define SDL_GAMEPAD_SDKLE_FIELD         "sdk<=:"
#define SDL_GAMEPAD_SDKLE_FIELD_SIZE    SDL_strlen(SDL_GAMEPAD_SDKLE_FIELD)

/* a list of currently opened gamepads */
static SDL_Gamepad *SDL_gamepads SDL_GUARDED_BY(SDL_joystick_lock) = NULL;

typedef struct
{
    SDL_GamepadBindingType inputType;
    union
    {
        int button;

        struct
        {
            int axis;
            int axis_min;
            int axis_max;
        } axis;

        struct
        {
            int hat;
            int hat_mask;
        } hat;

    } input;

    SDL_GamepadBindingType outputType;
    union
    {
        SDL_GamepadButton button;

        struct
        {
            SDL_GamepadAxis axis;
            int axis_min;
            int axis_max;
        } axis;

    } output;

} SDL_ExtendedGamepadBind;

/* our hard coded list of mapping support */
typedef enum
{
    SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT,
    SDL_GAMEPAD_MAPPING_PRIORITY_API,
    SDL_GAMEPAD_MAPPING_PRIORITY_USER,
} SDL_GamepadMappingPriority;

#define _guarded SDL_GUARDED_BY(SDL_joystick_lock)

typedef struct GamepadMapping_t
{
    SDL_JoystickGUID guid _guarded;
    char *name _guarded;
    char *mapping _guarded;
    SDL_GamepadMappingPriority priority _guarded;
    struct GamepadMapping_t *next _guarded;
} GamepadMapping_t;

#undef _guarded

static SDL_JoystickGUID s_zeroGUID;
static GamepadMapping_t *s_pSupportedGamepads SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static GamepadMapping_t *s_pDefaultMapping SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static GamepadMapping_t *s_pXInputMapping SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static char gamepad_magic;

#define _guarded SDL_GUARDED_BY(SDL_joystick_lock)

/* The SDL gamepad structure */
struct SDL_Gamepad
{
    const void *magic _guarded;

    SDL_Joystick *joystick _guarded; /* underlying joystick device */
    int ref_count _guarded;

    const char *name _guarded;
    GamepadMapping_t *mapping _guarded;
    int num_bindings _guarded;
    SDL_ExtendedGamepadBind *bindings _guarded;
    SDL_ExtendedGamepadBind **last_match_axis _guarded;
    Uint8 *last_hat_mask _guarded;
    Uint64 guide_button_down _guarded;

    struct SDL_Gamepad *next _guarded; /* pointer to next gamepad we have allocated */
};

#undef _guarded

#define CHECK_GAMEPAD_MAGIC(gamepad, retval)                   \
    if (!gamepad || gamepad->magic != &gamepad_magic || \
        !SDL_IsJoystickValid(gamepad->joystick)) {               \
        SDL_InvalidParamError("gamepad");                             \
        SDL_UnlockJoysticks();                                               \
        return retval;                                                       \
    }

typedef struct
{
    int num_entries;
    int max_entries;
    Uint32 *entries;
} SDL_vidpid_list;

static SDL_vidpid_list SDL_allowed_gamepads;
static SDL_vidpid_list SDL_ignored_gamepads;

static void SDL_LoadVIDPIDListFromHint(const char *hint, SDL_vidpid_list *list)
{
    Uint32 entry;
    char *spot;
    char *file = NULL;

    list->num_entries = 0;

    if (hint && *hint == '@') {
        spot = file = (char *)SDL_LoadFile(hint + 1, NULL);
    } else {
        spot = (char *)hint;
    }

    if (spot == NULL) {
        return;
    }

    while ((spot = SDL_strstr(spot, "0x")) != NULL) {
        entry = (Uint16)SDL_strtol(spot, &spot, 0);
        entry <<= 16;
        spot = SDL_strstr(spot, "0x");
        if (spot == NULL) {
            break;
        }
        entry |= (Uint16)SDL_strtol(spot, &spot, 0);

        if (list->num_entries == list->max_entries) {
            int max_entries = list->max_entries + 16;
            Uint32 *entries = (Uint32 *)SDL_realloc(list->entries, max_entries * sizeof(*list->entries));
            if (entries == NULL) {
                /* Out of memory, go with what we have already */
                break;
            }
            list->entries = entries;
            list->max_entries = max_entries;
        }
        list->entries[list->num_entries++] = entry;
    }

    if (file) {
        SDL_free(file);
    }
}

static void SDLCALL SDL_GamepadIgnoreDevicesChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_LoadVIDPIDListFromHint(hint, &SDL_ignored_gamepads);
}

static void SDLCALL SDL_GamepadIgnoreDevicesExceptChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_LoadVIDPIDListFromHint(hint, &SDL_allowed_gamepads);
}

static GamepadMapping_t *SDL_PrivateAddMappingForGUID(SDL_JoystickGUID jGUID, const char *mappingString, SDL_bool *existing, SDL_GamepadMappingPriority priority);
static int SDL_SendGamepadAxis(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadAxis axis, Sint16 value);
static int SDL_SendGamepadButton(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadButton button, Uint8 state);

static SDL_bool HasSameOutput(SDL_ExtendedGamepadBind *a, SDL_ExtendedGamepadBind *b)
{
    if (a->outputType != b->outputType) {
        return SDL_FALSE;
    }

    if (a->outputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
        return a->output.axis.axis == b->output.axis.axis;
    } else {
        return a->output.button == b->output.button;
    }
}

static void ResetOutput(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_ExtendedGamepadBind *bind)
{
    if (bind->outputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
        SDL_SendGamepadAxis(timestamp, gamepad, bind->output.axis.axis, 0);
    } else {
        SDL_SendGamepadButton(timestamp, gamepad, bind->output.button, SDL_RELEASED);
    }
}

static void HandleJoystickAxis(Uint64 timestamp, SDL_Gamepad *gamepad, int axis, int value)
{
    int i;
    SDL_ExtendedGamepadBind *last_match;
    SDL_ExtendedGamepadBind *match = NULL;

    SDL_AssertJoysticksLocked();

    last_match = gamepad->last_match_axis[axis];
    for (i = 0; i < gamepad->num_bindings; ++i) {
        SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
        if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS &&
            axis == binding->input.axis.axis) {
            if (binding->input.axis.axis_min < binding->input.axis.axis_max) {
                if (value >= binding->input.axis.axis_min &&
                    value <= binding->input.axis.axis_max) {
                    match = binding;
                    break;
                }
            } else {
                if (value >= binding->input.axis.axis_max &&
                    value <= binding->input.axis.axis_min) {
                    match = binding;
                    break;
                }
            }
        }
    }

    if (last_match && (match == NULL || !HasSameOutput(last_match, match))) {
        /* Clear the last input that this axis generated */
        ResetOutput(timestamp, gamepad, last_match);
    }

    if (match) {
        if (match->outputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
            if (match->input.axis.axis_min != match->output.axis.axis_min || match->input.axis.axis_max != match->output.axis.axis_max) {
                float normalized_value = (float)(value - match->input.axis.axis_min) / (match->input.axis.axis_max - match->input.axis.axis_min);
                value = match->output.axis.axis_min + (int)(normalized_value * (match->output.axis.axis_max - match->output.axis.axis_min));
            }
            SDL_SendGamepadAxis(timestamp, gamepad, match->output.axis.axis, (Sint16)value);
        } else {
            Uint8 state;
            int threshold = match->input.axis.axis_min + (match->input.axis.axis_max - match->input.axis.axis_min) / 2;
            if (match->input.axis.axis_max < match->input.axis.axis_min) {
                state = (value <= threshold) ? SDL_PRESSED : SDL_RELEASED;
            } else {
                state = (value >= threshold) ? SDL_PRESSED : SDL_RELEASED;
            }
            SDL_SendGamepadButton(timestamp, gamepad, match->output.button, state);
        }
    }
    gamepad->last_match_axis[axis] = match;
}

static void HandleJoystickButton(Uint64 timestamp, SDL_Gamepad *gamepad, int button, Uint8 state)
{
    int i;

    SDL_AssertJoysticksLocked();

    for (i = 0; i < gamepad->num_bindings; ++i) {
        SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
        if (binding->inputType == SDL_GAMEPAD_BINDTYPE_BUTTON &&
            button == binding->input.button) {
            if (binding->outputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                int value = state ? binding->output.axis.axis_max : binding->output.axis.axis_min;
                SDL_SendGamepadAxis(timestamp, gamepad, binding->output.axis.axis, (Sint16)value);
            } else {
                SDL_SendGamepadButton(timestamp, gamepad, binding->output.button, state);
            }
            break;
        }
    }
}

static void HandleJoystickHat(Uint64 timestamp, SDL_Gamepad *gamepad, int hat, Uint8 value)
{
    int i;
    Uint8 last_mask, changed_mask;

    SDL_AssertJoysticksLocked();

    last_mask = gamepad->last_hat_mask[hat];
    changed_mask = (last_mask ^ value);
    for (i = 0; i < gamepad->num_bindings; ++i) {
        SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
        if (binding->inputType == SDL_GAMEPAD_BINDTYPE_HAT && hat == binding->input.hat.hat) {
            if ((changed_mask & binding->input.hat.hat_mask) != 0) {
                if (value & binding->input.hat.hat_mask) {
                    if (binding->outputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                        SDL_SendGamepadAxis(timestamp, gamepad, binding->output.axis.axis, (Sint16)binding->output.axis.axis_max);
                    } else {
                        SDL_SendGamepadButton(timestamp, gamepad, binding->output.button, SDL_PRESSED);
                    }
                } else {
                    ResetOutput(timestamp, gamepad, binding);
                }
            }
        }
    }
    gamepad->last_hat_mask[hat] = value;
}

/* The joystick layer will _also_ send events to recenter before disconnect,
    but it has to make (sometimes incorrect) guesses at what being "centered"
    is. The gamepad layer, however, can set a definite logical idle
    position, so set them all here. If we happened to already be at the
    center thanks to the joystick layer or idle hands, this won't generate
    duplicate events. */
static void RecenterGamepad(SDL_Gamepad *gamepad)
{
    SDL_GamepadButton button;
    SDL_GamepadAxis axis;
    Uint64 timestamp = SDL_GetTicksNS();

    for (button = (SDL_GamepadButton)0; button < SDL_GAMEPAD_BUTTON_MAX; button++) {
        if (SDL_GetGamepadButton(gamepad, button)) {
            SDL_SendGamepadButton(timestamp, gamepad, button, SDL_RELEASED);
        }
    }

    for (axis = (SDL_GamepadAxis)0; axis < SDL_GAMEPAD_AXIS_MAX; axis++) {
        if (SDL_GetGamepadAxis(gamepad, axis) != 0) {
            SDL_SendGamepadAxis(timestamp, gamepad, axis, 0);
        }
    }
}

/*
 * Event filter to fire gamepad events from joystick ones
 */
static int SDLCALL SDL_GamepadEventWatcher(void *userdata, SDL_Event *event)
{
    SDL_Gamepad *gamepad;

    switch (event->type) {
    case SDL_JOYAXISMOTION:
    {
        SDL_AssertJoysticksLocked();

        for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
            if (gamepad->joystick->instance_id == event->jaxis.which) {
                HandleJoystickAxis(event->common.timestamp, gamepad, event->jaxis.axis, event->jaxis.value);
                break;
            }
        }
    } break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    {
        SDL_AssertJoysticksLocked();

        for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
            if (gamepad->joystick->instance_id == event->jbutton.which) {
                HandleJoystickButton(event->common.timestamp, gamepad, event->jbutton.button, event->jbutton.state);
                break;
            }
        }
    } break;
    case SDL_JOYHATMOTION:
    {
        SDL_AssertJoysticksLocked();

        for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
            if (gamepad->joystick->instance_id == event->jhat.which) {
                HandleJoystickHat(event->common.timestamp, gamepad, event->jhat.hat, event->jhat.value);
                break;
            }
        }
    } break;
    case SDL_JOYDEVICEADDED:
    {
        if (SDL_IsGamepad(event->jdevice.which)) {
            SDL_Event deviceevent;

            deviceevent.type = SDL_GAMEPADADDED;
            deviceevent.common.timestamp = 0;
            deviceevent.cdevice.which = event->jdevice.which;
            SDL_PushEvent(&deviceevent);
        }
    } break;
    case SDL_JOYDEVICEREMOVED:
    {
        SDL_AssertJoysticksLocked();

        for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
            if (gamepad->joystick->instance_id == event->jdevice.which) {
                RecenterGamepad(gamepad);
                break;
            }
        }

        /* We don't know if this was a gamepad, so go ahead and send an event */
        {
            SDL_Event deviceevent;

            deviceevent.type = SDL_GAMEPADREMOVED;
            deviceevent.common.timestamp = 0;
            deviceevent.cdevice.which = event->jdevice.which;
            SDL_PushEvent(&deviceevent);
        }
    } break;
    default:
        break;
    }

    return 1;
}

#ifdef __ANDROID__
/*
 * Helper function to guess at a mapping based on the elements reported for this gamepad
 */
static GamepadMapping_t *SDL_CreateMappingForAndroidGamepad(SDL_JoystickGUID guid)
{
    const int face_button_mask = ((1 << SDL_GAMEPAD_BUTTON_A) |
                                  (1 << SDL_GAMEPAD_BUTTON_B) |
                                  (1 << SDL_GAMEPAD_BUTTON_X) |
                                  (1 << SDL_GAMEPAD_BUTTON_Y));
    SDL_bool existing;
    char mapping_string[1024];
    int button_mask;
    int axis_mask;

    button_mask = SDL_SwapLE16(*(Uint16 *)(&guid.data[sizeof(guid.data) - 4]));
    axis_mask = SDL_SwapLE16(*(Uint16 *)(&guid.data[sizeof(guid.data) - 2]));
    if (!button_mask && !axis_mask) {
        /* Accelerometer, shouldn't have a gamepad mapping */
        return NULL;
    }
    if (!(button_mask & face_button_mask)) {
        /* We don't know what buttons or axes are supported, don't make up a mapping */
        return NULL;
    }

    SDL_strlcpy(mapping_string, "none,*,", sizeof(mapping_string));

    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_A)) {
        SDL_strlcat(mapping_string, "a:b0,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_B)) {
        SDL_strlcat(mapping_string, "b:b1,", sizeof(mapping_string));
    } else if (button_mask & (1 << SDL_GAMEPAD_BUTTON_BACK)) {
        /* Use the back button as "B" for easy UI navigation with TV remotes */
        SDL_strlcat(mapping_string, "b:b4,", sizeof(mapping_string));
        button_mask &= ~(1 << SDL_GAMEPAD_BUTTON_BACK);
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_X)) {
        SDL_strlcat(mapping_string, "x:b2,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_Y)) {
        SDL_strlcat(mapping_string, "y:b3,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_BACK)) {
        SDL_strlcat(mapping_string, "back:b4,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_GUIDE)) {
        /* The guide button generally isn't functional (or acts as a home button) on most Android gamepads before Android 11 */
        if (SDL_GetAndroidSDKVersion() >= 30 /* Android 11 */) {
            SDL_strlcat(mapping_string, "guide:b5,", sizeof(mapping_string));
        }
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_START)) {
        SDL_strlcat(mapping_string, "start:b6,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_LEFT_STICK)) {
        SDL_strlcat(mapping_string, "leftstick:b7,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
        SDL_strlcat(mapping_string, "rightstick:b8,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        SDL_strlcat(mapping_string, "leftshoulder:b9,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        SDL_strlcat(mapping_string, "rightshoulder:b10,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        SDL_strlcat(mapping_string, "dpup:b11,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        SDL_strlcat(mapping_string, "dpdown:b12,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        SDL_strlcat(mapping_string, "dpleft:b13,", sizeof(mapping_string));
    }
    if (button_mask & (1 << SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        SDL_strlcat(mapping_string, "dpright:b14,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_LEFTX)) {
        SDL_strlcat(mapping_string, "leftx:a0,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_LEFTY)) {
        SDL_strlcat(mapping_string, "lefty:a1,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_RIGHTX)) {
        SDL_strlcat(mapping_string, "rightx:a2,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_RIGHTY)) {
        SDL_strlcat(mapping_string, "righty:a3,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) {
        SDL_strlcat(mapping_string, "lefttrigger:a4,", sizeof(mapping_string));
    }
    if (axis_mask & (1 << SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        SDL_strlcat(mapping_string, "righttrigger:a5,", sizeof(mapping_string));
    }

    return SDL_PrivateAddMappingForGUID(guid, mapping_string, &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
}
#endif /* __ANDROID__ */

/*
 * Helper function to guess at a mapping for HIDAPI gamepads
 */
static GamepadMapping_t *SDL_CreateMappingForHIDAPIGamepad(SDL_JoystickGUID guid)
{
    SDL_bool existing;
    char mapping_string[1024];
    Uint16 vendor;
    Uint16 product;

    SDL_strlcpy(mapping_string, "none,*,", sizeof(mapping_string));

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, NULL);

    if ((vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_GAMECUBE_ADAPTER) ||
        (vendor == USB_VENDOR_DRAGONRISE && product == USB_PRODUCT_EVORETRO_GAMECUBE_ADAPTER)) {
        /* GameCube driver has 12 buttons and 6 axes */
        SDL_strlcat(mapping_string, "a:b0,b:b1,dpdown:b6,dpleft:b4,dpright:b5,dpup:b7,lefttrigger:a4,leftx:a0,lefty:a1,rightshoulder:b9,righttrigger:a5,rightx:a2,righty:a3,start:b8,x:b2,y:b3,", sizeof(mapping_string));
    } else if (vendor == USB_VENDOR_NINTENDO &&
               guid.data[15] != k_eSwitchDeviceInfoControllerType_Unknown &&
               guid.data[15] != k_eSwitchDeviceInfoControllerType_ProController &&
               guid.data[15] != k_eWiiExtensionControllerType_Gamepad &&
               guid.data[15] != k_eWiiExtensionControllerType_WiiUPro) {
        switch (guid.data[15]) {
        case k_eSwitchDeviceInfoControllerType_NESLeft:
        case k_eSwitchDeviceInfoControllerType_NESRight:
            SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,leftshoulder:b9,rightshoulder:b10,start:b6,", sizeof(mapping_string));
            break;
        case k_eSwitchDeviceInfoControllerType_SNES:
            SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,leftshoulder:b9,lefttrigger:a4,rightshoulder:b10,righttrigger:a5,start:b6,x:b2,y:b3,", sizeof(mapping_string));
            break;
        case k_eSwitchDeviceInfoControllerType_N64:
            SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,leftshoulder:b9,leftstick:b7,lefttrigger:a4,leftx:a0,lefty:a1,rightshoulder:b10,righttrigger:a5,start:b6,x:b2,y:b3,misc1:b15,", sizeof(mapping_string));
            break;
        case k_eSwitchDeviceInfoControllerType_SEGA_Genesis:
            SDL_strlcat(mapping_string, "a:b0,b:b1,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,rightshoulder:b10,righttrigger:a5,start:b6,misc1:b15,", sizeof(mapping_string));
            break;
        case k_eWiiExtensionControllerType_None:
            SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,start:b6,x:b2,y:b3,", sizeof(mapping_string));
            break;
        case k_eWiiExtensionControllerType_Nunchuk:
        {
            /* FIXME: Should we map this to the left or right side? */
            const SDL_bool map_nunchuck_left_side = SDL_TRUE;

            if (map_nunchuck_left_side) {
                SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,leftshoulder:b9,lefttrigger:a4,leftx:a0,lefty:a1,start:b6,x:b2,y:b3,", sizeof(mapping_string));
            } else {
                SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,rightshoulder:b9,righttrigger:a4,rightx:a0,righty:a1,start:b6,x:b2,y:b3,", sizeof(mapping_string));
            }
        } break;
        default:
            if (SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, SDL_FALSE)) {
                /* Vertical mode */
                if (guid.data[15] == k_eSwitchDeviceInfoControllerType_JoyConLeft) {
                    SDL_strlcat(mapping_string, "back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,leftshoulder:b9,leftstick:b7,lefttrigger:a4,leftx:a0,lefty:a1,misc1:b15,paddle2:b17,paddle4:b19,", sizeof(mapping_string));
                } else {
                    SDL_strlcat(mapping_string, "a:b0,b:b1,guide:b5,rightshoulder:b10,rightstick:b8,righttrigger:a5,rightx:a2,righty:a3,start:b6,x:b2,y:b3,paddle1:b16,paddle3:b18,", sizeof(mapping_string));
                }
            } else {
                /* Mini gamepad mode */
                if (guid.data[15] == k_eSwitchDeviceInfoControllerType_JoyConLeft) {
                    SDL_strlcat(mapping_string, "a:b0,b:b1,guide:b5,leftshoulder:b9,leftstick:b7,leftx:a0,lefty:a1,rightshoulder:b10,start:b6,x:b2,y:b3,paddle2:b17,paddle4:b19,", sizeof(mapping_string));
                } else {
                    SDL_strlcat(mapping_string, "a:b0,b:b1,guide:b5,leftshoulder:b9,leftstick:b7,leftx:a0,lefty:a1,rightshoulder:b10,start:b6,x:b2,y:b3,paddle1:b16,paddle3:b18,", sizeof(mapping_string));
                }
            }
            break;
        }
    } else {
        /* All other gamepads have the standard set of 19 buttons and 6 axes */
        SDL_strlcat(mapping_string, "a:b0,b:b1,back:b4,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,guide:b5,leftshoulder:b9,leftstick:b7,lefttrigger:a4,leftx:a0,lefty:a1,rightshoulder:b10,rightstick:b8,righttrigger:a5,rightx:a2,righty:a3,start:b6,x:b2,y:b3,", sizeof(mapping_string));

        if (SDL_IsJoystickXboxSeriesX(vendor, product)) {
            /* XBox Series X Controllers have a share button under the guide button */
            SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));
        } else if (SDL_IsJoystickXboxOneElite(vendor, product)) {
            /* XBox One Elite Controllers have 4 back paddle buttons */
            SDL_strlcat(mapping_string, "paddle1:b15,paddle2:b17,paddle3:b16,paddle4:b18,", sizeof(mapping_string));
        } else if (SDL_IsJoystickSteamController(vendor, product)) {
            /* Steam controllers have 2 back paddle buttons */
            SDL_strlcat(mapping_string, "paddle1:b16,paddle2:b15,", sizeof(mapping_string));
        } else if (SDL_IsJoystickNintendoSwitchJoyConPair(vendor, product)) {
            /* The Nintendo Switch Joy-Con combined controllers has a share button and paddles */
            SDL_strlcat(mapping_string, "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,", sizeof(mapping_string));
        } else {
            switch (SDL_GetGamepadTypeFromGUID(guid, NULL)) {
            case SDL_GAMEPAD_TYPE_PS4:
                /* PS4 controllers have an additional touchpad button */
                SDL_strlcat(mapping_string, "touchpad:b15,", sizeof(mapping_string));
                break;
            case SDL_GAMEPAD_TYPE_PS5:
                /* PS5 controllers have a microphone button and an additional touchpad button */
                SDL_strlcat(mapping_string, "touchpad:b15,misc1:b16,", sizeof(mapping_string));
                /* DualSense Edge controllers have paddles */
                if (SDL_IsJoystickDualSenseEdge(vendor, product)) {
                    SDL_strlcat(mapping_string, "paddle1:b20,paddle2:b19,paddle3:b18,paddle4:b17,", sizeof(mapping_string));
                }
                break;
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
                /* Nintendo Switch Pro controllers have a screenshot button */
                SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));
                break;
            case SDL_GAMEPAD_TYPE_AMAZON_LUNA:
                /* Amazon Luna Controller has a mic button under the guide button */
                SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));
                break;
            case SDL_GAMEPAD_TYPE_GOOGLE_STADIA:
                /* The Google Stadia controller has a share button and a Google Assistant button */
                SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));
                break;
            case SDL_GAMEPAD_TYPE_NVIDIA_SHIELD:
                /* The NVIDIA SHIELD controller has a share button between back and start buttons */
                SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));

                if (product == USB_PRODUCT_NVIDIA_SHIELD_CONTROLLER_V103) {
                    /* The original SHIELD controller has a touchpad as well */
                    SDL_strlcat(mapping_string, "touchpad:b16,", sizeof(mapping_string));
                }
                break;
            default:
                if (vendor == 0 && product == 0) {
                    /* This is a Bluetooth Nintendo Switch Pro controller */
                    SDL_strlcat(mapping_string, "misc1:b15,", sizeof(mapping_string));
                }
                break;
            }
        }
    }

    return SDL_PrivateAddMappingForGUID(guid, mapping_string, &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
}

/*
 * Helper function to guess at a mapping for RAWINPUT gamepads
 */
static GamepadMapping_t *SDL_CreateMappingForRAWINPUTGamepad(SDL_JoystickGUID guid)
{
    SDL_bool existing;
    char mapping_string[1024];

    SDL_strlcpy(mapping_string, "none,*,", sizeof(mapping_string));
    SDL_strlcat(mapping_string, "a:b0,b:b1,x:b2,y:b3,back:b6,guide:b10,start:b7,leftstick:b8,rightstick:b9,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,", sizeof(mapping_string));

    return SDL_PrivateAddMappingForGUID(guid, mapping_string, &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
}

/*
 * Helper function to guess at a mapping for WGI gamepads
 */
static GamepadMapping_t *SDL_CreateMappingForWGIGamepad(SDL_JoystickGUID guid)
{
    SDL_bool existing;
    char mapping_string[1024];

    if (guid.data[15] != SDL_JOYSTICK_TYPE_GAMEPAD) {
        return NULL;
    }

    SDL_strlcpy(mapping_string, "none,*,", sizeof(mapping_string));
    SDL_strlcat(mapping_string, "a:b0,b:b1,x:b2,y:b3,back:b6,start:b7,leftstick:b8,rightstick:b9,leftshoulder:b4,rightshoulder:b5,dpup:b10,dpdown:b12,dpleft:b13,dpright:b11,leftx:a1,lefty:a0~,rightx:a3,righty:a2~,lefttrigger:a4,righttrigger:a5,", sizeof(mapping_string));

    return SDL_PrivateAddMappingForGUID(guid, mapping_string, &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
}

/*
 * Helper function to scan the mappings database for a gamepad with the specified GUID
 */
static GamepadMapping_t *SDL_PrivateMatchGamepadMappingForGUID(SDL_JoystickGUID guid, SDL_bool match_crc, SDL_bool match_version)
{
    GamepadMapping_t *mapping;
    Uint16 crc = 0;

    SDL_AssertJoysticksLocked();

    if (match_crc) {
        SDL_GetJoystickGUIDInfo(guid, NULL, NULL, NULL, &crc);
    }

    /* Clear the CRC from the GUID for matching, the mappings never include it in the GUID */
    SDL_SetJoystickGUIDCRC(&guid, 0);

    if (!match_version) {
        SDL_SetJoystickGUIDVersion(&guid, 0);
    }

    for (mapping = s_pSupportedGamepads; mapping; mapping = mapping->next) {
        SDL_JoystickGUID mapping_guid;

        if (SDL_memcmp(&mapping->guid, &s_zeroGUID, sizeof(mapping->guid)) == 0) {
            continue;
        }

        SDL_memcpy(&mapping_guid, &mapping->guid, sizeof(mapping_guid));
        if (!match_version) {
            SDL_SetJoystickGUIDVersion(&mapping_guid, 0);
        }

        if (SDL_memcmp(&guid, &mapping_guid, sizeof(guid)) == 0) {
            Uint16 mapping_crc = 0;

            if (match_crc) {
                const char *crc_string = SDL_strstr(mapping->mapping, SDL_GAMEPAD_CRC_FIELD);
                if (crc_string) {
                    mapping_crc = (Uint16)SDL_strtol(crc_string + SDL_GAMEPAD_CRC_FIELD_SIZE, NULL, 16);
                }
            }
            if (crc == mapping_crc) {
                return mapping;
            }
        }
    }
    return NULL;
}

/*
 * Helper function to scan the mappings database for a gamepad with the specified GUID
 */
static GamepadMapping_t *SDL_PrivateGetGamepadMappingForGUID(SDL_JoystickGUID guid, SDL_bool create_mapping)
{
    GamepadMapping_t *mapping;
    Uint16 vendor, product, crc;

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, &crc);
    if (crc) {
        /* First check for exact CRC matching */
        mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_TRUE, SDL_TRUE);
        if (mapping) {
            return mapping;
        }
    }

    /* Now check for a mapping without CRC */
    mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_FALSE, SDL_TRUE);
    if (mapping) {
        return mapping;
    }

    if (vendor && product) {
        /* Try again, ignoring the version */
        if (crc) {
            mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_TRUE, SDL_FALSE);
            if (mapping) {
                return mapping;
            }
        }

        mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_FALSE, SDL_FALSE);
        if (mapping) {
            return mapping;
        }
    }

    if (!create_mapping) {
        return NULL;
    }

#if SDL_JOYSTICK_XINPUT
    if (SDL_IsJoystickXInput(guid)) {
        /* This is an XInput device */
        return s_pXInputMapping;
    }
#endif
    if (mapping == NULL) {
        if (SDL_IsJoystickHIDAPI(guid)) {
            mapping = SDL_CreateMappingForHIDAPIGamepad(guid);
        } else if (SDL_IsJoystickRAWINPUT(guid)) {
            mapping = SDL_CreateMappingForRAWINPUTGamepad(guid);
        } else if (SDL_IsJoystickWGI(guid)) {
            mapping = SDL_CreateMappingForWGIGamepad(guid);
        } else if (SDL_IsJoystickVIRTUAL(guid)) {
            /* We'll pick up a robust mapping in VIRTUAL_JoystickGetGamepadMapping */
#ifdef __ANDROID__
        } else {
            mapping = SDL_CreateMappingForAndroidGamepad(guid);
#endif
        }
    }
    return mapping;
}

static const char *map_StringForGamepadAxis[] = {
    "leftx",
    "lefty",
    "rightx",
    "righty",
    "lefttrigger",
    "righttrigger",
    NULL
};

/*
 * convert a string to its enum equivalent
 */
SDL_GamepadAxis SDL_GetGamepadAxisFromString(const char *str)
{
    int entry;

    if (str == NULL || str[0] == '\0') {
        return SDL_GAMEPAD_AXIS_INVALID;
    }

    if (*str == '+' || *str == '-') {
        ++str;
    }

    for (entry = 0; map_StringForGamepadAxis[entry]; ++entry) {
        if (SDL_strcasecmp(str, map_StringForGamepadAxis[entry]) == 0) {
            return (SDL_GamepadAxis)entry;
        }
    }
    return SDL_GAMEPAD_AXIS_INVALID;
}

/*
 * convert an enum to its string equivalent
 */
const char *SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis)
{
    if (axis > SDL_GAMEPAD_AXIS_INVALID && axis < SDL_GAMEPAD_AXIS_MAX) {
        return map_StringForGamepadAxis[axis];
    }
    return NULL;
}

static const char *map_StringForGamepadButton[] = {
    "a",
    "b",
    "x",
    "y",
    "back",
    "guide",
    "start",
    "leftstick",
    "rightstick",
    "leftshoulder",
    "rightshoulder",
    "dpup",
    "dpdown",
    "dpleft",
    "dpright",
    "misc1",
    "paddle1",
    "paddle2",
    "paddle3",
    "paddle4",
    "touchpad",
    NULL
};

/*
 * convert a string to its enum equivalent
 */
SDL_GamepadButton SDL_GetGamepadButtonFromString(const char *str)
{
    int entry;
    if (str == NULL || str[0] == '\0') {
        return SDL_GAMEPAD_BUTTON_INVALID;
    }

    for (entry = 0; map_StringForGamepadButton[entry]; ++entry) {
        if (SDL_strcasecmp(str, map_StringForGamepadButton[entry]) == 0) {
            return (SDL_GamepadButton)entry;
        }
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

/*
 * convert an enum to its string equivalent
 */
const char *SDL_GetGamepadStringForButton(SDL_GamepadButton button)
{
    if (button > SDL_GAMEPAD_BUTTON_INVALID && button < SDL_GAMEPAD_BUTTON_MAX) {
        return map_StringForGamepadButton[button];
    }
    return NULL;
}

/*
 * given a gamepad button name and a joystick name update our mapping structure with it
 */
static void SDL_PrivateParseGamepadElement(SDL_Gamepad *gamepad, const char *szGameButton, const char *szJoystickButton)
{
    SDL_ExtendedGamepadBind bind;
    SDL_GamepadButton button;
    SDL_GamepadAxis axis;
    SDL_bool invert_input = SDL_FALSE;
    char half_axis_input = 0;
    char half_axis_output = 0;

    SDL_AssertJoysticksLocked();

    if (*szGameButton == '+' || *szGameButton == '-') {
        half_axis_output = *szGameButton++;
    }

    axis = SDL_GetGamepadAxisFromString(szGameButton);
    button = SDL_GetGamepadButtonFromString(szGameButton);
    if (axis != SDL_GAMEPAD_AXIS_INVALID) {
        bind.outputType = SDL_GAMEPAD_BINDTYPE_AXIS;
        bind.output.axis.axis = axis;
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            bind.output.axis.axis_min = 0;
            bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
        } else {
            if (half_axis_output == '+') {
                bind.output.axis.axis_min = 0;
                bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
            } else if (half_axis_output == '-') {
                bind.output.axis.axis_min = 0;
                bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MIN;
            } else {
                bind.output.axis.axis_min = SDL_JOYSTICK_AXIS_MIN;
                bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
            }
        }
    } else if (button != SDL_GAMEPAD_BUTTON_INVALID) {
        bind.outputType = SDL_GAMEPAD_BINDTYPE_BUTTON;
        bind.output.button = button;
    } else {
        SDL_SetError("Unexpected gamepad element %s", szGameButton);
        return;
    }

    if (*szJoystickButton == '+' || *szJoystickButton == '-') {
        half_axis_input = *szJoystickButton++;
    }
    if (szJoystickButton[SDL_strlen(szJoystickButton) - 1] == '~') {
        invert_input = SDL_TRUE;
    }

    if (szJoystickButton[0] == 'a' && SDL_isdigit((unsigned char)szJoystickButton[1])) {
        bind.inputType = SDL_GAMEPAD_BINDTYPE_AXIS;
        bind.input.axis.axis = SDL_atoi(&szJoystickButton[1]);
        if (half_axis_input == '+') {
            bind.input.axis.axis_min = 0;
            bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
        } else if (half_axis_input == '-') {
            bind.input.axis.axis_min = 0;
            bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MIN;
        } else {
            bind.input.axis.axis_min = SDL_JOYSTICK_AXIS_MIN;
            bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
        }
        if (invert_input) {
            int tmp = bind.input.axis.axis_min;
            bind.input.axis.axis_min = bind.input.axis.axis_max;
            bind.input.axis.axis_max = tmp;
        }
    } else if (szJoystickButton[0] == 'b' && SDL_isdigit((unsigned char)szJoystickButton[1])) {
        bind.inputType = SDL_GAMEPAD_BINDTYPE_BUTTON;
        bind.input.button = SDL_atoi(&szJoystickButton[1]);
    } else if (szJoystickButton[0] == 'h' && SDL_isdigit((unsigned char)szJoystickButton[1]) &&
               szJoystickButton[2] == '.' && SDL_isdigit((unsigned char)szJoystickButton[3])) {
        int hat = SDL_atoi(&szJoystickButton[1]);
        int mask = SDL_atoi(&szJoystickButton[3]);
        bind.inputType = SDL_GAMEPAD_BINDTYPE_HAT;
        bind.input.hat.hat = hat;
        bind.input.hat.hat_mask = mask;
    } else {
        SDL_SetError("Unexpected joystick element: %s", szJoystickButton);
        return;
    }

    ++gamepad->num_bindings;
    gamepad->bindings = (SDL_ExtendedGamepadBind *)SDL_realloc(gamepad->bindings, gamepad->num_bindings * sizeof(*gamepad->bindings));
    if (!gamepad->bindings) {
        gamepad->num_bindings = 0;
        SDL_OutOfMemory();
        return;
    }
    gamepad->bindings[gamepad->num_bindings - 1] = bind;
}

/*
 * given a gamepad mapping string update our mapping object
 */
static void SDL_PrivateParseGamepadConfigString(SDL_Gamepad *gamepad, const char *pchString)
{
    char szGameButton[20];
    char szJoystickButton[20];
    SDL_bool bGameButton = SDL_TRUE;
    int i = 0;
    const char *pchPos = pchString;

    SDL_zeroa(szGameButton);
    SDL_zeroa(szJoystickButton);

    while (pchPos && *pchPos) {
        if (*pchPos == ':') {
            i = 0;
            bGameButton = SDL_FALSE;
        } else if (*pchPos == ' ') {

        } else if (*pchPos == ',') {
            i = 0;
            bGameButton = SDL_TRUE;
            SDL_PrivateParseGamepadElement(gamepad, szGameButton, szJoystickButton);
            SDL_zeroa(szGameButton);
            SDL_zeroa(szJoystickButton);

        } else if (bGameButton) {
            if (i >= sizeof(szGameButton)) {
                SDL_SetError("Button name too large: %s", szGameButton);
                return;
            }
            szGameButton[i] = *pchPos;
            i++;
        } else {
            if (i >= sizeof(szJoystickButton)) {
                SDL_SetError("Joystick button name too large: %s", szJoystickButton);
                return;
            }
            szJoystickButton[i] = *pchPos;
            i++;
        }
        pchPos++;
    }

    /* No more values if the string was terminated by a comma. Don't report an error. */
    if (szGameButton[0] != '\0' || szJoystickButton[0] != '\0') {
        SDL_PrivateParseGamepadElement(gamepad, szGameButton, szJoystickButton);
    }
}

/*
 * Make a new button mapping struct
 */
static void SDL_PrivateLoadButtonMapping(SDL_Gamepad *gamepad, GamepadMapping_t *pGamepadMapping)
{
    int i;

    SDL_AssertJoysticksLocked();

    gamepad->name = pGamepadMapping->name;
    gamepad->num_bindings = 0;
    gamepad->mapping = pGamepadMapping;
    if (gamepad->joystick->naxes != 0 && gamepad->last_match_axis != NULL) {
        SDL_memset(gamepad->last_match_axis, 0, gamepad->joystick->naxes * sizeof(*gamepad->last_match_axis));
    }

    SDL_PrivateParseGamepadConfigString(gamepad, pGamepadMapping->mapping);

    /* Set the zero point for triggers */
    for (i = 0; i < gamepad->num_bindings; ++i) {
        SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
        if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS &&
            binding->outputType == SDL_GAMEPAD_BINDTYPE_AXIS &&
            (binding->output.axis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
             binding->output.axis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            if (binding->input.axis.axis < gamepad->joystick->naxes) {
                gamepad->joystick->axes[binding->input.axis.axis].value =
                    gamepad->joystick->axes[binding->input.axis.axis].zero = (Sint16)binding->input.axis.axis_min;
            }
        }
    }
}

/*
 * grab the guid string from a mapping string
 */
static char *SDL_PrivateGetGamepadGUIDFromMappingString(const char *pMapping)
{
    const char *pFirstComma = SDL_strchr(pMapping, ',');
    if (pFirstComma) {
        char *pchGUID = SDL_malloc(pFirstComma - pMapping + 1);
        if (pchGUID == NULL) {
            SDL_OutOfMemory();
            return NULL;
        }
        SDL_memcpy(pchGUID, pMapping, pFirstComma - pMapping);
        pchGUID[pFirstComma - pMapping] = '\0';

        /* Convert old style GUIDs to the new style in 2.0.5 */
#if defined(__WIN32__) || defined(__WINGDK__)
        if (SDL_strlen(pchGUID) == 32 &&
            SDL_memcmp(&pchGUID[20], "504944564944", 12) == 0) {
            SDL_memcpy(&pchGUID[20], "000000000000", 12);
            SDL_memcpy(&pchGUID[16], &pchGUID[4], 4);
            SDL_memcpy(&pchGUID[8], &pchGUID[0], 4);
            SDL_memcpy(&pchGUID[0], "03000000", 8);
        }
#elif __MACOS__
        if (SDL_strlen(pchGUID) == 32 &&
            SDL_memcmp(&pchGUID[4], "000000000000", 12) == 0 &&
            SDL_memcmp(&pchGUID[20], "000000000000", 12) == 0) {
            SDL_memcpy(&pchGUID[20], "000000000000", 12);
            SDL_memcpy(&pchGUID[8], &pchGUID[0], 4);
            SDL_memcpy(&pchGUID[0], "03000000", 8);
        }
#endif
        return pchGUID;
    }
    return NULL;
}

/*
 * grab the name string from a mapping string
 */
static char *SDL_PrivateGetGamepadNameFromMappingString(const char *pMapping)
{
    const char *pFirstComma, *pSecondComma;
    char *pchName;

    pFirstComma = SDL_strchr(pMapping, ',');
    if (pFirstComma == NULL) {
        return NULL;
    }

    pSecondComma = SDL_strchr(pFirstComma + 1, ',');
    if (pSecondComma == NULL) {
        return NULL;
    }

    pchName = SDL_malloc(pSecondComma - pFirstComma);
    if (pchName == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_memcpy(pchName, pFirstComma + 1, pSecondComma - pFirstComma);
    pchName[pSecondComma - pFirstComma - 1] = 0;
    return pchName;
}

/*
 * grab the button mapping string from a mapping string
 */
static char *SDL_PrivateGetGamepadMappingFromMappingString(const char *pMapping)
{
    const char *pFirstComma, *pSecondComma;

    pFirstComma = SDL_strchr(pMapping, ',');
    if (pFirstComma == NULL) {
        return NULL;
    }

    pSecondComma = SDL_strchr(pFirstComma + 1, ',');
    if (pSecondComma == NULL) {
        return NULL;
    }

    return SDL_strdup(pSecondComma + 1); /* mapping is everything after the 3rd comma */
}

/*
 * Helper function to refresh a mapping
 */
static void SDL_PrivateRefreshGamepadMapping(GamepadMapping_t *pGamepadMapping)
{
    SDL_Gamepad *gamepad;

    SDL_AssertJoysticksLocked();

    for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
        if (gamepad->mapping == pGamepadMapping) {
            SDL_PrivateLoadButtonMapping(gamepad, pGamepadMapping);

            {
                SDL_Event event;

                event.type = SDL_GAMEPADDEVICEREMAPPED;
                event.common.timestamp = 0;
                event.cdevice.which = gamepad->joystick->instance_id;
                SDL_PushEvent(&event);
            }
        }
    }
}

/*
 * Helper function to add a mapping for a guid
 */
static GamepadMapping_t *SDL_PrivateAddMappingForGUID(SDL_JoystickGUID jGUID, const char *mappingString, SDL_bool *existing, SDL_GamepadMappingPriority priority)
{
    char *pchName;
    char *pchMapping;
    GamepadMapping_t *pGamepadMapping;
    Uint16 crc;

    SDL_AssertJoysticksLocked();

    pchName = SDL_PrivateGetGamepadNameFromMappingString(mappingString);
    if (pchName == NULL) {
        SDL_SetError("Couldn't parse name from %s", mappingString);
        return NULL;
    }

    pchMapping = SDL_PrivateGetGamepadMappingFromMappingString(mappingString);
    if (pchMapping == NULL) {
        SDL_free(pchName);
        SDL_SetError("Couldn't parse %s", mappingString);
        return NULL;
    }

    /* Fix up the GUID and the mapping with the CRC, if needed */
    SDL_GetJoystickGUIDInfo(jGUID, NULL, NULL, NULL, &crc);
    if (crc) {
        /* Make sure the mapping has the CRC */
        char *new_mapping;
        char *crc_end = "";
        char *crc_string = SDL_strstr(pchMapping, SDL_GAMEPAD_CRC_FIELD);
        if (crc_string) {
            crc_end = SDL_strchr(crc_string, ',');
            if (crc_end) {
                ++crc_end;
            } else {
                crc_end = "";
            }
            *crc_string = '\0';
        }

        if (SDL_asprintf(&new_mapping, "%s%s%.4x,%s", pchMapping, SDL_GAMEPAD_CRC_FIELD, crc, crc_end) >= 0) {
            SDL_free(pchMapping);
            pchMapping = new_mapping;
        }
    } else {
        /* Make sure the GUID has the CRC, for matching purposes */
        char *crc_string = SDL_strstr(pchMapping, SDL_GAMEPAD_CRC_FIELD);
        if (crc_string) {
            crc = (Uint16)SDL_strtol(crc_string + SDL_GAMEPAD_CRC_FIELD_SIZE, NULL, 16);
            if (crc) {
                SDL_SetJoystickGUIDCRC(&jGUID, crc);
            }
        }
    }

    pGamepadMapping = SDL_PrivateGetGamepadMappingForGUID(jGUID, SDL_FALSE);
    if (pGamepadMapping) {
        /* Only overwrite the mapping if the priority is the same or higher. */
        if (pGamepadMapping->priority <= priority) {
            /* Update existing mapping */
            SDL_free(pGamepadMapping->name);
            pGamepadMapping->name = pchName;
            SDL_free(pGamepadMapping->mapping);
            pGamepadMapping->mapping = pchMapping;
            pGamepadMapping->priority = priority;
            /* refresh open gamepads */
            SDL_PrivateRefreshGamepadMapping(pGamepadMapping);
        } else {
            SDL_free(pchName);
            SDL_free(pchMapping);
        }
        *existing = SDL_TRUE;
    } else {
        pGamepadMapping = SDL_malloc(sizeof(*pGamepadMapping));
        if (pGamepadMapping == NULL) {
            SDL_free(pchName);
            SDL_free(pchMapping);
            SDL_OutOfMemory();
            return NULL;
        }
        /* Clear the CRC, we've already added it to the mapping */
        if (crc) {
            SDL_SetJoystickGUIDCRC(&jGUID, 0);
        }
        pGamepadMapping->guid = jGUID;
        pGamepadMapping->name = pchName;
        pGamepadMapping->mapping = pchMapping;
        pGamepadMapping->next = NULL;
        pGamepadMapping->priority = priority;

        if (s_pSupportedGamepads) {
            /* Add the mapping to the end of the list */
            GamepadMapping_t *pCurrMapping, *pPrevMapping;

            for (pPrevMapping = s_pSupportedGamepads, pCurrMapping = pPrevMapping->next;
                 pCurrMapping;
                 pPrevMapping = pCurrMapping, pCurrMapping = pCurrMapping->next) {
                /* continue; */
            }
            pPrevMapping->next = pGamepadMapping;
        } else {
            s_pSupportedGamepads = pGamepadMapping;
        }
        *existing = SDL_FALSE;
    }
    return pGamepadMapping;
}

/*
 * Helper function to determine pre-calculated offset to certain joystick mappings
 */
static GamepadMapping_t *SDL_PrivateGetGamepadMappingForNameAndGUID(const char *name, SDL_JoystickGUID guid)
{
    GamepadMapping_t *mapping;

    SDL_AssertJoysticksLocked();

    mapping = SDL_PrivateGetGamepadMappingForGUID(guid, SDL_TRUE);
#ifdef __LINUX__
    if (mapping == NULL && name) {
        if (SDL_strstr(name, "Xbox 360 Wireless Receiver")) {
            /* The Linux driver xpad.c maps the wireless dpad to buttons */
            SDL_bool existing;
            mapping = SDL_PrivateAddMappingForGUID(guid,
                                                   "none,X360 Wireless Controller,a:b0,b:b1,back:b6,dpdown:b14,dpleft:b11,dpright:b12,dpup:b13,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,",
                                                   &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
        } else if (SDL_strstr(name, "Xbox") || SDL_strstr(name, "X-Box") || SDL_strstr(name, "XBOX")) {
            mapping = s_pXInputMapping;
        }
    }
#endif /* __LINUX__ */

    if (mapping == NULL) {
        mapping = s_pDefaultMapping;
    }
    return mapping;
}

static void SDL_PrivateAppendToMappingString(char *mapping_string,
                                             size_t mapping_string_len,
                                             const char *input_name,
                                             SDL_InputMapping *mapping)
{
    char buffer[16];
    if (mapping->kind == EMappingKind_None) {
        return;
    }

    SDL_strlcat(mapping_string, input_name, mapping_string_len);
    SDL_strlcat(mapping_string, ":", mapping_string_len);
    switch (mapping->kind) {
    case EMappingKind_Button:
        (void)SDL_snprintf(buffer, sizeof buffer, "b%i", mapping->target);
        break;
    case EMappingKind_Axis:
        (void)SDL_snprintf(buffer, sizeof buffer, "a%i", mapping->target);
        break;
    case EMappingKind_Hat:
        (void)SDL_snprintf(buffer, sizeof buffer, "h%i.%i", mapping->target >> 4, mapping->target & 0x0F);
        break;
    default:
        SDL_assert(SDL_FALSE);
    }

    SDL_strlcat(mapping_string, buffer, mapping_string_len);
    SDL_strlcat(mapping_string, ",", mapping_string_len);
}

static GamepadMapping_t *SDL_PrivateGenerateAutomaticGamepadMapping(const char *name,
                                                                          SDL_JoystickGUID guid,
                                                                          SDL_GamepadMapping *raw_map)
{
    SDL_bool existing;
    char name_string[128];
    char mapping[1024];

    /* Remove any commas in the name */
    SDL_strlcpy(name_string, name, sizeof(name_string));
    {
        char *spot;
        for (spot = name_string; *spot; ++spot) {
            if (*spot == ',') {
                *spot = ' ';
            }
        }
    }
    (void)SDL_snprintf(mapping, sizeof mapping, "none,%s,", name_string);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "a", &raw_map->a);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "b", &raw_map->b);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "x", &raw_map->x);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "y", &raw_map->y);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "back", &raw_map->back);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "guide", &raw_map->guide);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "start", &raw_map->start);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "leftstick", &raw_map->leftstick);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "rightstick", &raw_map->rightstick);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "leftshoulder", &raw_map->leftshoulder);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "rightshoulder", &raw_map->rightshoulder);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "dpup", &raw_map->dpup);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "dpdown", &raw_map->dpdown);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "dpleft", &raw_map->dpleft);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "dpright", &raw_map->dpright);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "misc1", &raw_map->misc1);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "paddle1", &raw_map->paddle1);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "paddle2", &raw_map->paddle2);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "paddle3", &raw_map->paddle3);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "paddle4", &raw_map->paddle4);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "leftx", &raw_map->leftx);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "lefty", &raw_map->lefty);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "rightx", &raw_map->rightx);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "righty", &raw_map->righty);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "lefttrigger", &raw_map->lefttrigger);
    SDL_PrivateAppendToMappingString(mapping, sizeof(mapping), "righttrigger", &raw_map->righttrigger);

    return SDL_PrivateAddMappingForGUID(guid, mapping, &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
}

static GamepadMapping_t *SDL_PrivateGetGamepadMapping(SDL_JoystickID instance_id)
{
    const char *name;
    SDL_JoystickGUID guid;
    GamepadMapping_t *mapping;

    SDL_AssertJoysticksLocked();

    name = SDL_GetJoystickInstanceName(instance_id);
    guid = SDL_GetJoystickInstanceGUID(instance_id);
    mapping = SDL_PrivateGetGamepadMappingForNameAndGUID(name, guid);
    if (mapping == NULL) {
        SDL_GamepadMapping raw_map;

        SDL_zero(raw_map);
        if (SDL_PrivateJoystickGetAutoGamepadMapping(instance_id, &raw_map)) {
            mapping = SDL_PrivateGenerateAutomaticGamepadMapping(name, guid, &raw_map);
        }
    }

    return mapping;
}

/*
 * Add or update an entry into the Mappings Database
 */
int SDL_AddGamepadMappingsFromRW(SDL_RWops *rw, int freerw)
{
    const char *platform = SDL_GetPlatform();
    int gamepads = 0;
    char *buf, *line, *line_end, *tmp, *comma, line_platform[64];
    size_t db_size, platform_len;

    if (rw == NULL) {
        return SDL_SetError("Invalid RWops");
    }
    db_size = (size_t)SDL_RWsize(rw);

    buf = (char *)SDL_malloc(db_size + 1);
    if (buf == NULL) {
        if (freerw) {
            SDL_RWclose(rw);
        }
        return SDL_SetError("Could not allocate space to read DB into memory");
    }

    if (SDL_RWread(rw, buf, db_size) != db_size) {
        if (freerw) {
            SDL_RWclose(rw);
        }
        SDL_free(buf);
        return SDL_SetError("Could not read DB");
    }

    if (freerw) {
        SDL_RWclose(rw);
    }

    buf[db_size] = '\0';
    line = buf;

    while (line < buf + db_size) {
        line_end = SDL_strchr(line, '\n');
        if (line_end != NULL) {
            *line_end = '\0';
        } else {
            line_end = buf + db_size;
        }

        /* Extract and verify the platform */
        tmp = SDL_strstr(line, SDL_GAMEPAD_PLATFORM_FIELD);
        if (tmp != NULL) {
            tmp += SDL_GAMEPAD_PLATFORM_FIELD_SIZE;
            comma = SDL_strchr(tmp, ',');
            if (comma != NULL) {
                platform_len = comma - tmp + 1;
                if (platform_len + 1 < SDL_arraysize(line_platform)) {
                    SDL_strlcpy(line_platform, tmp, platform_len);
                    if (SDL_strncasecmp(line_platform, platform, platform_len) == 0 &&
                        SDL_AddGamepadMapping(line) > 0) {
                        gamepads++;
                    }
                }
            }
        }

        line = line_end + 1;
    }

    SDL_free(buf);
    return gamepads;
}

/*
 * Add or update an entry into the Mappings Database with a priority
 */
static int SDL_PrivateAddGamepadMapping(const char *mappingString, SDL_GamepadMappingPriority priority)
{
    char *pchGUID;
    SDL_JoystickGUID jGUID;
    SDL_bool is_default_mapping = SDL_FALSE;
    SDL_bool is_xinput_mapping = SDL_FALSE;
    SDL_bool existing = SDL_FALSE;
    GamepadMapping_t *pGamepadMapping;

    SDL_AssertJoysticksLocked();

    if (mappingString == NULL) {
        return SDL_InvalidParamError("mappingString");
    }

    { /* Extract and verify the hint field */
        const char *tmp;

        tmp = SDL_strstr(mappingString, SDL_GAMEPAD_HINT_FIELD);
        if (tmp != NULL) {
            SDL_bool default_value, value, negate;
            int len;
            char hint[128];

            tmp += SDL_GAMEPAD_HINT_FIELD_SIZE;

            if (*tmp == '!') {
                negate = SDL_TRUE;
                ++tmp;
            } else {
                negate = SDL_FALSE;
            }

            len = 0;
            while (*tmp && *tmp != ',' && *tmp != ':' && len < (sizeof(hint) - 1)) {
                hint[len++] = *tmp++;
            }
            hint[len] = '\0';

            if (tmp[0] == ':' && tmp[1] == '=') {
                tmp += 2;
                default_value = SDL_atoi(tmp);
            } else {
                default_value = SDL_FALSE;
            }

            value = SDL_GetHintBoolean(hint, default_value);
            if (negate) {
                value = !value;
            }
            if (!value) {
                return 0;
            }
        }
    }

#ifdef ANDROID
    { /* Extract and verify the SDK version */
        const char *tmp;

        tmp = SDL_strstr(mappingString, SDL_GAMEPAD_SDKGE_FIELD);
        if (tmp != NULL) {
            tmp += SDL_GAMEPAD_SDKGE_FIELD_SIZE;
            if (!(SDL_GetAndroidSDKVersion() >= SDL_atoi(tmp))) {
                return SDL_SetError("SDK version %d < minimum version %d", SDL_GetAndroidSDKVersion(), SDL_atoi(tmp));
            }
        }
        tmp = SDL_strstr(mappingString, SDL_GAMEPAD_SDKLE_FIELD);
        if (tmp != NULL) {
            tmp += SDL_GAMEPAD_SDKLE_FIELD_SIZE;
            if (!(SDL_GetAndroidSDKVersion() <= SDL_atoi(tmp))) {
                return SDL_SetError("SDK version %d > maximum version %d", SDL_GetAndroidSDKVersion(), SDL_atoi(tmp));
            }
        }
    }
#endif

    pchGUID = SDL_PrivateGetGamepadGUIDFromMappingString(mappingString);
    if (pchGUID == NULL) {
        return SDL_SetError("Couldn't parse GUID from %s", mappingString);
    }
    if (!SDL_strcasecmp(pchGUID, "default")) {
        is_default_mapping = SDL_TRUE;
    } else if (!SDL_strcasecmp(pchGUID, "xinput")) {
        is_xinput_mapping = SDL_TRUE;
    }
    jGUID = SDL_GetJoystickGUIDFromString(pchGUID);
    SDL_free(pchGUID);

    pGamepadMapping = SDL_PrivateAddMappingForGUID(jGUID, mappingString, &existing, priority);
    if (pGamepadMapping == NULL) {
        return -1;
    }

    if (existing) {
        return 0;
    } else {
        if (is_default_mapping) {
            s_pDefaultMapping = pGamepadMapping;
        } else if (is_xinput_mapping) {
            s_pXInputMapping = pGamepadMapping;
        }
        return 1;
    }
}

/*
 * Add or update an entry into the Mappings Database
 */
int SDL_AddGamepadMapping(const char *mappingString)
{
    int retval;

    SDL_LockJoysticks();
    {
        retval = SDL_PrivateAddGamepadMapping(mappingString, SDL_GAMEPAD_MAPPING_PRIORITY_API);
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 *  Get the number of mappings installed
 */
int SDL_GetNumGamepadMappings(void)
{
    int num_mappings = 0;

    SDL_LockJoysticks();
    {
        GamepadMapping_t *mapping;

        for (mapping = s_pSupportedGamepads; mapping; mapping = mapping->next) {
            if (SDL_memcmp(&mapping->guid, &s_zeroGUID, sizeof(mapping->guid)) == 0) {
                continue;
            }
            ++num_mappings;
        }
    }
    SDL_UnlockJoysticks();

    return num_mappings;
}

/*
 * Create a mapping string for a mapping
 */
static char *CreateMappingString(GamepadMapping_t *mapping, SDL_JoystickGUID guid)
{
    char *pMappingString, *pPlatformString;
    char pchGUID[33];
    size_t needed;
    const char *platform = SDL_GetPlatform();

    SDL_AssertJoysticksLocked();

    SDL_GetJoystickGUIDString(guid, pchGUID, sizeof(pchGUID));

    /* allocate enough memory for GUID + ',' + name + ',' + mapping + \0 */
    needed = SDL_strlen(pchGUID) + 1 + SDL_strlen(mapping->name) + 1 + SDL_strlen(mapping->mapping) + 1;

    if (!SDL_strstr(mapping->mapping, SDL_GAMEPAD_PLATFORM_FIELD)) {
        /* add memory for ',' + platform:PLATFORM */
        if (mapping->mapping[SDL_strlen(mapping->mapping) - 1] != ',') {
            needed += 1;
        }
        needed += SDL_GAMEPAD_PLATFORM_FIELD_SIZE + SDL_strlen(platform);
    }

    pMappingString = SDL_malloc(needed);
    if (pMappingString == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    (void)SDL_snprintf(pMappingString, needed, "%s,%s,%s", pchGUID, mapping->name, mapping->mapping);

    if (!SDL_strstr(mapping->mapping, SDL_GAMEPAD_PLATFORM_FIELD)) {
        if (mapping->mapping[SDL_strlen(mapping->mapping) - 1] != ',') {
            SDL_strlcat(pMappingString, ",", needed);
        }
        SDL_strlcat(pMappingString, SDL_GAMEPAD_PLATFORM_FIELD, needed);
        SDL_strlcat(pMappingString, platform, needed);
    }

    /* Make sure multiple platform strings haven't made their way into the mapping */
    pPlatformString = SDL_strstr(pMappingString, SDL_GAMEPAD_PLATFORM_FIELD);
    if (pPlatformString) {
        pPlatformString = SDL_strstr(pPlatformString + 1, SDL_GAMEPAD_PLATFORM_FIELD);
        if (pPlatformString) {
            *pPlatformString = '\0';
        }
    }
    return pMappingString;
}

/*
 *  Get the mapping at a particular index.
 */
char *SDL_GetGamepadMappingForIndex(int mapping_index)
{
    char *retval = NULL;

    SDL_LockJoysticks();
    {
        GamepadMapping_t *mapping;

        for (mapping = s_pSupportedGamepads; mapping; mapping = mapping->next) {
            if (SDL_memcmp(&mapping->guid, &s_zeroGUID, sizeof(mapping->guid)) == 0) {
                continue;
            }
            if (mapping_index == 0) {
                retval = CreateMappingString(mapping, mapping->guid);
                break;
            }
            --mapping_index;
        }
    }
    SDL_UnlockJoysticks();

    if (retval == NULL) {
        SDL_SetError("Mapping not available");
    }
    return retval;
}

/*
 * Get the mapping string for this GUID
 */
char *SDL_GetGamepadMappingForGUID(SDL_JoystickGUID guid)
{
    char *retval;

    SDL_LockJoysticks();
    {
        GamepadMapping_t *mapping = SDL_PrivateGetGamepadMappingForGUID(guid, SDL_TRUE);
        if (mapping) {
            retval = CreateMappingString(mapping, guid);
        } else {
            SDL_SetError("Mapping not available");
            retval = NULL;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the mapping string for this device
 */
char *SDL_GetGamepadMapping(SDL_Gamepad *gamepad)
{
    char *retval;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, NULL);

        retval = CreateMappingString(gamepad->mapping, gamepad->joystick->guid);
    }
    SDL_UnlockJoysticks();

    return retval;
}

static void SDL_LoadGamepadHints()
{
    const char *hint = SDL_GetHint(SDL_HINT_GAMECONTROLLERCONFIG);
    if (hint && hint[0]) {
        size_t nchHints = SDL_strlen(hint);
        char *pUserMappings = SDL_malloc(nchHints + 1);
        char *pTempMappings = pUserMappings;
        SDL_memcpy(pUserMappings, hint, nchHints);
        pUserMappings[nchHints] = '\0';
        while (pUserMappings) {
            char *pchNewLine = NULL;

            pchNewLine = SDL_strchr(pUserMappings, '\n');
            if (pchNewLine) {
                *pchNewLine = '\0';
            }

            SDL_PrivateAddGamepadMapping(pUserMappings, SDL_GAMEPAD_MAPPING_PRIORITY_USER);

            if (pchNewLine) {
                pUserMappings = pchNewLine + 1;
            } else {
                pUserMappings = NULL;
            }
        }
        SDL_free(pTempMappings);
    }
}

/*
 * Fill the given buffer with the expected gamepad mapping filepath.
 * Usually this will just be SDL_HINT_GAMECONTROLLERCONFIG_FILE, but for
 * Android, we want to get the internal storage path.
 */
static SDL_bool SDL_GetGamepadMappingFilePath(char *path, size_t size)
{
    const char *hint = SDL_GetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE);
    if (hint && *hint) {
        return SDL_strlcpy(path, hint, size) < size;
    }

#if defined(__ANDROID__)
    return SDL_snprintf(path, size, "%s/gamepad_map.txt", SDL_AndroidGetInternalStoragePath()) < size;
#else
    return SDL_FALSE;
#endif
}

/*
 * Initialize the gamepad system, mostly load our DB of gamepad config mappings
 */
int SDL_InitGamepadMappings(void)
{
    char szGamepadMapPath[1024];
    int i = 0;
    const char *pMappingString = NULL;

    SDL_AssertJoysticksLocked();

    pMappingString = s_GamepadMappings[i];
    while (pMappingString) {
        SDL_PrivateAddGamepadMapping(pMappingString, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);

        i++;
        pMappingString = s_GamepadMappings[i];
    }

    if (SDL_GetGamepadMappingFilePath(szGamepadMapPath, sizeof(szGamepadMapPath))) {
        SDL_AddGamepadMappingsFromFile(szGamepadMapPath);
    }

    /* load in any user supplied config */
    SDL_LoadGamepadHints();

    SDL_AddHintCallback(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES,
                        SDL_GamepadIgnoreDevicesChanged, NULL);
    SDL_AddHintCallback(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,
                        SDL_GamepadIgnoreDevicesExceptChanged, NULL);

    return 0;
}

int SDL_InitGamepads(void)
{
    int i;
    SDL_JoystickID *joysticks;

    /* Watch for joystick events and fire gamepad ones if needed */
    SDL_AddEventWatch(SDL_GamepadEventWatcher, NULL);

    /* Send added events for gamepads currently attached */
    joysticks = SDL_GetJoysticks(NULL);
    if (joysticks) {
        for (i = 0; joysticks[i]; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                SDL_Event deviceevent;
                deviceevent.type = SDL_GAMEPADADDED;
                deviceevent.common.timestamp = 0;
                deviceevent.cdevice.which = joysticks[i];
                SDL_PushEvent(&deviceevent);
            }
        }
        SDL_free(joysticks);
    }

    return 0;
}

SDL_JoystickID *SDL_GetGamepads(int *count)
{
    int num_joysticks = 0;
    int num_gamepads = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
    if (joysticks) {
        int i;
        for (i = num_joysticks - 1; i >= 0; --i) {
            if (SDL_IsGamepad(joysticks[i])) {
                ++num_gamepads;
            } else {
                SDL_memmove(&joysticks[i], &joysticks[i+1], (num_gamepads + 1) * sizeof(joysticks[i]));
            }
        }
    }
    if (count) {
        *count = num_gamepads;
    }
    return joysticks;
}

const char *SDL_GetGamepadInstanceName(SDL_JoystickID instance_id)
{
    const char *retval = NULL;

    SDL_LockJoysticks();
    {
        GamepadMapping_t *mapping = SDL_PrivateGetGamepadMapping(instance_id);
        if (mapping != NULL) {
            if (SDL_strcmp(mapping->name, "*") == 0) {
                retval = SDL_GetJoystickInstanceName(instance_id);
            } else {
                retval = mapping->name;
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

const char *SDL_GetGamepadInstancePath(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstancePath(instance_id);
}

int SDL_GetGamepadInstancePlayerIndex(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstancePlayerIndex(instance_id);
}

SDL_JoystickGUID SDL_GetGamepadInstanceGUID(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstanceGUID(instance_id);
}

Uint16 SDL_GetGamepadInstanceVendor(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstanceVendor(instance_id);
}

Uint16 SDL_GetGamepadInstanceProduct(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstanceProduct(instance_id);
}

Uint16 SDL_GetGamepadInstanceProductVersion(SDL_JoystickID instance_id)
{
    return SDL_GetJoystickInstanceProductVersion(instance_id);
}

SDL_GamepadType SDL_GetGamepadInstanceType(SDL_JoystickID instance_id)
{
    return SDL_GetGamepadTypeFromGUID(SDL_GetJoystickInstanceGUID(instance_id), SDL_GetJoystickInstanceName(instance_id));
}

char *SDL_GetGamepadInstanceMapping(SDL_JoystickID instance_id)
{
    char *retval = NULL;

    SDL_LockJoysticks();
    {
        GamepadMapping_t *mapping = SDL_PrivateGetGamepadMapping(instance_id);
        if (mapping != NULL) {
            SDL_JoystickGUID guid;
            char pchGUID[33];
            size_t needed;
            guid = SDL_GetJoystickInstanceGUID(instance_id);
            SDL_GetJoystickGUIDString(guid, pchGUID, sizeof(pchGUID));
            /* allocate enough memory for GUID + ',' + name + ',' + mapping + \0 */
            needed = SDL_strlen(pchGUID) + 1 + SDL_strlen(mapping->name) + 1 + SDL_strlen(mapping->mapping) + 1;
            retval = (char *)SDL_malloc(needed);
            if (retval != NULL) {
                (void)SDL_snprintf(retval, needed, "%s,%s,%s", pchGUID, mapping->name, mapping->mapping);
            } else {
                SDL_OutOfMemory();
            }
        }
    }
    SDL_UnlockJoysticks();
    return retval;
}

/*
 * Return 1 if the joystick with this name and GUID is a supported gamepad
 */
SDL_bool SDL_IsGamepadNameAndGUID(const char *name, SDL_JoystickGUID guid)
{
    SDL_bool retval;

    SDL_LockJoysticks();
    {
        if (SDL_PrivateGetGamepadMappingForNameAndGUID(name, guid) != NULL) {
            retval = SDL_TRUE;
        } else {
            retval = SDL_FALSE;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Return 1 if the joystick at this device index is a supported gamepad
 */
SDL_bool SDL_IsGamepad(SDL_JoystickID instance_id)
{
    SDL_bool retval;

    SDL_LockJoysticks();
    {
        if (SDL_PrivateGetGamepadMapping(instance_id) != NULL) {
            retval = SDL_TRUE;
        } else {
            retval = SDL_FALSE;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

#if defined(__LINUX__)
static SDL_bool SDL_endswith(const char *string, const char *suffix)
{
    size_t string_length = string ? SDL_strlen(string) : 0;
    size_t suffix_length = suffix ? SDL_strlen(suffix) : 0;

    if (suffix_length > 0 && suffix_length <= string_length) {
        if (SDL_memcmp(string + string_length - suffix_length, suffix, suffix_length) == 0) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}
#endif

/*
 * Return 1 if the gamepad should be ignored by SDL
 */
SDL_bool SDL_ShouldIgnoreGamepad(const char *name, SDL_JoystickGUID guid)
{
    int i;
    Uint16 vendor;
    Uint16 product;
    Uint16 version;
    Uint32 vidpid;

#if defined(__LINUX__)
    if (SDL_endswith(name, " Motion Sensors")) {
        /* Don't treat the PS3 and PS4 motion controls as a separate gamepad */
        return SDL_TRUE;
    }
    if (SDL_strncmp(name, "Nintendo ", 9) == 0 && SDL_strstr(name, " IMU") != NULL) {
        /* Don't treat the Nintendo IMU as a separate gamepad */
        return SDL_TRUE;
    }
    if (SDL_endswith(name, " Accelerometer") ||
        SDL_endswith(name, " IR") ||
        SDL_endswith(name, " Motion Plus") ||
        SDL_endswith(name, " Nunchuk")) {
        /* Don't treat the Wii extension controls as a separate gamepad */
        return SDL_TRUE;
    }
#endif

    if (name && SDL_strcmp(name, "uinput-fpc") == 0) {
        /* The Google Pixel fingerprint sensor reports itself as a joystick */
        return SDL_TRUE;
    }

    if (SDL_allowed_gamepads.num_entries == 0 &&
        SDL_ignored_gamepads.num_entries == 0) {
        return SDL_FALSE;
    }

    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, &version, NULL);

    if (SDL_GetHintBoolean("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", SDL_FALSE)) {
        /* We shouldn't ignore Steam's virtual gamepad since it's using the hints to filter out the real gamepads so it can remap input for the virtual gamepad */
        /* https://partner.steamgames.com/doc/features/steam_gamepad/steam_input_gamepad_emulation_bestpractices */
        SDL_bool bSteamVirtualGamepad = SDL_FALSE;
#if defined(__LINUX__)
        bSteamVirtualGamepad = (vendor == USB_VENDOR_VALVE && product == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD);
#elif defined(__MACOS__)
        bSteamVirtualGamepad = (vendor == USB_VENDOR_MICROSOFT && product == USB_PRODUCT_XBOX360_WIRED_CONTROLLER && version == 1);
#elif defined(__WIN32__)
        /* We can't tell on Windows, but Steam will block others in input hooks */
        bSteamVirtualGamepad = SDL_TRUE;
#endif
        if (bSteamVirtualGamepad) {
            return SDL_FALSE;
        }
    }

    vidpid = MAKE_VIDPID(vendor, product);

    if (SDL_allowed_gamepads.num_entries > 0) {
        for (i = 0; i < SDL_allowed_gamepads.num_entries; ++i) {
            if (vidpid == SDL_allowed_gamepads.entries[i]) {
                return SDL_FALSE;
            }
        }
        return SDL_TRUE;
    } else {
        for (i = 0; i < SDL_ignored_gamepads.num_entries; ++i) {
            if (vidpid == SDL_ignored_gamepads.entries[i]) {
                return SDL_TRUE;
            }
        }
        return SDL_FALSE;
    }
}

/*
 * Open a gamepad for use
 *
 * This function returns a gamepad identifier, or NULL if an error occurred.
 */
SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID instance_id)
{
    SDL_Gamepad *gamepad;
    SDL_Gamepad *gamepadlist;
    GamepadMapping_t *pSupportedGamepad = NULL;

    SDL_LockJoysticks();

    gamepadlist = SDL_gamepads;
    /* If the gamepad is already open, return it */
    while (gamepadlist != NULL) {
        if (instance_id == gamepadlist->joystick->instance_id) {
            gamepad = gamepadlist;
            ++gamepad->ref_count;
            SDL_UnlockJoysticks();
            return gamepad;
        }
        gamepadlist = gamepadlist->next;
    }

    /* Find a gamepad mapping */
    pSupportedGamepad = SDL_PrivateGetGamepadMapping(instance_id);
    if (pSupportedGamepad == NULL) {
        SDL_SetError("Couldn't find mapping for device (%" SDL_PRIu32 ")", instance_id);
        SDL_UnlockJoysticks();
        return NULL;
    }

    /* Create and initialize the gamepad */
    gamepad = (SDL_Gamepad *)SDL_calloc(1, sizeof(*gamepad));
    if (gamepad == NULL) {
        SDL_OutOfMemory();
        SDL_UnlockJoysticks();
        return NULL;
    }
    gamepad->magic = &gamepad_magic;

    gamepad->joystick = SDL_OpenJoystick(instance_id);
    if (gamepad->joystick == NULL) {
        SDL_free(gamepad);
        SDL_UnlockJoysticks();
        return NULL;
    }

    if (gamepad->joystick->naxes) {
        gamepad->last_match_axis = (SDL_ExtendedGamepadBind **)SDL_calloc(gamepad->joystick->naxes, sizeof(*gamepad->last_match_axis));
        if (!gamepad->last_match_axis) {
            SDL_OutOfMemory();
            SDL_CloseJoystick(gamepad->joystick);
            SDL_free(gamepad);
            SDL_UnlockJoysticks();
            return NULL;
        }
    }
    if (gamepad->joystick->nhats) {
        gamepad->last_hat_mask = (Uint8 *)SDL_calloc(gamepad->joystick->nhats, sizeof(*gamepad->last_hat_mask));
        if (!gamepad->last_hat_mask) {
            SDL_OutOfMemory();
            SDL_CloseJoystick(gamepad->joystick);
            SDL_free(gamepad->last_match_axis);
            SDL_free(gamepad);
            SDL_UnlockJoysticks();
            return NULL;
        }
    }

    SDL_PrivateLoadButtonMapping(gamepad, pSupportedGamepad);

    /* Add the gamepad to list */
    ++gamepad->ref_count;
    /* Link the gamepad in the list */
    gamepad->next = SDL_gamepads;
    SDL_gamepads = gamepad;

    SDL_UnlockJoysticks();

    return gamepad;
}

/*
 * Manually pump for gamepad updates.
 */
void SDL_UpdateGamepads(void)
{
    /* Just for API completeness; the joystick API does all the work. */
    SDL_UpdateJoysticks();
}

/**
 *  Return whether a gamepad has a given axis
 */
SDL_bool SDL_GamepadHasAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
    SDL_GamepadBinding bind;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, SDL_FALSE);

        bind = SDL_GetGamepadBindForAxis(gamepad, axis);
    }
    SDL_UnlockJoysticks();

    return (bind.bindType != SDL_GAMEPAD_BINDTYPE_NONE) ? SDL_TRUE : SDL_FALSE;
}

/*
 * Get the current state of an axis control on a gamepad
 */
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
    Sint16 retval = 0;

    SDL_LockJoysticks();
    {
        int i;

        CHECK_GAMEPAD_MAGIC(gamepad, 0);

        for (i = 0; i < gamepad->num_bindings; ++i) {
            SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
            if (binding->outputType == SDL_GAMEPAD_BINDTYPE_AXIS && binding->output.axis.axis == axis) {
                int value = 0;
                SDL_bool valid_input_range;
                SDL_bool valid_output_range;

                if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                    value = SDL_GetJoystickAxis(gamepad->joystick, binding->input.axis.axis);
                    if (binding->input.axis.axis_min < binding->input.axis.axis_max) {
                        valid_input_range = (value >= binding->input.axis.axis_min && value <= binding->input.axis.axis_max);
                    } else {
                        valid_input_range = (value >= binding->input.axis.axis_max && value <= binding->input.axis.axis_min);
                    }
                    if (valid_input_range) {
                        if (binding->input.axis.axis_min != binding->output.axis.axis_min || binding->input.axis.axis_max != binding->output.axis.axis_max) {
                            float normalized_value = (float)(value - binding->input.axis.axis_min) / (binding->input.axis.axis_max - binding->input.axis.axis_min);
                            value = binding->output.axis.axis_min + (int)(normalized_value * (binding->output.axis.axis_max - binding->output.axis.axis_min));
                        }
                    } else {
                        value = 0;
                    }
                } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_BUTTON) {
                    value = SDL_GetJoystickButton(gamepad->joystick, binding->input.button);
                    if (value == SDL_PRESSED) {
                        value = binding->output.axis.axis_max;
                    }
                } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_HAT) {
                    int hat_mask = SDL_GetJoystickHat(gamepad->joystick, binding->input.hat.hat);
                    if (hat_mask & binding->input.hat.hat_mask) {
                        value = binding->output.axis.axis_max;
                    }
                }

                if (binding->output.axis.axis_min < binding->output.axis.axis_max) {
                    valid_output_range = (value >= binding->output.axis.axis_min && value <= binding->output.axis.axis_max);
                } else {
                    valid_output_range = (value >= binding->output.axis.axis_max && value <= binding->output.axis.axis_min);
                }
                /* If the value is zero, there might be another binding that makes it non-zero */
                if (value != 0 && valid_output_range) {
                    retval = (Sint16)value;
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Return whether a gamepad has a given button
 */
SDL_bool SDL_GamepadHasButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    SDL_GamepadBinding bind;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, SDL_FALSE);

        bind = SDL_GetGamepadBindForButton(gamepad, button);
    }
    SDL_UnlockJoysticks();

    return (bind.bindType != SDL_GAMEPAD_BINDTYPE_NONE) ? SDL_TRUE : SDL_FALSE;
}

/*
 * Get the current state of a button on a gamepad
 */
Uint8 SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    Uint8 retval = SDL_RELEASED;

    SDL_LockJoysticks();
    {
        int i;

        CHECK_GAMEPAD_MAGIC(gamepad, 0);

        for (i = 0; i < gamepad->num_bindings; ++i) {
            SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
            if (binding->outputType == SDL_GAMEPAD_BINDTYPE_BUTTON && binding->output.button == button) {
                if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                    SDL_bool valid_input_range;

                    int value = SDL_GetJoystickAxis(gamepad->joystick, binding->input.axis.axis);
                    int threshold = binding->input.axis.axis_min + (binding->input.axis.axis_max - binding->input.axis.axis_min) / 2;
                    if (binding->input.axis.axis_min < binding->input.axis.axis_max) {
                        valid_input_range = (value >= binding->input.axis.axis_min && value <= binding->input.axis.axis_max);
                        if (valid_input_range) {
                            retval = (value >= threshold) ? SDL_PRESSED : SDL_RELEASED;
                            break;
                        }
                    } else {
                        valid_input_range = (value >= binding->input.axis.axis_max && value <= binding->input.axis.axis_min);
                        if (valid_input_range) {
                            retval = (value <= threshold) ? SDL_PRESSED : SDL_RELEASED;
                            break;
                        }
                    }
                } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_BUTTON) {
                    retval = SDL_GetJoystickButton(gamepad->joystick, binding->input.button);
                    break;
                } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_HAT) {
                    int hat_mask = SDL_GetJoystickHat(gamepad->joystick, binding->input.hat.hat);
                    retval = (hat_mask & binding->input.hat.hat_mask) ? SDL_PRESSED : SDL_RELEASED;
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Get the number of touchpads on a gamepad.
 */
int SDL_GetGamepadNumTouchpads(SDL_Gamepad *gamepad)
{
    int retval = 0;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            retval = joystick->ntouchpads;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Get the number of supported simultaneous fingers on a touchpad on a gamepad.
 */
int SDL_GetGamepadNumTouchpadFingers(SDL_Gamepad *gamepad, int touchpad)
{
    int retval = 0;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            if (touchpad >= 0 && touchpad < joystick->ntouchpads) {
                retval = joystick->touchpads[touchpad].nfingers;
            } else {
                retval = SDL_InvalidParamError("touchpad");
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Get the current state of a finger on a touchpad on a gamepad.
 */
int SDL_GetGamepadTouchpadFinger(SDL_Gamepad *gamepad, int touchpad, int finger, Uint8 *state, float *x, float *y, float *pressure)
{
    int retval = -1;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            if (touchpad >= 0 && touchpad < joystick->ntouchpads) {
                SDL_JoystickTouchpadInfo *touchpad_info = &joystick->touchpads[touchpad];
                if (finger >= 0 && finger < touchpad_info->nfingers) {
                    SDL_JoystickTouchpadFingerInfo *info = &touchpad_info->fingers[finger];

                    if (state) {
                        *state = info->state;
                    }
                    if (x) {
                        *x = info->x;
                    }
                    if (y) {
                        *y = info->y;
                    }
                    if (pressure) {
                        *pressure = info->pressure;
                    }
                    retval = 0;
                } else {
                    retval = SDL_InvalidParamError("finger");
                }
            } else {
                retval = SDL_InvalidParamError("touchpad");
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/**
 *  Return whether a gamepad has a particular sensor.
 */
SDL_bool SDL_GamepadHasSensor(SDL_Gamepad *gamepad, SDL_SensorType type)
{
    SDL_bool retval = SDL_FALSE;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            int i;
            for (i = 0; i < joystick->nsensors; ++i) {
                if (joystick->sensors[i].type == type) {
                    retval = SDL_TRUE;
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 *  Set whether data reporting for a gamepad sensor is enabled
 */
int SDL_SetGamepadSensorEnabled(SDL_Gamepad *gamepad, SDL_SensorType type, SDL_bool enabled)
{
    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            int i;
            for (i = 0; i < joystick->nsensors; ++i) {
                SDL_JoystickSensorInfo *sensor = &joystick->sensors[i];

                if (sensor->type == type) {
                    if (sensor->enabled == enabled) {
                        SDL_UnlockJoysticks();
                        return 0;
                    }

                    if (enabled) {
                        if (joystick->nsensors_enabled == 0) {
                            if (joystick->driver->SetSensorsEnabled(joystick, SDL_TRUE) < 0) {
                                SDL_UnlockJoysticks();
                                return -1;
                            }
                        }
                        ++joystick->nsensors_enabled;
                    } else {
                        if (joystick->nsensors_enabled == 1) {
                            if (joystick->driver->SetSensorsEnabled(joystick, SDL_FALSE) < 0) {
                                SDL_UnlockJoysticks();
                                return -1;
                            }
                        }
                        --joystick->nsensors_enabled;
                    }

                    sensor->enabled = enabled;
                    SDL_UnlockJoysticks();
                    return 0;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return SDL_Unsupported();
}

/*
 *  Query whether sensor data reporting is enabled for a gamepad
 */
SDL_bool SDL_GamepadSensorEnabled(SDL_Gamepad *gamepad, SDL_SensorType type)
{
    SDL_bool retval = SDL_FALSE;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            int i;
            for (i = 0; i < joystick->nsensors; ++i) {
                if (joystick->sensors[i].type == type) {
                    retval = joystick->sensors[i].enabled;
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 *  Get the data rate of a gamepad sensor.
 */
float SDL_GetGamepadSensorDataRate(SDL_Gamepad *gamepad, SDL_SensorType type)
{
    float retval = 0.0f;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            int i;
            for (i = 0; i < joystick->nsensors; ++i) {
                SDL_JoystickSensorInfo *sensor = &joystick->sensors[i];

                if (sensor->type == type) {
                    retval = sensor->rate;
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 *  Get the current state of a gamepad sensor.
 */
int SDL_GetGamepadSensorData(SDL_Gamepad *gamepad, SDL_SensorType type, float *data, int num_values)
{
    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
        if (joystick) {
            int i;
            for (i = 0; i < joystick->nsensors; ++i) {
                SDL_JoystickSensorInfo *sensor = &joystick->sensors[i];

                if (sensor->type == type) {
                    num_values = SDL_min(num_values, SDL_arraysize(sensor->data));
                    SDL_memcpy(data, sensor->data, num_values * sizeof(*data));
                    SDL_UnlockJoysticks();
                    return 0;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return SDL_Unsupported();
}

const char *SDL_GetGamepadName(SDL_Gamepad *gamepad)
{
    const char *retval = NULL;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, NULL);

        if (SDL_strcmp(gamepad->name, "*") == 0) {
            retval = SDL_GetJoystickName(gamepad->joystick);
        } else {
            retval = gamepad->name;
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

const char *SDL_GetGamepadPath(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return NULL;
    }
    return SDL_GetJoystickPath(joystick);
}

SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return SDL_GAMEPAD_TYPE_UNKNOWN;
    }
    return SDL_GetGamepadTypeFromGUID(SDL_GetJoystickGUID(joystick), SDL_GetJoystickName(joystick));
}

int SDL_GetGamepadPlayerIndex(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return -1;
    }
    return SDL_GetJoystickPlayerIndex(joystick);
}

/**
 *  Set the player index of an opened gamepad
 */
void SDL_SetGamepadPlayerIndex(SDL_Gamepad *gamepad, int player_index)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return;
    }
    SDL_SetJoystickPlayerIndex(joystick, player_index);
}

Uint16 SDL_GetGamepadVendor(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return 0;
    }
    return SDL_GetJoystickVendor(joystick);
}

Uint16 SDL_GetGamepadProduct(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return 0;
    }
    return SDL_GetJoystickProduct(joystick);
}

Uint16 SDL_GetGamepadProductVersion(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return 0;
    }
    return SDL_GetJoystickProductVersion(joystick);
}

Uint16 SDL_GetGamepadFirmwareVersion(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return 0;
    }
    return SDL_GetJoystickFirmwareVersion(joystick);
}

const char * SDL_GetGamepadSerial(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return NULL;
    }
    return SDL_GetJoystickSerial(joystick);
}

/*
 * Return if the gamepad in question is currently attached to the system,
 *  \return 0 if not plugged in, 1 if still present.
 */
SDL_bool SDL_GamepadConnected(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return SDL_FALSE;
    }
    return SDL_JoystickConnected(joystick);
}

/*
 * Get the joystick for this gamepad
 */
SDL_Joystick *SDL_GetGamepadJoystick(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, NULL);

        joystick = gamepad->joystick;
    }
    SDL_UnlockJoysticks();

    return joystick;
}

/*
 * Return the SDL_Gamepad associated with an instance id.
 */
SDL_Gamepad *SDL_GetGamepadFromInstanceID(SDL_JoystickID joyid)
{
    SDL_Gamepad *gamepad;

    SDL_LockJoysticks();
    gamepad = SDL_gamepads;
    while (gamepad) {
        if (gamepad->joystick->instance_id == joyid) {
            SDL_UnlockJoysticks();
            return gamepad;
        }
        gamepad = gamepad->next;
    }
    SDL_UnlockJoysticks();
    return NULL;
}

/**
 * Return the SDL_Gamepad associated with a player index.
 */
SDL_Gamepad *SDL_GetGamepadFromPlayerIndex(int player_index)
{
    SDL_Gamepad *retval = NULL;

    SDL_LockJoysticks();
    {
        SDL_Joystick *joystick = SDL_GetJoystickFromPlayerIndex(player_index);
        if (joystick) {
            retval = SDL_GetGamepadFromInstanceID(joystick->instance_id);
        }
    }
    SDL_UnlockJoysticks();

    return retval;
}

/*
 * Get the SDL joystick layer binding for this gamepad axis mapping
 */
SDL_GamepadBinding SDL_GetGamepadBindForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
    SDL_GamepadBinding bind;

    SDL_zero(bind);

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, bind);

        if (axis != SDL_GAMEPAD_AXIS_INVALID) {
            int i;
            for (i = 0; i < gamepad->num_bindings; ++i) {
                SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
                if (binding->outputType == SDL_GAMEPAD_BINDTYPE_AXIS && binding->output.axis.axis == axis) {
                    bind.bindType = binding->inputType;
                    if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                        /* FIXME: There might be multiple axes bound now that we have axis ranges... */
                        bind.value.axis = binding->input.axis.axis;
                    } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_BUTTON) {
                        bind.value.button = binding->input.button;
                    } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_HAT) {
                        bind.value.hat.hat = binding->input.hat.hat;
                        bind.value.hat.hat_mask = binding->input.hat.hat_mask;
                    }
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return bind;
}

/*
 * Get the SDL joystick layer binding for this gamepad button mapping
 */
SDL_GamepadBinding SDL_GetGamepadBindForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    SDL_GamepadBinding bind;

    SDL_zero(bind);

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, bind);

        if (button != SDL_GAMEPAD_BUTTON_INVALID) {
            int i;
            for (i = 0; i < gamepad->num_bindings; ++i) {
                SDL_ExtendedGamepadBind *binding = &gamepad->bindings[i];
                if (binding->outputType == SDL_GAMEPAD_BINDTYPE_BUTTON && binding->output.button == button) {
                    bind.bindType = binding->inputType;
                    if (binding->inputType == SDL_GAMEPAD_BINDTYPE_AXIS) {
                        bind.value.axis = binding->input.axis.axis;
                    } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_BUTTON) {
                        bind.value.button = binding->input.button;
                    } else if (binding->inputType == SDL_GAMEPAD_BINDTYPE_HAT) {
                        bind.value.hat.hat = binding->input.hat.hat;
                        bind.value.hat.hat_mask = binding->input.hat.hat_mask;
                    }
                    break;
                }
            }
        }
    }
    SDL_UnlockJoysticks();

    return bind;
}

int SDL_RumbleGamepad(SDL_Gamepad *gamepad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return -1;
    }
    return SDL_RumbleJoystick(joystick, low_frequency_rumble, high_frequency_rumble, duration_ms);
}

int SDL_RumbleGamepadTriggers(SDL_Gamepad *gamepad, Uint16 left_rumble, Uint16 right_rumble, Uint32 duration_ms)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return -1;
    }
    return SDL_RumbleJoystickTriggers(joystick, left_rumble, right_rumble, duration_ms);
}

SDL_bool SDL_GamepadHasLED(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return SDL_FALSE;
    }
    return SDL_JoystickHasLED(joystick);
}

SDL_bool SDL_GamepadHasRumble(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return SDL_FALSE;
    }
    return SDL_JoystickHasRumble(joystick);
}

SDL_bool SDL_GamepadHasRumbleTriggers(SDL_Gamepad *gamepad)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return SDL_FALSE;
    }
    return SDL_JoystickHasRumbleTriggers(joystick);
}

int SDL_SetGamepadLED(SDL_Gamepad *gamepad, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return -1;
    }
    return SDL_SetJoystickLED(joystick, red, green, blue);
}

int SDL_SendGamepadEffect(SDL_Gamepad *gamepad, const void *data, int size)
{
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

    if (joystick == NULL) {
        return -1;
    }
    return SDL_SendJoystickEffect(joystick, data, size);
}

void SDL_CloseGamepad(SDL_Gamepad *gamepad)
{
    SDL_Gamepad *gamepadlist, *gamepadlistprev;

    SDL_LockJoysticks();

    if (gamepad == NULL || gamepad->magic != &gamepad_magic) {
        SDL_UnlockJoysticks();
        return;
    }

    /* First decrement ref count */
    if (--gamepad->ref_count > 0) {
        SDL_UnlockJoysticks();
        return;
    }

    SDL_CloseJoystick(gamepad->joystick);

    gamepadlist = SDL_gamepads;
    gamepadlistprev = NULL;
    while (gamepadlist) {
        if (gamepad == gamepadlist) {
            if (gamepadlistprev) {
                /* unlink this entry */
                gamepadlistprev->next = gamepadlist->next;
            } else {
                SDL_gamepads = gamepad->next;
            }
            break;
        }
        gamepadlistprev = gamepadlist;
        gamepadlist = gamepadlist->next;
    }

    gamepad->magic = NULL;
    SDL_free(gamepad->bindings);
    SDL_free(gamepad->last_match_axis);
    SDL_free(gamepad->last_hat_mask);
    SDL_free(gamepad);

    SDL_UnlockJoysticks();
}

/*
 * Quit the gamepad subsystem
 */
void SDL_QuitGamepads(void)
{
    SDL_LockJoysticks();
    while (SDL_gamepads) {
        SDL_gamepads->ref_count = 1;
        SDL_CloseGamepad(SDL_gamepads);
    }
    SDL_UnlockJoysticks();
}

void SDL_QuitGamepadMappings(void)
{
    GamepadMapping_t *pGamepadMap;

    SDL_AssertJoysticksLocked();

    while (s_pSupportedGamepads) {
        pGamepadMap = s_pSupportedGamepads;
        s_pSupportedGamepads = s_pSupportedGamepads->next;
        SDL_free(pGamepadMap->name);
        SDL_free(pGamepadMap->mapping);
        SDL_free(pGamepadMap);
    }

    SDL_DelEventWatch(SDL_GamepadEventWatcher, NULL);

    SDL_DelHintCallback(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES,
                        SDL_GamepadIgnoreDevicesChanged, NULL);
    SDL_DelHintCallback(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,
                        SDL_GamepadIgnoreDevicesExceptChanged, NULL);

    if (SDL_allowed_gamepads.entries) {
        SDL_free(SDL_allowed_gamepads.entries);
        SDL_zero(SDL_allowed_gamepads);
    }
    if (SDL_ignored_gamepads.entries) {
        SDL_free(SDL_ignored_gamepads.entries);
        SDL_zero(SDL_ignored_gamepads);
    }
}

/*
 * Event filter to transform joystick events into appropriate gamepad ones
 */
static int SDL_SendGamepadAxis(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadAxis axis, Sint16 value)
{
    int posted;

    SDL_AssertJoysticksLocked();

    /* translate the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_EventEnabled(SDL_GAMEPADAXISMOTION)) {
        SDL_Event event;
        event.type = SDL_GAMEPADAXISMOTION;
        event.common.timestamp = timestamp;
        event.caxis.which = gamepad->joystick->instance_id;
        event.caxis.axis = axis;
        event.caxis.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return posted;
}

/*
 * Event filter to transform joystick events into appropriate gamepad ones
 */
static int SDL_SendGamepadButton(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadButton button, Uint8 state)
{
    int posted;
#if !SDL_EVENTS_DISABLED
    SDL_Event event;

    SDL_AssertJoysticksLocked();

    if (button == SDL_GAMEPAD_BUTTON_INVALID) {
        return 0;
    }

    switch (state) {
    case SDL_PRESSED:
        event.type = SDL_GAMEPADBUTTONDOWN;
        break;
    case SDL_RELEASED:
        event.type = SDL_GAMEPADBUTTONUP;
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }
#endif /* !SDL_EVENTS_DISABLED */

    if (button == SDL_GAMEPAD_BUTTON_GUIDE) {
        Uint64 now = SDL_GetTicks();
        if (state == SDL_PRESSED) {
            gamepad->guide_button_down = now;

            if (gamepad->joystick->delayed_guide_button) {
                /* Skip duplicate press */
                return 0;
            }
        } else {
            if (now < (gamepad->guide_button_down + SDL_MINIMUM_GUIDE_BUTTON_DELAY_MS)) {
                gamepad->joystick->delayed_guide_button = SDL_TRUE;
                return 0;
            }
            gamepad->joystick->delayed_guide_button = SDL_FALSE;
        }
    }

    /* translate the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_EventEnabled(event.type)) {
        event.common.timestamp = timestamp;
        event.cbutton.which = gamepad->joystick->instance_id;
        event.cbutton.button = button;
        event.cbutton.state = state;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return posted;
}

static const Uint32 SDL_gamepad_event_list[] = {
    SDL_GAMEPADAXISMOTION,
    SDL_GAMEPADBUTTONDOWN,
    SDL_GAMEPADBUTTONUP,
    SDL_GAMEPADADDED,
    SDL_GAMEPADREMOVED,
    SDL_GAMEPADDEVICEREMAPPED,
    SDL_GAMEPADTOUCHPADDOWN,
    SDL_GAMEPADTOUCHPADMOTION,
    SDL_GAMEPADTOUCHPADUP,
    SDL_GAMEPADSENSORUPDATE,
};

void SDL_SetGamepadEventsEnabled(SDL_bool enabled)
{
#ifndef SDL_EVENTS_DISABLED
    unsigned int i;

    for (i = 0; i < SDL_arraysize(SDL_gamepad_event_list); ++i) {
        SDL_SetEventEnabled(SDL_gamepad_event_list[i], enabled);
    }
#endif /* !SDL_EVENTS_DISABLED */
}

SDL_bool SDL_GamepadEventsEnabled(void)
{
    SDL_bool enabled = SDL_FALSE;

#ifndef SDL_EVENTS_DISABLED
    unsigned int i;

    for (i = 0; i < SDL_arraysize(SDL_gamepad_event_list); ++i) {
        enabled = SDL_EventEnabled(SDL_gamepad_event_list[i]);
        if (enabled) {
            break;
        }
    }
#endif /* SDL_EVENTS_DISABLED */

    return enabled;
}

void SDL_GamepadHandleDelayedGuideButton(SDL_Joystick *joystick)
{
    SDL_Gamepad *gamepad;

    SDL_AssertJoysticksLocked();

    for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
        if (gamepad->joystick == joystick) {
            SDL_SendGamepadButton(0, gamepad, SDL_GAMEPAD_BUTTON_GUIDE, SDL_RELEASED);
            break;
        }
    }
}

const char *SDL_GetGamepadAppleSFSymbolsNameForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
#if defined(SDL_JOYSTICK_MFI)
    const char *IOS_GetAppleSFSymbolsNameForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);
    const char *retval;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, NULL);

        retval = IOS_GetAppleSFSymbolsNameForButton(gamepad, button);
    }
    SDL_UnlockJoysticks();

    return retval;
#else
    return NULL;
#endif
}

const char *SDL_GetGamepadAppleSFSymbolsNameForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
#if defined(SDL_JOYSTICK_MFI)
    const char *IOS_GetAppleSFSymbolsNameForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);
    const char *retval;

    SDL_LockJoysticks();
    {
        CHECK_GAMEPAD_MAGIC(gamepad, NULL);

        retval = IOS_GetAppleSFSymbolsNameForAxis(gamepad, axis);
    }
    SDL_UnlockJoysticks();

    return retval;
#else
    return NULL;
#endif
}
