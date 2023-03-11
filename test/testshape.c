/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define SHAPED_WINDOW_DIMENSION 640

typedef struct LoadedPicture
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_WindowShapeMode mode;
    const char *name;
} LoadedPicture;

static Uint8 *g_bitmap = NULL;
static int g_bitmap_w = 0, g_bitmap_h = 0;
static SDL_Surface *g_shape_surface = NULL;
static SDL_Texture *g_shape_texture = NULL;

/* REQUIRES that bitmap point to a w-by-h bitmap with ppb pixels-per-byte. */
static void SDL_CalculateShapeBitmap(SDL_WindowShapeMode mode, SDL_Surface *shape, Uint8 *bitmap, Uint8 ppb)
{
    int x = 0;
    int y = 0;
    Uint8 r = 0, g = 0, b = 0, alpha = 0;
    Uint8 *pixel = NULL;
    Uint32 pixel_value = 0, mask_value = 0;
    size_t bytes_per_scanline = (size_t)(shape->w + (ppb - 1)) / ppb;
    Uint8 *bitmap_scanline;
    SDL_Color key;

    if (SDL_MUSTLOCK(shape)) {
        SDL_LockSurface(shape);
    }

    SDL_memset(bitmap, 0, shape->h * bytes_per_scanline);

    for (y = 0; y < shape->h; y++) {
        bitmap_scanline = bitmap + y * bytes_per_scanline;
        for (x = 0; x < shape->w; x++) {
            alpha = 0;
            pixel_value = 0;
            pixel = (Uint8 *)(shape->pixels) + (y * shape->pitch) + (x * shape->format->BytesPerPixel);
            switch (shape->format->BytesPerPixel) {
            case (1):
                pixel_value = *pixel;
                break;
            case (2):
                pixel_value = *(Uint16 *)pixel;
                break;
            case (3):
                pixel_value = *(Uint32 *)pixel & (~shape->format->Amask);
                break;
            case (4):
                pixel_value = *(Uint32 *)pixel;
                break;
            }
            SDL_GetRGBA(pixel_value, shape->format, &r, &g, &b, &alpha);
            switch (mode.mode) {
            case (ShapeModeDefault):
                mask_value = (alpha >= 1 ? 1 : 0);
                break;
            case (ShapeModeBinarizeAlpha):
                mask_value = (alpha >= mode.parameters.binarizationCutoff ? 1 : 0);
                break;
            case (ShapeModeReverseBinarizeAlpha):
                mask_value = (alpha <= mode.parameters.binarizationCutoff ? 1 : 0);
                break;
            case (ShapeModeColorKey):
                key = mode.parameters.colorKey;
                mask_value = ((key.r != r || key.g != g || key.b != b) ? 1 : 0);
                break;
            }
            bitmap_scanline[x / ppb] |= mask_value << (x % ppb);
        }
    }

    if (SDL_MUSTLOCK(shape)) {
        SDL_UnlockSurface(shape);
    }
}

static int SDL3_SetWindowShape(SDL_Window *window, SDL_Surface *shape, SDL_WindowShapeMode *shape_mode)
{
    if (g_bitmap) {
        SDL_free(g_bitmap);
        g_bitmap = NULL;
    }

    if (g_shape_texture) {
        SDL_DestroyTexture(g_shape_texture);
        g_shape_texture = NULL;
    }

    if (g_shape_surface) {
        SDL_DestroySurface(g_shape_surface);
        g_shape_surface = NULL;
    }

    if (shape == NULL) {
        return SDL_SetError("shape");
    }

    if (shape_mode == NULL) {
        return SDL_SetError("shape_mode");
    }

    g_bitmap_w = shape->w;
    g_bitmap_h = shape->h;
    g_bitmap = (Uint8*) SDL_malloc(shape->w * shape->h);
    if (g_bitmap == NULL) {
        return SDL_OutOfMemory();
    }

    SDL_CalculateShapeBitmap(*shape_mode, shape, g_bitmap, 1);

    g_shape_surface = SDL_CreateSurface(g_bitmap_w, g_bitmap_h, SDL_PIXELFORMAT_ABGR8888);
    if (g_shape_surface) {
        int x, y, i = 0;
        Uint32 *ptr = g_shape_surface->pixels;
        for (y = 0; y < g_bitmap_h; y++) {
            for (x = 0; x < g_bitmap_w; x++) {
                Uint8 val = g_bitmap[i++];
                if (val == 0) {
                    ptr[x] = 0;
                } else {
                    ptr[x] = 0xffffffff;
                }
            }
            ptr = (Uint32 *)((Uint8 *)ptr + g_shape_surface->pitch);
        }
    }

    return 0;
}

static void render(SDL_Renderer *renderer, SDL_Texture *texture)
{
    /* Clear render-target to blue. */
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, 0xff);
    SDL_RenderClear(renderer);

    /* Render the texture. */
    SDL_RenderTexture(renderer, texture, NULL, NULL);

    /* Apply the shape */
    if (g_shape_surface) {
        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);

        if (info.flags & SDL_RENDERER_SOFTWARE) {
            if (g_bitmap) {
                int x, y, i = 0;
                Uint8 r, g, b, a;
                SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
                for (y = 0; y < g_bitmap_h; y++) {
                    for (x = 0; x < g_bitmap_w; x++) {
                        Uint8 val = g_bitmap[i++];
                        if (val == 0) {
                            SDL_RenderPoint(renderer, (float)x, (float)y);
                        }
                    }
                }
                SDL_SetRenderDrawColor(renderer, r, g, b, a);
            }
        } else {
            if (g_shape_texture == NULL) {
                SDL_BlendMode bm;

                g_shape_texture = SDL_CreateTextureFromSurface(renderer, g_shape_surface);

                /* if Alpha is 0, set all to 0, else leave unchanged. */
                bm = SDL_ComposeCustomBlendMode(
                        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);

                SDL_SetTextureBlendMode(g_shape_texture, bm);
            }
            SDL_RenderTexture(renderer, g_shape_texture, NULL, NULL);
        }
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    Uint8 num_pictures;
    LoadedPicture *pictures;
    int i, j;
    const SDL_DisplayMode *mode;
    SDL_PixelFormat *format = NULL;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Color black = { 0, 0, 0, 0xff };
    SDL_Event event;
    int should_exit = 0;
    unsigned int current_picture;
    int button_down;
    Uint32 pixelFormat = 0;
    int w, h, access = 0;

//    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
//    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "0");

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc < 2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Shape requires at least one bitmap file as argument.");
        exit(-1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL video.");
        exit(-2);
    }

    mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get desktop display mode: %s", SDL_GetError());
        exit(-2);
    }

    num_pictures = argc - 1;
    pictures = (LoadedPicture *)SDL_malloc(sizeof(LoadedPicture) * num_pictures);
    if (pictures == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not allocate memory.");
        exit(1);
    }
    for (i = 0; i < num_pictures; i++) {
        pictures[i].surface = NULL;
    }
    for (i = 0; i < num_pictures; i++) {
        pictures[i].surface = SDL_LoadBMP(argv[i + 1]);
        pictures[i].name = argv[i + 1];
        if (pictures[i].surface == NULL) {
            for (j = 0; j < num_pictures; j++) {
                SDL_DestroySurface(pictures[j].surface);
            }
            SDL_free(pictures);
            SDL_Quit();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load surface from named bitmap file: %s", argv[i + 1]);
            exit(-3);
        }

        format = pictures[i].surface->format;
        if (SDL_ISPIXELFORMAT_ALPHA(format->format)) {
            pictures[i].mode.mode = ShapeModeBinarizeAlpha;
            pictures[i].mode.parameters.binarizationCutoff = 255;
        } else {
            pictures[i].mode.mode = ShapeModeColorKey;
            pictures[i].mode.parameters.colorKey = black;
        }
    }

    window = SDL_CreateWindow("SDL_Shape test", SHAPED_WINDOW_DIMENSION, SHAPED_WINDOW_DIMENSION, SDL_WINDOW_TRANSPARENT);
    if (window == NULL) {
        for (i = 0; i < num_pictures; i++) {
            SDL_DestroySurface(pictures[i].surface);
        }
        SDL_free(pictures);
        SDL_Quit();
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create shaped window for SDL_Shape.");
        exit(-4);
    }
    renderer = SDL_CreateRenderer(window, NULL, 0);
    if (renderer == NULL) {
        SDL_DestroyWindow(window);
        for (i = 0; i < num_pictures; i++) {
            SDL_DestroySurface(pictures[i].surface);
        }
        SDL_free(pictures);
        SDL_Quit();
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create rendering context for SDL_Shape window.");
        exit(-5);
    }

    for (i = 0; i < num_pictures; i++) {
        pictures[i].texture = NULL;
    }
    for (i = 0; i < num_pictures; i++) {
        pictures[i].texture = SDL_CreateTextureFromSurface(renderer, pictures[i].surface);
        if (pictures[i].texture == NULL) {
            for (i = 0; i < num_pictures; i++) {
                if (pictures[i].texture != NULL) {
                    SDL_DestroyTexture(pictures[i].texture);
                }
            }
            for (i = 0; i < num_pictures; i++) {
                SDL_DestroySurface(pictures[i].surface);
            }
            SDL_free(pictures);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create texture for SDL_shape.");
            exit(-6);
        }
    }

    should_exit = 0;
    current_picture = 0;
    button_down = 0;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Changing to shaped bmp: %s", pictures[current_picture].name);
    SDL_QueryTexture(pictures[current_picture].texture, &pixelFormat, &access, &w, &h);
    /* We want to set the window size in pixels */
    SDL_SetWindowSize(window, (int)SDL_ceilf(w / mode->display_scale), (int)SDL_ceilf(h / mode->display_scale));
    SDL3_SetWindowShape(window, pictures[current_picture].surface, &pictures[current_picture].mode);
    while (should_exit == 0) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                button_down = 1;
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    should_exit = 1;
                    break;
                }
            }
            if (button_down && event.type == SDL_EVENT_KEY_UP) {
                button_down = 0;
                current_picture += 1;
                if (current_picture >= num_pictures) {
                    current_picture = 0;
                }
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Changing to shaped bmp: %s", pictures[current_picture].name);
                SDL_QueryTexture(pictures[current_picture].texture, &pixelFormat, &access, &w, &h);
                SDL_SetWindowSize(window, (int)SDL_ceilf(w / mode->display_scale), (int)SDL_ceilf(h / mode->display_scale));
                SDL3_SetWindowShape(window, pictures[current_picture].surface, &pictures[current_picture].mode);
            }
            if (event.type == SDL_EVENT_QUIT) {
                should_exit = 1;
                break;
            }
        }
        render(renderer, pictures[current_picture].texture);
        SDL_Delay(10);
    }

    /* Free the textures. */
    for (i = 0; i < num_pictures; i++) {
        SDL_DestroyTexture(pictures[i].texture);
    }
    SDL_DestroyRenderer(renderer);
    /* Destroy the window. */
    SDL_DestroyWindow(window);
    /* Free the original surfaces backing the textures. */
    for (i = 0; i < num_pictures; i++) {
        SDL_DestroySurface(pictures[i].surface);
    }
    SDL_free(pictures);
    /* Call SDL_Quit() before quitting. */
    SDL_Quit();

    return 0;
}
