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

/* Useful functions and variables from SDL_joystick.c */

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

struct SDL_JoystickDriver;
struct SDL_SteamVirtualGamepadInfo;
extern char SDL_joystick_magic;

/* Initialization and shutdown functions */
extern int SDL_InitJoysticks(void);
extern void SDL_QuitJoysticks(void);

/* Return whether the joystick system is currently initialized */
extern SDL_bool SDL_JoysticksInitialized(void);

/* Return whether the joystick system is shutting down */
extern SDL_bool SDL_JoysticksQuitting(void);

/* Return whether the joysticks are currently locked */
extern SDL_bool SDL_JoysticksLocked(void);

/* Make sure we currently have the joysticks locked */
extern void SDL_AssertJoysticksLocked(void) SDL_ASSERT_CAPABILITY(SDL_joystick_lock);

/* Function to return whether there are any joysticks opened by the application */
extern SDL_bool SDL_JoysticksOpened(void);

/* Function to standardize the name for a controller
   This should be freed with SDL_free() when no longer needed
 */
extern char *SDL_CreateJoystickName(Uint16 vendor, Uint16 product, const char *vendor_name, const char *product_name);

/* Function to create a GUID for a joystick based on the VID/PID and name */
extern SDL_JoystickGUID SDL_CreateJoystickGUID(Uint16 bus, Uint16 vendor, Uint16 product, Uint16 version, const char *vendor_name, const char *product_name, Uint8 driver_signature, Uint8 driver_data);

/* Function to create a GUID for a joystick based on the name, with no VID/PID information */
extern SDL_JoystickGUID SDL_CreateJoystickGUIDForName(const char *name);

/* Function to set the vendor field of a joystick GUID */
extern void SDL_SetJoystickGUIDVendor(SDL_JoystickGUID *guid, Uint16 vendor);

/* Function to set the product field of a joystick GUID */
extern void SDL_SetJoystickGUIDProduct(SDL_JoystickGUID *guid, Uint16 product);

/* Function to set the version field of a joystick GUID */
extern void SDL_SetJoystickGUIDVersion(SDL_JoystickGUID *guid, Uint16 version);

/* Function to set the CRC field of a joystick GUID */
extern void SDL_SetJoystickGUIDCRC(SDL_JoystickGUID *guid, Uint16 crc);

/* Function to return the type of a controller */
extern SDL_GamepadType SDL_GetGamepadTypeFromVIDPID(Uint16 vendor, Uint16 product, const char *name, SDL_bool forUI);
extern SDL_GamepadType SDL_GetGamepadTypeFromGUID(SDL_JoystickGUID guid, const char *name);

/* Function to return whether a joystick GUID uses the version field */
extern SDL_bool SDL_JoystickGUIDUsesVersion(SDL_JoystickGUID guid);

/* Function to return whether a joystick is an Xbox One controller */
extern SDL_bool SDL_IsJoystickXboxOne(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Xbox One Elite controller */
extern SDL_bool SDL_IsJoystickXboxOneElite(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Xbox Series X controller */
extern SDL_bool SDL_IsJoystickXboxSeriesX(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Xbox One controller connected via Bluetooth */
extern SDL_bool SDL_IsJoystickBluetoothXboxOne(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a PS4 controller */
extern SDL_bool SDL_IsJoystickPS4(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a PS5 controller */
extern SDL_bool SDL_IsJoystickPS5(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickDualSenseEdge(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Nintendo Switch Pro controller */
extern SDL_bool SDL_IsJoystickNintendoSwitchPro(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchProInputOnly(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchJoyCon(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchJoyConLeft(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchJoyConRight(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchJoyConGrip(Uint16 vendor_id, Uint16 product_id);
extern SDL_bool SDL_IsJoystickNintendoSwitchJoyConPair(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Nintendo GameCube style controller */
extern SDL_bool SDL_IsJoystickGameCube(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an Amazon Luna controller */
extern SDL_bool SDL_IsJoystickAmazonLunaController(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Google Stadia controller */
extern SDL_bool SDL_IsJoystickGoogleStadiaController(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is an NVIDIA SHIELD controller */
extern SDL_bool SDL_IsJoystickNVIDIASHIELDController(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Steam Controller */
extern SDL_bool SDL_IsJoystickSteamController(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick is a Steam Deck */
extern SDL_bool SDL_IsJoystickSteamDeck(Uint16 vendor_id, Uint16 product_id);

/* Function to return whether a joystick guid comes from the XInput driver */
extern SDL_bool SDL_IsJoystickXInput(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the WGI driver */
extern SDL_bool SDL_IsJoystickWGI(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the HIDAPI driver */
extern SDL_bool SDL_IsJoystickHIDAPI(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the MFI driver */
extern SDL_bool SDL_IsJoystickMFI(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the RAWINPUT driver */
extern SDL_bool SDL_IsJoystickRAWINPUT(SDL_JoystickGUID guid);

/* Function to return whether a joystick guid comes from the Virtual driver */
extern SDL_bool SDL_IsJoystickVIRTUAL(SDL_JoystickGUID guid);

/* Function to return whether a joystick should be ignored */
extern SDL_bool SDL_ShouldIgnoreJoystick(const char *name, SDL_JoystickGUID guid);

/* Internal event queueing functions */
extern void SDL_PrivateJoystickAddTouchpad(SDL_Joystick *joystick, int nfingers);
extern void SDL_PrivateJoystickAddSensor(SDL_Joystick *joystick, SDL_SensorType type, float rate);
extern void SDL_PrivateJoystickAdded(SDL_JoystickID instance_id);
extern SDL_bool SDL_IsJoystickBeingAdded(void);
extern void SDL_PrivateJoystickRemoved(SDL_JoystickID instance_id);
extern void SDL_PrivateJoystickForceRecentering(SDL_Joystick *joystick);
extern int SDL_SendJoystickAxis(Uint64 timestamp, SDL_Joystick *joystick,
                                   Uint8 axis, Sint16 value);
extern int SDL_SendJoystickHat(Uint64 timestamp, SDL_Joystick *joystick,
                                  Uint8 hat, Uint8 value);
extern int SDL_SendJoystickButton(Uint64 timestamp, SDL_Joystick *joystick,
                                     Uint8 button, Uint8 state);
extern int SDL_SendJoystickTouchpad(Uint64 timestamp, SDL_Joystick *joystick,
                                       int touchpad, int finger, Uint8 state, float x, float y, float pressure);
extern int SDL_SendJoystickSensor(Uint64 timestamp, SDL_Joystick *joystick,
                                     SDL_SensorType type, Uint64 sensor_timestamp, const float *data, int num_values);
extern void SDL_SendJoystickBatteryLevel(SDL_Joystick *joystick,
                                            SDL_JoystickPowerLevel ePowerLevel);

/* Function to get the Steam virtual gamepad info for a joystick */
extern const struct SDL_SteamVirtualGamepadInfo *SDL_GetJoystickInstanceVirtualGamepadInfo(SDL_JoystickID instance_id);

/* Internal sanity checking functions */
extern SDL_bool SDL_IsJoystickValid(SDL_Joystick *joystick);

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
    SDL_bool axis_reversed;
    SDL_bool half_axis_positive;
    SDL_bool half_axis_negative;
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

/* Function to get autodetected gamepad controller mapping from the driver */
extern SDL_bool SDL_PrivateJoystickGetAutoGamepadMapping(SDL_JoystickID instance_id,
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

    SDL_bool initialized;
} SDL_vidpid_list;

extern void SDL_LoadVIDPIDList(SDL_vidpid_list *list);
extern void SDL_LoadVIDPIDListFromHints(SDL_vidpid_list *list, const char *included_list, const char *excluded_list);
extern SDL_bool SDL_VIDPIDInList(Uint16 vendor_id, Uint16 product_id, const SDL_vidpid_list *list);
extern void SDL_FreeVIDPIDList(SDL_vidpid_list *list);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_joystick_c_h_ */
