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
    NativeDisplayType display = (NativeDisplayType)((SDL_VideoData *)_this->driverdata)->gbm_dev;
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

/*********************************/
/* Atomic functions block        */
/*********************************/

#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

static EGLSyncKHR create_fence(int fd, _THIS)
{
	EGLint attrib_list[] = {
		EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd,
		EGL_NONE,
	};
	EGLSyncKHR fence = _this->egl_data->eglCreateSyncKHR(_this->egl_data->egl_display,
			EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);
	assert(fence);
	return fence;
}

int
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    int ret;

    /*************************************************************************/
    /* Block for telling KMS to wait for GPU rendering of the current frame  */
    /* before applying the KMS changes requested in the atomic ioctl.        */
    /*************************************************************************/

    /* Create the fence that will be inserted in the cmdstream exactly at the end
       of the gl commands that form a frame. KMS will have to wait on it before doing a pageflip. */
    dispdata->gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(dispdata->gpu_fence);

    /* Mark, at EGL level, the buffer that we want to become the new front buffer.
       However, it won't really happen until we request a pageflip at the KMS level and it completes. */
    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

    /* It's safe to get the gpu_fence FD now, because eglSwapBuffers flushes it down the cmdstream, 
       so it's now in place in the cmdstream.
       Atomic ioctl will pass the in-fence fd into the kernel, telling KMS that it has to wait for GPU to
       finish rendering the frame before doing the changes requested in the atomic ioct (pageflip in this case). */
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID(_this->egl_data->egl_display, dispdata->gpu_fence);
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->gpu_fence);
    assert(dispdata->kms_in_fence_fd != -1);

    /***************/
    /* Block ends. */
    /***************/

    /* Lock the buffer that is marked by eglSwapBuffers() to become the next front buffer (so it can not
       be chosen by EGL as back buffer to draw on), and get a handle to it to request the pageflip on it. */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
	return SDL_SetError("Failed to lock frontbuffer");
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
	 return SDL_SetError("Failed to get a new framebuffer BO");
    }

    /* Add the pageflip to te request list. */
    drm_atomic_setbuffer(_this, dispdata->display_plane, fb->fb_id);

    /* Issue the one and only atomic commit where all changes will be requested!.
       We need e a non-blocking atomic commit for triple buffering, because we 
       must not block on this atomic commit so we can re-enter program loop once more. */
    ret = drm_atomic_commit(_this, SDL_FALSE);

    if (ret) {
        return SDL_SetError("failed to issue atomic commit");
    }

    /* Release the last front buffer so EGL can chose it as back buffer and render on it again. */
    if (windata->bo) {
	KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
    }

    /* Take note of current front buffer, so we can free it next time we come here. */
    windata->bo = windata->next_bo;

    /**************************************************************************/
    /* Block for telling GPU to wait for completion of requested KMS changes  */
    /* before starting cmdstream execution (=next frame rendering).           */
    /**************************************************************************/

    /* Import the kms fence from the out fence fd. We need it to tell GPU to wait for pageflip to complete. */
    dispdata->kms_fence = create_fence(dispdata->kms_out_fence_fd, _this);
    assert(dispdata->kms_fence);

    /* Reset out fence FD value because the fence is now away from us, on the driver side. */
    dispdata->kms_out_fence_fd = -1;

    /* Tell the GPU to wait until the requested pageflip has completed. */
    _this->egl_data->eglWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0);

    /***************/
    /* Block ends. */
    /***************/

    return ret;
}

int
KMSDRM_GLES_SwapWindowDB(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    int ret;

    /* In double-buffer mode, atomic commit will always be synchronous/blocking (ie: won't return until
       the requested changes are done).
       Also, there's no need to fence KMS or the GPU, because we won't be entering game loop again
       (hence not building or executing a new cmdstring) until pageflip is done. */

    /* Mark, at EGL level, the buffer that we want to become the new front buffer.
       However, it won't really happen until we request a pageflip at the KMS level and it completes. */
    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

    /* Lock the buffer that is marked by eglSwapBuffers() to become the next front buffer (so it can not
       be chosen by EGL as back buffer to draw on), and get a handle to it to request the pageflip on it. */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
        return SDL_SetError("Failed to lock frontbuffer");
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
         return SDL_SetError("Failed to get a new framebuffer BO");
    }

    /* Add the pageflip to te request list. */
    drm_atomic_setbuffer(_this, dispdata->display_plane, fb->fb_id);

    /* Issue the one and only atomic commit where all changes will be requested!. 
       Blocking for double buffering: won't return until completed. */
    ret = drm_atomic_commit(_this, SDL_TRUE);

    if (ret) {
        return SDL_SetError("failed to issue atomic commit");
    }

    /* Release last front buffer so EGL can chose it as back buffer and render on it again. */
    if (windata->bo) {
	KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
    }

    /* Take note of current front buffer, so we can free it next time we come here. */
    windata->bo = windata->next_bo;

    return ret;
}


/***************************************/
/* End of Atomic functions block       */
/***************************************/

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
