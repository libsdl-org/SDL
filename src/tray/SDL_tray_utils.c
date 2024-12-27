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
#include "SDL_internal.h"

#include "../video/SDL_sysvideo.h"
#include "../events/SDL_events_c.h"

static int active_trays = 0;

extern void SDL_IncrementTrayCount(void)
{
    if (++active_trays < 1) {
        SDL_Log("Active tray count corrupted (%d < 1), this is a bug. The app may close or fail to close unexpectedly.", active_trays);
    }
}

extern void SDL_DecrementTrayCount(void)
{
    int toplevel_count = 0;
    SDL_Window *n;

    if (--active_trays < 0) {
        SDL_Log("Active tray count corrupted (%d < 0), this is a bug. The app may close or fail to close unexpectedly.", active_trays);
    }

    if (!SDL_GetHintBoolean(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, true)) {
        return;
    }

    for (n = SDL_GetVideoDevice()->windows; n; n = n->next) {
        if (!n->parent && !(n->flags & SDL_WINDOW_HIDDEN)) {
            ++toplevel_count;
        }
    }

    if (toplevel_count < 1) {
        SDL_SendQuit();
    }
}

extern bool SDL_HasNoActiveTrays(void)
{
    return active_trays < 1;
}
