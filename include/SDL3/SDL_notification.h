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
 *  \file SDL_notification.h
 *
 *  \brief Header file for notification API
 */

#ifndef SDL_notification_h_
#define SDL_notification_h_

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief SDL_Notification flags.
 */
typedef enum SDL_NotificationFlags
{
    SDL_NOTIFICATION_PRIORITY_LOW        = 0x00000010,   /**< lowest */
    SDL_NOTIFICATION_PRIORITY_NORMAL     = 0x00000020,   /**< normal/medium */
    SDL_NOTIFICATION_PRIORITY_HIGH       = 0x00000040    /**< high/important/critical */
} SDL_NotificationFlags;

/**
 * \brief SDL_Icon flags.
 */
typedef enum SDL_IconFlags
{
    SDL_ICON_TYPE_SINGLE_FILE    = 0x00000010,   /**< A single icon file. */
    SDL_ICON_TYPE_SURFACE        = 0x00000020,   /**< Icon inside an SDL surface. */
    SDL_ICON_TYPE_WINDOW         = 0x00000040    /**< Icon is same as SDL window. */
} SDL_IconFlags;

/**
 * Notification structure containing title, text, window, etc.
 */
typedef struct SDL_NotificationData
{
    Uint32 flags;                       /**< ::SDL_NotificationFlags */
    const char *title;                  /**< UTF-8 title */
    const char *message;                /**< UTF-8 message text */

    struct
    {
        Uint32 flags;                       /**< ::SDL_IconFlags */
        union {
            const char *path;
            SDL_Surface *surface;
            SDL_Window *window;
        } data;
    } icon;

} SDL_NotificationData;

/**
 *  \brief Create a system notification.
 *
 *  \param notificationdata the SDL_NotificationData structure with title, text and other options
 *  \returns 0 on success or a negative error code on failure; call
 *           SDL_GetError() for more information.
 *
 *  \since This macro is available since SDL 3.TODO
 *
 *  \sa SDL_ShowSimpleNotification
 *  \sa SDL_NotificationData
 */
extern SDL_DECLSPEC int SDLCALL SDL_ShowNotification(const SDL_NotificationData *notificationdata);

/**
 *  \brief Create a simple system notification.
 *
 *  \param title    UTF-8 title text
 *  \param message  UTF-8 message text
 *  \returns 0 on success or a negative error code on failure; call
 *           SDL_GetError() for more information.
 *
 *  \since This macro is available since SDL 3.TODO
 *
 *  \sa SDL_ShowNotification
 *  \sa SDL_NotificationData
 */
extern SDL_DECLSPEC int SDLCALL SDL_ShowSimpleNotification(const char *title, const char *message);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_notification_h_ */

