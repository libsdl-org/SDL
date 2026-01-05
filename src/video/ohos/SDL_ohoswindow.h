#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../SDL_sysvideo.h"
bool OHOS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
void OHOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);

#endif
