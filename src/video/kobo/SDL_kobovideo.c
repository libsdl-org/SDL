/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

/* Kobo (MediaTek "hwtcon" e-ink) video driver -- ADDED for the Kobo port,
   not part of upstream SDL2.

   Draws to /dev/fb0 directly (Kobo has no X server, and the MediaTek panel is
   a plain fbdev, not DRM/KMS) and refreshes the e-ink via the hwtcon ioctls.
   Targeted/tested on the Kobo Elipsa 2E (MT8110, 1404x1872, 32bpp). */

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_KOBO

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "SDL_video.h"
#include "SDL_hints.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_kobovideo.h"
#include "SDL_koboevents_c.h"
#include "SDL_koboframebuffer_c.h"
#include "SDL_kobo_hwtcon.h"

#define KOBOVID_DRIVER_NAME "kobo"

/* Initialization/Query functions */
static int  KOBO_VideoInit(_THIS);
static int  KOBO_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void KOBO_VideoQuit(_THIS);

/* Window functions (minimal: one fullscreen-ish e-ink window). */
static int  KOBO_CreateWindow(_THIS, SDL_Window *window);
static void KOBO_DestroyWindow(_THIS, SDL_Window *window);

static void KOBO_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *KOBO_CreateDevice(void)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    device->VideoInit = KOBO_VideoInit;
    device->VideoQuit = KOBO_VideoQuit;
    device->SetDisplayMode = KOBO_SetDisplayMode;
    device->PumpEvents = KOBO_PumpEvents;

    device->CreateWindowFramebuffer = SDL_KOBO_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_KOBO_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_KOBO_DestroyWindowFramebuffer;

    device->CreateSDLWindow = KOBO_CreateWindow;
    device->DestroyWindow = KOBO_DestroyWindow;

    device->free = KOBO_DeleteDevice;

    return device;
}

VideoBootStrap KOBO_bootstrap = {
    KOBOVID_DRIVER_NAME, "SDL Kobo (MediaTek hwtcon) e-ink video driver",
    KOBO_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};

static int KOBO_VideoInit(_THIS)
{
    KOBO_VideoData *data;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    const char *dev;
    SDL_DisplayMode mode;

    data = (KOBO_VideoData *)SDL_calloc(1, sizeof(KOBO_VideoData));
    if (!data) {
        return SDL_OutOfMemory();
    }

    dev = SDL_GetHint("SDL_KOBO_FBDEV");
    if (!dev || !*dev) {
        dev = "/dev/fb0";
    }

    data->fd = open(dev, O_RDWR);
    if (data->fd < 0) {
        SDL_free(data);
        return SDL_SetError("Kobo: couldn't open framebuffer %s", dev);
    }

    if (ioctl(data->fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(data->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(data->fd);
        SDL_free(data);
        return SDL_SetError("Kobo: FBIOGET_*SCREENINFO failed");
    }

    data->width   = (int)vinfo.xres;
    data->height  = (int)vinfo.yres;
    data->bpp     = vinfo.bits_per_pixel;
    data->pitch   = (int)finfo.line_length;
    data->map_len = finfo.smem_len;

    if (data->bpp != 32) {
        /* The luma-replication blit assumes a 32bpp scanout (Elipsa 2E). */
        close(data->fd);
        SDL_free(data);
        return SDL_SetError("Kobo: expected 32bpp framebuffer, got %u", (unsigned)data->bpp);
    }

    data->map = (Uint8 *)mmap(NULL, data->map_len, PROT_READ | PROT_WRITE,
                              MAP_SHARED, data->fd, 0);
    if (data->map == MAP_FAILED) {
        data->map = NULL;
        close(data->fd);
        SDL_free(data);
        return SDL_SetError("Kobo: mmap of framebuffer failed");
    }

    _this->driverdata = data;

    /* Clear to white and do one full flashing update for a clean first light. */
    memset(data->map, 0xFF, data->map_len);
    SDL_KOBO_RefreshFull(data, HWTCON_WAVEFORM_MODE_GC16);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = data->width;
    mode.h = data->height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    return 0;
}

static int KOBO_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    (void)display;
    (void)mode;
    return 0;
}

static void KOBO_VideoQuit(_THIS)
{
    KOBO_VideoData *data = (KOBO_VideoData *)_this->driverdata;

    if (data) {
        if (data->map && data->map != MAP_FAILED) {
            munmap(data->map, data->map_len);
        }
        if (data->fd >= 0) {
            close(data->fd);
        }
        SDL_free(data);
        _this->driverdata = NULL;
    }
}

static int KOBO_CreateWindow(_THIS, SDL_Window *window)
{
    KOBO_VideoData *data = (KOBO_VideoData *)_this->driverdata;

    /* Single e-ink surface: pin the window to the panel. */
    if (data) {
        window->x = 0;
        window->y = 0;
        window->w = data->width;
        window->h = data->height;
    }
    window->driverdata = NULL;

    SDL_SetKeyboardFocus(window);
    SDL_SetMouseFocus(window);
    return 0;
}

static void KOBO_DestroyWindow(_THIS, SDL_Window *window)
{
    (void)window;
}

#endif /* SDL_VIDEO_DRIVER_KOBO */

/* vi: set ts=4 sw=4 expandtab: */
