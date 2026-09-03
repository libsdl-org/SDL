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

#ifdef SDL_VIDEO_DRIVER_PLAYDATE

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

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_playdate_video.h"
#include "SDL_playdate_events_c.h"
#include "SDL_playdate_framebuffer_c.h"

#include "pd_api.h"

#define PLAYDATEVID_DRIVER_NAME "playdate"

/* Initialization/Query functions */
static int PLAYDATE_VideoInit(_THIS);
static int PLAYDATE_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void PLAYDATE_VideoQuit(_THIS);

/* playdate driver bootstrap functions */

static int
PLAYDATE_Available(void)
{
    return 1;
}

static void
PLAYDATE_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
PLAYDATE_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    if (!PLAYDATE_Available()) {
        return (0);
    }

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return (0);
    }
    device->is_dummy = SDL_FALSE;

    /* Set the function pointers */
    device->VideoInit = PLAYDATE_VideoInit;
    device->VideoQuit = PLAYDATE_VideoQuit;
    device->SetDisplayMode = PLAYDATE_SetDisplayMode;
    device->PumpEvents = PLAYDATE_PumpEvents;
    device->CreateWindowFramebuffer = SDL_PLAYDATE_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_PLAYDATE_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_PLAYDATE_DestroyWindowFramebuffer;

    device->free = PLAYDATE_DeleteDevice;

    return device;
}

VideoBootStrap PLAYDATE_bootstrap = {
    PLAYDATEVID_DRIVER_NAME, "SDL playdate video driver",
    PLAYDATE_CreateDevice
};


int
PLAYDATE_VideoInit(_THIS)
{
    SDL_DisplayMode mode;
    /* Use a fake 32-bpp desktop mode */
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 400;
    mode.h = 240;
    mode.refresh_rate = 50;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_AddDisplayMode(&_this->displays[0], &mode);

    /* We're done! */
    return 0;
}

static int
PLAYDATE_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
PLAYDATE_VideoQuit(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_PLAYDATE */

/* vi: set ts=4 sw=4 expandtab: */
