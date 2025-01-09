/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifndef _SDL_mx6opengles_h
#define _SDL_mx6opengles_h

#if SDL_VIDEO_DRIVER_MX6 && SDL_VIDEO_OPENGL_EGL

#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

typedef struct MX6_EGL_VivanteData
{
    EGLNativeDisplayType(EGLAPIENTRY *fbGetDisplay) (void *context);
    EGLNativeDisplayType(EGLAPIENTRY *fbGetDisplayByIndex) (int DisplayIndex);
    void(EGLAPIENTRY *fbGetDisplayGeometry) (EGLNativeDisplayType Display, int *Width, int *Height);
    void(EGLAPIENTRY *fbGetDisplayInfo) (EGLNativeDisplayType Display, int *Width, int *Height, unsigned long *Physical, int *Stride, int *BitsPerPixel);
    void(EGLAPIENTRY *fbDestroyDisplay) (EGLNativeDisplayType Display);
    EGLNativeWindowType(EGLAPIENTRY *fbCreateWindow) (EGLNativeDisplayType Display, int X, int Y, int Width, int Height);
    void(EGLAPIENTRY *fbGetWindowGeometry) (EGLNativeWindowType Window, int *X, int *Y, int *Width, int *Height);
    void(EGLAPIENTRY *fbGetWindowInfo) (EGLNativeWindowType Window, int *X, int *Y, int *Width, int *Height, int *BitsPerPixel, unsigned int *Offset);
    void(EGLAPIENTRY *fbDestroyWindow) (EGLNativeWindowType Window);
    EGLNativePixmapType(EGLAPIENTRY *fbCreatePixmap) (EGLNativeDisplayType Display, int Width, int Height);
    EGLNativePixmapType(EGLAPIENTRY *fbCreatePixmapWithBpp) (EGLNativeDisplayType Display, int Width, int Height, int BitsPerPixel);
    void(EGLAPIENTRY *fbGetPixmapGeometry) (EGLNativePixmapType Pixmap, int *Width, int *Height);
    void(EGLAPIENTRY *fbGetPixmapInfo) (EGLNativePixmapType Pixmap, int *Width, int *Height, int *BitsPerPixel, int *Stride, void **Bits);
    void(EGLAPIENTRY *fbDestroyPixmap) (EGLNativePixmapType Pixmap);
} MX6_EGL_VivanteData;

struct MX6_EGL_VivanteData *egl_viv_data;

/* OpenGLES functions */
#define MX6_GLES_GetAttribute SDL_EGL_GetAttribute
#define MX6_GLES_GetProcAddress SDL_EGL_GetProcAddress
#define MX6_GLES_SetSwapInterval SDL_EGL_SetSwapInterval
#define MX6_GLES_GetSwapInterval SDL_EGL_GetSwapInterval
#define MX6_GLES_DeleteContext SDL_EGL_DeleteContext

extern int MX6_GLES_LoadLibrary(_THIS, const char *path);
extern void MX6_GLES_UnloadLibrary(_THIS);
extern SDL_GLContext MX6_GLES_CreateContext(_THIS, SDL_Window * window);
extern void MX6_GLES_SwapWindow(_THIS, SDL_Window * window);
extern int MX6_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);

#endif /* SDL_VIDEO_DRIVER_MX6 && SDL_VIDEO_OPENGL_EGL */

#endif /* _SDL_mx6opengles_h */

/* vi: set ts=4 sw=4 expandtab: */
