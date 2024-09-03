/*
 * This example code reads pen/stylus input and draws lines. Darker lines
 * for harder pressure.
 *
 * SDL can track multiple pens, but for simplicity here, this assumes any
 * pen input we see was from one device.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *render_target = NULL;
static float pressure = 0.0f;
static float previous_touch_x = -1.0f;
static float previous_touch_y = -1.0f;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/pen/drawing-lines", 640, 480, 0, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* we make a render target so we can draw lines to it and not have to record and redraw every pen stroke each frame.
       Instead rendering a frame for us is a single texture draw. */
    render_target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 640, 480);
    if (!render_target) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create render target!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* just blank the render target to gray to start. */
    SDL_SetRenderTarget(renderer, render_target);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    /* There are several events that track the specific stages of pen activity,
       but we're only going to look for motion and pressure, for simplicity. */
    if (event->type == SDL_EVENT_PEN_MOTION) {
        /* you can check for when the pen is touching, but if pressure > 0.0f, it's definitely touching! */
        if (pressure > 0.0f) {
            if (previous_touch_x >= 0.0f) {  /* only draw if we're moving while touching */
                /* draw with the alpha set to the pressure, so you effectively get a fainter line for lighter presses. */
                SDL_SetRenderTarget(renderer, render_target);
                SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, pressure);
                SDL_RenderLine(renderer, previous_touch_x, previous_touch_y, event->pmotion.x, event->pmotion.y);
            }
            previous_touch_x = event->pmotion.x;
            previous_touch_y = event->pmotion.y;
        } else {
            previous_touch_x = previous_touch_y = -1.0f;
        }
    } else if (event->type == SDL_EVENT_PEN_AXIS) {
        if (event->paxis.axis == SDL_PEN_AXIS_PRESSURE) {
            pressure = event->paxis.value;  /* remember new pressure for later draws. */
        }
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* make sure we're drawing to the window and not the render target */
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);  /* just in case. */
    SDL_RenderTexture(renderer, render_target, NULL, NULL);
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate)
{
    SDL_DestroyTexture(render_target);
    /* SDL will clean up the window/renderer for us. */
}

