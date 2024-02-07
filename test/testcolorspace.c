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

/* The value for the SDR white level on an SDR display, scRGB 1.0 */
#define SDR_DISPLAY_WHITE_LEVEL  80.0f

/* The default value for the SDR white level on an HDR display */
#define DEFAULT_SDR_WHITE_LEVEL 200.0f

/* The default value for the HDR white level on an HDR display */
#define DEFAULT_HDR_WHITE_LEVEL 400.0f

/* The maximum value for the HDR white level on an HDR display */
#define MAXIMUM_HDR_WHITE_LEVEL 1000.0f

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
static SDL_bool HDR_enabled = SDL_FALSE;
static float SDR_white_level = SDR_DISPLAY_WHITE_LEVEL;
static float SDR_color_scale = 1.0f;
static SDL_FRect SDR_calibration_rect;
static float HDR_white_level = DEFAULT_HDR_WHITE_LEVEL;
static float HDR_color_scale = DEFAULT_HDR_WHITE_LEVEL / SDR_DISPLAY_WHITE_LEVEL;
static SDL_FRect HDR_calibration_rect;

enum
{
    StageClearBackground,
    StageDrawBackground,
    StageBlendDrawing,
    StageBlendTexture,
    StageHDRCalibration,
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

static float GetDisplaySDRWhiteLevel(void)
{
    SDL_PropertiesID props;

    HDR_enabled = SDL_FALSE;

    props = SDL_GetRendererProperties(renderer);
    if (SDL_GetNumberProperty(props, SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB) != SDL_COLORSPACE_SCRGB) {
        /* We're not displaying in HDR, use the SDR white level */
        return SDR_DISPLAY_WHITE_LEVEL;
    }

    props = SDL_GetDisplayProperties(SDL_GetDisplayForWindow(window));
    if (!SDL_GetBooleanProperty(props, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, SDL_FALSE)) {
        /* HDR is not enabled on the display */
        return SDR_DISPLAY_WHITE_LEVEL;
    }

    HDR_enabled = SDL_TRUE;

    return SDL_GetFloatProperty(props, SDL_PROP_DISPLAY_SDR_WHITE_LEVEL_FLOAT, DEFAULT_SDR_WHITE_LEVEL);
}

static void SetSDRWhiteLevel(float value)
{
    if (value == SDR_white_level) {
        return;
    }

    SDL_Log("SDR white level set to %g nits\n", value);
    SDR_white_level = value;
    SDR_color_scale = SDR_white_level / SDR_DISPLAY_WHITE_LEVEL;
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
}

static void UpdateSDRWhiteLevel(void)
{
    SetSDRWhiteLevel(GetDisplaySDRWhiteLevel());
}

static void SetHDRWhiteLevel(float value)
{
    if (value == HDR_white_level) {
        return;
    }

    SDL_Log("HDR white level set to %g nits\n", value);
    HDR_white_level = value;
    HDR_color_scale = HDR_white_level / SDR_DISPLAY_WHITE_LEVEL;
}

static void CreateRenderer(void)
{
    SDL_PropertiesID props;
    SDL_RendererInfo info;

    props = SDL_CreateProperties();
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GetRenderDriver(renderer_index));
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, colorspace);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s\n", SDL_GetError());
        return;
    }

    SDL_GetRendererInfo(renderer, &info);
    SDL_Log("Created renderer %s\n", info.name);
    renderer_name = info.name;

    UpdateSDRWhiteLevel();

    SDL_Log("HDR is %s\n", HDR_enabled ? "enabled" : "disabled");
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
    if (StageCount <= 0) {
        return;
    }

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

static SDL_bool ReadPixel(int x, int y, SDL_Color *c)
{
    SDL_Surface *surface;
    SDL_Rect r;
    SDL_bool result = SDL_FALSE;

    r.x = x;
    r.y = y;
    r.w = 1;
    r.h = 1;

    surface = SDL_RenderReadPixels(renderer, &r);
    if (surface) {
        if (SDL_ReadSurfacePixel(surface, 0, 0, &c->r, &c->g, &c->b, &c->a) == 0) {
            result = SDL_TRUE;
        } else {
            SDL_Log("Couldn't read pixel: %s\n", SDL_GetError());
        }
        SDL_DestroySurface(surface);
    } else {
        SDL_Log("Couldn't read back pixels: %s\n", SDL_GetError());
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

static void DrawTextWhite(float x, float y, const char *fmt, ...)
{
    char *text;

    va_list ap;
    va_start(ap, fmt);
    SDL_vasprintf(&text, fmt, ap);
    va_end(ap);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
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

    SDL_RenderGeometryRawFloat(renderer, NULL, xy, xy_stride, color, color_stride, NULL, 0, num_vertices, indices, num_indices, size_indices);
}

static float scRGBtoNits(float v)
{
    return v * 80.0f;
}

static float scRGBfromNits(float v)
{
    return v / 80.0f;
}

static float sRGBtoLinear(float v)
{
    if (v <= 0.04045f) {
        v = (v / 12.92f);
    } else {
        v = SDL_powf(((v + 0.055f) / 1.055f), 2.4f);
    }
    return v;
}

static float sRGBFromLinear(float v)
{
    if (v <= 0.0031308f) {
        v = (v * 12.92f);
    } else {
        v = (SDL_powf(v, 1.0f / 2.4f) * 1.055f - 0.055f);
    }
    return v;
}

static float sRGBtoNits(float v)
{
    return scRGBtoNits(sRGBtoLinear(v));
}

static float sRGBfromNits(float v)
{
    return sRGBFromLinear(scRGBfromNits(v));
}

static void RenderGradientDrawing(void)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawTextWhite(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawTextWhite(x, y, "Test: Draw SDR and HDR gradients");
    y += TEXT_LINE_ADVANCE;

    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "SDR gradient (%g nits)", SDR_white_level);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradient(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(SDR_white_level));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "HDR gradient (%g nits)", HDR_white_level);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradient(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(HDR_white_level));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "HDR gradient (%g nits)", MAXIMUM_HDR_WHITE_LEVEL);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradient(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(MAXIMUM_HDR_WHITE_LEVEL));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;
}

static SDL_Texture *CreateGradientTexture(int width, float start, float end)
{
    SDL_Texture *texture;
    float *pixels;

    /* Floating point textures are in the scRGB colorspace by default */
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA128_FLOAT, SDL_TEXTUREACCESS_STATIC, width, 1);
    if (!texture) {
        return NULL;
    }

    if (colorspace == SDL_COLORSPACE_SCRGB) {
        /* The input values are in the sRGB colorspace, but we're blending in linear space */
        start = sRGBtoLinear(start);
        end = sRGBtoLinear(end);
    } else if (colorspace == SDL_COLORSPACE_SRGB) {
        /* The input values are in the sRGB colorspace, and we're blending in sRGB space */
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
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawTextWhite(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawTextWhite(x, y, "Test: Texture SDR and HDR gradients");
    y += TEXT_LINE_ADVANCE;

    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "SDR gradient (%g nits)", SDR_white_level);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradientTexture(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(SDR_white_level));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "HDR gradient (%g nits)", HDR_white_level);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradientTexture(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(HDR_white_level));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "HDR gradient (%g nits)", MAXIMUM_HDR_WHITE_LEVEL);
    y += TEXT_LINE_ADVANCE;
    SDL_SetRenderColorScale(renderer, 1.0f);
    DrawGradientTexture(x, y, WINDOW_WIDTH - 2 * x, 64.0f, 0.0f, sRGBfromNits(MAXIMUM_HDR_WHITE_LEVEL));
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;
}

static void RenderHDRCalibration(void)
{
    SDL_FRect rect;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    float x = TEXT_START_X;
    float y = TEXT_START_Y;
    DrawTextWhite(x, y, "%s %s", renderer_name, colorspace_name);
    y += TEXT_LINE_ADVANCE;
    DrawTextWhite(x, y, "Test: HDR calibration");
    y += TEXT_LINE_ADVANCE;

    y += TEXT_LINE_ADVANCE;

    if (!HDR_enabled) {
        DrawTextWhite(x, y, "HDR not enabled");
        return;
    }

    DrawTextWhite(x, y, "Select HDR maximum brightness (%g nits)", HDR_white_level);
    y += TEXT_LINE_ADVANCE;
    DrawTextWhite(x, y, "The square in the middle should just barely be visible");
    y += TEXT_LINE_ADVANCE;
    HDR_calibration_rect.x = x;
    HDR_calibration_rect.y = y;
    HDR_calibration_rect.w = WINDOW_WIDTH - 2 * x;
    HDR_calibration_rect.h = 64.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_SetRenderColorScale(renderer, MAXIMUM_HDR_WHITE_LEVEL / SDR_DISPLAY_WHITE_LEVEL);
    SDL_RenderFillRect(renderer, &HDR_calibration_rect);
    SDL_SetRenderColorScale(renderer, HDR_color_scale);
    rect = HDR_calibration_rect;
    rect.h -= 4.0f;
    rect.w = 60.0f;
    rect.x = (WINDOW_WIDTH - rect.w) / 2.0f;
    rect.y += 2.0f;
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderColorScale(renderer, SDR_color_scale);
    y += 64.0f;

    y += TEXT_LINE_ADVANCE;
    y += TEXT_LINE_ADVANCE;

    DrawTextWhite(x, y, "Select SDR maximum brightness (%g nits)", SDR_white_level);
    y += TEXT_LINE_ADVANCE;
    SDR_calibration_rect.x = x;
    SDR_calibration_rect.y = y;
    SDR_calibration_rect.w = WINDOW_WIDTH - 2 * x;
    SDR_calibration_rect.h = 64.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &SDR_calibration_rect);
}

static void OnHDRCalibrationMouseHeld(float x, float y)
{
    SDL_FPoint mouse = { x, y };

    if (SDL_PointInRectFloat(&mouse, &HDR_calibration_rect)) {
        float v = (x - HDR_calibration_rect.x) / HDR_calibration_rect.w;
        v *= (sRGBfromNits(MAXIMUM_HDR_WHITE_LEVEL) - 1.0f);
        v += 1.0f;
        v = sRGBtoNits(v);
        SetHDRWhiteLevel(v);
        return;
    }

    if (SDL_PointInRectFloat(&mouse, &SDR_calibration_rect)) {
        float v = (x - SDR_calibration_rect.x) / SDR_calibration_rect.w;
        v *= (sRGBfromNits(MAXIMUM_HDR_WHITE_LEVEL) - 1.0f);
        v += 1.0f;
        v = sRGBtoNits(v);
        SetSDRWhiteLevel(v);
        return;
    }
}

static void OnMouseHeld(float x, float y)
{
    if (stage_index == StageHDRCalibration) {
        OnHDRCalibrationMouseHeld(x, y);
    }
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
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            OnMouseHeld(event.button.x, event.button.y);
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (event.motion.state) {
                OnMouseHeld(event.button.x, event.button.y);
            }
        } else if (event.type == SDL_EVENT_DISPLAY_HDR_STATE_CHANGED) {
            SDL_Log("HDR %s\n", event.display.data1 ? "enabled" : "disabled");
            UpdateSDRWhiteLevel();
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
        case StageBlendDrawing:
            RenderBlendDrawing();
            break;
        case StageBlendTexture:
            RenderBlendTexture();
            break;
        case StageHDRCalibration:
            RenderHDRCalibration();
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
