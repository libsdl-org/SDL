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

/*
-We have to stop the GPU from executing the cmdstream again because the front butter
 has been marked as back buffer so it can be selected by EGL to draw on it BUT
 the pageflip has not completed, so it's still on screen, so letting GPU render on it
 would cause tearing. (kms_fence)
-We have to stop the DISPLAY from doing the changes we requested in the atomic ioctl,
 like the pageflip, until the GPU has completed the cmdstream execution,
 because we don't want the pageflip to be done in the middle of a frame rendering.
 (gpu_fence).
-We have to stop the program from doing a new atomic ioctl until the previous one
has been finished. (kms_fence)
*/

int
KMSDRM_GLES_SwapWindowDOUBLE(_THIS, SDL_Window * window) {

    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;

    EGLint status;
    int ret;
    
    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK;

    EGLSyncKHR kms_in_fence  = NULL;
    EGLSyncKHR kms_out_fence = NULL;

    /* Create the display in-fence, and export it as a fence fd to pass into the kernel. */
    kms_in_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(kms_in_fence);
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID(_this->egl_data->egl_display, kms_in_fence);
    
    /* Mark the back buffer as the next front buffer, and the current front buffer as elegible
       by EGL as back buffer to draw into.
       This will not really happen until we post the pageflip though KMS with the atomic ioctl
       and it completes on next vsync. */
    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

   /* Lock the buffer that is marked by eglSwapBuffers() to become the next front buffer (so it can not
      be chosen by EGL as back buffer to draw on), and get a handle to it, so we can use that handle 
      to request the KMS pageflip that will set it as front buffer. */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
	printf("Failed to lock frontbuffer\n");
	return -1;
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
	 printf("Failed to get a new framebuffer BO\n");
	 return -1;
    }

    /* 
     * Issue atomic pageflip to post the pageflip thru KMS. Returns immediately.
     * Never issue an atomic ioctl while the previuos one is pending: it will be rejected. 
     */
    ret = drm_atomic_commit(_this, fb->fb_id, flags);
    if (ret) {
	printf("failed to do atomic commit\n");
	return -1;
    }

    /* Get the display out fence, returned by the atomic ioctl. */
    kms_out_fence = create_fence(dispdata->kms_out_fence_fd, _this);
    assert(kms_out_fence);

    /* Wait on the CPU side for the _previous_ commit to complete before we post the flip through KMS,
     * because atomic will reject the commit if we post a new one while the previous one is still pending. */
    do {
	status = _this->egl_data->eglClientWaitSyncKHR(_this->egl_data->egl_display, kms_out_fence, 0, EGL_FOREVER_KHR);
    } while (status != EGL_CONDITION_SATISFIED_KHR);

    /* Destroy both in and out display fences. */
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, kms_out_fence);
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, kms_in_fence);

    /* Now that the pageflip is complete, release last front buffer so EGL can chose it
     * as back buffer and render on it again: */
    if (windata->curr_bo) {
	KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
	windata->curr_bo = NULL;
    }

    /* Take note of the current front buffer, so it can be freed next time this function is called. */
    windata->curr_bo = windata->next_bo;

    return ret;
}


int
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window) {

    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    int ret;

    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK;

    /* Create the fence that will be inserted in the cmdstream exactly at the end
       of the gl commands that form a frame. KMS will have to wait on it before doing a pageflip. */
    dispdata->gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(dispdata->gpu_fence);

    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

    /* It's safe to get the gpu_fence FD now, because eglSwapBuffers flushes it
       down the cmdstream, so it's now in place in the cmdstream.
       Atomic ioctl will pass the in-fence fd into the kernel. */
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID(_this->egl_data->egl_display, dispdata->gpu_fence);
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->gpu_fence);
    assert(dispdata->kms_in_fence_fd != -1);

    /* Lock the buffer that is marked by eglSwapBuffers() to become the next front buffer (so it can not
      be chosen by EGL as back buffer to draw on), and get a handle to it to request the pageflip on it. */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
	printf("Failed to lock frontbuffer\n");
	return -1;
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
	 printf("Failed to get a new framebuffer BO\n");
	 return -1;
    }


    /* Don't issue another atomic ioctl until previous one has completed. */
    if (dispdata->kms_fence) {
	EGLint status;

	do {
	    status = _this->egl_data->eglClientWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0, EGL_FOREVER_KHR);
	} while (status != EGL_CONDITION_SATISFIED_KHR);

	_this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->kms_fence);
    }


    /* Issue atomic commit. */
    ret = drm_atomic_commit(_this, fb->fb_id, flags);
    if (ret) {
	printf("failed to do atomic commit\n");
	return -1;
    }



    /* release last front buffer so EGL can chose it as back buffer and render on it again: */
    if (windata->curr_bo) {
	KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
	windata->curr_bo = NULL;
    }

    /* Take note of the current front buffer, so it can be freed next time this function is called. */
    windata->curr_bo = windata->next_bo;

    /* Import out fence from the out fence fd and tell the GPU to wait on it
       until the requested pageflip has completed. */
    dispdata->kms_fence = create_fence(dispdata->kms_out_fence_fd, _this);
    assert(dispdata->kms_fence);

    dispdata->kms_out_fence_fd = -1;

    _this->egl_data->eglWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0);

    return ret;

}

int
KMSDRM_GLES_SwapWindowOLD(_THIS, SDL_Window * window) {

    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    int ret;

    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK;

    dispdata->kms_fence = NULL;   /* in-fence to gpu, out-fence from kms */
    dispdata->gpu_fence = NULL;   /* in-fence to kms, out-fence from gpu, */

    /* Allow modeset (which is done inside atomic_commit). */
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;


    /* Import out fence from the out fence fd and tell the GPU to wait on it
       until the requested pageflip has completed. */
    dispdata->kms_fence = create_fence(dispdata->kms_out_fence_fd, _this);
    assert(dispdata->kms_fence);

    dispdata->kms_out_fence_fd = -1;


    _this->egl_data->eglWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0);


    /* GL DRAW */

    /* Create the gpu fence here so it's inserted in the cmdstream exactly
       at the end of the gl commands that form a frame. */
    dispdata->gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(dispdata->gpu_fence);

    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

    /* It's safe to get the gpu_fence fd now, because eglSwapBuffers() flushes the fd. */
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID(_this->egl_data->egl_display, dispdata->gpu_fence);

    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->gpu_fence);
    assert(dispdata->kms_in_fence_fd != -1);

    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
	printf("Failed to lock frontbuffer\n");
	return -1;
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
	 printf("Failed to get a new framebuffer BO\n");
	 return -1;
    }

    if (dispdata->kms_fence) {
	EGLint status;

	do {
	    status = _this->egl_data->eglClientWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0, EGL_FOREVER_KHR);
	} while (status != EGL_CONDITION_SATISFIED_KHR);

	_this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->kms_fence);
    }


    /* Issue atomic commit. */
    ret = drm_atomic_commit(_this, fb->fb_id, flags);
    if (ret) {
	printf("failed to do atomic commit\n");
	return -1;
    }



    /* release last front buffer so EGL can chose it as back buffer and render on it again: */
    if (windata->curr_bo) {
	KMSDRM_gbm_surface_release_buffer(windata->gs, windata->curr_bo);
	windata->curr_bo = NULL;
    }

    /* Take note of the current front buffer, so it can be freed next time this function is called. */
    windata->curr_bo = windata->next_bo;

    /* Allow a modeset change for the first commit only. */
    flags &= ~(DRM_MODE_ATOMIC_ALLOW_MODESET);


    return ret;
}

/***************************************/
/* End of Atomic functions block       */
/***************************************/

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
