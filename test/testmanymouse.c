/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

/* Stolen from the mailing list */
/* Creates a new mouse cursor from an XPM */

/* XPM */
static const char *arrow[] = {
    /* width height num_colors chars_per_pixel */
    "    32    32        3            1",
    /* colors */
    "X c #000000",
    ". c #ffffff",
    "  c None",
    /* pixels */
    "X                               ",
    "XX                              ",
    "X.X                             ",
    "X..X                            ",
    "X...X                           ",
    "X....X                          ",
    "X.....X                         ",
    "X......X                        ",
    "X.......X                       ",
    "X........X                      ",
    "X.....XXXXX                     ",
    "X..X..X                         ",
    "X.X X..X                        ",
    "XX  X..X                        ",
    "X    X..X                       ",
    "     X..X                       ",
    "      X..X                      ",
    "      X..X                      ",
    "       XX                       ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "0,0"
};

static const char *cross[] = {
    /* width height num_colors chars_per_pixel */
    "    32    32        3            1",
    /* colors */
    "o c #ffffff",
    ". c #000000",
    "  c None",
    /* pixels */
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "    oooooooooooooooooooooooo    ",
    "    oooooooooooooooooooooooo    ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "0,0"
};

static SDLTest_CommonState *state;
static int done;

#define PROP_ARROW_CURSOR_TEXTURE "arrow_cursor_texture"
#define PROP_CROSS_CURSOR_TEXTURE "cross_cursor_texture"
#define MAX_MICE        3
#define MAX_KEYBOARDS   3
#define CURSOR_SIZE 48.0f
#define MAX_TRAIL   500
#define TRAIL_SIZE  8.0f

static SDL_Color colors[] = {
    {   0, 255, 255, 255 }, /* mouse 1, cyan */
    { 255,   0, 255, 255 }, /* mouse 2, magenta */
    { 255, 255,   0, 255 }, /* mouse 3, yellow */
};
SDL_COMPILE_TIME_ASSERT(mouse_colors, SDL_arraysize(colors) == MAX_MICE);
SDL_COMPILE_TIME_ASSERT(keyboard_colors, SDL_arraysize(colors) == MAX_KEYBOARDS);

typedef struct
{
    SDL_MouseID instance_id;
    bool active;
    Uint8 button_state;
    SDL_FPoint position;
    int trail_head;
    int trail_length;
    SDL_FPoint trail[MAX_TRAIL];
} MouseState;

static MouseState mice[MAX_MICE];

typedef struct
{
    SDL_KeyboardID instance_id;
    bool active;
    Uint8 button_state;
    SDL_FPoint position;
} KeyboardState;

static KeyboardState keyboards[MAX_KEYBOARDS];

static SDL_Texture *CreateTexture(const char *image[], SDL_Renderer *renderer)
{
    SDL_Surface *surface;
    SDL_Palette *palette;
    SDL_Texture *texture;
    int row;

    surface = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_INDEX8);
    for (row = 0; row < surface->h; ++row) {
        SDL_memcpy((Uint8 *)surface->pixels + row * surface->pitch, image[4 + row], surface->w);
    }

    palette = SDL_CreateSurfacePalette(surface);
    if (!palette) {
        SDL_DestroySurface(surface);
        return NULL;
    }
    palette->colors['.'].r = 0xFF;
    palette->colors['.'].g = 0xFF;
    palette->colors['.'].b = 0xFF;
    palette->colors['o'].r = 0xFF;
    palette->colors['o'].g = 0xFF;
    palette->colors['o'].b = 0xFF;
    palette->colors['X'].r = 0x00;
    palette->colors['X'].g = 0x00;
    palette->colors['X'].b = 0x00;

    SDL_SetSurfaceColorKey(surface, true, ' ');

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

static void HandleMouseAdded(SDL_MouseID instance_id)
{
    SDL_Window *window = state->windows[0];
    int i, w = 0, h = 0;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    for (i = 0; i < SDL_arraysize(mice); ++i) {
        MouseState *mouse_state = &mice[i];
        if (!mouse_state->active) {
            mouse_state->instance_id = instance_id;
            mouse_state->active = true;
            mouse_state->position.x = w * 0.5f;
            mouse_state->position.y = h * 0.5f;
            return;
        }
    }
}

static void HandleMouseRemoved(SDL_MouseID instance_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(mice); ++i) {
        MouseState *mouse_state = &mice[i];
        if (instance_id == mouse_state->instance_id) {
            SDL_zerop(mouse_state);
            return;
        }
    }
}

static void ActivateMouse(SDL_MouseID instance_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(mice); ++i) {
        MouseState *mouse_state = &mice[i];
        if (mouse_state->active && instance_id == mouse_state->instance_id) {
            return;
        }
    }

    HandleMouseAdded(instance_id);
}

static void HandleMouseMotion(SDL_MouseMotionEvent *event)
{
    SDL_Window *window = state->windows[0];
    int i, w = 0, h = 0;

    if (event->which == 0) {
        /* The system pointer, not a distinct mouse device */
        return;
    }

    ActivateMouse(event->which);

    SDL_GetWindowSizeInPixels(window, &w, &h);

    for (i = 0; i < SDL_arraysize(mice); ++i) {
        MouseState *mouse_state = &mice[i];
        if (!mouse_state->active) {
            continue;
        }
        if (event->which == mouse_state->instance_id) {
            float x = (mouse_state->position.x + event->xrel);
            float y = (mouse_state->position.y + event->yrel);

            x = SDL_clamp(x, 0.0f, (float)w);
            y = SDL_clamp(y, 0.0f, (float)h);

            mouse_state->position.x = x;
            mouse_state->position.y = y;

            if (mouse_state->button_state) {
                /* Add a spot to the mouse trail */
                SDL_FPoint *spot = &mouse_state->trail[mouse_state->trail_head];
                spot->x = x - TRAIL_SIZE * 0.5f;
                spot->y = y - TRAIL_SIZE * 0.5f;
                if (mouse_state->trail_length < MAX_TRAIL) {
                    ++mouse_state->trail_length;
                }
                mouse_state->trail_head = (mouse_state->trail_head + 1) % MAX_TRAIL;
            }
        }
    }
}

static void HandleMouseButton(SDL_MouseButtonEvent *event)
{
    int i;

    if (event->which == 0) {
        /* The system pointer, not a distinct mouse device */
        return;
    }

    ActivateMouse(event->which);

    for (i = 0; i < SDL_arraysize(mice); ++i) {
        MouseState *mouse_state = &mice[i];
        if (!mouse_state->active) {
            continue;
        }
        if (event->which == mouse_state->instance_id) {
            if (event->down) {
                mouse_state->button_state |= SDL_BUTTON_MASK(event->button);
            } else {
                mouse_state->button_state &= ~SDL_BUTTON_MASK(event->button);
            }
        }
    }
}

static void DrawMouseState(SDL_Window *window, SDL_Renderer *renderer, MouseState *mouse_state, SDL_Texture *cursor, SDL_Color *color)
{
    SDL_FRect rect;

    if (!mouse_state->active) {
        return;
    }

    if (mouse_state->trail_length > 0) {
        int i;
        int spot = mouse_state->trail_head - mouse_state->trail_length;
        if (spot < 0) {
            spot += MAX_TRAIL;
        }

        rect.w = TRAIL_SIZE;
        rect.h = TRAIL_SIZE;
        SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
        for (i = 0; i < mouse_state->trail_length; ++i) {
            rect.x = mouse_state->trail[spot].x;
            rect.y = mouse_state->trail[spot].y;
            SDL_RenderFillRect(renderer, &rect);
            spot = (spot + 1) % MAX_TRAIL;
        }
    }

    rect.x = mouse_state->position.x;
    rect.y = mouse_state->position.y;
    rect.w = CURSOR_SIZE;
    rect.h = CURSOR_SIZE;
    SDL_SetTextureColorMod(cursor, color->r, color->g, color->b);
    SDL_RenderTexture(renderer, cursor, NULL, &rect);
}

static void HandleKeyboardAdded(SDL_KeyboardID instance_id)
{
    SDL_Window *window = state->windows[0];
    int i, w = 0, h = 0;

    SDL_GetWindowSize(window, &w, &h);

    for (i = 0; i < SDL_arraysize(keyboards); ++i) {
        KeyboardState *keyboard_state = &keyboards[i];
        if (!keyboard_state->active) {
            keyboard_state->instance_id = instance_id;
            keyboard_state->active = true;
            keyboard_state->position.x = w * 0.5f;
            keyboard_state->position.y = h * 0.5f;
            return;
        }
    }
}

static void HandleKeyboardRemoved(SDL_KeyboardID instance_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(keyboards); ++i) {
        KeyboardState *keyboard_state = &keyboards[i];
        if (instance_id == keyboard_state->instance_id) {
            SDL_zerop(keyboard_state);
            return;
        }
    }
}

static void ActivateKeyboard(SDL_KeyboardID instance_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(keyboards); ++i) {
        KeyboardState *keyboard_state = &keyboards[i];
        if (keyboard_state->active && instance_id == keyboard_state->instance_id) {
            return;
        }
    }

    HandleKeyboardAdded(instance_id);
}

static void HandleKeyboardKeyDown(SDL_KeyboardEvent *event)
{
    SDL_Window *window = state->windows[0];
    int i, w = 0, h = 0;

    SDL_GetWindowSize(window, &w, &h);

    ActivateKeyboard(event->which);

    for (i = 0; i < SDL_arraysize(keyboards); ++i) {
        KeyboardState *keyboard_state = &keyboards[i];
        if (!keyboard_state->active) {
            continue;
        }
        if (event->which == keyboard_state->instance_id) {
            switch (event->key) {
            case SDLK_LEFT:
                keyboard_state->position.x -= CURSOR_SIZE;
                if (keyboard_state->position.x < 0.0f) {
                    keyboard_state->position.x = 0.0f;
                }
                break;
            case SDLK_RIGHT:
                keyboard_state->position.x += CURSOR_SIZE;
                if (keyboard_state->position.x > w) {
                    keyboard_state->position.x = (float)w;
                }
                break;
            case SDLK_UP:
                keyboard_state->position.y -= CURSOR_SIZE;
                if (keyboard_state->position.y < 0.0f) {
                    keyboard_state->position.y = 0.0f;
                }
                break;
            case SDLK_DOWN:
                keyboard_state->position.y += CURSOR_SIZE;
                if (keyboard_state->position.y > h) {
                    keyboard_state->position.y = (float)h;
                }
                break;
            default:
                break;
            }
        }
    }
}

static void DrawKeyboardState(SDL_Window *window, SDL_Renderer *renderer, KeyboardState *keyboard_state, SDL_Texture *cursor, SDL_Color *color)
{
    SDL_FRect rect;

    if (!keyboard_state->active) {
        return;
    }

    rect.x = keyboard_state->position.x - CURSOR_SIZE / 2;
    rect.y = keyboard_state->position.y - CURSOR_SIZE / 2;
    rect.w = CURSOR_SIZE;
    rect.h = CURSOR_SIZE;
    SDL_SetTextureColorMod(cursor, color->r, color->g, color->b);
    SDL_RenderTexture(renderer, cursor, NULL, &rect);
}

static void loop(void)
{
    int i, j;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);

        switch (event.type) {
        case SDL_EVENT_KEYBOARD_ADDED:
            /* Wait for events before activating this keyboard */
            break;
        case SDL_EVENT_KEYBOARD_REMOVED:
            HandleKeyboardRemoved(event.kdevice.which);
            break;
        case SDL_EVENT_KEY_DOWN:
            HandleKeyboardKeyDown(&event.key);
            break;
        case SDL_EVENT_MOUSE_ADDED:
            /* Wait for events before activating this mouse */
            break;
        case SDL_EVENT_MOUSE_REMOVED:
            HandleMouseRemoved(event.mdevice.which);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            HandleMouseMotion(&event.motion);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            HandleMouseButton(&event.button);
            break;
        default:
            break;
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Window *window = state->windows[i];
        SDL_Renderer *renderer = state->renderers[i];
        SDL_Texture *arrow_cursor = (SDL_Texture *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), PROP_ARROW_CURSOR_TEXTURE, NULL);
        SDL_Texture *cross_cursor = (SDL_Texture *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), PROP_CROSS_CURSOR_TEXTURE, NULL);

        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        SDL_RenderClear(renderer);

        for (j = 0; j < SDL_arraysize(mice); ++j) {
            DrawMouseState(window, renderer, &mice[j], arrow_cursor, &colors[j]);
        }

        for (j = 0; j < SDL_arraysize(keyboards); ++j) {
            DrawKeyboardState(window, renderer, &keyboards[j], cross_cursor, &colors[j]);
        }

        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char *argv[])
{
    int i;

    /* Log all events, including mouse motion */
    SDL_SetHint(SDL_HINT_EVENT_LOGGING, "2");

    /* Support for multiple keyboards requires raw keyboard events on Windows */
    SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "1");

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed < 0) {
            SDLTest_CommonLogUsage(state, argv[0], NULL);
            SDLTest_CommonQuit(state);
            return 1;
        }
        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 2;
    }

    /* Create the cursor textures */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_Texture *cursor_arrow = CreateTexture(arrow, renderer);
        SDL_Texture *cursor_cross = CreateTexture(cross, renderer);

        SDL_SetPointerProperty(SDL_GetRendererProperties(renderer), PROP_ARROW_CURSOR_TEXTURE, cursor_arrow);
        SDL_SetPointerProperty(SDL_GetRendererProperties(renderer), PROP_CROSS_CURSOR_TEXTURE, cursor_cross);

        /* We only get mouse motion for distinct devices when relative mode is enabled */
        SDL_SetWindowRelativeMouseMode(state->windows[i], true);
    }

    /* Main render loop */
    done = 0;
    while (!done) {
        loop();
    }

    SDLTest_CommonQuit(state);
    return 0;
}
