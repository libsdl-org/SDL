/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PS3

#include "SDL_PS3video.h"
#include "../SDL_sysvideo.h"

#include <assert.h>

void PS3_InitModes(SDL_VideoDevice *_this)
{
    int rv = 0;
    (void)rv;
    deprintf(1, "+PS3_InitModes()\n");
    SDL_DisplayMode mode;
    PS3_DisplayModeData *modedata;
    videoOutState state;

    modedata = (PS3_DisplayModeData *) SDL_malloc(sizeof(*modedata));
    if (!modedata) {
        return;
    }

    rv = videoOutGetState(0, 0, &state);
    assert( rv == 0); // Get the state of the display
    assert(state.state == 0); // Make sure display is enabled

    // Get the current resolution
    videoOutResolution res;
    rv = videoOutGetResolution(state.displayMode.resolution, &res);
    assert(rv == 0);

    // Setting up the DisplayMode based on current settings
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.refresh_rate = 0;
    mode.w = res.width;
    mode.h = res.height;

    modedata->vconfig.resolution = state.displayMode.resolution;
    modedata->vconfig.format = VIDEO_OUT_BUFFER_FORMAT_XRGB;
    modedata->vconfig.pitch = res.width * 4;
    mode.internal = modedata;

    rv = videoOutConfigure(0, &modedata->vconfig, NULL, 1);
    assert(rv == 0);

    // Wait until RSX is ready
    do {
        SDL_Delay(10);
        rv = videoOutGetState(0, 0, &state);
        assert( rv == 0);
    } while ( state.state == 3);

    SDL_AddBasicVideoDisplay(&mode);

    deprintf(1, "-PS3_InitModes()\n");
}

/* DisplayModes available on the PS3 */
static SDL_DisplayMode ps3fb_modedb[] = {
    /* Native resolutions (progressive, "fullscreen") */
    {SDL_PIXELFORMAT_ARGB8888, 1920, 1080, 0, 0}, // 1080p
    {SDL_PIXELFORMAT_ARGB8888, 1280, 720, 0, 0}, // 720p
    {SDL_PIXELFORMAT_ARGB8888, 720, 480, 0, 0}, // 480p
    {SDL_PIXELFORMAT_ARGB8888, 720, 576, 0, 0}, // 576p
};

static PS3_DisplayModeData ps3fb_data[] = {
    // { resolution, format, aspect, padding, pitch }
    {{
        VIDEO_OUT_RESOLUTION_1080, 
        VIDEO_OUT_BUFFER_FORMAT_XRGB,
        VIDEO_OUT_ASPECT_16_9, 
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        1920 * 4
    }},
    {{
        VIDEO_OUT_RESOLUTION_720, 
        VIDEO_OUT_BUFFER_FORMAT_XRGB,
        VIDEO_OUT_ASPECT_16_9, 
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        1280 * 4
    }},
    {{
        VIDEO_OUT_RESOLUTION_480, 
        VIDEO_OUT_BUFFER_FORMAT_XRGB,
        VIDEO_OUT_ASPECT_16_9, 
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        720 * 4
    }},
    {{
        VIDEO_OUT_RESOLUTION_576, 
        VIDEO_OUT_BUFFER_FORMAT_XRGB,
        VIDEO_OUT_ASPECT_16_9, 
        {0, 0, 0, 0, 0, 0, 0, 0, 0},
        720 * 4
    }},
};

bool PS3_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay * display)
{
    deprintf(1, "+PS3_GetDisplayModes()\n");
    unsigned int nummodes;

    nummodes = sizeof(ps3fb_modedb) / sizeof(SDL_DisplayMode);

    int n;
    for (n=0; n<nummodes; ++n) {
        PS3_DisplayModeData *data = (PS3_DisplayModeData *)malloc(sizeof(PS3_DisplayModeData));
        *data = ps3fb_data[n];
        // Get driver specific mode data
        ps3fb_modedb[n].internal = data;

        // Add DisplayMode to list
        deprintf(2, "Adding resolution %u x %u\n", ps3fb_modedb[n].w, ps3fb_modedb[n].h);
        SDL_AddFullscreenDisplayMode(display, &ps3fb_modedb[n]);
    }
    deprintf(1, "-PS3_GetDisplayModes()\n");
    return true;
}

bool PS3_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    deprintf(1, "+PS3_SetDisplayMode()\n");
    PS3_DisplayModeData *dispdata = (PS3_DisplayModeData *) mode->internal;
    videoOutState state;

    /* Set the new DisplayMode */
    deprintf(2, "Setting PS3_MODE to %u\n", dispdata->vconfig.resolution);
    if ( videoOutConfigure(0, &dispdata->vconfig, NULL, 0) != 0)
    {
        deprintf(2, "Could not set PS3FB_MODE\n");
        SDL_SetError("Could not set PS3FB_MODE\n");
        return false;
    }

    // Wait until RSX is ready
    do {
        SDL_Delay(10);
        int rv = videoOutGetState(0, 0, &state);
        assert( rv == 0);
    }  while ( state.state == 3);

    deprintf(1, "-PS3_SetDisplayMode()\n");
    return true;
}

void PS3_QuitModes(SDL_VideoDevice *_this)
{
    // No need to do anything here.
    // SDL_ResetFullscreenDisplayModes will be called on SDL_Quit.
}

#endif // SDL_VIDEO_DRIVER_PS3

/* vi: set ts=4 sw=4 expandtab: */
