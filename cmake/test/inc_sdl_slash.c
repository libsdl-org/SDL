#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

void inc_sdl_slash(void) {
    SDL_SetMainReady();
    SDL_Init(0);
    SDL_Quit();
}
