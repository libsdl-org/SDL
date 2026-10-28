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

#ifndef SDL_sysjoystick_h_
#define SDL_sysjoystick_h_

// This is the system specific header for the SDL joystick API
#include "SDL_joystick_c.h"
#include "../events/SDL_events_c.h"

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

// The SDL joystick structure

typedef struct SDL_JoystickAxisInfo
{
    Sint16 initial_value;           // Initial axis state
    Sint16 value;                   // Current axis state
    Sint16 zero;                    // Zero point on the axis (-32768 for triggers)
    bool has_initial_value;     // Whether we've seen a value on the axis yet
    bool has_second_value;      // Whether we've seen a second value on the axis yet
    bool sent_initial_value;    // Whether we've sent the initial axis value
    bool sending_initial_value; // Whether we are sending the initial axis value
} SDL_JoystickAxisInfo;

typedef struct SDL_JoystickBallData
{
    int dx;
    int dy;
} SDL_JoystickBallData;

typedef struct SDL_JoystickTouchpadFingerInfo
{
    bool down;
    float x;
    float y;
    float pressure;
} SDL_JoystickTouchpadFingerInfo;

typedef struct SDL_JoystickTouchpadInfo
{
    int nfingers;
    SDL_JoystickTouchpadFingerInfo *fingers;
} SDL_JoystickTouchpadInfo;

typedef struct SDL_JoystickSensorInfo
{
    SDL_SensorType type;
    bool enabled;
    float rate;
    float data[3]; // If this needs to expand, update SDL_GamepadSensorEvent
} SDL_JoystickSensorInfo;

typedef struct SDL_JoystickCapSenseInfo
{
    SDL_GamepadCapSenseType type;
    bool down;
} SDL_JoystickCapSenseInfo;

#define _guarded SDL_GUARDED_BY(SDL_event_lock)

struct SDL_Joystick
{
    SDL_JoystickID instance_id _guarded; // Device instance, monotonically increasing from 0
    SDL_JoystickID driver_instance_id _guarded; // The device's driver instance ID, for drivers that need to know which device this is
    char *name _guarded;                 // Joystick name - system dependent
    char *path _guarded;                 // Joystick path - system dependent
    char *serial _guarded;               // Joystick serial
    SDL_GUID guid _guarded;      // Joystick guid
    Uint16 firmware_version _guarded;    // Firmware version, if available
    Uint64 steam_handle _guarded;        // Steam controller API handle
    bool swap_face_buttons _guarded;     // Whether we should swap face buttons
    bool is_virtual _guarded;            // Whether this is a virtual joystick

    int naxes _guarded; // Number of axis controls on the joystick
    SDL_JoystickAxisInfo *axes _guarded;

    int nballs _guarded; // Number of trackballs on the joystick
    SDL_JoystickBallData *balls _guarded; // Current ball motion deltas

    int nhats _guarded;   // Number of hats on the joystick
    Uint8 *hats _guarded; // Current hat states

    int nbuttons _guarded;   // Number of buttons on the joystick
    bool *buttons _guarded; // Current button states

    int ntouchpads _guarded;                      // Number of touchpads on the joystick
    SDL_JoystickTouchpadInfo *touchpads _guarded; // Current touchpad states

    int nsensors _guarded; // Number of sensors on the joystick
    int nsensors_enabled _guarded;
    SDL_JoystickSensorInfo *sensors _guarded;

    int ncapsenses _guarded;                      // Number of capsense sources on the joystick
    SDL_JoystickCapSenseInfo *capsenses _guarded; // Current capsense states

    Uint16 low_frequency_rumble _guarded;
    Uint16 high_frequency_rumble _guarded;
    Uint64 rumble_expiration _guarded;
    Uint64 rumble_resend _guarded;

    Uint16 left_trigger_rumble _guarded;
    Uint16 right_trigger_rumble _guarded;
    Uint64 trigger_rumble_expiration _guarded;
    Uint64 trigger_rumble_resend _guarded;

    Uint8 led_red _guarded;
    Uint8 led_green _guarded;
    Uint8 led_blue _guarded;
    Uint64 led_expiration _guarded;

    bool attached _guarded;
    SDL_JoystickConnectionState connection_state _guarded;
    SDL_PowerState battery_state _guarded;
    int battery_percent _guarded;

    bool delayed_guide_button _guarded;      // true if this device has the guide button event delayed

    SDL_SensorID accel_sensor _guarded;
    SDL_Sensor *accel _guarded;
    SDL_SensorID gyro_sensor _guarded;
    SDL_Sensor *gyro _guarded;
    float sensor_transform[3][3] _guarded;

    Uint64 update_complete _guarded;

    struct SDL_JoystickDriver *driver _guarded;

    // Driver dependent information for the open joystick. It has to work after the joystick is detached,
    // so it holds its own copies and references, not pointers into the device's driver_data.
    struct joystick_hwdata *hwdata _guarded;

    SDL_PropertiesID props _guarded;

    int ref_count _guarded; // Reference count for multiple opens

    struct SDL_Joystick *next _guarded; // pointer to next joystick we have allocated
};

#undef _guarded

// How a device is matched with the same controller seen by other drivers

#define SDL_MAX_JOYSTICK_MATCHES 8

typedef struct SDL_JoystickMatch
{
    Uint16 vendor_id;       // Match on VID/PID if non-zero
    Uint16 product_id;
    SDL_GamepadType type;   // Match on type if vendor_id is 0
} SDL_JoystickMatch;

typedef struct SDL_JoystickMatchInfo
{
    // Never hidden by other drivers, and never hides other drivers' devices (e.g. virtual joysticks)
    bool never_group;

    // Other devices this one is the same controller as, in addition to its own vendor and product IDs
    int num_matches;
    SDL_JoystickMatch matches[SDL_MAX_JOYSTICK_MATCHES];
} SDL_JoystickMatchInfo;

// A joystick device that has been reported by a driver, whether or not it's published or open
typedef struct SDL_JoystickDevice
{
    struct SDL_JoystickDriver *driver;

    // The ID the driver knows the device by, used for its removal and updates.
    SDL_JoystickID driver_instance_id;

    // The public instance ID, assigned each time the device is published and 0 when it isn't.
    // This is a unique ID and will never match the driver_instance_id.
    SDL_JoystickID instance_id;

    // Driver data that the driver needs to describe and open the device. Once the device is reported, only
    // the driver's callbacks on the app thread use it, so they can update it without locking (e.g. to cache
    // a gamepad mapping). It's freed with FreeDeviceData(), or SDL_free() if the driver doesn't have one,
    // after the device is removed, which can happen while a joystick opened from it is still open.
    void *driver_data;

    // Description
    char *name;
    char *path;
    SDL_GUID guid;
    int steam_virtual_gamepad_slot;
    int player_index;

    // Matching with other drivers' devices. If the vendor and product IDs aren't set, they come from the
    // device GUID.
    Uint16 vendor_id;
    Uint16 product_id;
    SDL_GamepadType type;
    SDL_JoystickMatchInfo match;

    // The physical device it belongs to, all zero if unknown. Devices with different container IDs are never
    // the same controller, even if they match otherwise, e.g. two controllers of the same model where only one
    // works with a higher priority driver.
    SDL_GUID container_id;

    // A claim-only device is never published. It hides lower priority drivers' devices that match it,
    // e.g. for a controller that a driver is still setting up, or a dongle with no controllers connected.
    bool claim_only;
    SDL_JoystickUsable usable;

    // How long a claim with usable UNKNOWN hides matching devices that aren't published yet, 0 for the default
    Uint64 claim_hold_ms;

    // When the device was reported with, or changed to, usable UNKNOWN
    Uint64 unknown_since_ms;

    // Resolution state
    int rank;
    bool published;
    Uint64 reported_ms;
    bool hold_checked;
    Uint64 hold_until_ms;
    Uint64 hold_cap_ms;
    int hold_driver;
    int hold_pass;
} SDL_JoystickDevice;

// Device bus definitions
#define SDL_HARDWARE_BUS_UNKNOWN   0x00
#define SDL_HARDWARE_BUS_USB       0x03
#define SDL_HARDWARE_BUS_BLUETOOTH 0x05
#define SDL_HARDWARE_BUS_VIRTUAL   0xFF

// Macro to combine a USB vendor ID and product ID into a single Uint32 value
#define MAKE_VIDPID(VID, PID) (((Uint32)(VID)) << 16 | (PID))

// A joystick driver. Drivers report devices with SDL_CreateJoystickDevice() and SDL_ReportJoystickDevice(),
// from any thread, and remove them with SDL_ReportJoystickRemoved(). The core decides which devices are
// published and owns their instance IDs.
//
// Drivers in C use designated initializers for this table. The C++ drivers can't (that needs C++20), so the
// order of these fields matters to them.
typedef struct SDL_JoystickDriver
{
    // Optional slow setup, like loading libraries and creating API objects, called before Init() on the
    // thread calling SDL_Init(), without the joystick lock. It must not report devices or use joystick state.
    // Returns false if the driver isn't available, and then Init() isn't called. Quit() undoes it either way.
    bool (*PreInit)(void);

    /* Set up the driver, on the app thread holding the joystick lock. Drivers that don't have Discover()
       can report the devices that are already connected here. Returns false if the driver isn't available.
     */
    bool (*Init)(void);

    // Find devices, on the joystick device thread without the joystick lock. Results are reported, or posted
    // with SDL_PostJoystickDriverMessage(). Returns how long to wait in milliseconds before it needs to run
    // again, or -1 to wait until SDL_WakeJoystickDeviceThread() is called or the discovery fd is readable.
    // On platforms without the device thread, it runs in SDL_UpdateJoysticks().
    Sint32 (*Discover)(void);

    // Return a file descriptor that becomes readable when Discover() should run again, or -1
    int (*GetDiscoveryFD)(void);

    // Handle a message posted by Discover(), on the app thread holding the joystick lock
    void (*HandleMessage)(void *message);

    // Free a message that won't be handled, before the driver's Quit()
    void (*FreeMessage)(void *message);

    // Return whether this driver might handle a device that a lower priority driver has reported, without
    // doing any I/O. Lower priority devices it supports are held for a moment, in case this driver reports
    // the same controller, so it doesn't appear twice.
    bool (*Supports)(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name);

    // Optional: called on the app thread holding the joystick lock, just before a device is published. It can
    // change the device's name and GUID, e.g. to the ones another driver uses.
    void (*DevicePublishing)(SDL_JoystickDevice *device);

    // Called on the app thread holding the joystick lock, after a device is published and before it's unpublished
    void (*DevicePublished)(SDL_JoystickDevice *device, bool published);

    // Free a device's driver_data, which may happen on any thread, and after the driver's Quit()
    void (*FreeDeviceData)(void *driver_data);

    // Set the player index of a published device
    void (*SetPlayerIndex)(SDL_JoystickDevice *device, int player_index);

    // Get the autodetected controller mapping; returns false if there isn't any
    bool (*GetGamepadMapping)(SDL_JoystickDevice *device, SDL_GamepadMapping *out);

    /* Open a joystick for use. This should fill the nbuttons and naxes fields of the joystick structure.
       Returns false on error.
     */
    bool (*Open)(SDL_Joystick *joystick, SDL_JoystickDevice *device);

    // Optional rumble functionality
    bool (*Rumble)(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble);
    bool (*RumbleTriggers)(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble);

    // Optional LED functionality
    bool (*SetLED)(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue);

    // Optional general effects
    bool (*SendEffect)(SDL_Joystick *joystick, const void *data, int size);

    // Optional sensor functionality
    bool (*SetSensorsEnabled)(SDL_Joystick *joystick, bool enabled);

    /* Function to update the state of a joystick - called as a device poll.
     * This function shouldn't update the joystick structure directly,
     * but instead should call SDL_PrivateJoystick*() to deliver events
     * and update joystick device state.
     */
    void (*Update)(SDL_Joystick *joystick);

    // Function to close a joystick after use
    void (*Close)(SDL_Joystick *joystick);

    // Function to perform any system-specific joystick related cleanup
    void (*Quit)(void);
} SDL_JoystickDriver;

// Windows and Mac OSX has a limit of MAX_DWORD / 1000, Linux kernel has a limit of 0xFFFF
#define SDL_MAX_RUMBLE_DURATION_MS 0xFFFF

/* Dualshock4 only rumbles for about 5 seconds max, resend rumble command every 2 seconds
 * to make long rumble work. */
#define SDL_RUMBLE_RESEND_MS 2000

#define SDL_LED_MIN_REPEAT_MS 5000

// The available joystick drivers
extern SDL_JoystickDriver SDL_PRIVATE_JoystickDriver;
extern SDL_JoystickDriver SDL_ANDROID_JoystickDriver;
extern SDL_JoystickDriver SDL_BSD_JoystickDriver;
extern SDL_JoystickDriver SDL_DARWIN_JoystickDriver;
extern SDL_JoystickDriver SDL_DUMMY_JoystickDriver;
extern SDL_JoystickDriver SDL_EMSCRIPTEN_JoystickDriver;
extern SDL_JoystickDriver SDL_HAIKU_JoystickDriver;
extern SDL_JoystickDriver SDL_HIDAPI_JoystickDriver;
extern SDL_JoystickDriver SDL_IOS_JoystickDriver;
extern SDL_JoystickDriver SDL_LINUX_JoystickDriver;
extern SDL_JoystickDriver SDL_VIRTUAL_JoystickDriver;
extern SDL_JoystickDriver SDL_XINPUT_JoystickDriver;
extern SDL_JoystickDriver SDL_DINPUT_JoystickDriver;
extern SDL_JoystickDriver SDL_PS2_JoystickDriver;
extern SDL_JoystickDriver SDL_PSP_JoystickDriver;
extern SDL_JoystickDriver SDL_VITA_JoystickDriver;
extern SDL_JoystickDriver SDL_N3DS_JoystickDriver;
extern SDL_JoystickDriver SDL_GAMEINPUT_JoystickDriver;
extern SDL_JoystickDriver SDL_DOS_JoystickDriver;

// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif

#endif // SDL_sysjoystick_h_
