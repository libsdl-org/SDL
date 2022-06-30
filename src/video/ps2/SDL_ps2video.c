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

#if SDL_VIDEO_DRIVER_PS2

/* PS2 SDL video driver implementation; this is just enough to make an
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
 *  SDL video driver.  Renamed to "PS2" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_ps2video.h"
#include "SDL_ps2events_c.h"
#include "SDL_ps2framebuffer_c.h"
#include "SDL_hints.h"

#define PS2VID_DRIVER_NAME "ps2"

/* Initialization/Query functions */
static int PS2_VideoInit(_THIS);
static int PS2_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void PS2_VideoQuit(_THIS);

/* PS2 driver bootstrap functions */

static void
PS2_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
PS2_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return (0);
    }
    device->is_dummy = SDL_TRUE;

    /* Set the function pointers */
    device->VideoInit = PS2_VideoInit;
    device->VideoQuit = PS2_VideoQuit;
    device->SetDisplayMode = PS2_SetDisplayMode;
    device->PumpEvents = PS2_PumpEvents;
    device->CreateWindowFramebuffer = SDL_PS2_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_PS2_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_PS2_DestroyWindowFramebuffer;

    device->free = PS2_DeleteDevice;

    return device;
}

VideoBootStrap PS2_bootstrap = {
    PS2VID_DRIVER_NAME, "SDL PS2 video driver",
    PS2_CreateDevice
};

int
PS2_VideoInit(_THIS)
{
    /*
	ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);

	gsGlobal = gsKit_init_global();

	gsGlobal->Mode = gsKit_check_rom();
	if (gsGlobal->Mode == GS_MODE_PAL){
		gsGlobal->Height = 512;
	} else {
		gsGlobal->Height = 448;
	}

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	printf("\nGraphics: created %ix%i video surface\n",
		gsGlobal->Width, gsGlobal->Height);

	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	gsKit_vram_clear(gsGlobal);

	gsKit_init_screen(gsGlobal);

	gsKit_TexManager_init(gsGlobal);

	gsKit_add_vsync_handler(vsync_handler);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x80,0x00,0x00,0x80,0x00));	

	if (gsGlobal->DoubleBuffering == GS_SETTING_OFF) {
		gsKit_sync(gsGlobal);
		gsKit_queue_exec(gsGlobal);
    } else {
		gsKit_queue_exec(gsGlobal);
		gsKit_finish();
		gsKit_sync(gsGlobal);
		gsKit_flip(gsGlobal);
	}
	gsKit_TexManager_nextFrame(gsGlobal);
    */

    SDL_DisplayMode mode;

    /* Use a fake 32-bpp desktop mode */
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.w = 640;
    /*if (gsGlobal->Mode == GS_MODE_PAL){
        mode.h = 512;
    } else {
        mode.h = 448;
    }*/
    mode.h = 448;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }

    SDL_AddDisplayMode(&_this->displays[0], &mode);

    /* We're done! */
    return 0;
}

static int
SDL_to_PS2_PFM(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_RGBA5551:
        return GS_PSM_CT16S;
    case SDL_PIXELFORMAT_RGB24:
        return GS_PSM_CT24;
    case SDL_PIXELFORMAT_ABGR32:
        return GS_PSM_CT32;
    default:
        return GS_PSM_CT24;
    }
}

static int
PS2_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
PS2_VideoQuit(_THIS)
{
    /*gsKit_deinit_global(gsGlobal);*/
}

#endif /* SDL_VIDEO_DRIVER_PS2 */

/* vi: set ts=4 sw=4 expandtab: */
