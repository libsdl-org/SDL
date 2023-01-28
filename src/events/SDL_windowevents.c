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

/* Window event handling code for SDL */

#include "SDL_events_c.h"
#include "SDL_mouse_c.h"


static int SDLCALL RemoveSupercededWindowEvents(void *userdata, SDL_Event *event)
{
    SDL_Event *new_event = (SDL_Event *)userdata;

    if (event->type == new_event->type &&
        event->window.windowID == new_event->window.windowID) {
        /* We're about to post a new move event, drop the old one */
        return 0;
    }
    return 1;
}

int SDL_SendWindowEvent(SDL_Window *window, SDL_EventType windowevent,
                        int data1, int data2)
{
    int posted;

    if (window == NULL) {
        return 0;
    }
    if (window->is_destroying) {
        return 0;
    }
    switch (windowevent) {
    case SDL_EVENT_WINDOW_SHOWN:
        if (!(window->flags & SDL_WINDOW_HIDDEN)) {
            return 0;
        }
        window->flags &= ~(SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED);
        SDL_OnWindowShown(window);
        break;
    case SDL_EVENT_WINDOW_HIDDEN:
        if (window->flags & SDL_WINDOW_HIDDEN) {
            return 0;
        }
        window->flags |= SDL_WINDOW_HIDDEN;
        SDL_OnWindowHidden(window);
        break;
    case SDL_EVENT_WINDOW_MOVED:
        if (SDL_WINDOWPOS_ISUNDEFINED(data1) ||
            SDL_WINDOWPOS_ISUNDEFINED(data2)) {
            return 0;
        }
        if ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == 0) {
            window->windowed.x = data1;
            window->windowed.y = data2;
        }
        if (data1 == window->x && data2 == window->y) {
            return 0;
        }
        window->x = data1;
        window->y = data2;
        SDL_OnWindowMoved(window);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if ((window->flags & SDL_WINDOW_FULLSCREEN_MASK) == 0) {
            window->windowed.w = data1;
            window->windowed.h = data2;
        }
        if (data1 == window->w && data2 == window->h) {
            SDL_CheckWindowPixelSizeChanged(window);
            return 0;
        }
        window->w = data1;
        window->h = data2;
        SDL_OnWindowResized(window);
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (data1 == window->last_pixel_w && data2 == window->last_pixel_h) {
            return 0;
        }
        window->last_pixel_w = data1;
        window->last_pixel_h = data2;
        SDL_OnWindowPixelSizeChanged(window);
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_MAXIMIZED;
        window->flags |= SDL_WINDOW_MINIMIZED;
        SDL_OnWindowMinimized(window);
        break;
    case SDL_EVENT_WINDOW_MAXIMIZED:
        if (window->flags & SDL_WINDOW_MAXIMIZED) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_MINIMIZED;
        window->flags |= SDL_WINDOW_MAXIMIZED;
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        if (!(window->flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))) {
            return 0;
        }
        window->flags &= ~(SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED);
        SDL_OnWindowRestored(window);
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        if (window->flags & SDL_WINDOW_MOUSE_FOCUS) {
            return 0;
        }
        window->flags |= SDL_WINDOW_MOUSE_FOCUS;
        SDL_OnWindowEnter(window);
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (!(window->flags & SDL_WINDOW_MOUSE_FOCUS)) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_MOUSE_FOCUS;
        SDL_OnWindowLeave(window);
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
            return 0;
        }
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_OnWindowFocusGained(window);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (!(window->flags & SDL_WINDOW_INPUT_FOCUS)) {
            return 0;
        }
        window->flags &= ~SDL_WINDOW_INPUT_FOCUS;
        SDL_OnWindowFocusLost(window);
        break;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        if (data1 < 0 || data1 == window->display_index) {
            return 0;
        }
        window->display_index = data1;
        SDL_OnWindowDisplayChanged(window);
        break;
    default:
        break;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(windowevent)) {
        SDL_Event event;
        event.type = windowevent;
        event.common.timestamp = 0;
        event.window.data1 = data1;
        event.window.data2 = data2;
        event.window.windowID = window->id;

        /* Fixes queue overflow with move/resize events that aren't processed */
        if (windowevent == SDL_EVENT_WINDOW_MOVED ||
            windowevent == SDL_EVENT_WINDOW_RESIZED ||
            windowevent == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
            windowevent == SDL_EVENT_WINDOW_EXPOSED) {
            SDL_FilterEvents(RemoveSupercededWindowEvents, &event);
        }
        posted = (SDL_PushEvent(&event) > 0);
    }

    if (windowevent == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (!window->prev && !window->next) {
            if (SDL_GetHintBoolean(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, SDL_TRUE)) {
                SDL_SendQuit(); /* This is the last window in the list so send the SDL_EVENT_QUIT event */
            }
        }
    }

    return posted;
}
