/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/


#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_EMSCRIPTEN

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "SDL_emscriptenmouse.h"

#include "../../events/SDL_mouse_c.h"
#include "SDL_assert.h"


static SDL_Cursor*
Emscripten_CreateDefaultCursor()
{
    SDL_Cursor* cursor;
    Emscripten_CursorData *curdata;

    cursor = SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor) {
        curdata = (Emscripten_CursorData *) SDL_calloc(1, sizeof(*curdata));
        if (!curdata) {
            SDL_OutOfMemory();
            SDL_free(cursor);
            return NULL;
        }

        curdata->system_cursor = "default";
        cursor->driverdata = curdata;
    }
    else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor*
Emscripten_CreateCursor(SDL_Surface* sruface, int hot_x, int hot_y)
{
    return Emscripten_CreateDefaultCursor();
}

static SDL_Cursor*
Emscripten_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    Emscripten_CursorData *curdata;
    const char *cursor_name = NULL;

    switch(id) {
        case SDL_SYSTEM_CURSOR_ARROW:
            cursor_name = "default";
            break;
        case SDL_SYSTEM_CURSOR_IBEAM:
            cursor_name = "text";
            break;
        case SDL_SYSTEM_CURSOR_WAIT:
            cursor_name = "wait";
            break;
        case SDL_SYSTEM_CURSOR_CROSSHAIR:
            cursor_name = "crosshair";
            break;
        case SDL_SYSTEM_CURSOR_WAITARROW:
            cursor_name = "progress";
            break;
        case SDL_SYSTEM_CURSOR_SIZENWSE:
            cursor_name = "nwse-resize";
            break;
        case SDL_SYSTEM_CURSOR_SIZENESW:
            cursor_name = "nesw-resize";
            break;
        case SDL_SYSTEM_CURSOR_SIZEWE:
            cursor_name = "ew-resize";
            break;
        case SDL_SYSTEM_CURSOR_SIZENS:
            cursor_name = "ns-resize";
            break;
        case SDL_SYSTEM_CURSOR_SIZEALL:
            break;
        case SDL_SYSTEM_CURSOR_NO:
            cursor_name = "not-allowed";
            break;
        case SDL_SYSTEM_CURSOR_HAND:
            cursor_name = "pointer";
            break;
        default:
            SDL_assert(0);
            return NULL;
    }

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        SDL_OutOfMemory();
        return NULL;
    }
    curdata = (Emscripten_CursorData *) SDL_calloc(1, sizeof(*curdata));
    if (!curdata) {
        SDL_OutOfMemory();
        SDL_free(cursor);
        return NULL;
    }

    curdata->system_cursor = cursor_name;
    cursor->driverdata = curdata;

    return cursor;
}

static void
Emscripten_FreeCursor(SDL_Cursor* cursor)
{
    Emscripten_CursorData *curdata;
    if (cursor) {
        curdata = (Emscripten_CursorData *) cursor->driverdata;

        if (curdata != NULL) {
            SDL_free(cursor->driverdata);
        }

        SDL_free(cursor);
    }
}

static int
Emscripten_ShowCursor(SDL_Cursor* cursor)
{
    Emscripten_CursorData *curdata;
    if (SDL_GetMouseFocus() != NULL) {
        if(cursor && cursor->driverdata) {
            curdata = (Emscripten_CursorData *) cursor->driverdata;

            if(curdata->system_cursor) {
                EM_ASM_INT({
                    if (Module['canvas']) {
                        Module['canvas'].style['cursor'] = Module['Pointer_stringify']($0);
                    }
                    return 0;
                }, curdata->system_cursor);
            }
        }
        else {
            EM_ASM(
                if (Module['canvas']) {
                    Module['canvas'].style['cursor'] = 'none';
                }
            );
        }
    }
    return 0;
}

static void
Emscripten_WarpMouse(SDL_Window* window, int x, int y)
{
    SDL_Unsupported();
}

static int
Emscripten_SetRelativeMouseMode(SDL_bool enabled)
{
    /* TODO: pointer lock isn't actually enabled yet */
    if(enabled) {
        if(emscripten_request_pointerlock(NULL, 1) >= EMSCRIPTEN_RESULT_SUCCESS) {
            return 0;
        }
    } else {
        if(emscripten_exit_pointerlock() >= EMSCRIPTEN_RESULT_SUCCESS) {
            return 0;
        }
    }
    return -1;
}

void
Emscripten_InitMouse()
{
    SDL_Mouse* mouse = SDL_GetMouse();

    mouse->CreateCursor         = Emscripten_CreateCursor;
    mouse->ShowCursor           = Emscripten_ShowCursor;
    mouse->FreeCursor           = Emscripten_FreeCursor;
    mouse->WarpMouse            = Emscripten_WarpMouse;
    mouse->CreateSystemCursor   = Emscripten_CreateSystemCursor;
    mouse->SetRelativeMouseMode = Emscripten_SetRelativeMouseMode;

    SDL_SetDefaultCursor(Emscripten_CreateDefaultCursor());
}

void
Emscripten_FiniMouse()
{
    SDL_Mouse* mouse = SDL_GetMouse();

    Emscripten_FreeCursor(mouse->def_cursor);
    mouse->def_cursor = NULL;

    mouse->CreateCursor         = NULL;
    mouse->ShowCursor           = NULL;
    mouse->FreeCursor           = NULL;
    mouse->WarpMouse            = NULL;
    mouse->CreateSystemCursor   = NULL;
    mouse->SetRelativeMouseMode = NULL;
}

#endif /* SDL_VIDEO_DRIVER_EMSCRIPTEN */

/* vi: set ts=4 sw=4 expandtab: */

