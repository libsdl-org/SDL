/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
/* This driver supports the Nintendo Switch Pro controller.
   Code and logic contributed by Valve Corporation under the SDL zlib license.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "../../SDL_hints_c.h"
#include "../../misc/SDL_libusb.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"

#ifdef SDL_JOYSTICK_HIDAPI_SWITCH2

typedef struct
{
    SDL_LibUSBContext *libusb;
    libusb_device_handle *device_handle;
    bool interface_claimed;
    Uint8 interface_number;
    Uint8 bulk_endpoint;
} SDL_DriverSwitch2_Context;

static void HIDAPI_DriverSwitch2_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, callback, userdata);
}

static void HIDAPI_DriverSwitch2_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, callback, userdata);
}

static bool HIDAPI_DriverSwitch2_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSwitch2_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id == USB_VENDOR_NINTENDO) {
        switch (product_id) {
        case USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER:
        case USB_PRODUCT_NINTENDO_SWITCH2_PRO:
            return true;
        }
    }

    return false;
}

static bool HIDAPI_DriverSwitch2_InitBluetooth(SDL_HIDAPI_Device *device)
{
    // FIXME: Need to add Bluetooth support
    return SDL_SetError("Nintendo Switch2 controllers not supported over Bluetooth");
}

static bool FindBulkOutEndpoint(SDL_LibUSBContext *libusb, libusb_device_handle *handle, Uint8 *bInterfaceNumber, Uint8 *bEndpointAddress)
{
    struct libusb_config_descriptor *config;
    if (libusb->get_config_descriptor(libusb->get_device(handle), 0, &config) != 0) {
         return false;
    }

    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface *iface = &config->interface[i];
        for (int j = 0; j < iface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[j];
            if (altsetting->bInterfaceNumber == 1) {
                for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                    const struct libusb_endpoint_descriptor *ep = &altsetting->endpoint[k];
                    if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK && (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {

                        *bInterfaceNumber = altsetting->bInterfaceNumber;
                        *bEndpointAddress = ep->bEndpointAddress;
                        libusb->free_config_descriptor(config);
                        return true;
                    }
                }
            }
        }
    }
    libusb->free_config_descriptor(config);
    return false;
}

static bool HIDAPI_DriverSwitch2_InitUSB(SDL_HIDAPI_Device *device)
{
    SDL_DriverSwitch2_Context *ctx = (SDL_DriverSwitch2_Context *)device->context;

    if (!SDL_InitLibUSB(&ctx->libusb)) {
        return false;
    }

    ctx->device_handle = (libusb_device_handle *)SDL_GetPointerProperty(SDL_hid_get_properties(device->dev), SDL_PROP_HIDAPI_LIBUSB_DEVICE_HANDLE_POINTER, NULL);
    if (!ctx->device_handle) {
        return SDL_SetError("Couldn't get libusb device handle");
    }

    if (!FindBulkOutEndpoint(ctx->libusb, ctx->device_handle, &ctx->interface_number, &ctx->bulk_endpoint)) {
        return SDL_SetError("Couldn't find bulk endpoint");
    }

    int res = ctx->libusb->claim_interface(ctx->device_handle, ctx->interface_number);
    if (res < 0) {
        return SDL_SetError("Couldn't claim interface %d: %d\n", ctx->interface_number, res);
    }
    ctx->interface_claimed = true;

    const unsigned char DEFAULT_REPORT_DATA[] = {
        0x03, 0x91, 0x00, 0x0d, 0x00, 0x08,
        0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    const unsigned char SET_LED_DATA[] = {
        0x09, 0x91, 0x00, 0x07, 0x00, 0x08,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    int transferred;
    res = ctx->libusb->bulk_transfer(ctx->device_handle,
                ctx->bulk_endpoint,
                (unsigned char *)DEFAULT_REPORT_DATA,
                sizeof(DEFAULT_REPORT_DATA),
                &transferred,
                1000);
    if (res < 0) {
        return SDL_SetError("Couldn't set report data: %d\n", res);
    }

    res = ctx->libusb->bulk_transfer(ctx->device_handle,
                ctx->bulk_endpoint,
                (unsigned char *)SET_LED_DATA,
                sizeof(SET_LED_DATA),
                &transferred,
                1000);
    if (res < 0) {
        return SDL_SetError("Couldn't set LED data: %d\n", res);
    }

    return true;
}

static bool HIDAPI_DriverSwitch2_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSwitch2_Context *ctx;

    ctx = (SDL_DriverSwitch2_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    device->context = ctx;

    if (device->is_bluetooth) {
        if (!HIDAPI_DriverSwitch2_InitBluetooth(device)) {
            return false;
        }
    } else {
        if (!HIDAPI_DriverSwitch2_InitUSB(device)) {
            return false;
        }
    }

    // Sometimes the device handle isn't available during enumeration so we don't get the device name, so set it explicitly
    switch (device->product_id) {
    case USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER:
        HIDAPI_SetDeviceName(device, "Nintendo GameCube Controller");
        break;
    case USB_PRODUCT_NINTENDO_SWITCH2_PRO:
        HIDAPI_SetDeviceName(device, "Nintendo Switch Pro Controller");
        break;
    default:
        break;
    }
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSwitch2_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSwitch2_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverSwitch2_UpdateDevice(SDL_HIDAPI_Device *device)
{
    const struct {
        int byte;
        unsigned char mask;
    } buttons[] = {
        {3, 0x01}, // B
        {3, 0x02}, // A
        {3, 0x04}, // Y
        {3, 0x08}, // X
        {3, 0x10}, // R (GameCube R Click)
        {3, 0x20}, // ZR (GameCube Z)
        {3, 0x40}, // PLUS (GameCube Start)
        {3, 0x80}, // RS (not on GameCube)
        {4, 0x01}, // DPAD_DOWN
        {4, 0x02}, // DPAD_RIGHT
        {4, 0x04}, // DPAD_LEFT
        {4, 0x08}, // DPAD_UP
        {4, 0x10}, // L (GameCube L Click)
        {4, 0x20}, // ZL
        {4, 0x40}, // MINUS (not on GameCube)
        {4, 0x80}, // LS (not on GameCube)
        {5, 0x01}, // Home
        {5, 0x02}, // Capture
        {5, 0x04}, // GR (not on GameCube)
        {5, 0x08}, // GL (not on GameCube)
        {5, 0x10}, // C
    };

    SDL_Joystick *joystick = NULL;
    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    }
    if (joystick == NULL) {
        return true;
    }

    // Read input packet

    Uint8 packet[USB_PACKET_LENGTH];
    int size;
    while ((size = SDL_hid_read_timeout(device->dev, packet, sizeof(packet), 0)) > 0) {
        if (size < 15) {
            continue;
        }

        Uint64 timestamp = SDL_GetTicksNS();
        for (size_t i = 0; i < SDL_arraysize(buttons); ++i) {
            SDL_SendJoystickButton(
                timestamp,
                joystick,
                (Uint8) i,
                (packet[buttons[i].byte] & buttons[i].mask) != 0
            );
        }
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_LEFTX,
            (Sint16) HIDAPI_RemapVal(
                (float) (packet[6] | ((packet[7] & 0x0F) << 8)),
                0,
                4096,
                SDL_MIN_SINT16,
                SDL_MAX_SINT16
            )
        );
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_LEFTY,
            (Sint16) HIDAPI_RemapVal(
                (float) ((packet[7] >> 4) | (packet[8] << 4)),
                0,
                4096,
                SDL_MIN_SINT16,
                SDL_MAX_SINT16
            )
        );
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_RIGHTX,
            (Sint16) HIDAPI_RemapVal(
                (float) (packet[9] | ((packet[10] & 0x0F) << 8)),
                0,
                4096,
                SDL_MIN_SINT16,
                SDL_MAX_SINT16
            )
        );
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_RIGHTY,
            (Sint16) HIDAPI_RemapVal(
                (float) ((packet[10] >> 4) | (packet[11] << 4)),
                0,
                4096,
                SDL_MIN_SINT16,
                SDL_MAX_SINT16
            )
        );
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
            (Sint16) HIDAPI_RemapVal(packet[13], 0, 255, SDL_MIN_SINT16, SDL_MAX_SINT16)
        );
        SDL_SendJoystickAxis(
            timestamp,
            joystick,
            SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
            (Sint16) HIDAPI_RemapVal(packet[14], 0, 255, SDL_MIN_SINT16, SDL_MAX_SINT16)
        );
    }
    return true;
}

static bool HIDAPI_DriverSwitch2_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    // Initialize the joystick capabilities
    joystick->nbuttons = 21;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;

    return true;
}

static bool HIDAPI_DriverSwitch2_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSwitch2_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverSwitch2_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void HIDAPI_DriverSwitch2_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSwitch2_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSwitch2_Context *ctx = (SDL_DriverSwitch2_Context *)device->context;

    if (ctx) {
        if (ctx->interface_claimed) {
            ctx->libusb->release_interface(ctx->device_handle, ctx->interface_number);
            ctx->interface_claimed = false;
        }
        if (ctx->libusb) {
            SDL_QuitLibUSB();
            ctx->libusb = NULL;
        }
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSwitch2 = {
    SDL_HINT_JOYSTICK_HIDAPI_SWITCH2,
    true,
    HIDAPI_DriverSwitch2_RegisterHints,
    HIDAPI_DriverSwitch2_UnregisterHints,
    HIDAPI_DriverSwitch2_IsEnabled,
    HIDAPI_DriverSwitch2_IsSupportedDevice,
    HIDAPI_DriverSwitch2_InitDevice,
    HIDAPI_DriverSwitch2_GetDevicePlayerIndex,
    HIDAPI_DriverSwitch2_SetDevicePlayerIndex,
    HIDAPI_DriverSwitch2_UpdateDevice,
    HIDAPI_DriverSwitch2_OpenJoystick,
    HIDAPI_DriverSwitch2_RumbleJoystick,
    HIDAPI_DriverSwitch2_RumbleJoystickTriggers,
    HIDAPI_DriverSwitch2_GetJoystickCapabilities,
    HIDAPI_DriverSwitch2_SetJoystickLED,
    HIDAPI_DriverSwitch2_SendJoystickEffect,
    HIDAPI_DriverSwitch2_SetJoystickSensorsEnabled,
    HIDAPI_DriverSwitch2_CloseJoystick,
    HIDAPI_DriverSwitch2_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SWITCH2

#endif // SDL_JOYSTICK_HIDAPI
