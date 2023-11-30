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

#ifdef SDL_VIDEO_DRIVER_N3DS

#include "../SDL_sysvideo.h"
#include "SDL_n3dsevents_c.h"
#include "SDL_n3dsframebuffer_c.h"
#include "SDL_n3dsswkb.h"
#include "SDL_n3dstouch.h"
#include "SDL_n3dsvideo.h"

#define N3DSVID_DRIVER_NAME "n3ds"

static int AddN3DSDisplay(gfxScreen_t screen);

static int N3DS_VideoInit(SDL_VideoDevice *_this);
static void N3DS_VideoQuit(SDL_VideoDevice *_this);
static int N3DS_GetDisplayBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect);
static int N3DS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
static void N3DS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);

struct SDL_DisplayData
{
    gfxScreen_t screen;
};

/* N3DS driver bootstrap functions */

static void N3DS_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *N3DS_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return 0;
    }

    device->VideoInit = N3DS_VideoInit;
    device->VideoQuit = N3DS_VideoQuit;

    device->GetDisplayBounds = N3DS_GetDisplayBounds;

    device->CreateSDLWindow = N3DS_CreateWindow;
    device->DestroyWindow = N3DS_DestroyWindow;

    device->HasScreenKeyboardSupport = N3DS_HasScreenKeyboardSupport;
    device->StartTextInput = N3DS_StartTextInput;
    device->StopTextInput = N3DS_StopTextInput;

    device->PumpEvents = N3DS_PumpEvents;

    device->CreateWindowFramebuffer = SDL_N3DS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_N3DS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_N3DS_DestroyWindowFramebuffer;

    device->free = N3DS_DeleteDevice;

    return device;
}

VideoBootStrap N3DS_bootstrap = { N3DSVID_DRIVER_NAME, "N3DS Video Driver", N3DS_CreateDevice };

static int N3DS_VideoInit(SDL_VideoDevice *_this)
{
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
    hidInit();

    AddN3DSDisplay(GFX_TOP);
    AddN3DSDisplay(GFX_BOTTOM);

    N3DS_InitTouch();
    N3DS_SwkbInit();

    return 0;
}

static int AddN3DSDisplay(gfxScreen_t screen)
{
    SDL_DisplayMode mode;
    SDL_VideoDisplay display;
    SDL_DisplayData *display_driver_data = SDL_calloc(1, sizeof(SDL_DisplayData));
    if (!display_driver_data) {
        return -1;
    }

    SDL_zero(mode);
    SDL_zero(display);

    display_driver_data->screen = screen;

    mode.w = (screen == GFX_TOP) ? GSP_SCREEN_HEIGHT_TOP : GSP_SCREEN_HEIGHT_BOTTOM;
    mode.h = GSP_SCREEN_WIDTH;
    mode.refresh_rate = 60.0f;
    mode.format = FRAMEBUFFER_FORMAT;

    display.name = (screen == GFX_TOP) ? "N3DS top screen" : "N3DS bottom screen";
    display.desktop_mode = mode;
    display.driverdata = display_driver_data;

    SDL_AddVideoDisplay(&display, SDL_FALSE);
    return 0;
}

static void N3DS_VideoQuit(SDL_VideoDevice *_this)
{
    N3DS_SwkbQuit();
    N3DS_QuitTouch();

    hidExit();
    gfxExit();
}

static int N3DS_GetDisplayBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    SDL_DisplayData *driver_data = display->driverdata;

    if (!driver_data) {
        return -1;
    }

    rect->x = 0;
    rect->y = (driver_data->screen == GFX_TOP) ? 0 : GSP_SCREEN_WIDTH;
    rect->w = display->current_mode->w;
    rect->h = display->current_mode->h;
    return 0;
}

static int N3DS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_DisplayData *display_data;
    SDL_WindowData *window_data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!window_data) {
        return -1;
    }
    display_data = SDL_GetDisplayDriverDataForWindow(window);
    window_data->screen = display_data->screen;
    window->driverdata = window_data;
    SDL_SetKeyboardFocus(window);
    return 0;
}

static void N3DS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (!window) {
        return;
    }
    SDL_free(window->driverdata);
}

#endif /* SDL_VIDEO_DRIVER_N3DS */
