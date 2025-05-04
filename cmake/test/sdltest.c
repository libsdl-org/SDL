#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    return SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    return SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDLTest_CommonState state;
    SDLTest_CommonDefaultArgs(&state, argc, argv);
    return SDL_APP_SUCCESS;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
}
