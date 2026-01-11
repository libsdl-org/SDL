/*
  Simple DirectMedia Layer
  Copyright (C) 2025 BlackBerry Limited

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
#include "../SDL_sysvideo.h"
#include "SDL_qnx.h"
#include "SDL_qnxscreenconsts.h"
#include "../../events/SDL_mouse_c.h"

#include <errno.h>


// TODO: Might need to iterate all windows and attach this cursor to each of
//       them.
static SDL_Cursor *createCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    SDL_Cursor          *cursor;
    SDL_CursorData      *impl;
    screen_session_t    session;

    cursor = SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor) {
        impl = SDL_calloc(1, sizeof(SDL_CursorData));;
        if (impl == NULL) {
            free(cursor);
            SDL_OutOfMemory();
        }
        impl->realized_shape = SCREEN_CURSOR_SHAPE_ARROW;

        screen_create_session_type(&session, context, SCREEN_EVENT_POINTER);
        screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &impl->realized_shape);

        impl->session = session;
        impl->is_visible = 1;
        cursor->internal = (void*)impl;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *createSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor          *cursor;
    SDL_CursorData      *impl;
    screen_session_t    session;
    int shape;

    switch(id)
    {
    default:
        SDL_assert(0);
        return NULL;
    case SDL_SYSTEM_CURSOR_DEFAULT:     shape = SCREEN_CURSOR_SHAPE_ARROW; break;
    case SDL_SYSTEM_CURSOR_TEXT:        shape = SCREEN_CURSOR_SHAPE_IBEAM; break;
    case SDL_SYSTEM_CURSOR_WAIT:        shape = SCREEN_CURSOR_SHAPE_WAIT; break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:   shape = SCREEN_CURSOR_SHAPE_CROSS; break;
    case SDL_SYSTEM_CURSOR_NWSE_RESIZE: shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_NESW_RESIZE: shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_EW_RESIZE:   shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_NS_RESIZE:   shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_MOVE:        shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_NOT_ALLOWED: shape = SCREEN_CURSOR_SHAPE_ARROW; break;
    case SDL_SYSTEM_CURSOR_POINTER:     shape = SCREEN_CURSOR_SHAPE_HAND; break;
    }

    cursor = SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor) {
        impl = SDL_calloc(1, sizeof(SDL_CursorData));;
        if (impl == NULL) {
            free(cursor);
            SDL_OutOfMemory();
        }
        impl->realized_shape = shape;

        screen_create_session_type(&session, context, SCREEN_EVENT_POINTER);
        screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &shape);

        impl->session = session;
        impl->is_visible = 1;
        cursor->internal = (void*)impl;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static bool showCursor(SDL_Cursor * cursor)
{
    SDL_CursorData      *impl;
    screen_session_t    session;
    int shape;

    // SDL does not provide information about previous visibility to its
    // drivers. We need to track that ourselves.
    if (cursor) {
        impl = (SDL_CursorData*)cursor->internal;
        if (impl->is_visible) {
            return true;
        }
        session = impl->session;
        shape = impl->realized_shape;
        impl->is_visible = 1;
    } else {
        cursor = SDL_GetCursor();
        if (cursor == NULL) {
            return false;
        }
        impl = (SDL_CursorData*)cursor->internal;
        if (!impl->is_visible) {
            return 0;
        }
        session = impl->session;
        shape = SCREEN_CURSOR_SHAPE_NONE;
        impl->is_visible = 0;
    }

    if (screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &shape) < 0) {
        return false;
    }

    return true;
}

static void freeCursor(SDL_Cursor * cursor)
{
    SDL_CursorData *impl = (SDL_CursorData*)cursor->internal;

    screen_destroy_session(impl->session);
    SDL_free(impl);
    SDL_free(cursor);
}

static bool setRelativeMouseMode(bool enabled)
{
    // We're tracking rel-position explicitly, but this is still needed so
    // SDL_SetRelativeMouseMode() & friends aren't a no-op.
    //
    // TODO: It may be possible to achieve this using SCREEN_PROPERTY_DISPLACEMENT instead.
    return true;
}

void initMouse(SDL_VideoDevice *_this)
{
    SDL_Mouse       *mouse = SDL_GetMouse();
    SDL_MouseData   *mouse_data;

    mouse_data = (SDL_MouseData *)SDL_calloc(1, sizeof(SDL_MouseData));
    if (mouse_data == NULL) {
        return;
    }
    SDL_zerop(mouse_data);
    mouse->internal = mouse_data;

    mouse->CreateCursor = createCursor;
    mouse->CreateSystemCursor = createSystemCursor;
    mouse->ShowCursor = showCursor;
    mouse->FreeCursor = freeCursor;

    mouse->SetRelativeMouseMode = setRelativeMouseMode;

    SDL_SetDefaultCursor(createCursor(NULL, 0, 0));
}

void quitMouse(SDL_VideoDevice *_this)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->internal != NULL) {
        SDL_free(mouse->internal);
        mouse->internal = NULL;
    }
}
