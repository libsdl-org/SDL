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

#define SVGAVID_DRIVER_NAME "svga"

/* Initialization/Query functions */
static int SVGA_VideoInit(_THIS);
static void SVGA_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
static int SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void SVGA_VideoQuit(_THIS);

/* SVGA driver bootstrap functions */

static int
SVGA_Available(void)
{
    VBEInfo info;

    return SVGA_GetVBEInfo(&info) == 0 && info.vbe_version.major >= 2;
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
    SDL_DeviceData *devdata;

    devdata = (SDL_DeviceData *) SDL_calloc(1, sizeof(*devdata));
    if (!devdata) {
        SDL_OutOfMemory();
        return NULL;
    }

    if (SVGA_GetVBEInfo(&devdata->vbe_info) || devdata->vbe_info.vbe_version.major < 2) {
        SDL_free(devdata);
        return NULL;
    }

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(*device));
    if (!device) {
        SDL_free(devdata);
        SDL_OutOfMemory();
        return NULL;
    }

    device->driverdata = devdata;

    /* Set the function pointers */
    device->VideoInit = SVGA_VideoInit;
    device->VideoQuit = SVGA_VideoQuit;
    device->GetDisplayModes = SVGA_GetDisplayModes;
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

static int
SVGA_VideoInit(_THIS)
{
    /* TODO: Query for current mode. */

    if (SDL_AddBasicVideoDisplay(NULL) < 0) {
        return -1;
    }

    /* TODO: Save original video state. */

    return 0;
}

static void
SVGA_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DeviceData *devdata = _this->driverdata;
    VBEMode vbe_mode;
    int index = 0;

    vbe_mode = SVGA_GetVBEModeAtIndex(&devdata->vbe_info, index++);

    while (vbe_mode != VBE_MODE_LIST_END) {
        SDL_DisplayMode mode;
        SDL_DisplayModeData *modedata;
        VBEModeInfo info;

        if (SVGA_GetVBEModeInfo(vbe_mode, &info)) {
            return;
        }

        /* TODO: Filter out banked memory and weird color formats. */

        modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(*modedata));
        if (!modedata) {
            return;
        }

        mode.w = info.x_resolution;
        mode.h = info.y_resolution;
        mode.format = SDL_PIXELFORMAT_INDEX8; /* FIXME: Select correct color format. */
        mode.refresh_rate = 0;
        mode.driverdata = modedata;
        modedata->vbe_mode = vbe_mode;
        modedata->framebuffer_phys_addr = (void *)(info.phys_base_ptr.segment * 16 + info.phys_base_ptr.offset);
        modedata->framebuffer_size = 0x1000; /* FIXME: Set correct framebuffer memory size. */

        if (!SDL_AddDisplayMode(display, &mode)) {
            SDL_free(modedata);
        }

        vbe_mode = SVGA_GetVBEModeAtIndex(&devdata->vbe_info, index++);
    }
}

static int
SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

static void
SVGA_VideoQuit(_THIS)
{
    /* TODO: Restore original video state. */
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
