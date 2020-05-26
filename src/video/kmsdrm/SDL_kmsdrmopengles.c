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
    SDL_bool crtc_setup_pending = SDL_FALSE;

    /* ALWAYS wait for each pageflip to complete before issuing another, vsync or not,
       or drmModePageFlip() will start returning EBUSY if there are pending pageflips.

       To disable vsync in games, it would be needed to issue async pageflips,
       and then wait for each pageflip to complete. Since async pageflips complete ASAP 
       instead of VBLANK, thats how non-vsync screen updates should wok.

       BUT Async pageflips do not work right now because calling drmModePageFlip() with the
       DRM_MODE_PAGE_FLIP_ASYNC flag returns error on every driver I have tried.

       So, for now, only do vsynced updates: _this->egl_data->egl_swapinterval is
       ignored for now, it makes no sense to use it until async pageflips work on drm drivers. */

    /* Recreate the GBM / EGL surfaces if the display mode has changed */
    if (windata->egl_surface_dirty) {
        KMSDRM_CreateSurfaces(_this, window);
        /* Do this later, when a fb_id is obtained. */
        crtc_setup_pending = SDL_TRUE;
    }

    if (windata->double_buffer) {
        /* Use a double buffering scheme, independently of the number of buffers that the GBM surface has,
           (number of buffers on the GBM surface depends on the implementation).
           Double buffering (instead of triple) is achieved by waiting for synchronous pageflip to complete
           inmediately after the pageflip is issued. That way, in the end of this function only two buffers
           are needed: a buffer that is available to be picked again by EGL as a backbuffer to draw on it,
           and the new front buffer that has just been set up.

           Since programmer has no control over the number of buffers of the GBM surface, wait for pageflip
           is done inmediately after issuing pageflip, and so a double-buffer scheme is achieved. */

        /* Ask EGL to mark the current back buffer to become the next front buffer. 
           That will happen when a pageflip is issued, and the next vsync arrives (sync flip)
           or ASAP (async flip). */
        if (!(_this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface))) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "eglSwapBuffers failed.");
            return 0;
        }

        /* Get a handler to the buffer that is marked to become the next front buffer, and lock it
           so it can not be chosen by EGL as a back buffer. */
        windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
        if (!windata->next_bo) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not lock GBM surface front buffer");
            return 0;
        /* } else {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Locked GBM surface %p", (void *)windata->next_bo); */
        }

        /* Issue synchronous pageflip: drmModePageFlip() NEVER blocks, synchronous here means that it
           will be done on next VBLANK, not ASAP. And return to program loop inmediately. */

        fb_info = KMSDRM_FBFromBO(_this, windata->next_bo);
        if (!fb_info) {
            return 0;
        }

        /* When needed, this is done once we have the needed fb_id, not before. */
        if (crtc_setup_pending) {
	    if (KMSDRM_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id, 0,
					0, &dispdata->conn->connector_id, 1, &dispdata->mode)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not configure CRTC on video mode setting.");
	    }
            crtc_setup_pending = SDL_FALSE;
        }

        if (!KMSDRM_drmModePageFlip(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id,
                                    DRM_MODE_PAGE_FLIP_EVENT, &windata->waiting_for_flip)) {
            windata->waiting_for_flip = SDL_TRUE;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not issue pageflip");
        }

        /* Since issued pageflips are always synchronous (ASYNC dont currently work), these pageflips
           will happen at next vsync, so in practice waiting for vsync is being done here. */ 
        if (!KMSDRM_WaitPageFlip(_this, windata, -1)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error waiting for pageflip event");
            return 0;
        }

        /* Return the previous front buffer to the available buffer pool of the GBM surface,
           so it can be chosen again by EGL as the back buffer for drawing into it. */
        if (windata->curr_bo) {
            KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
            /* SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Released GBM surface buffer %p", (void *)windata->curr_bo); */
            windata->curr_bo = NULL;
        }

        /* Take note of the current front buffer, so it can be freed next time this function is called. */
        windata->curr_bo = windata->next_bo;
    } else {
        /* Triple buffering requires waiting for last pageflip upon entering instead of waiting at the end,
           and issuing the next pageflip at the end, thus allowing the program loop to run 
           while the issued pageflip arrives (at next VBLANK, since ONLY synchronous pageflips are possible).
           In a game context, this means that the player can be doing inputs before seeing the last
           completed frame, causing "input lag" that is known to plage other APIs and backends.
           Triple buffering requires the use of three different buffers at the end of this function:
           1- the front buffer which is on screen,
           2- the back buffer wich is ready to be flipped (a pageflip has been issued on it, which has yet to complete)
           3- a third buffer that can be used by EGL to draw while the previously issued pageflip arrives
              (should not put back the previous front buffer into the free buffers pool of the
              GBM surface until that happens).
           If the implementation only has two buffers for the GBM surface, this would behave like a double buffer.
        */
        
        /* Wait for previously issued pageflip to complete. */
        if (!KMSDRM_WaitPageFlip(_this, windata, -1)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error waiting for pageflip event");
            return 0;
        }

        /* Free the previous front buffer so EGL can pick it again as back buffer.*/
        if (windata->curr_bo) {
            KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
            /* SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Released GBM surface buffer %p", (void *)windata->curr_bo); */
            windata->curr_bo = NULL;
        }

        /* Ask EGL to mark the current back buffer to become the next front buffer. 
           That will happen when a pageflip is issued, and the next vsync arrives (sync flip)
           or ASAP (async flip). */
        if (!(_this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface))) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "eglSwapBuffers failed.");
            return 0;
        }

        /* Take note of the current front buffer, so it can be freed next time this function is called. */
        windata->curr_bo = windata->next_bo;

        /* Get a handler to the buffer that is marked to become the next front buffer, and lock it
           so it can not be chosen by EGL as a back buffer. */
        windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
        if (!windata->next_bo) {
             SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not lock GBM surface front buffer");
             return 0;
	/* } else {
             SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Locked GBM surface %p", (void *)windata->next_bo); */
        }

        /* Issue synchronous pageflip: drmModePageFlip() NEVER blocks, synchronous here means that it
           will be done on next VBLANK, not ASAP. And return to program loop inmediately. */
        fb_info = KMSDRM_FBFromBO(_this, windata->next_bo);
        if (!fb_info) {
            return 0;
        }

        /* When needed, this is done once we have the needed fb_id, not before. */
        if (crtc_setup_pending) {
	    if (KMSDRM_drmModeSetCrtc(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id, 0,
					0, &dispdata->conn->connector_id, 1, &dispdata->mode)) {
		SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not configure CRTC on video mode setting.");
	    }
            crtc_setup_pending = SDL_FALSE;
        }


        if (!KMSDRM_drmModePageFlip(viddata->drm_fd, dispdata->crtc_id, fb_info->fb_id,
                                    DRM_MODE_PAGE_FLIP_EVENT, &windata->waiting_for_flip)) {
            windata->waiting_for_flip = SDL_TRUE;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not issue pageflip");
        }
    }

    return 0;
}

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
