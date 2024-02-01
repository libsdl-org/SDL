/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

#define TEXT_START_X 6.0f
#define TEXT_START_Y 6.0f
#define TEXT_LINE_ADVANCE FONT_CHARACTER_SIZE * 2

static SDL_Window *window;
static SDL_Renderer *renderer;
static const char *renderer_name;
static SDL_Colorspace colorspace = SDL_COLORSPACE_SRGB;
static const char *colorspace_name = "sRGB";
static int renderer_count = 0;
static int renderer_index = 0;
static int stage_count = 4;
static int stage_index = 0;
static int done;

static void FreeRenderer(void)
{
    SDLTest_CleanupTextDrawing();
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
}

static void CreateRenderer(void)
{
    SDL_PropertiesID props;
    SDL_RendererInfo info;

    props = SDL_CreateProperties();
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GetRenderDriver(renderer_index));
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_INPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, colorspace);
    SDL_SetBooleanProperty(props, SDL_PROP_RENDERER_CREATE_COLORSPACE_CONVERSION_BOOLEAN, SDL_TRUE);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s\n", SDL_GetError());
        return;
    }

    SDL_GetRendererInfo(renderer, &info);
    SDL_Log("Created renderer %s\n", info.name);
    renderer_name = info.name;
}

static void NextRenderer( void )
{
    if (renderer_count <= 0) {
        return;
    }

    ++renderer_index;
    if (renderer_index == renderer_count) {
        renderer_index = 0;
    }
    FreeRenderer();
    CreateRenderer();
}

static void PrevRenderer(void)
{
    if (renderer_count <= 0) {
        return;
    }

    --renderer_index;
    if (renderer_index == -1) {
        renderer_index += renderer_count;
    }
    FreeRenderer();
    CreateRenderer();
}

static void NextStage(void)
{
    if (stage_count <= 0) {
        return;
    }

    ++stage_index;
    if (stage_index == stage_count) {
        stage_index = 0;
    }
}

static void PrevStage(void)
{
    --stage_index;
    if (stage_index == -1) {
        stage_index += stage_count;
    }
}

static SDL_bool ReadPixel(int x, int y, SDL_Color *c)
{
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = 1;
    r.h = 1;
    if (SDL_RenderReadPixels(renderer, &r, SDL_PIXELFORMAT_RGBA32, c, sizeof(*c)) < 0) {
        SDL_Log("Couldn't read back pixels: %s\n", SDL_GetError());
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static void DrawText(float x, float y, const char *fmt, ...)
{
    char *text;

    va_list ap;
    va_start(ap, fmt);
    SDL_vasprintf(&text, fmt, ap);
    va_end(ap);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, x + 1.0f, y + 1.0f, text);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDLTest_DrawString(renderer, x, y, text);
    SDL_free(text);
}

static void RenderClearBackground(void)
{
    /* Draw a 50% gray background.
     * This will be darker when using sRGB colors and lighter using linear colors
     */
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderClear(renderer);

    /* Check the renderered pixels */
    SDL_Color c;
    if (!ReadPixel(0, 0, &c)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Clear 50%% Gray Background");
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Background color written: 0x808080, read: 0x%.2x%.2x%.2x", c.r, c.g, c.b);
    y += TEXT_LINE_ADVANCE;
    if (c.r != 128) {
        DrawText(x, y, "Incorrect background color, unknown reason");
        y += TEXT_LINE_ADVANCE;
    }
}

static void RenderDrawBackground(void)
{
    /* Draw a 50% gray background.
     * This will be darker when using sRGB colors and lighter using linear colors
     */
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderFillRect(renderer, NULL);

    /* Check the renderered pixels */
    SDL_Color c;
    if (!ReadPixel(0, 0, &c)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Draw 50%% Gray Background");
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Background color written: 0x808080, read: 0x%.2x%.2x%.2x", c.r, c.g, c.b);
    y += TEXT_LINE_ADVANCE;
    if (c.r != 128) {
        DrawText(x, y, "Incorrect background color, unknown reason");
        y += TEXT_LINE_ADVANCE;
    }
}

static void RenderBlendDrawing(void)
{
    SDL_Color a = { 238, 70, 166, 255 }; /* red square */
    SDL_Color b = { 147, 255, 0, 255 };  /* green square */
    SDL_FRect rect;

    /* Draw a green square blended over a red square
     * This will have different effects based on whether sRGB colorspaces and sRGB vs linear blending is used.
     */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    rect.x = WINDOW_WIDTH / 3;
    rect.y = 0;
    rect.w = WINDOW_WIDTH / 3;
    rect.h = WINDOW_HEIGHT;
    SDL_SetRenderDrawColor(renderer, a.r, a.g, a.b, a.a);
    SDL_RenderFillRect(renderer, &rect);

    rect.x = 0;
    rect.y = WINDOW_HEIGHT / 3;
    rect.w = WINDOW_WIDTH;
    rect.h = WINDOW_HEIGHT / 6;
    SDL_SetRenderDrawColor(renderer, b.r, b.g, b.b, b.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, b.r, b.g, b.b, 128);
    rect.y += WINDOW_HEIGHT / 6;
    SDL_RenderFillRect(renderer, &rect);

    SDL_Color ar, br, cr;
    if (!ReadPixel(WINDOW_WIDTH / 2, 0, &ar) ||
        !ReadPixel(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 3, &br) ||
        !ReadPixel(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, &cr)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Draw Linear Blending");
    y += TEXT_LINE_ADVANCE;
    if (cr.r == 199 && cr.g == 193 && cr.b == 121) {
        DrawText(x, y, "Correct blend color in linear space");
    } else if ((cr.r == 192 && cr.g == 163 && cr.b == 83) ||
               (cr.r == 191 && cr.g == 162 && cr.b == 82)) {
        DrawText(x, y, "Incorrect blend color, blending in sRGB space");
    } else if (cr.r == 214 && cr.g == 156 && cr.b == 113) {
        DrawText(x, y, "Incorrect blend color, blending in PQ space");
    } else {
        DrawText(x, y, "Incorrect blend color, unknown reason");
    }
    y += TEXT_LINE_ADVANCE;
}

static void RenderBlendTexture(void)
{
    SDL_Color color_a = { 238, 70, 166, 255 }; /* red square */
    SDL_Color color_b = { 147, 255, 0, 255 };  /* green square */
    SDL_Texture *a;
    SDL_Texture *b;
    SDL_FRect rect;

    /* Draw a green square blended over a red square
     * This will have different effects based on whether sRGB colorspaces and sRGB vs linear blending is used.
     */
    a = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
    SDL_UpdateTexture(a, NULL, &color_a, sizeof(color_a));
    b = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
    SDL_UpdateTexture(b, NULL, &color_b, sizeof(color_b));

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    rect.x = WINDOW_WIDTH / 3;
    rect.y = 0;
    rect.w = WINDOW_WIDTH / 3;
    rect.h = WINDOW_HEIGHT;
    SDL_RenderTexture(renderer, a, NULL, &rect);

    rect.x = 0;
    rect.y = WINDOW_HEIGHT / 3;
    rect.w = WINDOW_WIDTH;
    rect.h = WINDOW_HEIGHT / 6;
    SDL_RenderTexture(renderer, b, NULL, &rect);
    rect.y += WINDOW_HEIGHT / 6;
    SDL_SetTextureBlendMode(b, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaModFloat(b, 128 / 255.0f);
    SDL_RenderTexture(renderer, b, NULL, &rect);

    SDL_Color ar, br, cr;
    if (!ReadPixel(WINDOW_WIDTH / 2, 0, &ar) ||
        !ReadPixel(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 3, &br) ||
        !ReadPixel(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, &cr)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Texture Linear Blending");
    y += TEXT_LINE_ADVANCE;
    if (cr.r == 199 && cr.g == 193 && cr.b == 121) {
        DrawText(x, y, "Correct blend color in linear space");
    } else if ((cr.r == 192 && cr.g == 163 && cr.b == 83) ||
               (cr.r == 191 && cr.g == 162 && cr.b == 82)) {
        DrawText(x, y, "Incorrect blend color, blending in sRGB space");
    } else {
        DrawText(x, y, "Incorrect blend color, unknown reason");
    }
    y += TEXT_LINE_ADVANCE;

    SDL_DestroyTexture(a);
    SDL_DestroyTexture(b);
}

static void loop(void)
{
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                done = 1;
            case SDLK_SPACE:
            case SDLK_RIGHT:
                NextStage();
                break;
            case SDLK_LEFT:
                PrevStage();
                break;
            case SDLK_DOWN:
                NextRenderer();
                break;
            case SDLK_UP:
                PrevRenderer();
                break;
            default:
                break;
            }
        } else if (event.type == SDL_EVENT_QUIT) {
            done = 1;
        }
    }

    if (renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        switch (stage_index) {
        case 0:
            RenderClearBackground();
            break;
        case 1:
            RenderDrawBackground();
            break;
        case 2:
            RenderBlendDrawing();
            break;
        case 3:
            RenderBlendTexture();
            break;
        }

        SDL_RenderPresent(renderer);
    }
    SDL_Delay(100);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

static void LogUsage(const char *argv0)
{
    SDL_Log("Usage: %s [--renderer renderer] [--colorspace colorspace]\n", argv0);
}

int main(int argc, char *argv[])
{
    int return_code = 1;
    int i;

    for (i = 1; i < argc; ++i) {
        if (SDL_strcmp(argv[i], "--renderer") == 0) {
            if (argv[i + 1]) {
                renderer_name = argv[i + 1];
                ++i;
            } else {
                LogUsage(argv[0]);
                goto quit;
            }
        } else if (SDL_strcmp(argv[i], "--colorspace") == 0) {
            if (argv[i + 1]) {
                colorspace_name = argv[i + 1];
                if (SDL_strcasecmp(colorspace_name, "sRGB") == 0) {
                    colorspace = SDL_COLORSPACE_SRGB;
                } else if (SDL_strcasecmp(colorspace_name, "scRGB") == 0) {
                    colorspace = SDL_COLORSPACE_SCRGB;
/* Not currently supported
                } else if (SDL_strcasecmp(colorspace_name, "HDR10") == 0) {
                    colorspace = SDL_COLORSPACE_HDR10;
*/
                } else {
                    SDL_Log("Unknown colorspace %s\n", argv[i + 1]);
                    goto quit;
                }
                ++i;
            } else {
                LogUsage(argv[0]);
                goto quit;
            }
        } else {
            LogUsage(argv[0]);
            goto quit;
        }
    }

    window = SDL_CreateWindow("SDL colorspace test", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("Couldn't create window: %s\n", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    renderer_count = SDL_GetNumRenderDrivers();
    SDL_Log("There are %d render drivers:\n", renderer_count);
    for (i = 0; i < renderer_count; ++i) {
        const char *name = SDL_GetRenderDriver(i);

        if (renderer_name && SDL_strcasecmp(renderer_name, name) == 0) {
            renderer_index = i;
        }
        SDL_Log("    %s\n", name);
    }
    CreateRenderer();

    /* Main render loop */
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    return_code = 0;
quit:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return return_code;
}
