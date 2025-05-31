#include "SDL_ohoswindow.h"
#include <EGL/eglplatform.h>

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"
bool OHOS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    window->internal = &windowData;
#ifdef SDL_VIDEO_OPENGL_EGL
    if (window->flags & SDL_WINDOW_OPENGL) {
        SDL_LockMutex(g_ohosPageMutex);
        if (window->internal->egl_xcomponent == EGL_NO_SURFACE)
        {
            window->internal->egl_xcomponent = SDL_EGL_CreateSurface(_this, window, (NativeWindowType)window->internal->native_window);
        }
        SDL_UnlockMutex(g_ohosPageMutex);
    }
#endif
    window->x = (int)window->internal->x;
    window->y = (int)window->internal->y;

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
        if (window->internal->egl_xcomponent != EGL_NO_SURFACE)
        {
            SDL_EGL_DestroySurface(_this, window->internal->egl_xcomponent);
        }
        SDL_UnlockMutex(g_ohosPageMutex);
    }
#endif
}

#endif
