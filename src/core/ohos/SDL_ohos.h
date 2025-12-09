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

int OHOS_MessageBox(const char* title, const char* message, int ml, int* mapping, int bl, const char * const *buttons);
const char* OHOS_Locale();
void OHOS_OpenLink(const char* url);
bool OHOS_IsBatteryPresent();
bool OHOS_IsBatteryCharging();
bool OHOS_IsBatteryCharged();
int OHOS_GetBatteryPercent();
void OHOS_SetClipboardText(const char* data);
char* OHOS_GetStoragePath();

void OHOS_FileDialog(int id, const char* defpath, int allowmany, const char* filter);

bool OHOS_IsScreenKeyboardShown(); 
void OHOS_StartTextInput();
void OHOS_StopTextInput();

typedef struct SDL_VideoData {
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;

#endif
