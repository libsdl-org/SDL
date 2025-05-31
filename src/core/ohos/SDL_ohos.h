#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_mutex.h"
#include "SDL3/SDL_video.h"
#include "video/SDL_sysvideo.h"
#include <native_window/external_window.h>

extern SDL_Mutex *g_ohosPageMutex;

void OHOS_windowDataFill(SDL_Window* w);
void OHOS_removeWindow(SDL_Window* w);

typedef struct SDL_VideoData {
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;

#endif
