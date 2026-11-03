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

#ifdef SDL_JOYSTICK_GAMEINPUT

#include "../SDL_sysjoystick.h"
#include "../usb_ids.h"
#include "../../core/windows/SDL_windows.h"
#include "../../core/windows/SDL_gameinput.h"

extern "C"
{
#include "../hidapi/SDL_hidapijoystick_c.h"
}

enum
{
    SDL_GAMEPAD_BUTTON_GAMEINPUT_SHARE = 11
};

enum
{
    SDL_GAMEINPUT_RAWTYPE_NONE,
    SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR,
    SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT,
    SDL_GAMEINPUT_RAWTYPE_GUITAR_HERO_LIVE_GUITAR,
    SDL_GAMEINPUT_RAWTYPE_LEGACY_ADAPTER,
};

struct SDL_JoystickDeviceData
{
    APP_LOCAL_DEVICE_ID deviceId; // The unique ID for the device
    IGameInputDevice *device;
#if GAMEINPUT_API_VERSION < 4
    const GameInputDeviceInfo *info;
#endif
    Uint16 vendor;
    Uint16 product;
    Uint16 bus;
    Uint8 driver_signature;
    Uint8 subtype;
    int raw_type;
};

typedef struct joystick_hwdata
{
    IGameInputDevice *device;
#if GAMEINPUT_API_VERSION < 4
    const GameInputDeviceInfo *info;
#endif
    int raw_type;
    bool report_sensors;
#if GAMEINPUT_API_VERSION >= 4
    GameInputSensorsKind supported_sensors;
    GameInputGamepadButtons supported_paddles;
    Uint8 first_paddle_button;
    bool led_changed;
#endif
    GameInputRumbleParams rumbleParams;
#if GAMEINPUT_API_VERSION < 4 // g_pGameInput->RegisterSystemButtonCallback no longer exists in GameInput v4, so no need for a callback token
    GameInputCallbackToken system_button_callback_token;
#endif
} GAMEINPUT_InternalJoystickHwdata;

static IGameInput *g_pGameInput;
static bool g_GameInputInitialized;
static SDL_SpinLock g_GameInputDevicesLock;
static IGameInputDevice **g_GameInputDevices;
static int g_GameInputDeviceCount;
static GameInputCallbackToken g_GameInputCallbackToken;
static Uint64 g_GameInputTimestampOffset;

#if GAMEINPUT_API_VERSION < 4
static const GameInputDeviceInfo *GAMEINPUT_GetGameInputDeviceInfo(IGameInputDevice *pDevice)
{
    return pDevice->GetDeviceInfo();
}
#endif

static bool GAMEINPUT_InternalIsGamepad(IGameInputDevice *pDevice)
{
#if GAMEINPUT_API_VERSION >= 4
    GameInputKind supportedInput = pDevice->GetSupportedInput();
#else
    GameInputKind supportedInput = GAMEINPUT_GetGameInputDeviceInfo(pDevice)->supportedInput;
#endif
    return (supportedInput & GameInputKindGamepad) != 0;
}

#if GAMEINPUT_API_VERSION >= 4
// Paddles in SDL button order
static const GameInputGamepadButtons GAMEINPUT_Paddles[] = {
    GameInputGamepadPaddleRight1,
    GameInputGamepadPaddleLeft1,
    GameInputGamepadPaddleRight2,
    GameInputGamepadPaddleLeft2,
};

static bool GAMEINPUT_HasShareButton(IGameInputDevice *pDevice, Uint16 vendor, Uint16 product)
{
    // GameInput reports that the Xbox Elite Series 2 has the share button, so check SDL's list first
    if (SDL_IsJoystickXboxSeriesX(vendor, product) &&
        (pDevice->GetSupportedSystemButtons() & GameInputSystemButtonShare)) {
        return true;
    }
    return false;
}

static GameInputGamepadButtons GAMEINPUT_GetSupportedPaddles(IGameInputDevice *pDevice)
{
    GameInputGamepadInfo info;
    if (pDevice->GetGamepadInfo(&info)) {
        return (GameInputGamepadButtons)(info.supportedLayout & GameInputGamepadModulePaddles4);
    }
    return GameInputGamepadNone;
}

// Paddles follow the standard buttons and the optional share button
static int GAMEINPUT_GetFirstPaddleButton(IGameInputDevice *pDevice, Uint16 vendor, Uint16 product)
{
    int paddle_button_start = SDL_GAMEPAD_BUTTON_GAMEINPUT_SHARE;
    if (GAMEINPUT_HasShareButton(pDevice, vendor, product)) {
        ++paddle_button_start;
    }
    return paddle_button_start;
}
#endif // GAMEINPUT_API_VERSION >= 4

static Uint8 GAMEINPUT_GetDeviceRawType(IGameInputDevice *pDevice)
{
#if GAMEINPUT_API_VERSION >= 4
    GameInputKind supportedInput = pDevice->GetSupportedInput();
    uint16_t vendorId = pDevice->GetVendorId();
    uint16_t productId = pDevice->GetProductId();

    if (supportedInput & GameInputKindRawDeviceReport) {
        switch (vendorId) {
            case USB_VENDOR_MADCATZ:
                switch (productId) {
                    case USB_PRODUCT_MADCATZ_XB1_STRATOCASTER_GUITAR:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR;
                    case USB_PRODUCT_MADCATZ_XB1_DRUM_KIT:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT;
                    case USB_PRODUCT_MADCATZ_XB1_LEGACY_ADAPTER:
                        return SDL_GAMEINPUT_RAWTYPE_LEGACY_ADAPTER;
                    default:
                        break;
                }
                break;
            case USB_VENDOR_PDP:
                switch (productId) {
                    case USB_PRODUCT_PDP_XB1_JAGUAR_GUITAR:
                    case USB_PRODUCT_PDP_XB1_RIFFMASTER_GUITAR:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR;
                    case USB_PRODUCT_PDP_XB1_DRUM_KIT:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT;
                    default:
                        break;
                }
                break;
            case USB_VENDOR_CRKD:
                switch (productId) {
                    case USB_PRODUCT_RED_OCTANE_XB1_STAGE_TOUR_GUITAR:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR;
                    default:
                        break;
                }
                break;
            case USB_VENDOR_RED_OCTANE:
                switch (productId) {
                    case USB_PRODUCT_RED_OCTANE_XB1_GUITAR_HERO_LIVE_GUITAR:
                        return SDL_GAMEINPUT_RAWTYPE_GUITAR_HERO_LIVE_GUITAR;
                    default:
                        break;
                }
                break;
            case USB_VENDOR_RED_OCTANE_GAMES:
                switch (productId) {
                    case USB_PRODUCT_RED_OCTANE_XB1_STAGE_TOUR_GUITAR:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR;
                    case USB_PRODUCT_RED_OCTANE_XB1_STAGE_TOUR_DRUMS:
                        return SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT;
                    default:
                        break;
                }
                break;
        }
    }
#endif // GAMEINPUT_API_VERSION >= 4
    return SDL_GAMEINPUT_RAWTYPE_NONE;
}
static Uint8 GAMEINPUT_GetDeviceSubtype(IGameInputDevice *pDevice)
{
#if GAMEINPUT_API_VERSION >= 4
    GameInputKind supportedInput = pDevice->GetSupportedInput();
#else
    GameInputKind supportedInput = GAMEINPUT_GetGameInputDeviceInfo(pDevice)->supportedInput;
#endif
    Uint8 rawType = GAMEINPUT_GetDeviceRawType(pDevice);
    if (rawType == SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR || rawType == SDL_GAMEINPUT_RAWTYPE_GUITAR_HERO_LIVE_GUITAR) {
        return SDL_JOYSTICK_TYPE_GUITAR;
    }
    if (rawType == SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT) {
        return SDL_JOYSTICK_TYPE_DRUM_KIT;
    }
    if (supportedInput & GameInputKindRacingWheel) {
        return SDL_JOYSTICK_TYPE_WHEEL;
    }
    if (supportedInput & GameInputKindArcadeStick) {
        return SDL_JOYSTICK_TYPE_ARCADE_STICK;
    }
    if (supportedInput & GameInputKindFlightStick) {
        return SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    }
    if (supportedInput & (GameInputKindGamepad | GameInputKindController)) {
        return SDL_JOYSTICK_TYPE_GAMEPAD;
    }
    // Other device subtypes don't have their own GameInputKind enum entries.
    return 0;
}

#if GAMEINPUT_API_VERSION >= 4
static int GetSteamVirtualGamepadSlot(const char *device_path)
{
    int slot = -1;

    // The format for the raw input device path is documented here:
    // https://partner.steamgames.com/doc/features/steam_controller/steam_input_gamepad_emulation_bestpractices
    (void)SDL_sscanf(device_path, "\\\\.\\pipe\\HID#VID_045E&PID_028E&IG_00#%*X&%*X&%*X#%d#%*u", &slot);
    return slot;
}
#endif // GAMEINPUT_API_VERSION >= 4

static bool IsXbox360WirelessAdapter(Uint16 vendor, Uint16 product)
{
    if (vendor == USB_VENDOR_MICROSOFT) {
        if (product == USB_PRODUCT_XBOX360_WIRELESS_RECEIVER ||
            product == USB_PRODUCT_XBOX360_WIRELESS_RECEIVER_THIRDPARTY1 ||
            product == USB_PRODUCT_XBOX360_WIRELESS_RECEIVER_THIRDPARTY2) {
            return true;
        }
    }
    return false;
}

static void QueryDeviceName(IGameInputDevice *pDevice, char **product_string)
{
    if (!pDevice || !product_string) {
        return;
    }

#if GAMEINPUT_API_VERSION >= 4
    char buffer[256];
    if (pDevice->GetDisplayName(sizeof(buffer), buffer) > 0) {
        *product_string = SDL_strdup(buffer);
    } else {
        *product_string = NULL;
    }
#else
    const GameInputDeviceInfo *info = GAMEINPUT_GetGameInputDeviceInfo(pDevice);
    if (info->displayName) {
        *product_string = SDL_strdup(info->displayName->data);
    }
    if (!info->displayName) {
        *product_string = NULL;
    }
#endif
}

static void FreeGameInputDevice(SDL_JoystickDevice *device)
{
    device->internal->device->Release();
}

static SDL_JoystickDevice *CreateGameInputDevice(IGameInputDevice *pDevice)
{
#if GAMEINPUT_API_VERSION < 4
    const GameInputDeviceInfo *info = pDevice->GetDeviceInfo();
#endif
    Uint16 bus = SDL_HARDWARE_BUS_USB;
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint16 version = 0;
    Uint8 driver_signature = 'g';
    Uint8 subtype = 0;
    int raw_type = SDL_GAMEINPUT_RAWTYPE_NONE;

#if GAMEINPUT_API_VERSION >= 4
    vendor = pDevice->GetVendorId();
    product = pDevice->GetProductId();
#else
    vendor = info->vendorId;
    product = info->productId;
#endif

    if (false /*info->capabilities & GameInputDeviceCapabilityWireless*/) {
        bus = SDL_HARDWARE_BUS_BLUETOOTH;
    } else {
        bus = SDL_HARDWARE_BUS_USB;
    }
    //version = (info->firmwareVersion.major << 8) | info->firmwareVersion.minor;
    subtype = GAMEINPUT_GetDeviceSubtype(pDevice);
    raw_type = GAMEINPUT_GetDeviceRawType(pDevice);

    if (IsXbox360WirelessAdapter(vendor, product)) {
        return NULL;
    }

#if defined(SDL_JOYSTICK_DINPUT) && defined(SDL_HAPTIC_DINPUT)
    // This joystick backend currently doesn't provide a haptic backend, so fallback to DirectInput for haptic-capable devices.
    if (SDL_GetHintBoolean(SDL_HINT_JOYSTICK_DIRECTINPUT, true) &&
#if GAMEINPUT_API_VERSION >= 4
        pDevice->GetForceFeedbackMotorCount() > 0 &&
#else
        info->forceFeedbackMotorCount > 0 &&
#endif
        pDevice->IsForceFeedbackMotorPoweredOn(0)) {
        return NULL;
    }
#endif

    if (GAMEINPUT_InternalIsGamepad(pDevice)) {
        if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_GAMEINPUT, SDL_GAMEINPUT_DEFAULT)) {
            return NULL;
        }
    } else if (raw_type == SDL_GAMEINPUT_RAWTYPE_NONE) {
#if defined(SDL_JOYSTICK_DINPUT)
        // Let other backends handle non-gamepad controllers to possibly avoid bugs and/or regressions.
        if (SDL_GetHintBoolean(SDL_HINT_JOYSTICK_DIRECTINPUT, true)) {
            return NULL;
        }
#endif
#if GAMEINPUT_API_VERSION >= 4
        if (pDevice->GetSupportedInput() & GameInputKindController)
#else
        if (info->supportedInput & GameInputKindController)
#endif
        {
            // Maintain GUID compatibility with DirectInput controller mappings.
            driver_signature = 0;
            subtype = 0;
        } else {
            // This joystick backend currently doesn't provide proper reading of other joystick types.
            return NULL;
        }
    }

    char *product_string = NULL;
    QueryDeviceName(pDevice, &product_string);

    SDL_JoystickDevice *device = NULL;
    if (!SDL_ShouldIgnoreJoystick(vendor, product, version, product_string)) {
        device = SDL_CreateJoystickDevice(&SDL_GAMEINPUT_JoystickDriver, sizeof(SDL_JoystickDeviceData));
    }
    if (device) {
        SDL_JoystickDeviceData *data = (SDL_JoystickDeviceData *)(device + 1);
#if GAMEINPUT_API_VERSION >= 4
        pDevice->GetDeviceId(&data->deviceId);
#else
        data->deviceId = info->deviceId;
        data->info = info;
#endif
        pDevice->AddRef();
        data->device = pDevice;
        data->vendor = vendor;
        data->product = product;
        data->bus = bus;
        data->driver_signature = driver_signature;
        data->subtype = subtype;
        data->raw_type = raw_type;
        device->internal = data;
        device->free = FreeGameInputDevice;
        device->unique_id = &data->deviceId;
        device->unique_id_len = sizeof(data->deviceId);

        // The path is the APP_LOCAL_DEVICE_ID as a hex string, since it's required for some association callbacks
        char path[(APP_LOCAL_DEVICE_ID_SIZE * 2) + 1];
        for (int i = 0; i < APP_LOCAL_DEVICE_ID_SIZE; ++i) {
            (void)SDL_snprintf(&path[i * 2], 3, "%02X", data->deviceId.value[i]);
        }
        device->path = SDL_strdup(path);

        device->name = product_string;
        product_string = NULL;

#if GAMEINPUT_API_VERSION >= 4
        char pnpPath[256];
        pDevice->GetPnpPath(sizeof(pnpPath), pnpPath);
        device->steam_virtual_gamepad_slot = GetSteamVirtualGamepadSlot(pnpPath);

        uint32_t xinput_index;
        if (pDevice->GetXInputDeviceIndex(&xinput_index)) {
            int slot = (int)xinput_index;
            device->player_index = slot;
            SDL_AddJoystickDeviceMatchXInput(device, slot);
        }
#else
        // GameInput v0 only supports Xbox controllers, which are also available through XInput
        SDL_AddJoystickDeviceMatchXInput(device, -1);
#endif
        SDL_AddJoystickDeviceMatchVIDPID(device, vendor, product);

        if (!device->path) {
            SDL_DestroyJoystickDevice(device);
            device = NULL;
        }
    }

    SDL_free(product_string);

    return device;
}

static bool GAMEINPUT_JoystickPrepareDevice(SDL_JoystickDevice *device)
{
    const SDL_JoystickDeviceData *data = device->internal;
    Uint16 version = 0;
    char *manufacturer_string = NULL;
    char *product_string = NULL;

#ifdef SDL_JOYSTICK_HIDAPI
    // Use the HID device strings if they're available
    if (HIDAPI_GetDeviceStrings(data->vendor, data->product, &manufacturer_string, &product_string)) {
        SDL_free(device->name);
        device->name = NULL;
    }
#endif
    if (!product_string) {
        product_string = device->name;
        device->name = NULL;
    }

    device->name = SDL_CreateJoystickName(data->vendor, data->product, manufacturer_string, product_string);
    device->guid = SDL_CreateJoystickGUID(data->bus, data->vendor, data->product, version, manufacturer_string, product_string, data->driver_signature, data->subtype);
    SDL_free(manufacturer_string);
    SDL_free(product_string);

    if (!device->name) {
        return false;
    }
    return true;
}

static void CALLBACK GAMEINPUT_InternalJoystickDeviceCallback(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void *context,
#if GAMEINPUT_API_VERSION >= 4
    _In_ GameInputDeviceEvent deviceEvent,
    _In_ IGameInputDevice* device)
#else
    _In_ IGameInputDevice *device,
    _In_ uint64_t timestamp,
    _In_ GameInputDeviceStatus currentStatus,
    _In_ GameInputDeviceStatus previousStatus)
#endif
{
    bool connected;
    int idx;

    if (!device) {
        // This should never happen, but ignore it if it does
        return;
    }

#if GAMEINPUT_API_VERSION >= 4
    connected = device->IsConnected();
#else
    connected = ((currentStatus & GameInputDeviceConnected) != 0);
#endif

    SDL_LockSpinlock(&g_GameInputDevicesLock);

    if (!g_GameInputInitialized) {
        SDL_UnlockSpinlock(&g_GameInputDevicesLock);
        return;
    }

    bool found = false;
    for (idx = 0; idx < g_GameInputDeviceCount; ++idx) {
        if (g_GameInputDevices[idx] == device) {
            found = true;
            break;
        }
    }
    if (connected) {
        if (!found) {
            IGameInputDevice **devices = (IGameInputDevice **)SDL_realloc(g_GameInputDevices, (g_GameInputDeviceCount + 1) * sizeof(*devices));
            if (devices) {
                device->AddRef();
                devices[g_GameInputDeviceCount++] = device;
                g_GameInputDevices = devices;
            }
        }
    } else {
        if (found) {
            g_GameInputDevices[idx]->Release();
            --g_GameInputDeviceCount;
            if (idx < g_GameInputDeviceCount) {
                SDL_memmove(&g_GameInputDevices[idx], &g_GameInputDevices[idx + 1], (g_GameInputDeviceCount - idx) * sizeof(*g_GameInputDevices));
            }
        }
    }

    SDL_UnlockSpinlock(&g_GameInputDevicesLock);

    if (connected) {
        // Give higher priority drivers a chance to see new devices
        SDL_RequestJoystickDeviceUpdate(false);
    } else {
        SDL_RequestJoystickDeviceUpdate(true);
    }
}

static void GAMEINPUT_JoystickQuit(void);

static bool GAMEINPUT_IsRawGameInputEnabled(void)
{
#if GAMEINPUT_API_VERSION >= 4
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_GAMEINPUT_RAW, true);
#else
    return false;
#endif
}

static bool GAMEINPUT_JoystickInit(void)
{
    HRESULT hr;

    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_GAMEINPUT, SDL_GAMEINPUT_DEFAULT) && !GAMEINPUT_IsRawGameInputEnabled()) {
        return true;
    }

    if (!SDL_InitGameInput(&g_pGameInput)) {
        return false;
    }

    g_GameInputInitialized = true;

    // Allow background controller input
    // SDL manages focus policy at a higher level, so we can set this unconditionally.
#if GAMEINPUT_API_VERSION >= 4
    g_pGameInput->EnableBackgroundInput(true);
#endif

    GameInputKind kind = GameInputKindUnknown;
    if (SDL_GetHintBoolean(SDL_HINT_JOYSTICK_GAMEINPUT, SDL_GAMEINPUT_DEFAULT)) {
        kind |= GameInputKindController;
    }
#if GAMEINPUT_API_VERSION >= 4
    if (GAMEINPUT_IsRawGameInputEnabled()) {
        kind |= GameInputKindRawDeviceReport;
    }
#endif

    hr = g_pGameInput->RegisterDeviceCallback(NULL,
                                           kind,
#if GAMEINPUT_API_VERSION < 4
                                           GameInputDeviceConnected,
                                           GameInputBlockingEnumeration,
#endif
                                           NULL,
                                           GAMEINPUT_InternalJoystickDeviceCallback,
                                           &g_GameInputCallbackToken);
    if (FAILED(hr)) {
        GAMEINPUT_JoystickQuit();
        return WIN_SetErrorFromHRESULT("IGameInput::RegisterDeviceCallback", hr);
    }

    // Calculate the relative offset between SDL timestamps and GameInput timestamps
    Uint64 now = SDL_GetTicksNS();
    uint64_t timestampUS = g_pGameInput->GetCurrentTimestamp();
    g_GameInputTimestampOffset = (SDL_NS_TO_US(now) - timestampUS);

    return true;
}

static SDL_JoystickDevice *GAMEINPUT_JoystickQueryDevices(void)
{
    SDL_JoystickDevice *devices = NULL;
    SDL_JoystickDevice *tail = NULL;
    IGameInputDevice **connected_devices = NULL;
    int count = 0;

    if (!g_pGameInput) {
        return NULL;
    }

    // Take a reference to the connected devices so we don't hold the lock while creating device entries
    SDL_LockSpinlock(&g_GameInputDevicesLock);
    if (g_GameInputDeviceCount > 0) {
        connected_devices = SDL_stack_alloc(IGameInputDevice *, g_GameInputDeviceCount);
        if (connected_devices) {
            count = g_GameInputDeviceCount;
            for (int i = 0; i < count; ++i) {
                connected_devices[i] = g_GameInputDevices[i];
                connected_devices[i]->AddRef();
            }
        }
    }
    SDL_UnlockSpinlock(&g_GameInputDevicesLock);

    for (int i = 0; i < count; ++i) {
        IGameInputDevice *pDevice = connected_devices[i];
#if GAMEINPUT_API_VERSION >= 4
        bool connected = pDevice->IsConnected();
#else
        bool connected = ((pDevice->GetDeviceStatus() & GameInputDeviceConnected) != 0);
#endif
        if (connected) {
            SDL_JoystickDevice *device = CreateGameInputDevice(pDevice);
            if (device) {
                if (tail) {
                    tail->next = device;
                } else {
                    devices = device;
                }
                tail = device;
            }
        }
        pDevice->Release();
    }
    SDL_stack_free(connected_devices);

    return devices;
}

static void GAMEINPUT_UpdatePowerInfo(SDL_Joystick *joystick, IGameInputDevice *device)
{
#if 0
    GameInputBatteryState battery_state;
    SDL_PowerState state;
    int percent = 0;

    SDL_zero(battery_state);
    IGameInputDevice_GetBatteryState(device, &battery_state);

    switch (battery_state.status) {
    case GameInputBatteryNotPresent:
        state = SDL_POWERSTATE_NO_BATTERY;
        break;
    case GameInputBatteryDischarging:
        state = SDL_POWERSTATE_ON_BATTERY;
        break;
    case GameInputBatteryIdle:
        state = SDL_POWERSTATE_CHARGED;
        break;
    case GameInputBatteryCharging:
        state = SDL_POWERSTATE_CHARGING;
        break;
    default:
        state = SDL_POWERSTATE_UNKNOWN;
        break;
    }
    if (battery_state.fullChargeCapacity > 0.0f) {
        percent = (int)SDL_roundf((battery_state.remainingCapacity / battery_state.fullChargeCapacity) * 100.0f);
    }
    SDL_SendJoystickPowerInfo(joystick, state, percent);
#endif
}

static bool GAMEINPUT_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    const SDL_JoystickDeviceData *data = device->internal;
#if GAMEINPUT_API_VERSION < 4
    const GameInputDeviceInfo *info = data->info;
#endif
    GAMEINPUT_InternalJoystickHwdata *hwdata = NULL;
    GameInputRumbleMotors supportedRumbleMotors;
    IGameInputDevice *pDevice = data->device;

    hwdata = (GAMEINPUT_InternalJoystickHwdata *)SDL_calloc(1, sizeof(*hwdata));
    if (!hwdata) {
        return false;
    }

    pDevice->AddRef();
    hwdata->device = pDevice;
#if GAMEINPUT_API_VERSION < 4
    hwdata->info = info;
#endif
    hwdata->raw_type = data->raw_type;

    joystick->hwdata = hwdata;
    if (GAMEINPUT_InternalIsGamepad(pDevice) || GAMEINPUT_GetDeviceRawType(pDevice) != SDL_GAMEINPUT_RAWTYPE_NONE) {
        joystick->naxes = 6;
        joystick->nbuttons = 11;
        joystick->nhats = 1;

#if GAMEINPUT_API_VERSION >= 4
        if (GAMEINPUT_HasShareButton(pDevice, data->vendor, data->product)) {
            ++joystick->nbuttons;
        }
        if (GAMEINPUT_InternalIsGamepad(pDevice)) {
            hwdata->supported_paddles = GAMEINPUT_GetSupportedPaddles(pDevice);
            if (hwdata->supported_paddles) {
                hwdata->first_paddle_button = (Uint8)GAMEINPUT_GetFirstPaddleButton(pDevice, data->vendor, data->product);
                SDL_assert(hwdata->first_paddle_button == joystick->nbuttons);
                for (size_t i = 0; i < SDL_arraysize(GAMEINPUT_Paddles); ++i) {
                    if (hwdata->supported_paddles & GAMEINPUT_Paddles[i]) {
                        ++joystick->nbuttons;
                    }
                }
            }
        }
#endif // GAMEINPUT_API_VERSION >= 4
    } else {
#if GAMEINPUT_API_VERSION >= 4
        joystick->naxes = pDevice->GetControllerAxisCount();
        joystick->nbuttons = pDevice->GetControllerButtonCount();
        joystick->nhats = pDevice->GetControllerSwitchCount();
#else
        joystick->naxes = info->controllerAxisCount;
        joystick->nbuttons = info->controllerButtonCount;
        joystick->nhats = info->controllerSwitchCount;
#endif // GAMEINPUT_API_VERSION >= 4
    }

#if GAMEINPUT_API_VERSION >= 4
    supportedRumbleMotors = pDevice->GetSupportedRumbleMotors();
#else
    supportedRumbleMotors = info->supportedRumbleMotors;
#endif
    if (supportedRumbleMotors & (GameInputRumbleLowFrequency | GameInputRumbleHighFrequency)) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    }
    if (supportedRumbleMotors & (GameInputRumbleLeftTrigger | GameInputRumbleRightTrigger)) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, true);
    }

#if GAMEINPUT_API_VERSION >= 4
    if (pDevice->GetSupportedInput() & GameInputKindSensors) {
        GameInputSensorsInfo sensorsInfo;
        if (pDevice->GetSensorsInfo(&sensorsInfo)) {
            hwdata->supported_sensors = sensorsInfo.supportedSensors;
        }
        if (hwdata->supported_sensors & GameInputSensorsGyrometer) {
            SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 60.0f);
        }
        if (hwdata->supported_sensors & GameInputSensorsAccelerometer) {
            SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 60.0f);
        }
    }

    if (pDevice->GetSupportedLightingGroups() != GameInputLightingGroupUnknown) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, true);
    }

    if (pDevice->GetSupportedInput() & GameInputKindTouch) {
        // FIXME: Need to add touch support
    }
#endif // GAMEINPUT_API_VERSION >= 4

    return true;
}

static bool GAMEINPUT_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    // don't check for caps here, since SetRumbleState doesn't return any result - we don't need to check it
    GAMEINPUT_InternalJoystickHwdata *hwdata = joystick->hwdata;
    GameInputRumbleParams *params = &hwdata->rumbleParams;
    params->lowFrequency = (float)low_frequency_rumble / (float)SDL_MAX_UINT16;
    params->highFrequency = (float)high_frequency_rumble / (float)SDL_MAX_UINT16;
    hwdata->device->SetRumbleState(params);
    return true;
}

static bool GAMEINPUT_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    // don't check for caps here, since SetRumbleState doesn't return any result - we don't need to check it
    GAMEINPUT_InternalJoystickHwdata *hwdata = joystick->hwdata;
    GameInputRumbleParams *params = &hwdata->rumbleParams;
    params->leftTrigger = (float)left_rumble / (float)SDL_MAX_UINT16;
    params->rightTrigger = (float)right_rumble / (float)SDL_MAX_UINT16;
    hwdata->device->SetRumbleState(params);
    return true;
}

static bool GAMEINPUT_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
#if GAMEINPUT_API_VERSION >= 4
    GAMEINPUT_InternalJoystickHwdata *hwdata = joystick->hwdata;
    IGameInputDevice *device = hwdata->device;

    if (device->GetSupportedLightingGroups() == GameInputLightingGroupUnknown) {
        return SDL_Unsupported();
    }

    GameInputColor color = { red, green, blue };
    device->SetLightingColor(color, GameInputLightingGroupAll);
    device->ApplyLightingColors();
    hwdata->led_changed = true;
    return true;
#else
    return SDL_Unsupported();
#endif
}

static bool GAMEINPUT_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    joystick->hwdata->report_sensors = enabled;
    return true;
}
static void GAMEINPUT_DrumUpdate(SDL_Joystick *joystick, IGameInputReading *reading, Uint64 timestamp)
{
#if GAMEINPUT_API_VERSION >= 4
    IGameInputRawDeviceReport *rawState;
    if (reading->GetRawReport(&rawState)) {
        uint8_t rawData[40];
        SDL_memset(rawData, 0, sizeof(rawData));
        size_t len = rawState->GetRawData(sizeof(rawData), rawData);
        Uint8 hat = 0;
        uint16_t buttons = rawData[0] | rawData[1] << 8;
        if (len >= 6) {
            if ((buttons & 0x0100) || (rawData[4] & 0xf0)) {
                hat |= SDL_HAT_UP;
            }
            if ((buttons & 0x0200) || (rawData[4] & 0x0f)) {
                hat |= SDL_HAT_DOWN;
            }
            if (buttons & 0x0400) {
                hat |= SDL_HAT_LEFT;
            }
            if (buttons & 0x0800) {
                hat |= SDL_HAT_RIGHT;
            }
            SDL_SendJoystickHat(timestamp, joystick, 0, hat);
            // The rest of the instruments are mapped to PS3 style inputs
            // The PS3 used flags for dictating if a pad or a cymbal was hit, so we emulate that here
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, (buttons & 0x0008));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (buttons & 0x0004));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (buttons & 0x1000));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (buttons & 0x2000));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, (rawData[3] & 0x0f) || (rawData[5] & 0xf0) || (buttons & 0x0010));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, (rawData[2] & 0xf0) || (buttons & 0x0020));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, (rawData[3] & 0xf0) || (rawData[4] & 0x0f) || (buttons & 0x0040));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, (rawData[2] & 0x0f) || (rawData[4] & 0xf0) || (buttons & 0x0080));
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, rawData[2] || rawData[3]);
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, rawData[4] || rawData[5]);
        }
    }
#endif // GAMEINPUT_API_VERSION >= 4
}
static void GAMEINPUT_GuitarUpdate(SDL_Joystick *joystick, IGameInputReading *reading, Uint64 timestamp)
{
#if GAMEINPUT_API_VERSION >= 4
    IGameInputRawDeviceReport *rawState;
    if (reading->GetRawReport(&rawState)) {
        static WORD s_GuitarButtons[] = {
            0x0010,  // SDL_GAMEPAD_BUTTON_SOUTH
            0x0020,  // SDL_GAMEPAD_BUTTON_EAST
            0x0040,  // SDL_GAMEPAD_BUTTON_WEST
            0x0080,  // SDL_GAMEPAD_BUTTON_NORTH
            0x0008,  // SDL_GAMEPAD_BUTTON_BACK
            0,       // The guide button is not available
            0x0004,  // SDL_GAMEPAD_BUTTON_START
            0,       // right joystick click unavailable
            0x4000,  // SDL_GAMEPAD_BUTTON_RIGHT_STICK
            0x1000,  // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
            0,       // right shoulder unavailable
        };
        Uint8 btnidx = 0, hat = 0;
        uint8_t rawData[40];
        SDL_memset(rawData, 0, sizeof(rawData));
        size_t len = rawState->GetRawData(sizeof(rawData), rawData);
        uint16_t buttons = rawData[0] | rawData[1] << 8;
        if (len >= 10) {
            for (btnidx = 0; btnidx < SDL_arraysize(s_GuitarButtons); ++btnidx) {
                WORD button_mask = s_GuitarButtons[btnidx];
                if (!button_mask) {
                    continue;
                }
                bool down = ((buttons & button_mask) != 0);
                SDL_SendJoystickButton(timestamp, joystick, btnidx, down);
            }
            if (buttons & 0x0100) {
                hat |= SDL_HAT_UP;
            }
            if (buttons & 0x0200) {
                hat |= SDL_HAT_DOWN;
            }
            if (buttons & 0x0400) {
                hat |= SDL_HAT_LEFT;
            }
            if (buttons & 0x0800) {
                hat |= SDL_HAT_RIGHT;
            }
            SDL_SendJoystickHat(timestamp, joystick, 0, hat);
            SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, (rawData[3] * 257) - 32768);
            // PS3 RB guitars had tilt on right shoulder
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, rawData[2] >= 0xD0);
            // PS3 RB guitars send L2 when using solo buttons
            SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, (rawData[6]) ? 32767 : -32768);
            // Align pickup selector mappings with PS3 instruments
            static const Sint16 effects_mappings[] = {-26880, -13568, -1792, 11008, 24576};
            SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, effects_mappings[rawData[4] >> 4]);
        }
    }
#endif // GAMEINPUT_API_VERSION >= 4
}

static void GAMEINPUT_GamepadUpdate(SDL_Joystick *joystick, IGameInputReading *reading, Uint64 timestamp) {
    GameInputGamepadState state;
    static WORD s_XInputButtons[] = {
        GameInputGamepadA,               // SDL_GAMEPAD_BUTTON_SOUTH
        GameInputGamepadB,               // SDL_GAMEPAD_BUTTON_EAST
        GameInputGamepadX,               // SDL_GAMEPAD_BUTTON_WEST
        GameInputGamepadY,               // SDL_GAMEPAD_BUTTON_NORTH
        GameInputGamepadView,            // SDL_GAMEPAD_BUTTON_BACK
        0,                               // The guide button is not available
        GameInputGamepadMenu,            // SDL_GAMEPAD_BUTTON_START
        GameInputGamepadLeftThumbstick,  // SDL_GAMEPAD_BUTTON_LEFT_STICK
        GameInputGamepadRightThumbstick, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
        GameInputGamepadLeftShoulder,    // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
        GameInputGamepadRightShoulder,   // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
    };
    Uint8 btnidx = 0, hat = 0;

    if (reading->GetGamepadState(&state)) {
        for (btnidx = 0; btnidx < SDL_arraysize(s_XInputButtons); ++btnidx) {
            WORD button_mask = s_XInputButtons[btnidx];
            if (!button_mask) {
                continue;
            }
            bool down = ((state.buttons & button_mask) != 0);
            SDL_SendJoystickButton(timestamp, joystick, btnidx, down);
        }
#if GAMEINPUT_API_VERSION >= 4
        GameInputSystemButtons systemButtons;
        if (reading->GetSystemButtonState(&systemButtons)) {
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (systemButtons & GameInputSystemButtonGuide) != 0);
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GAMEINPUT_SHARE, (systemButtons & GameInputSystemButtonShare) != 0);
        }

        if (joystick->hwdata->supported_paddles) {
            Uint8 paddle_button = joystick->hwdata->first_paddle_button;
            for (size_t i = 0; i < SDL_arraysize(GAMEINPUT_Paddles); ++i) {
                if (joystick->hwdata->supported_paddles & GAMEINPUT_Paddles[i]) {
                    SDL_SendJoystickButton(timestamp, joystick, paddle_button++, (state.buttons & GAMEINPUT_Paddles[i]) != 0);
                }
            }
        }
#else
        // GAMEINPUT_InternalSystemButtonCallback is used instead
#endif

        if (state.buttons & GameInputGamepadDPadUp) {
            hat |= SDL_HAT_UP;
        }
        if (state.buttons & GameInputGamepadDPadDown) {
            hat |= SDL_HAT_DOWN;
        }
        if (state.buttons & GameInputGamepadDPadLeft) {
            hat |= SDL_HAT_LEFT;
        }
        if (state.buttons & GameInputGamepadDPadRight) {
            hat |= SDL_HAT_RIGHT;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);

#define CONVERT_AXIS(v) (Sint16)(((v) < 0.0f) ? ((v)*32768.0f) : ((v)*32767.0f))
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, CONVERT_AXIS(state.leftThumbstickX));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, CONVERT_AXIS(-state.leftThumbstickY));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, CONVERT_AXIS(state.rightThumbstickX));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, CONVERT_AXIS(-state.rightThumbstickY));
#undef CONVERT_AXIS
#define CONVERT_TRIGGER(v) (Sint16)((v)*65535.0f - 32768.0f)
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, CONVERT_TRIGGER(state.leftTrigger));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, CONVERT_TRIGGER(state.rightTrigger));
#undef CONVERT_TRIGGER
    }
}

static void GAMEINPUT_ControllerUpdate(SDL_Joystick *joystick, IGameInputReading *reading, Uint64 timestamp)
{
    bool *button_state = SDL_stack_alloc(bool, joystick->nbuttons);
    float *axis_state = SDL_stack_alloc(float, joystick->naxes);
    GameInputSwitchPosition *switch_state = SDL_stack_alloc(GameInputSwitchPosition, joystick->nhats);

    if (button_state) {
        uint32_t i;
        uint32_t button_count = reading->GetControllerButtonState(joystick->nbuttons, button_state);
        for (i = 0; i < button_count; ++i) {
            SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, button_state[i]);
        }
        SDL_stack_free(button_state);
    }

#define CONVERT_AXIS(v) (Sint16)((v)*65535.0f - 32768.0f)
    if (axis_state) {
        uint32_t i;
        uint32_t axis_count = reading->GetControllerAxisState(joystick->naxes, axis_state);
        for (i = 0; i < axis_count; ++i) {
            SDL_SendJoystickAxis(timestamp, joystick, (Uint8)i, CONVERT_AXIS(axis_state[i]));
        }
        SDL_stack_free(axis_state);
    }
#undef CONVERT_AXIS

    if (switch_state) {
        uint32_t i;
        uint32_t switch_count = reading->GetControllerSwitchState(joystick->nhats, switch_state);
        for (i = 0; i < switch_count; ++i) {
            Uint8 hat;
            switch (switch_state[i]) {
            case GameInputSwitchUp:
                hat = SDL_HAT_UP;
                break;
            case GameInputSwitchUpRight:
                hat = SDL_HAT_UP | SDL_HAT_RIGHT;
                break;
            case GameInputSwitchRight:
                hat = SDL_HAT_RIGHT;
                break;
            case GameInputSwitchDownRight:
                hat = SDL_HAT_DOWN | SDL_HAT_RIGHT;
                break;
            case GameInputSwitchDown:
                hat = SDL_HAT_DOWN;
                break;
            case GameInputSwitchDownLeft:
                hat = SDL_HAT_DOWN | SDL_HAT_LEFT;
                break;
            case GameInputSwitchLeft:
                hat = SDL_HAT_LEFT;
                break;
            case GameInputSwitchUpLeft:
                hat = SDL_HAT_UP | SDL_HAT_LEFT;
                break;
            case GameInputSwitchCenter:
            default:
                hat = SDL_HAT_CENTERED;
                break;
            }
            SDL_SendJoystickHat(timestamp, joystick, (Uint8)i, hat);
        }
        SDL_stack_free(switch_state);
    }
}

static void GAMEINPUT_JoystickUpdate(SDL_Joystick *joystick)
{
    GAMEINPUT_InternalJoystickHwdata *hwdata = joystick->hwdata;
    IGameInputDevice *device = hwdata->device;
#if GAMEINPUT_API_VERSION < 4
    const GameInputDeviceInfo *info = hwdata->info;
#endif
    IGameInputReading *reading = NULL;
    Uint64 timestamp;
    HRESULT hr;

#if GAMEINPUT_API_VERSION >= 4
    hr = g_pGameInput->GetCurrentReading(device->GetSupportedInput(), device, &reading);
#else
    hr = g_pGameInput->GetCurrentReading(info->supportedInput, device, &reading);
#endif
    if (FAILED(hr)) {
        // don't SetError here since there can be a legitimate case when there's no reading avail
        return;
    }

    timestamp = SDL_US_TO_NS(reading->GetTimestamp() + g_GameInputTimestampOffset);
    if (hwdata->raw_type == SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_GUITAR) {
        GAMEINPUT_GuitarUpdate(joystick, reading, timestamp);
    } else if (hwdata->raw_type == SDL_GAMEINPUT_RAWTYPE_ROCK_BAND_DRUM_KIT) {
        GAMEINPUT_DrumUpdate(joystick, reading, timestamp);
    } else if (GAMEINPUT_InternalIsGamepad(device)) {
        GAMEINPUT_GamepadUpdate(joystick, reading, timestamp);
    } else {
        GAMEINPUT_ControllerUpdate(joystick, reading, timestamp);
    }

#if GAMEINPUT_API_VERSION >= 4
    if (hwdata->report_sensors) {
        GameInputSensorsState sensor_state;

        if (reading->GetSensorsState(&sensor_state)) {
            GameInputSensorsKind supportedSensors = hwdata->supported_sensors;
            if ((supportedSensors & GameInputSensorsGyrometer) != 0) {
                float data[3] = {
                    sensor_state.angularVelocityInRadPerSecX,
                    sensor_state.angularVelocityInRadPerSecY,
                    sensor_state.angularVelocityInRadPerSecZ
                };
                SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, timestamp, data, SDL_arraysize(data));
            }
            if ((supportedSensors & GameInputSensorsAccelerometer) != 0) {
                float data[3] = {
                    sensor_state.accelerationInGX * SDL_STANDARD_GRAVITY,
                    sensor_state.accelerationInGY * SDL_STANDARD_GRAVITY,
                    sensor_state.accelerationInGZ * SDL_STANDARD_GRAVITY
                };
                SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, timestamp, data, SDL_arraysize(data));
            }
        }
    }
#endif // GAMEINPUT_API_VERSION >= 4

    reading->Release();

    // FIXME: We can poll this at a much lower rate
    GAMEINPUT_UpdatePowerInfo(joystick, device);
}

static void GAMEINPUT_JoystickClose(SDL_Joystick *joystick)
{
    GAMEINPUT_InternalJoystickHwdata *hwdata = joystick->hwdata;

#if GAMEINPUT_API_VERSION >= 4
    if (hwdata->led_changed) {
        hwdata->device->ApplyLightingSystemDefault();
    }
#else
    if (hwdata->system_button_callback_token) {
        g_pGameInput->UnregisterCallback(hwdata->system_button_callback_token, 10000);
    }
#endif
    hwdata->device->Release();
    SDL_free(hwdata);

    joystick->hwdata = NULL;
}

static void GAMEINPUT_JoystickQuit(void)
{
    if (g_pGameInput) {
        // free the callback
        if (g_GameInputCallbackToken) {
#if GAMEINPUT_API_VERSION >= 4
            g_pGameInput->UnregisterCallback(g_GameInputCallbackToken);
#else
            g_pGameInput->UnregisterCallback(g_GameInputCallbackToken, 10000);
#endif
            g_GameInputCallbackToken = 0;
        }

        // free the list
        SDL_LockSpinlock(&g_GameInputDevicesLock);
        for (int i = 0; i < g_GameInputDeviceCount; ++i) {
            g_GameInputDevices[i]->Release();
        }
        SDL_free(g_GameInputDevices);
        g_GameInputDevices = NULL;
        g_GameInputDeviceCount = 0;
        g_GameInputInitialized = false;
        SDL_UnlockSpinlock(&g_GameInputDevicesLock);

        SDL_QuitGameInput();
        g_pGameInput = NULL;
    }
}

static bool GAMEINPUT_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    const SDL_JoystickDeviceData *data = device->internal;

    if (!GAMEINPUT_InternalIsGamepad(data->device) && data->raw_type == SDL_GAMEINPUT_RAWTYPE_NONE) {
        return false;
    }

    out->a.kind = EMappingKind_Button;
    out->a.target = SDL_GAMEPAD_BUTTON_SOUTH;

    out->b.kind = EMappingKind_Button;
    out->b.target = SDL_GAMEPAD_BUTTON_EAST;

    out->x.kind = EMappingKind_Button;
    out->x.target = SDL_GAMEPAD_BUTTON_WEST;

    out->y.kind = EMappingKind_Button;
    out->y.target = SDL_GAMEPAD_BUTTON_NORTH;

    out->back.kind = EMappingKind_Button;
    out->back.target = SDL_GAMEPAD_BUTTON_BACK;

#if GAMEINPUT_API_VERSION >= 4
    GameInputSystemButtons supportedSystemButtons = data->device->GetSupportedSystemButtons();
    if (supportedSystemButtons & GameInputSystemButtonGuide) {
        out->guide.kind = EMappingKind_Button;
        out->guide.target = SDL_GAMEPAD_BUTTON_GUIDE;
    }

    if (GAMEINPUT_HasShareButton(data->device, data->vendor, data->product)) {
        out->misc1.kind = EMappingKind_Button;
        out->misc1.target = SDL_GAMEPAD_BUTTON_GAMEINPUT_SHARE;
    }

    if (GAMEINPUT_InternalIsGamepad(data->device)) {
        GameInputGamepadButtons supported_paddles = GAMEINPUT_GetSupportedPaddles(data->device);
        Uint8 paddle_button = (Uint8)GAMEINPUT_GetFirstPaddleButton(data->device, data->vendor, data->product);
        for (size_t i = 0; i < SDL_arraysize(GAMEINPUT_Paddles); ++i) {
            if (!(supported_paddles & GAMEINPUT_Paddles[i])) {
                continue;
            }

            SDL_InputMapping *mapping = NULL;
            switch (GAMEINPUT_Paddles[i]) {
            case GameInputGamepadPaddleRight1:
                mapping = &out->right_paddle1;
                break;
            case GameInputGamepadPaddleLeft1:
                mapping = &out->left_paddle1;
                break;
            case GameInputGamepadPaddleRight2:
                mapping = &out->right_paddle2;
                break;
            case GameInputGamepadPaddleLeft2:
                mapping = &out->left_paddle2;
                break;
            default:
                break;
            }
            if (mapping) {
                mapping->kind = EMappingKind_Button;
                mapping->target = paddle_button++;
            }
        }
    }
#endif // GAMEINPUT_API_VERSION >= 4

    out->start.kind = EMappingKind_Button;
    out->start.target = SDL_GAMEPAD_BUTTON_START;

    out->leftstick.kind = EMappingKind_Button;
    out->leftstick.target = SDL_GAMEPAD_BUTTON_LEFT_STICK;

    out->rightstick.kind = EMappingKind_Button;
    out->rightstick.target = SDL_GAMEPAD_BUTTON_RIGHT_STICK;

    out->leftshoulder.kind = EMappingKind_Button;
    out->leftshoulder.target = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;

    out->rightshoulder.kind = EMappingKind_Button;
    out->rightshoulder.target = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;

    out->dpup.kind = EMappingKind_Hat;
    out->dpup.target = SDL_HAT_UP;

    out->dpdown.kind = EMappingKind_Hat;
    out->dpdown.target = SDL_HAT_DOWN;

    out->dpleft.kind = EMappingKind_Hat;
    out->dpleft.target = SDL_HAT_LEFT;

    out->dpright.kind = EMappingKind_Hat;
    out->dpright.target = SDL_HAT_RIGHT;

    out->leftx.kind = EMappingKind_Axis;
    out->leftx.target = SDL_GAMEPAD_AXIS_LEFTX;

    out->lefty.kind = EMappingKind_Axis;
    out->lefty.target = SDL_GAMEPAD_AXIS_LEFTY;

    out->rightx.kind = EMappingKind_Axis;
    out->rightx.target = SDL_GAMEPAD_AXIS_RIGHTX;

    out->righty.kind = EMappingKind_Axis;
    out->righty.target = SDL_GAMEPAD_AXIS_RIGHTY;

    out->lefttrigger.kind = EMappingKind_Axis;
    out->lefttrigger.target = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;

    out->righttrigger.kind = EMappingKind_Axis;
    out->righttrigger.target = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;

    return true;
}


// This file is compiled as C++, so don't use designated initializers here
SDL_JoystickDriver SDL_GAMEINPUT_JoystickDriver =
{
    GAMEINPUT_JoystickInit,
    GAMEINPUT_JoystickQueryDevices,
    NULL, // DetectDevices
    GAMEINPUT_JoystickPrepareDevice,
    NULL, // UpdateSteamVirtualGamepadSlot
    GAMEINPUT_JoystickGetGamepadMapping,
    GAMEINPUT_JoystickOpen,
    GAMEINPUT_JoystickRumble,
    GAMEINPUT_JoystickRumbleTriggers,
    GAMEINPUT_JoystickSetLED,
    NULL, // SendEffect
    GAMEINPUT_JoystickSetSensorsEnabled,
    NULL, // SetPlayerIndex
    GAMEINPUT_JoystickUpdate,
    GAMEINPUT_JoystickClose,
    GAMEINPUT_JoystickQuit
};

#endif // SDL_JOYSTICK_GAMEINPUT
