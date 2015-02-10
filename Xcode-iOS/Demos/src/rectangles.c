/*
 *  rectangles.c
 *  written by Holmes Futrell
 *  use however you want
*/

#include "SDL.h"
#include <time.h>
#include "common.h"

void
render(SDL_Renderer *renderer)
{

    Uint8 r, g, b;
    /*  Come up with a random rectangle */
    SDL_Rect rect;
    rect.w = randomInt(64, 128);
    rect.h = randomInt(64, 128);
    rect.x = randomInt(0, SCREEN_WIDTH);
    rect.y = randomInt(0, SCREEN_HEIGHT);

    /* Come up with a random color */
    r = randomInt(50, 255);
    g = randomInt(50, 255);
    b = randomInt(50, 255);

    /*  Fill the rectangle in the color */
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderFillRect(renderer, &rect);

    /* update screen */
    SDL_RenderPresent(renderer);

}

int
main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO/* | SDL_INIT_AUDIO */) < 0)
    {
        printf("Unable to initialize SDL");
    }
    
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    int landscape = 1;
    int modes = SDL_GetNumDisplayModes(0);
    int sx = 0, sy = 0;
    for (int i = 0; i < modes; i++)
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, i, &mode);
        if (landscape ? mode.w > sx : mode.h > sy)
        {
            sx = mode.w;
            sy = mode.h;
        }
    }
    
    printf("picked: %d %d\n", sx, sy);
    
    SDL_Window *_sdl_window = NULL;
    SDL_GLContext _sdl_context = NULL;
    
    _sdl_window = SDL_CreateWindow("fred",
                                   0, 0,
                                   sx, sy,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
    
    SDL_SetHint("SDL_HINT_ORIENTATIONS", "LandscapeLeft LandscapeRight");
    
    int ax = 0, ay = 0;
    SDL_GetWindowSize(_sdl_window, &ax, &ay);
    
    printf("given: %d %d\n", ax, ay);

    return 0;
}
