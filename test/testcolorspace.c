/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
static int stage_index = 0;
static int done;
static float HDR_headroom = 1.0f;

enum
{
    StageClearBackground,
    StageDrawBackground,
    StageTextureBackground,
    StageTargetBackground,
    StageBlendDrawing,
    StageBlendTexture,
    StageGradientDrawing,
    StageGradientTexture,
    StageCount
};

static void FreeRenderer(void)
{
    SDLTest_CleanupTextDrawing();
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
}

static void UpdateHDRState(void)
{
    SDL_PropertiesID props;
    bool HDR_enabled;

    props = SDL_GetWindowProperties(window);
    HDR_enabled = SDL_GetBooleanProperty(props, SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN, false);

    SDL_Log("HDR %s", HDR_enabled ? "enabled" : "disabled");

    if (HDR_enabled) {
        props = SDL_GetRendererProperties(renderer);
        if (SDL_GetNumberProperty(props, SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB) != SDL_COLORSPACE_SRGB_LINEAR) {
            SDL_Log("Run with --colorspace linear to display HDR colors");
        }
        HDR_headroom = SDL_GetFloatProperty(props, SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT, 1.0f);
    }
}

static void CreateRenderer(void)
{
    SDL_PropertiesID props;

    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GetRenderDriver(renderer_index));
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, colorspace);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        return;
    }

    renderer_name = SDL_GetRendererName(renderer);
    SDL_Log("Created renderer %s", renderer_name);

    UpdateHDRState();
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
    ++stage_index;
    if (stage_index == StageCount) {
        stage_index = 0;
    }
}

static void PrevStage(void)
{
    --stage_index;
    if (stage_index == -1) {
        stage_index += StageCount;
    }
}

static bool ReadPixel(int x, int y, SDL_Color *c)
{
    SDL_Surface *surface;
    SDL_Rect r;
    bool result = false;

    r.x = x;
    r.y = y;
    r.w = 1;
    r.h = 1;

    surface = SDL_RenderReadPixels(renderer, &r);
    if (surface) {
        /* Don't tonemap back to SDR, our source content was SDR */
        SDL_SetStringProperty(SDL_GetSurfaceProperties(surface), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "*=1");

        if (SDL_ReadSurfacePixel(surface, 0, 0, &c->r, &c->g, &c->b, &c->a)) {
            result = true;
        } else {
            SDL_Log("Couldn't read pixel: %s", SDL_GetError());
        }
        SDL_DestroySurface(surface);
    } else {
        SDL_Log("Couldn't read back pixels: %s", SDL_GetError());
    }
    return result;
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

static SDL_Texture *CreateGrayTexture(void)
{
    SDL_Texture *texture;
    Uint8 pixels[4];

    /* Floating point textures are in the linear colorspace by default */
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (!texture) {
        return NULL;
    }

    pixels[0] = 128;
    pixels[1] = 128;
    pixels[2] = 128;
    pixels[3] = 255;
    SDL_UpdateTexture(texture, NULL, pixels, sizeof(pixels));

    return texture;
}

static void RenderTextureBackground(void)
{
    /* Fill the background with a 50% gray texture.
     * This will be darker when using sRGB colors and lighter using linear colors
     */
    SDL_Texture *texture = CreateGrayTexture();
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_DestroyTexture(texture);

    /* Check the renderered pixels */
    SDL_Color c;
    if (!ReadPixel(0, 0, &c)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Fill 50%% Gray Texture");
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Background color written: 0x808080, read: 0x%.2x%.2x%.2x", c.r, c.g, c.b);
    y += TEXT_LINE_ADVANCE;
    if (c.r != 128) {
        DrawText(x, y, "Incorrect background color, unknown reason");
        y += TEXT_LINE_ADVANCE;
    }
}

static void RenderTargetBackground(void)
{
    /* Fill the background with a 50% gray texture.
     * This will be darker when using sRGB colors and lighter using linear colors
     */
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1);
    SDL_Texture *texture = CreateGrayTexture();

    /* Fill the render target with the gray texture */
    SDL_SetRenderTarget(renderer, target);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_DestroyTexture(texture);

    /* Fill the output with the render target */
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderTexture(renderer, target, NULL, NULL);
    SDL_DestroyTexture(target);

    /* Check the renderered pixels */
    SDL_Color c;
    if (!ReadPixel(0, 0, &c)) {
        return;
    }

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Fill 50%% Gray Render Target");
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
    DrawText(x, y, "Test: Draw Blending");
    y += TEXT_LINE_ADVANCE;
    if (cr.r == 199 && cr.g == 193 && cr.b == 121) {
        DrawText(x, y, "Correct blend color, blending in linear space");
    } else if ((cr.r == 192 && cr.g == 163 && cr.b == 83) ||
               (cr.r == 191 && cr.g == 162 && cr.b == 82)) {
        DrawText(x, y, "Correct blend color, blending in sRGB space");
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
    DrawText(x, y, "Test: Texture Blending");
    y += TEXT_LINE_ADVANCE;
    if (cr.r == 199 && cr.g == 193 && cr.b == 121) {
        DrawText(x, y, "Correct blend color, blending in linear space");
    } else if ((cr.r == 192 && cr.g == 163 && cr.b == 83) ||
               (cr.r == 191 && cr.g == 162 && cr.b == 82)) {
        DrawText(x, y, "Correct blend color, blending in sRGB space");
    } else {
        DrawText(x, y, "Incorrect blend color, unknown reason");
    }
    y += TEXT_LINE_ADVANCE;

    SDL_DestroyTexture(a);
    SDL_DestroyTexture(b);
}

static void DrawGradient(float x, float y, float width, float height, float start, float end)
{
    float xy[8];
    const int xy_stride = 2 * sizeof(float);
    SDL_FColor color[4];
    const int color_stride = sizeof(SDL_FColor);
    const int num_vertices = 4;
    const int indices[6] = { 0, 1, 2, 0, 2, 3 };
    const int num_indices = 6;
    const int size_indices = 4;
    float minx, miny, maxx, maxy;
    SDL_FColor min_color = { start, start, start, 1.0f };
    SDL_FColor max_color = { end, end, end, 1.0f };

    minx = x;
    miny = y;
    maxx = minx + width;
    maxy = miny + height;

    xy[0] = minx;
    xy[1] = miny;
    xy[2] = maxx;
    xy[3] = miny;
    xy[4] = maxx;
    xy[5] = maxy;
    xy[6] = minx;
    xy[7] = maxy;

    color[0] = min_color;
    color[1] = max_color;
    color[2] = max_color;
    color[3] = min_color;

    SDL_RenderGeometryRaw(renderer, NULL, xy, xy_stride, color, color_stride, NULL, 0, num_vertices, indices, num_indices, size_indices);
}

static void RenderGradientDrawing(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Draw SDR and HDR gradients");
    y += TEXT_LINE_ADVANCE;

    y += TEXT_LINE_ADVANCE;

    DrawText(x, y, "SDR gradient");
    y += TEXT_LINE_ADVANCE;
    DrawGradient(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, 1.0f);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    if (HDR_headroom > 1.0f) {
        DrawText(x, y, "HDR gradient");
    } else {
        DrawText(x, y, "No HDR headroom, HDR and SDR gradient are the same");
    }
    y += TEXT_LINE_ADVANCE;
    /* Drawing is in the sRGB colorspace, so we need to use the color scale, which is applied in linear space, to get into high dynamic range */
    SDL_SetRenderColorScale(renderer, HDR_headroom);
    DrawGradient(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, 1.0f);
    SDL_SetRenderColorScale(renderer, 1.0f);
    y += 64.0f;
}

static SDL_Texture *CreateGradientTexture(int width, float start, float end)
{
    SDL_Texture *texture;
    float *pixels;

    /* Floating point textures are in the linear colorspace by default */
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA128_FLOAT, SDL_TEXTUREACCESS_STATIC, width, 1);
    if (!texture) {
        return NULL;
    }

    pixels = (float *)SDL_malloc(width * sizeof(float) * 4);
    if (pixels) {
        int i;
        float length = (end - start);

        for (i = 0; i < width; ++i) {
            float v = (start + (length * i) / width);
            pixels[i * 4 + 0] = v;
            pixels[i * 4 + 1] = v;
            pixels[i * 4 + 2] = v;
            pixels[i * 4 + 3] = 1.0f;
        }
        SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(float) * 4);
        SDL_free(pixels);
    }
    return texture;
}

static void DrawGradientTexture(float x, float y, float width, float height, float start, float end)
{
    SDL_FRect rect = { x, y, width, height };
    SDL_Texture *texture = CreateGradientTexture((int)width, start, end);
    SDL_RenderTexture(renderer, texture, NULL, &rect);
    SDL_DestroyTexture(texture);
}

static void RenderGradientTexture(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawText(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawText(x, y, "Test: Texture SDR and HDR gradients");
    y += TEXT_LINE_ADVANCE;

    y += TEXT_LINE_ADVANCE;

    DrawText(x, y, "SDR gradient");
    y += TEXT_LINE_ADVANCE;
    DrawGradientTexture(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, 1.0f);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    if (HDR_headroom > 1.0f) {
        DrawText(x, y, "HDR gradient");
    } else {
        DrawText(x, y, "No HDR headroom, HDR and SDR gradient are the same");
    }
    y += TEXT_LINE_ADVANCE;
    /* The gradient texture is in the linear colorspace, so we can use the HDR_headroom value directly */
    DrawGradientTexture(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, HDR_headroom);
    y += 64.0f;
}

static void loop(void)
{
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            switch (event.key.key) {
            case SDLK_ESCAPE:
                done = 1;
                break;
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
        } else if (event.type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED) {
            UpdateHDRState();
        } else if (event.type == SDL_EVENT_QUIT) {
            done = 1;
        }
    }

    if (renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        switch (stage_index) {
        case StageClearBackground:
            RenderClearBackground();
            break;
        case StageDrawBackground:
            RenderDrawBackground();
            break;
        case StageTextureBackground:
            RenderTextureBackground();
            break;
        case StageTargetBackground:
            RenderTargetBackground();
            break;
        case StageBlendDrawing:
            RenderBlendDrawing();
            break;
        case StageBlendTexture:
            RenderBlendTexture();
            break;
        case StageGradientDrawing:
            RenderGradientDrawing();
            break;
        case StageGradientTexture:
            RenderGradientTexture();
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
    SDL_Log("Usage: %s [--renderer renderer] [--colorspace colorspace]", argv0);
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
                } else if (SDL_strcasecmp(colorspace_name, "linear") == 0) {
                    colorspace = SDL_COLORSPACE_SRGB_LINEAR;
/* Not currently supported
                } else if (SDL_strcasecmp(colorspace_name, "HDR10") == 0) {
                    colorspace = SDL_COLORSPACE_HDR10;
*/
                } else {
                    SDL_Log("Unknown colorspace %s", argv[i + 1]);
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
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    renderer_count = SDL_GetNumRenderDrivers();
    SDL_Log("There are %d render drivers:", renderer_count);
    for (i = 0; i < renderer_count; ++i) {
        const char *name = SDL_GetRenderDriver(i);

        if (renderer_name && SDL_strcasecmp(renderer_name, name) == 0) {
            renderer_index = i;
        }
        SDL_Log("    %s", name);
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
