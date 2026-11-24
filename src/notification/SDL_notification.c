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

#include "SDL_notification_c.h"
#include <SDL3/SDL_properties.h>

SDL_NotificationID SDL_ShowNotificationWithProperties(SDL_PropertiesID props)
{
    if (!props) {
        SDL_InvalidParamError("props");
        return 0;
    }

    // The title property is required.
    CHECK_PARAM (true) {
        const char *title = SDL_GetStringProperty(props, SDL_PROP_NOTIFICATION_TITLE_STRING, NULL);
        if (!title) {
            SDL_SetError("Notifications must have a title");
            return 0;
        }
    }

    return SDL_SYS_ShowNotification(props);
}

SDL_NotificationID SDL_ShowNotification(const char *title, const char *message, SDL_Surface *image, SDL_NotificationAction *actions, int num_actions)
{
    SDL_NotificationID id = 0;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return 0;
    }

    if (title) {
        if (!SDL_SetStringProperty(props, SDL_PROP_NOTIFICATION_TITLE_STRING, title)) {
            goto cleanup;
        }
    } else {
        SDL_SetError("Notifications must have a title");
        goto cleanup;
    }
    if (message) {
        if (!SDL_SetStringProperty(props, SDL_PROP_NOTIFICATION_MESSAGE_STRING, message)) {
            goto cleanup;
        }
    }
    if (image) {
        if (!SDL_SetPointerProperty(props, SDL_PROP_NOTIFICATION_IMAGE_POINTER, image)) {
            goto cleanup;
        }
    }
    if (actions && num_actions) {
        if (!SDL_SetPointerProperty(props, SDL_PROP_NOTIFICATION_ACTIONS_POINTER, actions)) {
            goto cleanup;
        }
        if (!SDL_SetNumberProperty(props, SDL_PROP_NOTIFICATION_ACTION_COUNT_NUMBER, num_actions)) {
            goto cleanup;
        }
    }

    id = SDL_ShowNotificationWithProperties(props);

cleanup:
    SDL_DestroyProperties(props);
    return id;
}
