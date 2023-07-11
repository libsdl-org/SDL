/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program to test the SDL gamepad routines */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_font.h>

#include "gamepadutils.h"
#include "testutils.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define TITLE_HEIGHT 48
#define PANEL_SPACING 25
#define PANEL_WIDTH 250
#define GAMEPAD_WIDTH 512
#define GAMEPAD_HEIGHT 480

#define SCREEN_WIDTH  (PANEL_WIDTH + PANEL_SPACING + GAMEPAD_WIDTH + PANEL_SPACING + PANEL_WIDTH)
#define SCREEN_HEIGHT (TITLE_HEIGHT + GAMEPAD_HEIGHT)

/* This is indexed by SDL_JoystickPowerLevel + 1. */
static const char *power_level_strings[] = {
    "unknown", /* SDL_JOYSTICK_POWER_UNKNOWN */
    "empty",   /* SDL_JOYSTICK_POWER_EMPTY */
    "low",     /* SDL_JOYSTICK_POWER_LOW */
    "medium",  /* SDL_JOYSTICK_POWER_MEDIUM */
    "full",    /* SDL_JOYSTICK_POWER_FULL */
    "wired",   /* SDL_JOYSTICK_POWER_WIRED */
};
SDL_COMPILE_TIME_ASSERT(power_level_strings, SDL_arraysize(power_level_strings) == SDL_JOYSTICK_POWER_MAX + 1);

static SDL_Window *window = NULL;
static SDL_Renderer *screen = NULL;
static GamepadImage *image = NULL;
static GamepadDisplay *gamepad_elements = NULL;
static JoystickDisplay *joystick_elements = NULL;
static SDL_bool retval = SDL_FALSE;
static SDL_bool done = SDL_FALSE;
static SDL_bool set_LED = SDL_FALSE;
static int trigger_effect = 0;
static SDL_Gamepad *gamepad;
static SDL_Gamepad **gamepads;
static int num_gamepads = 0;
static SDL_Joystick *virtual_joystick = NULL;
static SDL_GamepadAxis virtual_axis_active = SDL_GAMEPAD_AXIS_INVALID;
static float virtual_axis_start_x;
static float virtual_axis_start_y;
static SDL_GamepadButton virtual_button_active = SDL_GAMEPAD_BUTTON_INVALID;


static void PrintJoystickInfo(SDL_JoystickID instance_id)
{
    char guid[64];
    const char *name;
    const char *path;
    const char *description;
    const char *mapping = NULL;

    SDL_GetJoystickGUIDString(SDL_GetJoystickInstanceGUID(instance_id), guid, sizeof(guid));

    if (SDL_IsGamepad(instance_id)) {
        name = SDL_GetGamepadInstanceName(instance_id);
        path = SDL_GetGamepadInstancePath(instance_id);
        switch (SDL_GetGamepadInstanceType(instance_id)) {
        case SDL_GAMEPAD_TYPE_AMAZON_LUNA:
            description = "Amazon Luna Controller";
            break;
        case SDL_GAMEPAD_TYPE_GOOGLE_STADIA:
            description = "Google Stadia Controller";
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            description = "Nintendo Switch Joy-Con";
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            description = "Nintendo Switch Pro Controller";
            break;
        case SDL_GAMEPAD_TYPE_PS3:
            description = "PS3 Controller";
            break;
        case SDL_GAMEPAD_TYPE_PS4:
            description = "PS4 Controller";
            break;
        case SDL_GAMEPAD_TYPE_PS5:
            description = "PS5 Controller";
            break;
        case SDL_GAMEPAD_TYPE_XBOX360:
            description = "XBox 360 Controller";
            break;
        case SDL_GAMEPAD_TYPE_XBOXONE:
            description = "XBox One Controller";
            break;
        case SDL_GAMEPAD_TYPE_VIRTUAL:
            description = "Virtual Gamepad";
            break;
        default:
            description = "Gamepad";
            break;
        }
        mapping = SDL_GetGamepadInstanceMapping(instance_id);
    } else {
        name = SDL_GetJoystickInstanceName(instance_id);
        path = SDL_GetJoystickInstancePath(instance_id);
        description = "Joystick";
    }
    SDL_Log("%s: %s%s%s (guid %s, VID 0x%.4x, PID 0x%.4x, player index = %d)\n",
            description, name ? name : "Unknown", path ? ", " : "", path ? path : "", guid,
            SDL_GetJoystickInstanceVendor(instance_id),
            SDL_GetJoystickInstanceProduct(instance_id),
            SDL_GetJoystickInstancePlayerIndex(instance_id));
    if (mapping) {
        SDL_Log("Mapping: %s\n", mapping);
    }
}

static void UpdateWindowTitle(void)
{
    if (window == NULL) {
        return;
    }

    if (gamepad) {
        const char *name = SDL_GetGamepadName(gamepad);
        const char *serial = SDL_GetGamepadSerial(gamepad);
        const char *basetitle = "Gamepad Test: ";
        const size_t titlelen = SDL_strlen(basetitle) + (name ? SDL_strlen(name) : 0) + (serial ? 3 + SDL_strlen(serial) : 0) + 1;
        char *title = (char *)SDL_malloc(titlelen);

        retval = SDL_FALSE;
        done = SDL_FALSE;

        if (title) {
            SDL_strlcpy(title, basetitle, titlelen);
            if (name) {
                SDL_strlcat(title, name, titlelen);
            }
            if (serial) {
                SDL_strlcat(title, " (", titlelen);
                SDL_strlcat(title, serial, titlelen);
                SDL_strlcat(title, ")", titlelen);
            }
            SDL_SetWindowTitle(window, title);
            SDL_free(title);
        }

        if (SDL_GetNumGamepadTouchpads(gamepad) > 0) {
            SetGamepadImageShowingTouchpad(image, SDL_TRUE);
        } else {
            SetGamepadImageShowingTouchpad(image, SDL_FALSE);
        }
    } else {
        SDL_SetWindowTitle(window, "Waiting for gamepad...");
    }
}

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

static int FindGamepad(SDL_JoystickID gamepad_id)
{
    int i;

    for (i = 0; i < num_gamepads; ++i) {
        if (gamepad_id == SDL_GetJoystickInstanceID(SDL_GetGamepadJoystick(gamepads[i]))) {
            return i;
        }
    }
    return -1;
}

static void AddGamepad(SDL_JoystickID gamepad_id, SDL_bool verbose)
{
    SDL_Gamepad **new_gamepads;
    SDL_Gamepad *new_gamepad;
    Uint16 firmware_version;
    SDL_SensorType sensors[] = {
        SDL_SENSOR_ACCEL,
        SDL_SENSOR_GYRO,
        SDL_SENSOR_ACCEL_L,
        SDL_SENSOR_GYRO_L,
        SDL_SENSOR_ACCEL_R,
        SDL_SENSOR_GYRO_R
    };
    unsigned int i;

    if (FindGamepad(gamepad_id) >= 0) {
        /* We already have this gamepad */
        return;
    }

    new_gamepad = SDL_OpenGamepad(gamepad_id);
    if (new_gamepad == NULL) {
        SDL_Log("Couldn't open gamepad: %s\n", SDL_GetError());
        return;
    }

    new_gamepads = (SDL_Gamepad **)SDL_realloc(gamepads, (num_gamepads + 1) * sizeof(*gamepads));
    if (new_gamepads == NULL) {
        SDL_CloseGamepad(gamepad);
        return;
    }

    new_gamepads[num_gamepads++] = new_gamepad;
    gamepads = new_gamepads;
    gamepad = new_gamepad;
    trigger_effect = 0;

    if (verbose) {
        const char *name = SDL_GetGamepadName(gamepad);
        const char *path = SDL_GetGamepadPath(gamepad);
        SDL_Log("Opened gamepad %s%s%s\n", name, path ? ", " : "", path ? path : "");
    }

    firmware_version = SDL_GetGamepadFirmwareVersion(gamepad);
    if (firmware_version) {
        if (verbose) {
            SDL_Log("Firmware version: 0x%x (%d)\n", firmware_version, firmware_version);
        }
    }

    for (i = 0; i < SDL_arraysize(sensors); ++i) {
        SDL_SensorType sensor = sensors[i];

        if (SDL_GamepadHasSensor(gamepad, sensor)) {
            if (verbose) {
                SDL_Log("Enabling %s at %.2f Hz\n", GetSensorName(sensor), SDL_GetGamepadSensorDataRate(gamepad, sensor));
            }
            SDL_SetGamepadSensorEnabled(gamepad, sensor, SDL_TRUE);
        }
    }

    if (SDL_GamepadHasRumble(gamepad)) {
        SDL_Log("Rumble supported");
    }

    if (SDL_GamepadHasRumbleTriggers(gamepad)) {
        SDL_Log("Trigger rumble supported");
    }

    UpdateWindowTitle();
}

static void SetGamepad(SDL_JoystickID gamepad_id)
{
    int i = FindGamepad(gamepad_id);

    if (i < 0) {
        return;
    }

    if (gamepad != gamepads[i]) {
        gamepad = gamepads[i];
        UpdateWindowTitle();
    }
}

static void DelGamepad(SDL_JoystickID gamepad_id)
{
    int i = FindGamepad(gamepad_id);

    if (i < 0) {
        return;
    }

    SDL_CloseGamepad(gamepads[i]);

    --num_gamepads;
    if (i < num_gamepads) {
        SDL_memcpy(&gamepads[i], &gamepads[i + 1], (num_gamepads - i) * sizeof(*gamepads));
    }

    if (num_gamepads > 0) {
        gamepad = gamepads[0];
    } else {
        gamepad = NULL;
    }
    UpdateWindowTitle();
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

static void CyclePS5TriggerEffect(void)
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

    trigger_effect = (trigger_effect + 1) % SDL_arraysize(effects);

    SDL_zero(state);
    state.ucEnableBits1 |= (0x04 | 0x08); /* Modify right and left trigger effect respectively */
    SDL_memcpy(state.rgucRightTriggerEffect, effects[trigger_effect], sizeof(effects[trigger_effect]));
    SDL_memcpy(state.rgucLeftTriggerEffect, effects[trigger_effect], sizeof(effects[trigger_effect]));
    SDL_SendGamepadEffect(gamepad, &state, sizeof(state));
}

static SDL_bool ShowingFront(void)
{
    SDL_bool showing_front = SDL_TRUE;
    int i;

    if (gamepad) {
        /* Show the back of the gamepad if the paddles are being held */
        for (i = SDL_GAMEPAD_BUTTON_PADDLE1; i <= SDL_GAMEPAD_BUTTON_PADDLE4; ++i) {
            if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)i) == SDL_PRESSED) {
                showing_front = SDL_FALSE;
                break;
            }
        }
    }
    if (SDL_GetModState() & SDL_KMOD_SHIFT) {
        showing_front = SDL_FALSE;
    }
    return showing_front;
}

static void SDLCALL VirtualGamepadSetPlayerIndex(void *userdata, int player_index)
{
    SDL_Log("Virtual Gamepad: player index set to %d\n", player_index);
}

static int SDLCALL VirtualGamepadRumble(void *userdata, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_Log("Virtual Gamepad: rumble set to %d/%d\n", low_frequency_rumble, high_frequency_rumble);
    return 0;
}

static int SDLCALL VirtualGamepadRumbleTriggers(void *userdata, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_Log("Virtual Gamepad: trigger rumble set to %d/%d\n", left_rumble, right_rumble);
    return 0;
}

static int SDLCALL VirtualGamepadSetLED(void *userdata, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_Log("Virtual Gamepad: LED set to RGB %d,%d,%d\n", red, green, blue);
    return 0;
}

static void OpenVirtualGamepad(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID virtual_id;

    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_MAX;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_MAX;
    desc.SetPlayerIndex = VirtualGamepadSetPlayerIndex;
    desc.Rumble = VirtualGamepadRumble;
    desc.RumbleTriggers = VirtualGamepadRumbleTriggers;
    desc.SetLED = VirtualGamepadSetLED;

    virtual_id = SDL_AttachVirtualJoystickEx(&desc);
    if (virtual_id == 0) {
        SDL_Log("Couldn't attach virtual device: %s\n", SDL_GetError());
    } else {
        virtual_joystick = SDL_OpenJoystick(virtual_id);
        if (virtual_joystick == NULL) {
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

static SDL_GamepadButton FindButtonAtPosition(float x, float y)
{
    return GetGamepadImageButtonAt(image, x, y);
}

static SDL_GamepadAxis FindAxisAtPosition(float x, float y)
{
    return GetGamepadImageAxisAt(image, x, y);
}

static void VirtualGamepadMouseMotion(float x, float y)
{
    if (virtual_button_active != SDL_GAMEPAD_BUTTON_INVALID) {
        if (virtual_axis_active != SDL_GAMEPAD_AXIS_INVALID) {
            const float MOVING_DISTANCE = 2.0f;
            if (SDL_fabs(x - virtual_axis_start_x) >= MOVING_DISTANCE ||
                SDL_fabs(y - virtual_axis_start_y) >= MOVING_DISTANCE) {
                SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, SDL_RELEASED);
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
}

static void VirtualGamepadMouseDown(float x, float y)
{
    SDL_GamepadButton button;
    SDL_GamepadAxis axis;

    button = FindButtonAtPosition(x, y);
    if (button != SDL_GAMEPAD_BUTTON_INVALID) {
        virtual_button_active = button;
        SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, SDL_PRESSED);
    }

    axis = FindAxisAtPosition(x, y);
    if (axis != SDL_GAMEPAD_AXIS_INVALID) {
        virtual_axis_active = axis;
        virtual_axis_start_x = x;
        virtual_axis_start_y = y;
    }
}

static void VirtualGamepadMouseUp(float x, float y)
{
    if (virtual_button_active != SDL_GAMEPAD_BUTTON_INVALID) {
        SDL_SetJoystickVirtualButton(virtual_joystick, virtual_button_active, SDL_RELEASED);
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
}

static void DrawGamepadWaiting(SDL_Renderer *renderer)
{
    const char *text = "Waiting for gamepad, press A to add a virtual controller";
    float x, y;

    x = (float)SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
    y = (float)TITLE_HEIGHT / 2 - FONT_CHARACTER_SIZE / 2;
    SDLTest_DrawString(renderer, x, y, text);
}

static void DrawGamepadInfo(SDL_Renderer *renderer, SDL_Gamepad *gamepad)
{
    const char *name;
    const char *serial;
    char text[128];
    float x, y;

    name = SDL_GetGamepadName(gamepad);
    if (name && *name) {
        x = (float)SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(name)) / 2;
        y = (float)TITLE_HEIGHT / 2 - FONT_CHARACTER_SIZE / 2;
        SDLTest_DrawString(renderer, x, y, name);
    }

    if (SDL_IsJoystickVirtual(SDL_GetGamepadInstanceID(gamepad))) {
        SDL_strlcpy(text, "Click on the gamepad image below to generate input", sizeof(text));
        x = (float)SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
        y = (float)TITLE_HEIGHT / 2 - FONT_CHARACTER_SIZE / 2 + FONT_LINE_HEIGHT + 2.0f;
        SDLTest_DrawString(renderer, x, y, text);
    }

    SDL_snprintf(text, SDL_arraysize(text), "VID: 0x%.4x PID: 0x%.4x", SDL_GetGamepadVendor(gamepad), SDL_GetGamepadProduct(gamepad));
    y = (float)SCREEN_HEIGHT - 8.0f - FONT_LINE_HEIGHT;
    x = (float)SCREEN_WIDTH - 8.0f - (FONT_CHARACTER_SIZE * SDL_strlen(text));
    SDLTest_DrawString(renderer, x, y, text);

    serial = SDL_GetGamepadSerial(gamepad);
    if (serial && *serial) {
        SDL_snprintf(text, SDL_arraysize(text), "Serial: %s", serial);
        x = (float)SCREEN_WIDTH / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(text)) / 2;
        y = (float)SCREEN_HEIGHT - 8.0f - FONT_LINE_HEIGHT;
        SDLTest_DrawString(renderer, x, y, text);
    }
}

static void loop(void *arg)
{
    SDL_Event event;

    /* Update to get the current event state */
    SDL_PumpEvents();

    /* Process all currently pending events */
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
        SDL_ConvertEventToRenderCoordinates(screen, &event);

        switch (event.type) {
        case SDL_EVENT_JOYSTICK_ADDED:
            PrintJoystickInfo(event.jdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            SDL_Log("Gamepad device %" SDL_PRIu32 " added.\n",
                    event.gdevice.which);
            AddGamepad(event.gdevice.which, SDL_TRUE);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            SDL_Log("Gamepad device %" SDL_PRIu32 " removed.\n",
                    event.gdevice.which);
            DelGamepad(event.gdevice.which);
            break;

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

#define VERBOSE_SENSORS
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

#define VERBOSE_AXES
#ifdef VERBOSE_AXES
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (event.gaxis.value <= (-SDL_JOYSTICK_AXIS_MAX / 2) || event.gaxis.value >= (SDL_JOYSTICK_AXIS_MAX / 2)) {
                SetGamepad(event.gaxis.which);
            }
            SDL_Log("Gamepad %" SDL_PRIu32 " axis %s changed to %d\n",
                    event.gaxis.which,
                    SDL_GetGamepadStringForAxis((SDL_GamepadAxis) event.gaxis.axis),
                    event.gaxis.value);
            break;
#endif /* VERBOSE_AXES */

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                SetGamepad(event.gbutton.which);
            }
            SDL_Log("Gamepad %" SDL_PRIu32 " button %s %s\n",
                    event.gbutton.which,
                    SDL_GetGamepadStringForButton((SDL_GamepadButton) event.gbutton.button),
                    event.gbutton.state ? "pressed" : "released");

            /* Cycle PS5 trigger effects when the microphone button is pressed */
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                event.gbutton.button == SDL_GAMEPAD_BUTTON_MISC1 &&
                SDL_GetGamepadType(gamepad) == SDL_GAMEPAD_TYPE_PS5) {
                CyclePS5TriggerEffect();
            }
            break;

        case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
            SDL_Log("Gamepad %" SDL_PRIu32 " battery state changed to %s\n", event.jbattery.which, power_level_strings[event.jbattery.level + 1]);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (virtual_joystick) {
                VirtualGamepadMouseDown(event.button.x, event.button.y);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (virtual_joystick) {
                VirtualGamepadMouseUp(event.button.x, event.button.y);
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (virtual_joystick) {
                VirtualGamepadMouseMotion(event.motion.x, event.motion.y);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.keysym.sym >= SDLK_0 && event.key.keysym.sym <= SDLK_9) {
                if (gamepad) {
                    int player_index = (event.key.keysym.sym - SDLK_0);

                    SDL_SetGamepadPlayerIndex(gamepad, player_index);
                }
                break;
            }
            if (event.key.keysym.sym == SDLK_a) {
                OpenVirtualGamepad();
                break;
            }
            if (event.key.keysym.sym == SDLK_d) {
                CloseVirtualGamepad();
                break;
            }
            if (event.key.keysym.sym != SDLK_ESCAPE) {
                break;
            }
            SDL_FALLTHROUGH;
        case SDL_EVENT_QUIT:
            done = SDL_TRUE;
            break;
        default:
            break;
        }
    }

    /* blank screen, set up for drawing this frame. */
    SDL_SetRenderDrawColor(screen, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_SetRenderDrawColor(screen, 0x10, 0x10, 0x10, SDL_ALPHA_OPAQUE);

    if (gamepad) {
        SetGamepadImageShowingFront(image, ShowingFront());
        UpdateGamepadImageFromGamepad(image, gamepad);
        RenderGamepadImage(image);

        RenderGamepadDisplay(gamepad_elements, gamepad);
        RenderJoystickDisplay(joystick_elements, SDL_GetGamepadJoystick(gamepad));

        DrawGamepadInfo(screen, gamepad);

        /* Update LED based on left thumbstick position */
        {
            Sint16 x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
            Sint16 y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);

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

                SDL_SetGamepadLED(gamepad, r, g, b);
            }
        }

        if (trigger_effect == 0) {
            /* Update rumble based on trigger state */
            {
                Sint16 left = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
                Sint16 right = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
                Uint16 low_frequency_rumble = ConvertAxisToRumble(left);
                Uint16 high_frequency_rumble = ConvertAxisToRumble(right);
                SDL_RumbleGamepad(gamepad, low_frequency_rumble, high_frequency_rumble, 250);
            }

            /* Update trigger rumble based on thumbstick state */
            {
                Sint16 left = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
                Sint16 right = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
                Uint16 left_rumble = ConvertAxisToRumble(~left);
                Uint16 right_rumble = ConvertAxisToRumble(~right);

                SDL_RumbleGamepadTriggers(gamepad, left_rumble, right_rumble, 250);
            }
        }
    } else {
        DrawGamepadWaiting(screen);
    }
    SDL_Delay(16);
    SDL_RenderPresent(screen);

#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    float content_scale;
    int screen_width, screen_height;
    int gamepad_index = -1;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ROG_CHAKRAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_LINUX_JOYSTICK_DEADZONES, "1");

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--mappings") == 0) {
                int map_i;
                SDL_Log("Supported mappings:\n");
                for (map_i = 0; map_i < SDL_GetNumGamepadMappings(); ++map_i) {
                    char *mapping = SDL_GetGamepadMappingForIndex(map_i);
                    if (mapping) {
                        SDL_Log("\t%s\n", mapping);
                        SDL_free(mapping);
                    }
                }
                SDL_Log("\n");
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");

    /* Create a window to display gamepad state */
    content_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (content_scale == 0.0f) {
        content_scale = 1.0f;
    }
    screen_width = (int)SDL_ceilf(SCREEN_WIDTH * content_scale);
    screen_height = (int)SDL_ceilf(SCREEN_HEIGHT * content_scale);
    window = SDL_CreateWindow("Gamepad Test", screen_width, screen_height, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return 2;
    }

    screen = SDL_CreateRenderer(window, NULL, 0);
    if (screen == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return 2;
    }

    SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_RenderPresent(screen);

    /* scale for platforms that don't give you the window size you asked for. */
    SDL_SetRenderLogicalPresentation(screen, SCREEN_WIDTH, SCREEN_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX,
                                     SDL_SCALEMODE_LINEAR);

    image = CreateGamepadImage(screen);
    if (image == NULL) {
        SDL_DestroyRenderer(screen);
        SDL_DestroyWindow(window);
        return 2;
    }
    SetGamepadImagePosition(image, PANEL_WIDTH + PANEL_SPACING, TITLE_HEIGHT);

    gamepad_elements = CreateGamepadDisplay(screen);
    SetGamepadDisplayArea(gamepad_elements, 0, TITLE_HEIGHT, PANEL_WIDTH, GAMEPAD_HEIGHT);

    joystick_elements = CreateJoystickDisplay(screen);
    SetJoystickDisplayArea(joystick_elements, PANEL_WIDTH + PANEL_SPACING + GAMEPAD_WIDTH + PANEL_SPACING, TITLE_HEIGHT, PANEL_WIDTH, GAMEPAD_HEIGHT);

    /* Process the initial gamepad list */
    loop(NULL);

    if (gamepad_index < num_gamepads) {
        gamepad = gamepads[gamepad_index];
    } else {
        gamepad = NULL;
    }
    UpdateWindowTitle();

    /* Loop, getting gamepad events! */
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop, NULL, 0, 1);
#else
    while (!done) {
        loop(NULL);
    }
#endif

    /* Reset trigger state */
    if (trigger_effect != 0) {
        trigger_effect = -1;
        CyclePS5TriggerEffect();
    }

    CloseVirtualGamepad();
    DestroyGamepadImage(image);
    DestroyGamepadDisplay(gamepad_elements);
    DestroyJoystickDisplay(joystick_elements);
    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
    SDLTest_CommonDestroyState(state);

    return 0;
}
