#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_mutex.h"
#include "video/SDL_sysvideo.h"
#include <native_window/external_window.h>

extern SDL_Mutex *g_ohosPageMutex;
extern OHNativeWindow *g_ohosNativeWindow;
extern SDL_WindowData windowData;

typedef struct SDL_VideoData {
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;

#endif
