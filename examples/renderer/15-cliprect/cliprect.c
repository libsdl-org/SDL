/*
 * This example creates an SDL window and renderer, and then draws a scene
 * to it every frame, while sliding around a clipping rectangle.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define CLIPRECT_SIZE 250
#define CLIPRECT_SPEED 200   /* pixels per second */

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_FPoint cliprect_position;
static SDL_FPoint cliprect_direction;
static Uint64 last_time = 0;

/* A lot of this program is examples/renderer/02-primitives, so we have a good
   visual that we can slide a clip rect around. The actual new magic in here
   is the SDL_SetRenderClipRect() function. */

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface = NULL;
    char *bmp_path = NULL;

    SDL_SetAppMetadata("Example Renderer Clipping Rectangle", "1.0", "com.example.renderer-cliprect");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/cliprect", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    cliprect_direction.x = cliprect_direction.y = 1.0f;

    last_time = SDL_GetTicks();

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .bmp into a surface, move it to a texture from there. */
    SDL_asprintf(&bmp_path, "%ssample.bmp", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface = SDL_LoadBMP(bmp_path);
    if (!surface) {
        SDL_Log("Couldn't load bitmap: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(bmp_path);  /* done with this, the file is loaded. */

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
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
    const SDL_Rect cliprect = { (int) SDL_roundf(cliprect_position.x), (int) SDL_roundf(cliprect_position.y), CLIPRECT_SIZE, CLIPRECT_SIZE };
    const Uint64 now = SDL_GetTicks();
    const float elapsed = ((float) (now - last_time)) / 1000.0f;  /* seconds since last iteration */
    const float distance = elapsed * CLIPRECT_SPEED;

    /* Set a new clipping rectangle position */
    cliprect_position.x += distance * cliprect_direction.x;
    if (cliprect_position.x < 0.0f) {
        cliprect_position.x = 0.0f;
        cliprect_direction.x = 1.0f;
    } else if (cliprect_position.x >= (WINDOW_WIDTH - CLIPRECT_SIZE)) {
        cliprect_position.x = (WINDOW_WIDTH - CLIPRECT_SIZE) - 1;
        cliprect_direction.x = -1.0f;
    }

    cliprect_position.y += distance * cliprect_direction.y;
    if (cliprect_position.y < 0.0f) {
        cliprect_position.y = 0.0f;
        cliprect_direction.y = 1.0f;
    } else if (cliprect_position.y >= (WINDOW_HEIGHT - CLIPRECT_SIZE)) {
        cliprect_position.y = (WINDOW_HEIGHT - CLIPRECT_SIZE) - 1;
        cliprect_direction.y = -1.0f;
    }
    SDL_SetRenderClipRect(renderer, &cliprect);

    last_time = now;

    /* okay, now draw! */

    /* Note that SDL_RenderClear is _not_ affected by the clipping rectangle! */
    SDL_SetRenderDrawColor(renderer, 33, 33, 33, SDL_ALPHA_OPAQUE);  /* grey, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* stretch the texture across the entire window. Only the piece in the
       clipping rectangle will actually render, though! */
    SDL_RenderTexture(renderer, texture, NULL, NULL);

    SDL_RenderPresent(renderer);  /* put it all on the screen! */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

