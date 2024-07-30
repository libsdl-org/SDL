/*
 * Logic implementation of the Snake game. It is designed to efficiently
 * represent in memory the state of the game.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */
#include "snake.h"

#include <limits.h> /* CHAR_BIT, CHAR_MAX */
#include <string.h> /* memcpy() */

#define THREE_BITS  0x7U /* ~CHAR_MAX >> (CHAR_BIT - SNAKE_CELL_MAX_BITS) */
#define SHIFT(x, y) (((x) + ((y) * SNAKE_GAME_WIDTH)) * SNAKE_CELL_MAX_BITS)

static void put_cell_at_(SnakeContext *ctx, char x, char y, SnakeCell ct)
{
    const int shift = SHIFT(x, y);
    const int adjust = shift % CHAR_BIT;
    unsigned char *const pos = ctx->cells + (shift / CHAR_BIT);
    unsigned short range;
    memcpy(&range, pos, sizeof(range));
    range &= ~(THREE_BITS << adjust); /* clear bits */
    range |= (ct & THREE_BITS) << adjust;
    memcpy(pos, &range, sizeof(range));
}

static int are_cells_full_(SnakeContext *ctx)
{
    return ctx->occupied_cells == SNAKE_GAME_WIDTH * SNAKE_GAME_HEIGHT;
}

static void new_food_pos_(SnakeContext *ctx, RandFunc rand)
{
    char x;
    char y;
    for (;;) {
        x = rand(SNAKE_GAME_WIDTH);
        y = rand(SNAKE_GAME_HEIGHT);
        if (snake_cell_at(ctx, x, y) == SNAKE_CELL_NOTHING) {
            put_cell_at_(ctx, x, y, SNAKE_CELL_FOOD);
            break;
        }
    }
}

void snake_initialize(SnakeContext *ctx, RandFunc rand)
{
    int i;
    memset(ctx, 0, sizeof ctx->cells);
    ctx->head_xpos = ctx->tail_xpos = SNAKE_GAME_WIDTH / 2;
    ctx->head_ypos = ctx->tail_ypos = SNAKE_GAME_HEIGHT / 2;
    ctx->next_dir = SNAKE_DIR_RIGHT;
    ctx->inhibit_tail_step = ctx->occupied_cells = 4;
    --ctx->occupied_cells;
    put_cell_at_(ctx, ctx->tail_xpos, ctx->tail_ypos, SNAKE_CELL_SRIGHT);
    for (i = 0; i < 4; i++) {
        new_food_pos_(ctx, rand);
        ++ctx->occupied_cells;
    }
}

void snake_redir(SnakeContext *ctx, SnakeDirection dir)
{
    SnakeCell ct = snake_cell_at(ctx, ctx->head_xpos, ctx->head_ypos);
    if ((dir == SNAKE_DIR_RIGHT && ct != SNAKE_CELL_SLEFT) ||
        (dir == SNAKE_DIR_UP && ct != SNAKE_CELL_SDOWN) ||
        (dir == SNAKE_DIR_LEFT && ct != SNAKE_CELL_SRIGHT) ||
        (dir == SNAKE_DIR_DOWN && ct != SNAKE_CELL_SUP))
        ctx->next_dir = dir;
}

static void wrap_around_(char *val, char max)
{
    if (*val < 0)
        *val = max - 1;
    if (*val > max - 1)
        *val = 0;
}

void snake_step(SnakeContext *ctx, RandFunc rand)
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
        snake_initialize(ctx, rand);
        return;
    }
    put_cell_at_(ctx, prev_xpos, prev_ypos, dir_as_cell);
    put_cell_at_(ctx, ctx->head_xpos, ctx->head_ypos, dir_as_cell);
    if (ct == SNAKE_CELL_FOOD) {
        if (are_cells_full_(ctx)) {
            snake_initialize(ctx, rand);
            return;
        }
        new_food_pos_(ctx, rand);
        ++ctx->inhibit_tail_step;
        ++ctx->occupied_cells;
    }
}

SnakeCell snake_cell_at(const SnakeContext *ctx, char x, char y)
{
    const int shift = SHIFT(x, y);
    unsigned short range;
    memcpy(&range, ctx->cells + (shift / CHAR_BIT), sizeof(range));
    return (SnakeCell)((range >> (shift % CHAR_BIT)) & THREE_BITS);
}
