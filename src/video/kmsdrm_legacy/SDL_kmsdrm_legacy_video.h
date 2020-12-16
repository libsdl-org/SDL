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

#ifndef __SDL_KMSDRM_LEGACYVIDEO_H__
#define __SDL_KMSDRM_LEGACYVIDEO_H__

#include "../SDL_sysvideo.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#if SDL_VIDEO_OPENGL_EGL
#include <EGL/egl.h>
#endif

typedef struct SDL_VideoData
{
    int devindex;               /* device index that was passed on creation */
    int drm_fd;                 /* DRM file desc */
    struct gbm_device *gbm;

    SDL_Window **windows;
    int max_windows;
    int num_windows;
} SDL_VideoData;


typedef struct SDL_DisplayModeData
{
    int mode_index;
} SDL_DisplayModeData;


typedef struct SDL_DisplayData
{
    uint32_t crtc_id;
    drmModeConnector *conn;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;    /* CRTC to restore on quit */
} SDL_DisplayData;


typedef struct SDL_WindowData
{
    SDL_VideoData *viddata;
    struct gbm_surface *gs;
    struct gbm_bo *curr_bo;
    struct gbm_bo *next_bo;
    struct gbm_bo *crtc_bo;
    SDL_bool waiting_for_flip;
    SDL_bool double_buffer;
#if SDL_VIDEO_OPENGL_EGL
    int egl_surface_dirty;
    EGLSurface egl_surface;
#endif
} SDL_WindowData;

typedef struct KMSDRM_LEGACY_FBInfo
{
    int drm_fd;         /* DRM file desc */
    uint32_t fb_id;     /* DRM framebuffer ID */
} KMSDRM_LEGACY_FBInfo;

/* Helper functions */
int KMSDRM_LEGACY_CreateSurfaces(_THIS, SDL_Window * window);
KMSDRM_LEGACY_FBInfo *KMSDRM_LEGACY_FBFromBO(_THIS, struct gbm_bo *bo);
SDL_bool KMSDRM_LEGACY_WaitPageFlip(_THIS, SDL_WindowData *windata, int timeout);

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int KMSDRM_LEGACY_VideoInit(_THIS);
void KMSDRM_LEGACY_VideoQuit(_THIS);
void KMSDRM_LEGACY_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
int KMSDRM_LEGACY_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
int KMSDRM_LEGACY_CreateWindow(_THIS, SDL_Window * window);
int KMSDRM_LEGACY_CreateWindowFrom(_THIS, SDL_Window * window, const void *data);
void KMSDRM_LEGACY_SetWindowTitle(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
void KMSDRM_LEGACY_SetWindowPosition(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_SetWindowSize(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_ShowWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_HideWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_RaiseWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_MaximizeWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_MinimizeWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_RestoreWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
void KMSDRM_LEGACY_DestroyWindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool KMSDRM_LEGACY_GetWindowWMInfo(_THIS, SDL_Window * window,
                             struct SDL_SysWMinfo *info);

/* OpenGL/OpenGL ES functions */
int KMSDRM_LEGACY_GLES_LoadLibrary(_THIS, const char *path);
void *KMSDRM_LEGACY_GLES_GetProcAddress(_THIS, const char *proc);
void KMSDRM_LEGACY_GLES_UnloadLibrary(_THIS);
SDL_GLContext KMSDRM_LEGACY_GLES_CreateContext(_THIS, SDL_Window * window);
int KMSDRM_LEGACY_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
int KMSDRM_LEGACY_GLES_SetSwapInterval(_THIS, int interval);
int KMSDRM_LEGACY_GLES_GetSwapInterval(_THIS);
int KMSDRM_LEGACY_GLES_SwapWindow(_THIS, SDL_Window * window);
void KMSDRM_LEGACY_GLES_DeleteContext(_THIS, SDL_GLContext context);

#endif /* __SDL_KMSDRM_LEGACYVIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
