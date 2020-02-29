/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_SVGA

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_svga_video.h"
#include "SDL_svga_events.h"
#include "SDL_svga_framebuffer.h"
#include "SDL_svga_vbe.h"

#define SVGAVID_DRIVER_NAME "svga"

/* Initialization/Query functions */
static int SVGA_VideoInit(_THIS);
static int SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void SVGA_VideoQuit(_THIS);

/* SVGA driver bootstrap functions */

static int
SVGA_Available(void)
{
    VBEInfo info;

    return SDL_SVGA_GetVBEInfo(&info) == 0 && info.vbe_version >= 0x0200;
}

static void
SVGA_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
SVGA_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    VBEInfo info;

    if (SDL_SVGA_GetVBEInfo(&info) || info.vbe_version < 0x0200) {
        return 0;
    }

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    /* Set the function pointers */
    device->VideoInit = SVGA_VideoInit;
    device->VideoQuit = SVGA_VideoQuit;
    device->SetDisplayMode = SVGA_SetDisplayMode;
    device->PumpEvents = SVGA_PumpEvents;
    device->CreateWindowFramebuffer = SDL_SVGA_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_SVGA_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_SVGA_DestroyWindowFramebuffer;

    device->free = SVGA_DeleteDevice;

    return device;
}

VideoBootStrap SVGA_bootstrap = {
    SVGAVID_DRIVER_NAME, "SDL SVGA video driver",
    SVGA_Available, SVGA_CreateDevice
};


int
SVGA_VideoInit(_THIS)
{
    SDL_DisplayMode mode;

    mode.format = SDL_PIXELFORMAT_INDEX8;
    mode.w = 320;
    mode.h = 200;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_zero(mode);
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    /* We're done! */
    return 0;
}

static int
SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
SVGA_VideoQuit(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
