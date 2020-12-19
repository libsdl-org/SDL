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

#ifndef __SDL_KMSDRMVIDEO_H__
#define __SDL_KMSDRMVIDEO_H__

#include "../SDL_sysvideo.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>
#include <assert.h>
#if SDL_VIDEO_OPENGL_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

/****************************************************************************************/
/* Driverdata pointers are void struct* used to store backend-specific variables        */
/* and info that supports the SDL-side structs like SDL Display Devices, SDL_Windows... */
/* which need to be "supported" with backend-side info and mechanisms to work.          */ 
/****************************************************************************************/

typedef struct SDL_VideoData
{
    int devindex;               /* device index that was passed on creation */
    int drm_fd;                 /* DRM file desc */
    char devpath[32];           /* DRM dev path. */

    struct gbm_device *gbm_dev;

    SDL_Window **windows;
    unsigned int max_windows;
    unsigned int num_windows;

    SDL_bool video_init;        /* Has VideoInit succeeded? */

} SDL_VideoData;

typedef struct plane {
    drmModePlane *plane;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
} plane;

typedef struct crtc {
    drmModeCrtc *crtc;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
} crtc;

typedef struct connector {
    drmModeConnector *connector;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
} connector;

/* More general driverdata info that gives support and substance to the SDL_Display. */
typedef struct SDL_DisplayData
{
    drmModeModeInfo mode;
    drmModeModeInfo preferred_mode;
    uint32_t atomic_flags;

    plane *display_plane;
    plane *cursor_plane;
    crtc *crtc;
    connector *connector;

    /* Central atomic request list, used for the prop
       changeset related to pageflip in SwapWindow. */ 
    drmModeAtomicReq *atomic_req;

    int kms_in_fence_fd;
    int kms_out_fence_fd;

    EGLSyncKHR kms_fence;
    EGLSyncKHR gpu_fence;

#if SDL_VIDEO_OPENGL_EGL
    EGLSurface old_egl_surface;
#endif

    SDL_bool modeset_pending;
    SDL_bool gbm_init;

    /* DRM & GBM cursor stuff lives here, not in an SDL_Cursor's driverdata struct,
       because setting/unsetting up these is done on window creation/destruction,
       where we may not have an SDL_Cursor at all (so no SDL_Cursor driverdata).
       There's only one cursor GBM BO because we only support one cursor. */
    struct gbm_bo *cursor_bo;
    uint64_t cursor_w, cursor_h;

} SDL_DisplayData;

/* Driverdata info that gives KMSDRM-side support and substance to the SDL_Window. */
typedef struct SDL_WindowData
{
    SDL_VideoData *viddata;
    /* SDL internals expect EGL surface to be here, and in KMSDRM the GBM surface is
       what supports the EGL surface on the driver side, so all these surfaces and buffers
       are expected to be here, in the struct pointed by SDL_Window driverdata pointer:
       this one. So don't try to move these to dispdata!  */
    struct gbm_surface *gs;
    struct gbm_bo *bo;
    struct gbm_bo *next_bo;

#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif

    /* For scaling and AR correction. */
    int32_t src_w;
    int32_t src_h;
    int32_t output_w;
    int32_t output_h;
    int32_t output_x;

    /* This dictates what approach we'll use for SwapBuffers. */
    int (*swap_window)(_THIS, SDL_Window * window);

} SDL_WindowData;

typedef struct SDL_DisplayModeData
{
    int mode_index;
} SDL_DisplayModeData;

typedef struct KMSDRM_FBInfo
{
    int drm_fd;         /* DRM file desc */
    uint32_t fb_id;     /* DRM framebuffer ID */
} KMSDRM_FBInfo;

typedef struct KMSDRM_PlaneInfo
{
    struct plane *plane;
    uint32_t fb_id;
    uint32_t crtc_id;
    int32_t src_x;
    int32_t src_y;
    int32_t src_w;
    int32_t src_h;
    int32_t crtc_x;
    int32_t crtc_y;
    int32_t crtc_w;
    int32_t crtc_h;
} KMSDRM_PlaneInfo;

/* Helper functions */
int KMSDRM_CreateEGLSurface(_THIS, SDL_Window * window);
KMSDRM_FBInfo *KMSDRM_FBFromBO(_THIS, struct gbm_bo *bo);

/* Atomic functions that are used from SDL_kmsdrmopengles.c and SDL_kmsdrmmouse.c */
void drm_atomic_set_plane_props(struct KMSDRM_PlaneInfo *info); 

void drm_atomic_waitpending(_THIS);
int drm_atomic_commit(_THIS, SDL_bool blocking);
int add_plane_property(drmModeAtomicReq *req, struct plane *plane,
                             const char *name, uint64_t value);
int add_crtc_property(drmModeAtomicReq *req, struct crtc *crtc,
                             const char *name, uint64_t value);
int add_connector_property(drmModeAtomicReq *req, struct connector *connector,
                             const char *name, uint64_t value);
int setup_plane(_THIS, struct plane **plane, uint32_t plane_type);
void free_plane(struct plane **plane);

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int KMSDRM_VideoInit(_THIS);
void KMSDRM_VideoQuit(_THIS);
void KMSDRM_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
int KMSDRM_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
int KMSDRM_CreateWindow(_THIS, SDL_Window * window);
int KMSDRM_CreateWindowFrom(_THIS, SDL_Window * window, const void *data);
void KMSDRM_SetWindowTitle(_THIS, SDL_Window * window);
void KMSDRM_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
void KMSDRM_SetWindowPosition(_THIS, SDL_Window * window);
void KMSDRM_SetWindowSize(_THIS, SDL_Window * window);
void KMSDRM_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * _display, SDL_bool fullscreen);
void KMSDRM_ShowWindow(_THIS, SDL_Window * window);
void KMSDRM_HideWindow(_THIS, SDL_Window * window);
void KMSDRM_RaiseWindow(_THIS, SDL_Window * window);
void KMSDRM_MaximizeWindow(_THIS, SDL_Window * window);
void KMSDRM_MinimizeWindow(_THIS, SDL_Window * window);
void KMSDRM_RestoreWindow(_THIS, SDL_Window * window);
void KMSDRM_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
void KMSDRM_DestroyWindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool KMSDRM_GetWindowWMInfo(_THIS, SDL_Window * window,
                             struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int KMSDRM_GLES_LoadLibrary(_THIS, const char *path);
void *KMSDRM_GLES_GetProcAddress(_THIS, const char *proc);
void KMSDRM_GLES_UnloadLibrary(_THIS);
SDL_GLContext KMSDRM_GLES_CreateContext(_THIS, SDL_Window * window);
int KMSDRM_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
int KMSDRM_GLES_SetSwapInterval(_THIS, int interval);
int KMSDRM_GLES_GetSwapInterval(_THIS);
int KMSDRM_GLES_SwapWindow(_THIS, SDL_Window * window);
void KMSDRM_GLES_DeleteContext(_THIS, SDL_GLContext context);

#endif /* __SDL_KMSDRMVIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
