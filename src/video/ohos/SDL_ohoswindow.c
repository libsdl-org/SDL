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
    OHOS_removeWindow(window);
}

#endif
