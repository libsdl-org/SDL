#include "SDL.h"
#include "SDL_main.h"

void inc_sdl_noslash(void) {
    SDL_SetMainReady();
    SDL_Init(0);
    SDL_Quit();
}
