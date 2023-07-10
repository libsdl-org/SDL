/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>

#include "gamepadutils.h"
#include "gamepad_front.h"
#include "gamepad_back.h"
#include "gamepad_button.h"
#include "gamepad_axis.h"

/* This is indexed by SDL_GamepadButton. */
static const struct
{
    int x;
    int y;
} button_positions[] = {
    { 412, 192 }, /* SDL_GAMEPAD_BUTTON_A */
    { 456, 157 }, /* SDL_GAMEPAD_BUTTON_B */
    { 367, 157 }, /* SDL_GAMEPAD_BUTTON_X */
    { 414, 126 }, /* SDL_GAMEPAD_BUTTON_Y */
    { 199, 157 }, /* SDL_GAMEPAD_BUTTON_BACK */
    { 257, 153 }, /* SDL_GAMEPAD_BUTTON_GUIDE */
    { 314, 157 }, /* SDL_GAMEPAD_BUTTON_START */
    { 100, 179 },  /* SDL_GAMEPAD_BUTTON_LEFT_STICK */
    { 330, 255 }, /* SDL_GAMEPAD_BUTTON_RIGHT_STICK */
    { 102, 65 },   /* SDL_GAMEPAD_BUTTON_LEFT_SHOULDER */
    { 421, 61 },  /* SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER */
    { 179, 213 }, /* SDL_GAMEPAD_BUTTON_DPAD_UP */
    { 179, 274 }, /* SDL_GAMEPAD_BUTTON_DPAD_DOWN */
    { 141, 242 }, /* SDL_GAMEPAD_BUTTON_DPAD_LEFT */
    { 211, 242 }, /* SDL_GAMEPAD_BUTTON_DPAD_RIGHT */
    { 257, 199 }, /* SDL_GAMEPAD_BUTTON_MISC1 */
    { 157, 160 }, /* SDL_GAMEPAD_BUTTON_PADDLE1 */
    { 355, 160 }, /* SDL_GAMEPAD_BUTTON_PADDLE2 */
    { 157, 200 }, /* SDL_GAMEPAD_BUTTON_PADDLE3 */
    { 355, 200 }, /* SDL_GAMEPAD_BUTTON_PADDLE4 */
};

/* This is indexed by SDL_GamepadAxis. */
static const struct
{
    int x;
    int y;
    double angle;
} axis_positions[] = {
    { 99, 178, 270.0 },  /* LEFTX */
    { 99, 178, 0.0 },    /* LEFTY */
    { 331, 256, 270.0 }, /* RIGHTX */
    { 331, 256, 0.0 },   /* RIGHTY */
    { 116, 5, 0.0 },    /* TRIGGERLEFT */
    { 400, 5, 0.0 },   /* TRIGGERRIGHT */
};

struct GamepadImage
{
    SDL_Renderer *renderer;
    SDL_Texture *front_texture;
    SDL_Texture *back_texture;
    SDL_Texture *button_texture;
    SDL_Texture *axis_texture;
    int gamepad_width;
    int gamepad_height;
    int button_width;
    int button_height;
    int axis_width;
    int axis_height;

    int x;
    int y;
    SDL_bool showing_front;

    SDL_bool buttons[SDL_GAMEPAD_BUTTON_MAX];
    int axes[SDL_GAMEPAD_AXIS_MAX];
};

static SDL_Texture *CreateTexture(SDL_Renderer *renderer, unsigned char *data, unsigned int len)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_RWops *src = SDL_RWFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_RW(src, SDL_TRUE);
        if (surface) {
            if (surface->format->palette) {
                SDL_SetSurfaceColorKey(surface, SDL_TRUE, *(Uint8 *)surface->pixels);
            } else {
                switch (surface->format->BitsPerPixel) {
                case 15:
                    SDL_SetSurfaceColorKey(surface, SDL_TRUE,
                                    (*(Uint16 *)surface->pixels) & 0x00007FFF);
                    break;
                case 16:
                    SDL_SetSurfaceColorKey(surface, SDL_TRUE, *(Uint16 *)surface->pixels);
                    break;
                case 24:
                    SDL_SetSurfaceColorKey(surface, SDL_TRUE,
                                    (*(Uint32 *)surface->pixels) & 0x00FFFFFF);
                    break;
                case 32:
                    SDL_SetSurfaceColorKey(surface, SDL_TRUE, *(Uint32 *)surface->pixels);
                    break;
                }
            }
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
        }
    }
    return texture;
}

GamepadImage *CreateGamepadImage(SDL_Renderer *renderer)
{
    GamepadImage *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;
        ctx->front_texture = CreateTexture(renderer, gamepad_front_bmp, gamepad_front_bmp_len);
        ctx->back_texture = CreateTexture(renderer, gamepad_back_bmp, gamepad_back_bmp_len);
        SDL_QueryTexture(ctx->front_texture, NULL, NULL, &ctx->gamepad_width, &ctx->gamepad_height);

        ctx->button_texture = CreateTexture(renderer, gamepad_button_bmp, gamepad_button_bmp_len);
        SDL_QueryTexture(ctx->button_texture, NULL, NULL, &ctx->button_width, &ctx->button_height);
        SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);

        ctx->axis_texture = CreateTexture(renderer, gamepad_axis_bmp, gamepad_axis_bmp_len);
        SDL_QueryTexture(ctx->axis_texture, NULL, NULL, &ctx->axis_width, &ctx->axis_height);
        SDL_SetTextureColorMod(ctx->axis_texture, 10, 255, 21);

        ctx->showing_front = SDL_TRUE;
    }
    return ctx;
}

void SetGamepadImagePosition(GamepadImage *ctx, int x, int y)
{
    if (!ctx) {
        return;
    }

    ctx->x = x;
    ctx->y = y;
}

void SetGamepadImageShowingFront(GamepadImage *ctx, SDL_bool showing_front)
{
    if (!ctx) {
        return;
    }

    ctx->showing_front = showing_front;
}

void GetGamepadImageArea(GamepadImage *ctx, int *x, int *y, int *width, int *height)
{
    if (!ctx) {
        if (x) {
            *x = 0;
        }
        if (y) {
            *y = 0;
        }
        if (width) {
            *width = 0;
        }
        if (height) {
            *height = 0;
        }
        return;
    }

    if (x) {
        *x = ctx->x;
    }
    if (y) {
        *y = ctx->y;
    }
    if (width) {
        *width = ctx->gamepad_width;
    }
    if (height) {
        *height = ctx->gamepad_height;
    }
}

int GetGamepadImageButtonWidth(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->button_width;
}

int GetGamepadImageButtonHeight(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->button_height;
}

int GetGamepadImageAxisWidth(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->axis_width;
}

int GetGamepadImageAxisHeight(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->axis_height;
}

SDL_GamepadButton GetGamepadImageButtonAt(GamepadImage *ctx, float x, float y)
{
    SDL_FPoint point;
    int i;

    if (!ctx) {
        return SDL_GAMEPAD_BUTTON_INVALID;
    }

    point.x = x;
    point.y = y;
    for (i = 0; i < SDL_arraysize(button_positions); ++i) {
        SDL_bool on_front = SDL_TRUE;

        if (i >= SDL_GAMEPAD_BUTTON_PADDLE1 && i <= SDL_GAMEPAD_BUTTON_PADDLE4) {
            on_front = SDL_FALSE;
        }
        if (on_front == ctx->showing_front) {
            SDL_FRect rect;
            rect.x = (float)ctx->x + button_positions[i].x - ctx->button_width / 2;
            rect.y = (float)ctx->y + button_positions[i].y - ctx->button_height / 2;
            rect.w = (float)ctx->button_width;
            rect.h = (float)ctx->button_height;
            if (SDL_PointInRectFloat(&point, &rect)) {
                return (SDL_GamepadButton)i;
            }
        }
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

SDL_GamepadAxis GetGamepadImageAxisAt(GamepadImage *ctx, float x, float y)
{
    SDL_FPoint point;
    int i;

    if (!ctx) {
        return SDL_GAMEPAD_AXIS_INVALID;
    }

    point.x = x;
    point.y = y;
    if (ctx->showing_front) {
        for (i = 0; i < SDL_arraysize(axis_positions); ++i) {
            SDL_FRect rect;
            rect.x = (float)ctx->x + axis_positions[i].x - ctx->axis_width / 2;
            rect.y = (float)ctx->y + axis_positions[i].y - ctx->axis_height / 2;
            rect.w = (float)ctx->axis_width;
            rect.h = (float)ctx->axis_height;
            if (SDL_PointInRectFloat(&point, &rect)) {
                return (SDL_GamepadAxis)i;
            }
        }
    }
    return SDL_GAMEPAD_AXIS_INVALID;
}

void ClearGamepadImage(GamepadImage *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_zeroa(ctx->buttons);
    SDL_zeroa(ctx->axes);
}

void SetGamepadImageButton(GamepadImage *ctx, SDL_GamepadButton button, SDL_bool active)
{
    if (!ctx) {
        return;
    }

    ctx->buttons[button] = active;
}

void SetGamepadImageAxis(GamepadImage *ctx, SDL_GamepadAxis axis, int direction)
{
    if (!ctx) {
        return;
    }

    ctx->axes[axis] = direction;
}

void UpdateGamepadImageFromGamepad(GamepadImage *ctx, SDL_Gamepad *gamepad)
{
    int i;

    if (!ctx) {
        return;
    }

    for (i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
        const SDL_GamepadButton button = (SDL_GamepadButton)i;

        if (SDL_GetGamepadButton(gamepad, button) == SDL_PRESSED) {
            SetGamepadImageButton(ctx, button, SDL_TRUE);
        } else {
            SetGamepadImageButton(ctx, button, SDL_FALSE);
        }
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
        const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
        const Sint16 deadzone = 8000; /* !!! FIXME: real deadzone */
        const Sint16 value = SDL_GetGamepadAxis(gamepad, axis);
        if (value < -deadzone) {
            SetGamepadImageAxis(ctx, axis, -1);
        } else if (value > deadzone) {
            SetGamepadImageAxis(ctx, axis, 1);
        } else {
            SetGamepadImageAxis(ctx, axis, 0);
        }
    }
}

void RenderGamepadImage(GamepadImage *ctx)
{
    SDL_FRect dst;
    int i;

    if (!ctx) {
        return;
    }

    dst.x = (float)ctx->x;
    dst.y = (float)ctx->y;
    dst.w = (float)ctx->gamepad_width;
    dst.h = (float)ctx->gamepad_height;

    if (ctx->showing_front) {
        SDL_RenderTexture(ctx->renderer, ctx->front_texture, NULL, &dst);
    } else {
        SDL_RenderTexture(ctx->renderer, ctx->back_texture, NULL, &dst);
    }

    for (i = 0; i < SDL_arraysize(button_positions); ++i) {
        if (ctx->buttons[i]) {
            SDL_bool on_front = SDL_TRUE;

            if (i >= SDL_GAMEPAD_BUTTON_PADDLE1 && i <= SDL_GAMEPAD_BUTTON_PADDLE4) {
                on_front = SDL_FALSE;
            }
            if (on_front == ctx->showing_front) {
                dst.x = (float)ctx->x + button_positions[i].x - ctx->button_width / 2;
                dst.y = (float)ctx->y + button_positions[i].y - ctx->button_height / 2;
                dst.w = (float)ctx->button_width;
                dst.h = (float)ctx->button_height;
                SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);
            }
        }
    }

    if (ctx->showing_front) {
        for (i = 0; i < SDL_arraysize(axis_positions); ++i) {
            if (ctx->axes[i] < 0) {
                const double angle = axis_positions[i].angle;
                dst.x = (float)ctx->x + axis_positions[i].x - ctx->axis_width / 2;
                dst.y = (float)ctx->y + axis_positions[i].y - ctx->axis_height / 2;
                dst.w = (float)ctx->axis_width;
                dst.h = (float)ctx->axis_height;
                SDL_RenderTextureRotated(ctx->renderer, ctx->axis_texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
            } else if (ctx->axes[i] > 0) {
                const double angle = axis_positions[i].angle + 180.0;
                dst.x = (float)ctx->x + axis_positions[i].x - ctx->axis_width / 2;
                dst.y = (float)ctx->y + axis_positions[i].y - ctx->axis_height / 2;
                dst.w = (float)ctx->axis_width;
                dst.h = (float)ctx->axis_height;
                SDL_RenderTextureRotated(ctx->renderer, ctx->axis_texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
            }
        }
    }
}

void DestroyGamepadImage(GamepadImage *ctx)
{
    if (ctx) {
        SDL_DestroyTexture(ctx->front_texture);
        SDL_DestroyTexture(ctx->back_texture);
        SDL_DestroyTexture(ctx->button_texture);
        SDL_DestroyTexture(ctx->axis_texture);
        SDL_free(ctx);
    }
}
