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
#include "SDL_internal.h"

/* Display event handling code for SDL */

#include "SDL_events_c.h"

int SDL_SendDisplayEvent(SDL_VideoDisplay *display, SDL_EventType displayevent, int data1)
{
    int posted;

    if (display == NULL || display->id == 0) {
        return 0;
    }
    switch (displayevent) {
    case SDL_EVENT_DISPLAY_ORIENTATION:
        if (data1 == SDL_ORIENTATION_UNKNOWN || data1 == display->current_orientation) {
            return 0;
        }
        display->current_orientation = (SDL_DisplayOrientation)data1;
        break;
    default:
        break;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(displayevent)) {
        SDL_Event event;
        event.type = displayevent;
        event.common.timestamp = 0;
        event.display.displayID = display->id;
        event.display.data1 = data1;
        posted = (SDL_PushEvent(&event) > 0);
    }

    switch (displayevent) {
    case SDL_EVENT_DISPLAY_CONNECTED:
        SDL_OnDisplayConnected(display);
        break;
    default:
        break;
    }

    return posted;
}
