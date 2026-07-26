#ifdef SDL_VIDEO_DRIVER_OHOS
#include "SDL_ohosvideo.h"

bool OHOS_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context);
SDL_GLContext OHOS_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
bool OHOS_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window);
bool OHOS_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path);

#endif
