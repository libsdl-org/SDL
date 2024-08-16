/*
 * This example code implements a Snake game that showcases some of the
 * functionalities of SDL, such as timer callbacks and event handling.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h> /* malloc(), free() */

#include "snake.h"

#define STEP_RATE_IN_MILLISECONDS  125
#define SNAKE_BLOCK_SIZE_IN_PIXELS 24
#define SDL_WINDOW_WIDTH           (SNAKE_BLOCK_SIZE_IN_PIXELS * SNAKE_GAME_WIDTH)
#define SDL_WINDOW_HEIGHT          (SNAKE_BLOCK_SIZE_IN_PIXELS * SNAKE_GAME_HEIGHT)

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_TimerID step_timer;
    SnakeContext snake_ctx;
} AppState;

static Uint32 sdl_timer_callback_(void *payload, SDL_TimerID timer_id, Uint32 interval)
{
    SDL_Event e;
    SDL_UserEvent ue;
    /* NOTE: snake_step is not called here directly for multithreaded concerns. */
    (void)payload;
    ue.type = SDL_EVENT_USER;
    ue.code = 0;
    ue.data1 = NULL;
    ue.data2 = NULL;
    e.type = SDL_EVENT_USER;
    e.user = ue;
    SDL_PushEvent(&e);
    return interval;
}

static int handle_key_event_(SnakeContext *ctx, SDL_Scancode key_code)
{
    switch (key_code) {
    /* Quit. */
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_Q:
        return SDL_APP_SUCCESS;
    /* Restart the game as if the program was launched. */
    case SDL_SCANCODE_R:
        snake_initialize(ctx, SDL_rand);
        break;
    /* Decide new direction of the snake. */
    case SDL_SCANCODE_RIGHT:
        snake_redir(ctx, SNAKE_DIR_RIGHT);
        break;
    case SDL_SCANCODE_UP:
        snake_redir(ctx, SNAKE_DIR_UP);
        break;
    case SDL_SCANCODE_LEFT:
        snake_redir(ctx, SNAKE_DIR_LEFT);
        break;
    case SDL_SCANCODE_DOWN:
        snake_redir(ctx, SNAKE_DIR_DOWN);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

static void set_rect_xy_(SDL_FRect *r, short x, short y)
{
    r->x = (float)(x * SNAKE_BLOCK_SIZE_IN_PIXELS);
    r->y = (float)(y * SNAKE_BLOCK_SIZE_IN_PIXELS);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as;
    SnakeContext *ctx;
    SDL_FRect r;
    unsigned i;
    unsigned j;
    int ct;
    as = (AppState *)appstate;
    ctx = &as->snake_ctx;
    r.w = r.h = SNAKE_BLOCK_SIZE_IN_PIXELS;
    SDL_SetRenderDrawColor(as->renderer, 0, 0, 0, 255);
    SDL_RenderClear(as->renderer);
    for (i = 0; i < SNAKE_GAME_WIDTH; i++) {
        for (j = 0; j < SNAKE_GAME_HEIGHT; j++) {
            ct = snake_cell_at(ctx, i, j);
            if (ct == SNAKE_CELL_NOTHING)
                continue;
            set_rect_xy_(&r, i, j);
            if (ct == SNAKE_CELL_FOOD)
                SDL_SetRenderDrawColor(as->renderer, 0, 0, 128, 255);
            else /* body */
                SDL_SetRenderDrawColor(as->renderer, 0, 128, 0, 255);
            SDL_RenderFillRect(as->renderer, &r);
        }
    }
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 0, 255); /*head*/
    set_rect_xy_(&r, ctx->head_xpos, ctx->head_ypos);
    SDL_RenderFillRect(as->renderer, &r);
    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        return SDL_APP_FAILURE;
    }
    SDL_SetHint("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR", "0");
    AppState *as = malloc(sizeof(AppState));
    *appstate = as;
    as->step_timer = 0;
    if (SDL_CreateWindowAndRenderer("examples/game/snake", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, 0, &as->window, &as->renderer) == -1) {
        return SDL_APP_FAILURE;
    }
    snake_initialize(&as->snake_ctx, SDL_rand);
    as->step_timer = SDL_AddTimer(
        STEP_RATE_IN_MILLISECONDS,
        sdl_timer_callback_,
        NULL);
    if (as->step_timer == 0) {
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    SnakeContext *ctx = &((AppState *)appstate)->snake_ctx;
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_USER:
        snake_step(ctx, SDL_rand);
        break;
    case SDL_EVENT_KEY_DOWN:
        return handle_key_event_(ctx, event->key.scancode);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate)
{
    if (appstate != NULL) {
        AppState *as = (AppState *)appstate;
        SDL_RemoveTimer(as->step_timer);
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        free(as);
    }
}
