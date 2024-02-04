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
 *  \file SDL_hints.h
 *
 *  Official documentation for SDL configuration variables
 *
 *  This file contains functions to set and get configuration hints,
 *  as well as listing each of them alphabetically.
 *
 *  The convention for naming hints is SDL_HINT_X, where "SDL_X" is
 *  the environment variable that can be used to override the default.
 *
 *  In general these hints are just that - they may or may not be
 *  supported or applicable on any given platform, but they provide
 *  a way for an application or user to give the library a hint as
 *  to how they would like the library to work.
 */

#ifndef SDL_hints_h_
#define SDL_hints_h_

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set if Android/iOS accelerometers should be listed as joystick devices.
 *
 * This variable can be set to the following values:
 *    "0"       - The accelerometer is not listed as a joystick
 *    "1"       - The accelerometer is available as a 3 axis joystick (the default).
 */
#define SDL_HINT_ACCELEROMETER_AS_JOYSTICK "SDL_ACCELEROMETER_AS_JOYSTICK"

/**
 * Specify the behavior of Alt+Tab while the keyboard is grabbed.
 *
 * By default, SDL emulates Alt+Tab functionality while the keyboard is grabbed
 * and your window is full-screen. This prevents the user from getting stuck in
 * your application if you've enabled keyboard grab.
 *
 * The variable can be set to the following values:
 *   "0"       - SDL will not handle Alt+Tab. Your application is responsible
                 for handling Alt+Tab while the keyboard is grabbed.
 *   "1"       - SDL will minimize your window when Alt+Tab is pressed (default)
*/
#define SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED "SDL_ALLOW_ALT_TAB_WHILE_GRABBED"

/**
 *  If set to "0" then never set the top most bit on a SDL Window, even if the video mode expects it.
 *      This is a debugging aid for developers and not expected to be used by end users. The default is "1"
 *
 *  This variable can be set to the following values:
 *    "0"       - don't allow topmost
 *    "1"       - allow topmost
 */
#define SDL_HINT_ALLOW_TOPMOST "SDL_ALLOW_TOPMOST"

/**
 * A variable to control whether the event loop will block itself when the app is paused.
 *
 * The variable can be set to the following values:
 *   "0"       - Non blocking.
 *   "1"       - Blocking. (default)
 *
 * The value should be set before SDL is initialized.
 */
#define SDL_HINT_ANDROID_BLOCK_ON_PAUSE "SDL_ANDROID_BLOCK_ON_PAUSE"

/**
 * A variable to control whether SDL will pause audio in background
 *        (Requires SDL_ANDROID_BLOCK_ON_PAUSE as "Non blocking")
 *
 * The variable can be set to the following values:
 *   "0"       - Non paused.
 *   "1"       - Paused. (default)
 *
 * The value should be set before SDL is initialized.
 */
#define SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO "SDL_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO"

/**
 * A variable to control whether we trap the Android back button to handle it manually.
 *        This is necessary for the right mouse button to work on some Android devices, or
 *        to be able to trap the back button for use in your code reliably.  If set to true,
 *        the back button will show up as an SDL_EVENT_KEY_DOWN / SDL_EVENT_KEY_UP pair with a keycode of
 *        SDL_SCANCODE_AC_BACK.
 *
 * The variable can be set to the following values:
 *   "0"       - Back button will be handled as usual for system. (default)
 *   "1"       - Back button will be trapped, allowing you to handle the key press
 *               manually.  (This will also let right mouse click work on systems
 *               where the right mouse button functions as back.)
 *
 * The value of this hint is used at runtime, so it can be changed at any time.
 */
#define SDL_HINT_ANDROID_TRAP_BACK_BUTTON "SDL_ANDROID_TRAP_BACK_BUTTON"

/**
 * A variable to control whether SDL activity is allowed to be re-created.
 *        If so, java static datas and static datas from native libraries remain with their current values.
 *        When not allowed, the activity terminates with exit(0) to be fully re-initialized afterward.
 *
 * The variable can be set to the following values:
 *   "0"       - Not allowed. (default)
 *   "1"       - Allowed.
 *
 * The value of this hint is used at runtime, so it can be changed at any time.
 */
#define SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY "SDL_ANDROID_ALLOW_RECREATE_ACTIVITY"

/**
 *  A variable setting the app ID string.
 *          This string is used by desktop compositors to identify and group windows
 *          together, as well as match applications with associated desktop settings
 *          and icons.
 *
 *          On Wayland this corresponds to the "app ID" window property and on X11 this
 *          corresponds to the WM_CLASS property. Windows inherit the value of this hint
 *          at creation time. Changing this hint after a window has been created will not
 *          change the app ID or class of existing windows.
 *
 *          For *nix platforms, this string should be formatted in reverse-DNS notation
 *          and follow some basic rules to be valid:
 *
 *          - The application ID must be composed of two or more elements separated by a
 *            period (‘.’) character.
 *
 *          - Each element must contain one or more of the alphanumeric characters
 *            (A-Z, a-z, 0-9) plus underscore (‘_’) and hyphen (‘-’) and must not start
 *            with a digit. Note that hyphens, while technically allowed, should not be
 *            used if possible, as they are not supported by all components that use the ID,
 *            such as D-Bus. For maximum compatibility, replace hyphens with an underscore.
 *
 *          - The empty string is not a valid element (ie: your application ID may not
 *            start or end with a period and it is not valid to have two periods in a row).
 *
 *          - The entire ID must be less than 255 characters in length.
 *
 *          Examples of valid app ID strings:
 *
 *          - org.MyOrg.MyApp
 *          - com.your_company.your_app
 *
 *          Desktops such as GNOME and KDE require that the app ID string matches your
 *          application's .desktop file name (e.g. if the app ID string is 'org.MyOrg.MyApp',
 *          your application's .desktop file should be named 'org.MyOrg.MyApp.desktop').
 *
 *          If you plan to package your application in a container such as Flatpak, the
 *          app ID should match the name of your Flatpak container as well.
 *
 *  If not set, SDL will attempt to use the application executable name.
 *  If the executable name cannot be retrieved, the generic string "SDL_App" will be used.
 *
 *  On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_APP_ID      "SDL_APP_ID"

/**
 *  Specify an application name.
 *
 * This hint lets you specify the application name sent to the OS when
 * required. For example, this will often appear in volume control applets for
 * audio streams, and in lists of applications which are inhibiting the
 * screensaver.  You should use a string that describes your program ("My Game
 * 2: The Revenge")
 *
 * Setting this to "" or leaving it unset will have SDL use a reasonable
 * default: probably the application's name or "SDL Application" if SDL
 * doesn't have any better information.
 *
 * Note that, for audio streams, this can be overridden with
 * SDL_HINT_AUDIO_DEVICE_APP_NAME.
 *
 * On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_APP_NAME "SDL_APP_NAME"

/**
 *  A variable controlling whether controllers used with the Apple TV
 *  generate UI events.
 *
 * When UI events are generated by controller input, the app will be
 * backgrounded when the Apple TV remote's menu button is pressed, and when the
 * pause or B buttons on gamepads are pressed.
 *
 * More information about properly making use of controllers for the Apple TV
 * can be found here:
 * https://developer.apple.com/tvos/human-interface-guidelines/remote-and-controllers/
 *
 *  This variable can be set to the following values:
 *    "0"       - Controller input does not generate UI events (the default).
 *    "1"       - Controller input generates UI events.
 */
#define SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS "SDL_APPLE_TV_CONTROLLER_UI_EVENTS"

/**
 * A variable controlling whether the Apple TV remote's joystick axes
 *         will automatically match the rotation of the remote.
 *
 *  This variable can be set to the following values:
 *    "0"       - Remote orientation does not affect joystick axes (the default).
 *    "1"       - Joystick axes are based on the orientation of the remote.
 */
#define SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION "SDL_APPLE_TV_REMOTE_ALLOW_ROTATION"

/**
 *  A variable controlling the audio category on iOS and macOS
 *
 *  This variable can be set to the following values:
 *
 *    "ambient"     - Use the AVAudioSessionCategoryAmbient audio category, will be muted by the phone mute switch (default)
 *    "playback"    - Use the AVAudioSessionCategoryPlayback category
 *
 *  For more information, see Apple's documentation:
 *  https://developer.apple.com/library/content/documentation/Audio/Conceptual/AudioSessionProgrammingGuide/AudioSessionCategoriesandModes/AudioSessionCategoriesandModes.html
 */
#define SDL_HINT_AUDIO_CATEGORY   "SDL_AUDIO_CATEGORY"

/**
 *  Specify an application name for an audio device.
 *
 * Some audio backends (such as PulseAudio) allow you to describe your audio
 * stream. Among other things, this description might show up in a system
 * control panel that lets the user adjust the volume on specific audio
 * streams instead of using one giant master volume slider.
 *
 * This hints lets you transmit that information to the OS. The contents of
 * this hint are used while opening an audio device. You should use a string
 * that describes your program ("My Game 2: The Revenge")
 *
 * Setting this to "" or leaving it unset will have SDL use a reasonable
 * default: this will be the name set with SDL_HINT_APP_NAME, if that hint is
 * set. Otherwise, it'll probably the application's name or "SDL Application"
 * if SDL doesn't have any better information.
 *
 * On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_AUDIO_DEVICE_APP_NAME "SDL_AUDIO_DEVICE_APP_NAME"

/**
 *  Specify an application name for an audio device.
 *
 * Some audio backends (such as PulseAudio) allow you to describe your audio
 * stream. Among other things, this description might show up in a system
 * control panel that lets the user adjust the volume on specific audio
 * streams instead of using one giant master volume slider.
 *
 * This hints lets you transmit that information to the OS. The contents of
 * this hint are used while opening an audio device. You should use a string
 * that describes your what your program is playing ("audio stream" is
 * probably sufficient in many cases, but this could be useful for something
 * like "team chat" if you have a headset playing VoIP audio separately).
 *
 * Setting this to "" or leaving it unset will have SDL use a reasonable
 * default: "audio stream" or something similar.
 *
 * On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_AUDIO_DEVICE_STREAM_NAME "SDL_AUDIO_DEVICE_STREAM_NAME"

/**
 *  Specify an application role for an audio device.
 *
 * Some audio backends (such as Pipewire) allow you to describe the role of
 * your audio stream. Among other things, this description might show up in
 * a system control panel or software for displaying and manipulating media
 * playback/capture graphs.
 *
 * This hints lets you transmit that information to the OS. The contents of
 * this hint are used while opening an audio device. You should use a string
 * that describes your what your program is playing (Game, Music, Movie,
 * etc...).
 *
 * Setting this to "" or leaving it unset will have SDL use a reasonable
 * default: "Game" or something similar.
 *
 * On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_AUDIO_DEVICE_STREAM_ROLE "SDL_AUDIO_DEVICE_STREAM_ROLE"

/**
 *  A variable controlling whether SDL updates joystick state when getting input events
 *
 *  This variable can be set to the following values:
 *
 *    "0"     - You'll call SDL_UpdateJoysticks() manually
 *    "1"     - SDL will automatically call SDL_UpdateJoysticks() (default)
 *
 *  This hint can be toggled on and off at runtime.
 */
#define SDL_HINT_AUTO_UPDATE_JOYSTICKS  "SDL_AUTO_UPDATE_JOYSTICKS"

/**
 *  A variable controlling whether SDL updates sensor state when getting input events
 *
 *  This variable can be set to the following values:
 *
 *    "0"     - You'll call SDL_UpdateSensors() manually
 *    "1"     - SDL will automatically call SDL_UpdateSensors() (default)
 *
 *  This hint can be toggled on and off at runtime.
 */
#define SDL_HINT_AUTO_UPDATE_SENSORS    "SDL_AUTO_UPDATE_SENSORS"

/**
 *  Prevent SDL from using version 4 of the bitmap header when saving BMPs.
 *
 * The bitmap header version 4 is required for proper alpha channel support and
 * SDL will use it when required. Should this not be desired, this hint can
 * force the use of the 40 byte header version which is supported everywhere.
 *
 * The variable can be set to the following values:
 *   "0"       - Surfaces with a colorkey or an alpha channel are saved to a
 *               32-bit BMP file with an alpha mask. SDL will use the bitmap
 *               header version 4 and set the alpha mask accordingly.
 *   "1"       - Surfaces with a colorkey or an alpha channel are saved to a
 *               32-bit BMP file without an alpha mask. The alpha channel data
 *               will be in the file, but applications are going to ignore it.
 *
 * The default value is "0".
 */
#define SDL_HINT_BMP_SAVE_LEGACY_FORMAT "SDL_BMP_SAVE_LEGACY_FORMAT"

/**
 *  Override for SDL_GetDisplayUsableBounds()
 *
 *  If set, this hint will override the expected results for
 *  SDL_GetDisplayUsableBounds() for display index 0. Generally you don't want
 *  to do this, but this allows an embedded system to request that some of the
 *  screen be reserved for other uses when paired with a well-behaved
 *  application.
 *
 *  The contents of this hint must be 4 comma-separated integers, the first
 *  is the bounds x, then y, width and height, in that order.
 */
#define SDL_HINT_DISPLAY_USABLE_BOUNDS "SDL_DISPLAY_USABLE_BOUNDS"

/**
 *  Disable giving back control to the browser automatically
 *  when running with asyncify
 *
 * With -s ASYNCIFY, SDL calls emscripten_sleep during operations
 * such as refreshing the screen or polling events.
 *
 * This hint only applies to the emscripten platform
 *
 * The variable can be set to the following values:
 *    "0"       - Disable emscripten_sleep calls (if you give back browser control manually or use asyncify for other purposes)
 *    "1"       - Enable emscripten_sleep calls (the default)
 */
#define SDL_HINT_EMSCRIPTEN_ASYNCIFY   "SDL_EMSCRIPTEN_ASYNCIFY"

/**
 *  Specify the CSS selector used for the "default" window/canvas
 *
 * This hint only applies to the emscripten platform
 *
 * The default value is "#canvas"
 */
#define SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR "SDL_EMSCRIPTEN_CANVAS_SELECTOR"

/**
 *  override the binding element for keyboard inputs for Emscripten builds
 *
 * This hint only applies to the emscripten platform
 *
 * The variable can be one of
 *    "#window"      - The javascript window object (this is the default)
 *    "#document"    - The javascript document object
 *    "#screen"      - the javascript window.screen object
 *    "#canvas"      - the WebGL canvas element
 *    any other string without a leading # sign applies to the element on the page with that ID.
 */
#define SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT   "SDL_EMSCRIPTEN_KEYBOARD_ELEMENT"

/**
 *  A variable that controls whether the on-screen keyboard should be shown when text input is active
 *
 *  The variable can be set to the following values:
 *    "0"       - Do not show the on-screen keyboard
 *    "1"       - Show the on-screen keyboard
 *
 *  The default value is "1". This hint must be set before text input is activated.
 */
#define SDL_HINT_ENABLE_SCREEN_KEYBOARD "SDL_ENABLE_SCREEN_KEYBOARD"

/**
 *  A variable controlling verbosity of the logging of SDL events pushed onto the internal queue.
 *
 *  This variable can be set to the following values, from least to most verbose:
 *
 *    "0"     - Don't log any events (default)
 *    "1"     - Log most events (other than the really spammy ones).
 *    "2"     - Include mouse and finger motion events.
 *
 *  This is generally meant to be used to debug SDL itself, but can be useful
 *  for application developers that need better visibility into what is going
 *  on in the event queue. Logged events are sent through SDL_Log(), which
 *  means by default they appear on stdout on most platforms or maybe
 *  OutputDebugString() on Windows, and can be funneled by the app with
 *  SDL_LogSetOutputFunction(), etc.
 *
 *  This hint can be toggled on and off at runtime, if you only need to log
 *  events for a small subset of program execution.
 */
#define SDL_HINT_EVENT_LOGGING   "SDL_EVENT_LOGGING"

/**
 *  A variable controlling whether raising the window should be done more forcefully
 *
 *  This variable can be set to the following values:
 *    "0"       - No forcing (the default)
 *    "1"       - Extra level of forcing
 *
 *  At present, this is only an issue under MS Windows, which makes it nearly impossible to
 *  programmatically move a window to the foreground, for "security" reasons. See
 *  http://stackoverflow.com/a/34414846 for a discussion.
 */
#define SDL_HINT_FORCE_RAISEWINDOW    "SDL_HINT_FORCE_RAISEWINDOW"

/**
*  A variable controlling whether the window is activated when the SDL_RaiseWindow function is called
*
*  This variable can be set to the following values:
*    "0"       - The window is not activated when the SDL_RaiseWindow function is called
*    "1"       - The window is activated when the SDL_RaiseWindow function is called
*
*  By default SDL will activate the window when the SDL_RaiseWindow function is called.
*  At present this is only available for MS Windows.
*/
#define SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED    "SDL_WINDOW_ACTIVATE_WHEN_RAISED"

/**
 *  A variable controlling how 3D acceleration is used to accelerate the SDL screen surface.
 *
 *  SDL can try to accelerate the SDL screen surface by using streaming
 *  textures with a 3D rendering engine.  This variable controls whether and
 *  how this is done.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable 3D acceleration
 *    "1"       - Enable 3D acceleration, using the default renderer.
 *    "X"       - Enable 3D acceleration, using X where X is one of the valid rendering drivers.  (e.g. "direct3d", "opengl", etc.)
 *
 *  By default SDL tries to make a best guess for each platform whether
 *  to use acceleration or not.
 */
#define SDL_HINT_FRAMEBUFFER_ACCELERATION   "SDL_FRAMEBUFFER_ACCELERATION"

/**
 *  A variable that lets you manually hint extra gamecontroller db entries.
 *
 *  The variable should be newline delimited rows of gamecontroller config data, see SDL_gamepad.h
 *
 *  This hint must be set before calling SDL_Init(SDL_INIT_GAMEPAD)
 *  You can update mappings after the system is initialized with SDL_GetGamepadMappingForGUID() and SDL_AddGamepadMapping()
 */
#define SDL_HINT_GAMECONTROLLERCONFIG "SDL_GAMECONTROLLERCONFIG"

/**
 *  A variable that lets you provide a file with extra gamecontroller db entries.
 *
 *  The file should contain lines of gamecontroller config data, see SDL_gamepad.h
 *
 *  This hint must be set before calling SDL_Init(SDL_INIT_GAMEPAD)
 *  You can update mappings after the system is initialized with SDL_GetGamepadMappingForGUID() and SDL_AddGamepadMapping()
 */
#define SDL_HINT_GAMECONTROLLERCONFIG_FILE "SDL_GAMECONTROLLERCONFIG_FILE"

/**
 *  A variable that overrides the automatic controller type detection
 *
 *  The variable should be comma separated entries, in the form: VID/PID=type
 *
 *  The VID and PID should be hexadecimal with exactly 4 digits, e.g. 0x00fd
 *
 *  The type should be one of:
 *      Xbox360
 *      XboxOne
 *      PS3
 *      PS4
 *      PS5
 *      SwitchPro
 *
 *  This hint affects what driver is used, and must be set before calling SDL_Init(SDL_INIT_GAMEPAD)
 */
#define SDL_HINT_GAMECONTROLLERTYPE "SDL_GAMECONTROLLERTYPE"

/**
 *  A variable containing a list of devices to skip when scanning for game controllers.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES "SDL_GAMECONTROLLER_IGNORE_DEVICES"

/**
 *  If set, all devices will be skipped when scanning for game controllers except for the ones listed in this variable.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT "SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT"

/**
 *  Controls whether the device's built-in accelerometer and gyro should be used as sensors for gamepads.
 *
 *  The variable can be set to the following values:
 *    "0"       - Sensor fusion is disabled
 *    "1"       - Sensor fusion is enabled for all controllers that lack sensors
 *
 *  Or the variable can be a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 *
 *  This hint is checked when a gamepad is opened.
 */
#define SDL_HINT_GAMECONTROLLER_SENSOR_FUSION "SDL_GAMECONTROLLER_SENSOR_FUSION"

/**
 *  A variable controlling whether grabbing input grabs the keyboard
 *
 *  This variable can be set to the following values:
 *    "0"       - Grab will affect only the mouse
 *    "1"       - Grab will affect mouse and keyboard
 *
 *  By default SDL will not grab the keyboard so system shortcuts still work.
 */
#define SDL_HINT_GRAB_KEYBOARD              "SDL_GRAB_KEYBOARD"

/**
 *  A variable to control whether SDL_hid_enumerate() enumerates all HID devices or only controllers.
 *
 *  This variable can be set to the following values:
 *    "0"       - SDL_hid_enumerate() will enumerate all HID devices
 *    "1"       - SDL_hid_enumerate() will only enumerate controllers
 *
 *  By default SDL will only enumerate controllers, to reduce risk of hanging or crashing on devices with bad drivers and avoiding macOS keyboard capture permission prompts.
 */
#define SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS "SDL_HIDAPI_ENUMERATE_ONLY_CONTROLLERS"

/**
 *  A variable containing a list of devices to ignore in SDL_hid_enumerate()
 *
 *  For example, to ignore the Shanwan DS3 controller and any Valve controller, you might
 *  have the string "0x2563/0x0523,0x28de/0x0000"
 */
#define SDL_HINT_HIDAPI_IGNORE_DEVICES "SDL_HIDAPI_IGNORE_DEVICES"

/**
 * A variable to control whether certain IMEs should handle text editing internally instead of sending SDL_EVENT_TEXT_EDITING events.
 *
 * The variable can be set to the following values:
 *   "0"       - SDL_EVENT_TEXT_EDITING events are sent, and it is the application's
 *               responsibility to render the text from these events and
 *               differentiate it somehow from committed text. (default)
 *   "1"       - If supported by the IME then SDL_EVENT_TEXT_EDITING events are not sent,
 *               and text that is being composed will be rendered in its own UI.
 */
#define SDL_HINT_IME_INTERNAL_EDITING "SDL_IME_INTERNAL_EDITING"

/**
 * A variable to control whether certain IMEs should show native UI components (such as the Candidate List) instead of suppressing them.
 *
 * The variable can be set to the following values:
 *   "0"       - Native UI components are not display. (default)
 *   "1"       - Native UI components are displayed.
 */
#define SDL_HINT_IME_SHOW_UI "SDL_IME_SHOW_UI"

/**
 * A variable controlling whether the home indicator bar on iPhone X
 *         should be hidden.
 *
 *  This variable can be set to the following values:
 *    "0"       - The indicator bar is not hidden (default for windowed applications)
 *    "1"       - The indicator bar is hidden and is shown when the screen is touched (useful for movie playback applications)
 *    "2"       - The indicator bar is dim and the first swipe makes it visible and the second swipe performs the "home" action (default for fullscreen applications)
 */
#define SDL_HINT_IOS_HIDE_HOME_INDICATOR "SDL_IOS_HIDE_HOME_INDICATOR"

/**
 *  A variable that lets you enable joystick (and gamecontroller) events even when your app is in the background.
 *
 *  The variable can be set to the following values:
 *    "0"       - Disable joystick & gamecontroller input events when the
 *                application is in the background.
 *    "1"       - Enable joystick & gamecontroller input events when the
 *                application is in the background.
 *
 *  The default value is "0".  This hint may be set at any time.
 */
#define SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"

/**
 *  A variable containing a list of arcade stick style controllers.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES "SDL_JOYSTICK_ARCADESTICK_DEVICES"

/**
 *  A variable containing a list of devices that are not arcade stick style controllers. This will override SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES_EXCLUDED "SDL_JOYSTICK_ARCADESTICK_DEVICES_EXCLUDED"

/**
 *  A variable containing a list of devices that should not be considerd joysticks.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_BLACKLIST_DEVICES "SDL_JOYSTICK_BLACKLIST_DEVICES"

/**
 *  A variable containing a list of devices that should be considered joysticks. This will override SDL_HINT_JOYSTICK_BLACKLIST_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_BLACKLIST_DEVICES_EXCLUDED "SDL_JOYSTICK_BLACKLIST_DEVICES_EXCLUDED"

/**
 *  A variable containing a list of flightstick style controllers.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES "SDL_JOYSTICK_FLIGHTSTICK_DEVICES"

/**
 *  A variable containing a list of devices that are not flightstick style controllers. This will override SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES_EXCLUDED "SDL_JOYSTICK_FLIGHTSTICK_DEVICES_EXCLUDED"

/**
 *  A variable containing a list of devices known to have a GameCube form factor.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_GAMECUBE_DEVICES "SDL_JOYSTICK_GAMECUBE_DEVICES"

/**
 *  A variable containing a list of devices known not to have a GameCube form factor. This will override SDL_HINT_JOYSTICK_GAMECUBE_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_GAMECUBE_DEVICES_EXCLUDED "SDL_JOYSTICK_GAMECUBE_DEVICES_EXCLUDED"

/**
 *  A variable controlling whether the HIDAPI joystick drivers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI drivers are not used
 *    "1"       - HIDAPI drivers are used (the default)
 *
 *  This variable is the default for all drivers, but can be overridden by the hints for specific drivers below.
 */
#define SDL_HINT_JOYSTICK_HIDAPI "SDL_JOYSTICK_HIDAPI"

/**
 *  A variable controlling whether the HIDAPI driver for Nintendo GameCube controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE "SDL_JOYSTICK_HIDAPI_GAMECUBE"

/**
 *  A variable controlling whether "low_frequency_rumble" and "high_frequency_rumble" is used to implement
 *          the GameCube controller's 3 rumble modes, Stop(0), Rumble(1), and StopHard(2)
 *          this is useful for applications that need full compatibility for things like ADSR envelopes.
 *          Stop is implemented by setting "low_frequency_rumble" to "0" and "high_frequency_rumble" ">0"
 *          Rumble is both at any arbitrary value,
 *          StopHard is implemented by setting both "low_frequency_rumble" and "high_frequency_rumble" to "0"
 *
 *  This variable can be set to the following values:
 *    "0"       - Normal rumble behavior is behavior is used (default)
 *    "1"       - Proper GameCube controller rumble behavior is used
 *
 */
#define SDL_HINT_JOYSTICK_GAMECUBE_RUMBLE_BRAKE "SDL_JOYSTICK_GAMECUBE_RUMBLE_BRAKE"

/**
  *  A variable controlling whether the HIDAPI driver for Nintendo Switch Joy-Cons should be used.
  *
  *  This variable can be set to the following values:
  *    "0"       - HIDAPI driver is not used
  *    "1"       - HIDAPI driver is used
  *
  *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
  */
#define SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS "SDL_JOYSTICK_HIDAPI_JOY_CONS"

/**
  *  A variable controlling whether Nintendo Switch Joy-Con controllers will be combined into a single Pro-like controller when using the HIDAPI driver
  *
  *  This variable can be set to the following values:
  *    "0"       - Left and right Joy-Con controllers will not be combined and each will be a mini-gamepad
  *    "1"       - Left and right Joy-Con controllers will be combined into a single controller (the default)
  */
#define SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS "SDL_JOYSTICK_HIDAPI_COMBINE_JOY_CONS"

/**
  *  A variable controlling whether Nintendo Switch Joy-Con controllers will be in vertical mode when using the HIDAPI driver
  *
  *  This variable can be set to the following values:
  *    "0"       - Left and right Joy-Con controllers will not be in vertical mode (the default)
  *    "1"       - Left and right Joy-Con controllers will be in vertical mode
  *
  *  This hint must be set before calling SDL_Init(SDL_INIT_GAMEPAD)
  */
#define SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS "SDL_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS"

/**
  *  A variable controlling whether the HIDAPI driver for Amazon Luna controllers connected via Bluetooth should be used.
  *
  *  This variable can be set to the following values:
  *    "0"       - HIDAPI driver is not used
  *    "1"       - HIDAPI driver is used
  *
  *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
  */
#define SDL_HINT_JOYSTICK_HIDAPI_LUNA "SDL_JOYSTICK_HIDAPI_LUNA"

/**
  *  A variable controlling whether the HIDAPI driver for Nintendo Online classic controllers should be used.
  *
  *  This variable can be set to the following values:
  *    "0"       - HIDAPI driver is not used
  *    "1"       - HIDAPI driver is used
  *
  *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
  */
#define SDL_HINT_JOYSTICK_HIDAPI_NINTENDO_CLASSIC "SDL_JOYSTICK_HIDAPI_NINTENDO_CLASSIC"

/**
  *  A variable controlling whether the HIDAPI driver for NVIDIA SHIELD controllers should be used.
  *
  *  This variable can be set to the following values:
  *    "0"       - HIDAPI driver is not used
  *    "1"       - HIDAPI driver is used
  *
  *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
  */
#define SDL_HINT_JOYSTICK_HIDAPI_SHIELD "SDL_JOYSTICK_HIDAPI_SHIELD"

/**
 *  A variable controlling whether the HIDAPI driver for PS3 controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI on macOS, and "0" on other platforms.
 *
 *  It is not possible to use this driver on Windows, due to limitations in the default drivers
 *  installed. See https://github.com/ViGEm/DsHidMini for an alternative driver on Windows.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS3 "SDL_JOYSTICK_HIDAPI_PS3"

/**
 *  A variable controlling whether the HIDAPI driver for PS4 controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS4 "SDL_JOYSTICK_HIDAPI_PS4"

/**
 *  A variable controlling whether extended input reports should be used for PS4 controllers when using the HIDAPI driver.
 *
 *  This variable can be set to the following values:
 *    "0"       - extended reports are not enabled (the default)
 *    "1"       - extended reports
 *
 *  Extended input reports allow rumble on Bluetooth PS4 controllers, but
 *  break DirectInput handling for applications that don't use SDL.
 *
 *  Once extended reports are enabled, they can not be disabled without
 *  power cycling the controller.
 *
 *  For compatibility with applications written for versions of SDL prior
 *  to the introduction of PS5 controller support, this value will also
 *  control the state of extended reports on PS5 controllers when the
 *  SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE hint is not explicitly set.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE "SDL_JOYSTICK_HIDAPI_PS4_RUMBLE"

/**
 *  A variable controlling whether the HIDAPI driver for PS5 controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS5 "SDL_JOYSTICK_HIDAPI_PS5"

/**
 *  A variable controlling whether the player LEDs should be lit to indicate which player is associated with a PS5 controller.
 *
 *  This variable can be set to the following values:
 *    "0"       - player LEDs are not enabled
 *    "1"       - player LEDs are enabled (the default)
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED "SDL_JOYSTICK_HIDAPI_PS5_PLAYER_LED"

/**
 *  A variable controlling whether extended input reports should be used for PS5 controllers when using the HIDAPI driver.
 *
 *  This variable can be set to the following values:
 *    "0"       - extended reports are not enabled (the default)
 *    "1"       - extended reports
 *
 *  Extended input reports allow rumble on Bluetooth PS5 controllers, but
 *  break DirectInput handling for applications that don't use SDL.
 *
 *  Once extended reports are enabled, they can not be disabled without
 *  power cycling the controller.
 *
 *  For compatibility with applications written for versions of SDL prior
 *  to the introduction of PS5 controller support, this value defaults to
 *  the value of SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE "SDL_JOYSTICK_HIDAPI_PS5_RUMBLE"

/**
 *  A variable controlling whether the HIDAPI driver for Google Stadia controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_STADIA "SDL_JOYSTICK_HIDAPI_STADIA"

/**
 *  A variable controlling whether the HIDAPI driver for Bluetooth Steam Controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used for Steam Controllers, which requires Bluetooth access
 *                and may prompt the user for permission on iOS and Android.
 *
 *  The default is "0"
 */
#define SDL_HINT_JOYSTICK_HIDAPI_STEAM "SDL_JOYSTICK_HIDAPI_STEAM"

/**
 *  A variable controlling whether the HIDAPI driver for the Steam Deck builtin controller should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK "SDL_JOYSTICK_HIDAPI_STEAMDECK"

/**
 *  A variable controlling whether the HIDAPI driver for Nintendo Switch controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_SWITCH "SDL_JOYSTICK_HIDAPI_SWITCH"

/**
 *  A variable controlling whether the Home button LED should be turned on when a Nintendo Switch Pro controller is opened
 *
 *  This variable can be set to the following values:
 *    "0"       - home button LED is turned off
 *    "1"       - home button LED is turned on
 *
 *  By default the Home button LED state is not changed. This hint can also be set to a floating point value between 0.0 and 1.0 which controls the brightness of the Home button LED.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED "SDL_JOYSTICK_HIDAPI_SWITCH_HOME_LED"

/**
 *  A variable controlling whether the Home button LED should be turned on when a Nintendo Switch Joy-Con controller is opened
 *
 *  This variable can be set to the following values:
 *    "0"       - home button LED is turned off
 *    "1"       - home button LED is turned on
 *
 *  By default the Home button LED state is not changed. This hint can also be set to a floating point value between 0.0 and 1.0 which controls the brightness of the Home button LED.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED "SDL_JOYSTICK_HIDAPI_JOYCON_HOME_LED"

/**
 *  A variable controlling whether the player LEDs should be lit to indicate which player is associated with a Nintendo Switch controller.
 *
 *  This variable can be set to the following values:
 *    "0"       - player LEDs are not enabled
 *    "1"       - player LEDs are enabled (the default)
 */
#define SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED "SDL_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED"

/**
 *  A variable controlling whether the HIDAPI driver for Nintendo Wii and Wii U controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  This driver doesn't work with the dolphinbar, so the default is SDL_FALSE for now.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_WII "SDL_JOYSTICK_HIDAPI_WII"

/**
 *  A variable controlling whether the player LEDs should be lit to indicate which player is associated with a Wii controller.
 *
 *  This variable can be set to the following values:
 *    "0"       - player LEDs are not enabled
 *    "1"       - player LEDs are enabled (the default)
 */
#define SDL_HINT_JOYSTICK_HIDAPI_WII_PLAYER_LED "SDL_JOYSTICK_HIDAPI_WII_PLAYER_LED"

/**
 *  A variable controlling whether the HIDAPI driver for XBox controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is "0" on Windows, otherwise the value of SDL_HINT_JOYSTICK_HIDAPI
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX   "SDL_JOYSTICK_HIDAPI_XBOX"

/**
 *  A variable controlling whether the HIDAPI driver for XBox 360 controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI_XBOX
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX_360   "SDL_JOYSTICK_HIDAPI_XBOX_360"

/**
 *  A variable controlling whether the player LEDs should be lit to indicate which player is associated with an Xbox 360 controller.
 *
 *  This variable can be set to the following values:
 *    "0"       - player LEDs are not enabled
 *    "1"       - player LEDs are enabled (the default)
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED "SDL_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED"

/**
 *  A variable controlling whether the HIDAPI driver for XBox 360 wireless controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI_XBOX_360
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_WIRELESS   "SDL_JOYSTICK_HIDAPI_XBOX_360_WIRELESS"

/**
 *  A variable controlling whether the HIDAPI driver for XBox One controllers should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - HIDAPI driver is not used
 *    "1"       - HIDAPI driver is used
 *
 *  The default is the value of SDL_HINT_JOYSTICK_HIDAPI_XBOX
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE   "SDL_JOYSTICK_HIDAPI_XBOX_ONE"

/**
 *  A variable controlling whether the Home button LED should be turned on when an Xbox One controller is opened
 *
 *  This variable can be set to the following values:
 *    "0"       - home button LED is turned off
 *    "1"       - home button LED is turned on
 *
 *  By default the Home button LED state is not changed. This hint can also be set to a floating point value between 0.0 and 1.0 which controls the brightness of the Home button LED. The default brightness is 0.4.
 */
#define SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED "SDL_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED"

/**
  *  A variable controlling whether IOKit should be used for controller handling.
  *
  *  This variable can be set to the following values:
  *    "0"       - IOKit is not used
  *    "1"       - IOKit is used (the default)
  */
#define SDL_HINT_JOYSTICK_IOKIT "SDL_JOYSTICK_IOKIT"

/**
  *  A variable controlling whether GCController should be used for controller handling.
  *
  *  This variable can be set to the following values:
  *    "0"       - GCController is not used
  *    "1"       - GCController is used (the default)
  */
#define SDL_HINT_JOYSTICK_MFI "SDL_JOYSTICK_MFI"

/**
  *  A variable controlling whether the RAWINPUT joystick drivers should be used for better handling XInput-capable devices.
  *
  *  This variable can be set to the following values:
  *    "0"       - RAWINPUT drivers are not used
  *    "1"       - RAWINPUT drivers are used (the default)
  */
#define SDL_HINT_JOYSTICK_RAWINPUT "SDL_JOYSTICK_RAWINPUT"

/**
  *  A variable controlling whether the RAWINPUT driver should pull correlated data from XInput.
  *
  *  This variable can be set to the following values:
  *    "0"       - RAWINPUT driver will only use data from raw input APIs
  *    "1"       - RAWINPUT driver will also pull data from XInput, providing
  *                better trigger axes, guide button presses, and rumble support
  *                for Xbox controllers
  *
  *  The default is "1".  This hint applies to any joysticks opened after setting the hint.
  */
#define SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT   "SDL_JOYSTICK_RAWINPUT_CORRELATE_XINPUT"

/**
  *  A variable controlling whether the ROG Chakram mice should show up as joysticks
  *
  *  This variable can be set to the following values:
  *    "0"       - ROG Chakram mice do not show up as joysticks (the default)
  *    "1"       - ROG Chakram mice show up as joysticks
  */
#define SDL_HINT_JOYSTICK_ROG_CHAKRAM "SDL_JOYSTICK_ROG_CHAKRAM"

/**
  *  A variable controlling whether a separate thread should be used
  *          for handling joystick detection and raw input messages on Windows
  *
  *  This variable can be set to the following values:
  *    "0"       - A separate thread is not used (the default)
  *    "1"       - A separate thread is used for handling raw input messages
  *
  */
#define SDL_HINT_JOYSTICK_THREAD "SDL_JOYSTICK_THREAD"

/**
 *  A variable containing a list of throttle style controllers.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_THROTTLE_DEVICES "SDL_JOYSTICK_THROTTLE_DEVICES"

/**
 *  A variable containing a list of devices that are not throttle style controllers. This will override SDL_HINT_JOYSTICK_THROTTLE_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_THROTTLE_DEVICES_EXCLUDED "SDL_JOYSTICK_THROTTLE_DEVICES_EXCLUDED"

/**
  *  A variable controlling whether Windows.Gaming.Input should be used for controller handling.
  *
  *  This variable can be set to the following values:
  *    "0"       - WGI is not used
  *    "1"       - WGI is used (the default)
  */
#define SDL_HINT_JOYSTICK_WGI "SDL_JOYSTICK_WGI"

/**
 *  A variable containing a list of wheel style controllers.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_WHEEL_DEVICES "SDL_JOYSTICK_WHEEL_DEVICES"

/**
 *  A variable containing a list of devices that are not wheel style controllers. This will override SDL_HINT_JOYSTICK_WHEEL_DEVICES and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_WHEEL_DEVICES_EXCLUDED "SDL_JOYSTICK_WHEEL_DEVICES_EXCLUDED"

/**
 *  A variable containing a list of devices known to have all axes centered at zero.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_JOYSTICK_ZERO_CENTERED_DEVICES "SDL_JOYSTICK_ZERO_CENTERED_DEVICES"

/**
 * Determines whether SDL enforces that DRM master is required in order
 *        to initialize the KMSDRM video backend.
 *
 * The DRM subsystem has a concept of a "DRM master" which is a DRM client that
 * has the ability to set planes, set cursor, etc. When SDL is DRM master, it
 * can draw to the screen using the SDL rendering APIs. Without DRM master, SDL
 * is still able to process input and query attributes of attached displays,
 * but it cannot change display state or draw to the screen directly.
 *
 * In some cases, it can be useful to have the KMSDRM backend even if it cannot
 * be used for rendering. An app may want to use SDL for input processing while
 * using another rendering API (such as an MMAL overlay on Raspberry Pi) or
 * using its own code to render to DRM overlays that SDL doesn't support.
 *
 * This hint must be set before initializing the video subsystem.
 *
 * This variable can be set to the following values:
 *    "0"       - SDL will allow usage of the KMSDRM backend without DRM master
 *    "1"       - SDL Will require DRM master to use the KMSDRM backend (default)
 */
#define SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER      "SDL_KMSDRM_REQUIRE_DRM_MASTER"

/**
  *  A comma separated list of devices to open as joysticks
  *
  *  This variable is currently only used by the Linux joystick driver.
  */
#define SDL_HINT_JOYSTICK_DEVICE "SDL_JOYSTICK_DEVICE"

/**
  *  A variable controlling whether joysticks on Linux will always treat 'hat' axis inputs (ABS_HAT0X - ABS_HAT3Y) as 8-way digital hats without checking whether they may be analog.
  *
  *  This variable can be set to the following values:
  *    "0"       - Only map hat axis inputs to digital hat outputs if the input axes appear to actually be digital (the default)
  *    "1"       - Always handle the input axes numbered ABS_HAT0X to ABS_HAT3Y as digital hats
  */
#define SDL_HINT_LINUX_DIGITAL_HATS "SDL_LINUX_DIGITAL_HATS"

/**
  *  A variable controlling whether digital hats on Linux will apply deadzones to their underlying input axes or use unfiltered values.
  *
  *  This variable can be set to the following values:
  *    "0"       - Return digital hat values based on unfiltered input axis values
  *    "1"       - Return digital hat values with deadzones on the input axes taken into account (the default)
  */
#define SDL_HINT_LINUX_HAT_DEADZONES "SDL_LINUX_HAT_DEADZONES"

/**
  *  A variable controlling whether to use the classic /dev/input/js* joystick interface or the newer /dev/input/event* joystick interface on Linux
  *
  *  This variable can be set to the following values:
  *    "0"       - Use /dev/input/event*
  *    "1"       - Use /dev/input/js*
  *
  *  By default the /dev/input/event* interfaces are used
  */
#define SDL_HINT_LINUX_JOYSTICK_CLASSIC "SDL_LINUX_JOYSTICK_CLASSIC"

/**
  *  A variable controlling whether joysticks on Linux adhere to their HID-defined deadzones or return unfiltered values.
  *
  *  This variable can be set to the following values:
  *    "0"       - Return unfiltered joystick axis values (the default)
  *    "1"       - Return axis values with deadzones taken into account
  */
#define SDL_HINT_LINUX_JOYSTICK_DEADZONES "SDL_LINUX_JOYSTICK_DEADZONES"

/**
 *  A variable controlling the default SDL log levels.
 *
 *  This variable is a comma separated set of category=level tokens that define the default logging levels for SDL applications.
 *
 *  The category can be a numeric category, one of "app", "error", "assert", "system", "audio", "video", "render", "input", "test", or `*` for any unspecified category.
 *
 *  The level can be a numeric level, one of "verbose", "debug", "info", "warn", "error", "critical", or "quiet" to disable that category.
 *
 *  You can omit the category if you want to set the logging level for all categories.
 *
 *  If this hint isn't set, the default log levels are equivalent to:
 *  "app=info,assert=warn,test=verbose,*=error"
 */
#define SDL_HINT_LOGGING   "SDL_LOGGING"

/**
*  When set don't force the SDL app to become a foreground process
*
*  This hint only applies to macOS.
*
*/
#define SDL_HINT_MAC_BACKGROUND_APP    "SDL_MAC_BACKGROUND_APP"

/**
 *  A variable that determines whether ctrl+click should generate a right-click event on Mac
 *
 *  If present, holding ctrl while left clicking will generate a right click
 *  event when on Mac.
 */
#define SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK "SDL_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK"

/**
 *  A variable controlling whether dispatching OpenGL context updates should block the dispatching thread until the main thread finishes processing
 *
 *  This variable can be set to the following values:
 *    "0"       - Dispatching OpenGL context updates will block the dispatching thread until the main thread finishes processing (default).
 *    "1"       - Dispatching OpenGL context updates will allow the dispatching thread to continue execution.
 *
 *  Generally you want the default, but if you have OpenGL code in a background thread on a Mac, and the main thread
 *  hangs because it's waiting for that background thread, but that background thread is also hanging because it's
 *  waiting for the main thread to do an update, this might fix your issue.
 *
 *  This hint only applies to macOS.
 *
 *  This hint is available since SDL 2.24.0.
 *
 */
#define SDL_HINT_MAC_OPENGL_ASYNC_DISPATCH "SDL_MAC_OPENGL_ASYNC_DISPATCH"

/**
 *  A variable setting the double click radius, in pixels.
 */
#define SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS    "SDL_MOUSE_DOUBLE_CLICK_RADIUS"

/**
 *  A variable setting the double click time, in milliseconds.
 */
#define SDL_HINT_MOUSE_DOUBLE_CLICK_TIME    "SDL_MOUSE_DOUBLE_CLICK_TIME"

/**
 *  Allow mouse click events when clicking to focus an SDL window
 *
 *  This variable can be set to the following values:
 *    "0"       - Ignore mouse clicks that activate a window
 *    "1"       - Generate events for mouse clicks that activate a window
 *
 *  By default SDL will ignore mouse clicks that activate a window
 */
#define SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH "SDL_MOUSE_FOCUS_CLICKTHROUGH"

/**
 *  A variable setting the speed scale for mouse motion, in floating point, when the mouse is not in relative mode
 */
#define SDL_HINT_MOUSE_NORMAL_SPEED_SCALE    "SDL_MOUSE_NORMAL_SPEED_SCALE"

/**
 *  A variable controlling whether relative mouse mode constrains the mouse to the center of the window
 *
 *  This variable can be set to the following values:
 *    "0"       - Relative mouse mode constrains the mouse to the window
 *    "1"       - Relative mouse mode constrains the mouse to the center of the window
 *
 *  Constraining to the center of the window works better for FPS games and when the
 *  application is running over RDP. Constraining to the whole window works better
 *  for 2D games and increases the chance that the mouse will be in the correct
 *  position when using high DPI mice.
 *
 *  By default SDL will constrain the mouse to the center of the window
 */
#define SDL_HINT_MOUSE_RELATIVE_MODE_CENTER    "SDL_MOUSE_RELATIVE_MODE_CENTER"

/**
 *  A variable controlling whether relative mouse mode is implemented using mouse warping
 *
 *  This variable can be set to the following values:
 *    "0"       - Relative mouse mode uses raw input
 *    "1"       - Relative mouse mode uses mouse warping
 *
 *  By default SDL will use raw input for relative mouse mode
 */
#define SDL_HINT_MOUSE_RELATIVE_MODE_WARP    "SDL_MOUSE_RELATIVE_MODE_WARP"

/**
 *  A variable setting the scale for mouse motion, in floating point, when the mouse is in relative mode
 */
#define SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE    "SDL_MOUSE_RELATIVE_SPEED_SCALE"

/**
 *  A variable controlling whether the system mouse acceleration curve is used for relative mouse motion.
 *
 *  This variable can be set to the following values:
 *    "0"       - Relative mouse motion will be unscaled (the default)
 *    "1"       - Relative mouse motion will be scaled using the system mouse acceleration curve.
 *
 *    If SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE is set, that will override the system speed scale.
 */
#define SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE    "SDL_MOUSE_RELATIVE_SYSTEM_SCALE"

/**
 *  A variable controlling whether a motion event should be generated for mouse warping in relative mode.
 *
 *  This variable can be set to the following values:
 *    "0"       - Warping the mouse will not generate a motion event in relative mode
 *    "1"       - Warping the mouse will generate a motion event in relative mode
 *
 *  By default warping the mouse will not generate motion events in relative mode. This avoids the application having to filter out large relative motion due to warping.
 */
#define SDL_HINT_MOUSE_RELATIVE_WARP_MOTION  "SDL_MOUSE_RELATIVE_WARP_MOTION"

/**
 *  A variable controlling whether mouse events should generate synthetic touch events
 *
 *  This variable can be set to the following values:
 *    "0"       - Mouse events will not generate touch events (default for desktop platforms)
 *    "1"       - Mouse events will generate touch events (default for mobile platforms, such as Android and iOS)
 */
#define SDL_HINT_MOUSE_TOUCH_EVENTS    "SDL_MOUSE_TOUCH_EVENTS"

/**
 *  A variable controlling whether the mouse is captured while mouse buttons are pressed
 *
 *  This variable can be set to the following values:
 *    "0"       - The mouse is not captured while mouse buttons are pressed
 *    "1"       - The mouse is captured while mouse buttons are pressed
 *
 *  By default the mouse is captured while mouse buttons are pressed so if the mouse is dragged
 *  outside the window, the application continues to receive mouse events until the button is
 *  released.
 */
#define SDL_HINT_MOUSE_AUTO_CAPTURE    "SDL_MOUSE_AUTO_CAPTURE"

/**
 *  Treat pen movement as separate from mouse movement
 *
 *  By default, pens report both ::SDL_MouseMotionEvent and ::SDL_PenMotionEvent updates
 *  (analogously for button presses).  This hint allows decoupling mouse and pen updates.
 *
 *  This variable toggles between the following behaviour:
 *    "0"       - (Default) Pen acts as a mouse with mouse ID ::SDL_PEN_MOUSEID.
 *                Use case: client application is not pen aware, user wants to
 *                use pen instead of mouse to interact.
 *    "1"       - Pen reports mouse clicks and movement events but does not update
 *                SDL-internal mouse state (buttons pressed, current mouse location).
 *                Use case: client application is not pen aware, user frequently
 *                alternates between pen and "real" mouse.
 *    "2"       - Pen reports no mouse events.
 *                Use case: pen-aware client application uses this hint to allow user to
 *                toggle between pen+mouse mode ("2") and pen-only mode ("1" or "0").
 */
#define SDL_HINT_PEN_NOT_MOUSE    "SDL_HINT_PEN_NOT_MOUSE"

/**
 *  Pen mouse button emulation triggers only when the pen touches the tablet surface
 *
 *    "0"       - The pen reports mouse button press/release immediately when the pen
 *                button is pressed/released, and the pen tip touching the surface counts
 *                as left mouse button press.
 *    "1"       - (Default) Mouse button presses are sent when the pen first touches
 *                the tablet (analogously for releases).  Not pressing a pen button
 *                simulates mouse button 1, pressing the first pen button simulates
 *                mouse button 2 etc.;  it is not possible to report multiple buttons
 *                as pressed at the same time.
 */
#define SDL_HINT_PEN_DELAY_MOUSE_BUTTON    "SDL_HINT_PEN_DELAY_MOUSE_BUTTON"

/**
 *  Tell SDL not to catch the SIGINT or SIGTERM signals.
 *
 * This hint only applies to Unix-like platforms, and should set before
 * any calls to SDL_Init()
 *
 * The variable can be set to the following values:
 *   "0"       - SDL will install a SIGINT and SIGTERM handler, and when it
 *               catches a signal, convert it into an SDL_EVENT_QUIT event.
 *   "1"       - SDL will not install a signal handler at all.
 */
#define SDL_HINT_NO_SIGNAL_HANDLERS   "SDL_NO_SIGNAL_HANDLERS"

/**
 *  A variable controlling what driver to use for OpenGL ES contexts.
 *
 *  On some platforms, currently Windows and X11, OpenGL drivers may support
 *  creating contexts with an OpenGL ES profile. By default SDL uses these
 *  profiles, when available, otherwise it attempts to load an OpenGL ES
 *  library, e.g. that provided by the ANGLE project. This variable controls
 *  whether SDL follows this default behaviour or will always load an
 *  OpenGL ES library.
 *
 *  Circumstances where this is useful include
 *  - Testing an app with a particular OpenGL ES implementation, e.g ANGLE,
 *    or emulator, e.g. those from ARM, Imagination or Qualcomm.
 *  - Resolving OpenGL ES function addresses at link time by linking with
 *    the OpenGL ES library instead of querying them at run time with
 *    SDL_GL_GetProcAddress().
 *
 *  Caution: for an application to work with the default behaviour across
 *  different OpenGL drivers it must query the OpenGL ES function
 *  addresses at run time using SDL_GL_GetProcAddress().
 *
 *  This variable is ignored on most platforms because OpenGL ES is native
 *  or not supported.
 *
 *  This variable can be set to the following values:
 *    "0"       - Use ES profile of OpenGL, if available. (Default when not set.)
 *    "1"       - Load OpenGL ES library using the default library names.
 *
 */
#define SDL_HINT_OPENGL_ES_DRIVER   "SDL_OPENGL_ES_DRIVER"

/**
 *  A variable controlling which orientations are allowed on iOS/Android.
 *
 *  In some circumstances it is necessary to be able to explicitly control
 *  which UI orientations are allowed.
 *
 *  This variable is a space delimited list of the following values:
 *    "LandscapeLeft", "LandscapeRight", "Portrait" "PortraitUpsideDown"
 */
#define SDL_HINT_ORIENTATIONS "SDL_IOS_ORIENTATIONS"

/**
 *  A variable controlling the use of a sentinel event when polling the event queue
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable poll sentinels
 *    "1"       - Enable poll sentinels
 *
 *  When polling for events, SDL_PumpEvents is used to gather new events from devices.
 *  If a device keeps producing new events between calls to SDL_PumpEvents, a poll loop will
 *  become stuck until the new events stop.
 *  This is most noticeable when moving a high frequency mouse.
 *
 *  By default, poll sentinels are enabled.
 */
#define SDL_HINT_POLL_SENTINEL "SDL_POLL_SENTINEL"

/**
 *  Override for SDL_GetPreferredLocales()
 *
 *  If set, this will be favored over anything the OS might report for the
 *  user's preferred locales. Changing this hint at runtime will not generate
 *  a SDL_EVENT_LOCALE_CHANGED event (but if you can change the hint, you can push
 *  your own event, if you want).
 *
 *  The format of this hint is a comma-separated list of language and locale,
 *  combined with an underscore, as is a common format: "en_GB". Locale is
 *  optional: "en". So you might have a list like this: "en_GB,jp,es_PT"
 */
#define SDL_HINT_PREFERRED_LOCALES "SDL_PREFERRED_LOCALES"

/**
 *  A variable describing the content orientation on QtWayland-based platforms.
 *
 *  On QtWayland platforms, windows are rotated client-side to allow for custom
 *  transitions. In order to correctly position overlays (e.g. volume bar) and
 *  gestures (e.g. events view, close/minimize gestures), the system needs to
 *  know in which orientation the application is currently drawing its contents.
 *
 *  This does not cause the window to be rotated or resized, the application
 *  needs to take care of drawing the content in the right orientation (the
 *  framebuffer is always in portrait mode).
 *
 *  This variable can be one of the following values:
 *    "primary" (default), "portrait", "landscape", "inverted-portrait", "inverted-landscape"
 *
 *  Since SDL 2.0.22 this variable accepts a comma-separated list of values above.
 */
#define SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION "SDL_QTWAYLAND_CONTENT_ORIENTATION"

/**
 *  Flags to set on QtWayland windows to integrate with the native window manager.
 *
 *  On QtWayland platforms, this hint controls the flags to set on the windows.
 *  For example, on Sailfish OS "OverridesSystemGestures" disables swipe gestures.
 *
 *  This variable is a space-separated list of the following values (empty = no flags):
 *    "OverridesSystemGestures", "StaysOnTop", "BypassWindowManager"
 */
#define SDL_HINT_QTWAYLAND_WINDOW_FLAGS "SDL_QTWAYLAND_WINDOW_FLAGS"

/**
 *  A variable controlling how the 2D render API renders lines
 *
 *  This variable can be set to the following values:
 *    "0"     - Use the default line drawing method (Bresenham's line algorithm as of SDL 2.0.20)
 *    "1"     - Use the driver point API using Bresenham's line algorithm (correct, draws many points)
 *    "2"     - Use the driver line API (occasionally misses line endpoints based on hardware driver quirks, was the default before 2.0.20)
 *    "3"     - Use the driver geometry API (correct, draws thicker diagonal lines)
 *
 *  This variable should be set when the renderer is created.
 */
#define SDL_HINT_RENDER_LINE_METHOD "SDL_RENDER_LINE_METHOD"

/**
 *  A variable controlling whether to enable Direct3D 11+'s Debug Layer.
 *
 *  This variable does not have any effect on the Direct3D 9 based renderer.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable Debug Layer use
 *    "1"       - Enable Debug Layer use
 *
 *  By default, SDL does not use Direct3D Debug Layer.
 */
#define SDL_HINT_RENDER_DIRECT3D11_DEBUG    "SDL_RENDER_DIRECT3D11_DEBUG"

/**
 *  A variable controlling whether the Direct3D device is initialized for thread-safe operations.
 *
 *  This variable can be set to the following values:
 *    "0"       - Thread-safety is not enabled (faster)
 *    "1"       - Thread-safety is enabled
 *
 *  By default the Direct3D device is created with thread-safety disabled.
 */
#define SDL_HINT_RENDER_DIRECT3D_THREADSAFE "SDL_RENDER_DIRECT3D_THREADSAFE"

/**
 *  A variable specifying which render driver to use.
 *
 *  If the application doesn't pick a specific renderer to use, this variable
 *  specifies the name of the preferred renderer.  If the preferred renderer
 *  can't be initialized, the normal default renderer is used.
 *
 *  This variable is case insensitive and can be set to the following values:
 *    "direct3d"
 *    "direct3d11"
 *    "direct3d12"
 *    "opengl"
 *    "opengles2"
 *    "opengles"
 *    "metal"
 *    "software"
 *
 *  The default varies by platform, but it's the first one in the list that
 *  is available on the current platform.
 */
#define SDL_HINT_RENDER_DRIVER              "SDL_RENDER_DRIVER"

/**
 *  A variable controlling whether the OpenGL render driver uses shaders if they are available.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable shaders
 *    "1"       - Enable shaders
 *
 *  By default shaders are used if OpenGL supports them.
 */
#define SDL_HINT_RENDER_OPENGL_SHADERS      "SDL_RENDER_OPENGL_SHADERS"

/**
 *  A variable controlling the scaling quality
 *
 *  This variable can be set to the following values:
 *    "0" or "nearest" - Nearest pixel sampling
 *    "1" or "linear"  - Linear filtering (supported by OpenGL and Direct3D)
 *    "2" or "best"    - Currently this is the same as "linear"
 *
 *  By default nearest pixel sampling is used
 */
#define SDL_HINT_RENDER_SCALE_QUALITY       "SDL_RENDER_SCALE_QUALITY"

/**
 *  A variable controlling whether updates to the SDL screen surface should be synchronized with the vertical refresh, to avoid tearing.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable vsync
 *    "1"       - Enable vsync
 *
 *  By default SDL does not sync screen surface updates with vertical refresh.
 */
#define SDL_HINT_RENDER_VSYNC               "SDL_RENDER_VSYNC"

/**
 *  A variable controlling whether the Metal render driver select low power device over default one
 *
 *  This variable can be set to the following values:
 *    "0"       - Use the prefered OS device
 *    "1"       - Select a low power one
 *
 *  By default the prefered OS device is used.
 */
#define SDL_HINT_RENDER_METAL_PREFER_LOW_POWER_DEVICE "SDL_RENDER_METAL_PREFER_LOW_POWER_DEVICE"

/**
 *  A variable containing a list of ROG gamepad capable mice.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_ROG_GAMEPAD_MICE "SDL_ROG_GAMEPAD_MICE"

/**
 *  A variable containing a list of devices that are not ROG gamepad capable mice. This will override SDL_HINT_ROG_GAMEPAD_MICE and the built in device list.
 *
 *  The format of the string is a comma separated list of USB VID/PID pairs
 *  in hexadecimal form, e.g.
 *
 *      0xAAAA/0xBBBB,0xCCCC/0xDDDD
 *
 *  The variable can also take the form of @file, in which case the named
 *  file will be loaded and interpreted as the value of the variable.
 */
#define SDL_HINT_ROG_GAMEPAD_MICE_EXCLUDED "SDL_ROG_GAMEPAD_MICE_EXCLUDED"

/**
 *  A variable controlling if VSYNC is automatically disable if doesn't reach the enough FPS
 *
 *  This variable can be set to the following values:
 *    "0"       - It will be using VSYNC as defined in the main flag. Default
 *    "1"       - If VSYNC was previously enabled, then it will disable VSYNC if doesn't reach enough speed
 *
 *  By default SDL does not enable the automatic VSYNC
 */
#define SDL_HINT_PS2_DYNAMIC_VSYNC    "SDL_PS2_DYNAMIC_VSYNC"

/**
 * A variable to control whether the return key on the soft keyboard
 *        should hide the soft keyboard on Android and iOS.
 *
 * The variable can be set to the following values:
 *   "0"       - The return key will be handled as a key event. This is the behaviour of SDL <= 2.0.3. (default)
 *   "1"       - The return key will hide the keyboard.
 *
 * The value of this hint is used at runtime, so it can be changed at any time.
 */
#define SDL_HINT_RETURN_KEY_HIDES_IME "SDL_RETURN_KEY_HIDES_IME"

/**
 * Tell SDL which Dispmanx layer to use on a Raspberry PI
 *
 * Also known as Z-order. The variable can take a negative or positive value.
 * The default is 10000.
 */
#define SDL_HINT_RPI_VIDEO_LAYER           "SDL_RPI_VIDEO_LAYER"

/**
 *  Specify an "activity name" for screensaver inhibition.
 *
 * Some platforms, notably Linux desktops, list the applications which are
 * inhibiting the screensaver or other power-saving features.
 *
 * This hint lets you specify the "activity name" sent to the OS when
 * SDL_DisableScreenSaver() is used (or the screensaver is automatically
 * disabled). The contents of this hint are used when the screensaver is
 * disabled. You should use a string that describes what your program is doing
 * (and, therefore, why the screensaver is disabled).  For example, "Playing a
 * game" or "Watching a video".
 *
 * Setting this to "" or leaving it unset will have SDL use a reasonable
 * default: "Playing a game" or something similar.
 *
 * On targets where this is not supported, this hint does nothing.
 */
#define SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME "SDL_SCREENSAVER_INHIBIT_ACTIVITY_NAME"

/**
 *  Specifies whether SDL_THREAD_PRIORITY_TIME_CRITICAL should be treated as realtime.
 *
 *  On some platforms, like Linux, a realtime priority thread may be subject to restrictions
 *  that require special handling by the application. This hint exists to let SDL know that
 *  the app is prepared to handle said restrictions.
 *
 *  On Linux, SDL will apply the following configuration to any thread that becomes realtime:
 *   * The SCHED_RESET_ON_FORK bit will be set on the scheduling policy,
 *   * An RLIMIT_RTTIME budget will be configured to the rtkit specified limit.
 *     * Exceeding this limit will result in the kernel sending SIGKILL to the app,
 *     * Refer to the man pages for more information.
 *
 *  This variable can be set to the following values:
 *    "0"       - default platform specific behaviour
 *    "1"       - Force SDL_THREAD_PRIORITY_TIME_CRITICAL to a realtime scheduling policy
 */
#define SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL "SDL_THREAD_FORCE_REALTIME_TIME_CRITICAL"

/**
*  A string specifying additional information to use with SDL_SetThreadPriority.
*
*  By default SDL_SetThreadPriority will make appropriate system changes in order to
*  apply a thread priority.  For example on systems using pthreads the scheduler policy
*  is changed automatically to a policy that works well with a given priority.
*  Code which has specific requirements can override SDL's default behavior with this hint.
*
*  pthread hint values are "current", "other", "fifo" and "rr".
*  Currently no other platform hint values are defined but may be in the future.
*
*  \note On Linux, the kernel may send SIGKILL to realtime tasks which exceed the distro
*  configured execution budget for rtkit. This budget can be queried through RLIMIT_RTTIME
*  after calling SDL_SetThreadPriority().
*/
#define SDL_HINT_THREAD_PRIORITY_POLICY         "SDL_THREAD_PRIORITY_POLICY"

/**
*  A string specifying SDL's threads stack size in bytes or "0" for the backend's default size
*
*  Use this hint in case you need to set SDL's threads stack size to other than the default.
*  This is specially useful if you build SDL against a non glibc libc library (such as musl) which
*  provides a relatively small default thread stack size (a few kilobytes versus the default 8MB glibc uses).
*  Support for this hint is currently available only in the pthread, Windows, and PSP backend.
*
*  Instead of this hint, in 2.0.9 and later, you can use
*  SDL_CreateThreadWithStackSize(). This hint only works with the classic
*  SDL_CreateThread().
*/
#define SDL_HINT_THREAD_STACK_SIZE              "SDL_THREAD_STACK_SIZE"

/**
 *  A variable that controls the timer resolution, in milliseconds.
 *
 *  The higher resolution the timer, the more frequently the CPU services
 *  timer interrupts, and the more precise delays are, but this takes up
 *  power and CPU time.  This hint is only used on Windows.
 *
 *  See this blog post for more information:
 *  http://randomascii.wordpress.com/2013/07/08/windows-timer-resolution-megawatts-wasted/
 *
 *  If this variable is set to "0", the system timer resolution is not set.
 *
 *  The default value is "1". This hint may be set at any time.
 */
#define SDL_HINT_TIMER_RESOLUTION "SDL_TIMER_RESOLUTION"

/**
 *  A variable controlling whether touch events should generate synthetic mouse events
 *
 *  This variable can be set to the following values:
 *    "0"       - Touch events will not generate mouse events
 *    "1"       - Touch events will generate mouse events
 *
 *  By default SDL will generate mouse events for touch events
 */
#define SDL_HINT_TOUCH_MOUSE_EVENTS    "SDL_TOUCH_MOUSE_EVENTS"

/**
 *  A variable controlling which touchpad should generate synthetic mouse events
 *
 *  This variable can be set to the following values:
 *    "0"       - Only front touchpad should generate mouse events. Default
 *    "1"       - Only back touchpad should generate mouse events.
 *    "2"       - Both touchpads should generate mouse events.
 *
 *  By default SDL will generate mouse events for all touch devices
 */
#define SDL_HINT_VITA_TOUCH_MOUSE_DEVICE    "SDL_HINT_VITA_TOUCH_MOUSE_DEVICE"

/**
 *  A variable controlling whether the Android / tvOS remotes
 *  should be listed as joystick devices, instead of sending keyboard events.
 *
 *  This variable can be set to the following values:
 *    "0"       - Remotes send enter/escape/arrow key events
 *    "1"       - Remotes are available as 2 axis, 2 button joysticks (the default).
 */
#define SDL_HINT_TV_REMOTE_AS_JOYSTICK "SDL_TV_REMOTE_AS_JOYSTICK"

/**
 *  A variable controlling whether the screensaver is enabled.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable screensaver
 *    "1"       - Enable screensaver
 *
 *  By default SDL will disable the screensaver.
 */
#define SDL_HINT_VIDEO_ALLOW_SCREENSAVER    "SDL_VIDEO_ALLOW_SCREENSAVER"

/**
 * Tell the video driver that we only want a double buffer.
 *
 * By default, most lowlevel 2D APIs will use a triple buffer scheme that
 * wastes no CPU time on waiting for vsync after issuing a flip, but
 * introduces a frame of latency. On the other hand, using a double buffer
 * scheme instead is recommended for cases where low latency is an important
 * factor because we save a whole frame of latency.
 * We do so by waiting for vsync immediately after issuing a flip, usually just
 * after eglSwapBuffers call in the backend's *_SwapWindow function.
 *
 * Since it's driver-specific, it's only supported where possible and
 * implemented. Currently supported the following drivers:
 *
 * - KMSDRM (kmsdrm)
 * - Raspberry Pi (raspberrypi)
 */
#define SDL_HINT_VIDEO_DOUBLE_BUFFER      "SDL_VIDEO_DOUBLE_BUFFER"

/**
 * If eglGetPlatformDisplay fails, fall back to calling eglGetDisplay.
 *
 * This variable can be set to one of the following values:
 *   "0"        - Do not fall back to eglGetDisplay
 *   "1"        - Fall back to eglGetDisplay if eglGetPlatformDisplay fails.
 *
 * By default, SDL will fall back to eglGetDisplay if eglGetPlatformDisplay
 * fails.
 */
#define SDL_HINT_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK "SDL_VIDEO_EGL_GETDISPLAY_FALLBACK"

/**
 * A variable controlling whether the graphics context is externally managed.
 *
 * This variable can be set to the following values:
 *  "0"         - SDL will manage graphics contexts that are attached to windows.
 *  "1"         - Disable graphics context management on windows.
 *
 * By default SDL will manage OpenGL contexts in certain situations. For example, on Android the
 * context will be automatically saved and restored when pausing the application. Additionally, some
 * platforms will assume usage of OpenGL if Vulkan isn't used. Setting this to "1" will prevent this
 * behavior, which is desirable when the application manages the graphics context, such as
 * an externally managed OpenGL context or attaching a Vulkan surface to the window.
 */
#define SDL_HINT_VIDEO_EXTERNAL_CONTEXT    "SDL_VIDEO_EXTERNAL_CONTEXT"

/**
 *  A variable that dictates policy for fullscreen Spaces on macOS.
 *
 *  This hint only applies to macOS.
 *
 *  The variable can be set to the following values:
 *    "0"       - Disable Spaces support (FULLSCREEN_DESKTOP won't use them and
 *                SDL_WINDOW_RESIZABLE windows won't offer the "fullscreen"
 *                button on their titlebars).
 *    "1"       - Enable Spaces support (FULLSCREEN_DESKTOP will use them and
 *                SDL_WINDOW_RESIZABLE windows will offer the "fullscreen"
 *                button on their titlebars).
 *
 *  The default value is "1". This hint must be set before any windows are created.
 */
#define SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES    "SDL_VIDEO_MAC_FULLSCREEN_SPACES"

/**
 *  Minimize your SDL_Window if it loses key focus when in fullscreen mode. Defaults to false.
 *  \warning  Before SDL 2.0.14, this defaulted to true! In 2.0.14, we're
 *            seeing if "true" causes more problems than it solves in modern times.
 *
 */
#define SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS   "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"

/**
 *  A variable controlling whether the libdecor Wayland backend is allowed to be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - libdecor use is disabled.
 *    "1"       - libdecor use is enabled (default).
 *
 *  libdecor is used over xdg-shell when xdg-decoration protocol is unavailable.
 */
#define SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR "SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR"

/**
 *  A variable controlling whether the libdecor Wayland backend is preferred over native decrations.
 *
 *  When this hint is set, libdecor will be used to provide window decorations, even if xdg-decoration is
 *  available. (Note that, by default, libdecor will use xdg-decoration itself if available).
 *
 *  This variable can be set to the following values:
 *    "0"       - libdecor is enabled only if server-side decorations are unavailable.
 *    "1"       - libdecor is always enabled if available.
 *
 *  libdecor is used over xdg-shell when xdg-decoration protocol is unavailable.
 */
#define SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR "SDL_VIDEO_WAYLAND_PREFER_LIBDECOR"

/**
 *  A variable controlling whether video mode emulation is enabled under Wayland.
 *
 *  When this hint is set, a standard set of emulated CVT video modes will be exposed for use by the application.
 *  If it is disabled, the only modes exposed will be the logical desktop size and, in the case of a scaled
 *  desktop, the native display resolution.
 *
 *  This variable can be set to the following values:
 *    "0"       - Video mode emulation is disabled.
 *    "1"       - Video mode emulation is enabled.
 *
 *  By default video mode emulation is enabled.
 */
#define SDL_HINT_VIDEO_WAYLAND_MODE_EMULATION "SDL_VIDEO_WAYLAND_MODE_EMULATION"

/**
 *  A variable controlling how modes with a non-native aspect ratio are displayed under Wayland.
 *
 *  When this hint is set, the requested scaling will be used when displaying fullscreen video modes
 *  that don't match the display's native aspect ratio. This is contingent on compositor viewport support.
 *
 *  This variable can be set to the following values:
 *    "aspect"       - Video modes will be displayed scaled, in their proper aspect ratio, with black bars.
 *    "stretch"      - Video modes will be scaled to fill the entire display.
 *    "none"         - Video modes will be displayed as 1:1 with no scaling.
 *
 *  By default 'stretch' is used.
 */
#define SDL_HINT_VIDEO_WAYLAND_MODE_SCALING "SDL_VIDEO_WAYLAND_MODE_SCALING"

/**
 *  A variable forcing non-DPI-aware Wayland windows to output at 1:1 scaling.
 *
 *  When this hint is set, Wayland windows that are not flagged as being DPI-aware will be
 *  output with scaling designed to force 1:1 pixel mapping.
 *
 *  This is intended to allow legacy applications to be displayed without desktop scaling
 *  being applied, and has issues with certain display configurations, as this forces the
 *  window to behave in a way that Wayland desktops were not designed to accommodate:
 *
 *   - Rounding errors can result with odd window sizes and/or desktop scales.
 *   - The window may be unusably small.
 *   - The window may jump in size at times.
 *   - The window may appear to be larger than the desktop size to the application.
 *   - Possible loss of cursor precision.
 *
 *  New applications should be designed with proper DPI awareness handling instead of enabling this.
 *
 *  This variable can be set to the following values:
 *    "0"       - Windows will be scaled normally.
 *    "1"       - Windows will be forced to scale to achieve 1:1 output.
 *
 *  By default, scaling to the display is disabled.
 */
#define SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY "SDL_VIDEO_WAYLAND_SCALE_TO_DISPLAY"

/**
 *  Enable or disable mouse pointer warp emulation, needed by some older games.
 *
 *  When this hint is set, any SDL will emulate mouse warps using relative mouse mode.
 *  This is required for some older games (such as Source engine games), which warp the
 *  mouse to the centre of the screen rather than using relative mouse motion. Note that
 *  relative mouse mode may have different mouse acceleration behaviour than pointer warps.
 *
 *  This variable can be set to the following values:
 *    "0"       - All mouse warps fail, as mouse warping is not available under wayland.
 *    "1"       - Some mouse warps will be emulated by forcing relative mouse mode.
 *
 *  If not set, this is automatically enabled unless an application uses relative mouse
 *  mode directly.
 */
#define SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP "SDL_VIDEO_WAYLAND_EMULATE_MOUSE_WARP"

/**
*  Set whether all window operations will block until complete.
*
*  Window systems that run asynchronously may not have the results of window operations that resize or move the window
*  applied immediately upon the return of the requesting function. Setting this hint will cause such operations to block
*  after every call until the pending operation has completed. Setting this to '1' is the equivalent of calling
*  SDL_SyncWindow() after every function call.
*
*  Be aware that amount of time spent blocking while waiting for window operations to complete can be quite lengthy, as
*  animations may have to complete, which can take upwards of multiple seconds in some cases.
*
*  This variable can be set to the following values:
*  "0" - Window operations are non-blocking
*  "1" - Window operations will block until completed
*
*  By default SDL will run in non-blocking mode
 */
#define SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS "SDL_VIDEO_SYNC_WINDOW_OPERATIONS"

/**
*  A variable specifying which shader compiler to preload when using the Chrome ANGLE binaries
*
*  SDL has EGL and OpenGL ES2 support on Windows via the ANGLE project. It
*  can use two different sets of binaries, those compiled by the user from source
*  or those provided by the Chrome browser. In the later case, these binaries require
*  that SDL loads a DLL providing the shader compiler.
*
*  This variable can be set to the following values:
*    "d3dcompiler_46.dll" - default, best for Vista or later.
*    "d3dcompiler_43.dll" - for XP support.
*    "none" - do not load any library, useful if you compiled ANGLE from source and included the compiler in your binaries.
*
*/
#define SDL_HINT_VIDEO_WIN_D3DCOMPILER              "SDL_VIDEO_WIN_D3DCOMPILER"

/**
 * Set whether the OpenGL context should be created with EGL by default
 *
 * This variable can be set to the following values:
 * "0" - Use platform-specific GL context creation API (GLX, WGL, CGL, etc)
 * "1" - Use EGL
 *
 * By default SDL will use the platform-specific GL context API when both are present.
 */
#define SDL_HINT_VIDEO_FORCE_EGL "SDL_VIDEO_FORCE_EGL"

/**
 * A variable controlling whether the X11 _NET_WM_BYPASS_COMPOSITOR hint should be used.
 *
 * This variable can be set to the following values:
 * "0" - Disable _NET_WM_BYPASS_COMPOSITOR
 * "1" - Enable _NET_WM_BYPASS_COMPOSITOR
 *
 * By default SDL will use _NET_WM_BYPASS_COMPOSITOR
 *
 */
#define SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"

/**
 *  A variable controlling whether the X11 _NET_WM_PING protocol should be supported.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable _NET_WM_PING
 *    "1"       - Enable _NET_WM_PING
 *
 *  By default SDL will use _NET_WM_PING, but for applications that know they
 *  will not always be able to respond to ping requests in a timely manner they can
 *  turn it off to avoid the window manager thinking the app is hung.
 *  The hint is checked in CreateWindow.
 */
#define SDL_HINT_VIDEO_X11_NET_WM_PING      "SDL_VIDEO_X11_NET_WM_PING"

/**
 *  A variable forcing the visual ID chosen for new X11 windows
 *
 */
#define SDL_HINT_VIDEO_X11_WINDOW_VISUALID      "SDL_VIDEO_X11_WINDOW_VISUALID"

/**
 *  A variable forcing the scaling factor for X11 windows
 *
 *  This variable can be set to a floating point value in the range 1.0-10.0f
 */
#define SDL_HINT_VIDEO_X11_SCALING_FACTOR      "SDL_VIDEO_X11_SCALING_FACTOR"

/**
 *  A variable controlling whether the X11 XRandR extension should be used.
 *
 *  This variable can be set to the following values:
 *    "0"       - Disable XRandR
 *    "1"       - Enable XRandR
 *
 *  By default SDL will use XRandR.
 */
#define SDL_HINT_VIDEO_X11_XRANDR           "SDL_VIDEO_X11_XRANDR"

/**
 *  Controls how the fact chunk affects the loading of a WAVE file.
 *
 *  The fact chunk stores information about the number of samples of a WAVE
 *  file. The Standards Update from Microsoft notes that this value can be used
 *  to 'determine the length of the data in seconds'. This is especially useful
 *  for compressed formats (for which this is a mandatory chunk) if they produce
 *  multiple sample frames per block and truncating the block is not allowed.
 *  The fact chunk can exactly specify how many sample frames there should be
 *  in this case.
 *
 *  Unfortunately, most application seem to ignore the fact chunk and so SDL
 *  ignores it by default as well.
 *
 *  This variable can be set to the following values:
 *
 *    "truncate"    - Use the number of samples to truncate the wave data if
 *                    the fact chunk is present and valid
 *    "strict"      - Like "truncate", but raise an error if the fact chunk
 *                    is invalid, not present for non-PCM formats, or if the
 *                    data chunk doesn't have that many samples
 *    "ignorezero"  - Like "truncate", but ignore fact chunk if the number of
 *                    samples is zero
 *    "ignore"      - Ignore fact chunk entirely (default)
 */
#define SDL_HINT_WAVE_FACT_CHUNK   "SDL_WAVE_FACT_CHUNK"

/**
 *  Controls how the size of the RIFF chunk affects the loading of a WAVE file.
 *
 *  The size of the RIFF chunk (which includes all the sub-chunks of the WAVE
 *  file) is not always reliable. In case the size is wrong, it's possible to
 *  just ignore it and step through the chunks until a fixed limit is reached.
 *
 *  Note that files that have trailing data unrelated to the WAVE file or
 *  corrupt files may slow down the loading process without a reliable boundary.
 *  By default, SDL stops after 10000 chunks to prevent wasting time. Use the
 *  environment variable SDL_WAVE_CHUNK_LIMIT to adjust this value.
 *
 *  This variable can be set to the following values:
 *
 *    "force"        - Always use the RIFF chunk size as a boundary for the chunk search
 *    "ignorezero"   - Like "force", but a zero size searches up to 4 GiB (default)
 *    "ignore"       - Ignore the RIFF chunk size and always search up to 4 GiB
 *    "maximum"      - Search for chunks until the end of file (not recommended)
 */
#define SDL_HINT_WAVE_RIFF_CHUNK_SIZE   "SDL_WAVE_RIFF_CHUNK_SIZE"

/**
 *  Controls how a truncated WAVE file is handled.
 *
 *  A WAVE file is considered truncated if any of the chunks are incomplete or
 *  the data chunk size is not a multiple of the block size. By default, SDL
 *  decodes until the first incomplete block, as most applications seem to do.
 *
 *  This variable can be set to the following values:
 *
 *    "verystrict" - Raise an error if the file is truncated
 *    "strict"     - Like "verystrict", but the size of the RIFF chunk is ignored
 *    "dropframe"  - Decode until the first incomplete sample frame
 *    "dropblock"  - Decode until the first incomplete block (default)
 */
#define SDL_HINT_WAVE_TRUNCATION   "SDL_WAVE_TRUNCATION"

/**
 *  Controls whether menus can be opened with their keyboard shortcut (Alt+mnemonic).
 *
 *  If the mnemonics are enabled, then menus can be opened by pressing the Alt
 *  key and the corresponding mnemonic (for example, Alt+F opens the File menu).
 *  However, in case an invalid mnemonic is pressed, Windows makes an audible
 *  beep to convey that nothing happened. This is true even if the window has
 *  no menu at all!
 *
 *  Because most SDL applications don't have menus, and some want to use the Alt
 *  key for other purposes, SDL disables mnemonics (and the beeping) by default.
 *
 *  Note: This also affects keyboard events: with mnemonics enabled, when a
 *  menu is opened from the keyboard, you will not receive a KEYUP event for
 *  the mnemonic key, and *might* not receive one for Alt.
 *
 *  This variable can be set to the following values:
 *    "0"       - Alt+mnemonic does nothing, no beeping. (default)
 *    "1"       - Alt+mnemonic opens menus, invalid mnemonics produce a beep.
 */
#define SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS "SDL_WINDOWS_ENABLE_MENU_MNEMONICS"

/**
 *  A variable controlling whether the windows message loop is processed by SDL
 *
 *  This variable can be set to the following values:
 *    "0"       - The window message loop is not run
 *    "1"       - The window message loop is processed in SDL_PumpEvents()
 *
 *  By default SDL will process the windows message loop
 */
#define SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP "SDL_WINDOWS_ENABLE_MESSAGELOOP"

/**
 * Force SDL to use Critical Sections for mutexes on Windows.
 *        On Windows 7 and newer, Slim Reader/Writer Locks are available.
 *        They offer better performance, allocate no kernel resources and
 *        use less memory. SDL will fall back to Critical Sections on older
 *        OS versions or if forced to by this hint.
 *
 *  This variable can be set to the following values:
 *    "0"       - Use SRW Locks when available. If not, fall back to Critical Sections. (default)
 *    "1"       - Force the use of Critical Sections in all cases.
 *
 */
#define SDL_HINT_WINDOWS_FORCE_MUTEX_CRITICAL_SECTIONS "SDL_WINDOWS_FORCE_MUTEX_CRITICAL_SECTIONS"

/**
 * Force SDL to use Kernel Semaphores on Windows.
 *        Kernel Semaphores are inter-process and require a context
 *        switch on every interaction. On Windows 8 and newer, the
 *        WaitOnAddress API is available. Using that and atomics to
 *        implement semaphores increases performance.
 *        SDL will fall back to Kernel Objects on older OS versions
 *        or if forced to by this hint.
 *
 *  This variable can be set to the following values:
 *    "0"       - Use Atomics and WaitOnAddress API when available. If not, fall back to Kernel Objects. (default)
 *    "1"       - Force the use of Kernel Objects in all cases.
 *
 */
#define SDL_HINT_WINDOWS_FORCE_SEMAPHORE_KERNEL "SDL_WINDOWS_FORCE_SEMAPHORE_KERNEL"

/**
 * A variable to specify custom icon resource id from RC file on Windows platform
 */
#define SDL_HINT_WINDOWS_INTRESOURCE_ICON       "SDL_WINDOWS_INTRESOURCE_ICON"
#define SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL "SDL_WINDOWS_INTRESOURCE_ICON_SMALL"

/**
 *  Tell SDL not to generate window-close events for Alt+F4 on Windows.
 *
 * The variable can be set to the following values:
 *   "0"       - SDL will generate a window-close event when it sees Alt+F4.
 *   "1"       - SDL will only do normal key handling for Alt+F4.
 */
#define SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4 "SDL_WINDOWS_NO_CLOSE_ON_ALT_F4"

/**
 * Use the D3D9Ex API introduced in Windows Vista, instead of normal D3D9.
 *        Direct3D 9Ex contains changes to state management that can eliminate device
 *        loss errors during scenarios like Alt+Tab or UAC prompts. D3D9Ex may require
 *        some changes to your application to cope with the new behavior, so this
 *        is disabled by default.
 *
 *  This hint must be set before initializing the video subsystem.
 *
 *  For more information on Direct3D 9Ex, see:
 *    - https://docs.microsoft.com/en-us/windows/win32/direct3darticles/graphics-apis-in-windows-vista#direct3d-9ex
 *    - https://docs.microsoft.com/en-us/windows/win32/direct3darticles/direct3d-9ex-improvements
 *
 *  This variable can be set to the following values:
 *    "0"       - Use the original Direct3D 9 API (default)
 *    "1"       - Use the Direct3D 9Ex API on Vista and later (and fall back if D3D9Ex is unavailable)
 *
 */
#define SDL_HINT_WINDOWS_USE_D3D9EX "SDL_WINDOWS_USE_D3D9EX"

/**
 *  A variable controlling whether the window frame and title bar are interactive when the cursor is hidden
 *
 *  This variable can be set to the following values:
 *    "0"       - The window frame is not interactive when the cursor is hidden (no move, resize, etc)
 *    "1"       - The window frame is interactive when the cursor is hidden
 *
 *  By default SDL will allow interaction with the window frame when the cursor is hidden
 */
#define SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN    "SDL_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN"

/**
*  A variable controlling whether the window is activated when the SDL_ShowWindow function is called
*
*  This variable can be set to the following values:
*    "0"       - The window is not activated when the SDL_ShowWindow function is called
*    "1"       - The window is activated when the SDL_ShowWindow function is called
*
*  By default SDL will activate the window when the SDL_ShowWindow function is called
*/
#define SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN    "SDL_WINDOW_ACTIVATE_WHEN_SHOWN"

/** Allows back-button-press events on Windows Phone to be marked as handled
 *
 *  Windows Phone devices typically feature a Back button.  When pressed,
 *  the OS will emit back-button-press events, which apps are expected to
 *  handle in an appropriate manner.  If apps do not explicitly mark these
 *  events as 'Handled', then the OS will invoke its default behavior for
 *  unhandled back-button-press events, which on Windows Phone 8 and 8.1 is to
 *  terminate the app (and attempt to switch to the previous app, or to the
 *  device's home screen).
 *
 *  Setting the SDL_HINT_WINRT_HANDLE_BACK_BUTTON hint to "1" will cause SDL
 *  to mark back-button-press events as Handled, if and when one is sent to
 *  the app.
 *
 *  Internally, Windows Phone sends back button events as parameters to
 *  special back-button-press callback functions.  Apps that need to respond
 *  to back-button-press events are expected to register one or more
 *  callback functions for such, shortly after being launched (during the
 *  app's initialization phase).  After the back button is pressed, the OS
 *  will invoke these callbacks.  If the app's callback(s) do not explicitly
 *  mark the event as handled by the time they return, or if the app never
 *  registers one of these callback, the OS will consider the event
 *  un-handled, and it will apply its default back button behavior (terminate
 *  the app).
 *
 *  SDL registers its own back-button-press callback with the Windows Phone
 *  OS.  This callback will emit a pair of SDL key-press events (SDL_EVENT_KEY_DOWN
 *  and SDL_EVENT_KEY_UP), each with a scancode of SDL_SCANCODE_AC_BACK, after which
 *  it will check the contents of the hint, SDL_HINT_WINRT_HANDLE_BACK_BUTTON.
 *  If the hint's value is set to "1", the back button event's Handled
 *  property will get set to 'true'.  If the hint's value is set to something
 *  else, or if it is unset, SDL will leave the event's Handled property
 *  alone.  (By default, the OS sets this property to 'false', to note.)
 *
 *  SDL apps can either set SDL_HINT_WINRT_HANDLE_BACK_BUTTON well before a
 *  back button is pressed, or can set it in direct-response to a back button
 *  being pressed.
 *
 *  In order to get notified when a back button is pressed, SDL apps should
 *  register a callback function with SDL_AddEventWatch(), and have it listen
 *  for SDL_EVENT_KEY_DOWN events that have a scancode of SDL_SCANCODE_AC_BACK.
 *  (Alternatively, SDL_EVENT_KEY_UP events can be listened-for.  Listening for
 *  either event type is suitable.)  Any value of SDL_HINT_WINRT_HANDLE_BACK_BUTTON
 *  set by such a callback, will be applied to the OS' current
 *  back-button-press event.
 *
 *  More details on back button behavior in Windows Phone apps can be found
 *  at the following page, on Microsoft's developer site:
 *  http://msdn.microsoft.com/en-us/library/windowsphone/develop/jj247550(v=vs.105).aspx
 */
#define SDL_HINT_WINRT_HANDLE_BACK_BUTTON "SDL_WINRT_HANDLE_BACK_BUTTON"

/** Label text for a WinRT app's privacy policy link
 *
 *  Network-enabled WinRT apps must include a privacy policy.  On Windows 8, 8.1, and RT,
 *  Microsoft mandates that this policy be available via the Windows Settings charm.
 *  SDL provides code to add a link there, with its label text being set via the
 *  optional hint, SDL_HINT_WINRT_PRIVACY_POLICY_LABEL.
 *
 *  Please note that a privacy policy's contents are not set via this hint.  A separate
 *  hint, SDL_HINT_WINRT_PRIVACY_POLICY_URL, is used to link to the actual text of the
 *  policy.
 *
 *  The contents of this hint should be encoded as a UTF8 string.
 *
 *  The default value is "Privacy Policy".  This hint should only be set during app
 *  initialization, preferably before any calls to SDL_Init().
 *
 *  For additional information on linking to a privacy policy, see the documentation for
 *  SDL_HINT_WINRT_PRIVACY_POLICY_URL.
 */
#define SDL_HINT_WINRT_PRIVACY_POLICY_LABEL "SDL_WINRT_PRIVACY_POLICY_LABEL"

/**
 *  A URL to a WinRT app's privacy policy
 *
 *  All network-enabled WinRT apps must make a privacy policy available to its
 *  users.  On Windows 8, 8.1, and RT, Microsoft mandates that this policy be
 *  be available in the Windows Settings charm, as accessed from within the app.
 *  SDL provides code to add a URL-based link there, which can point to the app's
 *  privacy policy.
 *
 *  To setup a URL to an app's privacy policy, set SDL_HINT_WINRT_PRIVACY_POLICY_URL
 *  before calling any SDL_Init() functions.  The contents of the hint should
 *  be a valid URL.  For example, "http://www.example.com".
 *
 *  The default value is "", which will prevent SDL from adding a privacy policy
 *  link to the Settings charm.  This hint should only be set during app init.
 *
 *  The label text of an app's "Privacy Policy" link may be customized via another
 *  hint, SDL_HINT_WINRT_PRIVACY_POLICY_LABEL.
 *
 *  Please note that on Windows Phone, Microsoft does not provide standard UI
 *  for displaying a privacy policy link, and as such, SDL_HINT_WINRT_PRIVACY_POLICY_URL
 *  will not get used on that platform.  Network-enabled phone apps should display
 *  their privacy policy through some other, in-app means.
 */
#define SDL_HINT_WINRT_PRIVACY_POLICY_URL "SDL_WINRT_PRIVACY_POLICY_URL"

/**
 *  Mark X11 windows as override-redirect.
 *
 *  If set, this _might_ increase framerate at the expense of the desktop
 *  not working as expected. Override-redirect windows aren't noticed by the
 *  window manager at all.
 *
 *  You should probably only use this for fullscreen windows, and you probably
 *  shouldn't even use it for that. But it's here if you want to try!
 */
#define SDL_HINT_X11_FORCE_OVERRIDE_REDIRECT "SDL_X11_FORCE_OVERRIDE_REDIRECT"

/**
 *  A variable that lets you disable the detection and use of Xinput gamepad devices
 *
 *  The variable can be set to the following values:
 *    "0"       - Disable XInput detection (only uses direct input)
 *    "1"       - Enable XInput detection (the default)
 */
#define SDL_HINT_XINPUT_ENABLED "SDL_XINPUT_ENABLED"

 /**
  *  A variable that lets you disable the detection and use of DirectInput gamepad devices
  *
  *  The variable can be set to the following values:
  *    "0"       - Disable DirectInput detection (only uses XInput)
  *    "1"       - Enable DirectInput detection (the default)
  */
#define SDL_HINT_DIRECTINPUT_ENABLED "SDL_DIRECTINPUT_ENABLED"

/**
 *  A variable that causes SDL to not ignore audio "monitors"
 *
 *  This is currently only used for PulseAudio and ignored elsewhere.
 *
 *  By default, SDL ignores audio devices that aren't associated with physical
 *  hardware. Changing this hint to "1" will expose anything SDL sees that
 *  appears to be an audio source or sink. This will add "devices" to the list
 *  that the user probably doesn't want or need, but it can be useful in
 *  scenarios where you want to hook up SDL to some sort of virtual device,
 *  etc.
 *
 *  The default value is "0".  This hint must be set before SDL_Init().
 *
 *  This hint is available since SDL 2.0.16. Before then, virtual devices are
 *  always ignored.
 */
#define SDL_HINT_AUDIO_INCLUDE_MONITORS "SDL_AUDIO_INCLUDE_MONITORS"

/**
 *  A variable that forces X11 windows to create as a custom type.
 *
 *  This is currently only used for X11 and ignored elsewhere.
 *
 *  During SDL_CreateWindow, SDL uses the _NET_WM_WINDOW_TYPE X11 property
 *  to report to the window manager the type of window it wants to create.
 *  This might be set to various things if SDL_WINDOW_TOOLTIP or
 *  SDL_WINDOW_POPUP_MENU, etc, were specified. For "normal" windows that
 *  haven't set a specific type, this hint can be used to specify a custom
 *  type. For example, a dock window might set this to
 *  "_NET_WM_WINDOW_TYPE_DOCK".
 *
 *  If not set or set to "", this hint is ignored. This hint must be set
 *  before the SDL_CreateWindow() call that it is intended to affect.
 *
 *  This hint is available since SDL 2.0.22.
 */
#define SDL_HINT_X11_WINDOW_TYPE "SDL_X11_WINDOW_TYPE"

/**
 *  A variable that decides whether to send SDL_EVENT_QUIT when closing the final window.
 *
 *  By default, SDL sends an SDL_EVENT_QUIT event when there is only one window
 *  and it receives an SDL_EVENT_WINDOW_CLOSE_REQUESTED event, under the assumption most
 *  apps would also take the loss of this window as a signal to terminate the
 *  program.
 *
 *  However, it's not unreasonable in some cases to have the program continue
 *  to live on, perhaps to create new windows later.
 *
 *  Changing this hint to "0" will cause SDL to not send an SDL_EVENT_QUIT event
 *  when the final window is requesting to close. Note that in this case,
 *  there are still other legitimate reasons one might get an SDL_EVENT_QUIT
 *  event: choosing "Quit" from the macOS menu bar, sending a SIGINT (ctrl-c)
 *  on Unix, etc.
 *
 *  The default value is "1".  This hint can be changed at any time.
 *
 *  This hint is available since SDL 2.0.22. Before then, you always get
 *  an SDL_EVENT_QUIT event when closing the final window.
 */
#define SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE "SDL_QUIT_ON_LAST_WINDOW_CLOSE"


/**
 *  A variable that decides what video backend to use.
 *
 *  By default, SDL will try all available video backends in a reasonable
 *  order until it finds one that can work, but this hint allows the app
 *  or user to force a specific target, such as "x11" if, say, you are
 *  on Wayland but want to try talking to the X server instead.
 *
 *  This functionality has existed since SDL 2.0.0 (indeed, before that)
 *  but before 2.0.22 this was an environment variable only. In 2.0.22,
 *  it was upgraded to a full SDL hint, so you can set the environment
 *  variable as usual or programmatically set the hint with SDL_SetHint,
 *  which won't propagate to child processes.
 *
 *  The default value is unset, in which case SDL will try to figure out
 *  the best video backend on your behalf. This hint needs to be set
 *  before SDL_Init() is called to be useful.
 *
 *  This hint is available since SDL 2.0.22. Before then, you could set
 *  the environment variable to get the same effect.
 */
#define SDL_HINT_VIDEO_DRIVER "SDL_VIDEO_DRIVER"

/**
 *  A variable that decides what audio backend to use.
 *
 *  By default, SDL will try all available audio backends in a reasonable
 *  order until it finds one that can work, but this hint allows the app
 *  or user to force a specific target, such as "alsa" if, say, you are
 *  on PulseAudio but want to try talking to the lower level instead.
 *
 *  This functionality has existed since SDL 2.0.0 (indeed, before that)
 *  but before 2.0.22 this was an environment variable only. In 2.0.22,
 *  it was upgraded to a full SDL hint, so you can set the environment
 *  variable as usual or programmatically set the hint with SDL_SetHint,
 *  which won't propagate to child processes.
 *
 *  The default value is unset, in which case SDL will try to figure out
 *  the best audio backend on your behalf. This hint needs to be set
 *  before SDL_Init() is called to be useful.
 *
 *  This hint is available since SDL 2.0.22. Before then, you could set
 *  the environment variable to get the same effect.
 */
#define SDL_HINT_AUDIO_DRIVER "SDL_AUDIO_DRIVER"

/**
 *  A variable that decides what KMSDRM device to use.
 *
 *  Internally, SDL might open something like "/dev/dri/cardNN" to
 *  access KMSDRM functionality, where "NN" is a device index number.
 *
 *  SDL makes a guess at the best index to use (usually zero), but the
 *  app or user can set this hint to a number between 0 and 99 to
 *  force selection.
 *
 *  This hint is available since SDL 2.24.0.
 */
#define SDL_HINT_KMSDRM_DEVICE_INDEX "SDL_KMSDRM_DEVICE_INDEX"


/**
 *  A variable that treats trackpads as touch devices.
 *
 *  On macOS (and possibly other platforms in the future), SDL will report
 *  touches on a trackpad as mouse input, which is generally what users
 *  expect from this device; however, these are often actually full
 *  multitouch-capable touch devices, so it might be preferable to some apps
 *  to treat them as such.
 *
 *  Setting this hint to true will make the trackpad input report as a
 *  multitouch device instead of a mouse. The default is false.
 *
 *  Note that most platforms don't support this hint. As of 2.24.0, it
 *  only supports MacBooks' trackpads on macOS. Others may follow later.
 *
 *  This hint is checked during SDL_Init and can not be changed after.
 *
 *  This hint is available since SDL 2.24.0.
 */
#define SDL_HINT_TRACKPAD_IS_TOUCH_ONLY "SDL_TRACKPAD_IS_TOUCH_ONLY"


/**
 *  Sets the title of the TextInput window on GDK platforms.
 *
 *  On GDK, if SDL_GDK_TEXTINPUT is defined, you can use the
 *  standard SDL text input and virtual keyboard capabilities
 *  to get text from the user.
 *
 *  This hint allows you to customize the virtual keyboard
 *  window that will be shown to the user.
 *
 *  Set this hint to change the title of the window.
 *
 *  This hint will not affect a window that is already being
 *  shown to the user. It will only affect new input windows.
 *
 *  This hint is available only if SDL_GDK_TEXTINPUT defined.
 */
#define SDL_HINT_GDK_TEXTINPUT_TITLE "SDL_GDK_TEXTINPUT_TITLE"

/**
 *  Sets the description of the TextInput window on GDK platforms.
 *
 *  On GDK, if SDL_GDK_TEXTINPUT is defined, you can use the
 *  standard SDL text input and virtual keyboard capabilities
 *  to get text from the user.
 *
 *  This hint allows you to customize the virtual keyboard
 *  window that will be shown to the user.
 *
 *  Set this hint to change the description of the window.
 *
 *  This hint will not affect a window that is already being
 *  shown to the user. It will only affect new input windows.
 *
 *  This hint is available only if SDL_GDK_TEXTINPUT defined.
 */
#define SDL_HINT_GDK_TEXTINPUT_DESCRIPTION "SDL_GDK_TEXTINPUT_DESCRIPTION"

/**
 *  Sets the default text of the TextInput window on GDK platforms.
 *
 *  On GDK, if SDL_GDK_TEXTINPUT is defined, you can use the
 *  standard SDL text input and virtual keyboard capabilities
 *  to get text from the user.
 *
 *  This hint allows you to customize the virtual keyboard
 *  window that will be shown to the user.
 *
 *  Set this hint to change the default text value of the window.
 *
 *  This hint will not affect a window that is already being
 *  shown to the user. It will only affect new input windows.
 *
 *  This hint is available only if SDL_GDK_TEXTINPUT defined.
 */
#define SDL_HINT_GDK_TEXTINPUT_DEFAULT "SDL_GDK_TEXTINPUT_DEFAULT"

/**
 *  Sets the input scope of the TextInput window on GDK platforms.
 *
 *  On GDK, if SDL_GDK_TEXTINPUT is defined, you can use the
 *  standard SDL text input and virtual keyboard capabilities
 *  to get text from the user.
 *
 *  This hint allows you to customize the virtual keyboard
 *  window that will be shown to the user.
 *
 *  Set this hint to change the XGameUiTextEntryInputScope value
 *  that will be passed to the window creation function.
 *
 *  The value must be a stringified integer,
 *  for example "0" for XGameUiTextEntryInputScope::Default.
 *
 *  This hint will not affect a window that is already being
 *  shown to the user. It will only affect new input windows.
 *
 *  This hint is available only if SDL_GDK_TEXTINPUT defined.
 */
#define SDL_HINT_GDK_TEXTINPUT_SCOPE "SDL_GDK_TEXTINPUT_SCOPE"

/**
 *  Sets the maximum input length of the TextInput window on GDK platforms.
 *
 *  On GDK, if SDL_GDK_TEXTINPUT is defined, you can use the
 *  standard SDL text input and virtual keyboard capabilities
 *  to get text from the user.
 *
 *  This hint allows you to customize the virtual keyboard
 *  window that will be shown to the user.
 *
 *  Set this hint to change the maximum allowed input
 *  length of the text box in the virtual keyboard window.
 *
 *  The value must be a stringified integer,
 *  for example "10" to allow for up to 10 characters of text input.
 *
 *  This hint will not affect a window that is already being
 *  shown to the user. It will only affect new input windows.
 *
 *  This hint is available only if SDL_GDK_TEXTINPUT defined.
 */
#define SDL_HINT_GDK_TEXTINPUT_MAX_LENGTH "SDL_GDK_TEXTINPUT_MAX_LENGTH"

/**
 * Set the next device open's buffer size.
 *
 * This hint is an integer > 0, that represents the size of the device's
 * buffer in sample frames (stereo audio data in 16-bit format is 4 bytes
 * per sample frame, for example).
 *
 * SDL3 generally decides this value on behalf of the app, but if for some
 * reason the app needs to dictate this (because they want either lower
 * latency or higher throughput AND ARE WILLING TO DEAL WITH what that
 * might require of the app), they can specify it.
 *
 * SDL will try to accomodate this value, but there is no promise you'll
 * get the buffer size requested. Many platforms won't honor this request
 * at all, or might adjust it.
 *
 * This hint is checked when opening an audio device and can be changed
 * between calls.
 */
#define SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES "SDL_AUDIO_DEVICE_SAMPLE_FRAMES"

/**
 * Request SDL_AppIterate() be called at a specific rate.
 *
 * This number is in Hz, so "60" means try to iterate 60 times per second.
 *
 * On some platforms, or if you are using SDL_main instead of SDL_AppIterate,
 * this hint is ignored. When the hint can be used, it is allowed to be
 * changed at any time.
 *
 * This defaults to 60, and specifying NULL for the hint's value will restore
 * the default.
 */
#define SDL_HINT_MAIN_CALLBACK_RATE "SDL_MAIN_CALLBACK_RATE"

/**
 * Cause SDL to call dbus_shutdown() on quit.
 *
 * This is useful as a debug tool to validate memory leaks, but shouldn't ever
 * be set in production applications, as other libraries used by the application
 * might use dbus under the hood and this cause cause crashes if they continue
 * after SDL_Quit().
 *
 * This variable can be set to the following values:
 *   "0"       - SDL will not call dbus_shutdown() on quit (default)
 *   "1"       - SDL will call dbus_shutdown() on quit
 *
 * This hint is available since SDL 3.0.0.
 */
#define SDL_HINT_SHUTDOWN_DBUS_ON_QUIT "SDL_SHUTDOWN_DBUS_ON_QUIT"


/**
 *  An enumeration of hint priorities
 */
typedef enum
{
    SDL_HINT_DEFAULT,
    SDL_HINT_NORMAL,
    SDL_HINT_OVERRIDE
} SDL_HintPriority;


/**
 * Set a hint with a specific priority.
 *
 * The priority controls the behavior when setting a hint that already has a
 * value. Hints will replace existing hints of their priority and lower.
 * Environment variables are considered to have override priority.
 *
 * \param name the hint to set
 * \param value the value of the hint variable
 * \param priority the SDL_HintPriority level for the hint
 * \returns SDL_TRUE if the hint was set, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetHint
 * \sa SDL_SetHint
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SetHintWithPriority(const char *name,
                                                         const char *value,
                                                         SDL_HintPriority priority);

/**
 * Set a hint with normal priority.
 *
 * Hints will not be set if there is an existing override hint or environment
 * variable that takes precedence. You can use SDL_SetHintWithPriority() to
 * set the hint with override priority instead.
 *
 * \param name the hint to set
 * \param value the value of the hint variable
 * \returns SDL_TRUE if the hint was set, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetHint
 * \sa SDL_SetHintWithPriority
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SetHint(const char *name,
                                             const char *value);

/**
 * Reset a hint to the default value.
 *
 * This will reset a hint to the value of the environment variable, or NULL if
 * the environment isn't set. Callbacks will be called normally with this
 * change.
 *
 * \param name the hint to set
 * \returns SDL_TRUE if the hint was set, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetHint
 * \sa SDL_SetHint
 */
extern DECLSPEC SDL_bool SDLCALL SDL_ResetHint(const char *name);

/**
 * Reset all hints to the default values.
 *
 * This will reset all hints to the value of the associated environment
 * variable, or NULL if the environment isn't set. Callbacks will be called
 * normally with this change.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetHint
 * \sa SDL_SetHint
 * \sa SDL_ResetHint
 */
extern DECLSPEC void SDLCALL SDL_ResetHints(void);

/**
 * Get the value of a hint.
 *
 * \param name the hint to query
 * \returns the string value of a hint or NULL if the hint isn't set.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetHint
 * \sa SDL_SetHintWithPriority
 */
extern DECLSPEC const char * SDLCALL SDL_GetHint(const char *name);

/**
 * Get the boolean value of a hint variable.
 *
 * \param name the name of the hint to get the boolean value from
 * \param default_value the value to return if the hint does not exist
 * \returns the boolean value of a hint or the provided default value if the
 *          hint does not exist.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetHint
 * \sa SDL_SetHint
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GetHintBoolean(const char *name, SDL_bool default_value);

/**
 * Type definition of the hint callback function.
 *
 * \param userdata what was passed as `userdata` to SDL_AddHintCallback()
 * \param name what was passed as `name` to SDL_AddHintCallback()
 * \param oldValue the previous hint value
 * \param newValue the new value hint is to be set to
 */
typedef void (SDLCALL *SDL_HintCallback)(void *userdata, const char *name, const char *oldValue, const char *newValue);

/**
 * Add a function to watch a particular hint.
 *
 * \param name the hint to watch
 * \param callback An SDL_HintCallback function that will be called when the
 *                 hint value changes
 * \param userdata a pointer to pass to the callback function
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DelHintCallback
 */
extern DECLSPEC int SDLCALL SDL_AddHintCallback(const char *name,
                                                SDL_HintCallback callback,
                                                void *userdata);

/**
 * Remove a function watching a particular hint.
 *
 * \param name the hint being watched
 * \param callback An SDL_HintCallback function that will be called when the
 *                 hint value changes
 * \param userdata a pointer being passed to the callback function
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AddHintCallback
 */
extern DECLSPEC void SDLCALL SDL_DelHintCallback(const char *name,
                                                 SDL_HintCallback callback,
                                                 void *userdata);

/**
 * Clear all hints.
 *
 * This function is automatically called during SDL_Quit(), and deletes all
 * callbacks without calling them and frees all memory associated with hints.
 * If you're calling this from application code you probably want to call
 * SDL_ResetHints() instead.
 *
 * This function will be removed from the API the next time we rev the ABI.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ResetHints
 */
extern DECLSPEC void SDLCALL SDL_ClearHints(void);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_hints_h_ */
