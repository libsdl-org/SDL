#include "SDL_ohoswindow.h"
#include "SDL_internal.h"
#include <EGL/eglplatform.h>

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"
bool OHOS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    OHOS_windowDataFill(window);

    return true;
}

void OHOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    #ifdef SDL_VIDEO_OPENGL_EGL
        if (window->flags & SDL_WINDOW_OPENGL) {
            SDL_LockMutex(g_ohosPageMutex);
            if (window->internal->egl_context)
            {
                SDL_EGL_DestroyContext(_this, window->internal->egl_context);
            }
            if (window->internal->egl_surface != EGL_NO_SURFACE)
            {
                SDL_EGL_DestroySurface(_this, window->internal->egl_surface);
            }
            SDL_UnlockMutex(g_ohosPageMutex);
        }
        SDL_free(window->internal);
    #endif
}

#endif
