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
#include <SDL3/SDL_test_font.h>

#include "gamepadutils.h"
#include "gamepad_front.h"
#include "gamepad_back.h"
#include "gamepad_face_abxy.h"
#include "gamepad_face_axby.h"
#include "gamepad_face_bayx.h"
#include "gamepad_face_sony.h"
#include "gamepad_battery.h"
#include "gamepad_battery_wired.h"
#include "gamepad_touchpad.h"
#include "gamepad_button.h"
#include "gamepad_button_small.h"
#include "gamepad_axis.h"
#include "gamepad_axis_arrow.h"
#include "gamepad_button_background.h"
#include "gamepad_wired.h"
#include "gamepad_wireless.h"

#include <limits.h>

#define RAD_TO_DEG (180.0f / SDL_PI_F)

/* Used to draw a 3D cube to represent the gyroscope orientation */
typedef struct
{
    float x, y, z;
} Vector3;

struct Quaternion
{
    float x, y, z, w;
};

static const Vector3 debug_cube_vertices[] = {
    { -1.0f, -1.0f, -1.0f },
    { 1.0f, -1.0f, -1.0f },
    { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },
    { -1.0f, -1.0f, 1.0f },
    { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },
    { -1.0f, 1.0f, 1.0f },
};

static const int debug_cube_edges[][2] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, /* bottom square */
    { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, /* top square */
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, /* verticals */
};

static Vector3 RotateVectorByQuaternion(const Vector3 *v, const Quaternion *q) {
    /* v' = q * v * q^-1 */
    float x = v->x, y = v->y, z = v->z;
    float qx = q->x, qy = q->y, qz = q->z, qw = q->w;

    /* Calculate quaternion *vector */
    float ix = qw * x + qy * z - qz * y;
    float iy = qw * y + qz * x - qx * z;
    float iz = qw * z + qx * y - qy * x;
    float iw = -qx * x - qy * y - qz * z;

    /* Result = result * conjugate(q) */
    Vector3 out;
    out.x = ix * qw + iw * -qx + iy * -qz - iz * -qy;
    out.y = iy * qw + iw * -qy + iz * -qx - ix * -qz;
    out.z = iz * qw + iw * -qz + ix * -qy - iy * -qx;
    return out;
}

#ifdef GYRO_ISOMETRIC_PROJECTION
static SDL_FPoint ProjectVec3ToRect(const Vector3 *v, const SDL_FRect *rect)
{
    SDL_FPoint out;
    /* Simple orthographic projection using X and Y; scale to fit into rect */
    out.x = rect->x + (rect->w / 2.0f) + (v->x * (rect->w / 2.0f));
    out.y = rect->y + (rect->h / 2.0f) - (v->y * (rect->h / 2.0f)); /* Y inverted */
    return out;
}
#else
static SDL_FPoint ProjectVec3ToRect(const Vector3 *v, const SDL_FRect *rect)
{
    const float verticalFOV_deg = 40.0f;
    const float cameraZ = 4.0f; /* Camera is at(0, 0, +4), looking toward origin */
    float aspect = rect->w / rect->h;

    float fovScaleY = SDL_tanf((verticalFOV_deg * SDL_PI_F / 180.0f) * 0.5f);
    float fovScaleX = fovScaleY * aspect;

    float relZ = cameraZ - v->z;
    if (relZ < 0.01f)
        relZ = 0.01f; /* Prevent division by 0 or negative depth */

    float ndc_x = (v->x / relZ) / fovScaleX;
    float ndc_y = (v->y / relZ) / fovScaleY;

    /* Convert to screen space */
    SDL_FPoint out;
    out.x = rect->x + (rect->w / 2.0f) + (ndc_x * rect->w / 2.0f);
    out.y = rect->y + (rect->h / 2.0f) - (ndc_y * rect->h / 2.0f); /* flip Y */
    return out;
}
#endif

void DrawGyroDebugCube(SDL_Renderer *renderer, const Quaternion *orientation, const SDL_FRect *rect)
{
    SDL_FPoint projected[8];
    int i;
    for (i = 0; i < 8; ++i) {
        Vector3 rotated = RotateVectorByQuaternion(&debug_cube_vertices[i], orientation);
        projected[i] = ProjectVec3ToRect(&rotated, rect);
    }

    for (i = 0; i < 12; ++i) {
        const SDL_FPoint p0 = projected[debug_cube_edges[i][0]];
        const SDL_FPoint p1 = projected[debug_cube_edges[i][1]];
        SDL_RenderLine(renderer, p0.x, p0.y, p1.x, p1.y);
    }
}

#define CIRCLE_SEGMENTS 64

static Vector3 kCirclePoints3D_XY_Plane[CIRCLE_SEGMENTS];
static Vector3 kCirclePoints3D_XZ_Plane[CIRCLE_SEGMENTS];
static Vector3 kCirclePoints3D_YZ_Plane[CIRCLE_SEGMENTS];

void InitCirclePoints3D(void)
{
    int i;
    for (i = 0; i < CIRCLE_SEGMENTS; ++i) {
        float theta = ((float)i / CIRCLE_SEGMENTS) * SDL_PI_F * 2.0f;
        kCirclePoints3D_XY_Plane[i].x = SDL_cosf(theta);
        kCirclePoints3D_XY_Plane[i].y = SDL_sinf(theta);
        kCirclePoints3D_XY_Plane[i].z = 0.0f;
    }

    for (i = 0; i < CIRCLE_SEGMENTS; ++i) {
        float theta = ((float)i / CIRCLE_SEGMENTS) * SDL_PI_F * 2.0f;
        kCirclePoints3D_XZ_Plane[i].x = SDL_cosf(theta);
        kCirclePoints3D_XZ_Plane[i].y = 0.0f;
        kCirclePoints3D_XZ_Plane[i].z = SDL_sinf(theta);
    }

    for (i = 0; i < CIRCLE_SEGMENTS; ++i) {
        float theta = ((float)i / CIRCLE_SEGMENTS) * SDL_PI_F * 2.0f;
        kCirclePoints3D_YZ_Plane[i].x = 0.0f;
        kCirclePoints3D_YZ_Plane[i].y = SDL_cosf(theta);
        kCirclePoints3D_YZ_Plane[i].z = SDL_sinf(theta);
    }
}

void DrawGyroCircle(
    SDL_Renderer *renderer,
    const Vector3 *circlePoints,
    int numSegments,
    const Quaternion *orientation,
    const SDL_FRect *bounds,
    Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    SDL_FPoint lastScreenPt = { 0 };
    bool hasLast = false;
    int i;
    for (i = 0; i <= numSegments; ++i) {
        int index = i % numSegments;

        Vector3 rotated = RotateVectorByQuaternion(&circlePoints[index], orientation);
        SDL_FPoint screenPtVec2 = ProjectVec3ToRect(&rotated, bounds);
        SDL_FPoint screenPt;
        screenPt.x = screenPtVec2.x;
        screenPt.y = screenPtVec2.y;


        if (hasLast) {
            SDL_RenderLine(renderer, lastScreenPt.x, lastScreenPt.y, screenPt.x, screenPt.y);
        }

        lastScreenPt = screenPt;
        hasLast = true;
    }
}

void DrawGyroDebugCircle(SDL_Renderer *renderer, const Quaternion *orientation, const SDL_FRect *bounds)
{
    /* Store current color */
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    DrawGyroCircle(renderer, kCirclePoints3D_YZ_Plane, CIRCLE_SEGMENTS, orientation, bounds, GYRO_COLOR_RED);   /* X axis - pitch */
    DrawGyroCircle(renderer, kCirclePoints3D_XZ_Plane, CIRCLE_SEGMENTS, orientation, bounds, GYRO_COLOR_GREEN); /* Y axis - yaw */
    DrawGyroCircle(renderer, kCirclePoints3D_XY_Plane, CIRCLE_SEGMENTS, orientation, bounds, GYRO_COLOR_BLUE);  /* Z axis - Roll */

    /* Restore current color */
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void DrawAccelerometerDebugArrow(SDL_Renderer *renderer, const Quaternion *gyro_quaternion, const float *accel_data, const SDL_FRect *bounds)
{
    /* Store current color */
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    const float flGravity = 9.81f;
    Vector3 vAccel;
    vAccel.x = accel_data[0] / flGravity;
    vAccel.y = accel_data[1] / flGravity;
    vAccel.z = accel_data[2] / flGravity;

    Vector3 origin = { 0.0f, 0.0f, 0.0f };
    Vector3 rotated_accel = RotateVectorByQuaternion(&vAccel, gyro_quaternion);

    /* Project the origin and rotated vector to screen space */
    SDL_FPoint origin_screen = ProjectVec3ToRect(&origin, bounds);
    SDL_FPoint accel_screen = ProjectVec3ToRect(&rotated_accel, bounds);

    /* Draw the line from origin to the rotated accelerometer vector */
    SDL_SetRenderDrawColor(renderer, GYRO_COLOR_ORANGE);
    SDL_RenderLine(renderer, origin_screen.x, origin_screen.y, accel_screen.x, accel_screen.y);

    const float head_width = 4.0f;
    SDL_FRect arrow_head_rect;
    arrow_head_rect.x = accel_screen.x - head_width * 0.5f;
    arrow_head_rect.y = accel_screen.y - head_width * 0.5f;
    arrow_head_rect.w = head_width;
    arrow_head_rect.h = head_width;
    SDL_RenderRect(renderer, &arrow_head_rect);

    /* Restore current color */
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

/* This is indexed by gamepad element */
static const struct
{
    int x;
    int y;
} button_positions[] = {
    { 413, 190 }, /* SDL_GAMEPAD_BUTTON_SOUTH */
    { 456, 156 }, /* SDL_GAMEPAD_BUTTON_EAST */
    { 372, 159 }, /* SDL_GAMEPAD_BUTTON_WEST */
    { 415, 127 }, /* SDL_GAMEPAD_BUTTON_NORTH */
    { 199, 157 }, /* SDL_GAMEPAD_BUTTON_BACK */
    { 257, 153 }, /* SDL_GAMEPAD_BUTTON_GUIDE */
    { 314, 157 }, /* SDL_GAMEPAD_BUTTON_START */
    {  98, 177 }, /* SDL_GAMEPAD_BUTTON_LEFT_STICK */
    { 331, 254 }, /* SDL_GAMEPAD_BUTTON_RIGHT_STICK */
    { 102, 65 },  /* SDL_GAMEPAD_BUTTON_LEFT_SHOULDER */
    { 421, 61 },  /* SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER */
    { 179, 213 }, /* SDL_GAMEPAD_BUTTON_DPAD_UP */
    { 179, 274 }, /* SDL_GAMEPAD_BUTTON_DPAD_DOWN */
    { 141, 242 }, /* SDL_GAMEPAD_BUTTON_DPAD_LEFT */
    { 211, 242 }, /* SDL_GAMEPAD_BUTTON_DPAD_RIGHT */
    { 257, 199 }, /* SDL_GAMEPAD_BUTTON_MISC1 */
    { 157, 160 }, /* SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 */
    { 355, 160 }, /* SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 */
    { 157, 200 }, /* SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2 */
    { 355, 200 }, /* SDL_GAMEPAD_BUTTON_LEFT_PADDLE2 */
};

/* This is indexed by gamepad element */
static const struct
{
    int x;
    int y;
    double angle;
} axis_positions[] = {
    { 99, 178, 270.0 },  /* SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE */
    { 99, 178, 90.0 },   /* SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE */
    { 99, 178, 0.0 },    /* SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE */
    { 99, 178, 180.0 },  /* SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE */
    { 331, 256, 270.0 }, /* SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE */
    { 331, 256, 90.0 },  /* SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE */
    { 331, 256, 0.0 },   /* SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE */
    { 331, 256, 180.0 }, /* SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE */
    { 116, 5, 180.0 },   /* SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER */
    { 400, 5, 180.0 },   /* SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER */
};

static SDL_FRect touchpad_area = {
    148.0f, 20.0f, 216.0f, 118.0f
};

typedef struct
{
    bool down;
    float x;
    float y;
    float pressure;
} GamepadTouchpadFinger;

struct GamepadImage
{
    SDL_Renderer *renderer;
    SDL_Texture *front_texture;
    SDL_Texture *back_texture;
    SDL_Texture *face_abxy_texture;
    SDL_Texture *face_axby_texture;
    SDL_Texture *face_bayx_texture;
    SDL_Texture *face_sony_texture;
    SDL_Texture *connection_texture[2];
    SDL_Texture *battery_texture[2];
    SDL_Texture *touchpad_texture;
    SDL_Texture *button_texture;
    SDL_Texture *axis_texture;
    float gamepad_width;
    float gamepad_height;
    float face_width;
    float face_height;
    float connection_width;
    float connection_height;
    float battery_width;
    float battery_height;
    float touchpad_width;
    float touchpad_height;
    float button_width;
    float button_height;
    float axis_width;
    float axis_height;

    float x;
    float y;
    bool showing_front;
    bool showing_touchpad;
    SDL_GamepadType type;
    ControllerDisplayMode display_mode;

    bool elements[SDL_GAMEPAD_ELEMENT_MAX];

    SDL_JoystickConnectionState connection_state;
    SDL_PowerState battery_state;
    int battery_percent;

    int num_fingers;
    GamepadTouchpadFinger *fingers;
};

static SDL_Texture *CreateTexture(SDL_Renderer *renderer, unsigned char *data, unsigned int len)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_IOStream *src = SDL_IOFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_IO(src, true);
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
        SDL_GetTextureSize(ctx->front_texture, &ctx->gamepad_width, &ctx->gamepad_height);

        ctx->face_abxy_texture = CreateTexture(renderer, gamepad_face_abxy_bmp, gamepad_face_abxy_bmp_len);
        ctx->face_axby_texture = CreateTexture(renderer, gamepad_face_axby_bmp, gamepad_face_axby_bmp_len);
        ctx->face_bayx_texture = CreateTexture(renderer, gamepad_face_bayx_bmp, gamepad_face_bayx_bmp_len);
        ctx->face_sony_texture = CreateTexture(renderer, gamepad_face_sony_bmp, gamepad_face_sony_bmp_len);
        SDL_GetTextureSize(ctx->face_abxy_texture, &ctx->face_width, &ctx->face_height);

        ctx->connection_texture[0] = CreateTexture(renderer, gamepad_wired_bmp, gamepad_wired_bmp_len);
        ctx->connection_texture[1] = CreateTexture(renderer, gamepad_wireless_bmp, gamepad_wireless_bmp_len);
        SDL_GetTextureSize(ctx->connection_texture[0], &ctx->connection_width, &ctx->connection_height);

        ctx->battery_texture[0] = CreateTexture(renderer, gamepad_battery_bmp, gamepad_battery_bmp_len);
        ctx->battery_texture[1] = CreateTexture(renderer, gamepad_battery_wired_bmp, gamepad_battery_wired_bmp_len);
        SDL_GetTextureSize(ctx->battery_texture[0], &ctx->battery_width, &ctx->battery_height);

        ctx->touchpad_texture = CreateTexture(renderer, gamepad_touchpad_bmp, gamepad_touchpad_bmp_len);
        SDL_GetTextureSize(ctx->touchpad_texture, &ctx->touchpad_width, &ctx->touchpad_height);

        ctx->button_texture = CreateTexture(renderer, gamepad_button_bmp, gamepad_button_bmp_len);
        SDL_GetTextureSize(ctx->button_texture, &ctx->button_width, &ctx->button_height);
        SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);

        ctx->axis_texture = CreateTexture(renderer, gamepad_axis_bmp, gamepad_axis_bmp_len);
        SDL_GetTextureSize(ctx->axis_texture, &ctx->axis_width, &ctx->axis_height);
        SDL_SetTextureColorMod(ctx->axis_texture, 10, 255, 21);

        ctx->showing_front = true;
    }
    return ctx;
}

void SetGamepadImagePosition(GamepadImage *ctx, float x, float y)
{
    if (!ctx) {
        return;
    }

    ctx->x = x;
    ctx->y = y;
}

void GetGamepadImageArea(GamepadImage *ctx, SDL_FRect *area)
{
    if (!ctx) {
        SDL_zerop(area);
        return;
    }

    area->x = ctx->x;
    area->y = ctx->y;
    area->w = ctx->gamepad_width;
    area->h = ctx->gamepad_height;
    if (ctx->showing_touchpad) {
        area->h += ctx->touchpad_height;
    }
}

void GetGamepadTouchpadArea(GamepadImage *ctx, SDL_FRect *area)
{
    if (!ctx) {
        SDL_zerop(area);
        return;
    }

    area->x = ctx->x + (ctx->gamepad_width - ctx->touchpad_width) / 2 + touchpad_area.x;
    area->y = ctx->y + ctx->gamepad_height + touchpad_area.y;
    area->w = touchpad_area.w;
    area->h = touchpad_area.h;
}

void SetGamepadImageShowingFront(GamepadImage *ctx, bool showing_front)
{
    if (!ctx) {
        return;
    }

    ctx->showing_front = showing_front;
}

SDL_GamepadType GetGamepadImageType(GamepadImage *ctx)
{
    if (!ctx) {
        return SDL_GAMEPAD_TYPE_UNKNOWN;
    }

    return ctx->type;
}

void SetGamepadImageDisplayMode(GamepadImage *ctx, ControllerDisplayMode display_mode)
{
    if (!ctx) {
        return;
    }

    ctx->display_mode = display_mode;
}

float GetGamepadImageButtonWidth(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->button_width;
}

float GetGamepadImageButtonHeight(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->button_height;
}

float GetGamepadImageAxisWidth(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->axis_width;
}

float GetGamepadImageAxisHeight(GamepadImage *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->axis_height;
}

int GetGamepadImageElementAt(GamepadImage *ctx, float x, float y)
{
    SDL_FPoint point;
    int i;

    if (!ctx) {
        return SDL_GAMEPAD_ELEMENT_INVALID;
    }

    point.x = x;
    point.y = y;

    if (ctx->showing_front) {
        for (i = 0; i < SDL_arraysize(axis_positions); ++i) {
            const int element = SDL_GAMEPAD_BUTTON_COUNT + i;
            SDL_FRect rect;

            if (element == SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER ||
                element == SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER) {
                rect.w = ctx->axis_width;
                rect.h = ctx->axis_height;
                rect.x = ctx->x + axis_positions[i].x - rect.w / 2;
                rect.y = ctx->y + axis_positions[i].y - rect.h / 2;
                if (SDL_PointInRectFloat(&point, &rect)) {
                    if (element == SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER) {
                        return SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER;
                    } else {
                        return SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER;
                    }
                }
            } else if (element == SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE) {
                rect.w = ctx->button_width * 2.0f;
                rect.h = ctx->button_height * 2.0f;
                rect.x = ctx->x + button_positions[SDL_GAMEPAD_BUTTON_LEFT_STICK].x - rect.w / 2;
                rect.y = ctx->y + button_positions[SDL_GAMEPAD_BUTTON_LEFT_STICK].y - rect.h / 2;
                if (SDL_PointInRectFloat(&point, &rect)) {
                    float delta_x, delta_y;
                    float delta_squared;
                    float thumbstick_radius = ctx->button_width * 0.1f;

                    delta_x = (x - (ctx->x + button_positions[SDL_GAMEPAD_BUTTON_LEFT_STICK].x));
                    delta_y = (y - (ctx->y + button_positions[SDL_GAMEPAD_BUTTON_LEFT_STICK].y));
                    delta_squared = (delta_x * delta_x) + (delta_y * delta_y);
                    if (delta_squared > (thumbstick_radius * thumbstick_radius)) {
                        float angle = SDL_atan2f(delta_y, delta_x) + SDL_PI_F;
                        if (angle < SDL_PI_F * 0.25f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE;
                        } else if (angle < SDL_PI_F * 0.75f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE;
                        } else if (angle < SDL_PI_F * 1.25f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE;
                        } else if (angle < SDL_PI_F * 1.75f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE;
                        } else {
                            return SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE;
                        }
                    }
                }
            } else if (element == SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE) {
                rect.w = ctx->button_width * 2.0f;
                rect.h = ctx->button_height * 2.0f;
                rect.x = ctx->x + button_positions[SDL_GAMEPAD_BUTTON_RIGHT_STICK].x - rect.w / 2;
                rect.y = ctx->y + button_positions[SDL_GAMEPAD_BUTTON_RIGHT_STICK].y - rect.h / 2;
                if (SDL_PointInRectFloat(&point, &rect)) {
                    float delta_x, delta_y;
                    float delta_squared;
                    float thumbstick_radius = ctx->button_width * 0.1f;

                    delta_x = (x - (ctx->x + button_positions[SDL_GAMEPAD_BUTTON_RIGHT_STICK].x));
                    delta_y = (y - (ctx->y + button_positions[SDL_GAMEPAD_BUTTON_RIGHT_STICK].y));
                    delta_squared = (delta_x * delta_x) + (delta_y * delta_y);
                    if (delta_squared > (thumbstick_radius * thumbstick_radius)) {
                        float angle = SDL_atan2f(delta_y, delta_x) + SDL_PI_F;
                        if (angle < SDL_PI_F * 0.25f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE;
                        } else if (angle < SDL_PI_F * 0.75f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE;
                        } else if (angle < SDL_PI_F * 1.25f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE;
                        } else if (angle < SDL_PI_F * 1.75f) {
                            return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE;
                        } else {
                            return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE;
                        }
                    }
                }
            }
        }
    }

    for (i = 0; i < SDL_arraysize(button_positions); ++i) {
        bool on_front = true;

        if (i >= SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 && i <= SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) {
            on_front = false;
        }
        if (on_front == ctx->showing_front) {
            SDL_FRect rect;
            rect.x = ctx->x + button_positions[i].x - ctx->button_width / 2;
            rect.y = ctx->y + button_positions[i].y - ctx->button_height / 2;
            rect.w = ctx->button_width;
            rect.h = ctx->button_height;
            if (SDL_PointInRectFloat(&point, &rect)) {
                return (SDL_GamepadButton)i;
            }
        }
    }
    return SDL_GAMEPAD_ELEMENT_INVALID;
}

void ClearGamepadImage(GamepadImage *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_zeroa(ctx->elements);
}

void SetGamepadImageElement(GamepadImage *ctx, int element, bool active)
{
    if (!ctx) {
        return;
    }

    ctx->elements[element] = active;
}

void UpdateGamepadImageFromGamepad(GamepadImage *ctx, SDL_Gamepad *gamepad)
{
    int i;

    if (!ctx) {
        return;
    }

    ctx->type = SDL_GetGamepadType(gamepad);
    char *mapping = SDL_GetGamepadMapping(gamepad);
    if (mapping) {
        if (SDL_strstr(mapping, "SDL_GAMECONTROLLER_USE_BUTTON_LABELS")) {
            /* Just for display purposes */
            ctx->type = SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
        }
        SDL_free(mapping);
    }

    for (i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
        const SDL_GamepadButton button = (SDL_GamepadButton)i;

        SetGamepadImageElement(ctx, button, SDL_GetGamepadButton(gamepad, button));
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
        const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
        const Sint16 deadzone = 8000; /* !!! FIXME: real deadzone */
        const Sint16 value = SDL_GetGamepadAxis(gamepad, axis);
        switch (i) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE, (value < -deadzone));
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE, (value > deadzone));
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE, (value < -deadzone));
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE, (value > deadzone));
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE, (value < -deadzone));
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE, (value > deadzone));
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE, (value < -deadzone));
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE, (value > deadzone));
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER, (value > deadzone));
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            SetGamepadImageElement(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER, (value > deadzone));
            break;
        default:
            break;
        }
    }

    ctx->connection_state = SDL_GetGamepadConnectionState(gamepad);
    ctx->battery_state = SDL_GetGamepadPowerInfo(gamepad, &ctx->battery_percent);

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

            SDL_GetGamepadTouchpadFinger(gamepad, 0, i, &finger->down, &finger->x, &finger->y, &finger->pressure);
        }
        ctx->showing_touchpad = true;
    } else {
        if (ctx->fingers) {
            SDL_free(ctx->fingers);
            ctx->fingers = NULL;
            ctx->num_fingers = 0;
        }
        ctx->showing_touchpad = false;
    }
}

void RenderGamepadImage(GamepadImage *ctx)
{
    SDL_FRect dst;
    int i;

    if (!ctx) {
        return;
    }

    dst.x = ctx->x;
    dst.y = ctx->y;
    dst.w = ctx->gamepad_width;
    dst.h = ctx->gamepad_height;

    if (ctx->showing_front) {
        SDL_RenderTexture(ctx->renderer, ctx->front_texture, NULL, &dst);
    } else {
        SDL_RenderTexture(ctx->renderer, ctx->back_texture, NULL, &dst);
    }

    for (i = 0; i < SDL_arraysize(button_positions); ++i) {
        if (ctx->elements[i]) {
            SDL_GamepadButton button_position = (SDL_GamepadButton)i;
            bool on_front = true;

            if (i >= SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 && i <= SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) {
                on_front = false;
            }
            if (on_front == ctx->showing_front) {
                dst.w = ctx->button_width;
                dst.h = ctx->button_height;
                dst.x = ctx->x + button_positions[button_position].x - dst.w / 2;
                dst.y = ctx->y + button_positions[button_position].y - dst.h / 2;
                SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);
            }
        }
    }

    if (ctx->showing_front) {
        dst.x = ctx->x + 363;
        dst.y = ctx->y + 118;
        dst.w = ctx->face_width;
        dst.h = ctx->face_height;

        switch (SDL_GetGamepadButtonLabelForType(ctx->type, SDL_GAMEPAD_BUTTON_EAST)) {
        case SDL_GAMEPAD_BUTTON_LABEL_B:
            SDL_RenderTexture(ctx->renderer, ctx->face_abxy_texture, NULL, &dst);
            break;
        case SDL_GAMEPAD_BUTTON_LABEL_X:
            SDL_RenderTexture(ctx->renderer, ctx->face_axby_texture, NULL, &dst);
            break;
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            SDL_RenderTexture(ctx->renderer, ctx->face_bayx_texture, NULL, &dst);
            break;
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
            SDL_RenderTexture(ctx->renderer, ctx->face_sony_texture, NULL, &dst);
            break;
        default:
            break;
        }
    }

    if (ctx->showing_front) {
        for (i = 0; i < SDL_arraysize(axis_positions); ++i) {
            const int element = SDL_GAMEPAD_BUTTON_COUNT + i;
            if (ctx->elements[element]) {
                const double angle = axis_positions[i].angle;
                dst.w = ctx->axis_width;
                dst.h = ctx->axis_height;
                dst.x = ctx->x + axis_positions[i].x - dst.w / 2;
                dst.y = ctx->y + axis_positions[i].y - dst.h / 2;
                SDL_RenderTextureRotated(ctx->renderer, ctx->axis_texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
            }
        }
    }

    if (ctx->display_mode == CONTROLLER_MODE_TESTING) {
        dst.x = ctx->x + ctx->gamepad_width - ctx->battery_width - 4 - ctx->connection_width;
        dst.y = ctx->y;
        dst.w = ctx->connection_width;
        dst.h = ctx->connection_height;

        switch (ctx->connection_state) {
        case SDL_JOYSTICK_CONNECTION_WIRED:
            SDL_RenderTexture(ctx->renderer, ctx->connection_texture[0], NULL, &dst);
            break;
        case SDL_JOYSTICK_CONNECTION_WIRELESS:
            SDL_RenderTexture(ctx->renderer, ctx->connection_texture[1], NULL, &dst);
            break;
        default:
            break;
        }
    }

    if (ctx->display_mode == CONTROLLER_MODE_TESTING &&
        ctx->battery_state != SDL_POWERSTATE_NO_BATTERY &&
        ctx->battery_state != SDL_POWERSTATE_UNKNOWN) {
        Uint8 r, g, b, a;
        SDL_FRect fill;

        dst.x = ctx->x + ctx->gamepad_width - ctx->battery_width;
        dst.y = ctx->y;
        dst.w = ctx->battery_width;
        dst.h = ctx->battery_height;

        SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);
        if (ctx->battery_percent > 40) {
            SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xD4, 0x50, 0xFF);
        } else if (ctx->battery_percent > 10) {
            SDL_SetRenderDrawColor(ctx->renderer, 0xFF, 0xC7, 0x00, 0xFF);
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, 0xC8, 0x1D, 0x13, 0xFF);
        }

        fill = dst;
        fill.x += 2;
        fill.y += 2;
        fill.h -= 4;
        fill.w = 25.0f * (ctx->battery_percent / 100.0f);
        SDL_RenderFillRect(ctx->renderer, &fill);
        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

        if (ctx->battery_state == SDL_POWERSTATE_ON_BATTERY) {
            SDL_RenderTexture(ctx->renderer, ctx->battery_texture[0], NULL, &dst);
        } else {
            SDL_RenderTexture(ctx->renderer, ctx->battery_texture[1], NULL, &dst);
        }
    }

    if (ctx->display_mode == CONTROLLER_MODE_TESTING && ctx->showing_touchpad) {
        dst.x = ctx->x + (ctx->gamepad_width - ctx->touchpad_width) / 2;
        dst.y = ctx->y + ctx->gamepad_height;
        dst.w = ctx->touchpad_width;
        dst.h = ctx->touchpad_height;
        SDL_RenderTexture(ctx->renderer, ctx->touchpad_texture, NULL, &dst);

        for (i = 0; i < ctx->num_fingers; ++i) {
            GamepadTouchpadFinger *finger = &ctx->fingers[i];

            if (finger->down) {
                dst.x = ctx->x + (ctx->gamepad_width - ctx->touchpad_width) / 2;
                dst.x += touchpad_area.x + finger->x * touchpad_area.w;
                dst.x -= ctx->button_width / 2;
                dst.y = ctx->y + ctx->gamepad_height;
                dst.y += touchpad_area.y + finger->y * touchpad_area.h;
                dst.y -= ctx->button_height / 2;
                dst.w = ctx->button_width;
                dst.h = ctx->button_height;
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
        SDL_DestroyTexture(ctx->face_abxy_texture);
        SDL_DestroyTexture(ctx->face_axby_texture);
        SDL_DestroyTexture(ctx->face_bayx_texture);
        SDL_DestroyTexture(ctx->face_sony_texture);
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
    "South",
    "East",
    "West",
    "North",
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
    "Right Paddle 1",
    "Left Paddle 1",
    "Right Paddle 2",
    "Left Paddle 2",
    "Touchpad",
    "Misc2",
    "Misc3",
    "Misc4",
    "Misc5",
    "Misc6",
};
SDL_COMPILE_TIME_ASSERT(gamepad_button_names, SDL_arraysize(gamepad_button_names) == SDL_GAMEPAD_BUTTON_COUNT);

static const char *gamepad_axis_names[] = {
    "LeftX",
    "LeftY",
    "RightX",
    "RightY",
    "Left Trigger",
    "Right Trigger",
};
SDL_COMPILE_TIME_ASSERT(gamepad_axis_names, SDL_arraysize(gamepad_axis_names) == SDL_GAMEPAD_AXIS_COUNT);

struct GamepadDisplay
{
    SDL_Renderer *renderer;
    SDL_Texture *button_texture;
    SDL_Texture *arrow_texture;
    float button_width;
    float button_height;
    float arrow_width;
    float arrow_height;

    float accel_data[3];
    float gyro_data[3];
    float gyro_drift_correction_data[3];

    Uint64 last_sensor_update;

    ControllerDisplayMode display_mode;
    int element_highlighted;
    bool element_pressed;
    int element_selected;

    SDL_FRect area;
};

GamepadDisplay *CreateGamepadDisplay(SDL_Renderer *renderer)
{
    GamepadDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->button_texture = CreateTexture(renderer, gamepad_button_small_bmp, gamepad_button_small_bmp_len);
        SDL_GetTextureSize(ctx->button_texture, &ctx->button_width, &ctx->button_height);

        ctx->arrow_texture = CreateTexture(renderer, gamepad_axis_arrow_bmp, gamepad_axis_arrow_bmp_len);
        SDL_GetTextureSize(ctx->arrow_texture, &ctx->arrow_width, &ctx->arrow_height);

        ctx->element_highlighted = SDL_GAMEPAD_ELEMENT_INVALID;
        ctx->element_selected = SDL_GAMEPAD_ELEMENT_INVALID;

        SDL_zeroa(ctx->accel_data);
        SDL_zeroa(ctx->gyro_data);
        SDL_zeroa(ctx->gyro_drift_correction_data);
    }
    return ctx;
}

struct GyroDisplay
{
    SDL_Renderer *renderer;

    /* Main drawing area */
    SDL_FRect area;

    /* This part displays extra info from the IMUstate in order to figure out actual polling rates. */
    float gyro_drift_solution[3];
    int reported_sensor_rate_hz;           /*hz - comes from HIDsdl implementation. Could be fixed, platform time, or true sensor time*/
    int estimated_sensor_rate_hz;          /*hz - our estimation of the actual polling rate by observing packets received*/
    float euler_displacement_angles[3];    /* pitch, yaw, roll */
    Quaternion gyro_quaternion;            /* Rotation since startup/reset, comprised of each gyro speed packet times sensor delta time. */
    float drift_calibration_progress_frac; /* [0..1] */
    float accelerometer_noise_sq;          /* Distance between last noise and new noise. Used to indicate motion.*/

    GamepadButton *reset_gyro_button;
    GamepadButton *calibrate_gyro_button;
};

GyroDisplay *CreateGyroDisplay(SDL_Renderer *renderer)
{
    GyroDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    {
        ctx->renderer = renderer;
        ctx->estimated_sensor_rate_hz = 0;
        SDL_zeroa(ctx->gyro_drift_solution);
        Quaternion quat_identity = { 0.0f, 0.0f, 0.0f, 1.0f };
        ctx->gyro_quaternion = quat_identity;

        ctx->reset_gyro_button = CreateGamepadButton(renderer, "Reset View");
        ctx->calibrate_gyro_button = CreateGamepadButton(renderer, "Recalibrate Drift");
    }

    return ctx;
}

void SetGyroDisplayArea(GyroDisplay *ctx, const SDL_FRect *area)
{
    if (!ctx) {
        return;
    }

    SDL_copyp(&ctx->area, area);

    /* Place the reset button to the bottom right of the gyro display area.*/
    SDL_FRect reset_button_area;
    reset_button_area.w = SDL_max(MINIMUM_BUTTON_WIDTH, GetGamepadButtonLabelWidth(ctx->reset_gyro_button) + 2 * BUTTON_PADDING);
    reset_button_area.h = GetGamepadButtonLabelHeight(ctx->reset_gyro_button) + BUTTON_PADDING;
    reset_button_area.x = area->x + area->w - reset_button_area.w - BUTTON_PADDING;
    reset_button_area.y = area->y + area->h - reset_button_area.h - BUTTON_PADDING;
    SetGamepadButtonArea(ctx->reset_gyro_button, &reset_button_area);
}

void SetGamepadDisplayDisplayMode(GamepadDisplay *ctx, ControllerDisplayMode display_mode)
{
    if (!ctx) {
        return;
    }

    ctx->display_mode = display_mode;
}

void SetGamepadDisplayArea(GamepadDisplay *ctx, const SDL_FRect *area)
{
    if (!ctx) {
        return;
    }

    SDL_copyp(&ctx->area, area);
}
void SetGamepadDisplayGyroDriftCorrection(GamepadDisplay *ctx, float *gyro_drift_correction)
{
    if (!ctx) {
        return;
    }

    ctx->gyro_drift_correction_data[0] = gyro_drift_correction[0];
    ctx->gyro_drift_correction_data[1] = gyro_drift_correction[1];
    ctx->gyro_drift_correction_data[2] = gyro_drift_correction[2];
}

static bool GetBindingString(const char *label, const char *mapping, char *text, size_t size)
{
    char *key;
    char *value, *end;
    size_t length;
    bool found = false;

    *text = '\0';

    if (!mapping) {
        return false;
    }

    key = SDL_strstr(mapping, label);
    while (key && size > 1) {
        if (found) {
            *text++ = ',';
            *text = '\0';
            --size;
        } else {
            found = true;
        }
        value = key + SDL_strlen(label);
        end = SDL_strchr(value, ',');
        if (end) {
            length = (end - value);
        } else {
            length = SDL_strlen(value);
        }
        if (length >= size) {
            length = size - 1;
        }
        SDL_memcpy(text, value, length);
        text[length] = '\0';
        text += length;
        size -= length;
        key = SDL_strstr(value, label);
    }
    return found;
}

static bool GetButtonBindingString(SDL_GamepadButton button, const char *mapping, char *text, size_t size)
{
    char label[32];
    bool baxy_mapping = false;

    if (!mapping) {
        return false;
    }

    SDL_snprintf(label, sizeof(label), ",%s:", SDL_GetGamepadStringForButton(button));
    if (GetBindingString(label, mapping, text, size)) {
        return true;
    }

    /* Try the legacy button names */
    if (SDL_strstr(mapping, ",hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") != NULL) {
        baxy_mapping = true;
    }
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        if (baxy_mapping) {
            return GetBindingString(",b:", mapping, text, size);
        } else {
            return GetBindingString(",a:", mapping, text, size);
        }
    case SDL_GAMEPAD_BUTTON_EAST:
        if (baxy_mapping) {
            return GetBindingString(",a:", mapping, text, size);
        } else {
            return GetBindingString(",b:", mapping, text, size);
        }
    case SDL_GAMEPAD_BUTTON_WEST:
        if (baxy_mapping) {
            return GetBindingString(",y:", mapping, text, size);
        } else {
            return GetBindingString(",x:", mapping, text, size);
        }
    case SDL_GAMEPAD_BUTTON_NORTH:
        if (baxy_mapping) {
            return GetBindingString(",x:", mapping, text, size);
        } else {
            return GetBindingString(",y:", mapping, text, size);
        }
    default:
        return false;
    }
}

static bool GetAxisBindingString(SDL_GamepadAxis axis, int direction, const char *mapping, char *text, size_t size)
{
    char label[32];

    /* Check for explicit half-axis */
    if (direction < 0) {
        SDL_snprintf(label, sizeof(label), ",-%s:", SDL_GetGamepadStringForAxis(axis));
    } else {
        SDL_snprintf(label, sizeof(label), ",+%s:", SDL_GetGamepadStringForAxis(axis));
    }
    if (GetBindingString(label, mapping, text, size)) {
        return true;
    }

    /* Get the binding for the whole axis and split it if necessary */
    SDL_snprintf(label, sizeof(label), ",%s:", SDL_GetGamepadStringForAxis(axis));
    if (!GetBindingString(label, mapping, text, size)) {
        return false;
    }
    if (axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        if (*text == 'a') {
            /* Split the axis */
            size_t length = SDL_strlen(text) + 1;
            if ((length + 1) <= size) {
                SDL_memmove(text + 1, text, length);
                if (text[SDL_strlen(text) - 1] == '~') {
                    direction *= -1;
                    text[SDL_strlen(text) - 1] = '\0';
                }
                if (direction > 0) {
                    *text = '+';
                } else {
                    *text = '-';
                }
            }
        }
    }
    return true;
}

void SetGamepadDisplayHighlight(GamepadDisplay *ctx, int element, bool pressed)
{
    if (!ctx) {
        return;
    }

    ctx->element_highlighted = element;
    ctx->element_pressed = pressed;
}

void SetGamepadDisplaySelected(GamepadDisplay *ctx, int element)
{
    if (!ctx) {
        return;
    }

    ctx->element_selected = element;
}

int GetGamepadDisplayElementAt(GamepadDisplay *ctx, SDL_Gamepad *gamepad, float x, float y)
{
    int i;
    const float margin = 8.0f;
    const float center = ctx->area.w / 2.0f;
    const float arrow_extent = 48.0f;
    SDL_FPoint point;
    SDL_FRect rect;

    if (!ctx) {
        return SDL_GAMEPAD_ELEMENT_INVALID;
    }

    point.x = x;
    point.y = y;

    rect.x = ctx->area.x + margin;
    rect.y = ctx->area.y + margin + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
    rect.w = ctx->area.w - (margin * 2);
    rect.h = ctx->button_height;

    for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
        SDL_GamepadButton button = (SDL_GamepadButton)i;

        if (ctx->display_mode == CONTROLLER_MODE_TESTING &&
            !SDL_GamepadHasButton(gamepad, button)) {
            continue;
        }


        if (SDL_PointInRectFloat(&point, &rect)) {
            return i;
        }

        rect.y += ctx->button_height + 2.0f;
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
        SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
        SDL_FRect area;

        if (ctx->display_mode == CONTROLLER_MODE_TESTING &&
            !SDL_GamepadHasAxis(gamepad, axis)) {
            continue;
        }

        area.x = rect.x + center + 2.0f;
        area.y = rect.y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
        area.w = ctx->arrow_width + arrow_extent;
        area.h = ctx->button_height;

        if (SDL_PointInRectFloat(&point, &area)) {
            switch (axis) {
            case SDL_GAMEPAD_AXIS_LEFTX:
                return SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE;
            case SDL_GAMEPAD_AXIS_LEFTY:
                return SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE;
            case SDL_GAMEPAD_AXIS_RIGHTX:
                return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE;
            case SDL_GAMEPAD_AXIS_RIGHTY:
                return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE;
            default:
                break;
            }
        }

        area.x += area.w;

        if (SDL_PointInRectFloat(&point, &area)) {
            switch (axis) {
            case SDL_GAMEPAD_AXIS_LEFTX:
                return SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE;
            case SDL_GAMEPAD_AXIS_LEFTY:
                return SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE;
            case SDL_GAMEPAD_AXIS_RIGHTX:
                return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE;
            case SDL_GAMEPAD_AXIS_RIGHTY:
                return SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE;
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                return SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER;
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                return SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER;
            default:
                break;
            }
        }

        rect.y += ctx->button_height + 2.0f;
    }
    return SDL_GAMEPAD_ELEMENT_INVALID;
}

static void RenderGamepadElementHighlight(GamepadDisplay *ctx, int element, const SDL_FRect *area)
{
    if (element == ctx->element_highlighted || element == ctx->element_selected) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

        if (element == ctx->element_highlighted) {
            if (ctx->element_pressed) {
                SDL_SetRenderDrawColor(ctx->renderer, PRESSED_COLOR);
            } else {
                SDL_SetRenderDrawColor(ctx->renderer, HIGHLIGHT_COLOR);
            }
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, SELECTED_COLOR);
        }
        SDL_RenderFillRect(ctx->renderer, area);

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
    }
}

bool BHasCachedGyroDriftSolution(GyroDisplay *ctx)
{
    if (!ctx) {
        return false;
    }
    return (ctx->gyro_drift_solution[0] != 0.0f ||
            ctx->gyro_drift_solution[1] != 0.0f ||
            ctx->gyro_drift_solution[2] != 0.0f);
}

void SetGamepadDisplayIMUValues(GyroDisplay *ctx, float *gyro_drift_solution, float *euler_displacement_angles, Quaternion *gyro_quaternion, int reported_senor_rate_hz, int estimated_sensor_rate_hz, float drift_calibration_progress_frac, float accelerometer_noise_sq)
{
    if (!ctx) {
        return;
    }

    SDL_memcpy(ctx->gyro_drift_solution, gyro_drift_solution, sizeof(ctx->gyro_drift_solution));
    ctx->estimated_sensor_rate_hz = estimated_sensor_rate_hz;

    if (reported_senor_rate_hz != 0)
        ctx->reported_sensor_rate_hz = reported_senor_rate_hz;

    SDL_memcpy(ctx->euler_displacement_angles, euler_displacement_angles, sizeof(ctx->euler_displacement_angles));
    ctx->gyro_quaternion = *gyro_quaternion;
    ctx->drift_calibration_progress_frac = drift_calibration_progress_frac;
    ctx->accelerometer_noise_sq = accelerometer_noise_sq;
}

extern GamepadButton *GetGyroResetButton(GyroDisplay *ctx)
{
    if (!ctx) {
        return NULL;
    }
    return ctx->reset_gyro_button;
}

extern GamepadButton *GetGyroCalibrateButton(GyroDisplay *ctx)
{
    if (!ctx) {
        return NULL;
    }
    return ctx->calibrate_gyro_button;
}

void RenderGamepadDisplay(GamepadDisplay *ctx, SDL_Gamepad *gamepad)
{
    float x, y;
    int i;
    char text[128], binding[32];
    const float margin = 8.0f;
    const float center = ctx->area.w / 2.0f;
    const float arrow_extent = 48.0f;
    SDL_FRect dst, rect, highlight;
    Uint8 r, g, b, a;
    char *mapping;
    bool has_accel;
    bool has_gyro;

    if (!ctx) {
        return;
    }

    SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

    mapping = SDL_GetGamepadMapping(gamepad);

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
        SDL_GamepadButton button = (SDL_GamepadButton)i;

        if (ctx->display_mode == CONTROLLER_MODE_TESTING &&
            !SDL_GamepadHasButton(gamepad, button)) {
            continue;
        }

        highlight.x = x;
        highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
        highlight.w = ctx->area.w - (margin * 2);
        highlight.h = ctx->button_height;
        RenderGamepadElementHighlight(ctx, i, &highlight);

        SDL_snprintf(text, sizeof(text), "%s:", gamepad_button_names[i]);
        SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);

        if (SDL_GetGamepadButton(gamepad, button)) {
            SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
        } else {
            SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
        }

        dst.x = x + center + 2.0f;
        dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
        dst.w = ctx->button_width;
        dst.h = ctx->button_height;
        SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

        if (ctx->display_mode == CONTROLLER_MODE_BINDING) {
            if (GetButtonBindingString(button, mapping, binding, sizeof(binding))) {
                dst.x += dst.w + 2 * margin;
                SDLTest_DrawString(ctx->renderer, dst.x, y, binding);
            }
        }

        y += ctx->button_height + 2.0f;
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
        SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
        bool has_negative = (axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        Sint16 value;

        if (ctx->display_mode == CONTROLLER_MODE_TESTING &&
            !SDL_GamepadHasAxis(gamepad, axis)) {
            continue;
        }

        value = SDL_GetGamepadAxis(gamepad, axis);

        SDL_snprintf(text, sizeof(text), "%s:", gamepad_axis_names[i]);
        SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);

        highlight.x = x + center + 2.0f;
        highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
        highlight.w = ctx->arrow_width + arrow_extent;
        highlight.h = ctx->button_height;

        switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE, &highlight);
            break;
        default:
            break;
        }

        highlight.x += highlight.w;

        switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER, &highlight);
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            RenderGamepadElementHighlight(ctx, SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER, &highlight);
            break;
        default:
            break;
        }

        dst.x = x + center + 2.0f;
        dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->arrow_height / 2;
        dst.w = ctx->arrow_width;
        dst.h = ctx->arrow_height;

        if (has_negative) {
            if (value == SDL_MIN_SINT16) {
                SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
            }
            SDL_RenderTextureRotated(ctx->renderer, ctx->arrow_texture, NULL, &dst, 0.0f, NULL, SDL_FLIP_HORIZONTAL);
        }

        dst.x += ctx->arrow_width;

        SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
        rect.x = dst.x + arrow_extent - 2.0f;
        rect.y = dst.y;
        rect.w = 4.0f;
        rect.h = ctx->arrow_height;
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

        if (ctx->display_mode == CONTROLLER_MODE_BINDING && has_negative) {
            if (GetAxisBindingString(axis, -1, mapping, binding, sizeof(binding))) {
                float text_x;

                SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
                text_x = dst.x + arrow_extent / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(binding)) / 2;
                SDLTest_DrawString(ctx->renderer, text_x, y, binding);
            }
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

        if (ctx->display_mode == CONTROLLER_MODE_BINDING) {
            if (GetAxisBindingString(axis, 1, mapping, binding, sizeof(binding))) {
                float text_x;

                SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
                text_x = dst.x + arrow_extent / 2 - (FONT_CHARACTER_SIZE * SDL_strlen(binding)) / 2;
                SDLTest_DrawString(ctx->renderer, text_x, y, binding);
            }
        }

        dst.x += arrow_extent;

        if (value == SDL_MAX_SINT16) {
            SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
        } else {
            SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
        }
        SDL_RenderTexture(ctx->renderer, ctx->arrow_texture, NULL, &dst);

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);

        y += ctx->button_height + 2.0f;
    }

    if (ctx->display_mode == CONTROLLER_MODE_TESTING) {
        if (SDL_GetNumGamepadTouchpads(gamepad) > 0) {
            int num_fingers = SDL_GetNumGamepadTouchpadFingers(gamepad, 0);
            for (i = 0; i < num_fingers; ++i) {
                bool down;
                float finger_x, finger_y, finger_pressure;

                if (!SDL_GetGamepadTouchpadFinger(gamepad, 0, i, &down, &finger_x, &finger_y, &finger_pressure)) {
                    continue;
                }

                SDL_snprintf(text, sizeof(text), "Touch finger %d:", i);
                SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);

                if (down) {
                    SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
                } else {
                    SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
                }

                dst.x = x + center + 2.0f;
                dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
                dst.w = ctx->button_width;
                dst.h = ctx->button_height;
                SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

                if (down) {
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
                SDL_snprintf(text, sizeof(text), "[%.2f,%.2f,%.2f]m/s%s", ctx->accel_data[0], ctx->accel_data[1], ctx->accel_data[2], SQUARED_UTF8 );
                SDLTest_DrawString(ctx->renderer, x + center + 2.0f, y, text);
                y += ctx->button_height + 2.0f;
            }

            if (has_gyro) {
                SDL_strlcpy(text, "Gyro:", sizeof(text));
                SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);
                SDL_snprintf(text, sizeof(text), "[%.2f,%.2f,%.2f]%s/s", ctx->gyro_data[0] * RAD_TO_DEG, ctx->gyro_data[1] * RAD_TO_DEG, ctx->gyro_data[2] * RAD_TO_DEG, DEGREE_UTF8);
                SDLTest_DrawString(ctx->renderer, x + center + 2.0f, y, text);


                /* Display the testcontroller tool's evaluation of drift. This is also useful to get an average rate of turn in calibrated turntable tests. */
                if (ctx->gyro_drift_correction_data[0] != 0.0f && ctx->gyro_drift_correction_data[2] != 0.0f && ctx->gyro_drift_correction_data[2] != 0.0f )
                {
                    y += ctx->button_height + 2.0f;
                    SDL_strlcpy(text, "Gyro Drift:", sizeof(text));
                    SDLTest_DrawString(ctx->renderer, x + center - SDL_strlen(text) * FONT_CHARACTER_SIZE, y, text);
                    SDL_snprintf(text, sizeof(text), "[%.2f,%.2f,%.2f]%s/s", ctx->gyro_drift_correction_data[0] * RAD_TO_DEG, ctx->gyro_drift_correction_data[1] * RAD_TO_DEG, ctx->gyro_drift_correction_data[2] * RAD_TO_DEG, DEGREE_UTF8);
                    SDLTest_DrawString(ctx->renderer, x + center + 2.0f, y, text);
                }

            }


        }
    }
    SDL_free(mapping);
}

void DestroyGamepadDisplay(GamepadDisplay *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_DestroyTexture(ctx->button_texture);
    SDL_DestroyTexture(ctx->arrow_texture);
    SDL_free(ctx);
}

void RenderSensorTimingInfo(GyroDisplay *ctx, GamepadDisplay *gamepad_display)
{
    /* Sensor timing section */
    char text[128];
    const float new_line_height = gamepad_display->button_height + 2.0f;
    const float text_offset_x = ctx->area.x + ctx->area.w / 4.0f + 40.0f;
    /* Anchor to bottom left of principle rect. */
    float text_y_pos = ctx->area.y + ctx->area.h - new_line_height * 2;
    /*
     * Display rate of gyro as reported by the HID implementation.
     * This could be based on a hardware time stamp (PS5), or it could be generated by the HID implementation.
     * One should expect this to match the estimated rate below, assuming a wired connection.
     */

    SDL_strlcpy(text, "HID Sensor Time:", sizeof(text));
    SDLTest_DrawString(ctx->renderer, text_offset_x - SDL_strlen(text) * FONT_CHARACTER_SIZE, text_y_pos, text);
    if (ctx->reported_sensor_rate_hz > 0) {
        /* Convert to micro seconds */
        const int delta_time_us = (int)1e6 / ctx->reported_sensor_rate_hz;
        SDL_snprintf(text, sizeof(text), "%d%ss %dhz", delta_time_us, MICRO_UTF8, ctx->reported_sensor_rate_hz);
    } else {
        SDL_snprintf(text, sizeof(text), "????%ss ???hz", MICRO_UTF8);
    }
    SDLTest_DrawString(ctx->renderer, text_offset_x + 2.0f, text_y_pos, text);

    /*
     * Display the instrumentation's count of all sensor packets received over time.
     * This may represent a more accurate polling rate for the IMU
     * But only when using a wired connection.
     * It does not necessarily reflect the rate at which the IMU is sampled.
     */

    text_y_pos += new_line_height;
    SDL_strlcpy(text, "Est.Sensor Time:", sizeof(text));
    SDLTest_DrawString(ctx->renderer, text_offset_x - SDL_strlen(text) * FONT_CHARACTER_SIZE, text_y_pos, text);
    if (ctx->estimated_sensor_rate_hz > 0) {
        /* Convert to micro seconds */
        const int delta_time_us = (int)1e6 / ctx->estimated_sensor_rate_hz;
        SDL_snprintf(text, sizeof(text), "%d%ss %dhz", delta_time_us, MICRO_UTF8, ctx->estimated_sensor_rate_hz);
    } else {
        SDL_snprintf(text, sizeof(text), "????%ss ???hz", MICRO_UTF8);
    }
    SDLTest_DrawString(ctx->renderer, text_offset_x + 2.0f, text_y_pos, text);
}

void RenderGyroDriftCalibrationButton(GyroDisplay *ctx, GamepadDisplay *gamepad_display )
{
    char label_text[128];
    float log_y = ctx->area.y + BUTTON_PADDING;
    const float new_line_height = gamepad_display->button_height + 2.0f;
    GamepadButton *start_calibration_button = GetGyroCalibrateButton(ctx);
    bool bHasCachedDriftSolution = BHasCachedGyroDriftSolution(ctx);

    /* Show the recalibration progress bar. */
    float recalibrate_button_width = GetGamepadButtonLabelWidth(start_calibration_button) + 2 * BUTTON_PADDING;
    SDL_FRect recalibrate_button_area;
    recalibrate_button_area.x = ctx->area.x + ctx->area.w - recalibrate_button_width - BUTTON_PADDING;
    recalibrate_button_area.y = log_y + FONT_CHARACTER_SIZE * 0.5f - gamepad_display->button_height * 0.5f;
    recalibrate_button_area.w = GetGamepadButtonLabelWidth(start_calibration_button) + 2.0f * BUTTON_PADDING;
    recalibrate_button_area.h = gamepad_display->button_height + BUTTON_PADDING * 2.0f;

     if (!bHasCachedDriftSolution) {
        SDL_snprintf(label_text, sizeof(label_text), "Progress: %3.0f%% ", ctx->drift_calibration_progress_frac * 100.0f);
    } else {
         SDL_strlcpy(label_text, "Calibrate Drift", sizeof(label_text));
    }

    SetGamepadButtonLabel(start_calibration_button, label_text);
    SetGamepadButtonArea(start_calibration_button, &recalibrate_button_area);
    RenderGamepadButton(start_calibration_button);

    /* Above button */
    SDL_strlcpy(label_text, "Gyro Orientation:", sizeof(label_text));
    SDLTest_DrawString(ctx->renderer, recalibrate_button_area.x, recalibrate_button_area.y - new_line_height, label_text);

    if (!bHasCachedDriftSolution) {

        float flNoiseFraction = SDL_clamp(SDL_sqrtf(ctx->accelerometer_noise_sq) / ACCELEROMETER_NOISE_THRESHOLD, 0.0f, 1.0f);
        bool bTooMuchNoise = (flNoiseFraction == 1.0f);

        float noise_bar_height = gamepad_display->button_height;
        SDL_FRect noise_bar_rect;
        noise_bar_rect.x = recalibrate_button_area.x;
        noise_bar_rect.y = recalibrate_button_area.y + recalibrate_button_area.h + BUTTON_PADDING;
        noise_bar_rect.w = recalibrate_button_area.w;
        noise_bar_rect.h = noise_bar_height;

        /* Adjust the noise bar rectangle based on the accelerometer noise value */

        float noise_bar_fill_width = flNoiseFraction * noise_bar_rect.w; /* Scale the width based on the noise value */
        SDL_FRect noise_bar_fill_rect;
        noise_bar_fill_rect.x = noise_bar_rect.x + (noise_bar_rect.w - noise_bar_fill_width) * 0.5f;
        noise_bar_fill_rect.y = noise_bar_rect.y;
        noise_bar_fill_rect.w = noise_bar_fill_width;
        noise_bar_fill_rect.h = noise_bar_height;

        /* Set the color based on the noise value */
        Uint8 red = (Uint8)(flNoiseFraction * 255.0f);
        Uint8 green = (Uint8)((1.0f - flNoiseFraction) * 255.0f);
        SDL_SetRenderDrawColor(ctx->renderer, red, green, 0, 255); /* red when high noise, green when low noise */
        SDL_RenderFillRect(ctx->renderer, &noise_bar_fill_rect);   /* draw the filled rectangle */

        SDL_SetRenderDrawColor(ctx->renderer, 100, 100, 100, 255); /* gray box */
        SDL_RenderRect(ctx->renderer, &noise_bar_rect);            /* draw the outline rectangle */

        /* Explicit warning message if we detect too much movement */
        if (bTooMuchNoise) {
            SDL_strlcpy(label_text, "Place GamePad Down!", sizeof(label_text));
            SDLTest_DrawString(ctx->renderer, recalibrate_button_area.x, noise_bar_rect.y + noise_bar_rect.h + new_line_height, label_text);
        }

        /* Drift progress bar */
        /* Demonstrate how far we are through the drift progress, and how it resets when there's "high noise", i.e if flNoiseFraction == 1.0f */
        SDL_FRect progress_bar_rect;
        progress_bar_rect.x = recalibrate_button_area.x + BUTTON_PADDING;
        progress_bar_rect.y = recalibrate_button_area.y + recalibrate_button_area.h * 0.5f + BUTTON_PADDING * 0.5f;
        progress_bar_rect.w = recalibrate_button_area.w - BUTTON_PADDING * 2.0f;
        progress_bar_rect.h = BUTTON_PADDING * 0.5f;

        /* Adjust the drift bar rectangle based on the drift calibration progress fraction */
        float drift_bar_fill_width = bTooMuchNoise ? 1.0f : ctx->drift_calibration_progress_frac * progress_bar_rect.w;
        SDL_FRect progress_bar_fill;
        progress_bar_fill.x = progress_bar_rect.x;
        progress_bar_fill.y = progress_bar_rect.y;
        progress_bar_fill.w = drift_bar_fill_width;
        progress_bar_fill.h = progress_bar_rect.h;

        /* Set the color based on the drift calibration progress fraction */
        SDL_SetRenderDrawColor(ctx->renderer, GYRO_COLOR_GREEN);        /* red when too much noise, green when low noise*/

        /* Now draw the bars with the filled, then empty rectangles */
        SDL_RenderFillRect(ctx->renderer, &progress_bar_fill);          /* draw the filled rectangle*/
        SDL_SetRenderDrawColor(ctx->renderer, 100, 100, 100, 255);      /* gray box*/
        SDL_RenderRect(ctx->renderer, &progress_bar_rect);              /* draw the outline rectangle*/

        /* If there is too much movement, we are going to draw two diagonal red lines between the progress rect corners.*/
        if (bTooMuchNoise) {
            SDL_SetRenderDrawColor(ctx->renderer, GYRO_COLOR_RED);      /* red */
            SDL_RenderFillRect(ctx->renderer, &progress_bar_fill);      /* draw the filled rectangle */
        }
    }
}

float RenderEulerReadout(GyroDisplay *ctx, GamepadDisplay *gamepad_display )
{
    /* Get the mater button's width and base our width off that */
    GamepadButton *master_button = GetGyroCalibrateButton(ctx);
    SDL_FRect gyro_calibrate_button_rect;
    GetGamepadButtonArea(master_button, &gyro_calibrate_button_rect);

    char text[128];
    float log_y = gyro_calibrate_button_rect.y + gyro_calibrate_button_rect.h + BUTTON_PADDING;
    const float new_line_height = gamepad_display->button_height + 2.0f;
    float log_gyro_euler_text_x = gyro_calibrate_button_rect.x;

    /* Pitch Readout */
    SDL_snprintf(text, sizeof(text), "Pitch: %6.2f%s", ctx->euler_displacement_angles[0], DEGREE_UTF8);
    SDLTest_DrawString(ctx->renderer, log_gyro_euler_text_x + 2.0f, log_y, text);

    /* Yaw Readout */
    log_y += new_line_height;
    SDL_snprintf(text, sizeof(text), "  Yaw: %6.2f%s", ctx->euler_displacement_angles[1], DEGREE_UTF8);
    SDLTest_DrawString(ctx->renderer, log_gyro_euler_text_x + 2.0f, log_y, text);

    /* Roll Readout */
    log_y += new_line_height;
    SDL_snprintf(text, sizeof(text), " Roll: %6.2f%s", ctx->euler_displacement_angles[2], DEGREE_UTF8);
    SDLTest_DrawString(ctx->renderer, log_gyro_euler_text_x + 2.0f, log_y, text);

    return log_y + new_line_height; /* Return the next y position for further rendering */
}

/* Draws the 3D cube, circles and accel arrow, positioning itself relative to the calibrate button. */
void RenderGyroGizmo(GyroDisplay *ctx, SDL_Gamepad *gamepad, float top)
{
    /* Get the calibrate button's on-screen area: */
    GamepadButton *btn = GetGyroCalibrateButton(ctx);
    SDL_FRect btnArea;
    GetGamepadButtonArea(btn, &btnArea);

    float gizmoSize = btnArea.w;
    /* Position it centered horizontally above the button with a small gap */
    SDL_FRect gizmoRect;
    gizmoRect.x = btnArea.x + (btnArea.w - gizmoSize) * 0.5f;
    gizmoRect.y = top;
    gizmoRect.w = gizmoSize;
    gizmoRect.h = gizmoSize;

    /* Draw the rotated cube */
    DrawGyroDebugCube(ctx->renderer, &ctx->gyro_quaternion, &gizmoRect);

    /* Overlay the XYZ circles */
    DrawGyroDebugCircle(ctx->renderer, &ctx->gyro_quaternion, &gizmoRect);

    /* If we have accel, draw that arrow too */
    if (SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL)) {
        float accel[3];
        SDL_GetGamepadSensorData(gamepad, SDL_SENSOR_ACCEL, accel, SDL_arraysize(accel));
        DrawAccelerometerDebugArrow(ctx->renderer, &ctx->gyro_quaternion, accel, &gizmoRect);
    }

    /* Follow the size of the main button, but position it below the gizmo */
    GamepadButton *reset_button = GetGyroResetButton(ctx);
    if (reset_button) {
        SDL_FRect reset_area;
        GetGamepadButtonArea(reset_button, &reset_area);
        /* Position the reset button below the gizmo */
        reset_area.x = btnArea.x;
        reset_area.y = gizmoRect.y + gizmoRect.h + BUTTON_PADDING * 0.5f;
        reset_area.w = btnArea.w;
        reset_area.h = btnArea.h;
        SetGamepadButtonArea(reset_button, &reset_area);
        RenderGamepadButton(reset_button);
    }
}

void RenderGyroDisplay(GyroDisplay *ctx, GamepadDisplay *gamepadElements, SDL_Gamepad *gamepad)
{
    if (!ctx)
        return;

    bool bHasAccelerometer = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL);
    bool bHasGyroscope = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO);
    bool bHasIMU = bHasAccelerometer || bHasGyroscope;
    if (!bHasIMU)
        return;

    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

    RenderSensorTimingInfo(ctx, gamepadElements);

    RenderGyroDriftCalibrationButton(ctx, gamepadElements);

    bool bHasCachedDriftSolution = BHasCachedGyroDriftSolution(ctx);
    if (bHasCachedDriftSolution) {
        float bottom = RenderEulerReadout(ctx, gamepadElements);
        RenderGyroGizmo(ctx, gamepad, bottom);

    }
    SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
}

void DestroyGyroDisplay(GyroDisplay *ctx)
{
    if (!ctx) {
        return;
    }
    DestroyGamepadButton(ctx->reset_gyro_button);
    DestroyGamepadButton(ctx->calibrate_gyro_button);
    SDL_free(ctx);
}


struct GamepadTypeDisplay
{
    SDL_Renderer *renderer;

    int type_highlighted;
    bool type_pressed;
    int type_selected;
    SDL_GamepadType real_type;

    SDL_FRect area;
};

GamepadTypeDisplay *CreateGamepadTypeDisplay(SDL_Renderer *renderer)
{
    GamepadTypeDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->type_highlighted = SDL_GAMEPAD_TYPE_UNSELECTED;
        ctx->type_selected = SDL_GAMEPAD_TYPE_UNSELECTED;
        ctx->real_type = SDL_GAMEPAD_TYPE_UNKNOWN;
    }
    return ctx;
}

void SetGamepadTypeDisplayArea(GamepadTypeDisplay *ctx, const SDL_FRect *area)
{
    if (!ctx) {
        return;
    }

    SDL_copyp(&ctx->area, area);
}

void SetGamepadTypeDisplayHighlight(GamepadTypeDisplay *ctx, int type, bool pressed)
{
    if (!ctx) {
        return;
    }

    ctx->type_highlighted = type;
    ctx->type_pressed = pressed;
}

void SetGamepadTypeDisplaySelected(GamepadTypeDisplay *ctx, int type)
{
    if (!ctx) {
        return;
    }

    ctx->type_selected = type;
}

void SetGamepadTypeDisplayRealType(GamepadTypeDisplay *ctx, SDL_GamepadType type)
{
    if (!ctx) {
        return;
    }

    ctx->real_type = type;
}

int GetGamepadTypeDisplayAt(GamepadTypeDisplay *ctx, float x, float y)
{
    int i;
    const float margin = 8.0f;
    const float line_height = 16.0f;
    SDL_FRect highlight;
    SDL_FPoint point;

    if (!ctx) {
        return SDL_GAMEPAD_TYPE_UNSELECTED;
    }

    point.x = x;
    point.y = y;

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    for (i = SDL_GAMEPAD_TYPE_UNKNOWN; i < SDL_GAMEPAD_TYPE_COUNT; ++i) {
        highlight.x = x;
        highlight.y = y;
        highlight.w = ctx->area.w - (margin * 2);
        highlight.h = line_height;

        if (SDL_PointInRectFloat(&point, &highlight)) {
            return i;
        }

        y += line_height;
    }
    return SDL_GAMEPAD_TYPE_UNSELECTED;
}

static void RenderGamepadTypeHighlight(GamepadTypeDisplay *ctx, int type, const SDL_FRect *area)
{
    if (type == ctx->type_highlighted || type == ctx->type_selected) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

        if (type == ctx->type_highlighted) {
            if (ctx->type_pressed) {
                SDL_SetRenderDrawColor(ctx->renderer, PRESSED_COLOR);
            } else {
                SDL_SetRenderDrawColor(ctx->renderer, HIGHLIGHT_COLOR);
            }
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, SELECTED_COLOR);
        }
        SDL_RenderFillRect(ctx->renderer, area);

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
    }
}

void RenderGamepadTypeDisplay(GamepadTypeDisplay *ctx)
{
    float x, y;
    int i;
    char text[128];
    const float margin = 8.0f;
    const float line_height = 16.0f;
    SDL_FPoint dst;
    SDL_FRect highlight;

    if (!ctx) {
        return;
    }

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    for (i = SDL_GAMEPAD_TYPE_UNKNOWN; i < SDL_GAMEPAD_TYPE_COUNT; ++i) {
        highlight.x = x;
        highlight.y = y;
        highlight.w = ctx->area.w - (margin * 2);
        highlight.h = line_height;
        RenderGamepadTypeHighlight(ctx, i, &highlight);

        if (i == SDL_GAMEPAD_TYPE_UNKNOWN) {
            if (ctx->real_type == SDL_GAMEPAD_TYPE_UNKNOWN ||
                ctx->real_type == SDL_GAMEPAD_TYPE_STANDARD) {
                SDL_strlcpy(text, "Auto (Standard)", sizeof(text));
            } else {
                SDL_snprintf(text, sizeof(text), "Auto (%s)", GetGamepadTypeString(ctx->real_type));
            }
        } else if (i == SDL_GAMEPAD_TYPE_STANDARD) {
            SDL_strlcpy(text, "Standard", sizeof(text));
        } else {
            SDL_strlcpy(text, GetGamepadTypeString((SDL_GamepadType)i), sizeof(text));
        }

        dst.x = x + margin;
        dst.y = y + line_height / 2 - FONT_CHARACTER_SIZE / 2;
        SDLTest_DrawString(ctx->renderer, dst.x, dst.y, text);

        y += line_height;
    }
}

void DestroyGamepadTypeDisplay(GamepadTypeDisplay *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_free(ctx);
}


struct JoystickDisplay
{
    SDL_Renderer *renderer;
    SDL_Texture *button_texture;
    SDL_Texture *arrow_texture;
    float button_width;
    float button_height;
    float arrow_width;
    float arrow_height;

    SDL_FRect area;

    char *element_highlighted;
    bool element_pressed;
};

JoystickDisplay *CreateJoystickDisplay(SDL_Renderer *renderer)
{
    JoystickDisplay *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->button_texture = CreateTexture(renderer, gamepad_button_small_bmp, gamepad_button_small_bmp_len);
        SDL_GetTextureSize(ctx->button_texture, &ctx->button_width, &ctx->button_height);

        ctx->arrow_texture = CreateTexture(renderer, gamepad_axis_arrow_bmp, gamepad_axis_arrow_bmp_len);
        SDL_GetTextureSize(ctx->arrow_texture, &ctx->arrow_width, &ctx->arrow_height);
    }
    return ctx;
}

void SetJoystickDisplayArea(JoystickDisplay *ctx, const SDL_FRect *area)
{
    if (!ctx) {
        return;
    }

    SDL_copyp(&ctx->area, area);
}

char *GetJoystickDisplayElementAt(JoystickDisplay *ctx, SDL_Joystick *joystick, float x, float y)
{
    SDL_FPoint point;
    int i;
    int nbuttons = SDL_GetNumJoystickButtons(joystick);
    int naxes = SDL_GetNumJoystickAxes(joystick);
    int nhats = SDL_GetNumJoystickHats(joystick);
    char text[32];
    const float margin = 8.0f;
    const float center = 80.0f;
    const float arrow_extent = 48.0f;
    SDL_FRect dst, highlight;
    char *element = NULL;

    if (!ctx) {
        return NULL;
    }

    point.x = x;
    point.y = y;

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    if (nbuttons > 0) {
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < nbuttons; ++i) {
            highlight.x = x;
            highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            highlight.w = center - (margin * 2);
            highlight.h = ctx->button_height;
            if (SDL_PointInRectFloat(&point, &highlight)) {
                SDL_asprintf(&element, "b%d", i);
                return element;
            }

            y += ctx->button_height + 2;
        }
    }

    x = ctx->area.x + margin + center + margin;
    y = ctx->area.y + margin;

    if (naxes > 0) {
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < naxes; ++i) {
            SDL_snprintf(text, sizeof(text), "%d:", i);

            highlight.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2.0f;
            highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            highlight.w = ctx->arrow_width + arrow_extent;
            highlight.h = ctx->button_height;
            if (SDL_PointInRectFloat(&point, &highlight)) {
                SDL_asprintf(&element, "-a%d", i);
                return element;
            }

            highlight.x += highlight.w;
            if (SDL_PointInRectFloat(&point, &highlight)) {
                SDL_asprintf(&element, "+a%d", i);
                return element;
            }

            y += ctx->button_height + 2;
        }
    }

    y += FONT_LINE_HEIGHT + 2;

    if (nhats > 0) {
        y += FONT_LINE_HEIGHT + 2 + 1.5f * ctx->button_height - FONT_CHARACTER_SIZE / 2;

        for (i = 0; i < nhats; ++i) {
            SDL_snprintf(text, sizeof(text), "%d:", i);

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = ctx->button_width;
            dst.h = ctx->button_height;
            if (SDL_PointInRectFloat(&point, &dst)) {
                SDL_asprintf(&element, "h%d.%d", i, SDL_HAT_LEFT);
                return element;
            }

            dst.x += ctx->button_width;
            dst.y -= ctx->button_height;
            if (SDL_PointInRectFloat(&point, &dst)) {
                SDL_asprintf(&element, "h%d.%d", i, SDL_HAT_UP);
                return element;
            }

            dst.y += ctx->button_height * 2;
            if (SDL_PointInRectFloat(&point, &dst)) {
                SDL_asprintf(&element, "h%d.%d", i, SDL_HAT_DOWN);
                return element;
            }

            dst.x += ctx->button_width;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            if (SDL_PointInRectFloat(&point, &dst)) {
                SDL_asprintf(&element, "h%d.%d", i, SDL_HAT_RIGHT);
                return element;
            }

            y += 3 * ctx->button_height + 2;
        }
    }
    return NULL;
}

void SetJoystickDisplayHighlight(JoystickDisplay *ctx, const char *element, bool pressed)
{
    if (ctx->element_highlighted) {
        SDL_free(ctx->element_highlighted);
        ctx->element_highlighted = NULL;
        ctx->element_pressed = false;
    }

    if (element) {
        ctx->element_highlighted = SDL_strdup(element);
        ctx->element_pressed = pressed;
    }
}

static void RenderJoystickButtonHighlight(JoystickDisplay *ctx, int button, const SDL_FRect *area)
{
    if (!ctx->element_highlighted || *ctx->element_highlighted != 'b') {
        return;
    }

    if (SDL_atoi(ctx->element_highlighted + 1) == button) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

        if (ctx->element_pressed) {
            SDL_SetRenderDrawColor(ctx->renderer, PRESSED_COLOR);
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, HIGHLIGHT_COLOR);
        }
        SDL_RenderFillRect(ctx->renderer, area);

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
    }
}

static void RenderJoystickAxisHighlight(JoystickDisplay *ctx, int axis, int direction, const SDL_FRect *area)
{
    char prefix = (direction < 0 ? '-' : '+');

    if (!ctx->element_highlighted ||
        ctx->element_highlighted[0] != prefix ||
        ctx->element_highlighted[1] != 'a') {
        return;
    }

    if (SDL_atoi(ctx->element_highlighted + 2) == axis) {
        Uint8 r, g, b, a;

        SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

        if (ctx->element_pressed) {
            SDL_SetRenderDrawColor(ctx->renderer, PRESSED_COLOR);
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, HIGHLIGHT_COLOR);
        }
        SDL_RenderFillRect(ctx->renderer, area);

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
    }
}

static bool SetupJoystickHatHighlight(JoystickDisplay *ctx, int hat, int direction)
{
    if (!ctx->element_highlighted || *ctx->element_highlighted != 'h') {
        return false;
    }

    if (SDL_atoi(ctx->element_highlighted + 1) == hat &&
        ctx->element_highlighted[2] == '.' &&
        SDL_atoi(ctx->element_highlighted + 3) == direction) {
        if (ctx->element_pressed) {
            SDL_SetTextureColorMod(ctx->button_texture, PRESSED_TEXTURE_MOD);
        } else {
            SDL_SetTextureColorMod(ctx->button_texture, HIGHLIGHT_TEXTURE_MOD);
        }
        return true;
    }
    return false;
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
    SDL_FRect dst, rect, highlight;
    Uint8 r, g, b, a;

    if (!ctx) {
        return;
    }

    SDL_GetRenderDrawColor(ctx->renderer, &r, &g, &b, &a);

    x = ctx->area.x + margin;
    y = ctx->area.y + margin;

    if (nbuttons > 0) {
        SDLTest_DrawString(ctx->renderer, x, y, "BUTTONS");
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < nbuttons; ++i) {
            highlight.x = x;
            highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            highlight.w = center - (margin * 2);
            highlight.h = ctx->button_height;
            RenderJoystickButtonHighlight(ctx, i, &highlight);

            SDL_snprintf(text, sizeof(text), "%2d:", i);
            SDLTest_DrawString(ctx->renderer, x, y, text);

            if (SDL_GetJoystickButton(joystick, (Uint8)i)) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = ctx->button_width;
            dst.h = ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            y += ctx->button_height + 2;
        }
    }

    x = ctx->area.x + margin + center + margin;
    y = ctx->area.y + margin;

    if (naxes > 0) {
        SDLTest_DrawString(ctx->renderer, x, y, "AXES");
        y += FONT_LINE_HEIGHT + 2;

        for (i = 0; i < naxes; ++i) {
            Sint16 value = SDL_GetJoystickAxis(joystick, i);

            SDL_snprintf(text, sizeof(text), "%d:", i);
            SDLTest_DrawString(ctx->renderer, x, y, text);

            highlight.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2.0f;
            highlight.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            highlight.w = ctx->arrow_width + arrow_extent;
            highlight.h = ctx->button_height;
            RenderJoystickAxisHighlight(ctx, i, -1, &highlight);

            highlight.x += highlight.w;
            RenderJoystickAxisHighlight(ctx, i, 1, &highlight);

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2.0f;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->arrow_height / 2;
            dst.w = ctx->arrow_width;
            dst.h = ctx->arrow_height;

            if (value == SDL_MIN_SINT16) {
                SDL_SetTextureColorMod(ctx->arrow_texture, 10, 255, 21);
            } else {
                SDL_SetTextureColorMod(ctx->arrow_texture, 255, 255, 255);
            }
            SDL_RenderTextureRotated(ctx->renderer, ctx->arrow_texture, NULL, &dst, 0.0f, NULL, SDL_FLIP_HORIZONTAL);

            dst.x += ctx->arrow_width;

            SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
            rect.x = dst.x + arrow_extent - 2.0f;
            rect.y = dst.y;
            rect.w = 4.0f;
            rect.h = ctx->arrow_height;
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
            } else if (!SetupJoystickHatHighlight(ctx, i, SDL_HAT_LEFT)) {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x = x + FONT_CHARACTER_SIZE * SDL_strlen(text) + 2;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            dst.w = ctx->button_width;
            dst.h = ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_UP) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else if (!SetupJoystickHatHighlight(ctx, i, SDL_HAT_UP)) {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x += ctx->button_width;
            dst.y -= ctx->button_height;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_DOWN) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else if (!SetupJoystickHatHighlight(ctx, i, SDL_HAT_DOWN)) {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.y += ctx->button_height * 2;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            if (value & SDL_HAT_RIGHT) {
                SDL_SetTextureColorMod(ctx->button_texture, 10, 255, 21);
            } else if (!SetupJoystickHatHighlight(ctx, i, SDL_HAT_RIGHT)) {
                SDL_SetTextureColorMod(ctx->button_texture, 255, 255, 255);
            }

            dst.x += ctx->button_width;
            dst.y = y + FONT_CHARACTER_SIZE / 2 - ctx->button_height / 2;
            SDL_RenderTexture(ctx->renderer, ctx->button_texture, NULL, &dst);

            y += 3 * ctx->button_height + 2;
        }
    }
}

void DestroyJoystickDisplay(JoystickDisplay *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_DestroyTexture(ctx->button_texture);
    SDL_DestroyTexture(ctx->arrow_texture);
    SDL_free(ctx);
}


struct GamepadButton
{
    SDL_Renderer *renderer;
    SDL_Texture *background;
    float background_width;
    float background_height;

    SDL_FRect area;

    char *label;
    float label_width;
    float label_height;

    bool highlight;
    bool pressed;
};

GamepadButton *CreateGamepadButton(SDL_Renderer *renderer, const char *label)
{
    GamepadButton *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->renderer = renderer;

        ctx->background = CreateTexture(renderer, gamepad_button_background_bmp, gamepad_button_background_bmp_len);
        SDL_GetTextureSize(ctx->background, &ctx->background_width, &ctx->background_height);

        SetGamepadButtonLabel(ctx, label);
    }
    return ctx;
}

void SetGamepadButtonLabel(GamepadButton *ctx, const char *label)
{
    if (!ctx) {
        return;
    }

    if (ctx->label) {
        SDL_free(ctx->label);
    }

    ctx->label = SDL_strdup(label);
    ctx->label_width = (float)(FONT_CHARACTER_SIZE * SDL_strlen(label));
    ctx->label_height = (float)FONT_CHARACTER_SIZE;
}
void SetGamepadButtonArea(GamepadButton *ctx, const SDL_FRect *area)
{
    if (!ctx) {
        return;
    }

    SDL_copyp(&ctx->area, area);
}

void GetGamepadButtonArea(GamepadButton *ctx, SDL_FRect *area)
{
    if (!ctx) {
        SDL_zerop(area);
        return;
    }

    SDL_copyp(area, &ctx->area);
}

void SetGamepadButtonHighlight(GamepadButton *ctx, bool highlight, bool pressed)
{
    if (!ctx) {
        return;
    }

    ctx->highlight = highlight;
    if (highlight) {
        ctx->pressed = pressed;
    } else {
        ctx->pressed = false;
    }
}

float GetGamepadButtonLabelWidth(GamepadButton *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->label_width;
}

float GetGamepadButtonLabelHeight(GamepadButton *ctx)
{
    if (!ctx) {
        return 0;
    }

    return ctx->label_height;
}

bool GamepadButtonContains(GamepadButton *ctx, float x, float y)
{
    SDL_FPoint point;

    if (!ctx) {
        return false;
    }

    point.x = x;
    point.y = y;
    return SDL_PointInRectFloat(&point, &ctx->area);
}

void RenderGamepadButton(GamepadButton *ctx)
{
    SDL_FRect src, dst;
    float one_third_src_width;
    float one_third_src_height;

    if (!ctx) {
        return;
    }

    one_third_src_width = ctx->background_width / 3;
    one_third_src_height = ctx->background_height / 3;

    if (ctx->pressed) {
        SDL_SetTextureColorMod(ctx->background, PRESSED_TEXTURE_MOD);
    } else if (ctx->highlight) {
        SDL_SetTextureColorMod(ctx->background, HIGHLIGHT_TEXTURE_MOD);
    } else {
        SDL_SetTextureColorMod(ctx->background, 255, 255, 255);
    }

    /* Top left */
    src.x = 0.0f;
    src.y = 0.0f;
    src.w = one_third_src_width;
    src.h = one_third_src_height;
    dst.x = ctx->area.x;
    dst.y = ctx->area.y;
    dst.w = src.w;
    dst.h = src.h;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Bottom left */
    src.y = ctx->background_height - src.h;
    dst.y = ctx->area.y + ctx->area.h - dst.h;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Bottom right */
    src.x = ctx->background_width - src.w;
    dst.x = ctx->area.x + ctx->area.w - dst.w;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Top right */
    src.y = 0.0f;
    dst.y = ctx->area.y;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Left */
    src.x = 0.0f;
    src.y = one_third_src_height;
    dst.x = ctx->area.x;
    dst.y = ctx->area.y + one_third_src_height;
    dst.w = one_third_src_width;
    dst.h = ctx->area.h - 2 * one_third_src_height;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Right */
    src.x = ctx->background_width - one_third_src_width;
    dst.x = ctx->area.x + ctx->area.w - one_third_src_width;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Top */
    src.x = one_third_src_width;
    src.y = 0.0f;
    dst.x = ctx->area.x + one_third_src_width;
    dst.y = ctx->area.y;
    dst.w = ctx->area.w - 2 * one_third_src_width;
    dst.h = one_third_src_height;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Bottom */
    src.y = ctx->background_height - src.h;
    dst.y = ctx->area.y + ctx->area.h - one_third_src_height;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Center */
    src.x = one_third_src_width;
    src.y = one_third_src_height;
    dst.x = ctx->area.x + one_third_src_width;
    dst.y = ctx->area.y + one_third_src_height;
    dst.w = ctx->area.w - 2 * one_third_src_width;
    dst.h = ctx->area.h - 2 * one_third_src_height;
    SDL_RenderTexture(ctx->renderer, ctx->background, &src, &dst);

    /* Label */
    dst.x = ctx->area.x + ctx->area.w / 2 - ctx->label_width / 2;
    dst.y = ctx->area.y + ctx->area.h / 2 - ctx->label_height / 2;
    SDLTest_DrawString(ctx->renderer, dst.x, dst.y, ctx->label);
}

void DestroyGamepadButton(GamepadButton *ctx)
{
    if (!ctx) {
        return;
    }

    SDL_DestroyTexture(ctx->background);
    SDL_free(ctx->label);
    SDL_free(ctx);
}


typedef struct
{
    char *guid;
    char *name;
    int num_elements;
    char **keys;
    char **values;
} MappingParts;

static bool AddMappingKeyValue(MappingParts *parts, char *key, char *value);

static bool AddMappingHalfAxisValue(MappingParts *parts, const char *key, const char *value, char sign)
{
    char *new_key, *new_value;

    if (SDL_asprintf(&new_key, "%c%s", sign, key) < 0) {
        return false;
    }

    if (*value && value[SDL_strlen(value) - 1] == '~') {
        /* Invert the sign of the bound axis */
        if (sign == '+') {
            sign = '-';
        } else {
            sign = '+';
        }
    }

    if (SDL_asprintf(&new_value, "%c%s", sign, value) < 0) {
        SDL_free(new_key);
        return false;
    }
    if (new_value[SDL_strlen(new_value) - 1] == '~') {
        new_value[SDL_strlen(new_value) - 1] = '\0';
    }

    return AddMappingKeyValue(parts, new_key, new_value);
}

static bool AddMappingKeyValue(MappingParts *parts, char *key, char *value)
{
    int axis;
    char **new_keys, **new_values;

    if (!key || !value) {
        SDL_free(key);
        SDL_free(value);
        return false;
    }

    /* Split axis values for easy binding purposes */
    for (axis = 0; axis < SDL_GAMEPAD_AXIS_LEFT_TRIGGER; ++axis) {
        if (SDL_strcmp(key, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)axis)) == 0) {
            bool result;

            result = AddMappingHalfAxisValue(parts, key, value, '-') &&
                     AddMappingHalfAxisValue(parts, key, value, '+');
            SDL_free(key);
            SDL_free(value);
            return result;
        }
    }

    new_keys = (char **)SDL_realloc(parts->keys, (parts->num_elements + 1) * sizeof(*new_keys));
    if (!new_keys) {
        return false;
    }
    parts->keys = new_keys;

    new_values = (char **)SDL_realloc(parts->values, (parts->num_elements + 1) * sizeof(*new_values));
    if (!new_values) {
        return false;
    }
    parts->values = new_values;

    new_keys[parts->num_elements] = key;
    new_values[parts->num_elements] = value;
    ++parts->num_elements;
    return true;
}

static void SplitMapping(const char *mapping, MappingParts *parts)
{
    const char *current, *comma, *colon, *key, *value;
    char *new_key, *new_value;

    SDL_zerop(parts);

    if (!mapping || !*mapping) {
        return;
    }

    /* Get the guid */
    current = mapping;
    comma = SDL_strchr(current, ',');
    if (!comma) {
        parts->guid = SDL_strdup(current);
        return;
    }
    parts->guid = SDL_strndup(current, (comma - current));
    current = comma + 1;

    /* Get the name */
    comma = SDL_strchr(current, ',');
    if (!comma) {
        parts->name = SDL_strdup(current);
        return;
    }
    if (*current != '*') {
        parts->name = SDL_strndup(current, (comma - current));
    }
    current = comma + 1;

    for (;;) {
        colon = SDL_strchr(current, ':');
        if (!colon) {
            break;
        }

        key = current;
        value = colon + 1;
        comma = SDL_strchr(value, ',');

        new_key = SDL_strndup(key, (colon - key));
        if (comma) {
            new_value = SDL_strndup(value, (comma - value));
        } else {
            new_value = SDL_strdup(value);
        }
        if (!AddMappingKeyValue(parts, new_key, new_value)) {
            break;
        }

        if (comma) {
            current = comma + 1;
        } else {
            break;
        }
    }
}

static int FindMappingKey(const MappingParts *parts, const char *key)
{
    int i;

    if (key) {
        for (i = 0; i < parts->num_elements; ++i) {
            if (SDL_strcmp(key, parts->keys[i]) == 0) {
                return i;
            }
        }
    }
    return -1;
}

static void RemoveMappingValueAt(MappingParts *parts, int index)
{
    SDL_free(parts->keys[index]);
    SDL_free(parts->values[index]);
    --parts->num_elements;
    if (index < parts->num_elements) {
        SDL_memmove(&parts->keys[index], &parts->keys[index] + 1, (parts->num_elements - index) * sizeof(parts->keys[index]));
        SDL_memmove(&parts->values[index], &parts->values[index] + 1, (parts->num_elements - index) * sizeof(parts->values[index]));
    }
}

static void ConvertBAXYMapping(MappingParts *parts)
{
    int i;
    bool baxy_mapping = false;

    for (i = 0; i < parts->num_elements; ++i) {
        const char *key = parts->keys[i];
        const char *value = parts->values[i];

        if (SDL_strcmp(key, "hint") == 0 &&
            SDL_strcmp(value, "SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") == 0) {
            baxy_mapping = true;
        }
    }

    if (!baxy_mapping) {
        return;
    }

    /* Swap buttons, invert hint */
    for (i = 0; i < parts->num_elements; ++i) {
        char *key = parts->keys[i];
        char *value = parts->values[i];

        if (SDL_strcmp(key, "a") == 0) {
            parts->keys[i] = SDL_strdup("b");
            SDL_free(key);
        } else if (SDL_strcmp(key, "b") == 0) {
            parts->keys[i] = SDL_strdup("a");
            SDL_free(key);
        } else if (SDL_strcmp(key, "x") == 0) {
            parts->keys[i] = SDL_strdup("y");
            SDL_free(key);
        } else if (SDL_strcmp(key, "y") == 0) {
            parts->keys[i] = SDL_strdup("x");
            SDL_free(key);
        } else if (SDL_strcmp(key, "hint") == 0 &&
                   SDL_strcmp(value, "SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") == 0) {
            parts->values[i] = SDL_strdup("!SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1");
            SDL_free(value);
        }
    }
}

static void UpdateLegacyElements(MappingParts *parts)
{
    ConvertBAXYMapping(parts);
}

static bool CombineMappingAxes(MappingParts *parts)
{
    int i, matching, axis;

    for (i = 0; i < parts->num_elements; ++i) {
        char *key = parts->keys[i];
        char *current_value;
        char *matching_key;
        char *matching_value;

        if (*key != '-' && *key != '+') {
            continue;
        }

        for (axis = 0; axis < SDL_GAMEPAD_AXIS_LEFT_TRIGGER; ++axis) {
            if (SDL_strcmp(key + 1, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)axis)) == 0) {
                /* Look for a matching axis with the opposite sign */
                if (SDL_asprintf(&matching_key, "%c%s", (*key == '-' ? '+' : '-'), key + 1) < 0) {
                    return false;
                }
                matching = FindMappingKey(parts, matching_key);
                if (matching >= 0) {
                    /* Check to see if they're bound to the same axis */
                    current_value = parts->values[i];
                    matching_value = parts->values[matching];
                    if (((*current_value == '-' && *matching_value == '+') ||
                         (*current_value == '+' && *matching_value == '-')) &&
                        SDL_strcmp(current_value + 1, matching_value + 1) == 0) {
                        /* Combine these axes */
                        if (*key == *current_value) {
                            SDL_memmove(current_value, current_value + 1, SDL_strlen(current_value));
                        } else {
                            /* Invert the bound axis */
                            SDL_memmove(current_value, current_value + 1, SDL_strlen(current_value)-1);
                            current_value[SDL_strlen(current_value) - 1] = '~';
                        }
                        SDL_memmove(key, key + 1, SDL_strlen(key));
                        RemoveMappingValueAt(parts, matching);
                    }
                }
                SDL_free(matching_key);
                break;
            }
        }
    }
    return true;
}

typedef struct
{
    MappingParts *parts;
    int index;
} MappingSortEntry;

static int SDLCALL SortMapping(const void *a, const void *b)
{
    MappingSortEntry *A = (MappingSortEntry *)a;
    MappingSortEntry *B = (MappingSortEntry *)b;
    const char *keyA = A->parts->keys[A->index];
    const char *keyB = B->parts->keys[B->index];

    return SDL_strcmp(keyA, keyB);
}

static void MoveSortedEntry(const char *key, MappingSortEntry *sort_order, int num_elements, bool front)
{
    int i;

    for (i = 0; i < num_elements; ++i) {
        MappingSortEntry *entry = &sort_order[i];
        if (SDL_strcmp(key, entry->parts->keys[entry->index]) == 0) {
            if (front && i != 0) {
                MappingSortEntry tmp = sort_order[i];
                SDL_memmove(&sort_order[1], &sort_order[0], sizeof(*sort_order)*i);
                sort_order[0] = tmp;
            } else if (!front && i != (num_elements - 1)) {
                MappingSortEntry tmp = sort_order[i];
                SDL_memmove(&sort_order[i], &sort_order[i + 1], sizeof(*sort_order)*(num_elements - i - 1));
                sort_order[num_elements - 1] = tmp;
            }
            break;
        }
    }
}

static char *JoinMapping(MappingParts *parts)
{
    int i;
    size_t length;
    char *mapping;
    const char *guid;
    const char *name;
    MappingSortEntry *sort_order;

    UpdateLegacyElements(parts);
    CombineMappingAxes(parts);

    guid = parts->guid;
    if (!guid || !*guid) {
        guid = "*";
    }

    name = parts->name;
    if (!name || !*name) {
        name = "*";
    }

    length = SDL_strlen(guid) + 1 + SDL_strlen(name) + 1;
    for (i = 0; i < parts->num_elements; ++i) {
        length += SDL_strlen(parts->keys[i]) + 1;
        length += SDL_strlen(parts->values[i]) + 1;
    }
    length += 1;

    /* The sort order is: crc, platform, type, *, sdk, hint */
    sort_order = SDL_stack_alloc(MappingSortEntry, parts->num_elements);
    for (i = 0; i < parts->num_elements; ++i) {
        sort_order[i].parts = parts;
        sort_order[i].index = i;
    }
    SDL_qsort(sort_order, parts->num_elements, sizeof(*sort_order), SortMapping);
    MoveSortedEntry("face", sort_order, parts->num_elements, true);
    MoveSortedEntry("type", sort_order, parts->num_elements, true);
    MoveSortedEntry("platform", sort_order, parts->num_elements, true);
    MoveSortedEntry("crc", sort_order, parts->num_elements, true);
    MoveSortedEntry("sdk>=", sort_order, parts->num_elements, false);
    MoveSortedEntry("sdk<=", sort_order, parts->num_elements, false);
    MoveSortedEntry("hint", sort_order, parts->num_elements, false);

    /* Move platform to the front */

    mapping = (char *)SDL_malloc(length);
    if (mapping) {
        *mapping = '\0';
        SDL_strlcat(mapping, guid, length);
        SDL_strlcat(mapping, ",", length);
        SDL_strlcat(mapping, name, length);
        SDL_strlcat(mapping, ",", length);
        for (i = 0; i < parts->num_elements; ++i) {
            int next = sort_order[i].index;
            SDL_strlcat(mapping, parts->keys[next], length);
            SDL_strlcat(mapping, ":", length);
            SDL_strlcat(mapping, parts->values[next], length);
            SDL_strlcat(mapping, ",", length);
        }
    }

    SDL_stack_free(sort_order);

    return mapping;
}

static void FreeMappingParts(MappingParts *parts)
{
    int i;

    SDL_free(parts->guid);
    SDL_free(parts->name);
    for (i = 0; i < parts->num_elements; ++i) {
        SDL_free(parts->keys[i]);
        SDL_free(parts->values[i]);
    }
    SDL_free(parts->keys);
    SDL_free(parts->values);
    SDL_zerop(parts);
}

/* Create a new mapping from the parts and free the old mapping and parts */
static char *RecreateMapping(MappingParts *parts, char *mapping)
{
    char *new_mapping = JoinMapping(parts);
    if (new_mapping) {
        SDL_free(mapping);
        mapping = new_mapping;
    }
    FreeMappingParts(parts);
    return mapping;
}

static const char *GetLegacyKey(const char *key, bool baxy)
{
    if (SDL_strcmp(key, SDL_GetGamepadStringForButton(SDL_GAMEPAD_BUTTON_SOUTH)) == 0) {
        if (baxy) {
            return "b";
        } else {
            return "a";
        }
    }

    if (SDL_strcmp(key, SDL_GetGamepadStringForButton(SDL_GAMEPAD_BUTTON_EAST)) == 0) {
        if (baxy) {
            return "a";
        } else {
            return "b";
        }
    }

    if (SDL_strcmp(key, SDL_GetGamepadStringForButton(SDL_GAMEPAD_BUTTON_WEST)) == 0) {
        if (baxy) {
            return "y";
        } else {
            return "x";
        }
    }

    if (SDL_strcmp(key, SDL_GetGamepadStringForButton(SDL_GAMEPAD_BUTTON_NORTH)) == 0) {
        if (baxy) {
            return "y";
        } else {
            return "x";
        }
    }

    return key;
}

static bool MappingHasKey(const char *mapping, const char *key)
{
    int i;
    MappingParts parts;
    bool result = false;

    SplitMapping(mapping, &parts);
    i = FindMappingKey(&parts, key);
    if (i < 0) {
        bool baxy_mapping = false;

        if (mapping && SDL_strstr(mapping, ",hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") != NULL) {
            baxy_mapping = true;
        }
        i = FindMappingKey(&parts, GetLegacyKey(key, baxy_mapping));
    }
    if (i >= 0) {
        result = true;
    }
    FreeMappingParts(&parts);

    return result;
}

static char *GetMappingValue(const char *mapping, const char *key)
{
    int i;
    MappingParts parts;
    char *value = NULL;

    SplitMapping(mapping, &parts);
    i = FindMappingKey(&parts, key);
    if (i < 0) {
        bool baxy_mapping = false;

        if (mapping && SDL_strstr(mapping, ",hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") != NULL) {
            baxy_mapping = true;
        }
        i = FindMappingKey(&parts, GetLegacyKey(key, baxy_mapping));
    }
    if (i >= 0) {
        value = parts.values[i];
        parts.values[i] = NULL; /* So we don't free it */
    }
    FreeMappingParts(&parts);

    return value;
}

static char *SetMappingValue(char *mapping, const char *key, const char *value)
{
    MappingParts parts;
    int i;
    char *new_key = NULL;
    char *new_value = NULL;
    char **new_keys = NULL;
    char **new_values = NULL;
    bool result = false;

    if (!key) {
        return mapping;
    }

    SplitMapping(mapping, &parts);
    i = FindMappingKey(&parts, key);
    if (i >= 0) {
        new_value = SDL_strdup(value);
        if (new_value) {
            SDL_free(parts.values[i]);
            parts.values[i] = new_value;
            result = true;
        }
    } else {
        int count = parts.num_elements;

        new_key = SDL_strdup(key);
        if (new_key) {
            new_value = SDL_strdup(value);
            if (new_value) {
                new_keys = (char **)SDL_realloc(parts.keys, (count + 1) * sizeof(*new_keys));
                if (new_keys) {
                    new_values = (char **)SDL_realloc(parts.values, (count + 1) * sizeof(*new_values));
                    if (new_values) {
                        new_keys[count] = new_key;
                        new_values[count] = new_value;
                        parts.num_elements = (count + 1);
                        parts.keys = new_keys;
                        parts.values = new_values;
                        result = true;
                    }
                }
            }
        }
    }

    if (result) {
        mapping = RecreateMapping(&parts, mapping);
    } else {
        SDL_free(new_key);
        SDL_free(new_value);
        SDL_free(new_keys);
        SDL_free(new_values);
    }
    return mapping;
}

static char *RemoveMappingValue(char *mapping, const char *key)
{
    MappingParts parts;
    int i;

    SplitMapping(mapping, &parts);
    i = FindMappingKey(&parts, key);
    if (i >= 0) {
        RemoveMappingValueAt(&parts, i);
    }
    return RecreateMapping(&parts, mapping);
}

bool MappingHasBindings(const char *mapping)
{
    MappingParts parts;
    int i;
    bool result = false;

    if (!mapping || !*mapping) {
        return false;
    }

    SplitMapping(mapping, &parts);
    for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
        if (FindMappingKey(&parts, SDL_GetGamepadStringForButton((SDL_GamepadButton)i)) >= 0) {
            result = true;
            break;
        }
    }
    if (!result) {
        for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
            if (FindMappingKey(&parts, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)i)) >= 0) {
                result = true;
                break;
            }
        }
    }
    FreeMappingParts(&parts);

    return result;
}

bool MappingHasName(const char *mapping)
{
    MappingParts parts;
    bool result;

    SplitMapping(mapping, &parts);
    result = parts.name ? true : false;
    FreeMappingParts(&parts);
    return result;
}

char *GetMappingName(const char *mapping)
{
    MappingParts parts;
    char *name;

    SplitMapping(mapping, &parts);
    name = parts.name;
    parts.name = NULL; /* Don't free the name we're about to return */
    FreeMappingParts(&parts);
    return name;
}

char *SetMappingName(char *mapping, const char *name)
{
    MappingParts parts;
    char *new_name, *spot;
    size_t length;

    if (!name) {
        return mapping;
    }

    /* Remove any leading whitespace */
    while (*name && SDL_isspace(*name)) {
        ++name;
    }

    new_name = SDL_strdup(name);
    if (!new_name) {
        return mapping;
    }

    /* Remove any commas, which are field separators in the mapping */
    length = SDL_strlen(new_name);
    while ((spot = SDL_strchr(new_name, ',')) != NULL) {
        SDL_memmove(spot, spot + 1, length - (spot - new_name) + 1);
        --length;
    }

    /* Remove any trailing whitespace */
    while (length > 0 && SDL_isspace(new_name[length - 1])) {
        --length;
    }

    /* See if we have anything left */
    if (length == 0) {
        SDL_free(new_name);
        return mapping;
    }

    /* null terminate to cut off anything we've trimmed */
    new_name[length] = '\0';

    SplitMapping(mapping, &parts);
    SDL_free(parts.name);
    parts.name = new_name;
    return RecreateMapping(&parts, mapping);
}


const char *GetGamepadTypeString(SDL_GamepadType type)
{
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
        return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return "Xbox One";
    case SDL_GAMEPAD_TYPE_PS3:
        return "PS3";
    case SDL_GAMEPAD_TYPE_PS4:
        return "PS4";
    case SDL_GAMEPAD_TYPE_PS5:
        return "PS5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        return "Nintendo Switch";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        return "Joy-Con (L)";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        return "Joy-Con (R)";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return "Joy-Con Pair";
    case SDL_GAMEPAD_TYPE_GAMECUBE:
        return "GameCube";
    default:
        return "";
    }
}

SDL_GamepadType GetMappingType(const char *mapping)
{
    return SDL_GetGamepadTypeFromString(GetMappingValue(mapping, "type"));
}

char *SetMappingType(char *mapping, SDL_GamepadType type)
{
    const char *type_string = SDL_GetGamepadStringForType(type);
    if (!type_string || type == SDL_GAMEPAD_TYPE_UNKNOWN) {
        return RemoveMappingValue(mapping, "type");
    } else {
        return SetMappingValue(mapping, "type", type_string);
    }
}

static const char *GetElementKey(int element)
{
    if (element < SDL_GAMEPAD_BUTTON_COUNT) {
        return SDL_GetGamepadStringForButton((SDL_GamepadButton)element);
    } else {
        static char key[16];

        switch (element) {
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE:
            SDL_snprintf(key, sizeof(key), "-%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTX));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE:
            SDL_snprintf(key, sizeof(key), "+%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTX));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE:
            SDL_snprintf(key, sizeof(key), "-%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTY));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE:
            SDL_snprintf(key, sizeof(key), "+%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTY));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE:
            SDL_snprintf(key, sizeof(key), "-%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTX));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE:
            SDL_snprintf(key, sizeof(key), "+%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTX));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE:
            SDL_snprintf(key, sizeof(key), "-%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTY));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE:
            SDL_snprintf(key, sizeof(key), "+%s", SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTY));
            break;
        case SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER:
            return SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        case SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER:
            return SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        default:
            return NULL;
        }
        return key;
    }
}

bool MappingHasElement(const char *mapping, int element)
{
    const char *key;

    key = GetElementKey(element);
    if (!key) {
        return false;
    }
    return MappingHasKey(mapping, key);
}

char *GetElementBinding(const char *mapping, int element)
{
    const char *key;

    key = GetElementKey(element);
    if (!key) {
        return NULL;
    }
    return GetMappingValue(mapping, key);
}

char *SetElementBinding(char *mapping, int element, const char *binding)
{
    if (binding) {
        return SetMappingValue(mapping, GetElementKey(element), binding);
    } else {
        return RemoveMappingValue(mapping, GetElementKey(element));
    }
}

int GetElementForBinding(char *mapping, const char *binding)
{
    MappingParts parts;
    int i, element;
    int result = SDL_GAMEPAD_ELEMENT_INVALID;

    if (!binding) {
        return SDL_GAMEPAD_ELEMENT_INVALID;
    }

    SplitMapping(mapping, &parts);
    for (i = 0; i < parts.num_elements; ++i) {
        if (SDL_strcmp(binding, parts.values[i]) == 0) {
            for (element = 0; element < SDL_GAMEPAD_ELEMENT_MAX; ++element) {
                const char *key = GetElementKey(element);
                if (key && SDL_strcmp(key, parts.keys[i]) == 0) {
                    result = element;
                    break;
                }
            }
            break;
        }
    }
    FreeMappingParts(&parts);

    return result;
}

bool MappingHasBinding(const char *mapping, const char *binding)
{
    MappingParts parts;
    int i;
    bool result = false;

    if (!binding) {
        return false;
    }

    SplitMapping(mapping, &parts);
    for (i = 0; i < parts.num_elements; ++i) {
        if (SDL_strcmp(binding, parts.values[i]) == 0) {
            result = true;
            break;
        }
    }
    FreeMappingParts(&parts);

    return result;
}

char *ClearMappingBinding(char *mapping, const char *binding)
{
    MappingParts parts;
    int i;
    bool modified = false;

    if (!binding) {
        return mapping;
    }

    SplitMapping(mapping, &parts);
    for (i = parts.num_elements - 1; i >= 0; --i) {
        if (SDL_strcmp(binding, parts.values[i]) == 0) {
            RemoveMappingValueAt(&parts, i);
            modified = true;
        }
    }
    if (modified) {
        return RecreateMapping(&parts, mapping);
    } else {
        FreeMappingParts(&parts);
        return mapping;
    }
}
