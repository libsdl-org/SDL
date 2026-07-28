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

/**
 * # CategoryNotification
 *
 * Notifications are temporary popup dialogs that passively present
 * information to the user, or prompt user action. They are managed and
 * presented by the system, and can present simple options for user feedback,
 * usually in the form of buttons.
 *
 * The capabilities of notifications, and how they are displayed, vary between
 * systems, but they generally allow for a title, message body, an associated
 * image, and buttons to allow the user to provide feedback.
 *
 * How notifications are presented and handled are subject to system policy,
 * and it should not be assumed that showing a notification means that the
 * user will see it immediately, if at all. The user may disable notifications
 * for certain applications, they may be suppressed based on the current
 * activity, and most systems provide a "do not disturb" mode that universally
 * silences notifications when activated.
 *
 * There is both a customizable function
 * `SDL_ShowNotificationWithProperties()` that offers many options for what is
 * displayed, and also a much-simplified version `SDL_ShowNotification()`,
 * which simply takes a header (required), message (optional), image
 * (optional), and button array (optional).
 */

#ifndef SDL_notification_h_
#define SDL_notification_h_

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The path to an image to be used as the header icon for system notifications
 * on some platforms.
 *
 * This is required on: - Windows - *nix when not running in a container, and
 * no .desktop entry is available
 *
 * Image types supported depend on the platform, but .png generally offers the
 * best compatability.
 *
 * On *nix platforms, this can also be the name of a system icon, as specified
 * by the Icon Naming Specification.
 *
 * Can be set before calling SDL_ShowNotificationWithProperties() or
 * SDL_ShowNotification() for the first time.
 *
 * \since This macro is available since SDL 3.6.0.
 */
#define SDL_PROP_GLOBAL_NOTIFICATION_HEADER_ICON_STRING "SDL.notification.header_icon"

typedef Uint32 SDL_NotificationID; /**< The identifier for a system notification. */

typedef enum SDL_NotificationPriority
{
    SDL_NOTIFICATION_PRIORITY_LOW = -1,    /**< Lowest priority. */
    SDL_NOTIFICATION_PRIORITY_NORMAL = 0,  /**< Normal/medium priority. */
    SDL_NOTIFICATION_PRIORITY_HIGH = 1,    /**< High/important priority. */
    SDL_NOTIFICATION_PRIORITY_CRITICAL = 2 /**< Highest/critical priority. Note that this may override any "Do Not Disturb" settings and wake the screen. */
} SDL_NotificationPriority;

typedef enum SDL_NotificationActionType
{
    SDL_NOTIFICATION_ACTION_TYPE_BUTTON = 1 /**< Adds a button to the notification that generates feedback when activated. */
} SDL_NotificationActionType;

/**
 * Notification structure describing actions that can be used to allow users
 * to interact with notification dialogs.
 *
 * Exactly How they are presented depends on the platform and implementation.
 *
 * User interactions with a notification are reported via events with the type
 * SDL_EVENT_NOTIFICATION_ACTION_INVOKED.
 *
 * Action types: - button: A button with a localized text label, which
 * generates feedback when activated.
 *
 * \sa SDL_NotificationEvent
 * \sa SDL_NotificationActionType
 */
typedef union SDL_NotificationAction
{
    SDL_NotificationActionType type;

    struct
    {
        SDL_NotificationActionType type; /**< SDL_NOTIFICATION_ACTION_TYPE_BUTTON */
        const char *action_id;           /**< The identifier string for the button. 'default' is a reserved identifier and must not be used. */
        const char *action_label;        /**< The localized label for the button associated with the action, in UTF-8 encoding. */
    } button;

    Uint8 padding[128];
} SDL_NotificationAction;

#define SDL_PROP_NOTIFICATION_ACTIONS_POINTER     "SDL.notification.actions"
#define SDL_PROP_NOTIFICATION_ACTION_COUNT_NUMBER "SDL.notification.action_count"
#define SDL_PROP_NOTIFICATION_IMAGE_POINTER       "SDL.notification.image"
#define SDL_PROP_NOTIFICATION_MESSAGE_STRING      "SDL.notification.message"
#define SDL_PROP_NOTIFICATION_PRIORITY_NUMBER     "SDL.notification.priority"
#define SDL_PROP_NOTIFICATION_REPLACES_NUMBER     "SDL.notification.replaces"
#define SDL_PROP_NOTIFICATION_SOUND_STRING        "SDL.notification.sound"
#define SDL_PROP_NOTIFICATION_TRANSIENT_BOOLEAN   "SDL.notification.transient"
#define SDL_PROP_NOTIFICATION_TITLE_STRING        "SDL.notification.title"

/**
 * Requests permission from the system to display notifications.
 *
 * A return value of `true` only means that the system supports notifications,
 * and that the request for permission was successfully issued. It does not
 * reflect any user settings to allow or deny notifications.
 *
 * \returns True on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.6.0.
 *
 * \sa SDL_ShowNotification
 * \sa SDL_ShowNotificationWithProperties
 * \sa SDL_NotificationAction
 */
extern SDL_DECLSPEC bool SDLCALL SDL_RequestNotificationPermission(void);

/**
 * Show a system notification.
 *
 * System notifications are small, asynchronous popup windows that notify the
 * user of some information. How they are displayed is system dependent.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_NOTIFICATION_TITLE_STRING`: the title of the notification, in
 *   UTF-8 encoding. This property is required.
 * - `SDL_PROP_NOTIFICATION_ACTIONS_POINTER`: An array of pointers to
 *   `SDL_NotificationAction` structs that will add actions to the
 *   notification, usually in the form of buttons or menu items. Note that
 *   systems may have a limit on the maximum number of actions that a
 *   notification can have.
 * - `SDL_PROP_NOTIFICATIONS_ACTION_COUNT_NUMBER`: the number of actions in
 *   the array of actions, if it exists.
 * - `SDL_PROP_NOTIFICATION_IMAGE_POINTER`: a pointer to an `SDL_Surface`
 *   containing an image that will be attached to the notification. In most
 *   cases, the image is displayed in the form of a large icon or thumbnail
 *   alongside the message body. Notifications on Apple platforms can be
 *   expanded to show a larger format image.
 * - `SDL_PROP_NOTIFICATION_MESSAGE_STRING`: the message body of the
 *   notification, in UTF-8 encoding.
 * - `SDL_PROP_NOTIFICATION_PRIORITY_NUMBER`: an `SDL_NotificationPriority`
 *   value representing the notification priority.
 * - `SDL_PROP_NOTIFICATION_REPLACES_NUMBER`: the `SDL_NotificationID` of a
 *   previously shown notification that this notification should replace.
 * - `SDL_PROP_NOTIFICATION_SOUND_STRING`: sets a sound to play when the
 *   notification is shown. This can have the value "default", to play the
 *   system default notification sound, "silent", to play no sound, or contain
 *   the path to a file with a custom sound. The paths and formats that can be
 *   used for custom sounds are system-specific, and can have some
 *   restrictions, depending on the platform:
 * - Apple platforms require that the sound file is contained within the app
 *   bundle. Supported formats are: Linear PCM, MA4 (IMA/ADPCM), uLaw, or
 *   aLaw, in an .aiff, .wav, or .caf file.
 * - Windows can only play custom notification sounds when the app is packaged
 *   inside an MSIX installer. Playback from arbitrary file paths is not
 *   supported. Supported formats are: .aac, .flac, .m4a, .mp3, .wav, and
 *   .wma.
 * - Unix platforms can generally load sounds from any arbitrary path, as long
 *   as the read permissions are correct. Supported formats are: ogg/opus,
 *   ogg/vorbis, and wav/pcm. If this property is not set, the system default
 *   sound will be used.
 * - `SDL_PROP_NOTIFICATION_TRANSIENT_BOOLEAN`: true if the notification
 *   should not persist in the system notification center after initially
 *   being shown.
 *
 * Not all properties are supported by all platforms.
 *
 * Notifications are available on: - Windows 10 or higher - macOS 10.14 or
 * higher - iOS 11 or higher - *nix platforms that support the
 * org.freedesktop.Notifications, or org.freedesktop.portal.Notification
 * interfaces
 *
 * \param props the properties to be used when creating this notification.
 * \returns A non-zero SDL_NotificationID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.6.0.
 *
 * \sa SDL_ShowNotification
 * \sa SDL_NotificationAction
 * \sa SDL_NotificationPriority
 * \sa SDL_NotificationEvent
 */
extern SDL_DECLSPEC SDL_NotificationID SDLCALL SDL_ShowNotificationWithProperties(SDL_PropertiesID props);

/**
 * Show a system notification with normal priority.
 *
 * \param title UTF-8 title text, required.
 * \param message UTF-8 message text, may be NULL.
 * \param image The image associated with this notification, may be NULL.
 * \param actions An array of actions to attach to the notification, may be
 *                NULL.
 * \param num_actions The number of actions in the actions array.
 * \returns A non-zero SDL_NotificationID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.6.0.
 *
 * \sa SDL_ShowNotificationWithProperties
 * \sa SDL_NotificationAction
 * \sa SDL_NotificationEvent
 */
extern SDL_DECLSPEC SDL_NotificationID SDLCALL SDL_ShowNotification(const char *title, const char *message, SDL_Surface *image, SDL_NotificationAction *actions, int num_actions);

/**
 * Remove a notification.
 *
 * \param notification the ID of the notification to remove.
 * \returns True on success or false on failure; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.6.0.
 *
 * \sa SDL_ShowNotificationWithProperties
 * \sa SDL_ShowNotification
 */
extern SDL_DECLSPEC bool SDLCALL SDL_RemoveNotification(SDL_NotificationID notification);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_notification_h_ */
