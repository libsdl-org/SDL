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

/**
 * # CategoryKeyboard
 *
 * SDL keyboard management.
 */

#ifndef SDL_keyboard_h_
#define SDL_keyboard_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is a unique ID for a keyboard for the time it is connected to the
 * system, and is never reused for the lifetime of the application.
 *
 * If the keyboard is disconnected and reconnected, it will get a new ID.
 *
 * The ID value starts at 1 and increments from there. The value 0 is an
 * invalid ID.
 *
 * \since This datatype is available since SDL 3.0.0.
 */
typedef Uint32 SDL_KeyboardID;

/* Function prototypes */

/**
 * Return whether a keyboard is currently connected.
 *
 * \returns SDL_TRUE if a keyboard is connected, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyboards
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_HasKeyboard(void);

/**
 * Get a list of currently connected keyboards.
 *
 * Note that this will include any device or virtual driver that includes
 * keyboard functionality, including some mice, KVM switches, motherboard
 * power buttons, etc. You should wait for input from a device before you
 * consider it actively in use.
 *
 * \param count a pointer filled in with the number of keyboards returned.
 * \returns a 0 terminated array of keyboards instance IDs which should be
 *          freed with SDL_free(), or NULL on error; call SDL_GetError() for
 *          more details.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyboardInstanceName
 * \sa SDL_HasKeyboard
 */
extern SDL_DECLSPEC SDL_KeyboardID *SDLCALL SDL_GetKeyboards(int *count);

/**
 * Get the name of a keyboard.
 *
 * This function returns "" if the keyboard doesn't have a name.
 *
 * The returned string follows the SDL_GetStringRule.
 *
 * \param instance_id the keyboard instance ID.
 * \returns the name of the selected keyboard, or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyboards
 */
extern SDL_DECLSPEC const char *SDLCALL SDL_GetKeyboardInstanceName(SDL_KeyboardID instance_id);

/**
 * Query the window which currently has keyboard focus.
 *
 * \returns the window with keyboard focus.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_Window * SDLCALL SDL_GetKeyboardFocus(void);

/**
 * Get a snapshot of the current state of the keyboard.
 *
 * The pointer returned is a pointer to an internal SDL array. It will be
 * valid for the whole lifetime of the application and should not be freed by
 * the caller.
 *
 * A array element with a value of 1 means that the key is pressed and a value
 * of 0 means that it is not. Indexes into this array are obtained by using
 * SDL_Scancode values.
 *
 * Use SDL_PumpEvents() to update the state array.
 *
 * This function gives you the current state after all events have been
 * processed, so if a key or button has been pressed and released before you
 * process events, then the pressed state will never show up in the
 * SDL_GetKeyboardState() calls.
 *
 * Note: This function doesn't take into account whether shift has been
 * pressed or not.
 *
 * \param numkeys if non-NULL, receives the length of the returned array.
 * \returns a pointer to an array of key states.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_PumpEvents
 * \sa SDL_ResetKeyboard
 */
extern SDL_DECLSPEC const Uint8 *SDLCALL SDL_GetKeyboardState(int *numkeys);

/**
 * Clear the state of the keyboard.
 *
 * This function will generate key up events for all pressed keys.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyboardState
 */
extern SDL_DECLSPEC void SDLCALL SDL_ResetKeyboard(void);

/**
 * Get the current key modifier state for the keyboard.
 *
 * \returns an OR'd combination of the modifier keys for the keyboard. See
 *          SDL_Keymod for details.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyboardState
 * \sa SDL_SetModState
 */
extern SDL_DECLSPEC SDL_Keymod SDLCALL SDL_GetModState(void);

/**
 * Set the current key modifier state for the keyboard.
 *
 * The inverse of SDL_GetModState(), SDL_SetModState() allows you to impose
 * modifier key states on your application. Simply pass your desired modifier
 * states into `modstate`. This value may be a bitwise, OR'd combination of
 * SDL_Keymod values.
 *
 * This does not change the keyboard state, only the key modifier flags that
 * SDL reports.
 *
 * \param modstate the desired SDL_Keymod for the keyboard.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetModState
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetModState(SDL_Keymod modstate);

/**
 * Get the key code corresponding to the given scancode according to a default
 * en_US keyboard layout.
 *
 * See SDL_Keycode for details.
 *
 * \param scancode the desired SDL_Scancode to query.
 * \param modstate the modifier state to use when translating the scancode to
 *                 a keycode.
 * \returns the SDL_Keycode that corresponds to the given SDL_Scancode.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyName
 * \sa SDL_GetScancodeFromKey
 */
extern SDL_DECLSPEC SDL_Keycode SDLCALL SDL_GetDefaultKeyFromScancode(SDL_Scancode scancode, SDL_Keymod modstate);

/**
 * Get the key code corresponding to the given scancode according to the
 * current keyboard layout.
 *
 * See SDL_Keycode for details.
 *
 * \param scancode the desired SDL_Scancode to query.
 * \param modstate the modifier state to use when translating the scancode to
 *                 a keycode.
 * \returns the SDL_Keycode that corresponds to the given SDL_Scancode.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetDefaultKeyFromScancode
 * \sa SDL_GetKeyName
 * \sa SDL_GetScancodeFromKey
 */
extern SDL_DECLSPEC SDL_Keycode SDLCALL SDL_GetKeyFromScancode(SDL_Scancode scancode, SDL_Keymod modstate);

/**
 * Get the scancode corresponding to the given key code according to a default
 * en_US keyboard layout.
 *
 * Note that there may be multiple scancode+modifier states that can generate
 * this keycode, this will just return the first one found.
 *
 * \param key the desired SDL_Keycode to query.
 * \param modstate a pointer to the modifier state that would be used when the
 *                 scancode generates this key, may be NULL.
 * \returns the SDL_Scancode that corresponds to the given SDL_Keycode.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetScancodeFromKey
 * \sa SDL_GetScancodeName
 */
extern SDL_DECLSPEC SDL_Scancode SDLCALL SDL_GetDefaultScancodeFromKey(SDL_Keycode key, SDL_Keymod *modstate);

/**
 * Get the scancode corresponding to the given key code according to the
 * current keyboard layout.
 *
 * Note that there may be multiple scancode+modifier states that can generate
 * this keycode, this will just return the first one found.
 *
 * \param key the desired SDL_Keycode to query.
 * \param modstate a pointer to the modifier state that would be used when the
 *                 scancode generates this key, may be NULL.
 * \returns the SDL_Scancode that corresponds to the given SDL_Keycode.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetDefaultScancodeFromKey
 * \sa SDL_GetKeyFromScancode
 * \sa SDL_GetScancodeName
 */
extern SDL_DECLSPEC SDL_Scancode SDLCALL SDL_GetScancodeFromKey(SDL_Keycode key, SDL_Keymod *modstate);

/**
 * Set a human-readable name for a scancode.
 *
 * \param scancode the desired SDL_Scancode.
 * \param name the name to use for the scancode, encoded as UTF-8. The string
 *             is not copied, so the pointer given to this function must stay
 *             valid while SDL is being used.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetScancodeName
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetScancodeName(SDL_Scancode scancode, const char *name);

/**
 * Get a human-readable name for a scancode.
 *
 * The returned string follows the SDL_GetStringRule.
 *
 * **Warning**: The returned name is by design not stable across platforms,
 * e.g. the name for `SDL_SCANCODE_LGUI` is "Left GUI" under Linux but "Left
 * Windows" under Microsoft Windows, and some scancodes like
 * `SDL_SCANCODE_NONUSBACKSLASH` don't have any name at all. There are even
 * scancodes that share names, e.g. `SDL_SCANCODE_RETURN` and
 * `SDL_SCANCODE_RETURN2` (both called "Return"). This function is therefore
 * unsuitable for creating a stable cross-platform two-way mapping between
 * strings and scancodes.
 *
 * \param scancode the desired SDL_Scancode to query.
 * \returns a pointer to the name for the scancode. If the scancode doesn't
 *          have a name this function returns an empty string ("").
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetScancodeFromKey
 * \sa SDL_GetScancodeFromName
 * \sa SDL_SetScancodeName
 */
extern SDL_DECLSPEC const char *SDLCALL SDL_GetScancodeName(SDL_Scancode scancode);

/**
 * Get a scancode from a human-readable name.
 *
 * \param name the human-readable scancode name.
 * \returns the SDL_Scancode, or `SDL_SCANCODE_UNKNOWN` if the name wasn't
 *          recognized; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyFromName
 * \sa SDL_GetScancodeFromKey
 * \sa SDL_GetScancodeName
 */
extern SDL_DECLSPEC SDL_Scancode SDLCALL SDL_GetScancodeFromName(const char *name);

/**
 * Get a human-readable name for a key.
 *
 * The returned string follows the SDL_GetStringRule.
 *
 * \param key the desired SDL_Keycode to query.
 * \returns a pointer to a UTF-8 string that stays valid at least until the
 *          next call to this function. If you need it around any longer, you
 *          must copy it. If the key doesn't have a name, this function
 *          returns an empty string ("").
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyFromName
 * \sa SDL_GetKeyFromScancode
 * \sa SDL_GetScancodeFromKey
 */
extern SDL_DECLSPEC const char *SDLCALL SDL_GetKeyName(SDL_Keycode key);

/**
 * Get a key code from a human-readable name.
 *
 * \param name the human-readable key name.
 * \returns key code, or `SDLK_UNKNOWN` if the name wasn't recognized; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetKeyFromScancode
 * \sa SDL_GetKeyName
 * \sa SDL_GetScancodeFromName
 */
extern SDL_DECLSPEC SDL_Keycode SDLCALL SDL_GetKeyFromName(const char *name);

/**
 * Start accepting Unicode text input events in a window.
 *
 * This function will enable text input (SDL_EVENT_TEXT_INPUT and
 * SDL_EVENT_TEXT_EDITING events) in the specified window. Please use this
 * function paired with SDL_StopTextInput().
 *
 * Text input events are not received by default.
 *
 * On some platforms using this function shows the screen keyboard.
 *
 * \param window the window to enable text input.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetTextInputRect
 * \sa SDL_StopTextInput
 * \sa SDL_TextInputActive
 */
extern SDL_DECLSPEC int SDLCALL SDL_StartTextInput(SDL_Window *window);

/**
 * Check whether or not Unicode text input events are enabled for a window.
 *
 * \param window the window to check.
 * \returns SDL_TRUE if text input events are enabled else SDL_FALSE.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartTextInput
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_TextInputActive(SDL_Window *window);

/**
 * Stop receiving any text input events in a window.
 *
 * If SDL_StartTextInput() showed the screen keyboard, this function will hide
 * it.
 *
 * \param window the window to disable text input.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartTextInput
 */
extern SDL_DECLSPEC int SDLCALL SDL_StopTextInput(SDL_Window *window);

/**
 * Dismiss the composition window/IME without disabling the subsystem.
 *
 * \param window the window to affect.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartTextInput
 * \sa SDL_StopTextInput
 */
extern SDL_DECLSPEC int SDLCALL SDL_ClearComposition(SDL_Window *window);

/**
 * Set the rectangle used to type Unicode text inputs.
 *
 * This is often set to the extents of a text field within the window.
 *
 * Native input methods will place a window with word suggestions near it,
 * without covering the text being inputted.
 *
 * To start text input in a given location, this function is intended to be
 * called before SDL_StartTextInput, although some platforms support moving
 * the rectangle even while text input (and a composition) is active.
 *
 * Note: If you want to use the system native IME window, try setting hint
 * **SDL_HINT_IME_SHOW_UI** to **1**, otherwise this function won't give you
 * any feedback.
 *
 * \param window the window for which to set the text input rectangle.
 * \param rect the SDL_Rect structure representing the rectangle to receive
 *             text (ignored if NULL).
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartTextInput
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetTextInputRect(SDL_Window *window, const SDL_Rect *rect);

/**
 * Check whether the platform has screen keyboard support.
 *
 * \returns SDL_TRUE if the platform has some screen keyboard support or
 *          SDL_FALSE if not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StartTextInput
 * \sa SDL_ScreenKeyboardShown
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_HasScreenKeyboardSupport(void);

/**
 * Check whether the screen keyboard is shown for given window.
 *
 * \param window the window for which screen keyboard should be queried.
 * \returns SDL_TRUE if screen keyboard is shown or SDL_FALSE if not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_HasScreenKeyboardSupport
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_ScreenKeyboardShown(SDL_Window *window);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_keyboard_h_ */
