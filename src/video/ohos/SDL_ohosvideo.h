#ifndef SDL_OHOSVIDEO_H
#define SDL_OHOSVIDEO_H

#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos.h"
struct SDL_WindowData {
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
    EGLContext egl_context;
#endif
    bool backup_done;
    OHNativeWindow *native_window;
    uint64_t width;
    uint64_t height;
};

#endif
