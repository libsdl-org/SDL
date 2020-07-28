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
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window) {

    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    int ret;

    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK;

    EGLSyncKHR gpu_fence = NULL;   /* out-fence from gpu, in-fence to kms */
    EGLSyncKHR kms_fence = NULL;   /* in-fence to gpu, out-fence from kms */

    /* Allow modeset (which is done inside atomic_commit). */
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

    if (dispdata->kms_out_fence_fd != -1) {
	kms_fence = create_fence(dispdata->kms_out_fence_fd, _this);
	assert(kms_fence);

	/* driver now has ownership of the fence fd: */
	dispdata->kms_out_fence_fd = -1;

	/* wait "on the gpu" (ie. this won't necessarily block, but
	 * will block the rendering until fence is signaled), until
	 * the previous pageflip completes so we don't render into
	 * the buffer that is still on screen.
	 */
	_this->egl_data->eglWaitSyncKHR(_this->egl_data->egl_display, kms_fence, 0);
    }

    /* insert fence to be singled in cmdstream.. this fence will be
     * signaled when gpu rendering done
     */
    gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(gpu_fence);

    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface);

    /* after swapbuffers, gpu_fence should be flushed, so safe to get the fd: */
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID(_this->egl_data->egl_display, gpu_fence);
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, gpu_fence);
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

    if (kms_fence) {
	EGLint status;

	/* Wait on the CPU side for the _previous_ commit to
	 * complete before we post the flip through KMS, as
	 * atomic will reject the commit if we post a new one
	 * whilst the previous one is still pending.
	 */
	do {
	    status = _this->egl_data->eglClientWaitSyncKHR(_this->egl_data->egl_display,
							     kms_fence,
							     0,
							     EGL_FOREVER_KHR);
	} while (status != EGL_CONDITION_SATISFIED_KHR);

	_this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, kms_fence);
    }

    /*
     * Here you could also update drm plane layers if you want
     * hw composition
     */
    ret = drm_atomic_commit(_this, fb->fb_id, flags);
    if (ret) {
	printf("failed to do atomic commit\n");
	return -1;
    }

    /* release last buffer to render on again: */
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
