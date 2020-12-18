/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
  Atomic KMSDRM backend by Manuel Alfayate Corchete <redwindwanderer@gmail.com>

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
#include "SDL_hints.h"

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

#ifndef EGL_SYNC_NATIVE_FENCE_ANDROID
#define EGL_SYNC_NATIVE_FENCE_ANDROID     0x3144
#endif

#ifndef EGL_SYNC_NATIVE_FENCE_FD_ANDROID
#define EGL_SYNC_NATIVE_FENCE_FD_ANDROID  0x3145
#endif

#ifndef EGL_NO_NATIVE_FENCE_FD_ANDROID
#define EGL_NO_NATIVE_FENCE_FD_ANDROID    -1
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
    /* As with KMSDRM_GLES_LoadLibrary(), we define our own unloading function so
       we manually unload the library whenever we want. */
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

    EGLSyncKHR fence = _this->egl_data->eglCreateSyncKHR
        (_this->egl_data->egl_display, EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);

    assert(fence);
    return fence;
}

/***********************************************************************************/
/* Comments about buffer access protection mechanism (=fences) are the ones boxed. */
/* Also, DON'T remove the asserts: if a fence-related call fails, it's better that */
/* program exits immediately, or we could leave KMS waiting for a failed/missing   */
/* fence forevever.                                                                */
/***********************************************************************************/
int
KMSDRM_GLES_SwapWindowFenced(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    KMSDRM_PlaneInfo info = {0};

    /******************************************************************/
    /* Create the GPU-side FENCE OBJECT. It will be inserted into the */
    /* GL CMDSTREAM exactly at the end of the gl commands that form a */
    /* frame.(KMS will have to wait on it before doing a pageflip.)   */
    /******************************************************************/
    dispdata->gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID, _this);
    assert(dispdata->gpu_fence);

    /******************************************************************/
    /* eglSwapBuffers flushes the fence down the GL CMDSTREAM, so we  */
    /* know for sure it's there now.                                  */
    /* Also it marks, at EGL level, the buffer that we want to become */
    /* the new front buffer. (Remember that won't really happen until */
    /* we request a pageflip at the KMS level and it completes.       */
    /******************************************************************/
    if (! _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface)) {
        return SDL_EGL_SetError("Failed to swap EGL buffers", "eglSwapBuffers");
    }

    /******************************************************************/
    /* EXPORT the GPU-side FENCE OBJECT to the fence INPUT FD, so we  */
    /* can pass it into the kernel. Atomic ioctl will pass the        */
    /* in-fence fd into the kernel, thus telling KMS that it has to   */
    /* wait for GPU to finish rendering the frame (remember where we  */
    /* put the fence in the GL CMDSTREAM) before doing the changes    */
    /* requested in the atomic ioct (the pageflip in this case).      */
    /* (We export the GPU-side FENCE OJECT to the fence INPUT FD now, */
    /* not sooner, because now we are sure that the GPU-side fence is */
    /* in the CMDSTREAM to be lifted when the CMDSTREAM to this point */
    /* is completed).                                                 */
    /******************************************************************/
    dispdata->kms_in_fence_fd = _this->egl_data->eglDupNativeFenceFDANDROID (_this->egl_data->egl_display,
        dispdata->gpu_fence);
    
    _this->egl_data->eglDestroySyncKHR(_this->egl_data->egl_display, dispdata->gpu_fence);
    assert(dispdata->kms_in_fence_fd != -1);

    /* Lock the buffer that is marked by eglSwapBuffers() to become the
       next front buffer (so it can not be chosen by EGL as back buffer
       to draw on), and get a handle to it to request the pageflip on it.
       REMEMBER that gbm_surface_lock_front_buffer() ALWAYS has to be
       called after eglSwapBuffers(). */
    windata->next_bo = KMSDRM_gbm_surface_lock_front_buffer(windata->gs);
    if (!windata->next_bo) {
        return SDL_SetError("Failed to lock frontbuffer");
    }
    fb = KMSDRM_FBFromBO(_this, windata->next_bo);
    if (!fb) {
        return SDL_SetError("Failed to get a new framebuffer from BO");
    }

    /* Add the pageflip to the request list. */
    info.plane = dispdata->display_plane;
    info.crtc_id = dispdata->crtc->crtc->crtc_id;
    info.fb_id = fb->fb_id;

    info.src_w = windata->src_w;
    info.src_h = windata->src_h;
    info.crtc_w = windata->output_w;
    info.crtc_h = windata->output_h;
    info.crtc_x = windata->output_x;

    drm_atomic_set_plane_props(&info);

    /*****************************************************************/
    /* Tell the display (KMS) that it will have to wait on the fence */
    /* for the GPU-side FENCE.                                       */
    /*                                                               */
    /* Since KMS is a kernel thing, we have to pass an FD into       */
    /* the kernel, and get another FD out of the kernel.             */
    /*                                                               */
    /* 1) To pass the GPU-side fence into the kernel, we set the     */
    /* INPUT FD as the IN_FENCE_FD prop of the PRIMARY PLANE.        */
    /* This FD tells KMS (the kernel) to wait for the GPU-side fence.*/
    /*                                                               */
    /* 2) To get the KMS-side fence out of the kernel, we set the    */
    /* OUTPUT FD as the OUT_FEWNCE_FD prop of the CRTC.              */
    /* This FD will be later imported as a FENCE OBJECT which will be*/
    /* used to tell the GPU to wait for KMS to complete the changes  */
    /* requested in atomic_commit (the pageflip in this case).       */ 
    /*****************************************************************/
    if (dispdata->kms_in_fence_fd != -1)
    {
        add_plane_property(dispdata->atomic_req, dispdata->display_plane,
            "IN_FENCE_FD", dispdata->kms_in_fence_fd);
        add_crtc_property(dispdata->atomic_req, dispdata->crtc,
            "OUT_FENCE_PTR", VOID2U64(&dispdata->kms_out_fence_fd));
    }

    /* Do we have a pending modesetting? If so, set the necessary 
       props so it's included in the incoming atomic commit. */
    if (dispdata->modeset_pending) {
        uint32_t blob_id;
        SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;

        dispdata->atomic_flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
        add_connector_property(dispdata->atomic_req, dispdata->connector, "CRTC_ID", dispdata->crtc->crtc->crtc_id);
        KMSDRM_drmModeCreatePropertyBlob(viddata->drm_fd, &dispdata->mode, sizeof(dispdata->mode), &blob_id);
        add_crtc_property(dispdata->atomic_req, dispdata->crtc, "MODE_ID", blob_id);
        add_crtc_property(dispdata->atomic_req, dispdata->crtc, "ACTIVE", 1);
        dispdata->modeset_pending = SDL_FALSE;
    }

    /*****************************************************************/   
    /* Issue a non-blocking atomic commit: for triple buffering,     */
    /* this must not block so the game can start building another    */
    /* frame, even if the just-requested pageflip hasnt't completed. */
    /*****************************************************************/   
    if (drm_atomic_commit(_this, SDL_FALSE)) {
        return SDL_SetError("Failed to issue atomic commit on pageflip");
    }

    /* Release the previous front buffer so EGL can chose it as back buffer
       and render on it again. */
    if (windata->bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
    }

    /* Take note of the buffer about to become front buffer, so next
       time we come here we can free it like we just did with the previous
       front buffer. */
    windata->bo = windata->next_bo;

    /****************************************************************/
    /* Import the KMS-side FENCE OUTPUT FD from the kernel to the   */
    /* KMS-side FENCE OBJECT so we can use use it to fence the GPU. */
    /****************************************************************/
    dispdata->kms_fence = create_fence(dispdata->kms_out_fence_fd, _this);
    assert(dispdata->kms_fence);

    /****************************************************************/
    /* "Delete" the fence OUTPUT FD, because we already have the    */
    /* KMS FENCE OBJECT, the fence itself is away from us, on the   */
    /* kernel side.                                                 */
    /****************************************************************/
    dispdata->kms_out_fence_fd = -1;

    /*****************************************************************/
    /* Tell the GPU to wait on the fence for the KMS-side FENCE,     */
    /* which means waiting until the requested pageflip is completed.*/
    /*****************************************************************/
    _this->egl_data->eglWaitSyncKHR(_this->egl_data->egl_display, dispdata->kms_fence, 0);

    return 0;
}

int
KMSDRM_GLES_SwapWindowDoubleBuffered(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);
    SDL_DisplayData *dispdata = (SDL_DisplayData *) SDL_GetDisplayForWindow(window)->driverdata;
    KMSDRM_FBInfo *fb;
    KMSDRM_PlaneInfo info = {0};

    /****************************************************************************************************/
    /* In double-buffer mode, atomic_commit will always be synchronous/blocking (ie: won't return until */
    /* the requested changes are really done).                                                          */
    /* Also, there's no need to fence KMS or the GPU, because we won't be entering game loop again      */
    /* (hence not building or executing a new cmdstring) until pageflip is done, so we don't need to    */
    /* protect the KMS/GPU access to the buffer.                                                        */
    /****************************************************************************************************/ 

    /* Mark, at EGL level, the buffer that we want to become the new front buffer.
       However, it won't really happen until we request a pageflip at the KMS level and it completes. */
    if (! _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, windata->egl_surface)) {
        return SDL_EGL_SetError("Failed to swap EGL buffers", "eglSwapBuffers");
    }

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

    /* Add the pageflip to the request list. */
    info.plane = dispdata->display_plane;
    info.crtc_id = dispdata->crtc->crtc->crtc_id;
    info.fb_id = fb->fb_id;

    info.src_w = windata->src_w;
    info.src_h = windata->src_h;
    info.crtc_w = windata->output_w;
    info.crtc_h = windata->output_h;
    info.crtc_x = windata->output_x;

    drm_atomic_set_plane_props(&info);

    /* Do we have a pending modesetting? If so, set the necessary 
       props so it's included in the incoming atomic commit. */
    if (dispdata->modeset_pending) {
        SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;
        uint32_t blob_id;
        dispdata->atomic_flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
        add_connector_property(dispdata->atomic_req, dispdata->connector, "CRTC_ID", dispdata->crtc->crtc->crtc_id);
        KMSDRM_drmModeCreatePropertyBlob(viddata->drm_fd, &dispdata->mode, sizeof(dispdata->mode), &blob_id);
        add_crtc_property(dispdata->atomic_req, dispdata->crtc, "MODE_ID", blob_id);
        add_crtc_property(dispdata->atomic_req, dispdata->crtc, "ACTIVE", 1);
        dispdata->modeset_pending = SDL_FALSE;
    }

    /* Issue the one and only atomic commit where all changes will be requested!. 
       Blocking for double buffering: won't return until completed. */
    if (drm_atomic_commit(_this, SDL_TRUE)) {
        return SDL_SetError("Failed to issue atomic commit");
    }

    /* Release last front buffer so EGL can chose it as back buffer and render on it again. */
    if (windata->bo) {
        KMSDRM_gbm_surface_release_buffer(windata->gs, windata->bo);
    }

    /* Take note of current front buffer, so we can free it next time we come here. */
    windata->bo = windata->next_bo;

    return 0;
}

int
KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = ((SDL_WindowData *) window->driverdata);

    if (windata->swap_window == NULL) {
        /* We want the fenced version by default, but it needs extensions. */
        if ( (SDL_GetHintBoolean(SDL_HINT_VIDEO_DOUBLE_BUFFER, SDL_FALSE)) ||
             (!SDL_EGL_HasExtension(_this, SDL_EGL_DISPLAY_EXTENSION, "EGL_ANDROID_native_fence_sync")) )
        {
            windata->swap_window = KMSDRM_GLES_SwapWindowDoubleBuffered;
        } else {
            windata->swap_window = KMSDRM_GLES_SwapWindowFenced;
        }
    }

    return windata->swap_window(_this, window);
}

/***************************************/
/* End of Atomic functions block       */
/***************************************/

SDL_EGL_MakeCurrent_impl(KMSDRM)

#endif /* SDL_VIDEO_DRIVER_KMSDRM && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
