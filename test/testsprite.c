#include <SDL3/SDL.h>

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("", 500, 500, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    SDL_Event event;
    bool running = true;

    while (running) {
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
        if (event.type == SDL_EVENT_MOUSE_MOTION) SDL_Log("%f\n", event.motion.x);
      }
      SDL_RenderPresent(renderer);
    }
}
