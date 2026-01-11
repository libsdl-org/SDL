/*
  Simple DirectMedia Layer
  Copyright (C) 2025 BlackBerry Limited

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

#include "SDL_qnx.h"
#include "../../SDL_internal.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include "../../events/SDL_mouse_c.h"


static Uint8 screenToMouseButton(int x)
{
    //Screen only supports 3 mouse buttons.
    switch(x){
        case SCREEN_LEFT_MOUSE_BUTTON: // 1 << 0
        return SDL_BUTTON_LEFT;
        case SCREEN_RIGHT_MOUSE_BUTTON: //1 << 1
        return SDL_BUTTON_RIGHT;
        case SCREEN_MIDDLE_MOUSE_BUTTON: //1 << 2
        return SDL_BUTTON_MIDDLE;
    }
    return 0;
}

void handlePointerEvent(screen_event_t event)
{
    int              buttons = 0;
    int              mouse_wheel = 0;
    int              mouse_h_wheel = 0;
    int              pos[2] = {0,0};
    int              x_win = 0;
    int              y_win = 0;

    Uint64           timestamp = SDL_GetTicksNS();
    SDL_Mouse        *mouse;
    SDL_MouseData    *mouse_data;
    SDL_Window       *window;

    screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &buttons);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_WHEEL, &mouse_wheel);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL, &mouse_h_wheel);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, pos);

    mouse = SDL_GetMouse();

    window = mouse->focus;
    mouse_data = mouse->internal;
    if (mouse_data == NULL) {
        return;
    }

    if (window && (window->x <= pos[0]) && (window->y <= pos[1])
        && (pos[0] < (window->x + window->w))
        && (pos[1] < (window->y + window->h))) {
        // Capture movement
        // Scale from position relative to display to position relative to window.
        x_win = pos[0] - window->x;
        y_win = pos[1] - window->y;

        // SDL wants us to track the relative position of the mouse separately
        // from its internals for some reason...
        if (mouse->relative_mode) {
            SDL_SendMouseMotion(timestamp, window, SDL_DEFAULT_MOUSE_ID, true, x_win - mouse_data->x_prev, y_win - mouse_data->y_prev);
        } else {
            SDL_SendMouseMotion(timestamp, window, SDL_DEFAULT_MOUSE_ID, false, x_win, y_win);
        }

        mouse_data->x_prev = x_win;
        mouse_data->y_prev = y_win;

        // Capture button presses
        for (int i = 0; i < 3; ++i)
        {
            Uint8 ret = screenToMouseButton(1 << i);
            SDL_SendMouseButton(timestamp, window, SDL_DEFAULT_MOUSE_ID, ret, (bool) ((buttons & (1 << i)) == (1 << i)));
        }

        // Capture mouse wheel
        // TODO: Verify this. I can at least confirm that this behaves the same
        //       way as x11.
        SDL_SendMouseWheel(timestamp, window, 0, (float) mouse_wheel, (float) mouse_h_wheel, SDL_MOUSEWHEEL_NORMAL);
    }
}
