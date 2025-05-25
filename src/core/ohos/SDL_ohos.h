#ifndef SDL_OHOS_H
#define SDL_OHOS_H

#include "SDL3/SDL_mutex.h"

extern SDL_Mutex *g_ohosPageMutex;
extern SDL_Semaphore *g_ohosPauseSem;
extern SDL_Semaphore *g_ohosResumeSem;
void SDL_OHOS_PAGEMUTEX_Lock();
void SDL_OHOS_PAGEMUTEX_Unlock();
void SDL_OHOS_PAGEMUTEX_LockRunning();

#endif
