#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_mutex.h"
#include "video/SDL_sysvideo.h"
#include <native_window/external_window.h>

void SDL_OHOS_SetDisplayOrientation(int orientation);
SDL_DisplayOrientation SDL_OHOS_GetDisplayOrientation();
extern OHNativeWindow *nativeWindow;

#endif
