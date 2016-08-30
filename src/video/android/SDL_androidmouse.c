/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_ANDROID

#include "SDL_androidmouse.h"

#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"

#include "../../core/android/SDL_android.h"

#define ACTION_DOWN 0
#define ACTION_UP 1
#define ACTION_MOVE 2
#define ACTION_HOVER_MOVE 7
#define ACTION_SCROLL 8
#define BUTTON_PRIMARY 1
#define BUTTON_SECONDARY 2
#define BUTTON_TERTIARY 4
#define BUTTON_BACK 8
#define BUTTON_FORWARD 16

static Uint8 SDLButton;

void
Android_InitMouse(void)
{
    SDLButton = 0;
}

void Android_OnMouse( int androidButton, int action, float x, float y) {
    if (!Android_Window) {
        return;
    }

    switch(action) {
        case ACTION_DOWN:
            // Determine which button originated the event, and store it for ACTION_UP
            SDLButton = SDL_BUTTON_LEFT;
            if (androidButton == BUTTON_SECONDARY) {
                SDLButton = SDL_BUTTON_RIGHT;
            } else if (androidButton == BUTTON_TERTIARY) {
                SDLButton = SDL_BUTTON_MIDDLE;
            } else if (androidButton == BUTTON_FORWARD) {
                SDLButton = SDL_BUTTON_X1;
            } else if (androidButton == BUTTON_BACK) {
                SDLButton = SDL_BUTTON_X2;
            }
            SDL_SendMouseMotion(Android_Window, 0, 0, x, y);
            SDL_SendMouseButton(Android_Window, 0, SDL_PRESSED, SDLButton);
            break;

        case ACTION_UP:
            // Android won't give us the button that originated the ACTION_DOWN event, so we'll
            // assume it's the one we stored
            SDL_SendMouseMotion(Android_Window, 0, 0, x, y);
            SDL_SendMouseButton(Android_Window, 0, SDL_RELEASED, SDLButton);
            break;

        case ACTION_MOVE:
        case ACTION_HOVER_MOVE:
            SDL_SendMouseMotion(Android_Window, 0, 0, x, y);
            break;

        case ACTION_SCROLL:
            SDL_SendMouseWheel(Android_Window, 0, x, y, SDL_MOUSEWHEEL_NORMAL);
            break;

        default:
            break;
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */

