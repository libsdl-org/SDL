/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_RISCOS

/* Dummy SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_version.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_riscosvideo.h"
#include "SDL_riscosevents_c.h"
#include "SDL_riscosframebuffer_c.h"

#include <kernel.h>
#include <swis.h>

#define RISCOSVID_DRIVER_NAME "riscos"

/* Initialization/Query functions */
static int RISCOS_VideoInit(_THIS);
static int RISCOS_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void RISCOS_VideoQuit(_THIS);
SDL_bool RISCOS_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info);

/* RISC OS driver bootstrap functions */

static void
RISCOS_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
RISCOS_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = RISCOS_VideoInit;
    device->VideoQuit = RISCOS_VideoQuit;
    device->SetDisplayMode = RISCOS_SetDisplayMode;
    device->PumpEvents = RISCOS_PumpEvents;

    device->GetWindowWMInfo = RISCOS_GetWindowWMInfo;

    device->CreateWindowFramebuffer = SDL_RISCOS_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_RISCOS_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_RISCOS_DestroyWindowFramebuffer;

    device->free = RISCOS_DeleteDevice;

    return device;
}

VideoBootStrap RISCOS_bootstrap = {
    RISCOSVID_DRIVER_NAME, "SDL RISC OS video driver",
    RISCOS_CreateDevice
};

enum {
    MODE_FLAG_565 = 1 << 7,

    MODE_FLAG_COLOUR_SPACE = 0xF << 12,

    MODE_FLAG_TBGR = 0,
    MODE_FLAG_TRGB = 1 << 14,
    MODE_FLAG_ABGR = 1 << 15,
    MODE_FLAG_ARGB = MODE_FLAG_TRGB | MODE_FLAG_ABGR
};

#define MODE_350(type, xdpi, ydpi) \
        (1 | (xdpi << 1) | (ydpi << 14) | (type << 27))
#define MODE_521(type, xeig, yeig, flags) \
        (0x78000001 | (xeig << 4) | (yeig << 6) | (flags & 0xFF00) | (type << 20))

static const struct {
    SDL_PixelFormatEnum pixel_format;
    int modeflags, ncolour, log2bpp, sprite_type;
} mode_to_pixelformat[] = {
    { SDL_PIXELFORMAT_INDEX1LSB, 0, 1, 0, 1 },
    /* { SDL_PIXELFORMAT_INDEX2LSB, 0, 3, 1, 2 }, */
    { SDL_PIXELFORMAT_INDEX4LSB, 0, 15, 2, 3 },
    { SDL_PIXELFORMAT_INDEX8,    MODE_FLAG_565, 255, 3, 4 },
    { SDL_PIXELFORMAT_BGR555,    MODE_FLAG_TBGR, 65535, 4, 5 },
    { SDL_PIXELFORMAT_RGB555,    MODE_FLAG_TRGB, 65535, 4, 5 },
    { SDL_PIXELFORMAT_ABGR1555,  MODE_FLAG_ABGR, 65535, 4, 5 },
    { SDL_PIXELFORMAT_ARGB1555,  MODE_FLAG_ARGB, 65535, 4, 5 },
    { SDL_PIXELFORMAT_BGR444,    MODE_FLAG_TBGR, 4095, 4, 16 },
    { SDL_PIXELFORMAT_RGB444,    MODE_FLAG_TRGB, 4095, 4, 16 },
    { SDL_PIXELFORMAT_ABGR4444,  MODE_FLAG_ABGR, 4095, 4, 16 },
    { SDL_PIXELFORMAT_ARGB4444,  MODE_FLAG_ARGB, 4095, 4, 16 },
    { SDL_PIXELFORMAT_BGR565,    MODE_FLAG_TBGR | MODE_FLAG_565, 65535, 4, 10 },
    { SDL_PIXELFORMAT_RGB565,    MODE_FLAG_TRGB | MODE_FLAG_565, 65535, 4, 10 },
    { SDL_PIXELFORMAT_BGR24,     MODE_FLAG_TBGR, 16777215, 6, 8 },
    { SDL_PIXELFORMAT_RGB24,     MODE_FLAG_TRGB, 16777215, 6, 8 },
    { SDL_PIXELFORMAT_BGR888,    MODE_FLAG_TBGR, -1, 5, 6 },
    { SDL_PIXELFORMAT_RGB888,    MODE_FLAG_TRGB, -1, 5, 6 },
    { SDL_PIXELFORMAT_ABGR8888,  MODE_FLAG_ABGR, -1, 5, 6 },
    { SDL_PIXELFORMAT_ARGB8888,  MODE_FLAG_ARGB, -1, 5, 6 }
};

static int ReadModeVariable(int mode, int variable) {
    _kernel_swi_regs regs;
    regs.r[0] = mode;
    regs.r[1] = variable;
    _kernel_swi(OS_ReadModeVariable, &regs, &regs);
    return regs.r[2];
}

SDL_PixelFormatEnum RISCOS_ModeToPixelFormat(int *mode) {
    int i, log2bpp, ncolour, modeflags;
    log2bpp = ReadModeVariable((int)mode, 9);
    ncolour = ReadModeVariable((int)mode, 3);
    modeflags = ReadModeVariable((int)mode, 0);

    for (i = 0; i < SDL_arraysize(mode_to_pixelformat); i++) {
        if (log2bpp == mode_to_pixelformat[i].log2bpp &&
           (ncolour == mode_to_pixelformat[i].ncolour || ncolour == 0) &&
           (modeflags & (MODE_FLAG_565 | MODE_FLAG_COLOUR_SPACE)) == mode_to_pixelformat[i].modeflags) {
            return mode_to_pixelformat[i].pixel_format;
        }
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

static void *copy_mode_block(int *storeBlock)
{
    int *outBlock;
    int blockSize = (storeBlock[0] == 2) ? 7 : 5;
    while(storeBlock[blockSize] != -1) {
        blockSize += 2;
    }
    blockSize++;

    outBlock = SDL_calloc(sizeof(int), blockSize);
    SDL_memcpy(outBlock, storeBlock, sizeof(int) * blockSize);
    return outBlock;
}

int
RISCOS_VideoInit(_THIS)
{
    SDL_DisplayMode mode;
    int *current_mode;
    _kernel_swi_regs regs;
    _kernel_oserror *error;

    regs.r[0] = 1;
    error = _kernel_swi(OS_ScreenMode, &regs, &regs);
    if (error != NULL) {
        SDL_SetError("Unable to retrieve the current screen mode: %s (%i)", error->errmess, error->errnum);
        return -1;
    }
    current_mode = (int *)regs.r[1];

    /* Use a fake 32-bpp desktop mode */
    mode.w = current_mode[1];
    mode.h = current_mode[2];
    if ((current_mode[0] & 0x7F) == 1) {
        mode.format = RISCOS_ModeToPixelFormat(current_mode);
        mode.refresh_rate = current_mode[4];
    } else if ((current_mode[0] & 0x7F) == 3) {
        mode.format = RISCOS_ModeToPixelFormat(current_mode);
        mode.refresh_rate = current_mode[6];
    } else {
        return -1;
    }
    mode.driverdata = copy_mode_block(current_mode);

    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_AddDisplayMode(&_this->displays[0], &mode);

    /* We're done! */
    return 0;
}

static int
RISCOS_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
RISCOS_VideoQuit(_THIS)
{
}

SDL_bool RISCOS_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info) {
    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_RISCOS;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

#endif /* SDL_VIDEO_DRIVER_RISCOS */

/* vi: set ts=4 sw=4 expandtab: */
