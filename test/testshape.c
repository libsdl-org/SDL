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
#include <SDL3/SDL_test.h>

#define SHAPED_WINDOW_DIMENSION 640

/** An enum denoting the specific type of contents present in an SDL_WindowShapeParams union. */
typedef enum
{
    /** The default mode, a binarized alpha cutoff of 1. */
    ShapeModeDefault,
    /** A binarized alpha cutoff with a given integer value. */
    ShapeModeBinarizeAlpha,
    /** A binarized alpha cutoff with a given integer value, but with the opposite comparison. */
    ShapeModeReverseBinarizeAlpha,
    /** A color key is applied. */
    ShapeModeColorKey
} WindowShapeMode;

/** A union containing parameters for shaped windows. */
typedef union
{
    /** A cutoff alpha value for binarization of the window shape's alpha channel. */
    Uint8 binarizationCutoff;
    SDL_Color colorKey;
} SDL_WindowShapeParams;

/** A struct that tags the SDL_WindowShapeParams union with an enum describing the type of its contents. */
typedef struct SDL_WindowShapeMode
{
    /** The mode of these window-shape parameters. */
    WindowShapeMode mode;
    /** Window-shape parameters. */
    SDL_WindowShapeParams parameters;
} SDL_WindowShapeMode;

typedef struct LoadedPicture
{
    SDL_Surface *surface;
    int num_textures;
    SDL_Texture **textures;
    SDL_Texture **shape_textures;
    SDL_WindowShapeMode mode;
    const char *name;
    struct LoadedPicture *next;
} LoadedPicture;

static Uint8 *g_bitmap = NULL;
static int g_bitmap_w = 0, g_bitmap_h = 0;
static SDL_Surface *g_shape_surface = NULL;

static void log_usage(SDLTest_CommonState *state, char *progname) {
    static const char *options[] = { "sample1.bmp [sample2.bmp [sample3.bmp ...]]", NULL };
    SDLTest_CommonLogUsage(state, progname, options);
}

/* REQUIRES that bitmap point to a w-by-h bitmap with ppb pixels-per-byte. */
static void SDL_CalculateShapeBitmap(SDL_WindowShapeMode mode, SDL_Surface *shape, Uint8 *bitmap, Uint8 ppb)
{
    int x = 0;
    int y = 0;
    Uint8 r = 0, g = 0, b = 0, alpha = 0;
    Uint32 mask_value = 0;
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
            if (SDLTest_ReadSurfacePixel(shape, x, y, &r, &g, &b, &alpha) != 0) {
                continue;
            }

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

static int SDL3_SetWindowShape(LoadedPicture *picture, SDL_WindowShapeMode *shape_mode)
{
    if (g_bitmap) {
        SDL_free(g_bitmap);
        g_bitmap = NULL;
    }

    if (!picture) {
        return SDL_SetError("picture");
    }

    if (picture->shape_textures[0]) {
        int i;
        for (i = 0; i < picture->num_textures; i++) {
            SDL_DestroyTexture(picture->shape_textures[i]);
            picture->shape_textures[i] = NULL;
        }
    }

    if (g_shape_surface) {
        SDL_DestroySurface(g_shape_surface);
        g_shape_surface = NULL;
    }

    if (!shape_mode) {
        return SDL_SetError("shape_mode");
    }

    g_bitmap_w = picture->surface->w;
    g_bitmap_h = picture->surface->h;
    g_bitmap = (Uint8*) SDL_malloc(picture->surface->w * picture->surface->h);
    if (!g_bitmap) {
        return -1;
    }

    SDL_CalculateShapeBitmap(*shape_mode, picture->surface, g_bitmap, 1);

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

static void render(int index, SDL_Renderer *renderer, LoadedPicture *picture)
{
    /* Clear render-target to blue. */
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, 0xff);
    SDL_RenderClear(renderer);

    /* Render the texture. */
    SDL_RenderTexture(renderer, picture->textures[index], NULL, NULL);

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
            if (!picture->shape_textures[index]) {
                SDL_BlendMode bm;

                picture->shape_textures[index] = SDL_CreateTextureFromSurface(renderer, g_shape_surface);

                /* if Alpha is 0, set all to 0, else leave unchanged. */
                bm = SDL_ComposeCustomBlendMode(
                        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);

                SDL_SetTextureBlendMode(picture->shape_textures[index], bm);
            }
            SDL_RenderTexture(renderer, picture->shape_textures[index], NULL, NULL);
        }
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    int num_pictures = 0;
    LoadedPicture *pic_i = NULL;
    LoadedPicture *picture_linked_list = NULL;
    LoadedPicture **pictures = NULL;
    int i;
    int j;
    const SDL_DisplayMode *mode;
    SDL_PixelFormat *format = NULL;
    SDL_Color black = { 0, 0, 0, 0xff };
    int done = 0;
    int current_picture;
    int button_down;
    Uint32 pixelFormat = 0;
    int w, h, access = 0;
    SDLTest_CommonState *state;
    int rc;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    state->window_flags |= SDL_WINDOW_TRANSPARENT;
    state->window_flags &= ~SDL_WINDOW_RESIZABLE;

    rc = 0;

    /* SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software"); */
    /* SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "0"); */

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            LoadedPicture *new_picture;

            new_picture = SDL_malloc(sizeof(LoadedPicture));
            new_picture->name = argv[i];
            new_picture->next = picture_linked_list;
            picture_linked_list = new_picture;
            num_pictures++;
            consumed = 1;
        }
        if (consumed <= 0) {
            log_usage(state, argv[0]);
            exit(-1);
        }

        i += consumed;
    }
    if (!num_pictures) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "testshape requires at least one bitmap file as argument.");
        log_usage(state, argv[0]);
        rc = -1;
        goto ret;
    }

    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL video.");
        rc = -2;
        goto ret;
    }

    mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get desktop display mode: %s", SDL_GetError());
        rc = -2;
        goto ret;
    }

    /* Allocate an array of LoadedPicture pointers for convenience accesses. */
    pictures = (LoadedPicture **)SDL_malloc(sizeof(LoadedPicture*) * num_pictures);
    if (!pictures) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not allocate memory.");
        rc = 1;
        goto ret;
    }
    for (i = 0, pic_i = picture_linked_list; i < num_pictures; i++, pic_i = pic_i->next) {
        pictures[i] = pic_i;
        pictures[i]->surface = NULL;
    }
    for (i = 0; i < num_pictures; i++) {
        pictures[i]->surface = SDL_LoadBMP(pictures[i]->name);
        if (pictures[i]->surface == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load surface from named bitmap file: %s", pictures[i]->name);
            rc = -3;
            goto ret;
        }

        format = pictures[i]->surface->format;
        if (SDL_ISPIXELFORMAT_ALPHA(format->format)) {
            pictures[i]->mode.mode = ShapeModeBinarizeAlpha;
            pictures[i]->mode.parameters.binarizationCutoff = 255;
        } else {
            pictures[i]->mode.mode = ShapeModeColorKey;
            pictures[i]->mode.parameters.colorKey = black;
        }
    }

    for (i = 0; i < num_pictures; i++) {
        pictures[i]->textures = SDL_calloc(state->num_windows, sizeof(SDL_Texture *));
        pictures[i]->shape_textures = SDL_calloc(state->num_windows, sizeof(SDL_Texture *));
        if (!pictures[i]->textures || !pictures[i]->shape_textures) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create textures array(s).");
            rc = -4;
            goto ret;
        }

        for (j = 0; j < state->num_windows; j++) {
            pictures[i]->textures[j] = SDL_CreateTextureFromSurface(state->renderers[j], pictures[i]->surface);
            if (!pictures[i]->textures[j]) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create textures for SDL_shape.");
                rc = -5;
                goto ret;
            }
        }
    }

    done = 0;
    current_picture = 0;
    button_down = 0;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Changing to shaped bmp: %s", pictures[current_picture]->name);
    for (i = 0; i < state->num_windows; i++) {
        SDL_QueryTexture(pictures[current_picture]->textures[i], &pixelFormat, &access, &w, &h);
        /* We want to set the window size in pixels */
        SDL_SetWindowSize(state->windows[i], (int)SDL_ceilf(w / mode->pixel_density), (int)SDL_ceilf(h / mode->pixel_density));
    }
    SDL3_SetWindowShape(pictures[current_picture], &pictures[current_picture]->mode);
    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);
            if (event.type == SDL_EVENT_KEY_DOWN) {
                button_down = 1;
            }
            if (button_down && event.type == SDL_EVENT_KEY_UP) {
                button_down = 0;
                current_picture += 1;
                if (current_picture >= num_pictures) {
                    current_picture = 0;
                }
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Changing to shaped bmp: %s", pictures[current_picture]->name);
                for (i = 0; i < state->num_windows; i++) {
                    SDL_QueryTexture(pictures[current_picture]->textures[i], &pixelFormat, &access, &w, &h);
                    SDL_SetWindowSize(state->windows[i], (int)SDL_ceilf(w / mode->pixel_density),(int)SDL_ceilf(h / mode->pixel_density));
                    SDL3_SetWindowShape(pictures[current_picture], &pictures[current_picture]->mode);
                }
            }
        }
        for (i = 0; i < state->num_windows; i++) {
            render(i, state->renderers[i], pictures[current_picture]);
        }
        SDL_Delay(10);
    }

ret:
    /* Free the textures + original surfaces backing the textures. */
    for (pic_i = picture_linked_list; pic_i; ) {
        LoadedPicture *next = pic_i->next;
        for (j = 0; j < state->num_windows; j++) {
            SDL_DestroyTexture(pic_i->textures[i]);
        }
        SDL_free(pic_i->textures);
        SDL_DestroySurface(pic_i->surface);
        SDL_free(pic_i);
        pic_i = next;
    }
    SDL_free(pictures);

    if (g_bitmap) {
        SDL_free(g_bitmap);
        g_bitmap = NULL;
    }
    if (g_shape_surface) {
        SDL_DestroySurface(g_shape_surface);
        g_shape_surface = NULL;
    }

    SDLTest_CommonQuit(state);

    return rc;
}
