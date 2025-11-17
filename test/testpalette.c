/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Move N sprites around on the screen as fast as possible */

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480


static const SDL_Color Palette[256] = {
    { 255,   0,   0, SDL_ALPHA_OPAQUE },
    { 255,   5,   0, SDL_ALPHA_OPAQUE },
    { 255,  11,   0, SDL_ALPHA_OPAQUE },
    { 255,  17,   0, SDL_ALPHA_OPAQUE },
    { 255,  23,   0, SDL_ALPHA_OPAQUE },
    { 255,  29,   0, SDL_ALPHA_OPAQUE },
    { 255,  35,   0, SDL_ALPHA_OPAQUE },
    { 255,  41,   0, SDL_ALPHA_OPAQUE },
    { 255,  47,   0, SDL_ALPHA_OPAQUE },
    { 255,  53,   0, SDL_ALPHA_OPAQUE },
    { 255,  59,   0, SDL_ALPHA_OPAQUE },
    { 255,  65,   0, SDL_ALPHA_OPAQUE },
    { 255,  71,   0, SDL_ALPHA_OPAQUE },
    { 255,  77,   0, SDL_ALPHA_OPAQUE },
    { 255,  83,   0, SDL_ALPHA_OPAQUE },
    { 255,  89,   0, SDL_ALPHA_OPAQUE },
    { 255,  95,   0, SDL_ALPHA_OPAQUE },
    { 255, 101,   0, SDL_ALPHA_OPAQUE },
    { 255, 107,   0, SDL_ALPHA_OPAQUE },
    { 255, 113,   0, SDL_ALPHA_OPAQUE },
    { 255, 119,   0, SDL_ALPHA_OPAQUE },
    { 255, 125,   0, SDL_ALPHA_OPAQUE },
    { 255, 131,   0, SDL_ALPHA_OPAQUE },
    { 255, 137,   0, SDL_ALPHA_OPAQUE },
    { 255, 143,   0, SDL_ALPHA_OPAQUE },
    { 255, 149,   0, SDL_ALPHA_OPAQUE },
    { 255, 155,   0, SDL_ALPHA_OPAQUE },
    { 255, 161,   0, SDL_ALPHA_OPAQUE },
    { 255, 167,   0, SDL_ALPHA_OPAQUE },
    { 255, 173,   0, SDL_ALPHA_OPAQUE },
    { 255, 179,   0, SDL_ALPHA_OPAQUE },
    { 255, 185,   0, SDL_ALPHA_OPAQUE },
    { 255, 191,   0, SDL_ALPHA_OPAQUE },
    { 255, 197,   0, SDL_ALPHA_OPAQUE },
    { 255, 203,   0, SDL_ALPHA_OPAQUE },
    { 255, 209,   0, SDL_ALPHA_OPAQUE },
    { 255, 215,   0, SDL_ALPHA_OPAQUE },
    { 255, 221,   0, SDL_ALPHA_OPAQUE },
    { 255, 227,   0, SDL_ALPHA_OPAQUE },
    { 255, 233,   0, SDL_ALPHA_OPAQUE },
    { 255, 239,   0, SDL_ALPHA_OPAQUE },
    { 255, 245,   0, SDL_ALPHA_OPAQUE },
    { 255, 251,   0, SDL_ALPHA_OPAQUE },
    { 253, 255,   0, SDL_ALPHA_OPAQUE },
    { 247, 255,   0, SDL_ALPHA_OPAQUE },
    { 241, 255,   0, SDL_ALPHA_OPAQUE },
    { 235, 255,   0, SDL_ALPHA_OPAQUE },
    { 229, 255,   0, SDL_ALPHA_OPAQUE },
    { 223, 255,   0, SDL_ALPHA_OPAQUE },
    { 217, 255,   0, SDL_ALPHA_OPAQUE },
    { 211, 255,   0, SDL_ALPHA_OPAQUE },
    { 205, 255,   0, SDL_ALPHA_OPAQUE },
    { 199, 255,   0, SDL_ALPHA_OPAQUE },
    { 193, 255,   0, SDL_ALPHA_OPAQUE },
    { 187, 255,   0, SDL_ALPHA_OPAQUE },
    { 181, 255,   0, SDL_ALPHA_OPAQUE },
    { 175, 255,   0, SDL_ALPHA_OPAQUE },
    { 169, 255,   0, SDL_ALPHA_OPAQUE },
    { 163, 255,   0, SDL_ALPHA_OPAQUE },
    { 157, 255,   0, SDL_ALPHA_OPAQUE },
    { 151, 255,   0, SDL_ALPHA_OPAQUE },
    { 145, 255,   0, SDL_ALPHA_OPAQUE },
    { 139, 255,   0, SDL_ALPHA_OPAQUE },
    { 133, 255,   0, SDL_ALPHA_OPAQUE },
    { 127, 255,   0, SDL_ALPHA_OPAQUE },
    { 121, 255,   0, SDL_ALPHA_OPAQUE },
    { 115, 255,   0, SDL_ALPHA_OPAQUE },
    { 109, 255,   0, SDL_ALPHA_OPAQUE },
    { 103, 255,   0, SDL_ALPHA_OPAQUE },
    {  97, 255,   0, SDL_ALPHA_OPAQUE },
    {  91, 255,   0, SDL_ALPHA_OPAQUE },
    {  85, 255,   0, SDL_ALPHA_OPAQUE },
    {  79, 255,   0, SDL_ALPHA_OPAQUE },
    {  73, 255,   0, SDL_ALPHA_OPAQUE },
    {  67, 255,   0, SDL_ALPHA_OPAQUE },
    {  61, 255,   0, SDL_ALPHA_OPAQUE },
    {  55, 255,   0, SDL_ALPHA_OPAQUE },
    {  49, 255,   0, SDL_ALPHA_OPAQUE },
    {  43, 255,   0, SDL_ALPHA_OPAQUE },
    {  37, 255,   0, SDL_ALPHA_OPAQUE },
    {  31, 255,   0, SDL_ALPHA_OPAQUE },
    {  25, 255,   0, SDL_ALPHA_OPAQUE },
    {  19, 255,   0, SDL_ALPHA_OPAQUE },
    {  13, 255,   0, SDL_ALPHA_OPAQUE },
    {   7, 255,   0, SDL_ALPHA_OPAQUE },
    {   1, 255,   0, SDL_ALPHA_OPAQUE },
    {   0, 255,   3, SDL_ALPHA_OPAQUE },
    {   0, 255,   9, SDL_ALPHA_OPAQUE },
    {   0, 255,  15, SDL_ALPHA_OPAQUE },
    {   0, 255,  21, SDL_ALPHA_OPAQUE },
    {   0, 255,  27, SDL_ALPHA_OPAQUE },
    {   0, 255,  33, SDL_ALPHA_OPAQUE },
    {   0, 255,  39, SDL_ALPHA_OPAQUE },
    {   0, 255,  45, SDL_ALPHA_OPAQUE },
    {   0, 255,  51, SDL_ALPHA_OPAQUE },
    {   0, 255,  57, SDL_ALPHA_OPAQUE },
    {   0, 255,  63, SDL_ALPHA_OPAQUE },
    {   0, 255,  69, SDL_ALPHA_OPAQUE },
    {   0, 255,  75, SDL_ALPHA_OPAQUE },
    {   0, 255,  81, SDL_ALPHA_OPAQUE },
    {   0, 255,  87, SDL_ALPHA_OPAQUE },
    {   0, 255,  93, SDL_ALPHA_OPAQUE },
    {   0, 255,  99, SDL_ALPHA_OPAQUE },
    {   0, 255, 105, SDL_ALPHA_OPAQUE },
    {   0, 255, 111, SDL_ALPHA_OPAQUE },
    {   0, 255, 117, SDL_ALPHA_OPAQUE },
    {   0, 255, 123, SDL_ALPHA_OPAQUE },
    {   0, 255, 129, SDL_ALPHA_OPAQUE },
    {   0, 255, 135, SDL_ALPHA_OPAQUE },
    {   0, 255, 141, SDL_ALPHA_OPAQUE },
    {   0, 255, 147, SDL_ALPHA_OPAQUE },
    {   0, 255, 153, SDL_ALPHA_OPAQUE },
    {   0, 255, 159, SDL_ALPHA_OPAQUE },
    {   0, 255, 165, SDL_ALPHA_OPAQUE },
    {   0, 255, 171, SDL_ALPHA_OPAQUE },
    {   0, 255, 177, SDL_ALPHA_OPAQUE },
    {   0, 255, 183, SDL_ALPHA_OPAQUE },
    {   0, 255, 189, SDL_ALPHA_OPAQUE },
    {   0, 255, 195, SDL_ALPHA_OPAQUE },
    {   0, 255, 201, SDL_ALPHA_OPAQUE },
    {   0, 255, 207, SDL_ALPHA_OPAQUE },
    {   0, 255, 213, SDL_ALPHA_OPAQUE },
    {   0, 255, 219, SDL_ALPHA_OPAQUE },
    {   0, 255, 225, SDL_ALPHA_OPAQUE },
    {   0, 255, 231, SDL_ALPHA_OPAQUE },
    {   0, 255, 237, SDL_ALPHA_OPAQUE },
    {   0, 255, 243, SDL_ALPHA_OPAQUE },
    {   0, 255, 249, SDL_ALPHA_OPAQUE },
    {   0, 255, 255, SDL_ALPHA_OPAQUE },
    {   0, 249, 255, SDL_ALPHA_OPAQUE },
    {   0, 243, 255, SDL_ALPHA_OPAQUE },
    {   0, 237, 255, SDL_ALPHA_OPAQUE },
    {   0, 231, 255, SDL_ALPHA_OPAQUE },
    {   0, 225, 255, SDL_ALPHA_OPAQUE },
    {   0, 219, 255, SDL_ALPHA_OPAQUE },
    {   0, 213, 255, SDL_ALPHA_OPAQUE },
    {   0, 207, 255, SDL_ALPHA_OPAQUE },
    {   0, 201, 255, SDL_ALPHA_OPAQUE },
    {   0, 195, 255, SDL_ALPHA_OPAQUE },
    {   0, 189, 255, SDL_ALPHA_OPAQUE },
    {   0, 183, 255, SDL_ALPHA_OPAQUE },
    {   0, 177, 255, SDL_ALPHA_OPAQUE },
    {   0, 171, 255, SDL_ALPHA_OPAQUE },
    {   0, 165, 255, SDL_ALPHA_OPAQUE },
    {   0, 159, 255, SDL_ALPHA_OPAQUE },
    {   0, 153, 255, SDL_ALPHA_OPAQUE },
    {   0, 147, 255, SDL_ALPHA_OPAQUE },
    {   0, 141, 255, SDL_ALPHA_OPAQUE },
    {   0, 135, 255, SDL_ALPHA_OPAQUE },
    {   0, 129, 255, SDL_ALPHA_OPAQUE },
    {   0, 123, 255, SDL_ALPHA_OPAQUE },
    {   0, 117, 255, SDL_ALPHA_OPAQUE },
    {   0, 111, 255, SDL_ALPHA_OPAQUE },
    {   0, 105, 255, SDL_ALPHA_OPAQUE },
    {   0,  99, 255, SDL_ALPHA_OPAQUE },
    {   0,  93, 255, SDL_ALPHA_OPAQUE },
    {   0,  87, 255, SDL_ALPHA_OPAQUE },
    {   0,  81, 255, SDL_ALPHA_OPAQUE },
    {   0,  75, 255, SDL_ALPHA_OPAQUE },
    {   0,  69, 255, SDL_ALPHA_OPAQUE },
    {   0,  63, 255, SDL_ALPHA_OPAQUE },
    {   0,  57, 255, SDL_ALPHA_OPAQUE },
    {   0,  51, 255, SDL_ALPHA_OPAQUE },
    {   0,  45, 255, SDL_ALPHA_OPAQUE },
    {   0,  39, 255, SDL_ALPHA_OPAQUE },
    {   0,  33, 255, SDL_ALPHA_OPAQUE },
    {   0,  27, 255, SDL_ALPHA_OPAQUE },
    {   0,  21, 255, SDL_ALPHA_OPAQUE },
    {   0,  15, 255, SDL_ALPHA_OPAQUE },
    {   0,   9, 255, SDL_ALPHA_OPAQUE },
    {   0,   3, 255, SDL_ALPHA_OPAQUE },
    {   1,   0, 255, SDL_ALPHA_OPAQUE },
    {   7,   0, 255, SDL_ALPHA_OPAQUE },
    {  13,   0, 255, SDL_ALPHA_OPAQUE },
    {  19,   0, 255, SDL_ALPHA_OPAQUE },
    {  25,   0, 255, SDL_ALPHA_OPAQUE },
    {  31,   0, 255, SDL_ALPHA_OPAQUE },
    {  37,   0, 255, SDL_ALPHA_OPAQUE },
    {  43,   0, 255, SDL_ALPHA_OPAQUE },
    {  49,   0, 255, SDL_ALPHA_OPAQUE },
    {  55,   0, 255, SDL_ALPHA_OPAQUE },
    {  61,   0, 255, SDL_ALPHA_OPAQUE },
    {  67,   0, 255, SDL_ALPHA_OPAQUE },
    {  73,   0, 255, SDL_ALPHA_OPAQUE },
    {  79,   0, 255, SDL_ALPHA_OPAQUE },
    {  85,   0, 255, SDL_ALPHA_OPAQUE },
    {  91,   0, 255, SDL_ALPHA_OPAQUE },
    {  97,   0, 255, SDL_ALPHA_OPAQUE },
    { 103,   0, 255, SDL_ALPHA_OPAQUE },
    { 109,   0, 255, SDL_ALPHA_OPAQUE },
    { 115,   0, 255, SDL_ALPHA_OPAQUE },
    { 121,   0, 255, SDL_ALPHA_OPAQUE },
    { 127,   0, 255, SDL_ALPHA_OPAQUE },
    { 133,   0, 255, SDL_ALPHA_OPAQUE },
    { 139,   0, 255, SDL_ALPHA_OPAQUE },
    { 145,   0, 255, SDL_ALPHA_OPAQUE },
    { 151,   0, 255, SDL_ALPHA_OPAQUE },
    { 157,   0, 255, SDL_ALPHA_OPAQUE },
    { 163,   0, 255, SDL_ALPHA_OPAQUE },
    { 169,   0, 255, SDL_ALPHA_OPAQUE },
    { 175,   0, 255, SDL_ALPHA_OPAQUE },
    { 181,   0, 255, SDL_ALPHA_OPAQUE },
    { 187,   0, 255, SDL_ALPHA_OPAQUE },
    { 193,   0, 255, SDL_ALPHA_OPAQUE },
    { 199,   0, 255, SDL_ALPHA_OPAQUE },
    { 205,   0, 255, SDL_ALPHA_OPAQUE },
    { 211,   0, 255, SDL_ALPHA_OPAQUE },
    { 217,   0, 255, SDL_ALPHA_OPAQUE },
    { 223,   0, 255, SDL_ALPHA_OPAQUE },
    { 229,   0, 255, SDL_ALPHA_OPAQUE },
    { 235,   0, 255, SDL_ALPHA_OPAQUE },
    { 241,   0, 255, SDL_ALPHA_OPAQUE },
    { 247,   0, 255, SDL_ALPHA_OPAQUE },
    { 253,   0, 255, SDL_ALPHA_OPAQUE },
    { 255,   0, 251, SDL_ALPHA_OPAQUE },
    { 255,   0, 245, SDL_ALPHA_OPAQUE },
    { 255,   0, 239, SDL_ALPHA_OPAQUE },
    { 255,   0, 233, SDL_ALPHA_OPAQUE },
    { 255,   0, 227, SDL_ALPHA_OPAQUE },
    { 255,   0, 221, SDL_ALPHA_OPAQUE },
    { 255,   0, 215, SDL_ALPHA_OPAQUE },
    { 255,   0, 209, SDL_ALPHA_OPAQUE },
    { 255,   0, 203, SDL_ALPHA_OPAQUE },
    { 255,   0, 197, SDL_ALPHA_OPAQUE },
    { 255,   0, 191, SDL_ALPHA_OPAQUE },
    { 255,   0, 185, SDL_ALPHA_OPAQUE },
    { 255,   0, 179, SDL_ALPHA_OPAQUE },
    { 255,   0, 173, SDL_ALPHA_OPAQUE },
    { 255,   0, 167, SDL_ALPHA_OPAQUE },
    { 255,   0, 161, SDL_ALPHA_OPAQUE },
    { 255,   0, 155, SDL_ALPHA_OPAQUE },
    { 255,   0, 149, SDL_ALPHA_OPAQUE },
    { 255,   0, 143, SDL_ALPHA_OPAQUE },
    { 255,   0, 137, SDL_ALPHA_OPAQUE },
    { 255,   0, 131, SDL_ALPHA_OPAQUE },
    { 255,   0, 125, SDL_ALPHA_OPAQUE },
    { 255,   0, 119, SDL_ALPHA_OPAQUE },
    { 255,   0, 113, SDL_ALPHA_OPAQUE },
    { 255,   0, 107, SDL_ALPHA_OPAQUE },
    { 255,   0, 101, SDL_ALPHA_OPAQUE },
    { 255,   0,  95, SDL_ALPHA_OPAQUE },
    { 255,   0,  89, SDL_ALPHA_OPAQUE },
    { 255,   0,  83, SDL_ALPHA_OPAQUE },
    { 255,   0,  77, SDL_ALPHA_OPAQUE },
    { 255,   0,  71, SDL_ALPHA_OPAQUE },
    { 255,   0,  65, SDL_ALPHA_OPAQUE },
    { 255,   0,  59, SDL_ALPHA_OPAQUE },
    { 255,   0,  53, SDL_ALPHA_OPAQUE },
    { 255,   0,  47, SDL_ALPHA_OPAQUE },
    { 255,   0,  41, SDL_ALPHA_OPAQUE },
    { 255,   0,  35, SDL_ALPHA_OPAQUE },
    { 255,   0,  29, SDL_ALPHA_OPAQUE },
    { 255,   0,  23, SDL_ALPHA_OPAQUE },
    { 255,   0,  17, SDL_ALPHA_OPAQUE },
    { 255,   0,  11, SDL_ALPHA_OPAQUE },
    { 255,   0,   5, SDL_ALPHA_OPAQUE }
};

static SDL_Renderer *renderer;
static SDL_Palette *palette;
static SDL_Texture *texture;
static SDL_Texture *black_texture1;
static SDL_Texture *black_texture2;
static SDL_Texture *white_texture1;
static SDL_Texture *white_texture2;
static int palettePos = 0;
static int paletteDir = -1;
static bool done;

static SDL_Texture *CreateTexture(const void *pixels, int pitch)
{
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STATIC, 256, 1);
    if (!tex) {
        return NULL;
    }
    SDL_UpdateTexture(tex, NULL, pixels, pitch);
    SDL_SetTexturePalette(tex, palette);
    return tex;
}

static bool CreateTextures()
{
    Uint8 data[256];
    int i;

    palette = SDL_CreatePalette(256);
    if (!palette) {
        return false;
    }

    for (i = 0; i < SDL_arraysize(data); i++) {
        data[i] = i;
    }

    texture = CreateTexture(data, SDL_arraysize(data));
    if (!texture) {
        return false;
    }

    black_texture1 = CreateTexture(data, SDL_arraysize(data));
    if (!black_texture1) {
        return false;
    }
    SDL_SetTextureScaleMode(black_texture1, SDL_SCALEMODE_NEAREST);

    black_texture2 = CreateTexture(data, SDL_arraysize(data));
    if (!black_texture2) {
        return false;
    }
    SDL_SetTextureScaleMode(black_texture2, SDL_SCALEMODE_NEAREST);

    white_texture1 = CreateTexture(data, SDL_arraysize(data));
    if (!white_texture1) {
        return false;
    }
    SDL_SetTextureScaleMode(white_texture1, SDL_SCALEMODE_NEAREST);

    white_texture2 = CreateTexture(data, SDL_arraysize(data));
    if (!white_texture2) {
        return false;
    }
    SDL_SetTextureScaleMode(white_texture2, SDL_SCALEMODE_NEAREST);

    return true;
}

static void UpdatePalette(int pos)
{
    int paletteSize = (int)SDL_arraysize(Palette);

    if (pos == 0) {
        SDL_SetPaletteColors(palette, Palette, 0, paletteSize);
    } else {
        SDL_SetPaletteColors(palette, Palette + pos, 0, paletteSize - pos);
        SDL_SetPaletteColors(palette, Palette, paletteSize - pos, pos);
    }
}

static void loop(void)
{
    SDL_Event event;
    SDL_FRect src = { 1.0f, 0.0f, 1.0f, 1.0f };
    SDL_FRect dst1 = { 0.0f, 0.0f, 32.0f, 32.0f };
    SDL_FRect dst2 = { 0.0f, WINDOW_HEIGHT - 32.0f, 32.0f, 32.0f };
    SDL_FRect dst3 = { WINDOW_WIDTH - 32.0f, 0.0f, 32.0f, 32.0f };
    SDL_FRect dst4 = { WINDOW_WIDTH - 32.0f, WINDOW_HEIGHT - 32.0f, 32.0f, 32.0f };
    SDL_FRect dst5 = { 0.0f, 32.0f + 2.0f, 32.0f, 32.0f };
    SDL_FRect dst6 = { WINDOW_WIDTH - 32.0f, 32.0f + 2.0f, 32.0f, 32.0f };
    const SDL_Color black = { 0, 0, 0, SDL_ALPHA_OPAQUE };
    const SDL_Color white = { 255, 255, 255, SDL_ALPHA_OPAQUE };
    const SDL_Color red = { 255, 0, 0, SDL_ALPHA_OPAQUE };
    const SDL_Color blue = { 0, 0, 255, SDL_ALPHA_OPAQUE };

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
            case SDLK_LEFT:
                paletteDir = 1;
                break;
            case SDLK_RIGHT:
                paletteDir = -1;
                break;
            case SDLK_ESCAPE:
                done = true;
                break;
            default:
                break;
            }
            break;
        case SDL_EVENT_QUIT:
            done = true;
            break;
        default:
            break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    /* Draw the rainbow texture */
    UpdatePalette(palettePos);
    palettePos += paletteDir;
    if (palettePos < 0) {
        palettePos = SDL_arraysize(Palette) - 1;
    } else if (palettePos >= SDL_arraysize(Palette)) {
        palettePos = 0;
    }
    SDL_RenderTexture(renderer, texture, NULL, NULL);

    /* Draw one square with black, and one square with white
     * This tests changing palette colors within a single frame
     */
    SDL_SetPaletteColors(palette, &black, 1, 1);
    SDL_SetRenderDrawColor(renderer, black.r, black.g, black.b, black.a);
    SDL_RenderDebugText(renderer, dst1.x + 32.0f + 2.0f, dst1.y + 12, "Black");
    SDL_RenderTexture(renderer, black_texture1, &src, &dst1);
    SDL_RenderDebugText(renderer, dst2.x + 32.0f + 2.0f, dst2.y + 12, "Black");
    SDL_RenderTexture(renderer, black_texture2, &src, &dst2);
    SDL_SetPaletteColors(palette, &white, 1, 1);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, white.a);
    SDL_RenderDebugText(renderer, dst3.x - 40.0f - 2.0f, dst3.y + 12, "White");
    SDL_RenderTexture(renderer, white_texture1, &src, &dst3);
    SDL_RenderDebugText(renderer, dst4.x - 40.0f - 2.0f, dst4.y + 12, "White");
    SDL_RenderTexture(renderer, white_texture2, &src, &dst4);

    /* Draw the same textures again with different colors */
    SDL_SetPaletteColors(palette, &red, 1, 1);
    SDL_SetRenderDrawColor(renderer, red.r, red.g, red.b, red.a);
    SDL_RenderDebugText(renderer, dst5.x + 32.0f + 2.0f, dst5.y + 12, "Red");
    SDL_RenderTexture(renderer, black_texture1, &src, &dst5);
    SDL_SetPaletteColors(palette, &blue, 1, 1);
    SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, blue.a);
    SDL_RenderDebugText(renderer, dst6.x - 40.0f - 2.0f, dst6.y + 12, "Blue");
    SDL_RenderTexture(renderer, white_texture1, &src, &dst6);

    SDL_RenderPresent(renderer);
    SDL_Delay(10);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    int i, return_code = -1;
    SDLTest_CommonState *state;

    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        goto quit;
    }

    for (i = 1; i < argc; ) {
        int consumed = SDLTest_CommonArg(state, i);
        SDL_Log("consumed=%d", consumed);
        if (consumed == 0) {
            if (SDL_strcmp(argv[i], "--renderer") == 0 && argv[i + 1]) {
                SDL_SetHint(SDL_HINT_RENDER_DRIVER, argv[i + 1]);
                consumed = 2;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--renderer RENDERER]",
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return_code = 1;
            goto quit;
        }
        i += consumed;
    }

    if (!SDL_CreateWindowAndRenderer("testpalette", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    if (!CreateTextures()) {
        SDL_Log("Couldn't create textures: %s", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Main render loop */
    done = false;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    return_code = 0;
quit:
    SDL_DestroyPalette(palette);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return return_code;
}
