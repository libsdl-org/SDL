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

#ifndef SDL_JOYSTICK_HIDAPI_H
#define SDL_JOYSTICK_HIDAPI_H

#include "../SDL_sysjoystick.h"
#include "../usb_ids.h"

// This is the full set of HIDAPI drivers available
#define SDL_JOYSTICK_HIDAPI_GAMECUBE
#define SDL_JOYSTICK_HIDAPI_LUNA
#define SDL_JOYSTICK_HIDAPI_PS3
#define SDL_JOYSTICK_HIDAPI_PS4
#define SDL_JOYSTICK_HIDAPI_PS5
#define SDL_JOYSTICK_HIDAPI_STADIA
#define SDL_JOYSTICK_HIDAPI_STEAM
#define SDL_JOYSTICK_HIDAPI_STEAMDECK
#define SDL_JOYSTICK_HIDAPI_SWITCH
#ifdef HAVE_LIBUSB
#define SDL_JOYSTICK_HIDAPI_SWITCH2
#endif
#define SDL_JOYSTICK_HIDAPI_WII
#define SDL_JOYSTICK_HIDAPI_XBOX360
#define SDL_JOYSTICK_HIDAPI_XBOXONE
#define SDL_JOYSTICK_HIDAPI_SHIELD
#define SDL_JOYSTICK_HIDAPI_STEAM_HORI
#define SDL_JOYSTICK_HIDAPI_STEAM_TRITON
#define SDL_JOYSTICK_HIDAPI_LG4FF
#define SDL_JOYSTICK_HIDAPI_8BITDO
#define SDL_JOYSTICK_HIDAPI_FLYDIGI
#define SDL_JOYSTICK_HIDAPI_GAMESIR
#define SDL_JOYSTICK_HIDAPI_GIP
#define SDL_JOYSTICK_HIDAPI_SINPUT
#define SDL_JOYSTICK_HIDAPI_ZUIKI

// Joystick capability definitions
#define SDL_JOYSTICK_CAP_MONO_LED       0x00000001
#define SDL_JOYSTICK_CAP_RGB_LED        0x00000002
#define SDL_JOYSTICK_CAP_PLAYER_LED     0x00000004
#define SDL_JOYSTICK_CAP_RUMBLE         0x00000010
#define SDL_JOYSTICK_CAP_TRIGGER_RUMBLE 0x00000020

// Whether HIDAPI is enabled by default
#if defined(SDL_PLATFORM_ANDROID) || \
    defined(SDL_PLATFORM_IOS) || \
    defined(SDL_PLATFORM_TVOS) || \
    defined(SDL_PLATFORM_VISIONOS)
// On Android, HIDAPI prompts for permissions and acquires exclusive access to the device, and on Apple mobile platforms it doesn't do anything except for handling Bluetooth Steam Controllers, so we'll leave it off by default.
#define SDL_HIDAPI_DEFAULT false
#else
#define SDL_HIDAPI_DEFAULT true
#endif

// The maximum size of a USB packet for HID devices
#define USB_PACKET_LENGTH 64

// Forward declaration
struct SDL_HIDAPI_DeviceDriver;

typedef struct SDL_HIDAPI_Device
{
    char *name;
    char *manufacturer_string;
    char *product_string;
    char *path;
    Uint16 vendor_id;
    Uint16 product_id;
    Uint16 version;
    char *serial;
    SDL_GUID guid;
    int interface_number; // Available on Windows and Linux
    int interface_class;
    int interface_subclass;
    int interface_protocol;
    Uint16 usage_page; // Available on Windows and macOS
    Uint16 usage;      // Available on Windows and macOS
    bool is_bluetooth;
    SDL_JoystickType joystick_type;
    SDL_GamepadType type;
    int steam_virtual_gamepad_slot;

    struct SDL_HIDAPI_DeviceDriver *driver;
    void *context;
    SDL_hid_device *dev;
    SDL_AtomicInt rumble_pending;
    int num_joysticks;
    // Driver instance IDs, use SDL_GetJoystickFromDriverID() to find their open joysticks
    SDL_JoystickID *joysticks;

    // Opened and matched with a driver on the joystick device thread, before being added to the list
    bool prepared;
    struct SDL_HIDAPI_DeviceDriver *prepared_driver;

    // Set when the device thread couldn't open the device, so it isn't retried until devices change
    bool prepare_failed;

    // A second handle a driver opened in PrepareDevice(), e.g. GameSir's output interface. InitDevice() takes
    // it, otherwise it's closed when the device is freed.
    SDL_hid_device *aux_dev;

    // Set when UpdateDevice() fails, which drivers do on read errors
    bool update_failed;

    // A claim-only joystick device for this device's VID/PID, see HIDAPI_UpdateDeviceClaim()
    SDL_JoystickID claim_id;
    SDL_GUID container_id;
    SDL_GamepadType claim_type;

    // Whether the driver can handle the controller, for drivers that only find out after setup (Flydigi)
    SDL_JoystickUsable usable;

    // Used to flag devices that failed open
    // This can happen on Windows with Bluetooth devices that have turned off
    bool broken;

    struct SDL_HIDAPI_Device *parent;
    int num_children;
    struct SDL_HIDAPI_Device **children;

    struct SDL_HIDAPI_Device *next;
} SDL_HIDAPI_Device;

typedef struct SDL_HIDAPI_DeviceDriver
{
    const char *name;
    bool enabled;
    void (*RegisterHints)(SDL_HintCallback callback, void *userdata);
    void (*UnregisterHints)(SDL_HintCallback callback, void *userdata);
    bool (*IsEnabled)(void);
    bool (*IsSupportedDevice)(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol);
    bool (*InitDevice)(SDL_HIDAPI_Device *device);
    int (*GetDevicePlayerIndex)(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id);
    void (*SetDevicePlayerIndex)(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index);
    bool (*UpdateDevice)(SDL_HIDAPI_Device *device);
    bool (*OpenJoystick)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick);
    // Optional: RumbleJoystick, RumbleJoystickTriggers, SetJoystickLED, SendJoystickEffect and SetJoystickSensorsEnabled
    bool (*RumbleJoystick)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble);
    bool (*RumbleJoystickTriggers)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble);
    Uint32 (*GetJoystickCapabilities)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick);
    bool (*SetJoystickLED)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue);
    bool (*SendJoystickEffect)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size);
    bool (*SetJoystickSensorsEnabled)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled);
    void (*CloseJoystick)(SDL_HIDAPI_Device *device, SDL_Joystick *joystick);
    void (*FreeDevice)(SDL_HIDAPI_Device *device);

    // Optional: set up the device on the joystick device thread after it's opened, before it's added, for
    // work that would block the app thread (enumerating devices, slow USB transfers, waiting for replies to
    // queries). It runs without the joystick lock, so it may only touch the device itself: it can create
    // device->context for InitDevice() to use, but changes to the device's name, serial or joysticks are
    // left to InitDevice(). If it fails, the device isn't used until devices change. A driver that has it
    // only has InitDevice() called after it succeeded.
    bool (*PrepareDevice)(SDL_HIDAPI_Device *device);

} SDL_HIDAPI_DeviceDriver;

// HIDAPI device support
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverCombined;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGameCube;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGIP;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverJoyCons;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverLuna;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverNintendoClassic;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS3;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS3ThirdParty;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS3SonySixaxis;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS4;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS5;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverShield;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverStadia;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteam;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteamDeck;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSwitch;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSwitch2;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverWii;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360W;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360BB;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXboxOne;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteamHori;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteamTriton;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverLg4ff;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_Driver8BitDo;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverFlydigi;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGameSir;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSInput;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverZUIKI;

#define LOAD16(A, B)       (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))
#define LOAD32(A, B, C, D) ((((Uint32)(A)) << 0) |  \
                            (((Uint32)(B)) << 8) |  \
                            (((Uint32)(C)) << 16) | \
                            (((Uint32)(D)) << 24))

// Return the type of a joystick if it's present and supported
extern SDL_JoystickType HIDAPI_GetJoystickTypeFromGUID(SDL_GUID guid);

// Return the type of a game controller if it's present and supported
extern SDL_GamepadType HIDAPI_GetGamepadTypeFromGUID(SDL_GUID guid);

extern void HIDAPI_UpdateDevices(void);
extern void HIDAPI_SetDeviceName(SDL_HIDAPI_Device *device, const char *name);
extern void HIDAPI_SetDeviceProduct(SDL_HIDAPI_Device *device, Uint16 vendor_id, Uint16 product_id);
extern void HIDAPI_SetDeviceUsable(SDL_HIDAPI_Device *device, SDL_JoystickUsable usable);
extern void HIDAPI_SetDeviceGUIDVersion(SDL_HIDAPI_Device *device, Uint16 version);
extern void HIDAPI_SetDeviceGUIDDriverData(SDL_HIDAPI_Device *device, Uint8 driver_data);
extern void HIDAPI_SetDeviceSerial(SDL_HIDAPI_Device *device, const char *serial);
extern bool HIDAPI_HasConnectedUSBDevice(const char *serial);
extern void HIDAPI_DisconnectBluetoothDevice(const char *serial);
extern bool HIDAPI_JoystickConnected(SDL_HIDAPI_Device *device, SDL_JoystickID *pJoystickID);
extern void HIDAPI_JoystickDisconnected(SDL_HIDAPI_Device *device, SDL_JoystickID joystickID);
extern void HIDAPI_UpdateDeviceProperties(SDL_HIDAPI_Device *device);
extern void HIDAPI_UpdateJoystickDeviceName(SDL_JoystickDevice *joystick_device);

extern void HIDAPI_DumpPacket(const char *prefix, const Uint8 *data, int size);

extern bool HIDAPI_SupportsPlaystationDetection(Uint16 vendor, Uint16 product);

extern float HIDAPI_RemapVal(float val, float val_min, float val_max, float output_min, float output_max);

#endif // SDL_JOYSTICK_HIDAPI_H
