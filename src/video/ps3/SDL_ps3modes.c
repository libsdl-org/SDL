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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PS3

#include "../SDL_sysvideo.h"
#include "SDL_ps3video.h"


bool PS3_InitModes(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    PS3_DisplayModeData *modedata;
    videoState state;

    modedata = (PS3_DisplayModeData *)SDL_malloc(sizeof(*modedata));
    if (!modedata) {
        return false;
    }
    s32 res = videoGetState(0, 0, &state);
    // Make sure display is enabled
    if (res != 0 || state.state != 0) {
        return SDL_SetError("PS3 video state setup");
    }

    // Get the current resolution
    videoResolution resolution;
    res = videoGetResolution(state.displayMode.resolution, &resolution);
    if (res != 0) {
        return SDL_SetError("PS3 video resolution setup");
    }

    // Setting up the DisplayMode based on current settings
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.refresh_rate = 0;
    mode.w = resolution.width;
    mode.h = resolution.height;

    modedata->vconfig.resolution = state.displayMode.resolution;
    modedata->vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    modedata->vconfig.pitch = resolution.width * 4;
    mode.internal = modedata;

    res = videoConfigure(VIDEO_PRIMARY, &modedata->vconfig, NULL, 0);
    if (res != 0) {
        return SDL_SetError("PS3 video configure setup");
    }

    // Wait until RSX is ready
    do {
        SDL_Delay(10);
        videoGetState(0, 0, &state);
    } while (state.state == 3);

    SDL_AddBasicVideoDisplay(&mode);

    return true;
}

// DisplayModes available on the PS3
static SDL_DisplayMode ps3fb_modedb[] = {
    // Native resolutions (progressive, "fullscreen")
    { SDL_PIXELFORMAT_ARGB8888, 1920, 1080, 0, 0 }, // 1080p
    { SDL_PIXELFORMAT_ARGB8888, 1280, 720, 0, 0 },  // 720p
    { SDL_PIXELFORMAT_ARGB8888, 720, 480, 0, 0 },   // 480p
    { SDL_PIXELFORMAT_ARGB8888, 720, 576, 0, 0 },   // 576p
};

static PS3_DisplayModeData ps3fb_data[] = {
    // { resolution, format, aspect, padding, pitch }
    { { VIDEO_RESOLUTION_1080,
        VIDEO_BUFFER_FORMAT_XRGB,
        VIDEO_ASPECT_16_9,
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        1920 * 4 } },
    { { VIDEO_RESOLUTION_720,
        VIDEO_BUFFER_FORMAT_XRGB,
        VIDEO_ASPECT_16_9,
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        1280 * 4 } },
    { { VIDEO_RESOLUTION_480,
        VIDEO_BUFFER_FORMAT_XRGB,
        VIDEO_ASPECT_16_9,
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        720 * 4 } },
    { { VIDEO_RESOLUTION_576,
        VIDEO_BUFFER_FORMAT_XRGB,
        VIDEO_ASPECT_16_9,
        { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        720 * 4 } },
};

bool PS3_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display)
{
    unsigned int nummodes = sizeof(ps3fb_modedb) / sizeof(SDL_DisplayMode);

    for (int n = 0; n < nummodes; ++n) {
        PS3_DisplayModeData *data = (PS3_DisplayModeData *)malloc(sizeof(PS3_DisplayModeData));
        *data = ps3fb_data[n];
        // Get driver specific mode data
        ps3fb_modedb[n].internal = data;

        // Add DisplayMode to the list
        SDL_AddFullscreenDisplayMode(display, &ps3fb_modedb[n]);
    }
    return true;
}

bool PS3_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    PS3_DisplayModeData *dispdata = (PS3_DisplayModeData *)mode->internal;
    videoState state;

    // Set the new DisplayMode
    if (videoConfigure(0, &dispdata->vconfig, NULL, 0) != 0) {
        return SDL_SetError("PS3 video configure display");
    }

    // Wait until RSX is ready
    do {
        SDL_Delay(10);
        videoGetState(0, 0, &state);
    } while (state.state == 3);

    return true;
}

void PS3_QuitModes(SDL_VideoDevice *_this)
{
    // No need to do anything here.
    // SDL_ResetFullscreenDisplayModes will be called on SDL_Quit.
}

#endif // SDL_VIDEO_DRIVER_PS3
