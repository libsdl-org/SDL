#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[])
{
    SDL_SetMainReady();
    if (!SDL_Init(0)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return 1;
    }
    SDL_Delay(100);
    SDL_Quit();
    return 0;
}
