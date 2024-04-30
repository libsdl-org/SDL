#ifndef SDL_OHOSHEAD_H
#define SDL_OHOSHEAD_H

#include "SDL_mutex.h"
#include "../../core/ohos/SDL_ohos.h"
#include "../src/video/SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"

typedef struct {
    SDL_mutex *mLock;
    SDL_cond *mCond;
} OhosThreadLock;

typedef struct {
    EGLSurface egl_xcomponent;
    EGLContext egl_context; /* We use this to preserve the context when losing focus */
    SDL_bool   backup_done;
    OHNativeWindow *native_window;
    uint64_t width;
    uint64_t height;
    double x;
    double y;
} SDL_WindowData;

#endif