/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_N3DS

#include "../SDL_sysvideo.h"
#include "SDL_n3dsevents_c.h"
#include "SDL_n3dsframebuffer_c.h"
#include "SDL_n3dsswkb.h"
#include "SDL_n3dsvideo.h"

#define N3DSVID_DRIVER_NAME "n3ds"

SDL_FORCE_INLINE void AddN3DSDisplay(gfxScreen_t screen);

static int N3DS_VideoInit(_THIS);
static void N3DS_VideoQuit(_THIS);
static void N3DS_GetDisplayModes(_THIS, SDL_VideoDisplay *display);
static int N3DS_CreateWindow(_THIS, SDL_Window *window);
static void N3DS_DestroyWindow(_THIS, SDL_Window *window);

/* N3DS driver bootstrap functions */

static void
N3DS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->displays);
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *
N3DS_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return (0);
    }

    device->VideoInit = N3DS_VideoInit;
    device->VideoQuit = N3DS_VideoQuit;

    device->GetDisplayModes = N3DS_GetDisplayModes;

    device->CreateSDLWindow = N3DS_CreateWindow;
    device->DestroyWindow = N3DS_DestroyWindow;

    device->HasScreenKeyboardSupport = N3DS_HasScreenKeyboardSupport;
    device->StartTextInput = N3DS_StartTextInput;
    device->StopTextInput = N3DS_StopTextInput;

    device->PumpEvents = N3DS_PumpEvents;

    device->CreateWindowFramebuffer = SDL_N3DS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_N3DS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_N3DS_DestroyWindowFramebuffer;

    device->num_displays = 2;

    device->free = N3DS_DeleteDevice;

    return device;
}

VideoBootStrap N3DS_bootstrap = { N3DSVID_DRIVER_NAME, "N3DS Video Driver", N3DS_CreateDevice };

static int
N3DS_VideoInit(_THIS)
{
    AddN3DSDisplay(GFX_TOP);
    AddN3DSDisplay(GFX_BOTTOM);

    N3DS_SwkbInit();

    return 0;
}

SDL_FORCE_INLINE void
AddN3DSDisplay(gfxScreen_t screen)
{
    SDL_DisplayMode mode;
    SDL_VideoDisplay display;

    SDL_zero(mode);
    SDL_zero(display);

    mode.w = (screen == GFX_TOP) ? GSP_SCREEN_HEIGHT_TOP : GSP_SCREEN_HEIGHT_BOTTOM;
    mode.h = GSP_SCREEN_WIDTH;
    mode.refresh_rate = 60;
    mode.format = FRAMEBUFFER_FORMAT;
    mode.driverdata = NULL;

    display.desktop_mode = mode;
    display.current_mode = mode;
    display.driverdata = NULL;

    SDL_AddVideoDisplay(&display, SDL_FALSE);
}

static void
N3DS_VideoQuit(_THIS)
{
    N3DS_SwkbQuit();
    return;
}

static void
N3DS_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    /* Each display only has a single mode */
    SDL_AddDisplayMode(display, &display->current_mode);
}

static int
N3DS_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *drv_data = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (drv_data == NULL) {
        return SDL_OutOfMemory();
    }

    if (window->flags & SDL_WINDOW_N3DS_BOTTOM) {
        drv_data->screen = GFX_BOTTOM;
    } else {
        drv_data->screen = GFX_TOP;
    }

    window->driverdata = drv_data;
    return 0;
}

static void
N3DS_DestroyWindow(_THIS, SDL_Window *window)
{
    if (window == NULL) {
        return;
    }
    SDL_free(window->driverdata);
}

#endif /* SDL_VIDEO_DRIVER_N3DS */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
