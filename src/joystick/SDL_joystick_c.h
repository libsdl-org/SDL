/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
extern void SDL_AssertJoysticksLocked(void) SDL_ASSERT_CAPABILITY(SDL_joystick_lock);

// Function to return whether there are any joysticks opened by the application
extern bool SDL_JoysticksOpened(void);

// Function to determine whether a device is currently detected by this driver
extern bool SDL_JoystickHandledByAnotherDriver(struct SDL_JoystickDriver *driver, uint16_t vendor_id, uint16_t product_id, uint16_t version, const char *name);

/* Function to standardize the name for a controller
   This should be freed with SDL_free() when no longer needed
 */
extern char *SDL_CreateJoystickName(uint16_t vendor, uint16_t product, const char *vendor_name, const char *product_name);

// Function to create a GUID for a joystick based on the VID/PID and name
extern SDL_GUID SDL_CreateJoystickGUID(uint16_t bus, uint16_t vendor, uint16_t product, uint16_t version, const char *vendor_name, const char *product_name, uint8_t driver_signature, uint8_t driver_data);

// Function to create a GUID for a joystick based on the name, with no VID/PID information
extern SDL_GUID SDL_CreateJoystickGUIDForName(const char *name);

// Function to set the vendor field of a joystick GUID
extern void SDL_SetJoystickGUIDVendor(SDL_GUID *guid, uint16_t vendor);

// Function to set the product field of a joystick GUID
extern void SDL_SetJoystickGUIDProduct(SDL_GUID *guid, uint16_t product);

// Function to set the version field of a joystick GUID
extern void SDL_SetJoystickGUIDVersion(SDL_GUID *guid, uint16_t version);

// Function to set the CRC field of a joystick GUID
extern void SDL_SetJoystickGUIDCRC(SDL_GUID *guid, uint16_t crc);

// Function to return the type of a controller
extern SDL_GamepadType SDL_GetGamepadTypeFromVIDPID(uint16_t vendor, uint16_t product, const char *name, bool forUI);
extern SDL_GamepadType SDL_GetGamepadTypeFromGUID(SDL_GUID guid, const char *name);

// Function to return whether a joystick GUID uses the version field
extern bool SDL_JoystickGUIDUsesVersion(SDL_GUID guid);

// Function to return whether a joystick is an Xbox One controller
extern bool SDL_IsJoystickXboxOne(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is an Xbox One Elite controller
extern bool SDL_IsJoystickXboxOneElite(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is an Xbox Series X controller
extern bool SDL_IsJoystickXboxSeriesX(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is an Xbox One controller connected via Bluetooth
extern bool SDL_IsJoystickBluetoothXboxOne(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a PS4 controller
extern bool SDL_IsJoystickPS4(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a PS5 controller
extern bool SDL_IsJoystickPS5(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickDualSenseEdge(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a Nintendo Switch Pro controller
extern bool SDL_IsJoystickNintendoSwitchPro(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchProInputOnly(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyCon(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConLeft(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConRight(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConGrip(uint16_t vendor_id, uint16_t product_id);
extern bool SDL_IsJoystickNintendoSwitchJoyConPair(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a Nintendo GameCube style controller
extern bool SDL_IsJoystickGameCube(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is an Amazon Luna controller
extern bool SDL_IsJoystickAmazonLunaController(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a Google Stadia controller
extern bool SDL_IsJoystickGoogleStadiaController(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is an NVIDIA SHIELD controller
extern bool SDL_IsJoystickNVIDIASHIELDController(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a Steam Controller
extern bool SDL_IsJoystickSteamController(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick is a Steam Deck
extern bool SDL_IsJoystickSteamDeck(uint16_t vendor_id, uint16_t product_id);

// Function to return whether a joystick guid comes from the XInput driver
extern bool SDL_IsJoystickXInput(SDL_GUID guid);

// Function to return whether a joystick guid comes from the WGI driver
extern bool SDL_IsJoystickWGI(SDL_GUID guid);

// Function to return whether a joystick guid comes from the HIDAPI driver
extern bool SDL_IsJoystickHIDAPI(SDL_GUID guid);

// Function to return whether a joystick guid comes from the MFI driver
extern bool SDL_IsJoystickMFI(SDL_GUID guid);

// Function to return whether a joystick guid comes from the RAWINPUT driver
extern bool SDL_IsJoystickRAWINPUT(SDL_GUID guid);

// Function to return whether a joystick guid comes from the Virtual driver
extern bool SDL_IsJoystickVIRTUAL(SDL_GUID guid);

// Function to return whether a joystick should be ignored
extern bool SDL_ShouldIgnoreJoystick(const char *name, SDL_GUID guid);

// Internal event queueing functions
extern void SDL_PrivateJoystickAddTouchpad(SDL_Joystick *joystick, int nfingers);
extern void SDL_PrivateJoystickAddSensor(SDL_Joystick *joystick, SDL_SensorType type, float rate);
extern void SDL_PrivateJoystickSensorRate(SDL_Joystick *joystick, SDL_SensorType type, float rate);
extern void SDL_PrivateJoystickAdded(SDL_JoystickID instance_id);
extern bool SDL_IsJoystickBeingAdded(void);
extern void SDL_PrivateJoystickRemoved(SDL_JoystickID instance_id);
extern void SDL_PrivateJoystickForceRecentering(SDL_Joystick *joystick);
extern void SDL_SendJoystickAxis(uint64_t timestamp, SDL_Joystick *joystick, uint8_t axis, int16_t value);
extern void SDL_SendJoystickBall(uint64_t timestamp, SDL_Joystick *joystick, uint8_t ball, int16_t xrel, int16_t yrel);
extern void SDL_SendJoystickHat(uint64_t timestamp, SDL_Joystick *joystick, uint8_t hat, uint8_t value);
extern void SDL_SendJoystickButton(uint64_t timestamp, SDL_Joystick *joystick, uint8_t button, bool down);
extern void SDL_SendJoystickTouchpad(uint64_t timestamp, SDL_Joystick *joystick, int touchpad, int finger, bool down, float x, float y, float pressure);
extern void SDL_SendJoystickSensor(uint64_t timestamp, SDL_Joystick *joystick, SDL_SensorType type, uint64_t sensor_timestamp, const float *data, int num_values);
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
    uint8_t target;
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
    uint32_t *included_entries;

    const char *excluded_hint_name;
    int num_excluded_entries;
    int max_excluded_entries;
    uint32_t *excluded_entries;

    int num_initial_entries;
    uint32_t *initial_entries;

    bool initialized;
} SDL_vidpid_list;

extern void SDL_LoadVIDPIDList(SDL_vidpid_list *list);
extern void SDL_LoadVIDPIDListFromHints(SDL_vidpid_list *list, const char *included_list, const char *excluded_list);
extern bool SDL_VIDPIDInList(uint16_t vendor_id, uint16_t product_id, const SDL_vidpid_list *list);
extern void SDL_FreeVIDPIDList(SDL_vidpid_list *list);

// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif

#endif // SDL_joystick_c_h_
