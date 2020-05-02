/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2020 Jay Petacat <jay@jayschwa.net>

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

#include "SDL_svga_video.h"

#include "../../core/dos/SDL_dos.h"

#include "SDL_svga_events.h"
#include "SDL_svga_framebuffer.h"
#include "SDL_svga_mouse.h"

#define SVGAVID_DRIVER_NAME "svga"

/* Mandatory mode attributes */
#define VBE_MODE_ATTRS (VBE_MODE_ATTR_GRAPHICS_MODE | VBE_MODE_ATTR_LINEAR_MEM_AVAIL)

/* Initialization/Query functions */
static int SVGA_VideoInit(_THIS);
static void SVGA_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
static int SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void SVGA_VideoQuit(_THIS);
static int SVGA_CreateWindow(_THIS, SDL_Window * window);
static void SVGA_DestroyWindow(_THIS, SDL_Window * window);

/* SVGA driver bootstrap functions */

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
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SVGA: VESA BIOS Extensions v2.0 or greater is required");
        SDL_Unsupported();
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
    device->CreateSDLWindow = SVGA_CreateWindow;
    device->DestroyWindow = SVGA_DestroyWindow;
    device->CreateWindowFramebuffer = SDL_SVGA_CreateFramebuffer;
    device->UpdateWindowFramebuffer = SDL_SVGA_UpdateFramebuffer;
    device->DestroyWindowFramebuffer = SDL_SVGA_DestroyFramebuffer;

    device->free = SVGA_DeleteDevice;

    return device;
}

VideoBootStrap SVGA_bootstrap = {
    SVGAVID_DRIVER_NAME, "SDL SVGA video driver",
    SVGA_CreateDevice
};

static int
SVGA_VideoInit(_THIS)
{
    SDL_DeviceData *devdata = _this->driverdata;

    /* Save original video mode. */
    if (SVGA_GetCurrentVBEMode(&devdata->original_mode, NULL)) {
        return SDL_SetError("Couldn't query current video mode");
    }

    /* TODO: Use mode info if it exists. */

    if (SDL_AddBasicVideoDisplay(NULL) < 0) {
        return -1;
    }

    /* Save original video state. */
    devdata->state_size = SVGA_GetState(&devdata->original_state);
    if (devdata->state_size < 0) {
        return -1;
    }

    /* Initialize keyboard. */
    /* TODO: Just move keyboard stuff under this module and rename to DOS! */
    if (SDL_DOS_Init()) {
        return -1;
    }

    DOS_InitMouse();

    return 0;
}

static void
SVGA_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DeviceData *devdata = _this->driverdata;
    SDL_DisplayMode mode;
    VBEMode vbe_mode;
    int index = 0;

    SDL_zero(mode);

    for (
        vbe_mode = SVGA_GetVBEModeAtIndex(&devdata->vbe_info, index++);
        vbe_mode != VBE_MODE_LIST_END;
        vbe_mode = SVGA_GetVBEModeAtIndex(&devdata->vbe_info, index++)
    ) {
        SDL_DisplayModeData *modedata;
        VBEModeInfo info;
        int status = SVGA_GetVBEModeInfo(vbe_mode, &info);

        if (status) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "SVGA_GetVBEModeInfo failed: %d", status);
            return;
        }

        /* Mode must support graphics with a linear framebuffer. */
        if ((info.mode_attributes & VBE_MODE_ATTRS) != VBE_MODE_ATTRS) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "SVGA: Ignoring mode 0x%X: Bad attributes", vbe_mode);
            continue;
        }

        /* Mode must be a known pixel format. */
        mode.format = SVGA_GetPixelFormat(&info);
        if (mode.format == SDL_PIXELFORMAT_UNKNOWN) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "SVGA: Ignoring mode 0x%X: Bad pixel format", vbe_mode);
            continue;
        }

        /* Mode must be capable of double buffering. */
        if (!info.number_of_image_pages) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "SVGA: Ignoring mode 0x%X: No double-buffering", vbe_mode);
            continue;
        }

        /* Scan lines must be 4-byte aligned to match SDL surface pitch. */
        if (info.bytes_per_scan_line % 4 != 0) {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "SVGA: Ignoring mode 0x%X: Bad pitch", vbe_mode);
            continue;
        }

        /* Allocate display mode internal data. */
        modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(*modedata));
        if (!modedata) {
            return;
        }

        mode.w = info.x_resolution;
        mode.h = info.y_resolution;
        mode.driverdata = modedata;
        modedata->vbe_mode = vbe_mode;
        modedata->framebuffer_phys_addr = info.phys_base_ptr;

        if (!SDL_AddDisplayMode(display, &mode)) {
            SDL_free(modedata);
        }
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "SVGA: VBE lists %d modes", index - 1);
}

static int
SVGA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    SDL_DisplayModeData *modedata = mode->driverdata;

    if (!modedata) {
        return SDL_SetError("Missing display mode data");
    }

    if (SVGA_SetVBEMode(modedata->vbe_mode)) {
        /* TODO: Include VBE error message. */
        return SDL_SetError("Couldn't set VBE display mode");
    }

    /* TODO: Switch to 8 bit palette format, if possible and relevant. */

    DOS_InitMouse(); /* TODO: Is this necessary when video mode changes? */

    return 0;
}

static void
SVGA_VideoQuit(_THIS)
{
    SDL_DeviceData *devdata = _this->driverdata;

    /* Restore original video state. */
    if (devdata->original_state) {
        SVGA_SetState(devdata->original_state, devdata->state_size);
        SDL_free(devdata->original_state);
    }

    /* Restore original video mode. */
    if (devdata->original_mode) {
        SVGA_SetVBEMode(devdata->original_mode);
    }

    SDL_DOS_Quit();
    DOS_QuitMouse();
}

static int
SVGA_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata;

    /* TODO: Allow only one window. */

    /* Allocate window internal data. */
    windata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (!windata) {
        return SDL_OutOfMemory();
    }
    window->driverdata = windata;

    /* Set framebuffer selector to sentinel value. */
    windata->framebuffer_selector = -1;

    /* Window is always fullscreen. */
    /* QUESTION: Is this appropriate, or should an error be returned instead? */
    window->flags |= SDL_WINDOW_FULLSCREEN;

    return 0;
}

static void
SVGA_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_free(window->driverdata);
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
