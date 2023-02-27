/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_gamepad.h
 *
 *  \brief Include file for SDL gamepad event handling
 */

#ifndef SDL_gamepad_h_
#define SDL_gamepad_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_rwops.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_joystick.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \file SDL_gamepad.h
 *
 *  In order to use these functions, SDL_Init() must have been called
 *  with the ::SDL_INIT_GAMEPAD flag.  This causes SDL to scan the system
 *  for gamepads, and load appropriate drivers.
 *
 *  If you would like to receive gamepad updates while the application
 *  is in the background, you should set the following hint before calling
 *  SDL_Init(): SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS
 */

/**
 * The structure used to identify an SDL gamepad
 */
struct SDL_Gamepad;
typedef struct SDL_Gamepad SDL_Gamepad;

typedef enum
{
    SDL_GAMEPAD_TYPE_UNKNOWN = 0,
    SDL_GAMEPAD_TYPE_VIRTUAL,
    SDL_GAMEPAD_TYPE_XBOX360,
    SDL_GAMEPAD_TYPE_XBOXONE,
    SDL_GAMEPAD_TYPE_PS3,
    SDL_GAMEPAD_TYPE_PS4,
    SDL_GAMEPAD_TYPE_PS5,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT,
    SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR,
    SDL_GAMEPAD_TYPE_AMAZON_LUNA,
    SDL_GAMEPAD_TYPE_GOOGLE_STADIA,
    SDL_GAMEPAD_TYPE_NVIDIA_SHIELD
} SDL_GamepadType;

/**
 *  The list of buttons available on a gamepad
 */
typedef enum
{
    SDL_GAMEPAD_BUTTON_INVALID = -1,
    SDL_GAMEPAD_BUTTON_A,
    SDL_GAMEPAD_BUTTON_B,
    SDL_GAMEPAD_BUTTON_X,
    SDL_GAMEPAD_BUTTON_Y,
    SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_MISC1,    /* Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button */
    SDL_GAMEPAD_BUTTON_PADDLE1,  /* Xbox Elite paddle P1 (upper left, facing the back) */
    SDL_GAMEPAD_BUTTON_PADDLE2,  /* Xbox Elite paddle P3 (upper right, facing the back) */
    SDL_GAMEPAD_BUTTON_PADDLE3,  /* Xbox Elite paddle P2 (lower left, facing the back) */
    SDL_GAMEPAD_BUTTON_PADDLE4,  /* Xbox Elite paddle P4 (lower right, facing the back) */
    SDL_GAMEPAD_BUTTON_TOUCHPAD, /* PS4/PS5 touchpad button */
    SDL_GAMEPAD_BUTTON_MAX
} SDL_GamepadButton;

/**
 *  The list of axes available on a gamepad
 *
 *  Thumbstick axis values range from SDL_JOYSTICK_AXIS_MIN to SDL_JOYSTICK_AXIS_MAX,
 *  and are centered within ~8000 of zero, though advanced UI will allow users to set
 *  or autodetect the dead zone, which varies between gamepads.
 *
 *  Trigger axis values range from 0 to SDL_JOYSTICK_AXIS_MAX.
 */
typedef enum
{
    SDL_GAMEPAD_AXIS_INVALID = -1,
    SDL_GAMEPAD_AXIS_LEFTX,
    SDL_GAMEPAD_AXIS_LEFTY,
    SDL_GAMEPAD_AXIS_RIGHTX,
    SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    SDL_GAMEPAD_AXIS_MAX
} SDL_GamepadAxis;

typedef enum
{
    SDL_GAMEPAD_BINDTYPE_NONE = 0,
    SDL_GAMEPAD_BINDTYPE_BUTTON,
    SDL_GAMEPAD_BINDTYPE_AXIS,
    SDL_GAMEPAD_BINDTYPE_HAT
} SDL_GamepadBindingType;

/**
 *  Get the SDL joystick layer binding for this gamepad button/axis mapping
 */
typedef struct SDL_GamepadBinding
{
    SDL_GamepadBindingType bindType;
    union
    {
        int button;
        int axis;
        struct {
            int hat;
            int hat_mask;
        } hat;
    } value;

} SDL_GamepadBinding;


/**
 * Add support for gamepads that SDL is unaware of or change the binding of an
 * existing gamepad.
 *
 * The mapping string has the format "GUID,name,mapping", where GUID is the
 * string value from SDL_GetJoystickGUIDString(), name is the human readable
 * string for the device and mappings are gamepad mappings to joystick ones.
 * Under Windows there is a reserved GUID of "xinput" that covers all XInput
 * devices. The mapping format for joystick is:
 *
 * - `bX`: a joystick button, index X
 * - `hX.Y`: hat X with value Y
 * - `aX`: axis X of the joystick
 *
 * Buttons can be used as a gamepad axes and vice versa.
 *
 * This string shows an example of a valid mapping for a gamepad:
 *
 * ```c
 * "341a3608000000000000504944564944,Afterglow PS3 Controller,a:b1,b:b2,y:b3,x:b0,start:b9,guide:b12,back:b8,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftshoulder:b4,rightshoulder:b5,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b6,righttrigger:b7"
 * ```
 *
 * \param mappingString the mapping string
 * \returns 1 if a new mapping is added, 0 if an existing mapping is updated,
 *          -1 on error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadMapping
 * \sa SDL_GetGamepadMappingForGUID
 */
extern DECLSPEC int SDLCALL SDL_AddGamepadMapping(const char *mappingString);

/**
 * Load a set of gamepad mappings from a seekable SDL data stream.
 *
 * You can call this function several times, if needed, to load different
 * database files.
 *
 * If a new mapping is loaded for an already known gamepad GUID, the later
 * version will overwrite the one currently loaded.
 *
 * Mappings not belonging to the current platform or with no platform field
 * specified will be ignored (i.e. mappings for Linux will be ignored in
 * Windows, etc).
 *
 * This function will load the text database entirely in memory before
 * processing it, so take this into consideration if you are in a memory
 * constrained environment.
 *
 * \param rw the data stream for the mappings to be added
 * \param freerw non-zero to close the stream after being read
 * \returns the number of mappings added or -1 on error; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AddGamepadMapping
 * \sa SDL_AddGamepadMappingsFromFile
 * \sa SDL_GetGamepadMappingForGUID
 */
extern DECLSPEC int SDLCALL SDL_AddGamepadMappingsFromRW(SDL_RWops *rw, int freerw);

/**
 *  Load a set of mappings from a file, filtered by the current SDL_GetPlatform()
 *
 *  Convenience macro.
 */
#define SDL_AddGamepadMappingsFromFile(file)   SDL_AddGamepadMappingsFromRW(SDL_RWFromFile(file, "rb"), 1)

/**
 * Get the number of mappings installed.
 *
 * \returns the number of mappings.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetNumGamepadMappings(void);

/**
 * Get the mapping at a particular index.
 *
 * \param mapping_index mapping index
 * \returns the mapping string. Must be freed with SDL_free(). Returns NULL if
 *          the index is out of range.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC char * SDLCALL SDL_GetGamepadMappingForIndex(int mapping_index);

/**
 * Get the gamepad mapping string for a given GUID.
 *
 * The returned string must be freed with SDL_free().
 *
 * \param guid a structure containing the GUID for which a mapping is desired
 * \returns a mapping string or NULL on error; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetJoystickInstanceGUID
 * \sa SDL_GetJoystickGUID
 */
extern DECLSPEC char * SDLCALL SDL_GetGamepadMappingForGUID(SDL_JoystickGUID guid);

/**
 * Get the current mapping of a gamepad.
 *
 * The returned string must be freed with SDL_free().
 *
 * Details about mappings are discussed with SDL_AddGamepadMapping().
 *
 * \param gamepad the gamepad you want to get the current mapping for
 * \returns a string that has the gamepad's mapping or NULL if no mapping is
 *          available; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AddGamepadMapping
 * \sa SDL_GetGamepadMappingForGUID
 */
extern DECLSPEC char * SDLCALL SDL_GetGamepadMapping(SDL_Gamepad *gamepad);

/**
 * Get a list of currently connected gamepads.
 *
 * \param count a pointer filled in with the number of gamepads returned
 * \returns a 0 terminated array of joystick instance IDs which should be
 *          freed with SDL_free(), or NULL on error; call SDL_GetError() for
 *          more details.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC SDL_JoystickID *SDLCALL SDL_GetGamepads(int *count);

/**
 * Check if the given joystick is supported by the gamepad interface.
 *
 * \param instance_id the joystick instance ID
 * \returns SDL_TRUE if the given joystick is supported by the gamepad
 *          interface, SDL_FALSE if it isn't or it's an invalid index.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadNameForIndex
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsGamepad(SDL_JoystickID instance_id);

/**
 * Get the implementation dependent name of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the name of the selected gamepad. If no name can be found, this
 *          function returns NULL; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadName
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC const char *SDLCALL SDL_GetGamepadInstanceName(SDL_JoystickID instance_id);

/**
 * Get the implementation dependent path of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the path of the selected gamepad. If no path can be found, this
 *          function returns NULL; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadPath
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC const char *SDLCALL SDL_GetGamepadInstancePath(SDL_JoystickID instance_id);

/**
 * Get the player index of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the player index of a gamepad, or -1 if it's not available
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadPlayerIndex
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC int SDLCALL SDL_GetGamepadInstancePlayerIndex(SDL_JoystickID instance_id);

/**
 * Get the implementation-dependent GUID of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the GUID of the selected gamepad. If called on an invalid index,
 *          this function returns a zero GUID
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadGUID
 * \sa SDL_GetGamepadGUIDString
 */
extern DECLSPEC SDL_JoystickGUID SDLCALL SDL_GetGamepadInstanceGUID(SDL_JoystickID instance_id);

/**
 * Get the USB vendor ID of a gamepad, if available.
 *
 * This can be called before any gamepads are opened. If the vendor ID isn't
 * available this function returns 0.
 *
 * \param instance_id the joystick instance ID
 * \returns the USB vendor ID of the selected gamepad. If called on an invalid
 *          index, this function returns zero
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadInstanceVendor(SDL_JoystickID instance_id);

/**
 * Get the USB product ID of a gamepad, if available.
 *
 * This can be called before any gamepads are opened. If the product ID isn't
 * available this function returns 0.
 *
 * \param instance_id the joystick instance ID
 * \returns the USB product ID of the selected gamepad. If called on an
 *          invalid index, this function returns zero
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadInstanceProduct(SDL_JoystickID instance_id);

/**
 * Get the product version of a gamepad, if available.
 *
 * This can be called before any gamepads are opened. If the product version
 * isn't available this function returns 0.
 *
 * \param instance_id the joystick instance ID
 * \returns the product version of the selected gamepad. If called on an
 *          invalid index, this function returns zero
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadInstanceProductVersion(SDL_JoystickID instance_id);

/**
 * Get the type of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the gamepad type.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_GamepadType SDLCALL SDL_GetGamepadInstanceType(SDL_JoystickID instance_id);

/**
 * Get the mapping of a gamepad.
 *
 * This can be called before any gamepads are opened.
 *
 * \param instance_id the joystick instance ID
 * \returns the mapping string. Must be freed with SDL_free(). Returns NULL if
 *          no mapping is available.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC char *SDLCALL SDL_GetGamepadInstanceMapping(SDL_JoystickID instance_id);

/**
 * Open a gamepad for use.
 *
 * \param instance_id the joystick instance ID
 * \returns a gamepad identifier or NULL if an error occurred; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseGamepad
 * \sa SDL_GetGamepadNameForIndex
 * \sa SDL_IsGamepad
 */
extern DECLSPEC SDL_Gamepad *SDLCALL SDL_OpenGamepad(SDL_JoystickID instance_id);

/**
 * Get the SDL_Gamepad associated with a joystick instance ID, if it has been
 * opened.
 *
 * \param instance_id the joystick instance ID of the gamepad
 * \returns an SDL_Gamepad on success or NULL on failure or if it hasn't been
 *          opened yet; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Gamepad *SDLCALL SDL_GetGamepadFromInstanceID(SDL_JoystickID instance_id);

/**
 * Get the SDL_Gamepad associated with a player index.
 *
 * \param player_index the player index, which different from the instance ID
 * \returns the SDL_Gamepad associated with a player index.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadPlayerIndex
 * \sa SDL_SetGamepadPlayerIndex
 */
extern DECLSPEC SDL_Gamepad *SDLCALL SDL_GetGamepadFromPlayerIndex(int player_index);

/**
 * Get the implementation-dependent name for an opened gamepad.
 *
 * This is the same name as returned by SDL_GetGamepadNameForIndex(), but it
 * takes a gamepad identifier instead of the (unstable) device index.
 *
 * \param gamepad a gamepad identifier previously returned by
 *                SDL_OpenGamepad()
 * \returns the implementation dependent name for the gamepad, or NULL if
 *          there is no name or the identifier passed is invalid.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadNameForIndex
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC const char *SDLCALL SDL_GetGamepadName(SDL_Gamepad *gamepad);

/**
 * Get the implementation-dependent path for an opened gamepad.
 *
 * This is the same path as returned by SDL_GetGamepadNameForIndex(), but it
 * takes a gamepad identifier instead of the (unstable) device index.
 *
 * \param gamepad a gamepad identifier previously returned by
 *                SDL_OpenGamepad()
 * \returns the implementation dependent path for the gamepad, or NULL if
 *          there is no path or the identifier passed is invalid.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadInstancePath
 */
extern DECLSPEC const char *SDLCALL SDL_GetGamepadPath(SDL_Gamepad *gamepad);

/**
 * Get the type of this currently opened gamepad
 *
 * This is the same name as returned by SDL_GetGamepadInstanceType(), but it
 * takes a gamepad identifier instead of the (unstable) device index.
 *
 * \param gamepad the gamepad object to query.
 * \returns the gamepad type.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_GamepadType SDLCALL SDL_GetGamepadType(SDL_Gamepad *gamepad);

/**
 * Get the player index of an opened gamepad.
 *
 * For XInput gamepads this returns the XInput user index.
 *
 * \param gamepad the gamepad object to query.
 * \returns the player index for gamepad, or -1 if it's not available.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetGamepadPlayerIndex(SDL_Gamepad *gamepad);

/**
 * Set the player index of an opened gamepad.
 *
 * \param gamepad the gamepad object to adjust.
 * \param player_index Player index to assign to this gamepad, or -1 to clear
 *                     the player index and turn off player LEDs.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_SetGamepadPlayerIndex(SDL_Gamepad *gamepad, int player_index);

/**
 * Get the USB vendor ID of an opened gamepad, if available.
 *
 * If the vendor ID isn't available this function returns 0.
 *
 * \param gamepad the gamepad object to query.
 * \returns the USB vendor ID, or zero if unavailable.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadVendor(SDL_Gamepad *gamepad);

/**
 * Get the USB product ID of an opened gamepad, if available.
 *
 * If the product ID isn't available this function returns 0.
 *
 * \param gamepad the gamepad object to query.
 * \returns the USB product ID, or zero if unavailable.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadProduct(SDL_Gamepad *gamepad);

/**
 * Get the product version of an opened gamepad, if available.
 *
 * If the product version isn't available this function returns 0.
 *
 * \param gamepad the gamepad object to query.
 * \returns the USB product version, or zero if unavailable.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadProductVersion(SDL_Gamepad *gamepad);

/**
 * Get the firmware version of an opened gamepad, if available.
 *
 * If the firmware version isn't available this function returns 0.
 *
 * \param gamepad the gamepad object to query.
 * \returns the gamepad firmware version, or zero if unavailable.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC Uint16 SDLCALL SDL_GetGamepadFirmwareVersion(SDL_Gamepad *gamepad);

/**
 * Get the serial number of an opened gamepad, if available.
 *
 * Returns the serial number of the gamepad, or NULL if it is not available.
 *
 * \param gamepad the gamepad object to query.
 * \returns the serial number, or NULL if unavailable.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char * SDLCALL SDL_GetGamepadSerial(SDL_Gamepad *gamepad);

/**
 * Check if a gamepad has been opened and is currently connected.
 *
 * \param gamepad a gamepad identifier previously returned by
 *                SDL_OpenGamepad()
 * \returns SDL_TRUE if the gamepad has been opened and is currently
 *          connected, or SDL_FALSE if not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseGamepad
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadConnected(SDL_Gamepad *gamepad);

/**
 * Get the underlying joystick from a gamepad
 *
 * This function will give you a SDL_Joystick object, which allows you to use
 * the SDL_Joystick functions with a SDL_Gamepad object. This would be useful
 * for getting a joystick's position at any given time, even if it hasn't
 * moved (moving it would produce an event, which would have the axis' value).
 *
 * The pointer returned is owned by the SDL_Gamepad. You should not call
 * SDL_CloseJoystick() on it, for example, since doing so will likely cause
 * SDL to crash.
 *
 * \param gamepad the gamepad object that you want to get a joystick from
 * \returns an SDL_Joystick object; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Joystick *SDLCALL SDL_GetGamepadJoystick(SDL_Gamepad *gamepad);

/**
 * Set the state of gamepad event processing.
 *
 * If gamepad events are disabled, you must call SDL_UpdateGamepads() yourself
 * and check the state of the gamepad when you want gamepad information.
 *
 * \param enabled whether to process gamepad events or not
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GamepadEventsEnabled
 */
extern DECLSPEC void SDLCALL SDL_SetGamepadEventsEnabled(SDL_bool enabled);

/**
 * Query the state of gamepad event processing.
 *
 * If gamepad events are disabled, you must call SDL_UpdateGamepads() yourself
 * and check the state of the gamepad when you want gamepad information.
 *
 * \returns SDL_TRUE if gamepad events are being processed, SDL_FALSE
 *          otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetGamepadEventsEnabled
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadEventsEnabled(void);

/**
 * Manually pump gamepad updates if not using the loop.
 *
 * This function is called automatically by the event loop if events are
 * enabled. Under such circumstances, it will not be necessary to call this
 * function.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_UpdateGamepads(void);


/**
 * Convert a string into SDL_GamepadAxis enum.
 *
 * This function is called internally to translate SDL_Gamepad mapping strings
 * for the underlying joystick device into the consistent SDL_Gamepad mapping.
 * You do not normally need to call this function unless you are parsing
 * SDL_Gamepad mappings in your own code.
 *
 * Note specially that "righttrigger" and "lefttrigger" map to
 * `SDL_GAMEPAD_AXIS_RIGHT_TRIGGER` and `SDL_GAMEPAD_AXIS_LEFT_TRIGGER`,
 * respectively.
 *
 * \param str string representing a SDL_Gamepad axis
 * \returns the SDL_GamepadAxis enum corresponding to the input string, or
 *          `SDL_GAMEPAD_AXIS_INVALID` if no match was found.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadStringForAxis
 */
extern DECLSPEC SDL_GamepadAxis SDLCALL SDL_GetGamepadAxisFromString(const char *str);

/**
 * Convert from an SDL_GamepadAxis enum to a string.
 *
 * The caller should not SDL_free() the returned string.
 *
 * \param axis an enum value for a given SDL_GamepadAxis
 * \returns a string for the given axis, or NULL if an invalid axis is
 *          specified. The string returned is of the format used by
 *          SDL_Gamepad mapping strings.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadAxisFromString
 */
extern DECLSPEC const char* SDLCALL SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis);

/**
 * Get the SDL joystick layer binding for a gamepad axis mapping.
 *
 * \param gamepad a gamepad
 * \param axis an axis enum value (one of the SDL_GamepadAxis values)
 * \returns a SDL_GamepadBinding describing the bind. On failure (like the
 *          given Controller axis doesn't exist on the device), its
 *          `.bindType` will be `SDL_GAMEPAD_BINDTYPE_NONE`.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadBindForButton
 */
extern DECLSPEC SDL_GamepadBinding SDLCALL SDL_GetGamepadBindForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);

/**
 * Query whether a gamepad has a given axis.
 *
 * This merely reports whether the gamepad's mapping defined this axis, as
 * that is all the information SDL has about the physical device.
 *
 * \param gamepad a gamepad
 * \param axis an axis enum value (an SDL_GamepadAxis value)
 * \returns SDL_TRUE if the gamepad has this axis, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);

/**
 * Get the current state of an axis control on a gamepad.
 *
 * The axis indices start at index 0.
 *
 * The state is a value ranging from -32768 to 32767. Triggers, however, range
 * from 0 to 32767 (they never return a negative value).
 *
 * \param gamepad a gamepad
 * \param axis an axis index (one of the SDL_GamepadAxis values)
 * \returns axis state (including 0) on success or 0 (also) on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadButton
 */
extern DECLSPEC Sint16 SDLCALL SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);

/**
 * Convert a string into an SDL_GamepadButton enum.
 *
 * This function is called internally to translate SDL_Gamepad mapping strings
 * for the underlying joystick device into the consistent SDL_Gamepad mapping.
 * You do not normally need to call this function unless you are parsing
 * SDL_Gamepad mappings in your own code.
 *
 * \param str string representing a SDL_Gamepad axis
 * \returns the SDL_GamepadButton enum corresponding to the input string, or
 *          `SDL_GAMEPAD_AXIS_INVALID` if no match was found.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_GamepadButton SDLCALL SDL_GetGamepadButtonFromString(const char *str);

/**
 * Convert from an SDL_GamepadButton enum to a string.
 *
 * The caller should not SDL_free() the returned string.
 *
 * \param button an enum value for a given SDL_GamepadButton
 * \returns a string for the given button, or NULL if an invalid button is
 *          specified. The string returned is of the format used by
 *          SDL_Gamepad mapping strings.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadButtonFromString
 */
extern DECLSPEC const char* SDLCALL SDL_GetGamepadStringForButton(SDL_GamepadButton button);

/**
 * Get the SDL joystick layer binding for a gamepad button mapping.
 *
 * \param gamepad a gamepad
 * \param button an button enum value (an SDL_GamepadButton value)
 * \returns a SDL_GamepadBinding describing the bind. On failure (like the
 *          given Controller button doesn't exist on the device), its
 *          `.bindType` will be `SDL_GAMEPAD_BINDTYPE_NONE`.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadBindForAxis
 */
extern DECLSPEC SDL_GamepadBinding SDLCALL SDL_GetGamepadBindForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);

/**
 * Query whether a gamepad has a given button.
 *
 * This merely reports whether the gamepad's mapping defined this button, as
 * that is all the information SDL has about the physical device.
 *
 * \param gamepad a gamepad
 * \param button a button enum value (an SDL_GamepadButton value)
 * \returns SDL_TRUE if the gamepad has this button, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);

/**
 * Get the current state of a button on a gamepad.
 *
 * \param gamepad a gamepad
 * \param button a button index (one of the SDL_GamepadButton values)
 * \returns 1 for pressed state or 0 for not pressed state or error; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadAxis
 */
extern DECLSPEC Uint8 SDLCALL SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);

/**
 * Get the number of touchpads on a gamepad.
 *
 * \param gamepad a gamepad
 * \returns number of touchpads
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetNumGamepadTouchpads(SDL_Gamepad *gamepad);

/**
 * Get the number of supported simultaneous fingers on a touchpad on a game
 * gamepad.
 *
 * \param gamepad a gamepad
 * \param touchpad a touchpad
 * \returns number of supported simultaneous fingers
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetNumGamepadTouchpadFingers(SDL_Gamepad *gamepad, int touchpad);

/**
 * Get the current state of a finger on a touchpad on a gamepad.
 *
 * \param gamepad a gamepad
 * \param touchpad a touchpad
 * \param finger a finger
 * \param state filled with state
 * \param x filled with x position
 * \param y filled with y position
 * \param pressure filled with pressure value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetGamepadTouchpadFinger(SDL_Gamepad *gamepad, int touchpad, int finger, Uint8 *state, float *x, float *y, float *pressure);

/**
 * Return whether a gamepad has a particular sensor.
 *
 * \param gamepad The gamepad to query
 * \param type The type of sensor to query
 * \returns SDL_TRUE if the sensor exists, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasSensor(SDL_Gamepad *gamepad, SDL_SensorType type);

/**
 * Set whether data reporting for a gamepad sensor is enabled.
 *
 * \param gamepad The gamepad to update
 * \param type The type of sensor to enable/disable
 * \param enabled Whether data reporting should be enabled
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_SetGamepadSensorEnabled(SDL_Gamepad *gamepad, SDL_SensorType type, SDL_bool enabled);

/**
 * Query whether sensor data reporting is enabled for a gamepad.
 *
 * \param gamepad The gamepad to query
 * \param type The type of sensor to query
 * \returns SDL_TRUE if the sensor is enabled, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadSensorEnabled(SDL_Gamepad *gamepad, SDL_SensorType type);

/**
 * Get the data rate (number of events per second) of a gamepad sensor.
 *
 * \param gamepad The gamepad to query
 * \param type The type of sensor to query
 * \returns the data rate, or 0.0f if the data rate is not available.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC float SDLCALL SDL_GetGamepadSensorDataRate(SDL_Gamepad *gamepad, SDL_SensorType type);

/**
 * Get the current state of a gamepad sensor.
 *
 * The number of values and interpretation of the data is sensor dependent.
 * See SDL_sensor.h for the details for each type of sensor.
 *
 * \param gamepad The gamepad to query
 * \param type The type of sensor to query
 * \param data A pointer filled with the current sensor state
 * \param num_values The number of values to write to data
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetGamepadSensorData(SDL_Gamepad *gamepad, SDL_SensorType type, float *data, int num_values);

/**
 * Start a rumble effect on a gamepad.
 *
 * Each call to this function cancels any previous rumble effect, and calling
 * it with 0 intensity stops any rumbling.
 *
 * \param gamepad The gamepad to vibrate
 * \param low_frequency_rumble The intensity of the low frequency (left)
 *                             rumble motor, from 0 to 0xFFFF
 * \param high_frequency_rumble The intensity of the high frequency (right)
 *                              rumble motor, from 0 to 0xFFFF
 * \param duration_ms The duration of the rumble effect, in milliseconds
 * \returns 0, or -1 if rumble isn't supported on this gamepad
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GamepadHasRumble
 */
extern DECLSPEC int SDLCALL SDL_RumbleGamepad(SDL_Gamepad *gamepad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms);

/**
 * Start a rumble effect in the gamepad's triggers.
 *
 * Each call to this function cancels any previous trigger rumble effect, and
 * calling it with 0 intensity stops any rumbling.
 *
 * Note that this is rumbling of the _triggers_ and not the gamepad as a
 * whole. This is currently only supported on Xbox One gamepads. If you want
 * the (more common) whole-gamepad rumble, use SDL_RumbleGamepad() instead.
 *
 * \param gamepad The gamepad to vibrate
 * \param left_rumble The intensity of the left trigger rumble motor, from 0
 *                    to 0xFFFF
 * \param right_rumble The intensity of the right trigger rumble motor, from 0
 *                     to 0xFFFF
 * \param duration_ms The duration of the rumble effect, in milliseconds
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GamepadHasRumbleTriggers
 */
extern DECLSPEC int SDLCALL SDL_RumbleGamepadTriggers(SDL_Gamepad *gamepad, Uint16 left_rumble, Uint16 right_rumble, Uint32 duration_ms);

/**
 * Query whether a gamepad has an LED.
 *
 * \param gamepad The gamepad to query
 * \returns SDL_TRUE, or SDL_FALSE if this gamepad does not have a modifiable
 *          LED
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasLED(SDL_Gamepad *gamepad);

/**
 * Query whether a gamepad has rumble support.
 *
 * \param gamepad The gamepad to query
 * \returns SDL_TRUE, or SDL_FALSE if this gamepad does not have rumble
 *          support
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RumbleGamepad
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasRumble(SDL_Gamepad *gamepad);

/**
 * Query whether a gamepad has rumble support on triggers.
 *
 * \param gamepad The gamepad to query
 * \returns SDL_TRUE, or SDL_FALSE if this gamepad does not have trigger
 *          rumble support
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RumbleGamepadTriggers
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GamepadHasRumbleTriggers(SDL_Gamepad *gamepad);

/**
 * Update a gamepad's LED color.
 *
 * \param gamepad The gamepad to update
 * \param red The intensity of the red LED
 * \param green The intensity of the green LED
 * \param blue The intensity of the blue LED
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_SetGamepadLED(SDL_Gamepad *gamepad, Uint8 red, Uint8 green, Uint8 blue);

/**
 * Send a gamepad specific effect packet
 *
 * \param gamepad The gamepad to affect
 * \param data The data to send to the gamepad
 * \param size The size of the data to send to the gamepad
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_SendGamepadEffect(SDL_Gamepad *gamepad, const void *data, int size);

/**
 * Close a gamepad previously opened with SDL_OpenGamepad().
 *
 * \param gamepad a gamepad identifier previously returned by
 *                SDL_OpenGamepad()
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenGamepad
 */
extern DECLSPEC void SDLCALL SDL_CloseGamepad(SDL_Gamepad *gamepad);

/**
 * Return the sfSymbolsName for a given button on a gamepad on Apple
 * platforms.
 *
 * \param gamepad the gamepad to query
 * \param button a button on the gamepad
 * \returns the sfSymbolsName or NULL if the name can't be found
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadAppleSFSymbolsNameForAxis
 */
extern DECLSPEC const char* SDLCALL SDL_GetGamepadAppleSFSymbolsNameForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);

/**
 * Return the sfSymbolsName for a given axis on a gamepad on Apple platforms.
 *
 * \param gamepad the gamepad to query
 * \param axis an axis on the gamepad
 * \returns the sfSymbolsName or NULL if the name can't be found
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGamepadAppleSFSymbolsNameForButton
 */
extern DECLSPEC const char* SDLCALL SDL_GetGamepadAppleSFSymbolsNameForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_gamepad_h_ */
