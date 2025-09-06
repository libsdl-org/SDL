#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_video.h"
#include "video/SDL_sysvideo.h"
#include <native_window/external_window.h>

void OHOS_windowDataFill(SDL_Window* w);
void OHOS_removeWindow(SDL_Window* w);
void OHOS_LockPage();
void OHOS_UnlockPage();
int OHOS_FetchWidth();
int OHOS_FetchHeight();

void OHOS_MessageBox(const char* title, const char* message);
const char* OHOS_Locale();
void OHOS_SetClipboardText(const char* data);

bool OHOS_IsScreenKeyboardShown(); 
void OHOS_StartTextInput();
void OHOS_StopTextInput();

typedef struct SDL_VideoData {
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;

#endif
