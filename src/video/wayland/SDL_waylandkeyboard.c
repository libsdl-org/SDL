/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WAYLAND

#include "../SDL_sysvideo.h"
#include "SDL_waylandvideo.h"

int
Wayland_InitKeyboard(_THIS)
{
#ifdef SDL_USE_IME
    SDL_IME_Init();
#endif

    return 0;
}

void
Wayland_QuitKeyboard(_THIS)
{
#ifdef SDL_USE_IME
    SDL_IME_Quit();
#endif
}

void
Wayland_StartTextInput(_THIS)
{
    /* No-op */
}

void
Wayland_StopTextInput(_THIS)
{
#ifdef SDL_USE_IME
    SDL_IME_Reset();
#endif
}

void
Wayland_SetTextInputRect(_THIS, SDL_Rect *rect)
{
    if (!rect) {
        SDL_InvalidParamError("rect");
        return;
    }
       
#ifdef SDL_USE_IME
    SDL_IME_UpdateTextRect(rect);
#endif
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */

/* vi: set ts=4 sw=4 expandtab: */
