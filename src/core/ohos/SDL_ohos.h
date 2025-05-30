#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_mutex.h"
#include "video/SDL_sysvideo.h"
#include <native_window/external_window.h>

extern SDL_Mutex *g_ohosPageMutex;
void SDL_OHOS_SetDisplayOrientation(int orientation);
extern OHNativeWindow *nativeWindow;

#endif
