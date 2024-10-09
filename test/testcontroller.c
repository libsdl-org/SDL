/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program to test the SDL controller routines */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_font.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include "gamepadutils.h"
#include "testutils.h"

#if 0
#define DEBUG_AXIS_MAPPING
#endif

#define TITLE_HEIGHT 48.0f
#define PANEL_SPACING 25.0f
#define PANEL_WIDTH 250.0f
#define MINIMUM_BUTTON_WIDTH 96.0f
#define BUTTON_MARGIN 16.0f
#define BUTTON_PADDING 12.0f
#define GAMEPAD_WIDTH 512.0f
#define GAMEPAD_HEIGHT 560.0f

#define SCREEN_WIDTH  (PANEL_WIDTH + PANEL_SPACING + GAMEPAD_WIDTH + PANEL_SPACING + PANEL_WIDTH)
#define SCREEN_HEIGHT (TITLE_HEIGHT + GAMEPAD_HEIGHT)

typedef struct
{
    bool m_bMoving;
    int m_nLastValue;
    int m_nStartingValue;
    int m_nFarthestValue;
} AxisState;

typedef struct
{
    SDL_JoystickID id;

    SDL_Joystick *joystick;
    int num_axes;
    AxisState *axis_state;

    SDL_Gamepad *gamepad;
    char *mapping;
    bool has_bindings;

    int audio_route;
    int trigger_effect;
} Controller;

static SDL_Window *window = NULL;
static SDL_Renderer *screen = NULL;
static ControllerDisplayMode display_mode = CONTROLLER_MODE_TESTING;
static GamepadImage *image = NULL;
static GamepadDisplay *gamepad_elements = NULL;
static GamepadTypeDisplay *gamepad_type = NULL;
static JoystickDisplay *joystick_elements = NULL;
static GamepadButton *setup_mapping_button = NULL;
static GamepadButton *done_mapping_button = NULL;
static GamepadButton *cancel_button = NULL;
static GamepadButton *clear_button = NULL;
static GamepadButton *copy_button = NULL;
static GamepadButton *paste_button = NULL;
static char *backup_mapping = NULL;
static bool done = false;
static bool set_LED = false;
static int num_controllers = 0;
static Controller *controllers;
static Controller *controller;
static SDL_JoystickID mapping_controller = 0;
static int binding_element = SDL_GAMEPAD_ELEMENT_INVALID;
static int last_binding_element = SDL_GAMEPAD_ELEMENT_INVALID;
static bool binding_flow = false;
static int binding_flow_direction = 0;
static Uint64 binding_advance_time = 0;
static SDL_FRect title_area;
static bool title_highlighted;
static bool title_pressed;
static SDL_FRect type_area;
static bool type_highlighted;
static bool type_pressed;
static char *controller_name;
static SDL_Joystick *virtual_joystick = NULL;
static SDL_GamepadAxis virtual_axis_active = SDL_GAMEPAD_AXIS_INVALID;
static float virtual_axis_start_x;
static float virtual_axis_start_y;
static SDL_GamepadButton virtual_button_active = SDL_GAMEPAD_BUTTON_INVALID;
static bool virtual_touchpad_active = false;
static float virtual_touchpad_x;
static float virtual_touchpad_y;

static int s_arrBindingOrder[] = {
    /* Standard sequence */
    SDL_GAMEPAD_BUTTON_SOUTH,
    SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_WEST,
    SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER,
    SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_ELEMENT_INVALID,

    /* Paddle sequence */
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    SDL_GAMEPAD_ELEMENT_INVALID,
};


static const char *GetSensorName(SDL_SensorType sensor)
{
    switch (sensor) {
    case SDL_SENSOR_ACCEL:
        return "accelerometer";
    case SDL_SENSOR_GYRO:
        return "gyro";
    case SDL_SENSOR_ACCEL_L:
        return "accelerometer (L)";
    case SDL_SENSOR_GYRO_L:
        return "gyro (L)";
    case SDL_SENSOR_ACCEL_R:
        return "accelerometer (R)";
    case SDL_SENSOR_GYRO_R:
        return "gyro (R)";
    default:
        return "UNKNOWN";
    }
}

/* PS5 trigger effect documentation:
   https://controllers.fandom.com/wiki/Sony_DualSense#FFB_Trigger_Modes
*/
typedef struct
{
    Uint8 ucEnableBits1;              /* 0 */
    Uint8 ucEnableBits2;              /* 1 */
    Uint8 ucRumbleRight;              /* 2 */
    Uint8 ucRumbleLeft;               /* 3 */
    Uint8 ucHeadphoneVolume;          /* 4 */
    Uint8 ucSpeakerVolume;            /* 5 */
    Uint8 ucMicrophoneVolume;         /* 6 */
    Uint8 ucAudioEnableBits;          /* 7 */
    Uint8 ucMicLightMode;             /* 8 */
    Uint8 ucAudioMuteBits;            /* 9 */
    Uint8 rgucRightTriggerEffect[11]; /* 10 */
    Uint8 rgucLeftTriggerEffect[11];  /* 21 */
    Uint8 rgucUnknown1[6];            /* 32 */
    Uint8 ucLedFlags;                 /* 38 */
    Uint8 rgucUnknown2[2];            /* 39 */
    Uint8 ucLedAnim;                  /* 41 */
    Uint8 ucLedBrightness;            /* 42 */
    Uint8 ucPadLights;                /* 43 */
    Uint8 ucLedRed;                   /* 44 */
    Uint8 ucLedGreen;                 /* 45 */
    Uint8 ucLedBlue;                  /* 46 */
} DS5EffectsState_t;

static void CyclePS5AudioRoute(Controller *device)
{
    DS5EffectsState_t state;

    device->audio_route = (device->audio_route + 1) % 4;

    SDL_zero(state);
    switch (device->audio_route) {
    case 0:
        /* Audio disabled */
        state.ucEnableBits1 |= (0x80 | 0x20 | 0x10); /* Modify audio route and speaker / headphone volume */
        state.ucSpeakerVolume = 0;                   /* Minimum volume */
        state.ucHeadphoneVolume = 0;                 /* Minimum volume */
        state.ucAudioEnableBits = 0x00;              /* Output to headphones */
        break;
    case 1:
        /* Headphones */
        state.ucEnableBits1 |= (0x80 | 0x10); /* Modify audio route and headphone volume */
        state.ucHeadphoneVolume = 50;         /* 50% volume - don't blast into the ears */
        state.ucAudioEnableBits = 0x00;       /* Output to headphones */
        break;
    case 2:
        /* Speaker */
        state.ucEnableBits1 |= (0x80 | 0x20); /* Modify audio route and speaker volume */
        state.ucSpeakerVolume = 100;          /* Maximum volume */
        state.ucAudioEnableBits = 0x30;       /* Output to speaker */
        break;
    case 3:
        /* Both */
        state.ucEnableBits1 |= (0x80 | 0x20 | 0x10); /* Modify audio route and speaker / headphone volume */
        state.ucSpeakerVolume = 100;                 /* Maximum volume */
        state.ucHeadphoneVolume = 50;                /* 50% volume - don't blast into the ears */
        state.ucAudioEnableBits = 0x20;              /* Output to both speaker and headphones */
        break;
    }
    SDL_SendGamepadEffect(device->gamepad, &state, sizeof(state));
}

static void CyclePS5TriggerEffect(Controller *device)
{
    DS5EffectsState_t state;

    Uint8 effects[3][11] = {
        /* Clear trigger effect */
        { 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        /* Constant resistance across entire trigger pull */
        { 0x01, 0, 110, 0, 0, 0, 0, 0, 0, 0, 0 },
        /* Resistance and vibration when trigger is pulled */
        { 0x06, 15, 63, 128, 0, 0, 0, 0, 0, 0, 0 },
    };

    device->trigger_effect = (device->trigger_effect + 1) % SDL_arraysize(effects);

    SDL_zero(state);
    state.ucEnableBits1 |= (0x04 | 0x08); /* Modify right and left trigger effect respectively */
    SDL_memcpy(state.rgucRightTriggerEffect, effects[device->trigger_effect], sizeof(effects[0]));
    SDL_memcpy(state.rgucLeftTriggerEffect, effects[device->trigger_effect], sizeof(effects[0]));
    SDL_SendGamepadEffect(device->gamepad, &state, sizeof(state));
}

static void ClearButtonHighlights(void)
{
    title_highlighted = false;
    title_pressed = false;

    type_highlighted = false;
    type_pressed = false;

    ClearGamepadImage(image);
    SetGamepadDisplayHighlight(gamepad_elements, SDL_GAMEPAD_ELEMENT_INVALID, false);
    SetGamepadTypeDisplayHighlight(gamepad_type, SDL_GAMEPAD_TYPE_UNSELECTED, false);
    SetGamepadButtonHighlight(setup_mapping_button, false, false);
    SetGamepadButtonHighlight(done_mapping_button, false, false);
    SetGamepadButtonHighlight(cancel_button, false, false);
    SetGamepadButtonHighlight(clear_button, false, false);
    SetGamepadButtonHighlight(copy_button, false, false);
    SetGamepadButtonHighlight(paste_button, false, false);
}

static void UpdateButtonHighlights(float x, float y, bool button_down)
{
    ClearButtonHighlights();

    if (display_mode == CONTROLLER_MODE_TESTING) {
        SetGamepadButtonHighlight(setup_mapping_button, GamepadButtonContains(setup_mapping_button, x, y), button_down);
    } else if (display_mode == CONTROLLER_MODE_BINDING) {
        SDL_FPoint point;
        int gamepad_highlight_element = SDL_GAMEPAD_ELEMENT_INVALID;
        char *joystick_highlight_element;

        point.x = x;
        point.y = y;
        if (SDL_PointInRectFloat(&point, &title_area)) {
            title_highlighted = true;
            title_pressed = button_down;
        } else {
            title_highlighted = false;
            title_pressed = false;
        }

        if (SDL_PointInRectFloat(&point, &type_area)) {
            type_highlighted = true;
            type_pressed = button_down;
        } else {
            type_highlighted = false;
            type_pressed = false;
        }

        if (controller->joystick != virtual_joystick) {
            gamepad_highlight_element = GetGamepadImageElementAt(image, x, y);
        }
        if (gamepad_highlight_element == SDL_GAMEPAD_ELEMENT_INVALID) {
            gamepad_highlight_element = GetGamepadDisplayElementAt(gamepad_elements, controller->gamepad, x, y);
        }
        SetGamepadDisplayHighlight(gamepad_elements, gamepad_highlight_element, button_down);

        if (binding_element == SDL_GAMEPAD_ELEMENT_TYPE) {
            int gamepad_highlight_type = GetGamepadTypeDisplayAt(gamepad_type, x, y);
            SetGamepadTypeDisplayHighlight(gamepad_type, gamepad_highlight_type, button_down);
        }

        joystick_highlight_element = GetJoystickDisplayElementAt(joystick_elements, controller->joystick, x, y);
        SetJoystickDisplayHighlight(joystick_elements, joystick_highlight_element, button_down);
        SDL_free(joystick_highlight_element);

        SetGamepadButtonHighlight(done_mapping_button, GamepadButtonContains(done_mapping_button, x, y), button_down);
        SetGamepadButtonHighlight(cancel_button, GamepadButtonContains(cancel_button, x, y), button_down);
        SetGamepadButtonHighlight(clear_button, GamepadButtonContains(clear_button, x, y), button_down);
        SetGamepadButtonHighlight(copy_button, GamepadButtonContains(copy_button, x, y), button_down);
        SetGamepadButtonHighlight(paste_button, GamepadButtonContains(paste_button, x, y), button_down);
    }
}

static int StandardizeAxisValue(int nValue)
{
    if (nValue > SDL_JOYSTICK_AXIS_MAX / 2) {
        return SDL_JOYSTICK_AXIS_MAX;
    } else if (nValue < SDL_JOYSTICK_AXIS_MIN / 2) {
        return SDL_JOYSTICK_AXIS_MIN;
    } else {
        return 0;
    }
}

static void RefreshControllerName(void)
{
    const char *name = NULL;

    SDL_free(controller_name);
    controller_name = NULL;

    if (controller) {
        if (controller->gamepad) {
            name = SDL_GetGamepadName(controller->gamepad);
        } else {
            name = SDL_GetJoystickName(controller->joystick);
        }
    }

    if (name) {
        controller_name = SDL_strdup(name);
    } else {
        controller_name = SDL_strdup("");
    }
}

static void SetAndFreeGamepadMapping(char *mapping)
{
    SDL_SetGamepadMapping(controller->id, mapping);
    SDL_free(mapping);
}

static void SetCurrentBindingElement(int element, bool flow)
{
    int i;

    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
        RefreshControllerName();
    }

    if (element == SDL_GAMEPAD_ELEMENT_INVALID) {
        binding_flow_direction = 0;
        last_binding_element = SDL_GAMEPAD_ELEMENT_INVALID;
    } else {
        last_binding_element = binding_element;
    }
    binding_element = element;
    binding_flow = flow || (element == SDL_GAMEPAD_BUTTON_SOUTH);
    binding_advance_time = 0;

    for (i = 0; i < controller->num_axes; ++i) {
        controller->axis_state[i].m_nFarthestValue = controller->axis_state[i].m_nStartingValue;
    }

    SetGamepadDisplaySelected(gamepad_elements, element);
}

static void SetNextBindingElement(void)
{
    int i;

    if (binding_element == SDL_GAMEPAD_ELEMENT_INVALID) {
        return;
    }

    for (i = 0; i < SDL_arraysize(s_arrBindingOrder); ++i) {
        if (binding_element == s_arrBindingOrder[i]) {
            binding_flow_direction = 1;
            SetCurrentBindingElement(s_arrBindingOrder[i + 1], true);
            return;
        }
    }
    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_INVALID, false);
}

static void SetPrevBindingElement(void)
{
    int i;

    if (binding_element == SDL_GAMEPAD_ELEMENT_INVALID) {
        return;
    }

    for (i = 1; i < SDL_arraysize(s_arrBindingOrder); ++i) {
        if (binding_element == s_arrBindingOrder[i]) {
            binding_flow_direction = -1;
            SetCurrentBindingElement(s_arrBindingOrder[i - 1], true);
            return;
        }
    }
    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_INVALID, false);
}

static void StopBinding(void)
{
    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_INVALID, false);
}

typedef struct
{
    int axis;
    int direction;
} AxisInfo;

static bool ParseAxisInfo(const char *description, AxisInfo *info)
{
    if (!description) {
        return false;
    }

    if (*description == '-') {
        info->direction = -1;
        ++description;
    } else if (*description == '+') {
        info->direction = 1;
        ++description;
    } else {
        info->direction = 0;
    }

    if (description[0] == 'a' && SDL_isdigit(description[1])) {
        ++description;
        info->axis = SDL_atoi(description);
        return true;
    }
    return false;
}

static void CommitBindingElement(const char *binding, bool force)
{
    char *mapping;
    int direction = 1;
    bool ignore_binding = false;

    if (binding_element == SDL_GAMEPAD_ELEMENT_INVALID) {
        return;
    }

    if (controller->mapping) {
        mapping = SDL_strdup(controller->mapping);
    } else {
        mapping = NULL;
    }

    /* If the controller generates multiple events for a single element, pick the best one */
    if (!force && binding_advance_time) {
        char *current = GetElementBinding(mapping, binding_element);
        bool native_button = (binding_element < SDL_GAMEPAD_BUTTON_COUNT);
        bool native_axis = (binding_element >= SDL_GAMEPAD_BUTTON_COUNT &&
                                binding_element <= SDL_GAMEPAD_ELEMENT_AXIS_MAX);
        bool native_trigger = (binding_element == SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER ||
                                   binding_element == SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER);
        bool native_dpad = (binding_element == SDL_GAMEPAD_BUTTON_DPAD_UP ||
                                binding_element == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
                                binding_element == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
                                binding_element == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

        if (native_button) {
            bool current_button = (current && *current == 'b');
            bool proposed_button = (binding && *binding == 'b');
            if (current_button && !proposed_button) {
                ignore_binding = true;
            }
            /* Use the lower index button (we map from lower to higher button index) */
            if (current_button && proposed_button && current[1] < binding[1]) {
                ignore_binding = true;
            }
        }
        if (native_axis) {
            AxisInfo current_axis_info  = { 0, 0 };
            AxisInfo proposed_axis_info = { 0, 0 };
            bool current_axis = ParseAxisInfo(current, &current_axis_info);
            bool proposed_axis = ParseAxisInfo(binding, &proposed_axis_info);

            if (current_axis) {
                /* Ignore this unless the proposed binding extends the existing axis */
                ignore_binding = true;

                if (native_trigger &&
                    ((*current == '-' && *binding == '+' &&
                      SDL_strcmp(current + 1, binding + 1) == 0) ||
                     (*current == '+' && *binding == '-' &&
                      SDL_strcmp(current + 1, binding + 1) == 0))) {
                    /* Merge two half axes into a whole axis for a trigger */
                    ++binding;
                    ignore_binding = false;
                }

                /* Use the lower index axis (we map from lower to higher axis index) */
                if (proposed_axis && proposed_axis_info.axis < current_axis_info.axis) {
                    ignore_binding = false;
                }
            }
        }
        if (native_dpad) {
            bool current_hat = (current && *current == 'h');
            bool proposed_hat = (binding && *binding == 'h');
            if (current_hat && !proposed_hat) {
                ignore_binding = true;
            }
            /* Use the lower index hat (we map from lower to higher hat index) */
            if (current_hat && proposed_hat && current[1] < binding[1]) {
                ignore_binding = true;
            }
        }
        SDL_free(current);
    }

    if (!ignore_binding && binding_flow && !force) {
        int existing = GetElementForBinding(mapping, binding);
        if (existing != SDL_GAMEPAD_ELEMENT_INVALID) {
            SDL_GamepadButton action_forward = SDL_GAMEPAD_BUTTON_SOUTH;
            SDL_GamepadButton action_backward = SDL_GAMEPAD_BUTTON_EAST;
            SDL_GamepadButton action_delete = SDL_GAMEPAD_BUTTON_WEST;
            if (binding_element == action_forward) {
                /* Bind it! */
            } else if (binding_element == action_backward) {
                if (existing == action_forward) {
                    bool bound_backward = MappingHasElement(controller->mapping, action_backward);
                    if (bound_backward) {
                        /* Just move on to the next one */
                        ignore_binding = true;
                        SetNextBindingElement();
                    } else {
                        /* You can't skip the backward action, go back and start over */
                        ignore_binding = true;
                        SetPrevBindingElement();
                    }
                } else if (existing == action_backward && binding_flow_direction == -1) {
                    /* Keep going backwards */
                    ignore_binding = true;
                    SetPrevBindingElement();
                } else {
                    /* Bind it! */
                }
            } else if (existing == action_forward) {
                /* Just move on to the next one */
                ignore_binding = true;
                SetNextBindingElement();
            } else if (existing == action_backward) {
                ignore_binding = true;
                SetPrevBindingElement();
            } else if (existing == binding_element) {
                /* We're rebinding the same thing, just move to the next one */
                ignore_binding = true;
                SetNextBindingElement();
            } else if (existing == action_delete) {
                /* Clear the current binding and move to the next one */
                binding = NULL;
                direction = 1;
                force = true;
            } else if (binding_element != action_forward &&
                       binding_element != action_backward) {
                /* Actually, we'll just clear the existing binding */
                /*ignore_binding = true;*/
            }
        }
    }

    if (ignore_binding) {
        SDL_free(mapping);
        return;
    }

    mapping = ClearMappingBinding(mapping, binding);
    mapping = SetElementBinding(mapping, binding_element, binding);
    SetAndFreeGamepadMapping(mapping);

    if (force) {
        if (binding_flow) {
            if (direction > 0) {
                SetNextBindingElement();
            } else if (direction < 0) {
                SetPrevBindingElement();
            }
        } else {
            StopBinding();
        }
    } else {
        /* Wait to see if any more bindings come in */
        binding_advance_time = SDL_GetTicks() + 30;
    }
}

static void ClearBinding(void)
{
    CommitBindingElement(NULL, true);
}

static void SetDisplayMode(ControllerDisplayMode mode)
{
    float x, y;
    SDL_MouseButtonFlags button_state;

    if (mode == CONTROLLER_MODE_BINDING) {
        /* Make a backup of the current mapping */
        if (controller->mapping) {
            backup_mapping = SDL_strdup(controller->mapping);
        }
        mapping_controller = controller->id;
        if (MappingHasBindings(backup_mapping)) {
            SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_INVALID, false);
        } else {
            SetCurrentBindingElement(SDL_GAMEPAD_BUTTON_SOUTH, true);
        }
    } else {
        if (backup_mapping) {
            SDL_free(backup_mapping);
            backup_mapping = NULL;
        }
        mapping_controller = 0;
        StopBinding();
    }

    display_mode = mode;
    SetGamepadImageDisplayMode(image, mode);
    SetGamepadDisplayDisplayMode(gamepad_elements, mode);

    button_state = SDL_GetMouseState(&x, &y);
    SDL_RenderCoordinatesFromWindow(screen, x, y, &x, &y);
    UpdateButtonHighlights(x, y, button_state ? true : false);
}

static void CancelMapping(void)
{
    SetAndFreeGamepadMapping(backup_mapping);
    backup_mapping = NULL;

    SetDisplayMode(CONTROLLER_MODE_TESTING);
}

static void ClearMapping(void)
{
    SetAndFreeGamepadMapping(NULL);
    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_INVALID, false);
}

static void CopyMapping(void)
{
    if (controller && controller->mapping) {
        SDL_SetClipboardText(controller->mapping);
    }
}

static void PasteMapping(void)
{
    if (controller) {
        char *mapping = SDL_GetClipboardText();
        if (MappingHasBindings(mapping)) {
            StopBinding();
            SDL_SetGamepadMapping(controller->id, mapping);
            RefreshControllerName();
        } else {
            /* Not a valid mapping, ignore it */
        }
        SDL_free(mapping);
    }
}

static void CommitControllerName(void)
{
    char *mapping = NULL;

    if (controller->mapping) {
        mapping = SDL_strdup(controller->mapping);
    } else {
        mapping = NULL;
    }
    mapping = SetMappingName(mapping, controller_name);
    SetAndFreeGamepadMapping(mapping);
}

static void AddControllerNameText(const char *text)
{
    size_t current_length = (controller_name ? SDL_strlen(controller_name) : 0);
    size_t text_length = SDL_strlen(text);
    size_t size = current_length + text_length + 1;
    char *name = (char *)SDL_realloc(controller_name, size);
    if (name) {
        SDL_memcpy(&name[current_length], text, text_length + 1);
        controller_name = name;
    }
    CommitControllerName();
}

static void BackspaceControllerName(void)
{
    size_t length = (controller_name ? SDL_strlen(controller_name) : 0);
    if (length > 0) {
        controller_name[length - 1] = '\0';
    }
    CommitControllerName();
}

static void ClearControllerName(void)
{
    if (controller_name) {
        *controller_name = '\0';
    }
    CommitControllerName();
}

static void CopyControllerName(void)
{
    SDL_SetClipboardText(controller_name);
}

static void PasteControllerName(void)
{
    SDL_free(controller_name);
    controller_name = SDL_GetClipboardText();
    CommitControllerName();
}

static void CommitGamepadType(SDL_GamepadType type)
{
    char *mapping = NULL;

    if (controller->mapping) {
        mapping = SDL_strdup(controller->mapping);
    } else {
        mapping = NULL;
    }
    mapping = SetMappingType(mapping, type);
    SetAndFreeGamepadMapping(mapping);
}

static const char *GetBindingInstruction(void)
{
    switch (binding_element) {
    case SDL_GAMEPAD_ELEMENT_INVALID:
        return "Select an element to bind from the list on the left";
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH:
        switch (SDL_GetGamepadButtonLabelForType(GetGamepadImageType(image), (SDL_GamepadButton)binding_element)) {
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            return "Press the A button";
        case SDL_GAMEPAD_BUTTON_LABEL_B:
            return "Press the B button";
        case SDL_GAMEPAD_BUTTON_LABEL_X:
            return "Press the X button";
        case SDL_GAMEPAD_BUTTON_LABEL_Y:
            return "Press the Y button";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
            return "Press the Cross (X) button";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
            return "Press the Circle button";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
            return "Press the Square button";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
            return "Press the Triangle button";
        default:
            return "";
        }
        break;
    case SDL_GAMEPAD_BUTTON_BACK:
        return "Press the left center button (Back/View/Share)";
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return "Press the center button (Home/Guide)";
    case SDL_GAMEPAD_BUTTON_START:
        return "Press the right center button (Start/Menu/Options)";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return "Press the left thumbstick button (LSB/L3)";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return "Press the right thumbstick button (RSB/R3)";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return "Press the left shoulder button (LB/L1)";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return "Press the right shoulder button (RB/R1)";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return "Press the D-Pad up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return "Press the D-Pad down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return "Press the D-Pad left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return "Press the D-Pad right";
    case SDL_GAMEPAD_BUTTON_MISC1:
        return "Press the bottom center button (Share/Capture)";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
        return "Press the upper paddle under your right hand";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
        return "Press the upper paddle under your left hand";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
        return "Press the lower paddle under your right hand";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
        return "Press the lower paddle under your left hand";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
        return "Press down on the touchpad";
    case SDL_GAMEPAD_BUTTON_MISC2:
    case SDL_GAMEPAD_BUTTON_MISC3:
    case SDL_GAMEPAD_BUTTON_MISC4:
    case SDL_GAMEPAD_BUTTON_MISC5:
    case SDL_GAMEPAD_BUTTON_MISC6:
        return "Press any additional button not already bound";
    case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE:
        return "Move the left thumbstick to the left";
    case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE:
        return "Move the left thumbstick to the right";
    case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE:
        return "Move the left thumbstick up";
    case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE:
        return "Move the left thumbstick down";
    case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE:
        return "Move the right thumbstick to the left";
    case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE:
        return "Move the right thumbstick to the right";
    case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE:
        return "Move the right thumbstick up";
    case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE:
        return "Move the right thumbstick down";
    case SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER:
        return "Pull the left trigger (LT/L2)";
    case SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER:
        return "Pull the right trigger (RT/R2)";
    case SDL_GAMEPAD_ELEMENT_NAME:
        return "Type the name of your controller";
    case SDL_GAMEPAD_ELEMENT_TYPE:
        return "Select the type of your controller";
    default:
        return "";
    }
}

static int FindController(SDL_JoystickID id)
{
    int i;

    for (i = 0; i < num_controllers; ++i) {
        if (id == controllers[i].id) {
            return i;
        }
    }
    return -1;
}

static void SetController(SDL_JoystickID id)
{
    int i = FindController(id);

    if (i < 0 && num_controllers > 0) {
        i = 0;
    }

    if (i >= 0) {
        controller = &controllers[i];
    } else {
        controller = NULL;
    }

    RefreshControllerName();
}

static void AddController(SDL_JoystickID id, bool verbose)
{
    Controller *new_controllers;
    Controller *new_controller;
    SDL_Joystick *joystick;

    if (FindController(id) >= 0) {
        /* We already have this controller */
        return;
    }

    new_controllers = (Controller *)SDL_realloc(controllers, (num_controllers + 1) * sizeof(*controllers));
    if (!new_controllers) {
        return;
    }

    controller = NULL;
    controllers = new_controllers;
    new_controller = &new_controllers[num_controllers++];
    SDL_zerop(new_controller);
    new_controller->id = id;

    new_controller->joystick = SDL_OpenJoystick(id);
    new_controller->num_axes = SDL_GetNumJoystickAxes(new_controller->joystick);
    new_controller->axis_state = (AxisState *)SDL_calloc(new_controller->num_axes, sizeof(*new_controller->axis_state));

    joystick = new_controller->joystick;
    if (joystick) {
        if (verbose && !SDL_IsGamepad(id)) {
            const char *name = SDL_GetJoystickName(joystick);
            const char *path = SDL_GetJoystickPath(joystick);
            char guid[33];
            SDL_Log("Opened joystick %s%s%s\n", name, path ? ", " : "", path ? path : "");
            SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, sizeof(guid));
            SDL_Log("No gamepad mapping for %s\n", guid);
        }
    } else {
        SDL_Log("Couldn't open joystick: %s", SDL_GetError());
    }

    if (mapping_controller) {
        SetController(mapping_controller);
    } else {
        SetController(id);
    }
}

static void DelController(SDL_JoystickID id)
{
    int i = FindController(id);

    if (i < 0) {
        return;
    }

    if (display_mode == CONTROLLER_MODE_BINDING && id == controller->id) {
        SetDisplayMode(CONTROLLER_MODE_TESTING);
    }

    /* Reset trigger state */
    if (controllers[i].trigger_effect != 0) {
        controllers[i].trigger_effect = -1;
        CyclePS5TriggerEffect(&controllers[i]);
    }
    SDL_assert(controllers[i].gamepad == NULL);
    if (controllers[i].axis_state) {
        SDL_free(controllers[i].axis_state);
    }
    if (controllers[i].joystick) {
        SDL_CloseJoystick(controllers[i].joystick);
    }

    --num_controllers;
    if (i < num_controllers) {
        SDL_memcpy(&controllers[i], &controllers[i + 1], (num_controllers - i) * sizeof(*controllers));
    }

    if (mapping_controller) {
        SetController(mapping_controller);
    } else {
        SetController(id);
    }
}

static void HandleGamepadRemapped(SDL_JoystickID id)
{
    char *mapping;
    int i = FindController(id);

    SDL_assert(i >= 0);
    if (i < 0) {
        return;
    }

    if (!controllers[i].gamepad) {
        /* Failed to open this controller */
        return;
    }

    /* Get the current mapping */
    mapping = SDL_GetGamepadMapping(controllers[i].gamepad);

    /* Make sure the mapping has a valid name */
    if (mapping && !MappingHasName(mapping)) {
        mapping = SetMappingName(mapping, SDL_GetJoystickName(controllers[i].joystick));
    }

    SDL_free(controllers[i].mapping);
    controllers[i].mapping = mapping;
    controllers[i].has_bindings = MappingHasBindings(mapping);
}

static void HandleGamepadAdded(SDL_JoystickID id, bool verbose)
{
    SDL_Gamepad *gamepad;
    Uint16 firmware_version;
    SDL_SensorType sensors[] = {
        SDL_SENSOR_ACCEL,
        SDL_SENSOR_GYRO,
        SDL_SENSOR_ACCEL_L,
        SDL_SENSOR_GYRO_L,
        SDL_SENSOR_ACCEL_R,
        SDL_SENSOR_GYRO_R
    };
    int i;

    i = FindController(id);
    if (i < 0) {
        return;
    }

    SDL_assert(!controllers[i].gamepad);
    controllers[i].gamepad = SDL_OpenGamepad(id);

    gamepad = controllers[i].gamepad;
    if (gamepad) {
        if (verbose) {
            SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
            const char *name = SDL_GetGamepadName(gamepad);
            const char *path = SDL_GetGamepadPath(gamepad);
            SDL_Log("Opened gamepad %s%s%s\n", name, path ? ", " : "", path ? path : "");

            firmware_version = SDL_GetGamepadFirmwareVersion(gamepad);
            if (firmware_version) {
                SDL_Log("Firmware version: 0x%x (%d)\n", firmware_version, firmware_version);
            }

            if (SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN, false)) {
                SDL_Log("Has player LED");
            }

            if (SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)) {
                SDL_Log("Rumble supported");
            }

            if (SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false)) {
                SDL_Log("Trigger rumble supported");
            }

            if (SDL_GetGamepadPlayerIndex(gamepad) >= 0) {
                SDL_Log("Player index: %d\n", SDL_GetGamepadPlayerIndex(gamepad));
            }
        }

        for (i = 0; i < SDL_arraysize(sensors); ++i) {
            SDL_SensorType sensor = sensors[i];

            if (SDL_GamepadHasSensor(gamepad, sensor)) {
                if (verbose) {
                    SDL_Log("Enabling %s at %.2f Hz\n", GetSensorName(sensor), SDL_GetGamepadSensorDataRate(gamepad, sensor));
                }
                SDL_SetGamepadSensorEnabled(gamepad, sensor, true);
            }
        }

        if (verbose) {
            char *mapping = SDL_GetGamepadMapping(gamepad);
            if (mapping) {
                SDL_Log("Mapping: %s\n", mapping);
                SDL_free(mapping);
            }
        }
    } else {
        SDL_Log("Couldn't open gamepad: %s", SDL_GetError());
    }

    HandleGamepadRemapped(id);
}

static void HandleGamepadRemoved(SDL_JoystickID id)
{
    int i = FindController(id);

    SDL_assert(i >= 0);
    if (i < 0) {
        return;
    }

    if (controllers[i].mapping) {
        SDL_free(controllers[i].mapping);
        controllers[i].mapping = NULL;
    }
    if (controllers[i].gamepad) {
        SDL_CloseGamepad(controllers[i].gamepad);
        controllers[i].gamepad = NULL;
    }
}

static Uint16 ConvertAxisToRumble(Sint16 axisval)
{
    /* Only start rumbling if the axis is past the halfway point */
    const Sint16 half_axis = (Sint16)SDL_ceil(SDL_JOYSTICK_AXIS_MAX / 2.0f);
    if (axisval > half_axis) {
        return (Uint16)(axisval - half_axis) * 4;
    } else {
        return 0;
    }
}

static bool ShowingFront(void)
{
    bool showing_front = true;
    int i;

    /* Show the back of the gamepad if the paddles are being held or bound */
    for (i = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1; i <= SDL_GAMEPAD_BUTTON_LEFT_PADDLE2; ++i) {
        if (SDL_GetGamepadButton(controller->gamepad, (SDL_GamepadButton)i) ||
            binding_element == i) {
            showing_front = false;
            break;
        }
    }
    if ((SDL_GetModState() & SDL_KMOD_SHIFT) && binding_element != SDL_GAMEPAD_ELEMENT_NAME) {
        showing_front = false;
    }
    return showing_front;
}

static void SDLCALL VirtualGamepadSetPlayerIndex(void *userdata, int player_index)
{
    SDL_Log("Virtual Gamepad: player index set to %d\n", player_index);
}

static bool SDLCALL VirtualGamepadRumble(void *userdata, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_Log("Virtual Gamepad: rumble set to %d/%d\n", low_frequency_rumble, high_frequency_rumble);
    return true;
}

static bool SDLCALL VirtualGamepadRumbleTriggers(void *userdata, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_Log("Virtual Gamepad: trigger rumble set to %d/%d\n", left_rumble, right_rumble);
    return true;
}

static bool SDLCALL VirtualGamepadSetLED(void *userdata, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_Log("Virtual Gamepad: LED set to RGB %d,%d,%d\n", red, green, blue);
    return true;
}

static void OpenVirtualGamepad(void)
{
    SDL_VirtualJoystickTouchpadDesc virtual_touchpad = { 1, { 0, 0, 0 } };
    SDL_VirtualJoystickSensorDesc virtual_sensor = { SDL_SENSOR_ACCEL, 0.0f };
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID virtual_id;

    if (virtual_joystick) {
        return;
    }

    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.ntouchpads = 1;
    desc.touchpads = &virtual_touchpad;
    desc.nsensors = 1;
    desc.sensors = &virtual_sensor;
    desc.SetPlayerIndex = VirtualGamepadSetPlayerIndex;
    desc.Rumble = VirtualGamepadRumble;
    desc.RumbleTriggers = VirtualGamepadRumbleTriggers;
    desc.SetLED = VirtualGamepadSetLED;

    virtual_id = SDL_AttachVirtualJoystick(&desc);
    if (virtual_id == 0) {
        SDL_Log("Couldn't attach virtual device: %s\n", SDL_GetError());
    } else {
        virtual_joystick = SDL_OpenJoystick(virtual_id);
        if (!virtual_joystick) {
            SDL_Log("Couldn't open virtual device: %s\n", SDL_GetError());
        }
    }
}

static void CloseVirtualGamepad(void)
{
    int i;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(NULL);
    if (joysticks) {
        for (i = 0; joysticks[i]; ++i) {
            SDL_JoystickID instance_id = joysticks[i];
            if (SDL_IsJoystickVirtual(instance_id)) {
                SDL_DetachVirtualJoystick(instance_id);
            }
        }
        SDL_free(joysticks);
    }

    if (virtual_joystick) {
        SDL_CloseJoystick(virtual_joystick);
        virtual_joystick = NULL;
    }
}

static void VirtualGamepadMouseMotion(float x, float y)
{
    if (virtual_button_active != SDL_GAMEPAD_BUTTON_INVALID) {
        if (virtual_axis_active != SDL_GAMEPAD_AXIS_INVALID) {
            const float MOVING_DISTANCE = 2.0f;
            if (SDL_fabs(x - virtual_axis_start_x) >= MOVING_DISTANCE ||
                SDL_fabs(y - virtual_axis_start_y) >= MOVING_DISTANCE) {
                SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, false);
                virtual_button_active = SDL_GAMEPAD_BUTTON_INVALID;
            }
        }
    }

    if (virtual_axis_active != SDL_GAMEPAD_AXIS_INVALID) {
        if (virtual_axis_active == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
            virtual_axis_active == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            int range = (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);
            float distance = SDL_clamp((y - virtual_axis_start_y) / GetGamepadImageAxisHeight(image), 0.0f, 1.0f);
            Sint16 value = (Sint16)(SDL_JOYSTICK_AXIS_MIN + (distance * range));
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active, value);
        } else {
            float distanceX = SDL_clamp((x - virtual_axis_start_x) / GetGamepadImageAxisWidth(image), -1.0f, 1.0f);
            float distanceY = SDL_clamp((y - virtual_axis_start_y) / GetGamepadImageAxisHeight(image), -1.0f, 1.0f);
            Sint16 valueX, valueY;

            if (distanceX >= 0) {
                valueX = (Sint16)(distanceX * SDL_JOYSTICK_AXIS_MAX);
            } else {
                valueX = (Sint16)(distanceX * -SDL_JOYSTICK_AXIS_MIN);
            }
            if (distanceY >= 0) {
                valueY = (Sint16)(distanceY * SDL_JOYSTICK_AXIS_MAX);
            } else {
                valueY = (Sint16)(distanceY * -SDL_JOYSTICK_AXIS_MIN);
            }
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active, valueX);
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active + 1, valueY);
        }
    }

    if (virtual_touchpad_active) {
        SDL_FRect touchpad;
        GetGamepadTouchpadArea(image, &touchpad);
        virtual_touchpad_x = (x - touchpad.x) / touchpad.w;
        virtual_touchpad_y = (y - touchpad.y) / touchpad.h;
        SDL_SetJoystickVirtualTouchpad(virtual_joystick, 0, 0, true, virtual_touchpad_x, virtual_touchpad_y, 1.0f);
    }
}

static void VirtualGamepadMouseDown(float x, float y)
{
    int element = GetGamepadImageElementAt(image, x, y);

    if (element == SDL_GAMEPAD_ELEMENT_INVALID) {
        SDL_FPoint point = { x, y };
        SDL_FRect touchpad;
        GetGamepadTouchpadArea(image, &touchpad);
        if (SDL_PointInRectFloat(&point, &touchpad)) {
            virtual_touchpad_active = true;
            virtual_touchpad_x = (x - touchpad.x) / touchpad.w;
            virtual_touchpad_y = (y - touchpad.y) / touchpad.h;
            SDL_SetJoystickVirtualTouchpad(virtual_joystick, 0, 0, true, virtual_touchpad_x, virtual_touchpad_y, 1.0f);
        }
        return;
    }

    if (element < SDL_GAMEPAD_BUTTON_COUNT) {
        virtual_button_active = (SDL_GamepadButton)element;
        SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, true);
    } else {
        switch (element) {
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE:
            virtual_axis_active = SDL_GAMEPAD_AXIS_LEFTX;
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE:
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE:
            virtual_axis_active = SDL_GAMEPAD_AXIS_RIGHTX;
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER:
            virtual_axis_active = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER:
            virtual_axis_active = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            break;
        }
        virtual_axis_start_x = x;
        virtual_axis_start_y = y;
    }
}

static void VirtualGamepadMouseUp(float x, float y)
{
    if (virtual_button_active != SDL_GAMEPAD_BUTTON_INVALID) {
        SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, false);
        virtual_button_active = SDL_GAMEPAD_BUTTON_INVALID;
    }

    if (virtual_axis_active != SDL_GAMEPAD_AXIS_INVALID) {
        if (virtual_axis_active == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
            virtual_axis_active == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active, SDL_JOYSTICK_AXIS_MIN);
        } else {
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active, 0);
            SDL_SetJoystickVirtualAxis(virtual_joystick, virtual_axis_active + 1, 0);
        }
        virtual_axis_active = SDL_GAMEPAD_AXIS_INVALID;
    }

    if (virtual_touchpad_active) {
        SDL_SetJoystickVirtualTouchpad(virtual_joystick, 0, 0, false, virtual_touchpad_x, virtual_touchpad_y, 0.0f);
        virtual_touchpad_active = false;
    }
}

static void DrawGamepadWaiting(SDL_Renderer *renderer)
{
    const char *text = "Waiting for gamepad, press A to add a virtual controller";
    float x, y;

    x = SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
    y = TITLE_HEIGHT / 2 - FONT_CHARACTER_SIZE / 2;
    SDLTest_DrawString(renderer, x, y, text);
}

static void DrawGamepadInfo(SDL_Renderer *renderer)
{
    const char *type;
    const char *serial;
    char text[128];
    float x, y;

    if (title_highlighted) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

        if (title_pressed) {
            SDL_SetRenderDrawColor(renderer, PRESSED_COLOR);
        } else {
            SDL_SetRenderDrawColor(renderer, HIGHLIGHT_COLOR);
        }
        SDL_RenderFillRect(renderer, &title_area);

        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    }

    if (type_highlighted) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

        if (type_pressed) {
            SDL_SetRenderDrawColor(renderer, PRESSED_COLOR);
        } else {
            SDL_SetRenderDrawColor(renderer, HIGHLIGHT_COLOR);
        }
        SDL_RenderFillRect(renderer, &type_area);

        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    }

    if (controller->joystick) {
        SDL_snprintf(text, sizeof(text), "(%" SDL_PRIu32 ")", SDL_GetJoystickID(controller->joystick));
        x = SCREEN_WIDTH - (FONT_CHARACTER_SIZE * SDL_strlen(text)) - 8.0f;
        y = 8.0f;
        SDLTest_DrawString(renderer, x, y, text);
    }

    if (controller_name && *controller_name) {
        x = title_area.x + title_area.w / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(controller_name)) / 2;
        y = title_area.y + title_area.h / 2 - FONT_CHARACTER_SIZE / 2;
        SDLTest_DrawString(renderer, x, y, controller_name);
    }

    if (SDL_IsJoystickVirtual(controller->id)) {
        SDL_strlcpy(text, "Click on the gamepad image below to generate input", sizeof(text));
        x = SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
        y = TITLE_HEIGHT / 2 - FONT_CHARACTER_SIZE / 2 + FONT_LINE_HEIGHT + 2.0f;
        SDLTest_DrawString(renderer, x, y, text);
    }

    type = GetGamepadTypeString(SDL_GetGamepadType(controller->gamepad));
    x = type_area.x + type_area.w / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(type)) / 2;
    y = type_area.y + type_area.h / 2 - FONT_CHARACTER_SIZE / 2;
    SDLTest_DrawString(renderer, x, y, type);

    if (display_mode == CONTROLLER_MODE_TESTING) {
        Uint64 steam_handle = SDL_GetGamepadSteamHandle(controller->gamepad);
        if (steam_handle) {
            SDL_snprintf(text, SDL_arraysize(text), "Steam: 0x%.16" SDL_PRIx64, steam_handle);
            y = SCREEN_HEIGHT - 2 * (8.0f + FONT_LINE_HEIGHT);
            x = SCREEN_WIDTH - 8.0f - (FONT_CHARACTER_SIZE * SDL_strlen(text));
            SDLTest_DrawString(renderer, x, y, text);
        }

        SDL_snprintf(text, SDL_arraysize(text), "VID: 0x%.4x PID: 0x%.4x",
                     SDL_GetJoystickVendor(controller->joystick),
                     SDL_GetJoystickProduct(controller->joystick));
        y = SCREEN_HEIGHT - 8.0f - FONT_LINE_HEIGHT;
        x = SCREEN_WIDTH - 8.0f - (FONT_CHARACTER_SIZE * SDL_strlen(text));
        SDLTest_DrawString(renderer, x, y, text);

        serial = SDL_GetJoystickSerial(controller->joystick);
        if (serial && *serial) {
            SDL_snprintf(text, SDL_arraysize(text), "Serial: %s", serial);
            x = SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
            y = SCREEN_HEIGHT - 8.0f - FONT_LINE_HEIGHT;
            SDLTest_DrawString(renderer, x, y, text);
        }
    }
}

static const char *GetButtonLabel(SDL_GamepadType type, SDL_GamepadButton button)
{
    switch (SDL_GetGamepadButtonLabelForType(type, button)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:
        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        return "Cross (X)";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        return "Triangle";
    default:
        return "UNKNOWN";
    }
}

static void DrawBindingTips(SDL_Renderer *renderer)
{
    const char *text;
    SDL_FRect image_area, button_area;
    float x, y;

    GetGamepadImageArea(image, &image_area);
    GetGamepadButtonArea(done_mapping_button, &button_area);
    x = image_area.x + image_area.w / 2;
    y = image_area.y + image_area.h;
    y += (button_area.y - y - FONT_CHARACTER_SIZE) / 2;

    text = GetBindingInstruction();

    if (binding_element == SDL_GAMEPAD_ELEMENT_INVALID) {
        SDLTest_DrawString(renderer, x - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2, y, text);
    } else {
        Uint8 r, g, b, a;
        SDL_FRect rect;
        SDL_GamepadButton action_forward = SDL_GAMEPAD_BUTTON_SOUTH;
        bool bound_forward = MappingHasElement(controller->mapping, action_forward);
        SDL_GamepadButton action_backward = SDL_GAMEPAD_BUTTON_EAST;
        bool bound_backward = MappingHasElement(controller->mapping, action_backward);
        SDL_GamepadButton action_delete = SDL_GAMEPAD_BUTTON_WEST;
        bool bound_delete = MappingHasElement(controller->mapping, action_delete);

        y -= (FONT_CHARACTER_SIZE + BUTTON_MARGIN) / 2;

        rect.w = 2.0f + (FONT_CHARACTER_SIZE * SDL_strlen(text)) + 2.0f;
        rect.h = 2.0f + FONT_CHARACTER_SIZE + 2.0f;
        rect.x = x - rect.w / 2;
        rect.y = y - 2.0f;

        SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
        SDL_SetRenderDrawColor(renderer, SELECTED_COLOR);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDLTest_DrawString(renderer, x - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2, y, text);

        y += (FONT_CHARACTER_SIZE + BUTTON_MARGIN);

        if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
            text = "(press RETURN to complete)";
        } else if (binding_element == SDL_GAMEPAD_ELEMENT_TYPE ||
                   binding_element == action_forward ||
                   binding_element == action_backward) {
            text = "(press ESC to cancel)";
        } else {
            static char dynamic_text[128];
            SDL_GamepadType type = GetGamepadImageType(image);
            if (binding_flow && bound_forward && bound_backward) {
                if (binding_element != action_delete && bound_delete) {
                    SDL_snprintf(dynamic_text, sizeof(dynamic_text), "(press %s to skip, %s to go back, %s to delete, and ESC to cancel)", GetButtonLabel(type, action_forward), GetButtonLabel(type, action_backward), GetButtonLabel(type, action_delete));
                } else {
                    SDL_snprintf(dynamic_text, sizeof(dynamic_text), "(press %s to skip, %s to go back, SPACE to delete, and ESC to cancel)", GetButtonLabel(type, action_forward), GetButtonLabel(type, action_backward));
                }
                text = dynamic_text;
            } else {
                text = "(press SPACE to delete and ESC to cancel)";
            }
        }
        SDLTest_DrawString(renderer, x - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2, y, text);
    }
}

static void UpdateGamepadEffects(void)
{
    if (display_mode != CONTROLLER_MODE_TESTING || !controller->gamepad) {
        return;
    }

    /* Update LED based on left thumbstick position */
    {
        Sint16 x = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        Sint16 y = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_LEFTY);

        if (!set_LED) {
            set_LED = (x < -8000 || x > 8000 || y > 8000);
        }
        if (set_LED) {
            Uint8 r, g, b;

            if (x < 0) {
                r = (Uint8)(((~x) * 255) / 32767);
                b = 0;
            } else {
                r = 0;
                b = (Uint8)(((int)(x)*255) / 32767);
            }
            if (y > 0) {
                g = (Uint8)(((int)(y)*255) / 32767);
            } else {
                g = 0;
            }

            SDL_SetGamepadLED(controller->gamepad, r, g, b);
        }
    }

    if (controller->trigger_effect == 0) {
        /* Update rumble based on trigger state */
        {
            Sint16 left = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
            Sint16 right = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            Uint16 low_frequency_rumble = ConvertAxisToRumble(left);
            Uint16 high_frequency_rumble = ConvertAxisToRumble(right);
            SDL_RumbleGamepad(controller->gamepad, low_frequency_rumble, high_frequency_rumble, 250);
        }

        /* Update trigger rumble based on thumbstick state */
        {
            Sint16 left = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_LEFTY);
            Sint16 right = SDL_GetGamepadAxis(controller->gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
            Uint16 left_rumble = ConvertAxisToRumble(~left);
            Uint16 right_rumble = ConvertAxisToRumble(~right);

            SDL_RumbleGamepadTriggers(controller->gamepad, left_rumble, right_rumble, 250);
        }
    }
}

static void loop(void *arg)
{
    SDL_Event event;

    /* If we have a virtual controller, send a virtual accelerometer sensor reading */
    if (virtual_joystick) {
        float data[3] = { 0.0f, SDL_STANDARD_GRAVITY, 0.0f };
        SDL_SendJoystickVirtualSensorData(virtual_joystick, SDL_SENSOR_ACCEL, SDL_GetTicksNS(), data, SDL_arraysize(data));
    }

    /* Update to get the current event state */
    SDL_PumpEvents();

    /* Process all currently pending events */
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
        SDL_ConvertEventToRenderCoordinates(screen, &event);

        switch (event.type) {
        case SDL_EVENT_JOYSTICK_ADDED:
            AddController(event.jdevice.which, true);
            break;

        case SDL_EVENT_JOYSTICK_REMOVED:
            DelController(event.jdevice.which);
            break;

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (event.jaxis.value <= (-SDL_JOYSTICK_AXIS_MAX / 2) || event.jaxis.value >= (SDL_JOYSTICK_AXIS_MAX / 2)) {
                    SetController(event.jaxis.which);
                }
            } else if (display_mode == CONTROLLER_MODE_BINDING &&
                       event.jaxis.which == controller->id &&
                       event.jaxis.axis < controller->num_axes &&
                       binding_element != SDL_GAMEPAD_ELEMENT_INVALID) {
                const int MAX_ALLOWED_JITTER = SDL_JOYSTICK_AXIS_MAX / 80; /* ShanWan PS3 gamepad needed 96 */
                AxisState *pAxisState = &controller->axis_state[event.jaxis.axis];
                int nValue = event.jaxis.value;
                int nCurrentDistance, nFarthestDistance;
                if (!pAxisState->m_bMoving) {
                    Sint16 nInitialValue;
                    pAxisState->m_bMoving = SDL_GetJoystickAxisInitialState(controller->joystick, event.jaxis.axis, &nInitialValue);
                    pAxisState->m_nLastValue = nValue;
                    pAxisState->m_nStartingValue = nInitialValue;
                    pAxisState->m_nFarthestValue = nInitialValue;
                } else if (SDL_abs(nValue - pAxisState->m_nLastValue) <= MAX_ALLOWED_JITTER) {
                    break;
                } else {
                    pAxisState->m_nLastValue = nValue;
                }
                nCurrentDistance = SDL_abs(nValue - pAxisState->m_nStartingValue);
                nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
                if (nCurrentDistance > nFarthestDistance) {
                    pAxisState->m_nFarthestValue = nValue;
                    nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
                }

#ifdef DEBUG_AXIS_MAPPING
                SDL_Log("AXIS %d nValue %d nCurrentDistance %d nFarthestDistance %d\n", event.jaxis.axis, nValue, nCurrentDistance, nFarthestDistance);
#endif
                /* If we've gone out far enough and started to come back, let's bind this axis */
                if (nFarthestDistance >= 16000 && nCurrentDistance <= 10000) {
                    char binding[12];
                    int axis_min = StandardizeAxisValue(pAxisState->m_nStartingValue);
                    int axis_max = StandardizeAxisValue(pAxisState->m_nFarthestValue);

                    if (axis_min == 0 && axis_max == SDL_JOYSTICK_AXIS_MIN) {
                        /* The negative half axis */
                        (void)SDL_snprintf(binding, sizeof(binding), "-a%d", event.jaxis.axis);
                    } else if (axis_min == 0 && axis_max == SDL_JOYSTICK_AXIS_MAX) {
                        /* The positive half axis */
                        (void)SDL_snprintf(binding, sizeof(binding), "+a%d", event.jaxis.axis);
                    } else {
                        (void)SDL_snprintf(binding, sizeof(binding), "a%d", event.jaxis.axis);
                        if (axis_min > axis_max) {
                            /* Invert the axis */
                            SDL_strlcat(binding, "~", SDL_arraysize(binding));
                        }
                    }
#ifdef DEBUG_AXIS_MAPPING
                    SDL_Log("AXIS %d axis_min = %d, axis_max = %d, binding = %s\n", event.jaxis.axis, axis_min, axis_max, binding);
#endif
                    CommitBindingElement(binding, false);
                }
            }
            break;

        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            if (display_mode == CONTROLLER_MODE_TESTING) {
                SetController(event.jbutton.which);
            }
            break;

        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            if (display_mode == CONTROLLER_MODE_BINDING &&
                event.jbutton.which == controller->id &&
                binding_element != SDL_GAMEPAD_ELEMENT_INVALID) {
                char binding[12];

                SDL_snprintf(binding, sizeof(binding), "b%d", event.jbutton.button);
                CommitBindingElement(binding, false);
            }
            break;

        case SDL_EVENT_JOYSTICK_HAT_MOTION:
            if (display_mode == CONTROLLER_MODE_BINDING &&
                event.jhat.which == controller->id &&
                event.jhat.value != SDL_HAT_CENTERED &&
                binding_element != SDL_GAMEPAD_ELEMENT_INVALID) {
                char binding[12];

                SDL_snprintf(binding, sizeof(binding), "h%d.%d", event.jhat.hat, event.jhat.value);
                CommitBindingElement(binding, false);
            }
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            HandleGamepadAdded(event.gdevice.which, true);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            HandleGamepadRemoved(event.gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_REMAPPED:
            HandleGamepadRemapped(event.gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
            RefreshControllerName();
            break;

#ifdef VERBOSE_TOUCHPAD
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            SDL_Log("Gamepad %" SDL_PRIu32 " touchpad %" SDL_PRIs32 " finger %" SDL_PRIs32 " %s %.2f, %.2f, %.2f\n",
                    event.gtouchpad.which,
                    event.gtouchpad.touchpad,
                    event.gtouchpad.finger,
                    (event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN ? "pressed at" : (event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_UP ? "released at" : "moved to")),
                    event.gtouchpad.x,
                    event.gtouchpad.y,
                    event.gtouchpad.pressure);
            break;
#endif /* VERBOSE_TOUCHPAD */

#ifdef VERBOSE_SENSORS
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            SDL_Log("Gamepad %" SDL_PRIu32 " sensor %s: %.2f, %.2f, %.2f (%" SDL_PRIu64 ")\n",
                    event.gsensor.which,
                    GetSensorName((SDL_SensorType) event.gsensor.sensor),
                    event.gsensor.data[0],
                    event.gsensor.data[1],
                    event.gsensor.data[2],
                    event.gsensor.sensor_timestamp);
            break;
#endif /* VERBOSE_SENSORS */

#ifdef VERBOSE_AXES
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (event.gaxis.value <= (-SDL_JOYSTICK_AXIS_MAX / 2) || event.gaxis.value >= (SDL_JOYSTICK_AXIS_MAX / 2)) {
                    SetController(event.gaxis.which);
                }
            }
            SDL_Log("Gamepad %" SDL_PRIu32 " axis %s changed to %d\n",
                    event.gaxis.which,
                    SDL_GetGamepadStringForAxis((SDL_GamepadAxis) event.gaxis.axis),
                    event.gaxis.value);
            break;
#endif /* VERBOSE_AXES */

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    SetController(event.gbutton.which);
                }
            }
#ifdef VERBOSE_BUTTONS
            SDL_Log("Gamepad %" SDL_PRIu32 " button %s %s\n",
                    event.gbutton.which,
                    SDL_GetGamepadStringForButton((SDL_GamepadButton) event.gbutton.button),
                    event.gbutton.state ? "pressed" : "released");
#endif /* VERBOSE_BUTTONS */

            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                    controller && SDL_GetGamepadType(controller->gamepad) == SDL_GAMEPAD_TYPE_PS5) {
                    /* Cycle PS5 audio routing when the microphone button is pressed */
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_MISC1) {
                        CyclePS5AudioRoute(controller);
                    }

                    /* Cycle PS5 trigger effects when the triangle button is pressed */
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) {
                        CyclePS5TriggerEffect(controller);
                    }
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (virtual_joystick && controller && controller->joystick == virtual_joystick) {
                VirtualGamepadMouseDown(event.button.x, event.button.y);
            }
            UpdateButtonHighlights(event.button.x, event.button.y, event.button.down);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (virtual_joystick && controller && controller->joystick == virtual_joystick) {
                VirtualGamepadMouseUp(event.button.x, event.button.y);
            }

            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (GamepadButtonContains(setup_mapping_button, event.button.x, event.button.y)) {
                    SetDisplayMode(CONTROLLER_MODE_BINDING);
                }
            } else if (display_mode == CONTROLLER_MODE_BINDING) {
                if (GamepadButtonContains(done_mapping_button, event.button.x, event.button.y)) {
                    if (controller->mapping) {
                        SDL_Log("Mapping complete:\n");
                        SDL_Log("%s\n", controller->mapping);
                    }
                    SetDisplayMode(CONTROLLER_MODE_TESTING);
                } else if (GamepadButtonContains(cancel_button, event.button.x, event.button.y)) {
                    CancelMapping();
                } else if (GamepadButtonContains(clear_button, event.button.x, event.button.y)) {
                    ClearMapping();
                } else if (controller->has_bindings &&
                           GamepadButtonContains(copy_button, event.button.x, event.button.y)) {
                    CopyMapping();
                } else if (GamepadButtonContains(paste_button, event.button.x, event.button.y)) {
                    PasteMapping();
                } else if (title_pressed) {
                    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_NAME, false);
                } else if (type_pressed) {
                    SetCurrentBindingElement(SDL_GAMEPAD_ELEMENT_TYPE, false);
                } else if (binding_element == SDL_GAMEPAD_ELEMENT_TYPE) {
                    int type = GetGamepadTypeDisplayAt(gamepad_type, event.button.x, event.button.y);
                    if (type != SDL_GAMEPAD_TYPE_UNSELECTED) {
                        CommitGamepadType((SDL_GamepadType)type);
                        StopBinding();
                    }
                } else {
                    int gamepad_element = SDL_GAMEPAD_ELEMENT_INVALID;
                    char *joystick_element;

                    if (controller->joystick != virtual_joystick) {
                        gamepad_element = GetGamepadImageElementAt(image, event.button.x, event.button.y);
                    }
                    if (gamepad_element == SDL_GAMEPAD_ELEMENT_INVALID) {
                        gamepad_element = GetGamepadDisplayElementAt(gamepad_elements, controller->gamepad, event.button.x, event.button.y);
                    }
                    if (gamepad_element != SDL_GAMEPAD_ELEMENT_INVALID) {
                        /* Set this to false if you don't want to start the binding flow at this point */
                        const bool should_start_flow = true;
                        SetCurrentBindingElement(gamepad_element, should_start_flow);
                    }

                    joystick_element = GetJoystickDisplayElementAt(joystick_elements, controller->joystick, event.button.x, event.button.y);
                    if (joystick_element) {
                        CommitBindingElement(joystick_element, true);
                        SDL_free(joystick_element);
                    }
                }
            }
            UpdateButtonHighlights(event.button.x, event.button.y, event.button.down);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (virtual_joystick && controller && controller->joystick == virtual_joystick) {
                VirtualGamepadMouseMotion(event.motion.x, event.motion.y);
            }
            UpdateButtonHighlights(event.motion.x, event.motion.y, event.motion.state ? true : false);
            break;

        case SDL_EVENT_KEY_DOWN:
            if (display_mode == CONTROLLER_MODE_TESTING) {
                if (event.key.key >= SDLK_0 && event.key.key <= SDLK_9) {
                    if (controller && controller->gamepad) {
                        int player_index = (event.key.key - SDLK_0);

                        SDL_SetGamepadPlayerIndex(controller->gamepad, player_index);
                    }
                    break;
                } else if (event.key.key == SDLK_A) {
                    OpenVirtualGamepad();
                } else if (event.key.key == SDLK_D) {
                    CloseVirtualGamepad();
                } else if (event.key.key == SDLK_R && (event.key.mod & SDL_KMOD_CTRL)) {
                    SDL_ReloadGamepadMappings();
                } else if (event.key.key == SDLK_ESCAPE) {
                    done = true;
                }
            } else if (display_mode == CONTROLLER_MODE_BINDING) {
                if (event.key.key == SDLK_C && (event.key.mod & SDL_KMOD_CTRL)) {
                    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                        CopyControllerName();
                    } else {
                        CopyMapping();
                    }
                } else if (event.key.key == SDLK_V && (event.key.mod & SDL_KMOD_CTRL)) {
                    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                        ClearControllerName();
                        PasteControllerName();
                    } else {
                        PasteMapping();
                    }
                } else if (event.key.key == SDLK_X && (event.key.mod & SDL_KMOD_CTRL)) {
                    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                        CopyControllerName();
                        ClearControllerName();
                    } else {
                        CopyMapping();
                        ClearMapping();
                    }
                } else if (event.key.key == SDLK_SPACE) {
                    if (binding_element != SDL_GAMEPAD_ELEMENT_NAME) {
                        ClearBinding();
                    }
                } else if (event.key.key == SDLK_BACKSPACE) {
                    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                        BackspaceControllerName();
                    }
                } else if (event.key.key == SDLK_RETURN) {
                    if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                        StopBinding();
                    }
                } else if (event.key.key == SDLK_ESCAPE) {
                    if (binding_element != SDL_GAMEPAD_ELEMENT_INVALID) {
                        StopBinding();
                    } else {
                        CancelMapping();
                    }
                }
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (display_mode == CONTROLLER_MODE_BINDING) {
                if (binding_element == SDL_GAMEPAD_ELEMENT_NAME) {
                    AddControllerNameText(event.text.text);
                }
            }
            break;
        case SDL_EVENT_QUIT:
            done = true;
            break;
        default:
            break;
        }
    }

    /* Wait 30 ms for joystick events to stop coming in,
       in case a gamepad sends multiple events for a single control (e.g. axis and button for trigger)
    */
    if (binding_advance_time && SDL_GetTicks() > (binding_advance_time + 30)) {
        if (binding_flow) {
            SetNextBindingElement();
        } else {
            StopBinding();
        }
    }

    /* blank screen, set up for drawing this frame. */
    SDL_SetRenderDrawColor(screen, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_SetRenderDrawColor(screen, 0x10, 0x10, 0x10, SDL_ALPHA_OPAQUE);

    if (controller) {
        SetGamepadImageShowingFront(image, ShowingFront());
        UpdateGamepadImageFromGamepad(image, controller->gamepad);
        if (display_mode == CONTROLLER_MODE_BINDING &&
            binding_element != SDL_GAMEPAD_ELEMENT_INVALID) {
            SetGamepadImageElement(image, binding_element, true);
        }
        RenderGamepadImage(image);

        if (binding_element == SDL_GAMEPAD_ELEMENT_TYPE) {
            SetGamepadTypeDisplayRealType(gamepad_type, SDL_GetRealGamepadType(controller->gamepad));
            RenderGamepadTypeDisplay(gamepad_type);
        } else {
            RenderGamepadDisplay(gamepad_elements, controller->gamepad);
        }
        RenderJoystickDisplay(joystick_elements, controller->joystick);

        if (display_mode == CONTROLLER_MODE_TESTING) {
            RenderGamepadButton(setup_mapping_button);
        } else if (display_mode == CONTROLLER_MODE_BINDING) {
            DrawBindingTips(screen);
            RenderGamepadButton(done_mapping_button);
            RenderGamepadButton(cancel_button);
            RenderGamepadButton(clear_button);
            if (controller->has_bindings) {
                RenderGamepadButton(copy_button);
            }
            RenderGamepadButton(paste_button);
        }

        DrawGamepadInfo(screen);

        UpdateGamepadEffects();
    } else {
        DrawGamepadWaiting(screen);
    }
    SDL_Delay(16);
    SDL_RenderPresent(screen);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    bool show_mappings = false;
    int i;
    float content_scale;
    int screen_width, screen_height;
    SDL_FRect area;
    int gamepad_index = -1;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ROG_CHAKRAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_LINUX_DEADZONES, "1");

    /* Enable input debug logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--mappings") == 0) {
                show_mappings = true;
                consumed = 1;
            } else if (SDL_strcmp(argv[i], "--virtual") == 0) {
                OpenVirtualGamepad();
                consumed = 1;
            } else if (gamepad_index < 0) {
                char *endptr = NULL;
                gamepad_index = (int)SDL_strtol(argv[i], &endptr, 0);
                if (endptr != argv[i] && *endptr == '\0' && gamepad_index >= 0) {
                    consumed = 1;
                }
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--mappings]", "[--virtual]", "[index]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }
    if (gamepad_index < 0) {
        gamepad_index = 0;
    }

    /* Initialize SDL (Note: video is required to start event loop) */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");

    if (show_mappings) {
        int count = 0;
        char **mappings = SDL_GetGamepadMappings(&count);
        int map_i;
        SDL_Log("Supported mappings:\n");
        for (map_i = 0; map_i < count; ++map_i) {
            SDL_Log("\t%s\n", mappings[map_i]);
        }
        SDL_Log("\n");
        SDL_free(mappings);
    }

    /* Create a window to display gamepad state */
    content_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (content_scale == 0.0f) {
        content_scale = 1.0f;
    }
    screen_width = (int)SDL_ceilf(SCREEN_WIDTH * content_scale);
    screen_height = (int)SDL_ceilf(SCREEN_HEIGHT * content_scale);
    window = SDL_CreateWindow("SDL Controller Test", screen_width, screen_height, 0);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return 2;
    }

    screen = SDL_CreateRenderer(window, NULL);
    if (!screen) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return 2;
    }

    SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_RenderPresent(screen);

    /* scale for platforms that don't give you the window size you asked for. */
    SDL_SetRenderLogicalPresentation(screen, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);


    title_area.w = GAMEPAD_WIDTH;
    title_area.h = FONT_CHARACTER_SIZE + 2 * BUTTON_MARGIN;
    title_area.x = PANEL_WIDTH + PANEL_SPACING;
    title_area.y = TITLE_HEIGHT / 2 - title_area.h / 2;

    type_area.w = PANEL_WIDTH - 2 * BUTTON_MARGIN;
    type_area.h = FONT_CHARACTER_SIZE + 2 * BUTTON_MARGIN;
    type_area.x = BUTTON_MARGIN;
    type_area.y = TITLE_HEIGHT / 2 - type_area.h / 2;

    image = CreateGamepadImage(screen);
    if (!image) {
        SDL_DestroyRenderer(screen);
        SDL_DestroyWindow(window);
        return 2;
    }
    SetGamepadImagePosition(image, PANEL_WIDTH + PANEL_SPACING, TITLE_HEIGHT);

    gamepad_elements = CreateGamepadDisplay(screen);
    area.x = 0;
    area.y = TITLE_HEIGHT;
    area.w = PANEL_WIDTH;
    area.h = GAMEPAD_HEIGHT;
    SetGamepadDisplayArea(gamepad_elements, &area);

    gamepad_type = CreateGamepadTypeDisplay(screen);
    area.x = 0;
    area.y = TITLE_HEIGHT;
    area.w = PANEL_WIDTH;
    area.h = GAMEPAD_HEIGHT;
    SetGamepadTypeDisplayArea(gamepad_type, &area);

    joystick_elements = CreateJoystickDisplay(screen);
    area.x = PANEL_WIDTH + PANEL_SPACING + GAMEPAD_WIDTH + PANEL_SPACING;
    area.y = TITLE_HEIGHT;
    area.w = PANEL_WIDTH;
    area.h = GAMEPAD_HEIGHT;
    SetJoystickDisplayArea(joystick_elements, &area);

    setup_mapping_button = CreateGamepadButton(screen, "Setup Mapping");
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(setup_mapping_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(setup_mapping_button) + 2 * BUTTON_PADDING;
    area.x = BUTTON_MARGIN;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(setup_mapping_button, &area);

    cancel_button = CreateGamepadButton(screen, "Cancel");
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(cancel_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(cancel_button) + 2 * BUTTON_PADDING;
    area.x = BUTTON_MARGIN;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(cancel_button, &area);

    clear_button = CreateGamepadButton(screen, "Clear");
    area.x += area.w + BUTTON_PADDING;
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(clear_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(clear_button) + 2 * BUTTON_PADDING;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(clear_button, &area);

    copy_button = CreateGamepadButton(screen, "Copy");
    area.x += area.w + BUTTON_PADDING;
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(copy_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(copy_button) + 2 * BUTTON_PADDING;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(copy_button, &area);

    paste_button = CreateGamepadButton(screen, "Paste");
    area.x += area.w + BUTTON_PADDING;
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(paste_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(paste_button) + 2 * BUTTON_PADDING;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(paste_button, &area);

    done_mapping_button = CreateGamepadButton(screen, "Done");
    area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(done_mapping_button) + 2 * BUTTON_PADDING);
    area.h = GetGamepadButtonLabelHeight(done_mapping_button) + 2 * BUTTON_PADDING;
    area.x = SCREEN_WIDTH / 2 - area.w / 2;
    area.y = SCREEN_HEIGHT - BUTTON_MARGIN - area.h;
    SetGamepadButtonArea(done_mapping_button, &area);

    /* Process the initial gamepad list */
    loop(NULL);

    if (gamepad_index < num_controllers) {
        SetController(controllers[gamepad_index].id);
    } else if (num_controllers > 0) {
        SetController(controllers[0].id);
    }

    /* Loop, getting gamepad events! */
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop_arg(loop, NULL, 0, 1);
#else
    while (!done) {
        loop(NULL);
    }
#endif

    CloseVirtualGamepad();
    while (num_controllers > 0) {
        HandleGamepadRemoved(controllers[0].id);
        DelController(controllers[0].id);
    }
    SDL_free(controllers);
    SDL_free(controller_name);
    DestroyGamepadImage(image);
    DestroyGamepadDisplay(gamepad_elements);
    DestroyGamepadTypeDisplay(gamepad_type);
    DestroyJoystickDisplay(joystick_elements);
    DestroyGamepadButton(setup_mapping_button);
    DestroyGamepadButton(done_mapping_button);
    DestroyGamepadButton(cancel_button);
    DestroyGamepadButton(clear_button);
    DestroyGamepadButton(copy_button);
    DestroyGamepadButton(paste_button);
    SDLTest_CleanupTextDrawing();
    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
