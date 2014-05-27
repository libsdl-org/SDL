#include <stdio.h>
#include "SDL.h"

/* !!! FIXME: rewrite this to be wired in to test framework. */

int main(int argc, char **argv)
{
    int done = 0;
    SDL_Window *window;
    SDL_Renderer *renderer;

    const SDL_Rect drag_areas[] = {
        { 20, 20, 100, 100 },
        { 200, 70, 100, 100 },
        { 400, 90, 100, 100 }
    };

    const SDL_Rect *areas = drag_areas;
    int numareas = SDL_arraysize(drag_areas);

    /* !!! FIXME: check for errors. */
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Drag the red boxes", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_BORDERLESS);
    renderer = SDL_CreateRenderer(window, -1, 0);

    if (SDL_SetWindowDragAreas(window, areas, numareas) == -1) {
        fprintf(stderr, "Setting drag areas failed!\n");
        SDL_Quit();
        return 1;
    }

    while (!done)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 127, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRects(renderer, areas, SDL_arraysize(drag_areas));
        SDL_RenderPresent(renderer);

        SDL_Event e;
        int nothing_to_do = 1;
        while (SDL_PollEvent(&e)) {
            nothing_to_do = 0;

            switch (e.type)
            {
                case SDL_MOUSEBUTTONDOWN:
                    printf("button down!\n");
                    break;

                case SDL_MOUSEBUTTONUP:
                    printf("button up!\n");
                    break;

                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_MOVED) {
                        printf("Window event moved to (%d, %d)!\n", (int) e.window.data1, (int) e.window.data2);
                    }
                    break;

                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        done = 1;
                    } else if (e.key.keysym.sym == SDLK_x) {
                        if (!areas) {
                            areas = drag_areas;
                            numareas = SDL_arraysize(drag_areas);
                        } else {
                            areas = NULL;
                            numareas = 0;
                        }
                        if (SDL_SetWindowDragAreas(window, areas, numareas) == -1) {
                            fprintf(stderr, "Setting drag areas failed!\n");
                            SDL_Quit();
                            return 1;
                        }
                    }
                    break;

                case SDL_QUIT:
                    done = 1;
                    break;
            }
        }

        if (nothing_to_do) {
            SDL_Delay(50);
        }
    }

    SDL_Quit();
    return 0;
}
