/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

#define IMAGE_SIZE 256

static SDLTest_CommonState *state;
static SDL_Surface *image;
static SDL_Texture *texture;
static int format_index = -1;
static SDL_PixelFormat formats[] = {
    SDL_PIXELFORMAT_RGBA32,
    SDL_PIXELFORMAT_ARGB32,
    SDL_PIXELFORMAT_RGBX32,
    SDL_PIXELFORMAT_XRGB32,
    SDL_PIXELFORMAT_ARGB1555,
    SDL_PIXELFORMAT_INDEX8
};
static int angle;
static int direction = 1;

static bool UpdateImageFormat(void)
{
    static const SDL_Color colors[] = {
        { 255, 255, 255, SDL_ALPHA_TRANSPARENT },   /* Colorkey - white with transparent alpha */
        { 255,   0,   0, SDL_ALPHA_OPAQUE },        /* Red */
        { 255, 255,   0, SDL_ALPHA_OPAQUE },        /* Yellow */
        {   0, 255,   0, SDL_ALPHA_OPAQUE },        /* Green */
        {   0,   0, 255, SDL_ALPHA_OPAQUE },        /* Blue */
    };
    SDL_Rect rect;
    Uint32 color;

    ++format_index;
    if (format_index == SDL_arraysize(formats)) {
        format_index = 0;
    }

    SDL_DestroySurface(image);

    image = SDL_CreateSurface(IMAGE_SIZE, IMAGE_SIZE, formats[format_index]);
    if (!image) {
        SDL_Log("Couldn't create surface: %s\n", SDL_GetError());
        return false;
    }

    if (image->format == SDL_PIXELFORMAT_INDEX8) {
        /* Set the palette and colorkey */
        SDL_Palette *palette = SDL_CreateSurfacePalette(image);

        SDL_SetPaletteColors(palette, colors, 0, SDL_arraysize(colors));
        SDL_SetSurfaceColorKey(image, true, 0);
    }

    /* Upper left */
    rect.x = 0;
    rect.y = 0;
    rect.w = IMAGE_SIZE / 2;
    rect.h = IMAGE_SIZE / 2;
    color = SDL_MapSurfaceRGB(image, colors[1].r, colors[1].g, colors[1].b);
    SDL_FillSurfaceRect(image, &rect, color);

    /* Upper right */
    rect.x += rect.w;
    color = SDL_MapSurfaceRGB(image, colors[2].r, colors[2].g, colors[2].b);
    SDL_FillSurfaceRect(image, &rect, color);

    /* Lower left */
    rect.x = 0;
    rect.y += rect.h;
    color = SDL_MapSurfaceRGB(image, colors[3].r, colors[3].g, colors[3].b);
    SDL_FillSurfaceRect(image, &rect, color);

    /* Lower right */
    rect.x += rect.w;
    color = SDL_MapSurfaceRGB(image, colors[4].r, colors[4].g, colors[4].b);
    SDL_FillSurfaceRect(image, &rect, color);

    return true;
}

static bool UpdateRotation(SDL_Renderer *renderer)
{
    SDL_Surface *rotated;

    angle += direction;

    rotated = SDL_RotateSurface(image, (float)angle);
    if (!rotated) {
        SDL_Log("Couldn't rotate surface: %s", SDL_GetError());
        return false;
    }

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, rotated);
    SDL_DestroySurface(rotated);
    if (!texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return false;
    }

    return true;
}

static void Draw(SDL_Renderer *renderer)
{
    int w, h;
    SDL_FRect dst;

    /* Clear the screen */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    UpdateRotation(renderer);

    /* Draw the rotated image */
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    dst.x = (w - texture->w) / 2.0f;
    dst.y = (h - texture->h) / 2.0f;
    dst.w = (float)texture->w;
    dst.h = (float)texture->h;
    SDL_RenderTexture(renderer, texture, NULL, &dst);

    /* Show the current format */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugTextFormat(renderer, 4.0f, 4.0f, "Format: %s, press SPACE to cycle", SDL_GetPixelFormatName(formats[format_index]));

    /* All done! */
    SDL_RenderPresent(renderer);
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDLTest_CommonQuit(state);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    if (!SDLTest_CommonInit(state)) {
        return SDL_APP_FAILURE;
    }

    /* Create the spinning image */
    UpdateImageFormat();

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_UP:
        switch (event->key.key) {
        case SDLK_SPACE:
            UpdateImageFormat();
            break;
        case SDLK_LEFT:
            direction = -1;
            break;
        case SDLK_RIGHT:
            direction = 1;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return SDLTest_CommonEventMainCallbacks(state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        Draw(state->renderers[i]);
    }

    /* Wait a bit so we don't spin too quickly to see */
    SDL_Delay(10);

    return SDL_APP_CONTINUE;
}
