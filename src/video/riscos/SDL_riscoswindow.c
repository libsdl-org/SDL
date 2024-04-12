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

#ifdef SDL_VIDEO_DRIVER_RISCOS

#include "../SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"

#include "SDL_riscosvideo.h"
#include "SDL_riscoswindow.h"

int RISCOS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *driverdata;

    driverdata = (SDL_WindowData *)SDL_calloc(1, sizeof(*driverdata));
    if (!driverdata) {
        return -1;
    }
    driverdata->window = window;

    SDL_SetMouseFocus(window);

    /* All done! */
    window->driverdata = driverdata;
    return 0;
}

void RISCOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *driverdata = window->driverdata;

    if (!driverdata) {
        return;
    }

    SDL_free(driverdata);
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_RISCOS */
