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
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h> /* exit() */

#ifdef SDL_PLATFORM_3DS
/* For mouse-based tests, we want to have the window on the touch screen */
#define SCREEN_X 40
#define SCREEN_Y 240
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   240
#elif defined(SDL_PLATFORM_IOS)
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   480
#else
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#endif

static SDL_Window *window;

typedef struct _Object
{
    struct _Object *next;

    float x1, y1, x2, y2;
    Uint8 r, g, b;

    bool isRect;
} Object;

static Object *active = NULL;
static Object *objects = NULL;
static int buttons = 0;
static bool isRect = false;

static bool wheel_x_active = false;
static bool wheel_y_active = false;
static float wheel_x = SCREEN_WIDTH * 0.5f;
static float wheel_y = SCREEN_HEIGHT * 0.5f;

struct mouse_loop_data {
    bool done;
    SDL_Renderer *renderer;
};

static void DrawObject(SDL_Renderer *renderer, Object *object)
{
    SDL_SetRenderDrawColor(renderer, object->r, object->g, object->b, 255);

    if (object->isRect) {
        SDL_FRect rect;

        if (object->x1 > object->x2) {
            rect.x = object->x2;
            rect.w = object->x1 - object->x2;
        } else {
            rect.x = object->x1;
            rect.w = object->x2 - object->x1;
        }

        if (object->y1 > object->y2) {
            rect.y = object->y2;
            rect.h = object->y1 - object->y2;
        } else {
            rect.y = object->y1;
            rect.h = object->y2 - object->y1;
        }

        SDL_RenderFillRect(renderer, &rect);
    } else {
        SDL_RenderLine(renderer, object->x1, object->y1, object->x2, object->y2);
    }
}

static void DrawObjects(SDL_Renderer *renderer)
{
    Object *next = objects;
    while (next) {
        DrawObject(renderer, next);
        next = next->next;
    }
}

static void AppendObject(Object *object)
{
    if (objects) {
        Object *next = objects;
        while (next->next) {
            next = next->next;
        }
        next->next = object;
    } else {
        objects = object;
    }
}

static void loop(void *arg)
{
    struct mouse_loop_data *loop_data = (struct mouse_loop_data *)arg;
    SDL_Event event;
    SDL_Renderer *renderer = loop_data->renderer;
    float fx, fy;
    SDL_MouseButtonFlags flags;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                event.wheel.x *= -1;
                event.wheel.y *= -1;
            }
            if (event.wheel.x != 0.0f) {
                wheel_x_active = true;
                /* "positive to the right and negative to the left"  */
                wheel_x += event.wheel.x * 10.0f;
            }
            if (event.wheel.y != 0.0f) {
                wheel_y_active = true;
                /* "positive away from the user and negative towards the user" */
                wheel_y -= event.wheel.y * 10.0f;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (!active) {
                break;
            }

            active->x2 = event.motion.x;
            active->y2 = event.motion.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!active) {
                active = SDL_calloc(1, sizeof(*active));
                active->x1 = active->x2 = event.button.x;
                active->y1 = active->y2 = event.button.y;
                active->isRect = isRect;
            }

            switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                active->r = 255;
                buttons |= SDL_BUTTON_LMASK;
                break;
            case SDL_BUTTON_MIDDLE:
                active->g = 255;
                buttons |= SDL_BUTTON_MMASK;
                break;
            case SDL_BUTTON_RIGHT:
                active->b = 255;
                buttons |= SDL_BUTTON_RMASK;
                break;
            case SDL_BUTTON_X1:
                active->r = 255;
                active->b = 255;
                buttons |= SDL_BUTTON_X1MASK;
                break;
            case SDL_BUTTON_X2:
                active->g = 255;
                active->b = 255;
                buttons |= SDL_BUTTON_X2MASK;
                break;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (!active) {
                break;
            }

            switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                buttons &= ~SDL_BUTTON_LMASK;
                break;
            case SDL_BUTTON_MIDDLE:
                buttons &= ~SDL_BUTTON_MMASK;
                break;
            case SDL_BUTTON_RIGHT:
                buttons &= ~SDL_BUTTON_RMASK;
                break;
            case SDL_BUTTON_X1:
                buttons &= ~SDL_BUTTON_X1MASK;
                break;
            case SDL_BUTTON_X2:
                buttons &= ~SDL_BUTTON_X2MASK;
                break;
            }

            if (buttons == 0) {
                AppendObject(active);
                active = NULL;
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_C) {
                int x, y, w, h;
                SDL_GetWindowPosition(window, &x, &y);
                SDL_GetWindowSize(window, &w, &h);
                w /= 2;
                h /= 2;

                if (event.key.mod & SDL_KMOD_ALT) {
                    SDL_WarpMouseGlobal((float)(x + w), (float)(y + h));
                } else {
                    SDL_WarpMouseInWindow(window, (float)w, (float)h);
                }
            }
            SDL_FALLTHROUGH;
        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
            case SDLK_LSHIFT:
                isRect = event.key.down;
                if (active) {
                    active->isRect = isRect;
                }
                break;
            default:
                break;
            }
            break;

        case SDL_EVENT_QUIT:
            loop_data->done = true;
            break;

        default:
            break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Mouse wheel */
    SDL_SetRenderDrawColor(renderer, 0, 255, 128, 255);
    if (wheel_x_active) {
        SDL_RenderLine(renderer, wheel_x, 0.0f, wheel_x, (float)SCREEN_HEIGHT);
    }
    if (wheel_y_active) {
        SDL_RenderLine(renderer, 0.0f, wheel_y, (float)SCREEN_WIDTH, wheel_y);
    }

    /* Objects from mouse clicks */
    DrawObjects(renderer);
    if (active) {
        DrawObject(renderer, active);
    }

    flags = SDL_GetGlobalMouseState(&fx, &fy);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(renderer, 0, 0, "Global Mouse State: x=%f y=%f flags=%" SDL_PRIu32, fx, fy, flags);

    SDL_RenderPresent(renderer);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (loop_data->done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    struct mouse_loop_data loop_data;
    SDLTest_CommonState *state;
#ifdef SDL_PLATFORM_3DS
    SDL_PropertiesID props;
#endif

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Initialize SDL (Note: video is required to start event loop) */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        exit(1);
    }

    /* Create a window to display joystick axis position */
#ifdef SDL_PLATFORM_3DS
    props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Mouse Test");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SCREEN_X);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SCREEN_Y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, SCREEN_WIDTH);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, SCREEN_HEIGHT);
    SDL_SetNumberProperty(props, "flags", 0);
    window = SDL_CreateWindowWithProperties(props);
#else
    window = SDL_CreateWindow("Mouse Test", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
#endif
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        return 0;
    }

    loop_data.done = false;

    loop_data.renderer = SDL_CreateRenderer(window, NULL);
    if (!loop_data.renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        return 0;
    }

    /* Main render loop */
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop_arg(loop, &loop_data, 0, 1);
#else
    while (loop_data.done == false) {
        loop(&loop_data);
    }
#endif

    SDL_DestroyRenderer(loop_data.renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
