/*
  Simple DirectMedia Layer
  Copyright (C) 2023 Max Maisel <max.maisel@posteo.de>

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

#include "../SDL_sysjoystick.h"
#include "SDL_events.h"
#include "SDL_hidapijoystick_c.h"

#ifdef SDL_JOYSTICK_HIDAPI_STEAMDECK

/*****************************************************************************************************/

#include "steam/controller_constants.h"
#include "steam/controller_structs.h"

typedef struct
{
    Uint32 update_rate_us;
    Uint32 sensor_timestamp_us;
    Uint64 last_button_state;
    Uint8 watchdog_counter;
} SDL_DriverSteamDeck_Context;

static SDL_bool DisableDeckLizardMode(SDL_hid_device *dev)
{
    int rc;
    Uint8 buffer[HID_FEATURE_REPORT_BYTES + 1] = { 0 };
    FeatureReportMsg *msg = (FeatureReportMsg *)(buffer + 1);

    msg->header.type = ID_CLEAR_DIGITAL_MAPPINGS;

    rc = SDL_hid_send_feature_report(dev, buffer, sizeof(buffer));
    if (rc != sizeof(buffer))
        return SDL_FALSE;

    msg->header.type = ID_SET_SETTINGS_VALUES;
    msg->header.length = 5 * sizeof(WriteDeckRegister);
    msg->payload.wrDeckRegister.reg[0].addr = SETTING_DECK_RPAD_MARGIN; // disable margin
    msg->payload.wrDeckRegister.reg[0].val = 0;
    msg->payload.wrDeckRegister.reg[1].addr = SETTING_DECK_LPAD_MODE; // disable mouse
    msg->payload.wrDeckRegister.reg[1].val = 7;
    msg->payload.wrDeckRegister.reg[2].addr = SETTING_DECK_RPAD_MODE; // disable mouse
    msg->payload.wrDeckRegister.reg[2].val = 7;
    msg->payload.wrDeckRegister.reg[3].addr = SETTING_DECK_LPAD_CLICK_PRESSURE; // disable clicky pad
    msg->payload.wrDeckRegister.reg[3].val = 0xFFFF;
    msg->payload.wrDeckRegister.reg[4].addr = SETTING_DECK_RPAD_CLICK_PRESSURE; // disable clicky pad
    msg->payload.wrDeckRegister.reg[4].val = 0xFFFF;

    rc = SDL_hid_send_feature_report(dev, buffer, sizeof(buffer));
    if (rc != sizeof(buffer))
        return SDL_FALSE;

    // There may be a lingering report read back after changing settings.
    // Discard it.
    SDL_hid_get_feature_report(dev, buffer, sizeof(buffer));

    return SDL_TRUE;
}

static SDL_bool FeedDeckLizardWatchdog(SDL_hid_device *dev)
{
    int rc;
    Uint8 buffer[HID_FEATURE_REPORT_BYTES + 1] = { 0 };
    FeatureReportMsg *msg = (FeatureReportMsg *)(buffer + 1);

    msg->header.type = ID_CLEAR_DIGITAL_MAPPINGS;

    rc = SDL_hid_send_feature_report(dev, buffer, sizeof(buffer));
    if (rc != sizeof(buffer))
        return SDL_FALSE;

    msg->header.type = ID_SET_SETTINGS_VALUES;
    msg->header.length = 1 * sizeof(WriteDeckRegister);
    msg->payload.wrDeckRegister.reg[0].addr = SETTING_DECK_RPAD_MODE; // disable mouse
    msg->payload.wrDeckRegister.reg[0].val = 7;

    rc = SDL_hid_send_feature_report(dev, buffer, sizeof(buffer));
    if (rc != sizeof(buffer))
        return SDL_FALSE;

    // There may be a lingering report read back after changing settings.
    // Discard it.
    SDL_hid_get_feature_report(dev, buffer, sizeof(buffer));

    return SDL_TRUE;
}

/*****************************************************************************************************/

static void HIDAPI_DriverSteamDeck_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, callback, userdata);
}

static void HIDAPI_DriverSteamDeck_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, callback, userdata);
}

static SDL_bool HIDAPI_DriverSteamDeck_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,
                              SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static SDL_bool HIDAPI_DriverSteamDeck_IsSupportedDevice(
    SDL_HIDAPI_Device *device,
    const char *name,
    SDL_GameControllerType type,
    Uint16 vendor_id,
    Uint16 product_id,
    Uint16 version,
    int interface_number,
    int interface_class,
    int interface_subclass,
    int interface_protocol)
{
    return SDL_IsJoystickSteamDeck(vendor_id, product_id);
}

static SDL_bool HIDAPI_DriverSteamDeck_InitDevice(SDL_HIDAPI_Device *device)
{
    int size;
    Uint8 data[64];
    SDL_DriverSteamDeck_Context *ctx;

    ctx = (SDL_DriverSteamDeck_Context *)SDL_calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }

    // Always 1kHz according to USB descriptor
    ctx->update_rate_us = 1000;

    device->context = ctx;

    // Read a report to see if this is the correct endpoint.
    // Mouse, Keyboard and Controller have the same VID/PID but
    // only the controller hidraw device receives hid reports.
    size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 16);
    if (size == 0)
        return SDL_FALSE;

    if (!DisableDeckLizardMode(device->dev))
        return SDL_FALSE;

    HIDAPI_SetDeviceName(device, "Steam Deck");

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSteamDeck_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSteamDeck_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static SDL_bool HIDAPI_DriverSteamDeck_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSteamDeck_Context *ctx = (SDL_DriverSteamDeck_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    int r;
    uint8_t data[64];
    float values[3];
    ValveInReport_t *pInReport = (ValveInReport_t *)data;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
        if (joystick == NULL) {
            return SDL_FALSE;
        }
    } else {
        return SDL_FALSE;
    }

    if (ctx->watchdog_counter++ > 200) {
        ctx->watchdog_counter = 0;
        if (!FeedDeckLizardWatchdog(device->dev))
            return SDL_FALSE;
    }

    SDL_memset(data, 0, sizeof(data));
    r = SDL_hid_read(device->dev, data, sizeof(data));
    if (r == 0) {
        return SDL_FALSE;
    } else if (r <= 0) {
        /* Failed to read from controller */
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
        return SDL_FALSE;
    }

    if (!(r == 64 && pInReport->header.unReportVersion == k_ValveInReportMsgVersion && pInReport->header.ucType == ID_CONTROLLER_DECK_STATE && pInReport->header.ucLength == 64)) {
        return SDL_FALSE;
    }

    // Uint64 timestamp = SDL_GetTicksNS();

    if (pInReport->payload.deckState.ulButtons != ctx->last_button_state) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_A) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_B) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_X) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_Y) ? SDL_PRESSED : SDL_RELEASED);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_LT) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_RT) ? SDL_PRESSED : SDL_RELEASED);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_SELECT) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_START) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_MODE) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MISC1,
                                  (pInReport->payload.deckState.ulButtonsH & STEAMDECK_HBUTTON_BASE) ? SDL_PRESSED : SDL_RELEASED);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_STICKL) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_STICKR) ? SDL_PRESSED : SDL_RELEASED);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_PADDLE1,
                                  (pInReport->payload.deckState.ulButtonsH & STEAMDECK_HBUTTON_PADDLE1) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_PADDLE2,
                                  (pInReport->payload.deckState.ulButtonsH & STEAMDECK_HBUTTON_PADDLE2) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_PADDLE3,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_PADDLE3) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_PADDLE4,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_PADDLE4) ? SDL_PRESSED : SDL_RELEASED);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_DPAD_UP) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_DPAD_DOWN) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_DPAD_LEFT) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                                  (pInReport->payload.deckState.ulButtonsL & STEAMDECK_LBUTTON_DPAD_RIGHT) ? SDL_PRESSED : SDL_RELEASED);
        ctx->last_button_state = pInReport->payload.deckState.ulButtons;
    }

    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT,
                            (int)pInReport->payload.deckState.sLeftTrigger * 2 - 32768);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
                            (int)pInReport->payload.deckState.sRightTrigger * 2 - 32768);

    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX,
                            pInReport->payload.deckState.sLeftStickX);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY,
                            -pInReport->payload.deckState.sLeftStickY);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX,
                            pInReport->payload.deckState.sRightStickX);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY,
                            -pInReport->payload.deckState.sRightStickY);

    ctx->sensor_timestamp_us += ctx->update_rate_us;

    values[0] = (pInReport->payload.deckState.sGyroX / 32768.0f) * (2000.0f * ((float)M_PI / 180.0f));
    values[1] = (pInReport->payload.deckState.sGyroZ / 32768.0f) * (2000.0f * ((float)M_PI / 180.0f));
    values[2] = (-pInReport->payload.deckState.sGyroY / 32768.0f) * (2000.0f * ((float)M_PI / 180.0f));
    SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_GYRO, ctx->sensor_timestamp_us, values, 3);

    values[0] = (pInReport->payload.deckState.sAccelX / 32768.0f) * 2.0f * SDL_STANDARD_GRAVITY;
    values[1] = (pInReport->payload.deckState.sAccelZ / 32768.0f) * 2.0f * SDL_STANDARD_GRAVITY;
    values[2] = (-pInReport->payload.deckState.sAccelY / 32768.0f) * 2.0f * SDL_STANDARD_GRAVITY;
    SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_ACCEL, ctx->sensor_timestamp_us, values, 3);

    return SDL_TRUE;
}

static SDL_bool HIDAPI_DriverSteamDeck_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSteamDeck_Context *ctx = (SDL_DriverSteamDeck_Context *)device->context;
    float update_rate_in_hz = 1.0f / (float)(ctx->update_rate_us) * 1.0e6f;

    SDL_AssertJoysticksLocked();

    // Initialize the joystick capabilities
    joystick->nbuttons = 20;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;

    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, update_rate_in_hz);
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, update_rate_in_hz);

    return SDL_TRUE;
}

static int HIDAPI_DriverSteamDeck_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    /* You should use the full Steam Input API for rumble support */
    return SDL_Unsupported();
}

static int HIDAPI_DriverSteamDeck_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSteamDeck_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static int HIDAPI_DriverSteamDeck_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int HIDAPI_DriverSteamDeck_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static int HIDAPI_DriverSteamDeck_SetSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, SDL_bool enabled)
{
    // On steam deck, sensors are enabled by default. Nothing to do here.
    return 0;
}

static void HIDAPI_DriverSteamDeck_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    // Lizard mode id automatically re-enabled by watchdog. Nothing to do here.
}

static void HIDAPI_DriverSteamDeck_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteamDeck = {
    SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,
    SDL_TRUE,
    HIDAPI_DriverSteamDeck_RegisterHints,
    HIDAPI_DriverSteamDeck_UnregisterHints,
    HIDAPI_DriverSteamDeck_IsEnabled,
    HIDAPI_DriverSteamDeck_IsSupportedDevice,
    HIDAPI_DriverSteamDeck_InitDevice,
    HIDAPI_DriverSteamDeck_GetDevicePlayerIndex,
    HIDAPI_DriverSteamDeck_SetDevicePlayerIndex,
    HIDAPI_DriverSteamDeck_UpdateDevice,
    HIDAPI_DriverSteamDeck_OpenJoystick,
    HIDAPI_DriverSteamDeck_RumbleJoystick,
    HIDAPI_DriverSteamDeck_RumbleJoystickTriggers,
    HIDAPI_DriverSteamDeck_GetJoystickCapabilities,
    HIDAPI_DriverSteamDeck_SetJoystickLED,
    HIDAPI_DriverSteamDeck_SendJoystickEffect,
    HIDAPI_DriverSteamDeck_SetSensorsEnabled,
    HIDAPI_DriverSteamDeck_CloseJoystick,
    HIDAPI_DriverSteamDeck_FreeDevice,
};

#endif /* SDL_JOYSTICK_HIDAPI_STEAMDECK */

#endif /* SDL_JOYSTICK_HIDAPI */
