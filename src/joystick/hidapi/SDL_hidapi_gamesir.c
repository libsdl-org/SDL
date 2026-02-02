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

#ifdef SDL_JOYSTICK_HIDAPI

#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"

/* ========================================================================= */
/*  Win32 HID helper                                                         */
/* ========================================================================= */

/* This helper requires full desktop Win32 HID APIs.
 * These APIs are NOT available on GDK platforms.
 */
#if defined(SDL_PLATFORM_WIN32) && !defined(SDL_PLATFORM_GDK)
#define SDL_HAS_WIN32_HID 1
#else
#define SDL_HAS_WIN32_HID 0
#endif

#if SDL_HAS_WIN32_HID

/* --- Win32 HID includes ------------------------------------------------- */
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#if defined(_MSC_VER)
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#endif

static char *FindHIDInterfacePath(Uint16 vid, Uint16 pid, int collection_index)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        &hidGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

    for (DWORD i = 0;
         SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, i, &deviceInterfaceData);
         i++) {

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(
            deviceInfoSet, &deviceInterfaceData,
            NULL, 0, &requiredSize, NULL
        );

        PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetail =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)SDL_malloc(requiredSize);
        if (!deviceDetail) {
            continue;
        }

        deviceDetail->cbSize = sizeof(*deviceDetail);

        if (!SetupDiGetDeviceInterfaceDetail(
                deviceInfoSet, &deviceInterfaceData,
                deviceDetail, requiredSize, NULL, NULL)) {
            SDL_free(deviceDetail);
            continue;
        }

        HANDLE hDevice = CreateFile(
            deviceDetail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL
        );

        if (hDevice == INVALID_HANDLE_VALUE) {
            SDL_free(deviceDetail);
            continue;
        }

        HIDD_ATTRIBUTES attributes;
        attributes.Size = sizeof(attributes);

        if (!HidD_GetAttributes(hDevice, &attributes) ||
            attributes.VendorID != vid ||
            attributes.ProductID != pid) {
            CloseHandle(hDevice);
            SDL_free(deviceDetail);
            continue;
        }

        PHIDP_PREPARSED_DATA preparsedData = NULL;
        if (!HidD_GetPreparsedData(hDevice, &preparsedData) || !preparsedData) {
            CloseHandle(hDevice);
            SDL_free(deviceDetail);
            continue;
        }

        HIDP_CAPS caps;
        if (HidP_GetCaps(preparsedData, &caps) != HIDP_STATUS_SUCCESS) {
            HidD_FreePreparsedData(preparsedData);
            CloseHandle(hDevice);
            SDL_free(deviceDetail);
            continue;
        }

        if ((caps.InputReportByteLength == 64 && caps.OutputReportByteLength == 64) ||
            (caps.InputReportByteLength == 37 && caps.OutputReportByteLength == 37)) {

            char col_str[16];
            SDL_snprintf(col_str, sizeof(col_str), "col%02d", collection_index);

            if (SDL_strcasestr(deviceDetail->DevicePath, col_str)) {
                char *result = SDL_strdup(deviceDetail->DevicePath);
                HidD_FreePreparsedData(preparsedData);
                CloseHandle(hDevice);
                SDL_free(deviceDetail);
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return result;
            }
        }

        HidD_FreePreparsedData(preparsedData);
        CloseHandle(hDevice);
        SDL_free(deviceDetail);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return NULL;
}

#else  /* !SDL_HAS_WIN32_HID */
/* Stub for GDK / non-Win32 platforms */

#if defined(__GNUC__) || defined(__clang__)
#define SDL_UNUSED_FUNC __attribute__((unused))
#else
#define SDL_UNUSED_FUNC
#endif

static char *FindHIDInterfacePath(Uint16 vid, Uint16 pid, int collection_index) SDL_UNUSED_FUNC;
static char *FindHIDInterfacePath(Uint16 vid, Uint16 pid, int collection_index)
{
    (void)vid;
    (void)pid;
    (void)collection_index;
    return NULL;
}

#endif /* SDL_HAS_WIN32_HID */

#ifdef SDL_JOYSTICK_HIDAPI_GAMESIR

#define GAMESIR_PACKET_HEADER_0 0xA1
#define GAMESIR_PACKET_HEADER_1_GAMEPAD 0xC8


#define BTN_A        0x01
#define BTN_B        0x02
#define BTN_C        0x04
#define BTN_X        0x08
#define BTN_Y        0x10
#define BTN_Z        0x20
#define BTN_L1       0x40
#define BTN_R1       0x80


#define BTN_L2       0x01
#define BTN_R2       0x02
#define BTN_SELECT   0x04
#define BTN_START    0x08
#define BTN_HOME     0x10
#define BTN_L3       0x20
#define BTN_R3       0x40
#define BTN_CAPTURE  0x80


#define BTN_UP       0x01
#define BTN_UP_L     0x08
#define BTN_UP_R     0x02
#define BTN_DOWN     0x05
#define BTN_DOWN_L   0x06
#define BTN_DOWN_R   0X04
#define BTN_LEFT     0x07
#define BTN_RIGHT    0x03


#define BTN_M        0x10
#define BTN_MUTE     0x20
#define BTN_L4       0x40
#define BTN_R4       0x80


#define BTN_L5       0x01
#define BTN_R5       0x02
#define BTN_L6       0x04
#define BTN_R6       0x08
#define BTN_L7       0x10
#define BTN_R7       0x20
#define BTN_L8       0x40
#define BTN_R8       0x80

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif

#define LOAD16(A, B) (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))

typedef struct {
    Uint8 cmd;
    Uint8 mode;
} Gamesir_CommandMode;

typedef struct {
    bool sensors_supported;
    bool sensors_enabled;
    Uint64 sensor_timestamp_ns;
    Uint64 sensor_timestamp_step_ns;
    float accelScale;
    float gyroScale;
    Uint8 last_state[USB_PACKET_LENGTH];
    SDL_hid_device *output_handle;
} SDL_DriverGamesir_Context;


static void HIDAPI_DriverGameSir_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_GAMESIR, callback, userdata);
}


static void HIDAPI_DriverGameSir_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_GAMESIR, callback, userdata);
}


static bool HIDAPI_DriverGameSir_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_GAMESIR, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}


static bool HIDAPI_DriverGameSir_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return SDL_IsJoystickGameSirController(vendor_id, product_id);
}


static bool SendGameSirModeSwitch(SDL_HIDAPI_Device *device)
{
    Gamesir_CommandMode cmd = { 0x01 };
    Uint8 buf[64];
    SDL_zero(buf);
    buf[0] = 0xA2;
    SDL_memcpy(buf + 1, &cmd, sizeof(cmd));

    SDL_hid_device *handle = NULL;
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (ctx && ctx->output_handle) {
        handle = ctx->output_handle;
    }
#else
    handle = device->dev;
#endif
    if (handle == NULL) {
        SDL_SetError("Gamesir: output handle is null");
        return false;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        int result = SDL_hid_write(handle, buf, sizeof(buf));
        if (result < 0) {
            return false;
        }


        for (int i = 0; i < 10; ++i) {
            SDL_Delay(1);

            Uint8 data[USB_PACKET_LENGTH] = {0};
            int size = SDL_hid_read_timeout(handle, data, sizeof(data), 0);
            if (size < 0) {
                break;
            }
            if (size == 0) {
                continue;
            }

            if (size == 64 && data[0] == 0xA1 && data[1] == 0x43 && data[2] == 0x01) {
                return true;
            }
        }
    }

    SDL_Delay(10);
    return true;
}

static bool HIDAPI_DriverGameSir_InitDevice(SDL_HIDAPI_Device *device)
{
    Uint16 vendor_id = device->vendor_id;
    Uint16 product_id = device->product_id;
    SDL_hid_device *output_handle = NULL;

    struct SDL_hid_device_info *devs = SDL_hid_enumerate(vendor_id, product_id);
    for (struct SDL_hid_device_info *info = devs; info; info = info->next) {
        if (info->interface_number == 0) {
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
            if (!output_handle) {
                char *col02_path = FindHIDInterfacePath(vendor_id, product_id, 2);
                if (col02_path) {
                    output_handle = SDL_hid_open_path(col02_path);
                    SDL_free(col02_path);
                }
            }
#else

#endif
        }
        if (info->interface_number == -1) {
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
            if (info->usage_page == 0x0001 && info->usage == 0x0005) {
                output_handle = SDL_hid_open_path(info->path);
                if (output_handle) {
                    SDL_Log("GameSir: Opened output interface via path pattern");
                    break;
                }

            }
#endif
        }
        else if (!output_handle && info->interface_number == 1) {
            output_handle = SDL_hid_open_path(info->path);
        }
    }
    SDL_hid_free_enumeration(devs);

    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_hid_close(device->dev);
        if (output_handle) {
            SDL_hid_close(output_handle);
        }
        return false;
    }

    ctx->output_handle = output_handle;
    device->context = ctx;


    switch (device->product_id) {
    case USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_HID:
        HIDAPI_SetDeviceName(device, "GameSir-G7 Pro (HID)");
        ctx->sensors_supported = true;
        SDL_Log("GameSir: Device detected - G7 Pro HID mode (PID 0x%04X)", device->product_id);
        break;
    case USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_8K_HID:
        HIDAPI_SetDeviceName(device, "GameSir-G7 Pro 8K (HID)");
        ctx->sensors_supported = true;
        SDL_Log("GameSir: Device detected - G7 Pro 8K HID mode (PID 0x%04X)", device->product_id);
        break;
    default:
        HIDAPI_SetDeviceName(device, "GameSir Controller");
        break;
    }
    SendGameSirModeSwitch(device);

    return HIDAPI_JoystickConnected(device, NULL);
}


static int HIDAPI_DriverGameSir_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}


static void HIDAPI_DriverGameSir_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}


static bool HIDAPI_DriverGameSir_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;

    SDL_AssertJoysticksLocked();

    if (!ctx) {
        return false;
    }

    SDL_zeroa(ctx->last_state);



    SendGameSirModeSwitch(device);


    if (device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_HID) {
        if (device->is_bluetooth) {
            joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
            SDL_Log("GameSir: Joystick opened - Connection type: Bluetooth (wireless, HID mode)");
        } else {
            joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRED;
            SDL_Log("GameSir: Joystick opened - Connection type: USB/2.4G (HID mode)");
        }
    } else if (device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_8K_HID) {
        if (device->is_bluetooth) {
            joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
            SDL_Log("GameSir: Joystick opened - Connection type: Bluetooth (wireless, 8K HID mode)");
        } else {
            joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRED;
            SDL_Log("GameSir: Joystick opened - Connection type: USB/2.4G (8K HID mode)");
        }
    } else if (device->is_bluetooth) {
        joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
        SDL_Log("GameSir: Joystick opened - Connection type: Bluetooth (wireless)");
    } else {
        joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRED;
        SDL_Log("GameSir: Joystick opened - Connection type: USB (wired)");
    }


    joystick->nbuttons = 35;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    if (ctx->sensors_supported) {


        ctx->sensor_timestamp_step_ns = SDL_NS_PER_SECOND / 125;
        // Accelerometer scale factor: assume a range of ±2g, 16-bit signed values (-32768 to 32767)
        // 32768 corresponds to 2g, so the scale factor = 2 * SDL_STANDARD_GRAVITY / 32768.0f
        ctx->accelScale = 2.0f * SDL_STANDARD_GRAVITY / 32768.0f;

        // Gyro scale factor: based on the PS4 implementation
        // PS4 uses (gyro_numerator / gyro_denominator) * (π / 180)
        // The default value is (1 / 16) * (π / 180), corresponding to a range of approximately ±2048 degrees/second
        // This is a common range for gamepad gyroscopes
        const float gyro_numerator = 1.0f;
        const float gyro_denominator = 16.0f;
        ctx->gyroScale = (gyro_numerator / gyro_denominator) * (SDL_PI_F / 180.0f);

        const float flSensorRate = 1000.0f;
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, flSensorRate);
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, flSensorRate);
    }

    return true;
}


static bool HIDAPI_DriverGameSir_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    if (!device) {
        return false;
    }
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (!ctx) {
        return false;
    }
    Uint8 buf[64];
    SDL_zero(buf);

    buf[0] = 0xA2;
    buf[1] = 0x03;


    buf[2] = (Uint8)(low_frequency_rumble / 256);


    buf[3] = (Uint8)(high_frequency_rumble / 256);

    SDL_hid_device *handle = NULL;
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    if (ctx && ctx->output_handle) {
        handle = ctx->output_handle;
    }
#else
    handle = device->dev;
#endif

    if (handle == NULL) {
        SDL_SetError("Gamesir: output handle is null");
        return false;
    }

    int result = SDL_hid_write(handle, buf, sizeof(buf));
    if (result < 0) {
        SDL_SetError("Gamesir: Failed to send rumble command, error: %s", SDL_GetError());
        return false;
    }

    return true;
}


static bool HIDAPI_DriverGameSir_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    if (!device) {
        return false;
    }
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (!ctx) {
        return false;
    }
    Uint8 buf[64];
    SDL_zero(buf);

    buf[0] = 0xA2;
    buf[1] = 0x03;


    buf[4] = (Uint8)(left_rumble / 256);


    buf[5] = (Uint8)(right_rumble / 256);

    SDL_hid_device *handle = NULL;
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    if (ctx && ctx->output_handle) {
        handle = ctx->output_handle;
    }
#else
    handle = device->dev;
#endif

    if (handle == NULL) {
        SDL_SetError("Gamesir: output handle is null");
        return false;
    }

    int result = SDL_hid_write(handle, buf, sizeof(buf));
    if (result < 0) {
        SDL_SetError("Gamesir: Failed to send trigger rumble command, error: %s", SDL_GetError());
        return false;
    }

    return true;
}


static Uint32 HIDAPI_DriverGameSir_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    Uint32 caps = SDL_JOYSTICK_CAP_RUMBLE | SDL_JOYSTICK_CAP_TRIGGER_RUMBLE;
    if (ctx && ctx->sensors_supported) {


    }
    return caps;
}


static bool HIDAPI_DriverGameSir_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    if (!device) {
        return false;
    }


    if (device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_HID ||
        device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_8K_HID) {
        return SDL_Unsupported();
    }

    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (!ctx) {
        return false;
    }
    Uint8 buf[64];
    SDL_zero(buf);

    buf[0] = 0xA2;
    buf[1] = 0x04;
    buf[2] = 0x01;
    buf[3] = 0x01;


    buf[4] = red;
    buf[5] = green;
    buf[6] = blue;

    SDL_hid_device *handle = NULL;
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    if (ctx && ctx->output_handle) {
        handle = ctx->output_handle;
    }
#else
    handle = device->dev;
#endif

    if (handle == NULL) {
        SDL_SetError("Gamesir: output handle is null");
        return false;
    }

    int result = SDL_hid_write(handle, buf, sizeof(buf));
    if (result < 0) {
        SDL_SetError("Gamesir: Failed to send LED command, error: %s", SDL_GetError());
        return false;
    }

    return true;
}

static bool HIDAPI_DriverGameSir_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverGameSir_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (ctx && ctx->sensors_supported) {
        ctx->sensors_enabled = enabled;
        return true;
    }
    return SDL_Unsupported();
}

static bool ApplyCircularDeadzone(Sint16 x, Sint16 y, Sint16 *out_x, Sint16 *out_y)
{
    const Sint16 MAX_AXIS = 32767;
    const float deadzone_percent = 5.0f;
    const float deadzone_radius = (float)MAX_AXIS * deadzone_percent / 100.0f;


    float distance = SDL_sqrtf((float)x * (float)x + (float)y * (float)y);
    if (distance == 0.0f) {
        *out_x = 0;
        *out_y = 0;
        return false;
    }

    if (distance < deadzone_radius) {

        *out_x = 0;
        *out_y = 0;
        return false;
    }

    float scale = (distance - deadzone_radius) / (MAX_AXIS - deadzone_radius);
    float normalized_x = (float)x / distance;
    float normalized_y = (float)y / distance;

    *out_x = (Sint16)(normalized_x * scale * MAX_AXIS);
    *out_y = (Sint16)(normalized_y * scale * MAX_AXIS);

    return true;
}

static void HIDAPI_DriverGameSir_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverGamesir_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    if (!joystick || !ctx || !data || size <= 0) {
        return;
    }

    // Check packet format: it may include a report ID (0x43) as the first byte
    // Actual packet format: 43 a1 c8 [button data...]
    // If the first byte is 0x43, the second is 0xA1 and the third is 0xC8, skip the report ID
    int packet_size = size;

    if (size >= 3 && data[0] == 0x43 && data[1] == GAMESIR_PACKET_HEADER_0 && data[2] == GAMESIR_PACKET_HEADER_1_GAMEPAD) {
        // Packet contains a report ID; skip the first byte
        packet_size = size - 1;
    } else if (size >= 2 && data[0] == GAMESIR_PACKET_HEADER_0 && data[1] == GAMESIR_PACKET_HEADER_1_GAMEPAD) {
        // Standard format: no report ID, header bytes directly
        packet_size = size;
    } else {
        // Packet format does not match; return immediately
        return;
    }

    bool is_initial_packet = (ctx->last_state[0] == 0 && ctx->last_state[1] == 0 && ctx->last_state[2] == 0 && ctx->last_state[3] == 0);

    const int min_packet_size = ctx->sensors_enabled ? 29 : 17;
    if (packet_size < min_packet_size) {
        return;
    }

    if (ctx->last_state[3] != data[3]) {
        Uint8 buttons = data[3];
        // BTN1: A B C X Y Z L1 R1
        // Use bitwise operations to check whether each button is pressed
        // buttons & BTN_A returns the value of BTN_A (if pressed) or 0 (if not pressed)
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, buttons & BTN_A);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST,  buttons & BTN_B);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST,  buttons & BTN_X);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, buttons & BTN_Y);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  buttons & BTN_L1);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, buttons & BTN_R1);

    }

    if (ctx->last_state[4] != data[4]) {
        Uint8 buttons = data[4];
        // BTN2: L2 R2 SELECT START HOME L3 R3 CAPTURE
        // Note: L2/R2 appear as digital buttons in data[4], but their actual analog values are in data[15]/data[16].
        // Only handle the other buttons here; trigger analog values are processed later in the code.
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, buttons & BTN_SELECT);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, buttons & BTN_START);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, buttons & BTN_HOME);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, buttons & BTN_L3);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, buttons & BTN_R3);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, buttons & BTN_CAPTURE);

    }

    if (ctx->last_state[5] != data[5]) {
        Uint8 buttons = data[5];
        // BTN3: UP DOWN LEFT RIGHT M MUTE L4 R4
        // Handle the directional pad (D-pad)
        Uint8 hat = SDL_HAT_CENTERED;


        if (buttons == BTN_UP_R) {
            hat = SDL_HAT_RIGHTUP;
        } else if (buttons == BTN_UP_L) {
            hat = SDL_HAT_LEFTUP;
        } else if (buttons == BTN_DOWN_R) {
            hat = SDL_HAT_RIGHTDOWN;
        } else if (buttons == BTN_DOWN_L) {
            hat = SDL_HAT_LEFTDOWN;
        } else if (buttons == BTN_UP) {
            hat = SDL_HAT_UP;
        } else if (buttons == BTN_DOWN) {
            hat = SDL_HAT_DOWN;
        } else if (buttons == BTN_LEFT) {
            hat = SDL_HAT_LEFT;
        } else if (buttons == BTN_RIGHT) {
            hat = SDL_HAT_RIGHT;
        } else {
            hat = SDL_HAT_CENTERED;
        }

        SDL_SendJoystickHat(timestamp, joystick, 0, hat);

        // Handle other buttons
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, buttons & BTN_L4);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, buttons & BTN_R4);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC2, buttons & BTN_MUTE);

    }

    if (ctx->last_state[6] != data[6]) {
        Uint8 buttons = data[6];
        // BTN4: L5 R5 L6 R6 L7 R7 L8 R8
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, buttons & BTN_L5);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, buttons & BTN_R5);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC3, buttons & BTN_L6);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC4, buttons & BTN_R6);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC5, buttons & BTN_L7);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC6, buttons & BTN_R7);


    }

    if (is_initial_packet) {
        // Initialize all joystick axes to center positions
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, 0);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, 0);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, 0);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, 0);
    } else {
        // Left stick handling
        // Left stick: bytes 7-10 (16-bit values)
        // Bytes 7-8: X axis (Hi/Low combined into a signed 16-bit value, e.g. 0x7df6)
        // Bytes 9-10: Y axis (Hi/Low combined into a signed 16-bit value)
        if (size >= 11) {
            // Combine bytes 7-8 into a 16-bit value, e.g.: data[7]=0x7d, data[8]=0xf6 -> 0x7df6
            Uint16 raw_x_unsigned = ((Uint16)data[7] << 8) | data[8];
            Uint16 raw_y_unsigned = ((Uint16)data[9] << 8) | data[10];

            // Interpret the unsigned 16-bit value as a signed 16-bit value
            Sint16 raw_x = (Sint16)raw_x_unsigned;
            Sint16 raw_y = (Sint16)raw_y_unsigned;

            Sint16 left_x, left_y;
            // Use signed 16-bit values directly; invert Y-axis (SDL convention: up is negative)
            left_x = raw_x;
            left_y = -raw_y;

            Uint16 last_raw_x_unsigned = ((Uint16)ctx->last_state[7] << 8) | ctx->last_state[8];
            Uint16 last_raw_y_unsigned = ((Uint16)ctx->last_state[9] << 8) | ctx->last_state[10];
            Sint16 last_raw_x = (Sint16)last_raw_x_unsigned;
            Sint16 last_raw_y = (Sint16)last_raw_y_unsigned;
            bool raw_changed = (raw_x != last_raw_x || raw_y != last_raw_y);

            if (raw_changed) {
                Sint16 deadzone_x, deadzone_y;
                ApplyCircularDeadzone(left_x, left_y, &deadzone_x, &deadzone_y);

                Sint16 last_left_x, last_left_y;
                last_left_x = last_raw_x;
                last_left_y = -last_raw_y;  // invert Y axis

                Sint16 last_deadzone_x, last_deadzone_y;
                ApplyCircularDeadzone(last_left_x, last_left_y, &last_deadzone_x, &last_deadzone_y);

                if (deadzone_x != last_deadzone_x || deadzone_y != last_deadzone_y) {
                    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, deadzone_x);
                    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, deadzone_y);
                }
            }
        }

        // Right stick handling
        // Right stick: bytes 11-14 (16-bit values)
        // Bytes 11-12: X axis (Hi/Low combined into a signed 16-bit value)
        // Bytes 13-14: Y axis (Hi/Low combined into a signed 16-bit value)
        if (size >= 15) {
            // Combine bytes 11-12 into a 16-bit value
            Uint16 raw_x_unsigned = ((Uint16)data[11] << 8) | data[12];
            // Combine bytes 13-14 into a 16-bit value
            Uint16 raw_y_unsigned = ((Uint16)data[13] << 8) | data[14];

            // Interpret the unsigned 16-bit value as a signed 16-bit value
            Sint16 raw_x = (Sint16)raw_x_unsigned;
            Sint16 raw_y = (Sint16)raw_y_unsigned;

            Sint16 right_x, right_y;
            // Use signed 16-bit values directly; invert Y-axis (SDL convention: up is negative)
            right_x = raw_x;
            right_y = -raw_y;

            Uint16 last_raw_x_unsigned = ((Uint16)ctx->last_state[11] << 8) | ctx->last_state[12];
            Uint16 last_raw_y_unsigned = ((Uint16)ctx->last_state[13] << 8) | ctx->last_state[14];
            Sint16 last_raw_x = (Sint16)last_raw_x_unsigned;
            Sint16 last_raw_y = (Sint16)last_raw_y_unsigned;
            bool raw_changed = (raw_x != last_raw_x || raw_y != last_raw_y);

            if (raw_changed) {
                Sint16 deadzone_x, deadzone_y;
                ApplyCircularDeadzone(right_x, right_y, &deadzone_x, &deadzone_y);

                Sint16 last_right_x, last_right_y;
                last_right_x = last_raw_x;
                last_right_y = -last_raw_y;  // invert Y axis

                Sint16 last_deadzone_x, last_deadzone_y;
                ApplyCircularDeadzone(last_right_x, last_right_y, &last_deadzone_x, &last_deadzone_y);

                if (deadzone_x != last_deadzone_x || deadzone_y != last_deadzone_y) {
                    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, deadzone_x);
                    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, deadzone_y);
                }
            }
        }

        // Handle trigger axes
        // Protocol: L2 (byte 15) - analog left trigger 0-255, 0 = released, 255 = pressed
        //           R2 (byte 16) - analog right trigger 0-255, 0 = released, 255 = pressed
        // SDL range: 0-32767 (0 = released, 32767 = fully pressed)
        // Linear mapping: 0-255 -> 0-32767
        if (ctx->last_state[15] != data[15]) {
            axis = (Sint16)(((int)data[15] * 255) - 32767);
            SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
        }

        if (ctx->last_state[16] != data[16]) {
            axis = (Sint16)(((int)data[16] * 255) - 32767);
            SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
        }
    }

    if (ctx->sensors_enabled && !is_initial_packet && size >= 29)
    {
        Uint64 sensor_timestamp;
        float values[3];

        sensor_timestamp = ctx->sensor_timestamp_ns;
        ctx->sensor_timestamp_ns += ctx->sensor_timestamp_step_ns;

        // Accelerometer data (bytes 17-22)
        // Bytes 17-18: Acc X (Hi/Low combined into a signed 16-bit value)
        // Bytes 19-20: Acc Y (Hi/Low combined into a signed 16-bit value)
        // Bytes 21-22: Acc Z (Hi/Low combined into a signed 16-bit value)
        Uint16 acc_x_unsigned = ((Uint16)data[17] << 8) | data[18];
        Uint16 acc_y_unsigned = ((Uint16)data[19] << 8) | data[20];
        Uint16 acc_z_unsigned = ((Uint16)data[21] << 8) | data[22];

        // Convert the unsigned 16-bit values to signed 16-bit values
        Sint16 acc_x = (Sint16)acc_x_unsigned;
        Sint16 acc_y = (Sint16)acc_y_unsigned;
        Sint16 acc_z = (Sint16)acc_z_unsigned;

        // Apply scale factor and convert to floating point
        // Coordinate system matches PS4; use raw values directly without sign inversion
        values[0] = (float)acc_x * ctx->accelScale;  // Acc X
        values[1] = (float)acc_y * ctx->accelScale;   // Acc Y
        values[2] = (float)acc_z * ctx->accelScale;  // Acc Z
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, sensor_timestamp, values, 3);

        // Gyroscope data (bytes 23-28)
        // Bytes 23-24: Gyro X (Hi/Low combined into a signed 16-bit value)
        // Bytes 25-26: Gyro Y (Hi/Low combined into a signed 16-bit value)
        // Bytes 27-28: Gyro Z (Hi/Low combined into a signed 16-bit value)
        Uint16 gyro_x_unsigned = ((Uint16)data[23] << 8) | data[24];
        Uint16 gyro_y_unsigned = ((Uint16)data[25] << 8) | data[26];
        Uint16 gyro_z_unsigned = ((Uint16)data[27] << 8) | data[28];

        // Convert the unsigned 16-bit values to signed 16-bit values
        Sint16 gyro_x = (Sint16)gyro_x_unsigned;
        Sint16 gyro_y = (Sint16)gyro_y_unsigned;
        Sint16 gyro_z = (Sint16)gyro_z_unsigned;

        // Apply scale factor and convert to floating point (radians/second)
        // Based on the PS4 implementation: use (gyro_numerator / gyro_denominator) * (π / 180)
        // The default configuration corresponds to a range of approximately ±2048 degrees/second,
        // which is a common range for gamepad gyroscopes
        // Coordinate system matches the PS4; use raw values directly without sign inversion
        values[0] = (float)gyro_x * ctx->gyroScale;  // Gyro X (Pitch)
        values[1] = (float)gyro_y * ctx->gyroScale;  // Gyro Y (Yaw)
        values[2] = (float)gyro_z * ctx->gyroScale;  // Gyro Z (Roll)
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, sensor_timestamp, values, 3);
    }
    if (size >= 35) {
        Uint16 l_touchpad_x = ((Uint16)data[29] << 4) | ((data[30] >> 4) & 0x0F);
        Uint16 l_touchpad_y = ((Uint16)(data[30] & 0x0F) << 8) | data[31];

        Uint16 r_touchpad_x = ((Uint16)data[32] << 4) | ((data[33] >> 4) & 0x0F);
        Uint16 r_touchpad_y = ((Uint16)(data[33] & 0x0F) << 8) | data[34];

        (void)l_touchpad_x;
        (void)l_touchpad_y;
        (void)r_touchpad_x;
        (void)r_touchpad_y;
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}


static bool HIDAPI_DriverGameSir_UpdateDevice(SDL_HIDAPI_Device *device)
{
    if (!device) {
        return false;
    }

    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (!ctx) {
        return false;
    }

    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
        if (!joystick) {
            return false;
        }
    }

    SDL_hid_device *handle = NULL;
#if defined(_WIN32)

    if (device->is_bluetooth) {

        handle = device->dev;
    } else {

        if (ctx && ctx->output_handle) {
            handle = ctx->output_handle;
        } else {
            handle = device->dev;
        }
    }
#else

    handle = device->dev;
#endif
    if (handle == NULL) {
        return false;
    }

    while ((size = SDL_hid_read_timeout(handle, data, sizeof(data), 0)) > 0) {
        if (joystick) {
            // Uncomment to handle the packet
            HIDAPI_DriverGameSir_HandleStatePacket(joystick, ctx, data, size);
        }
    }

    if (size < 0) {


        if (device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_HID) {
            if (device->is_bluetooth) {
                SDL_Log("GameSir: Device disconnected - Connection type was: Bluetooth (wireless, HID mode)");
            } else {
                SDL_Log("GameSir: Device disconnected - Connection type was: USB/2.4G (HID mode)");
            }
        } else if (device->product_id == USB_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_8K_HID) {
            if (device->is_bluetooth) {
                SDL_Log("GameSir: Device disconnected - Connection type was: Bluetooth (wireless, 8K HID mode)");
            } else {
                SDL_Log("GameSir: Device disconnected - Connection type was: USB/2.4G (8K HID mode)");
            }
        } else if (device->is_bluetooth) {
            SDL_Log("GameSir: Device disconnected - Connection type was: Bluetooth (wireless)");
        } else {
            SDL_Log("GameSir: Device disconnected - Connection type was: USB (wired)");
        }
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}


static void HIDAPI_DriverGameSir_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverGameSir_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverGamesir_Context *ctx = (SDL_DriverGamesir_Context *)device->context;
    if (ctx) {
        if (ctx->output_handle) {
            SDL_hid_close(ctx->output_handle);
        }
        SDL_free(ctx);
        device->context = NULL;
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGameSir = {
    SDL_HINT_JOYSTICK_HIDAPI_GAMESIR,
    true,
    HIDAPI_DriverGameSir_RegisterHints,
    HIDAPI_DriverGameSir_UnregisterHints,
    HIDAPI_DriverGameSir_IsEnabled,
    HIDAPI_DriverGameSir_IsSupportedDevice,
    HIDAPI_DriverGameSir_InitDevice,
    HIDAPI_DriverGameSir_GetDevicePlayerIndex,
    HIDAPI_DriverGameSir_SetDevicePlayerIndex,
    HIDAPI_DriverGameSir_UpdateDevice,
    HIDAPI_DriverGameSir_OpenJoystick,
    HIDAPI_DriverGameSir_RumbleJoystick,
    HIDAPI_DriverGameSir_RumbleJoystickTriggers,
    HIDAPI_DriverGameSir_GetJoystickCapabilities,
    HIDAPI_DriverGameSir_SetJoystickLED,
    HIDAPI_DriverGameSir_SendJoystickEffect,
    HIDAPI_DriverGameSir_SetJoystickSensorsEnabled,
    HIDAPI_DriverGameSir_CloseJoystick,
    HIDAPI_DriverGameSir_FreeDevice,
};

#endif

#endif
