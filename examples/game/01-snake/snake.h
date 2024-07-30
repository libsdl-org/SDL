/*
 * Interface definition of the Snake game.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */
#ifndef SNAKE_H
#define SNAKE_H
#define SNAKE_GAME_WIDTH  24U
#define SNAKE_GAME_HEIGHT 18U
#define SNAKE_MATRIX_SIZE (SNAKE_GAME_WIDTH * SNAKE_GAME_HEIGHT)

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

typedef Sint32 SDLCALL (*RandFunc)(Sint32 n);

void snake_initialize(SnakeContext *ctx, RandFunc rand);
void snake_redir(SnakeContext *ctx, SnakeDirection dir);
void snake_step(SnakeContext *ctx, RandFunc rand);
SnakeCell snake_cell_at(const SnakeContext *ctx, char x, char y);

#endif /* SNAKE_H */
