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
#include <SDL3/SDL_test_font.h>

#include "gamepadutils.h"
#include "gamepad_front.h"
#include "gamepad_back.h"
#include "gamepad_battery_unknown.h"
#include "gamepad_battery_empty.h"
#include "gamepad_battery_low.h"
#include "gamepad_battery_medium.h"
#include "gamepad_battery_full.h"
#include "gamepad_battery_wired.h"
#include "gamepad_touchpad.h"
#include "gamepad_button.h"
#include "gamepad_button_small.h"
#include "gamepad_axis.h"
#include "gamepad_axis_arrow.h"

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

static SDL_Rect touchpad_area = {
    148, 20, 216, 118
};

typedef struct
{
    Uint8 state;
    float x;
    float y;
    float pressure;
} GamepadTouchpadFinger;

struct GamepadImage
{
    SDL_Renderer *renderer;
    SDL_Texture *front_texture;
    SDL_Texture *back_texture;
    SDL_Texture *battery_texture[1 + SDL_JOYSTICK_POWER_MAX];
    SDL_Texture *touchpad_texture;
    SDL_Texture *button_texture;
    SDL_Texture *axis_texture;
    int gamepad_width;
    int gamepad_height;
    int battery_width;
    int battery_height;
    int touchpad_width;
    int touchpad_height;
    int button_width;
    int button_height;
    int axis_width;
    int axis_height;

    int x;
    int y;
    SDL_bool showing_front;
    SDL_bool showing_battery;
    SDL_bool showing_touchpad;

    SDL_bool buttons[SDL_GAMEPAD_BUTTON_MAX];
    int axes[SDL_GAMEPAD_AXIS_MAX];

    SDL_JoystickPowerLevel battery_level;

    int num_fingers;
    GamepadTouchpadFinger *fingers;
};

static SDL_Texture *CreateTexture(SDL_Renderer *renderer, unsigned char *data, unsigned int len)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_RWops *src = SDL_RWFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_RW(src, SDL_TRUE);
        if (surface) {
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

        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_UNKNOWN] = CreateTexture(renderer, gamepad_battery_unknown_bmp, gamepad_battery_unknown_bmp_len);
        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_EMPTY] = CreateTexture(renderer, gamepad_battery_empty_bmp, gamepad_battery_empty_bmp_len);
        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_LOW] = CreateTexture(renderer, gamepad_battery_low_bmp, gamepad_battery_low_bmp_len);
        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_MEDIUM] = CreateTexture(renderer, gamepad_battery_medium_bmp, gamepad_battery_medium_bmp_len);
        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_FULL] = CreateTexture(renderer, gamepad_battery_full_bmp, gamepad_battery_full_bmp_len);
        ctx->battery_texture[1 + SDL_JOYSTICK_POWER_WIRED] = CreateTexture(renderer, gamepad_battery_wired_bmp, gamepad_battery_wired_bmp_len);
        SDL_QueryTexture(ctx->battery_texture[1 + SDL_JOYSTICK_POWER_UNKNOWN], NULL, NULL, &ctx->battery_width, &ctx->battery_height);

        ctx->touchpad_texture = CreateTexture(renderer, gamepad_touchpad_bmp, gamepad_touchpad_bmp_len);
        SDL_QueryTexture(ctx->touchpad_texture, NULL, NULL, &ctx->touchpad_width, &ctx->touchpad_height);

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

void SetGamepadImageShowingBattery(GamepadImage *ctx, SDL_bool showing_battery)
{
    if (!ctx) {
        return;
    }

    ctx->showing_battery = showing_battery;
}

void SetGamepadImageShowingTouchpad(GamepadImage *ctx, SDL_bool showing_touchpad)
{
    if (!ctx) {
        return;
    }

    ctx->showing_touchpad = showing_touchpad;
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
        if (ctx->showing_touchpad) {
            *height += ctx->touchpad_height;
        }
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

    ctx->battery_level = SDL_GetGamepadPowerLevel(gamepad);

    if (SDL_GetNumGamepadTouchpads(gamepad) > 0) {
        int num_fingers = SDL_GetNumGamepadTouchpadFingers(gamepad, 0);
        if (num_fingers != ctx->num_fingers) {
            GamepadTouchpadFinger *fingers = (GamepadTouchpadFinger *)SDL_realloc(ctx->fingers, num_fingers * sizeof(*fingers));
            if (fingers) {
                ctx->fingers = fingers;
                ctx->num_fingers = num_fingers;
            } else {
                num_fingers = SDL_min(ctx->num_fingers, num_fingers);
            }
        }
        for (i = 0; i < num_fingers; ++i) {
            GamepadTouchpadFinger *finger = &ctx->fingers[i];

            SDL_GetGamepadTouchpadFinger(gamepad, 0, i, &finger->state, &finger->x, &finger->y, &finger->pressure);
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

    if (ctx->showing_battery) {
        dst.x = (float)ctx->x + ctx->gamepad_width - ctx->battery_width;
        dst.y = (float)ctx->y;
        dst.w = (float)ctx->battery_width;
        dst.h = (float)ctx->battery_height;
        SDL_RenderTexture(ctx->renderer, ctx->battery_texture[1 + ctx->battery_level], NULL, &dst);
    }

    if (ctx->showing_touchpad) {
        dst.x = (float)ctx->x + (ctx->gamepad_width - ctx->touchpad_width) / 2;
        dst.y = (float)ctx->y + ctx->gamepad_height;
        dst.w = (float)ctx->touchpad_width;
        dst.h = (float)ctx->touchpad_height;
        SDL_RenderTexture(ctx->renderer, ctx->touchpad_texture, NULL, &dst);

        for (i = 0; i < ctx->num_fingers; ++i) {
            GamepadTouchpadFinger *finger = &ctx->fingers[i];

            if (finger->state) {
                dst.x = (float)ctx->x + (ctx->gamepad_width - ctx->touchpad_width) / 2;
                dst.x += touchpad_area.x + finger->x * touchpad_area.w;
                dst.x -= ctx->button_width / 2;
                dst.y = (float)ctx->y + ctx->gamepad_height;
                dst.y += touchpad_area.y + finger->y * touchpad_area.h;
                dst.y -= ctx->button_height / 2;
                dst.w = (float)ctx->button_width;
                dst.h = (float)ctx->button_height;
                SDL_SetTextureAlphaMod(ctx->button_texture, (Uint8)(finger->pressure * SDL_ALPHA_OPAQUE));
                SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);
                SDL_SetTextureAlphaMod(ctx->button_texture, SDL_ALPHA_OPAQUE);
            }
        }
    }
}

void DestroyGamepadImage(GamepadImage *ctx)
{
    if (ctx) {
        int i;

        SDL_DestroyTexture(ctx->front_texture);
        SDL_DestroyTexture(ctx->back_texture);
        for (i = 0; i < SDL_arraysize(ctx->battery_texture); ++i) {
            SDL_DestroyTexture(ctx->battery_texture[i]);
        }
        SDL_DestroyTexture(ctx->touchpad_texture);
        SDL_DestroyTexture(ctx->button_texture);
        SDL_DestroyTexture(ctx->axis_texture);
        SDL_free(ctx);
    }
}


static const char *gamepad_button_names[] = {
    "A",
    "B",
    "X",
    "Y",
    "Back",
    "Guide",
    "Start",
    "Left Stick",
    "Right Stick",
    "Left Shoulder",
    "Right Shoulder",
    "DPAD Up",
    "DPAD Down",
    "DPAD Left",
    "DPAD Right",
    "Misc1",
    "Paddle1",
    "Paddle2",
    "Paddle3",
    "Paddle4",
    "Touchpad",
};
SDL_COMPILE_TIME_ASSERT(gamepad_button_names, SDL_arraysize(gamepad_button_names) == SDL_GAMEPAD_BUTTON_MAX);

static const char *gamepad_axis_names[] = {
    "LeftX",
    "RightX",
    "RightX",
    "RightY",
    "Left Trigger",
    "Right Trigger",
};
SDL_COMPILE_TIME_ASSERT(gamepad_axis_names, SDL_arraysize(gamepad_axis_names) == SDL_GAMEPAD_AXIS_MAX);

struct GamepadDisplay
{
    SDL_Renderer *renderer;
    SDL_Texture *button_texture;
    SDL_Texture *arrow_texture;
    int button_width;
    int button_height;
    int arrow_width;
    int arrow_height;

    float accel_data[3];
    float gyro_data[3];
    Uint64 last_sensor_update;

    SDL_Rect area;
};

GamepadDisplay *CreateGamepadDisplay(SDL_Renderer *renderer)
{
    GamepadDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->button_texture = CreateTexture(renderer, gamepad_button_small_bmp, gamepad_button_small_bmp_len);
        SDL_QueryTexture(ctx->button_texture, NULL, NULL, &ctx->button_width, &ctx->button_height);

        ctx->arrow_texture = CreateTexture(renderer, gamepad_axis_arrow_bmp, gamepad_axis_arrow_bmp_len);
        SDL_QueryTexture(ctx->arrow_texture, NULL, NULL, &ctx->arrow_width, &ctx->arrow_height);
    }
    return ctx;
}

void SetGamepadDisplayArea(GamepadDisplay *ctx, int x, int y, int w, int h)
{
    if (!ctx) {
        return;
    }

    ctx->area.x = x;
    ctx->area.y = y;
    ctx->area.w = w;
    ctx->area.h = h;
}

void RenderGamepadDisplay(GamepadDisplay *ctx, SDL_Gamepad *gamepad)
{
    float x, y;
    int i;
    char text[128];
    const float margin = 8.0f;
    const float center = ctx->area.w / 2.0f;
    const float arrow_extent = 48.0f;
    SDL_FRect dst, rect;
    Uint8 r, g, b, a;
    SDL_bool has_accel;
    SDL_bool has_gyro;

    SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    for (i = 0; i < SDL_GAMEPAD_BUTTON_MAX; ++i) {
        if (SDL_GamepadHasButton(gamepad, (SDL_GamepadButton)i)) {
            SDL_snprintf(text, sizeof(text), "%s:", gamepad_button_names[i]);
            SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);

            if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)i)) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + center + 2.0f;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = (float)ctx->button_width;
            dst.h = (float)ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            y += ctx->button_height + 2.0f;
        }
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
        if (SDL_GamepadHasAxis(gamepad, (SDL_GamepadAxis)i)) {
            SDL_bool has_negative = (i != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && i != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            Sint16 value = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)i);

            SDL_snprintf(text, sizeof(text), "%s:", gamepad_axis_names[i]);
            SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);
            dst.x = x + center + 2.0f;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->arrow_height / 2;
            dst.w = (float)ctx->arrow_width;
            dst.h = (float)ctx->arrow_height;

            if (has_negative) {
                if (value == SDL_MIN_SINT16) {
                    SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
                } else {
                    SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
                }
                SDL_RenderTextureRotated(ctx->renderer, ctx->arrow_texture, NULL, &dst, 0.0f, NULL, SDL_FLIP_HORIZONTAL);
            }

            dst.x += (float)ctx->arrow_width;

            SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
            rect.x = dst.x + arrow_extent - 2.0f;
            rect.y = dst.y;
            rect.w = 4.0f;
            rect.h = (float)ctx->arrow_height;
            SDL_RenderFillRect(ctx->renderer, &rect);
            SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

            if (value < 0) {
                SDL_SetRenderDrawColor(ctx->renderer, 8, 200, 16, SDL_ALPHA_OPAQUE);
                rect.w = ((float)value / SDL_MIN_SINT16) * arrow_extent;
                rect.x = dst.x + arrow_extent - rect.w;
                rect.y = dst.y + ctx->arrow_height * 0.25f;
                rect.h = ctx->arrow_height / 2.0f;
                SDL_RenderFillRect(ctx->renderer, &rect);
            }

            dst.x += arrow_extent;

            if (value > 0) {
                SDL_SetRenderDrawColor(ctx->renderer, 8, 200, 16, SDL_ALPHA_OPAQUE);
                rect.w = ((float)value / SDL_MAX_SINT16) * arrow_extent;
                rect.x = dst.x;
                rect.y = dst.y + ctx->arrow_height * 0.25f;
                rect.h = ctx->arrow_height / 2.0f;
                SDL_RenderFillRect(ctx->renderer, &rect);
            }

            dst.x += arrow_extent;

            if (value == SDL_MAX_SINT16) {
                SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
            }
            SDL_RenderTexture(ctx->renderer, ctx->arrow_texture, NULL, &dst);

            SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

            y += ctx->button_height + 2;
        }
    }

    if (SDL_GetNumGamepadTouchpads(gamepad) > 0) {
        int num_fingers = SDL_GetNumGamepadTouchpadFingers(gamepad, 0);
        for (i = 0; i < num_fingers; ++i) {
            Uint8 state;
            float finger_x, finger_y, finger_pressure;

            if (SDL_GetGamepadTouchpadFinger(gamepad, 0, i, &state, &finger_x, &finger_y, &finger_pressure) < 0) {
                continue;
            }

            SDL_snprintf(text, sizeof(text), "Touch finger %d:", i);
            SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);

            if (state) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + center + 2.0f;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = (float)ctx->button_width;
            dst.h = (float)ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (state) {
                SDL_snprintf(text, sizeof(text), "(%.2f,%.2f)", finger_x, finger_y);
                SDLTest_DrawString(ctx->renderer, x + center + ctx->button_width + 4.0f, y, text);
            }

            y += ctx->button_height + 2.0f;
        }
    }

    has_accel = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL);
    has_gyro = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO);
    if (has_accel || has_gyro) {
        const int SENSOR_UPDATE_INTERVAL_MS = 100;
        Uint64 now = SDL_GetTicks();

        if (now >= ctx->last_sensor_update + SENSOR_UPDATE_INTERVAL_MS) {
            if (has_accel) {
                SDL_GetGamepadSensorData(gamepad, SDL_SENSOR_ACCEL, ctx->accel_data, SDL_arraysize(ctx->accel_data));
            }
            if (has_gyro) {
                SDL_GetGamepadSensorData(gamepad, SDL_SENSOR_GYRO, ctx->gyro_data, SDL_arraysize(ctx->gyro_data));
            }
            ctx->last_sensor_update = now;
        }

        if (has_accel) {
            SDL_strlcpy(text, "Accelerometer:", sizeof(text));
            SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);
            SDL_snprintf(text, sizeof(text), "(%.2f,%.2f,%.2f)", ctx->accel_data[0], ctx->accel_data[1], ctx->accel_data[2]);
            SDLTest_DrawString(ctx->renderer, x + center + 2.0f, y, text);

            y += ctx->button_height + 2.0f;
        }

        if (has_gyro) {
            SDL_strlcpy(text, "Gyro:", sizeof(text));
            SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);
            SDL_snprintf(text, sizeof(text), "(%.2f,%.2f,%.2f)", ctx->gyro_data[0], ctx->gyro_data[1], ctx->gyro_data[2]);
            SDLTest_DrawString(ctx->renderer, x + center + 2.0f, y, text);

            y += ctx->button_height + 2.0f;
        }
    }
}

void DestroyGamepadDisplay(GamepadDisplay *ctx)
{
    SDL_free(ctx);
}


struct JoystickDisplay
{
    SDL_Renderer *renderer;
    SDL_Texture *button_texture;
    SDL_Texture *arrow_texture;
    int button_width;
    int button_height;
    int arrow_width;
    int arrow_height;

    SDL_Rect area;
};

JoystickDisplay *CreateJoystickDisplay(SDL_Renderer *renderer)
{
    JoystickDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->button_texture = CreateTexture(renderer, gamepad_button_small_bmp, gamepad_button_small_bmp_len);
        SDL_QueryTexture(ctx->button_texture, NULL, NULL, &ctx->button_width, &ctx->button_height);

        ctx->arrow_texture = CreateTexture(renderer, gamepad_axis_arrow_bmp, gamepad_axis_arrow_bmp_len);
        SDL_QueryTexture(ctx->arrow_texture, NULL, NULL, &ctx->arrow_width, &ctx->arrow_height);
    }
    return ctx;
}

void SetJoystickDisplayArea(JoystickDisplay *ctx, int x, int y, int w, int h)
{
    if (!ctx) {
        return;
    }

    ctx->area.x = x;
    ctx->area.y = y;
    ctx->area.w = w;
    ctx->area.h = h;
}

void RenderJoystickDisplay(JoystickDisplay *ctx, SDL_Joystick *joystick)
{
    float x, y;
    int i;
    int nbuttons = SDL_GetNumJoystickButtons(joystick);
    int naxes = SDL_GetNumJoystickAxes(joystick);
    int nhats = SDL_GetNumJoystickHats(joystick);
    char text[32];
    const float margin = 8.0f;
    const float center = 80.0f;
    const float arrow_extent = 48.0f;
    SDL_FRect dst, rect;
    Uint8 r, g, b, a;

    SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

    x = (float)ctx->area.x + margin;
    y = (float)ctx->area.y + margin;

    if (nbuttons > 0) {
        SDLTest_DrawString(ctx->renderer, x, y, "BUTTONS");
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < nbuttons; ++i) {
            SDL_snprintf(text, sizeof(text), "%2.d:", i);
            SDLTest_DrawString(ctx->renderer, x, y, text);

            if (SDL_GetJoystickButton(joystick, (Uint8)i)) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = (float)ctx->button_width;
            dst.h = (float)ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            y += ctx->button_height + 2;
        }
    }

    x = (float)ctx->area.x + margin + center + margin;
    y = (float)ctx->area.y + margin;

    if (naxes > 0) {
        SDLTest_DrawString(ctx->renderer, x, y, "AXES");
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < naxes; ++i) {
            Sint16 value = SDL_GetJoystickAxis(joystick, i);

            SDL_snprintf(text, sizeof(text), "%d:", i);
            SDLTest_DrawString(ctx->renderer, x, y, text);

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2.0f;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->arrow_height / 2;
            dst.w = (float)ctx->arrow_width;
            dst.h = (float)ctx->arrow_height;

            if (value == SDL_MIN_SINT16) {
                SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
            }
            SDL_RenderTextureRotated(ctx->renderer, ctx->arrow_texture, NULL, &dst, 0.0f, NULL, SDL_FLIP_HORIZONTAL);

            dst.x += (float)ctx->arrow_width;

            SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
            rect.x = dst.x + arrow_extent - 2.0f;
            rect.y = dst.y;
            rect.w = 4.0f;
            rect.h = (float)ctx->arrow_height;
            SDL_RenderFillRect(ctx->renderer, &rect);
            SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

            if (value < 0) {
                SDL_SetRenderDrawColor(ctx->renderer, 8, 200, 16, SDL_ALPHA_OPAQUE);
                rect.w = ((float)value / SDL_MIN_SINT16) * arrow_extent;
                rect.x = dst.x + arrow_extent - rect.w;
                rect.y = dst.y + ctx->arrow_height * 0.25f;
                rect.h = ctx->arrow_height / 2.0f;
                SDL_RenderFillRect(ctx->renderer, &rect);
            }

            dst.x += arrow_extent;

            if (value > 0) {
                SDL_SetRenderDrawColor(ctx->renderer, 8, 200, 16, SDL_ALPHA_OPAQUE);
                rect.w = ((float)value / SDL_MAX_SINT16) * arrow_extent;
                rect.x = dst.x;
                rect.y = dst.y + ctx->arrow_height * 0.25f;
                rect.h = ctx->arrow_height / 2.0f;
                SDL_RenderFillRect(ctx->renderer, &rect);
            }

            dst.x += arrow_extent;

            if (value == SDL_MAX_SINT16) {
                SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
            }
            SDL_RenderTexture(ctx->renderer, ctx->arrow_texture, NULL, &dst);

            SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

            y += ctx->button_height + 2;
        }
    }

    y += FONT_LINE_HEIGHT + 2;

    if (nhats > 0) {
        SDLTest_DrawString(ctx->renderer, x, y, "HATS");
        y += FONT_LINE_HEIGHT + 2 + 1.5f * ctx->button_height - FONT_CHARACTER_SIZE / 2;

        for (i = 0; i < nhats; ++i) {
            Uint8 value = SDL_GetJoystickHat(joystick, i);

            SDL_snprintf(text, sizeof(text), "%d:", i);
            SDLTest_DrawString(ctx->renderer, x, y, text);

            if (value & SDL_HAT_LEFT) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = (float)ctx->button_width;
            dst.h = (float)ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_UP) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x += (float)ctx->button_width;
            dst.y -= (float)ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_DOWN) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.y += (float)ctx->button_height * 2;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_RIGHT) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x += (float)ctx->button_width;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            y += 3 * ctx->button_height + 2;
        }
    }
}

void DestroyJoystickDisplay(JoystickDisplay *ctx)
{
    SDL_free(ctx);
}

