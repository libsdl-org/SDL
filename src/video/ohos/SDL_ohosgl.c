#include "SDL_internal.h"
#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosvideo.h"

bool OHOS_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context)
{
    if (window && context) {
        return SDL_EGL_MakeCurrent(_this, window->internal->egl_surface, context);
    } else {
        return SDL_EGL_MakeCurrent(_this, NULL, NULL);
    }
}

SDL_GLContext OHOS_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_GLContext result;

    OHOS_LockPage();
    
    OHOS_windowDataFill(window);
    result = SDL_EGL_CreateContext(_this, window->internal->egl_surface);

    OHOS_UnlockPage();

    return result;
}

bool OHOS_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    bool result;

    OHOS_LockPage();

    result = SDL_EGL_SwapBuffers(_this, window->internal->egl_surface);

    OHOS_UnlockPage();

    return result;
}

bool OHOS_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    return SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)0, 0);
}

#endif
