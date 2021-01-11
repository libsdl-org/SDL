/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_KMSDRM

#include "SDL_log.h"

#include "SDL_kmsdrmvideo.h"
#include "SDL_kmsdrmopengles.h"
#include "SDL_kmsdrmdyn.h"
#include <errno.h>

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

/* EGL implementation of SDL OpenGL support */

void
KMSDRM_GLES_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor)
{
    /* if SDL was _also_ built with the Raspberry Pi driver (so we're
       definitely a Pi device), default to GLES2. */
#if SDL_VIDEO_DRIVER_RPI
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
#endif
}

int
KMSDRM_GLES_LoadLibrary(_THIS, const char *path) {
    /* Just pretend you do this here, but don't do it until KMSDRM_CreateWindow(),
       where we do the same library load we would normally do here.
       because this gets called by SDL_CreateWindow() before KMSDR_CreateWindow(),
       so gbm dev isn't yet created when this is called, AND we can't alter the
       call order in SDL_CreateWindow(). */
#if 0
    NativeDisplayType display = (NativeDisplayType)((SDL_VideoData *)_this->driverdata)->gbm_dev;
    return SDL_EGL_LoadLibrary(_this, path, display, EGL_PLATFORM_GBM_MESA);
#endif
    return 0;
}

void
KMSDRM_GLES_UnloadLibrary(_THIS) {
    /* As with KMSDRM_GLES_LoadLibrary(), we define our own "dummy" unloading function
       so we manually unload the library whenever we want. */
}

SDL_EGL_CreateContext_impl(KMSDRM)

int KMSDRM_GLES_SetSwapInterval(_THIS, int interval) {

    /*
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }

    if (interval == 0 || interval == 1) {
        _this->egl_data->egl_swapinterval = interval;
    } else {
        return SDL_SetError("Only swap intervals of 0 or 1 are supported");
    }*/

    /* Issuing a new pageflip before the previous has completed
       causes drmModePageFlip() to return EBUSY errors. Don't do it. */
    _this->egl_data->egl_swapinterval = 1;

    return 0;
}

/* Check if we need to re-create the GBM and EGL surfaces and setup a new mode on the CRTC
   matching the new GBM surface size so the buffers from these surfaces can be displayed
   on the CRTC (remember we need the CRTC mode and the buffer size to match). */
/*KMSDRM_CheckKMSDRM (_THIS, window) {


}*/

int
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window) {
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    KMSDRM_FBInfo *fb_info;
    int ret, timeout;

    /* Wait for confirmation that the next front buffer has been flipped, at which
       point the previous front buffer can be released */
    timeout = 0;
    if (_this->egl_data->egl_swapinterval == 1) {
        timeout = -1;
    }
    if (!KMSDRM_WaitPageFlip(_this, windata, timeout)) {
        return 0;
    }

    /* Release the previous front buffer */
    if (windata->bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
        windata->bo = NULL;
    }

    windata->bo = windata->next_bo;

    /* Mark a buffer to becume the next front buffer.
       This won't happen until pagelip completes. */
    if (!(_this->egl_data->eglSwapBuffers(_this->egl_data->egl_display,
                                           windata->egl_surface))) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "eglSwapBuffers failed.");
        return 0;
    }

    /* From the GBM surface, get the next BO to become the next front buffer,
       and lock it so it can't be allocated as a back buffer (to prevent EGL
       from drawing into it!) */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not lock GBM surface front buffer");
        return 0;
    }

    /* Get an actual usable fb for the next front buffer. */
    fb_info = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb_info) {
        return 0;
    }

#if 0
    /* Get an actual usable fb (of the specified dimensions) form the BO,
       so we can pass it to the CRTC. */
    fb_info = KMSDRM_FBFromBO2(_this, windata->next_bo, window->w, window->h);
    if (!fb_info) {
        return 0;
    }
#endif

#if 0
    /* If the window size does not match the current videomode configured on the CRTC,
       update the current video mode and configure the new mode on the CRTC.  */
    if (dispdata->mode.hdisplay != window->w || dispdata->mode.vdisplay != window->h) { 
        for (int i = 0; i < dispdata->connector->count_modes; i++) {
            drmModeModeInfo mode = dispdata->connector->modes[i] ;
            if (mode.hdisplay == window->w && mode.vdisplay == window->h) {
                dispdata->mode = mode;
	        ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd,
		     dispdata->crtc->crtc_id, fb_info->fb_id, 0, 0,
		     &dispdata->connector->connector_id, 1, &dispdata->mode);

	        if (ret) {
		    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not set mode on CRTC.");
		    printf("********Could not set mode on CRTC.\n");
                }
                return 0;
            }
        }
        return 0;
    }
#endif

#if 0
    /* Get an actual usable fb for the next front buffer. */
    fb_info = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb_info) {
        return 0;
    }
#endif

#if 1 
    /* Do we have a modeset pending? If so, configure the new mode on the CRTC.
       Has to be done before the upcoming pageflip issue, so the buffer with the
       new size is big enough so the CRTC doesn't read out of bounds. */
    if (dispdata->modeset_pending) {

        ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd,
          dispdata->crtc->crtc_id, fb_info->fb_id, 0, 0,
          &dispdata->connector->connector_id, 1, &dispdata->mode);

        dispdata->modeset_pending = SDL_FALSE;

        if (ret) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not set videomode on CRTC.");
        }

        return 0;
    }
#endif

    /* Issue pageflip on the next front buffer.
       The pageflip will be done during the next vblank. */
    ret = KMSDRM_drmModePageFlip(viddata->drm_fd, dispdata->crtc->crtc_id,
	     fb_info->fb_id, DRM_MODE_PAGE_FLIP_EVENT, &windata->waiting_for_flip);

    if (ret == 0) {
        if (_this->egl_data->egl_swapinterval == 1) {
	    windata->waiting_for_flip = SDL_TRUE;
        }
    } else {
	SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not queue pageflip: %d", ret);
    }

    /* If we are in double-buffer mode, wait immediately for vsync
       (as if we only had two buffers),
       Run your SDL2 program with "SDL_KMSDRM_DOUBLE_BUFFER=1 <program_name>"
       to enable this. */
    if (_this->egl_data->egl_swapinterval == 1 && windata->double_buffer) {
	KMSDRM_WaitPageFlip(_this, windata, -1);
    }

    return 0;
}

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM */

/* vi: set ts=4 sw=4 expandtab: */
