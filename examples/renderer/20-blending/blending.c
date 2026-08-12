/*
 * Blending combines a source color 'src',
 * with the pixels already on the screen 'dst',
 * to produce transparency and other visual effects.
 *
 * formula: dst := (a * dst) op (b * src)
 *
 * where:
 *     dst: existed pixel on the screen.
 *     src: new pixel.
 *     a:   dst factor.
 *     b:   src factor.
 *     op:  blend operation (usually addition).
 *
 * In graphics programming, color and alpha are usually blended separately:
 *     dstRGB := (a * srcRGB) op (b * dstRGB)
 *     dstA   := (c * srcA)   op (d * dstA)
 *
 * This example uses SDL_SetTextureBlendMode() to apply blending to textures,
 * and uses SDL_ComposeCustomBlendMode() to create a custom blend mode.
 *
 * You can also use SDL_SetRenderDrawBlendMode() to apply blending to the
 * entire renderer, but it only affects filled rects and lines, not textures.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */


#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

/* UI Constants */
#define ROWS 2
#define COLS 3
#define GRID_SIZE    ((WINDOW_WIDTH - 1) / 18.0f)
#define PANEL_SIZE   (GRID_SIZE * 4)
#define ROW_OFFSET   ((WINDOW_HEIGHT - ROWS * PANEL_SIZE) / 4)
#define COL_OFFSET   (GRID_SIZE * COLS)
#define RECT_SIZE     50.0f
#define RED_OFFSET   (GRID_SIZE)
#define GREEN_OFFSET (RECT_SIZE / 3 + GRID_SIZE)
#define BLUE_OFFSET  (RECT_SIZE * 2 / 3 + GRID_SIZE)

static SDL_FRect     panels[ROWS*COLS];
static SDL_Window    *window             = NULL;
static SDL_Renderer  *renderer           = NULL;
static SDL_Texture   *red_rect_texture   = NULL;
static SDL_Texture   *green_rect_texture = NULL;
static SDL_Texture   *blue_rect_texture  = NULL;
static Uint8         alpha               = 255;
static SDL_BlendMode blend_modes[]       = {
    /*The default no blending: dstRGB := srcRGB
                                dstA  := srcA   */
    SDL_BLENDMODE_NONE,

    /* Alpha blending: dstRGB := srcA * srcRGB + (1 - srcA) * dstRGB
                       dstA   := srcA          + (1 - srcA) * dstA   */
    SDL_BLENDMODE_BLEND,

    /* Additive blending: dstRGB := srcRGB + dstRGB
                          dstA   := srcA   + dstA   */
    SDL_BLENDMODE_ADD,

    /* Modulate blending: dstRGB := srcRGB * dstRGB
                          dstA   := dstA            */
    SDL_BLENDMODE_MOD,

    /* Multiply blending: dstRGB := srcRGB * dstRGB + (1 - srcA) * dstRGB
                          dstA   := dstA                                  */
    SDL_BLENDMODE_MUL,

    /* Our custom blending 'Screen Blending': dstRGB := 1 - (1 - dstRGB) * (1 - srcRGB)
                                              dstA   := dstA                            */
    0
};
static const char *blend_mode_names[] = { "NONE", "BLEND", "ADD", "MOD", "MUL", "SCREEN \"CUSTOM\"" };

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface  = NULL;

    SDL_SetAppMetadata("Example Blending", "1.0", "com.example.blending");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("examples/renderer/blending", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    int row = 0;
    int col = 0;
    for (row = 0; row < ROWS; row++)
    {
        for (col = 0; col < COLS; col++)
        {
            panels[col + row*COLS] = (SDL_FRect){ col*PANEL_SIZE + col*COL_OFFSET, row*PANEL_SIZE + (row+1)*ROW_OFFSET, PANEL_SIZE, PANEL_SIZE };
        }
    }

    /* Create 'screen blend' mode */
    blend_modes[ROWS*COLS - 1] = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, /* srcRGB factor    := (1 - dstRGB) */
        SDL_BLENDFACTOR_ONE,                 /* dstRGB factor    := 1            */
        SDL_BLENDOPERATION_ADD,              /* RGB    operation := +            */
        SDL_BLENDFACTOR_ZERO,                /* srcA   factor    := 0            */
        SDL_BLENDFACTOR_ONE,                 /* dstA   factor    := dstA         */
        SDL_BLENDOPERATION_ADD               /* A      operation := +            */
    );

    surface = SDL_CreateSurface((int)RECT_SIZE, (int)RECT_SIZE, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) {
        SDL_Log("Couldn't create surface: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_FillSurfaceRect(surface, NULL, 0xFF0000FF); /* Red */
    red_rect_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!red_rect_texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_FillSurfaceRect(surface, NULL, 0x00FF00FF); /* Green */
    green_rect_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!green_rect_texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_FillSurfaceRect(surface, NULL, 0x0000FFFF); /* Blue */
    blue_rect_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!blue_rect_texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        /* UP arrow increase alpha */
        if (event->key.key == SDLK_UP && alpha <= 255-8) alpha += 8;
        /* DOWN arrow decrease alpha */
        if (event->key.key == SDLK_DOWN && alpha >= 8)   alpha -= 8;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    int i = 0;
    float x;
    float y;
    /* Render checkerboard panels */
    for (i = 0; i < ROWS*COLS; i++)
    {
        /* Loop through the panel pixels */
        for (y = panels[i].y; y < PANEL_SIZE + panels[i].y; y += GRID_SIZE)
        {
            for (x = panels[i].x; x < PANEL_SIZE + panels[i].x; x += GRID_SIZE)
            {
                SDL_FRect grid = { x, y, GRID_SIZE, GRID_SIZE };
                bool dark      = (int)(x/GRID_SIZE + y/GRID_SIZE) % 2;

                if (dark) SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);    /* Darker color  */
                else      SDL_SetRenderDrawColor(renderer, 110, 110, 110, 255); /* Lighter color */

                SDL_RenderFillRect(renderer, &grid);
            }
        }

        /* Label the blend mode */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugText(renderer, panels[i].x, panels[i].y - 15, blend_mode_names[i]);
    }

    /* Render panels */
    SDL_RenderRects(renderer, panels, ROWS*COLS);

    /* Render UI text */
    SDL_RenderDebugText(renderer, (WINDOW_WIDTH - 176) / 2, WINDOW_HEIGHT - 30, "UP/DOWN: CHANGE ALPHA");
    SDL_RenderDebugTextFormat(renderer, (WINDOW_WIDTH - 80) / 2, WINDOW_HEIGHT - 20, "ALPHA: %d", alpha);

    /* Update textures alpha mod */
    SDL_SetTextureAlphaMod(red_rect_texture,   alpha);
    SDL_SetTextureAlphaMod(green_rect_texture, alpha);
    SDL_SetTextureAlphaMod(blue_rect_texture,  alpha);

    /* Render panels */
    for (i = 0; i < ROWS*COLS; i++) {
        /* Update rects destination */
        SDL_FRect red_dst   = { panels[i].x + RED_OFFSET,   panels[i].y + RED_OFFSET,   RECT_SIZE, RECT_SIZE };
        SDL_FRect green_dst = { panels[i].x + GREEN_OFFSET, panels[i].y + GREEN_OFFSET, RECT_SIZE, RECT_SIZE };
        SDL_FRect blue_dst  = { panels[i].x + BLUE_OFFSET,  panels[i].y + BLUE_OFFSET,  RECT_SIZE, RECT_SIZE };

        /* Apply the current blend mode */
        const bool supported = SDL_SetTextureBlendMode(red_rect_texture, blend_modes[i]);  /* just make sure the renderer supports this blend mode */
        SDL_SetTextureBlendMode(green_rect_texture, blend_modes[i]);
        SDL_SetTextureBlendMode(blue_rect_texture, blend_modes[i]);

        /* Render textures */
        SDL_RenderTexture(renderer, red_rect_texture,   NULL, &red_dst);
        SDL_RenderTexture(renderer, green_rect_texture, NULL, &green_dst);
        SDL_RenderTexture(renderer, blue_rect_texture,  NULL, &blue_dst);

        /* Not all renderers support all blend modes. The renderer will try to pick something close in this case,
           but it should be noted that the results might be unexpected, so we add "[UNSUPPORTED]" to this panel. */
        if (!supported) {
            const float textwidth = 104.0f;
            const SDL_FRect dst = { panels[i].x + ((panels[i].w - textwidth) / 2.0f), panels[i].y + (panels[i].h - 8), textwidth, 8 };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, dst.x, dst.y, "[UNSUPPORTED]");
        }
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyTexture(red_rect_texture);
    SDL_DestroyTexture(green_rect_texture);
    SDL_DestroyTexture(blue_rect_texture);
}
