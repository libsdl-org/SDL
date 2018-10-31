/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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
/* This driver supports both simplified reports and the extended input reports enabled by Steam.
   Code and logic contributed by Valve Corporation under the SDL zlib license.
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


#ifdef SDL_JOYSTICK_HIDAPI_PS4

#define SONY_USB_VID        0x054C
#define SONY_DS4_PID        0x05C4
#define SONY_DS4_DONGLE_PID 0x0BA0
#define SONY_DS4_SLIM_PID   0x09CC

#define RAZER_USB_VID       0x1532
#define RAZER_PANTHERA_PID  0X0401
#define RAZER_PANTHERA_EVO_PID  0x1008

#define USB_PACKET_LENGTH   64

#define VOLUME_CHECK_INTERVAL_MS    (10 * 1000)

typedef enum
{
    k_EPS4ReportIdUsbState = 1,
    k_EPS4ReportIdUsbEffects = 5,
    k_EPS4ReportIdBluetoothState = 17,
    k_EPS4ReportIdBluetoothEffects = 17,
    k_EPS4ReportIdDisconnectMessage = 226,
} EPS4ReportId;

typedef enum 
{
    k_ePS4FeatureReportIdGyroCalibration_USB = 0x02,
    k_ePS4FeatureReportIdGyroCalibration_BT = 0x05,
    k_ePS4FeatureReportIdSerialNumber = 0x12,
} EPS4FeatureReportID;

typedef struct
{
    Uint8 ucLeftJoystickX;
    Uint8 ucLeftJoystickY;
    Uint8 ucRightJoystickX;
    Uint8 ucRightJoystickY;
    Uint8 rgucButtonsHatAndCounter[ 3 ];
    Uint8 ucTriggerLeft;
    Uint8 ucTriggerRight;
    Uint8 _rgucPad0[ 3 ];
    Sint16 sGyroX;
    Sint16 sGyroY;
    Sint16 sGyroZ;
    Sint16 sAccelX;
    Sint16 sAccelY;
    Sint16 sAccelZ;
    Uint8 _rgucPad1[ 5 ];
    Uint8 ucBatteryLevel;
    Uint8 _rgucPad2[ 4 ];
    Uint8 ucTrackpadCounter1;
    Uint8 rgucTrackpadData1[ 3 ];
    Uint8 ucTrackpadCounter2;
    Uint8 rgucTrackpadData2[ 3 ];
} PS4StatePacket_t;

typedef struct
{
    Uint8 ucRumbleRight;
    Uint8 ucRumbleLeft;
    Uint8 ucLedRed;
    Uint8 ucLedGreen;
    Uint8 ucLedBlue;
    Uint8 ucLedDelayOn;
    Uint8 ucLedDelayOff;
    Uint8 _rgucPad0[ 8 ];
    Uint8 ucVolumeLeft;
    Uint8 ucVolumeRight;
    Uint8 ucVolumeMic;
    Uint8 ucVolumeSpeaker;
} DS4EffectsState_t;

typedef struct {
    SDL_bool is_dongle;
    SDL_bool is_bluetooth;
    SDL_bool audio_supported;
    SDL_bool rumble_supported;
    Uint8 volume;
    Uint32 last_volume_check;
    Uint32 rumble_expiration;
    PS4StatePacket_t last_state;
} SDL_DriverPS4_Context;


/* Public domain CRC implementation adapted from:
   http://home.thep.lu.se/~bjorn/crc/crc32_simple.c
*/
static Uint32 crc32_for_byte(Uint32 r)
{
    int i;
    for(i = 0; i < 8; ++i) {
        r = (r & 1? 0: (Uint32)0xEDB88320L) ^ r >> 1;
    }
    return r ^ (Uint32)0xFF000000L;
}

static Uint32 crc32(Uint32 crc, const void *data, int count)
{
    int i;
    for(i = 0; i < count; ++i) {
        crc = crc32_for_byte((Uint8)crc ^ ((const Uint8*)data)[i]) ^ crc >> 8;
    }
    return crc;
}

#if defined(__WIN32__) && defined(HAVE_ENDPOINTVOLUME_H)
#include "../../core/windows/SDL_windows.h"

#ifndef NTDDI_VISTA
#define NTDDI_VISTA    0x06000000
#endif
#ifndef _WIN32_WINNT_VISTA
#define _WIN32_WINNT_VISTA 0x0600
#endif

/* Define Vista for the Audio related includes below to work */
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>

#undef DEFINE_GUID
#define DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
DEFINE_GUID(SDL_CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(SDL_IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(SDL_IID_IAudioEndpointVolume, 0x5CDF2C82, 0x841E, 0x4546, 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A);
#endif



static float GetSystemVolume(void)
{
    float volume = -1.0f;    /* Return this if we can't get system volume */

#if defined(__WIN32__) && defined(HAVE_ENDPOINTVOLUME_H)
    HRESULT hr = WIN_CoInitialize();
    if (SUCCEEDED(hr)) {
        IMMDeviceEnumerator *pEnumerator;

        /* This should gracefully fail on XP and succeed on everything Vista and above */
        hr = CoCreateInstance(&SDL_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &SDL_IID_IMMDeviceEnumerator, (LPVOID*)&pEnumerator);
        if (SUCCEEDED(hr)) {
            IMMDevice *pDevice;

            hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr)) {
                IAudioEndpointVolume *pEndpointVolume;

                hr = IMMDevice_Activate(pDevice, &SDL_IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (LPVOID*)&pEndpointVolume);
                if (SUCCEEDED(hr)) {
                    IAudioEndpointVolume_GetMasterVolumeLevelScalar(pEndpointVolume, &volume);
                    IUnknown_Release(pEndpointVolume);
                }
                IUnknown_Release(pDevice);
            }
            IUnknown_Release(pEnumerator);
        }
        WIN_CoUninitialize();
    }
#endif /* __WIN32__ */

    return volume;
}

static uint8_t GetPlaystationVolumeFromFloat(float fVolume)
{
    const int k_nVolumeFitRatio = 15;
    const int k_nVolumeFitOffset = 9;
    float fVolLog;

    if (fVolume > 1.0f || fVolume < 0.0f) {
        fVolume = 0.30f;
    }
    fVolLog = SDL_logf(fVolume * 100);

    return (Uint8)((fVolLog * k_nVolumeFitRatio) + k_nVolumeFitOffset);
}

static SDL_bool
HIDAPI_DriverPS4_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number)
{
    return SDL_IsJoystickPS4(vendor_id, product_id);
}

static const char *
HIDAPI_DriverPS4_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == SONY_USB_VID) {
        return "PS4 Controller";
    }
    return NULL;
}

static SDL_bool ReadFeatureReport(hid_device *dev, Uint8 report_id, Uint8 *data, size_t size)
{
    Uint8 report[USB_PACKET_LENGTH + 1];

    SDL_memset(report, 0, sizeof(report));
    report[0] = report_id;
    if (hid_get_feature_report(dev, report, sizeof(report)) < 0) {
        return SDL_FALSE;
    }
    SDL_memcpy(data, report, SDL_min(size, sizeof(report)));
    return SDL_TRUE;
}

static SDL_bool CheckUSBConnected(hid_device *dev)
{
    int i;
    Uint8 data[16];

    /* This will fail if we're on Bluetooth */
    if (ReadFeatureReport(dev, k_ePS4FeatureReportIdSerialNumber, data, sizeof(data))) {
        for (i = 0; i < sizeof(data); ++i) {
            if (data[i] != 0x00) {
                return SDL_TRUE;
            }
        }
        /* Maybe the dongle without a connected controller? */
    }
    return SDL_FALSE;
}

static SDL_bool HIDAPI_DriverPS4_CanRumble(Uint16 vendor_id, Uint16 product_id)
{
    /* The Razer Panthera fight stick hangs when trying to rumble */
    if (vendor_id == RAZER_USB_VID &&
        (product_id == RAZER_PANTHERA_PID || product_id == RAZER_PANTHERA_EVO_PID)) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static int HIDAPI_DriverPS4_Rumble(SDL_Joystick *joystick, hid_device *dev, void *context, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms);

static SDL_bool
HIDAPI_DriverPS4_Init(SDL_Joystick *joystick, hid_device *dev, Uint16 vendor_id, Uint16 product_id, void **context)
{
    SDL_DriverPS4_Context *ctx;

    ctx = (SDL_DriverPS4_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    *context = ctx;

    /* Check for type of connection */
    ctx->is_dongle = (vendor_id == SONY_USB_VID && product_id == SONY_DS4_DONGLE_PID);
    if (ctx->is_dongle) {
        ctx->is_bluetooth = SDL_FALSE;
    } else if (vendor_id == SONY_USB_VID) {
        ctx->is_bluetooth = !CheckUSBConnected(dev);
    } else {
        /* Third party controllers appear to all be wired */
        ctx->is_bluetooth = SDL_FALSE;
    }
#ifdef DEBUG_PS4
    SDL_Log("PS4 dongle = %s, bluetooth = %s\n", ctx->is_dongle ? "TRUE" : "FALSE", ctx->is_bluetooth ? "TRUE" : "FALSE");
#endif

    /* Check to see if audio is supported */
    if (vendor_id == SONY_USB_VID &&
        (product_id == SONY_DS4_SLIM_PID || product_id == SONY_DS4_DONGLE_PID )) {
        ctx->audio_supported = SDL_TRUE;
    }

    if (HIDAPI_DriverPS4_CanRumble(vendor_id, product_id)) {
        if (ctx->is_bluetooth) {
            ctx->rumble_supported = SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, SDL_FALSE);
        } else {
            ctx->rumble_supported = SDL_TRUE;
        }
    }

    /* Initialize LED and effect state */
    HIDAPI_DriverPS4_Rumble(joystick, dev, ctx, 0, 0, 0);

    /* Initialize the joystick capabilities */
    joystick->nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverPS4_Rumble(SDL_Joystick *joystick, hid_device *dev, void *context, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverPS4_Context *ctx = (SDL_DriverPS4_Context *)context;
    DS4EffectsState_t *effects;
    Uint8 data[78];
    int report_size, offset;

    if (!ctx->rumble_supported) {
        return SDL_Unsupported();
    }

    /* In order to send rumble, we have to send a complete effect packet */
    SDL_memset(data, 0, sizeof(data));

    if (ctx->is_bluetooth) {
        data[0] = k_EPS4ReportIdBluetoothEffects;
        data[1] = 0xC0 | 0x04;  /* Magic value HID + CRC, also sets interval to 4ms for samples */
        data[3] = 0x03;  /* 0x1 is rumble, 0x2 is lightbar, 0x4 is the blink interval */

        report_size = 78;
        offset = 6;
    } else {
        data[0] = k_EPS4ReportIdUsbEffects;
        data[1] = 0x07;  /* Magic value */

        report_size = 32;
        offset = 4;
    }
    effects = (DS4EffectsState_t *)&data[offset];

    effects->ucRumbleLeft = (low_frequency_rumble >> 8);
    effects->ucRumbleRight = (high_frequency_rumble >> 8);

    effects->ucLedRed = 0;
    effects->ucLedGreen = 0;
    effects->ucLedBlue = 80;

    if (ctx->audio_supported) {
        Uint32 now = SDL_GetTicks();
        if (!ctx->last_volume_check ||
            SDL_TICKS_PASSED(now, ctx->last_volume_check + VOLUME_CHECK_INTERVAL_MS)) {
            ctx->volume = GetPlaystationVolumeFromFloat(GetSystemVolume());
            ctx->last_volume_check = now;
        }

        effects->ucVolumeRight = ctx->volume;
        effects->ucVolumeLeft = ctx->volume;
        effects->ucVolumeSpeaker = ctx->volume;
        effects->ucVolumeMic = 0xFF;
    }

    if (ctx->is_bluetooth) {
        /* Bluetooth reports need a CRC at the end of the packet (at least on Linux) */
        Uint8 ubHdr = 0xA2; /* hidp header is part of the CRC calculation */
        Uint32 unCRC;
        unCRC = crc32(0, &ubHdr, 1);
        unCRC = crc32(unCRC, data, (Uint32)(report_size - sizeof(unCRC)));
        SDL_memcpy(&data[report_size - sizeof(unCRC)], &unCRC, sizeof(unCRC));
    }

    if (hid_write(dev, data, report_size) != report_size) {
        return SDL_SetError("Couldn't send rumble packet");
    }

    if ((low_frequency_rumble || high_frequency_rumble) && duration_ms) {
        ctx->rumble_expiration = SDL_GetTicks() + duration_ms;
    } else {
        ctx->rumble_expiration = 0;
    }
    return 0;
}

static void
HIDAPI_DriverPS4_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverPS4_Context *ctx, PS4StatePacket_t *packet)
{
    Sint16 axis;

    if (ctx->last_state.rgucButtonsHatAndCounter[0] != packet->rgucButtonsHatAndCounter[0]) {
        {
            Uint8 data = (packet->rgucButtonsHatAndCounter[0] >> 4);

            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data & 0x02) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data & 0x04) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        }
        {
            Uint8 data = (packet->rgucButtonsHatAndCounter[0] & 0x0F);
            SDL_bool dpad_up = SDL_FALSE;
            SDL_bool dpad_down = SDL_FALSE;
            SDL_bool dpad_left = SDL_FALSE;
            SDL_bool dpad_right = SDL_FALSE;

            switch (data) {
            case 0:
                dpad_up = SDL_TRUE;
                break;
            case 1:
                dpad_up = SDL_TRUE;
                dpad_right = SDL_TRUE;
                break;
            case 2:
                dpad_right = SDL_TRUE;
                break;
            case 3:
                dpad_right = SDL_TRUE;
                dpad_down = SDL_TRUE;
                break;
            case 4:
                dpad_down = SDL_TRUE;
                break;
            case 5:
                dpad_left = SDL_TRUE;
                dpad_down = SDL_TRUE;
                break;
            case 6:
                dpad_left = SDL_TRUE;
                break;
            case 7:
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
    }

    if (ctx->last_state.rgucButtonsHatAndCounter[1] != packet->rgucButtonsHatAndCounter[1]) {
        Uint8 data = packet->rgucButtonsHatAndCounter[1];

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state.rgucButtonsHatAndCounter[2] != packet->rgucButtonsHatAndCounter[2]) {
        Uint8 data = (packet->rgucButtonsHatAndCounter[2] & 0x03);

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data & 0x01) ? SDL_PRESSED : SDL_RELEASED);
    }

    axis = ((int)packet->ucTriggerLeft * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)packet->ucTriggerRight * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = ((int)packet->ucLeftJoystickX * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = ((int)packet->ucLeftJoystickY * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = ((int)packet->ucRightJoystickX * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = ((int)packet->ucRightJoystickY * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    if (packet->ucBatteryLevel & 0x10) {
        joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;
    } else {
        /* Battery level ranges from 0 to 10 */
        int level = (packet->ucBatteryLevel & 0xF);
        if (level == 0) {
            joystick->epowerlevel = SDL_JOYSTICK_POWER_EMPTY;
        } else if (level <= 2) {
            joystick->epowerlevel = SDL_JOYSTICK_POWER_LOW;
        } else if (level <= 7) {
            joystick->epowerlevel = SDL_JOYSTICK_POWER_MEDIUM;
        } else {
            joystick->epowerlevel = SDL_JOYSTICK_POWER_FULL;
        }
    }

    SDL_memcpy(&ctx->last_state, packet, sizeof(ctx->last_state));
}

static SDL_bool
HIDAPI_DriverPS4_Update(SDL_Joystick *joystick, hid_device *dev, void *context)
{
    SDL_DriverPS4_Context *ctx = (SDL_DriverPS4_Context *)context;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    while ((size = hid_read_timeout(dev, data, sizeof(data), 0)) > 0) {
        switch (data[0]) {
        case k_EPS4ReportIdUsbState:
            HIDAPI_DriverPS4_HandleStatePacket(joystick, dev, ctx, (PS4StatePacket_t *)&data[1]);
            break;
        case k_EPS4ReportIdBluetoothState:
            /* Bluetooth state packets have two additional bytes at the beginning */
            HIDAPI_DriverPS4_HandleStatePacket(joystick, dev, ctx, (PS4StatePacket_t *)&data[3]);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown PS4 packet: 0x%.2x\n", data[0]);
#endif
            break;
        }
    }

    if (ctx->rumble_expiration) {
        Uint32 now = SDL_GetTicks();
        if (SDL_TICKS_PASSED(now, ctx->rumble_expiration)) {
            HIDAPI_DriverPS4_Rumble(joystick, dev, context, 0, 0, 0);
        }
    }

    return (size >= 0);
}

static void
HIDAPI_DriverPS4_Quit(SDL_Joystick *joystick, hid_device *dev, void *context)
{
    SDL_free(context);
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS4 =
{
    SDL_HINT_JOYSTICK_HIDAPI_PS4,
    SDL_TRUE,
    HIDAPI_DriverPS4_IsSupportedDevice,
    HIDAPI_DriverPS4_GetDeviceName,
    HIDAPI_DriverPS4_Init,
    HIDAPI_DriverPS4_Rumble,
    HIDAPI_DriverPS4_Update,
    HIDAPI_DriverPS4_Quit
};

#endif /* SDL_JOYSTICK_HIDAPI_PS4 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
