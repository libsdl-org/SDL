/*
 * Logic implementation of the Snake game. It is designed to efficiently
 * represent in memory the state of the game.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STEP_RATE_IN_MILLISECONDS  125
#define SNAKE_BLOCK_SIZE_IN_PIXELS 24
#define SDL_WINDOW_WIDTH           (SNAKE_BLOCK_SIZE_IN_PIXELS * SNAKE_GAME_WIDTH)
#define SDL_WINDOW_HEIGHT          (SNAKE_BLOCK_SIZE_IN_PIXELS * SNAKE_GAME_HEIGHT)

#define SNAKE_GAME_WIDTH  24U
#define SNAKE_GAME_HEIGHT 18U
#define SNAKE_MATRIX_SIZE (SNAKE_GAME_WIDTH * SNAKE_GAME_HEIGHT)

#define THREE_BITS  0x7U /* ~CHAR_MAX >> (CHAR_BIT - SNAKE_CELL_MAX_BITS) */
#define SHIFT(x, y) (((x) + ((y) * SNAKE_GAME_WIDTH)) * SNAKE_CELL_MAX_BITS)

typedef enum
{
    SNAKE_CELL_NOTHING = 0U,
    SNAKE_CELL_SRIGHT = 1U,
    SNAKE_CELL_SUP = 2U,
    SNAKE_CELL_SLEFT = 3U,
    SNAKE_CELL_SDOWN = 4U,
    SNAKE_CELL_FOOD = 5U
} SnakeCell;

#define SNAKE_CELL_MAX_BITS 3U /* floor(log2(SNAKE_CELL_FOOD)) + 1 */

typedef enum
{
    SNAKE_DIR_RIGHT,
    SNAKE_DIR_UP,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_DOWN
} SnakeDirection;

typedef struct
{
    unsigned char cells[(SNAKE_MATRIX_SIZE * SNAKE_CELL_MAX_BITS) / 8U];
    char head_xpos;
    char head_ypos;
    char tail_xpos;
    char tail_ypos;
    char next_dir;
    char inhibit_tail_step;
    unsigned occupied_cells;
} SnakeContext;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_TimerID step_timer;
    SnakeContext snake_ctx;
} AppState;

SnakeCell snake_cell_at(const SnakeContext *ctx, char x, char y)
{
    const int shift = SHIFT(x, y);
    unsigned short range;
    SDL_memcpy(&range, ctx->cells + (shift / 8), sizeof(range));
    return (SnakeCell)((range >> (shift % 8)) & THREE_BITS);
}

static void set_rect_xy_(SDL_FRect *r, short x, short y)
{
    r->x = (float)(x * SNAKE_BLOCK_SIZE_IN_PIXELS);
    r->y = (float)(y * SNAKE_BLOCK_SIZE_IN_PIXELS);
}

static void put_cell_at_(SnakeContext *ctx, char x, char y, SnakeCell ct)
{
    const int shift = SHIFT(x, y);
    const int adjust = shift % 8;
    unsigned char *const pos = ctx->cells + (shift / 8);
    unsigned short range;
    SDL_memcpy(&range, pos, sizeof(range));
    range &= ~(THREE_BITS << adjust); /* clear bits */
    range |= (ct & THREE_BITS) << adjust;
    SDL_memcpy(pos, &range, sizeof(range));
}

static int are_cells_full_(SnakeContext *ctx)
{
    return ctx->occupied_cells == SNAKE_GAME_WIDTH * SNAKE_GAME_HEIGHT;
}

static void new_food_pos_(SnakeContext *ctx)
{
    while (true) {
        const char x = (char) SDL_rand(SNAKE_GAME_WIDTH);
        const char y = (char) SDL_rand(SNAKE_GAME_HEIGHT);
        if (snake_cell_at(ctx, x, y) == SNAKE_CELL_NOTHING) {
            put_cell_at_(ctx, x, y, SNAKE_CELL_FOOD);
            break;
        }
    }
}

void snake_initialize(SnakeContext *ctx)
{
    int i;
    SDL_zeroa(ctx->cells);
    ctx->head_xpos = ctx->tail_xpos = SNAKE_GAME_WIDTH / 2;
    ctx->head_ypos = ctx->tail_ypos = SNAKE_GAME_HEIGHT / 2;
    ctx->next_dir = SNAKE_DIR_RIGHT;
    ctx->inhibit_tail_step = ctx->occupied_cells = 4;
    --ctx->occupied_cells;
    put_cell_at_(ctx, ctx->tail_xpos, ctx->tail_ypos, SNAKE_CELL_SRIGHT);
    for (i = 0; i < 4; i++) {
        new_food_pos_(ctx);
        ++ctx->occupied_cells;
    }
}

void snake_redir(SnakeContext *ctx, SnakeDirection dir)
{
    SnakeCell ct = snake_cell_at(ctx, ctx->head_xpos, ctx->head_ypos);
    if ((dir == SNAKE_DIR_RIGHT && ct != SNAKE_CELL_SLEFT) ||
        (dir == SNAKE_DIR_UP && ct != SNAKE_CELL_SDOWN) ||
        (dir == SNAKE_DIR_LEFT && ct != SNAKE_CELL_SRIGHT) ||
        (dir == SNAKE_DIR_DOWN && ct != SNAKE_CELL_SUP)) {
        ctx->next_dir = dir;
    }
}

static void wrap_around_(char *val, char max)
{
    if (*val < 0) {
        *val = max - 1;
    } else if (*val > max - 1) {
        *val = 0;
    }
}

void snake_step(SnakeContext *ctx)
{
    const SnakeCell dir_as_cell = (SnakeCell)(ctx->next_dir + 1);
    SnakeCell ct;
    char prev_xpos;
    char prev_ypos;
    /* Move tail forward */
    if (--ctx->inhibit_tail_step == 0) {
        ++ctx->inhibit_tail_step;
        ct = snake_cell_at(ctx, ctx->tail_xpos, ctx->tail_ypos);
        put_cell_at_(ctx, ctx->tail_xpos, ctx->tail_ypos, SNAKE_CELL_NOTHING);
        switch (ct) {
        case SNAKE_CELL_SRIGHT:
            ctx->tail_xpos++;
            break;
        case SNAKE_CELL_SUP:
            ctx->tail_ypos--;
            break;
        case SNAKE_CELL_SLEFT:
            ctx->tail_xpos--;
            break;
        case SNAKE_CELL_SDOWN:
            ctx->tail_ypos++;
            break;
        default:
            break;
        }
        wrap_around_(&ctx->tail_xpos, SNAKE_GAME_WIDTH);
        wrap_around_(&ctx->tail_ypos, SNAKE_GAME_HEIGHT);
    }
    /* Move head forward */
    prev_xpos = ctx->head_xpos;
    prev_ypos = ctx->head_ypos;
    switch (ctx->next_dir) {
    case SNAKE_DIR_RIGHT:
        ++ctx->head_xpos;
        break;
    case SNAKE_DIR_UP:
        --ctx->head_ypos;
        break;
    case SNAKE_DIR_LEFT:
        --ctx->head_xpos;
        break;
    case SNAKE_DIR_DOWN:
        ++ctx->head_ypos;
        break;
    }
    wrap_around_(&ctx->head_xpos, SNAKE_GAME_WIDTH);
    wrap_around_(&ctx->head_ypos, SNAKE_GAME_HEIGHT);
    /* Collisions */
    ct = snake_cell_at(ctx, ctx->head_xpos, ctx->head_ypos);
    if (ct != SNAKE_CELL_NOTHING && ct != SNAKE_CELL_FOOD) {
        snake_initialize(ctx);
        return;
    }
    put_cell_at_(ctx, prev_xpos, prev_ypos, dir_as_cell);
    put_cell_at_(ctx, ctx->head_xpos, ctx->head_ypos, dir_as_cell);
    if (ct == SNAKE_CELL_FOOD) {
        if (are_cells_full_(ctx)) {
            snake_initialize(ctx);
            return;
        }
        new_food_pos_(ctx);
        ++ctx->inhibit_tail_step;
        ++ctx->occupied_cells;
    }
}

static Uint32 sdl_timer_callback_(void *payload, SDL_TimerID timer_id, Uint32 interval)
{
    /* NOTE: snake_step is not called here directly for multithreaded concerns. */
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
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
        snake_initialize(ctx);
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
                SDL_SetRenderDrawColor(as->renderer, 80, 80, 255, 255);
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
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    AppState *as = SDL_calloc(1, sizeof(AppState));

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("examples/game/snake", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, 0, &as->window, &as->renderer)) {
        return SDL_APP_FAILURE;
    }

    snake_initialize(&as->snake_ctx);

    as->step_timer = SDL_AddTimer(STEP_RATE_IN_MILLISECONDS, sdl_timer_callback_, NULL);
    if (as->step_timer == 0) {
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    SnakeContext *ctx = &((AppState *)appstate)->snake_ctx;
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_USER:
        snake_step(ctx);
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
        SDL_free(as);
    }
}
