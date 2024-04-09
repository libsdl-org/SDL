/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "../../events/SDL_mouse_c.h"
#include "../../core/ohos//SDL_ohos.h"
#include "../../SDL_internal.h"
#include "SDL_events.h"
#include "SDL_ohosmouse.h"


#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

/* See OHOS's MotionEvent class for constants */
#define ACTION_DOWN 1
#define ACTION_UP 2
#define ACTION_MOVE 3

#define BUTTON_PRIMARY 0x01
#define BUTTON_SECONDARY 0x02
#define BUTTON_TERTIARY 0x04
#define BUTTON_BACK 0x08
#define BUTTON_FORWARD 0x10

typedef struct
{
    int custom_cursor;
    int system_cursor;

} SDL_OHOSCursorData;

/* Last known OHOS mouse button state (includes all buttons) */
static int last_state;

/* Blank cursor */
static SDL_Cursor *empty_cursor;

static SDL_Cursor *
OHOS_WrapCursor(int custom_cursor, int system_cursor)
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        SDL_OHOSCursorData *data = (SDL_OHOSCursorData *)SDL_calloc(1, sizeof(*data));
        if (data) {
            data->custom_cursor = custom_cursor;
            data->system_cursor = system_cursor;
            cursor->driverdata = data;
        } else {
            SDL_free(cursor);
            cursor = NULL;
            SDL_OutOfMemory();
        }
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
OHOS_CreateDefaultCursor()
{
    return OHOS_WrapCursor(0, SDL_SYSTEM_CURSOR_ARROW);
}

static SDL_Cursor *
OHOS_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    int custom_cursor;
    SDL_Surface *converted;

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!converted) {
        return NULL;
    }
    custom_cursor = OHOS_CreateCustomCursor(converted, hot_x, hot_y);
    SDL_FreeSurface(converted);
    if (!custom_cursor) {
        SDL_Unsupported();
        return NULL;
    }
    return OHOS_WrapCursor(custom_cursor, 0);
}

static SDL_Cursor *
OHOS_CreateSystemCursor(SDL_SystemCursor id)
{
    return OHOS_WrapCursor(0, id);
}

static void
OHOS_FreeCursor(SDL_Cursor * cursor)
{
    SDL_free(cursor->driverdata);
    SDL_free(cursor);
}

static SDL_Cursor *
OHOS_CreateEmptyCursor()
{
    if (!empty_cursor) {
        SDL_Surface *empty_surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_ARGB8888);
        if (empty_surface) {
            SDL_memset(empty_surface->pixels, 0, empty_surface->h * empty_surface->pitch);
            empty_cursor = OHOS_CreateCursor(empty_surface, 0, 0);
            SDL_FreeSurface(empty_surface);
        }
    }
    return empty_cursor;
}

static void 
OHOS_DestroyEmptyCursor() 
{
    if (empty_cursor) {
        OHOS_FreeCursor(empty_cursor);
        empty_cursor = NULL;
    }
}

static int
OHOS_ShowCursor(SDL_Cursor *cursor)
{
    if (!cursor) {
        cursor = OHOS_CreateEmptyCursor();
    }
    if (cursor) {
        SDL_OHOSCursorData *data = (SDL_OHOSCursorData *)cursor->driverdata;
        if (data->custom_cursor) {
            if (!OHOS_SetCustomCursor(data->custom_cursor)) {
                return SDL_Unsupported();
            }
        } else {
            if (!OHOS_SetSystemCursor(data->system_cursor)) {
                return SDL_Unsupported();
            }
        }
        return 0;
    } else {
        /* SDL error set inside OHOS_CreateEmptyCursor() */
        return -1;
    }
}

static int
OHOS_SetRelativeMouseMode(SDL_bool enabled)
{
    if (!OHOS_SupportsRelativeMouse()) {
        return SDL_Unsupported();
    }

    if (!OHOS_SetRelativeMouseEnabled(enabled)) {
        return SDL_Unsupported();
    }

    return 0;
}

void
OHOS_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = OHOS_CreateCursor;
    mouse->CreateSystemCursor = OHOS_CreateSystemCursor;
    mouse->ShowCursor = OHOS_ShowCursor;
    mouse->FreeCursor = OHOS_FreeCursor;
    mouse->SetRelativeMouseMode = OHOS_SetRelativeMouseMode;

    SDL_SetDefaultCursor(OHOS_CreateDefaultCursor());
    SDL_SetRelativeMouseMode(SDL_TRUE);

    last_state = 0;
}

void
OHOS_QuitMouse(void)
{
    OHOS_DestroyEmptyCursor();
}

/* Translate OHOS mouse button state to SDL mouse button */
static Uint8
TranslateButton(int state)
{
    if (state & BUTTON_PRIMARY) {
        return SDL_BUTTON_LEFT;
    } else if (state & BUTTON_SECONDARY) {
        return SDL_BUTTON_RIGHT;
    } else if (state & BUTTON_TERTIARY) {
        return SDL_BUTTON_MIDDLE;
    } else if (state & BUTTON_FORWARD) {
        return SDL_BUTTON_X1;
    } else if (state & BUTTON_BACK) {
        return SDL_BUTTON_X2;
    } else {
        return 0;
    }
}

void
OHOS_OnMouse(SDL_Window *window, OHOS_Window_Size *windowsize, SDL_bool relative)
{
    int changes;
    Uint8 button;

    if (!window) {
        return;
    }

    switch(windowsize->action) {
        case ACTION_DOWN:
            changes = windowsize->state & ~last_state;
            button = TranslateButton(changes);
            last_state = windowsize->state;
            SDL_SendMouseMotion(window, 0, relative, (int)windowsize->x, (int)windowsize->y);
            SDL_SendMouseButton(window, 0, SDL_PRESSED, button);
            break;

        case ACTION_UP:
            changes = last_state & ~windowsize->state;
            button = TranslateButton(changes);
            last_state = windowsize->state;
            SDL_SendMouseMotion(window, 0, relative, (int)windowsize->x, (int)windowsize->y);
            SDL_SendMouseButton(window, 0, SDL_RELEASED, button);
            break;

        case ACTION_MOVE:
            SDL_SendMouseMotion(window, 0, relative, (int)windowsize->x, (int)windowsize->y);
            break;

        default:
            break;
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
