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

#ifndef SDL_joystick_c_h_
#define SDL_joystick_c_h_

#include "SDL_internal.h"

// Useful functions and variables from SDL_joystick.c

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

struct SDL_JoystickDriver;
struct SDL_JoystickMatchInfo;
struct SDL_SteamVirtualGamepadInfo;

// Initialization and shutdown functions
extern bool SDL_InitJoysticks(void);
extern void SDL_QuitJoysticks(void);

// Return whether the joystick system is currently initialized
extern bool SDL_JoysticksInitialized(void);

// Return whether the joystick system is shutting down
extern bool SDL_JoysticksQuitting(void);

// Return whether the joysticks are currently locked
extern bool SDL_JoysticksLocked(void);

// Make sure we currently have the joysticks locked
extern void SDL_AssertJoysticksLocked(void) SDL_ASSERT_CAPABILITY(SDL_event_lock);

// Function to return whether there are any joysticks opened by the application
extern bool SDL_JoysticksOpened(void);

/* Function to standardize the name for a controller
   This should be freed with SDL_free() when no longer needed
 */
extern char *SDL_CreateJoystickName(Uint16 vendor, Uint16 product, const char *vendor_name, const char *product_name);

// Function to create a GUID for a joystick based on the VID/PID and name
extern SDL_GUID SDL_CreateJoystickGUID(Uint16 bus, Uint16 vendor, Uint16 product, Uint16 version, const char *vendor_name, const char *product_name, Uint8 driver_signature, Uint8 driver_data);

// Function to create a GUID for a joystick based on the name, with no VID/PID information
extern SDL_GUID SDL_CreateJoystickGUIDForName(const char *name);

// Function to set the vendor field of a joystick GUID
extern void SDL_SetJoystickGUIDVendor(SDL_GUID *guid, Uint16 vendor);

// Function to set the product field of a joystick GUID
extern void SDL_SetJoystickGUIDProduct(SDL_GUID *guid, Uint16 product);

// Function to set the version field of a joystick GUID
extern void SDL_SetJoystickGUIDVersion(SDL_GUID *guid, Uint16 version);

// Function to set the CRC field of a joystick GUID
extern void SDL_SetJoystickGUIDCRC(SDL_GUID *guid, Uint16 crc);

// Function to get the CRC of the names in a joystick GUID
extern Uint16 SDL_GetJoystickNameCRC(const char *vendor_name, const char *product_name);

// Function to return the type of a controller
extern SDL_GamepadType SDL_GetGamepadTypeFromVIDPID(Uint16 vendor, Uint16 product, const char *name, bool forUI);
extern SDL_GamepadType SDL_GetGamepadTypeFromGUID(SDL_GUID guid, const char *name);

// Function to return whether a joystick GUID uses the version field
extern bool SDL_JoystickGUIDUsesVersion(SDL_GUID guid);

// Function to return whether a joystick is an Xbox One controller
extern bool SDL_IsJoystickXboxOne(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an Xbox One Elite controller
extern bool SDL_IsJoystickXboxOneElite(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an Xbox Series X controller
extern bool SDL_IsJoystickXboxSeriesX(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an Xbox One controller connected via Bluetooth
extern bool SDL_IsJoystickBluetoothXboxOne(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a PS4 controller
extern bool SDL_IsJoystickPS4(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a PS5 controller
extern bool SDL_IsJoystickPS5(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickDualSenseEdge(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Nintendo Switch Pro controller
extern bool SDL_IsJoystickNintendoSwitchPro(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitch2Pro(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchProInputOnly(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitch2ProInputOnly(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyCon(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConLeft(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConRight(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConGrip(Uint16 vendor_id, Uint16 product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConPair(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Nintendo GameCube style controller
extern bool SDL_IsJoystickGameCube(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an Amazon Luna controller
extern bool SDL_IsJoystickAmazonLunaController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Google Stadia controller
extern bool SDL_IsJoystickGoogleStadiaController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an NVIDIA SHIELD controller
extern bool SDL_IsJoystickNVIDIASHIELDController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Steam Virtual Gamepad
extern bool SDL_IsJoystickSteamVirtualGamepad(Uint16 vendor_id, Uint16 product_id, Uint16 version);

// Function to return whether a joystick is a Steam Controller
extern bool SDL_IsJoystickSteamController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a HORI Steam controller
extern bool SDL_IsJoystickHoriSteamController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is an SInput (Open Format) controller
extern bool SDL_IsJoystickSInputController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Flydigi controller
extern bool SDL_IsJoystickFlydigiController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a GameSir controller
extern bool SDL_IsJoystickGameSirController(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Steam Deck
extern bool SDL_IsJoystickSteamDeck(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick is a Steam Triton
extern bool SDL_IsJoystickSteamTriton(Uint16 vendor_id, Uint16 product_id);

// Function to return whether a joystick guid comes from the XInput driver
extern bool SDL_IsJoystickXInput(SDL_GUID guid);

// Function to return whether a joystick guid comes from the GameInput driver
extern bool SDL_IsJoystickGameInput(SDL_GUID guid);

// Function to return whether a joystick guid comes from the HIDAPI driver
extern bool SDL_IsJoystickHIDAPI(SDL_GUID guid);

// Function to return whether a joystick guid comes from the MFI driver
extern bool SDL_IsJoystickMFI(SDL_GUID guid);

// Function to return whether a joystick guid comes from the Virtual driver
extern bool SDL_IsJoystickVIRTUAL(SDL_GUID guid);

// Function to return whether a joystick is a wheel
extern bool SDL_IsJoystickWheel(Uint16 vendor_id, Uint16 product_id, Uint16 crc);

// Function to return whether a joystick should be ignored
extern bool SDL_ShouldIgnoreJoystick(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name);

// Internal event queueing functions
extern void SDL_PrivateJoystickAddTouchpad(SDL_Joystick *joystick, int nfingers);
extern void SDL_PrivateJoystickAddSensor(SDL_Joystick *joystick, SDL_SensorType type, float rate);
extern void SDL_PrivateJoystickSensorRate(SDL_Joystick *joystick, SDL_SensorType type, float rate);
extern void SDL_PrivateJoystickAddCapSense(SDL_Joystick *joystick, SDL_GamepadCapSenseType type);
extern bool SDL_IsJoystickBeingAdded(void);

// Joystick devices

// Whether a device's driver can handle the controller
typedef enum SDL_JoystickUsable
{
    SDL_JOYSTICK_USABLE_YES,
    SDL_JOYSTICK_USABLE_UNKNOWN, // The driver is still finding out, so hold lower priority devices for a while
    SDL_JOYSTICK_USABLE_NO       // Lower priority drivers handle the controller
} SDL_JoystickUsable;

// Create a joystick device with a new instance ID. Callers should set its description, then report it.
extern struct SDL_JoystickDevice *SDL_CreateJoystickDevice(struct SDL_JoystickDriver *driver);

// Set the description of a device that hasn't been reported yet
extern void SDL_SetJoystickDeviceDescription(struct SDL_JoystickDevice *device, const char *name, const char *path, SDL_GUID guid, int steam_virtual_gamepad_slot);

// Add a controller that a device is the same as, matched by VID/PID or by gamepad type
extern void SDL_AddJoystickMatchVIDPID(struct SDL_JoystickMatchInfo *info, Uint16 vendor_id, Uint16 product_id);
extern void SDL_AddJoystickMatchType(struct SDL_JoystickMatchInfo *info, SDL_GamepadType type);

// Queue the device to be published, takes ownership of 'device'
extern void SDL_ReportJoystickDevice(struct SDL_JoystickDevice *device);

// Queue a description change for a reported device
extern void SDL_UpdateJoystickDeviceDescription(SDL_JoystickID driver_instance_id, const char *name, const char *path, SDL_GUID guid, int steam_virtual_gamepad_slot);

// Queue the removal of a reported device
extern void SDL_ReportJoystickRemoved(SDL_JoystickID driver_instance_id);

// Queue a message for the driver's HandleMessage(), called in order with the other changes
extern void SDL_PostJoystickDriverMessage(struct SDL_JoystickDriver *driver, void *message);

// Return the attached open joystick for a driver instance ID, or NULL.
extern SDL_Joystick *SDL_GetJoystickFromDriverID(SDL_JoystickID driver_instance_id);

// Report a claim-only device, which hides lower priority drivers' devices matching it. Remove it with
// SDL_ReportJoystickRemoved(). Returns the device's driver instance ID, or 0 on failure.
extern SDL_JoystickID SDL_ReportJoystickClaim(struct SDL_JoystickDriver *driver, Uint16 vendor_id, Uint16 product_id, SDL_GamepadType type, SDL_JoystickUsable usable, const SDL_GUID *container_id);

// Report a device with usable UNKNOWN that hides matching devices that aren't published yet for longer than
// usual, e.g. while the user is asked for permission to open the controller.
extern SDL_JoystickID SDL_ReportJoystickLongClaim(struct SDL_JoystickDriver *driver, Uint16 vendor_id, Uint16 product_id, SDL_GamepadType type, Uint32 hold_ms);

// Change whether a device's driver can handle it.
extern void SDL_SetJoystickDeviceUsable(SDL_JoystickID driver_instance_id, SDL_JoystickUsable usable);

// Have the joystick device thread run Discover() again.
extern void SDL_WakeJoystickDeviceThread(void);

// Return true if Discover() should stop early because joysticks are shutting down
extern bool SDL_JoystickDiscoveryQuitting(void);

extern void SDL_PrivateJoystickForceRecentering(SDL_Joystick *joystick);
extern void SDL_SendJoystickAxis(Uint64 timestamp, SDL_Joystick *joystick, Uint8 axis, Sint16 value);
extern void SDL_SendJoystickBall(Uint64 timestamp, SDL_Joystick *joystick, Uint8 ball, Sint16 xrel, Sint16 yrel);
extern void SDL_SendJoystickHat(Uint64 timestamp, SDL_Joystick *joystick, Uint8 hat, Uint8 value);
extern void SDL_SendJoystickButton(Uint64 timestamp, SDL_Joystick *joystick, Uint8 button, bool down);
extern void SDL_SendJoystickTouchpad(Uint64 timestamp, SDL_Joystick *joystick, int touchpad, int finger, bool down, float x, float y, float pressure);
extern void SDL_SendJoystickSensor(Uint64 timestamp, SDL_Joystick *joystick, SDL_SensorType type, Uint64 sensor_timestamp, const float *data, int num_values);
extern void SDL_SendJoystickCapSense(Uint64 timestamp, SDL_Joystick *joystick, SDL_GamepadCapSenseType type, bool down);
extern void SDL_SendJoystickPowerInfo(SDL_Joystick *joystick, SDL_PowerState state, int percent);

// Function to get the Steam virtual gamepad info for a joystick
extern const struct SDL_SteamVirtualGamepadInfo *SDL_GetJoystickVirtualGamepadInfoForID(SDL_JoystickID instance_id);

// Internal sanity checking functions
extern bool SDL_IsJoystickValid(SDL_Joystick *joystick);

typedef enum
{
    EMappingKind_None,
    EMappingKind_Button,
    EMappingKind_Axis,
    EMappingKind_Hat,
} EMappingKind;

typedef struct SDL_InputMapping
{
    EMappingKind kind;
    Uint8 target;
    bool axis_reversed;
    bool half_axis_positive;
    bool half_axis_negative;
} SDL_InputMapping;

typedef struct SDL_GamepadMapping
{
    SDL_InputMapping a;
    SDL_InputMapping b;
    SDL_InputMapping x;
    SDL_InputMapping y;
    SDL_InputMapping back;
    SDL_InputMapping guide;
    SDL_InputMapping start;
    SDL_InputMapping leftstick;
    SDL_InputMapping rightstick;
    SDL_InputMapping leftshoulder;
    SDL_InputMapping rightshoulder;
    SDL_InputMapping dpup;
    SDL_InputMapping dpdown;
    SDL_InputMapping dpleft;
    SDL_InputMapping dpright;
    SDL_InputMapping misc1;
    SDL_InputMapping misc2;
    SDL_InputMapping misc3;
    SDL_InputMapping misc4;
    SDL_InputMapping misc5;
    SDL_InputMapping misc6;
    SDL_InputMapping right_paddle1;
    SDL_InputMapping left_paddle1;
    SDL_InputMapping right_paddle2;
    SDL_InputMapping left_paddle2;
    SDL_InputMapping leftx;
    SDL_InputMapping lefty;
    SDL_InputMapping rightx;
    SDL_InputMapping righty;
    SDL_InputMapping lefttrigger;
    SDL_InputMapping righttrigger;
    SDL_InputMapping touchpad;
} SDL_GamepadMapping;

// Function to get autodetected gamepad controller mapping from the driver
extern bool SDL_PrivateJoystickGetAutoGamepadMapping(SDL_JoystickID instance_id,
                                                         SDL_GamepadMapping *out);


typedef struct
{
    const char *included_hint_name;
    int num_included_entries;
    int max_included_entries;
    Uint32 *included_entries;

    const char *excluded_hint_name;
    int num_excluded_entries;
    int max_excluded_entries;
    Uint32 *excluded_entries;

    int num_initial_entries;
    Uint32 *initial_entries;

    bool initialized;
} SDL_vidpid_list;

extern void SDL_LoadVIDPIDList(SDL_vidpid_list *list);
extern void SDL_LoadVIDPIDListFromHints(SDL_vidpid_list *list, const char *included_list, const char *excluded_list);
extern bool SDL_VIDPIDInList(Uint16 vendor_id, Uint16 product_id, const SDL_vidpid_list *list);
extern void SDL_FreeVIDPIDList(SDL_vidpid_list *list);

// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif

#endif // SDL_joystick_c_h_
