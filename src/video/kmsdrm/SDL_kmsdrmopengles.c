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

#if SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL

#include "SDL_log.h"

#include "SDL_kmsdrmvideo.h"
#include "SDL_kmsdrmopengles.h"
#include "SDL_kmsdrmdyn.h"

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

/* EGL implementation of SDL OpenGL support */

int
KMSDRM_GLES_LoadLibrary(_THIS, const char *path) {
    NativeDisplayType display = (NativeDisplayType)((SDL_VideoData *)_this->driverdata)->gbm;
    return SDL_EGL_LoadLibrary(_this, path, display, EGL_PLATFORM_GBM_MESA);
}

SDL_EGL_CreateContext_impl(KMSDRM)

int KMSDRM_GLES_SetSwapInterval(_THIS, int interval) {
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }

    if (interval == 0 || interval == 1) {
        _this->egl_data->egl_swapinterval = interval;
    } else {
        return SDL_SetError("Only swap intervals of 0 or 1 are supported");
    }

    return 0;
}

int
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window) {
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    SDL_VideoData *viddata = ((SDL_VideoData *)_this->driverdata);
    KMSDRM_FBInfo *fb_info;
    int ret, timeout;

    /* Recreate the GBM / EGL surfaces if the display mode has changed */
    if (windata->egl_surface_dirty) {
        KMSDRM_CreateSurfaces(_this, window);
    }

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
    if (windata->curr_bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
        /* SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Released GBM surface %p", (void *)windata->curr_bo); */
        windata->curr_bo = NULL;
    }

    windata->curr_bo = windata->next_bo;

    /* Make the current back buffer the next front buffer */
    if (!(_this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface))) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "eglSwapBuffers failed.");
        return 0;
    }

    /* Lock the next front buffer so it can't be allocated as a back buffer */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not lock GBM surface front buffer");
        return 0;
    /* } else {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Locked GBM surface %p", (void *)windata->next_bo); */
    }

    fb_info = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb_info) {
        return 0;
    }

    if (!windata->curr_bo) {
        /* On the first swap, immediately present the new front buffer. Before
           drmModePageFlip can be used the CRTC has to be configured to use
           the current connector and mode with drmModeSetCrtc */
        ret = KMSDRM_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id, 0,
                                    0, &dispdata->conn->connector_id, 1, &dispdata->mode);

        if (ret) {
          SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not configure CRTC");
        }
    } else {
        /* On subsequent swaps, queue the new front buffer to be flipped during
           the next vertical blank */
        ret = KMSDRM_drmModePageFlip(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id,
                                     DRM_MODE_PAGE_FLIP_EVENT, &windata->waiting_for_flip);
        /* SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "drmModePageFlip(%d, %u, %u, DRM_MODE_PAGE_FLIP_EVENT, &windata->waiting_for_flip)",
            viddata->drm_fd, displaydata->crtc_id, fb_info->fb_id); */

        if (_this->egl_data->egl_swapinterval == 1) {
            if (ret == 0) {
                windata->waiting_for_flip = SDL_TRUE;
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not queue pageflip: %d", ret);
            }
        }

        /* Wait immediately for vsync (as if we only had two buffers), for low input-lag scenarios.
           Run your SDL2 program with "SDL_KMSDRM_DOUBLE_BUFFER=1 <program_name>" to enable this. */
        if (_this->egl_data->egl_swapinterval == 1 && windata->double_buffer) {
            KMSDRM_WaitPageFlip(_this, windata, -1);
        }
    }

    return 0;
}

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
