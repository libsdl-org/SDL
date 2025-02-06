// The purpose of this test is to validate that there are no undefined references
// when building with the N-Gage SDK since SDL can only be linked statically.

#define IMPORT_C
#include "SDL.h"

int SDL_main(int argc, char* argv[])
{
    for (int i = 0; i < argc; i++) { // C99 check.
        /* Nothing to do here. */
    }

    return 0;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
}
