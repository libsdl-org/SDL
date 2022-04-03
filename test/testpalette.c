/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "SDL.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef __IPHONEOS__
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480
#else
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#endif

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

static SDL_Texture *CreateTexture(SDL_Renderer *renderer)
{
    SDL_Texture *texture;
    Uint8 data[256];
    int i;

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STATIC, 256, 1);
    if (!texture)
        return NULL;

    for (i = 0; i < SDL_arraysize(data); i++) {
        data[i] = i;
    }
    SDL_UpdateTexture(texture, NULL, data, SDL_arraysize(data));

    return texture;
}

static void UpdatePalette(SDL_Texture *texture, int pos)
{
    size_t paletteSize = SDL_arraysize(Palette);

    if (pos == 0) {
        SDL_SetTexturePalette(texture, Palette, 0, paletteSize);
    } else {
        SDL_SetTexturePalette(texture, Palette + pos, 0, paletteSize - pos);
        SDL_SetTexturePalette(texture, Palette, paletteSize - pos, pos);
    }
}

static SDL_Texture *texture = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;

static SDL_bool done = SDL_FALSE;
static int palettePos = 0;
static int paletteDir = -1;
static Uint64 lastTicks = 0;

void loop(void *arg)
{
    SDL_Event event;
    Uint64 ticks;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_LEFT:
                paletteDir = 1;
                break;

            case SDLK_RIGHT:
                paletteDir = -1;
                break;
            }
            break;

        case SDL_QUIT:
            done = SDL_TRUE;
            break;

        default:
            break;
        }
    }

    ticks = SDL_GetTicks64();
    if (ticks > lastTicks + 10) {
        UpdatePalette(texture, palettePos);
        palettePos += paletteDir;

        if (palettePos < 0)
            palettePos = SDL_arraysize(Palette) - 1;
        else if (palettePos >= SDL_arraysize(Palette))
            palettePos = 0;

        lastTicks = ticks;
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("Palette Test", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return 1;
    }

    texture = CreateTexture(renderer);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return 1;
    }

    /* Main render loop */
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop, NULL, 0, 1);
#else
    while (!done) {
        loop(NULL);
    }
#endif

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
