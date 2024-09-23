/*
 * This example creates an SDL window and renderer, and then draws some
 * textures to it every frame, adjusting the viewport.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static int texture_width = 0;
static int texture_height = 0;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface = NULL;
    char *bmp_path = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/viewport", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .bmp into a surface, move it to a texture from there. */
    SDL_asprintf(&bmp_path, "%ssample.bmp", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface = SDL_LoadBMP(bmp_path);
    if (!surface) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't load bitmap!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    SDL_free(bmp_path);  /* done with this, the file is loaded. */

    texture_width = surface->w;
    texture_height = surface->h;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create static texture!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);  /* done with this, the texture has a copy of the pixels now. */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_FRect dst_rect = { 0, 0, (float) texture_width, (float) texture_height };
    SDL_Rect viewport;

    /* Setting a viewport has the effect of limiting the area that rendering
       can happen, and making coordinate (0, 0) live somewhere else in the
       window. It does _not_ scale rendering to fit the viewport. */

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* Draw once with the whole window as the viewport. */
    viewport.x = 0;
    viewport.y = 0;
    viewport.w = WINDOW_WIDTH / 2;
    viewport.h = WINDOW_HEIGHT / 2;
    SDL_SetRenderViewport(renderer, NULL);  /* NULL means "use the whole window" */
    SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

    /* top right quarter of the window. */
    viewport.x = WINDOW_WIDTH / 2;
    viewport.y = WINDOW_HEIGHT / 2;
    viewport.w = WINDOW_WIDTH / 2;
    viewport.h = WINDOW_HEIGHT / 2;
    SDL_SetRenderViewport(renderer, &viewport);
    SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

    /* bottom 20% of the window. Note it clips the width! */
    viewport.x = 0;
    viewport.y = WINDOW_HEIGHT - (WINDOW_HEIGHT / 5);
    viewport.w = WINDOW_WIDTH / 5;
    viewport.h = WINDOW_HEIGHT / 5;
    SDL_SetRenderViewport(renderer, &viewport);
    SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

    /* what happens if you try to draw above the viewport? It should clip! */
    viewport.x = 100;
    viewport.y = 200;
    viewport.w = WINDOW_WIDTH;
    viewport.h = WINDOW_HEIGHT;
    SDL_SetRenderViewport(renderer, &viewport);
    dst_rect.y = -50;
    SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

    SDL_RenderPresent(renderer);  /* put it all on the screen! */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate)
{
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

