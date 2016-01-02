/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WINRT

/*
 * Windows includes:
 */
#include <Windows.h>
using namespace Windows::UI::Core;
using Windows::UI::Core::CoreCursor;

/*
 * SDL includes:
 */
extern "C" {
#include "SDL_assert.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../SDL_sysvideo.h"
#include "SDL_events.h"
#include "SDL_log.h"
}

#include "../../core/winrt/SDL_winrtapp_direct3d.h"
#include "SDL_winrtvideo_cpp.h"
#include "SDL_winrtmouse_c.h"


extern "C" SDL_bool WINRT_UsingRelativeMouseMode = SDL_FALSE;


static SDL_Cursor *
WINRT_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    CoreCursorType cursorType = CoreCursorType::Arrow;

    switch(id)
    {
    default:
        SDL_assert(0);
        return NULL;
    case SDL_SYSTEM_CURSOR_ARROW:     cursorType = CoreCursorType::Arrow; break;
    case SDL_SYSTEM_CURSOR_IBEAM:     cursorType = CoreCursorType::IBeam; break;
    case SDL_SYSTEM_CURSOR_WAIT:      cursorType = CoreCursorType::Wait; break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR: cursorType = CoreCursorType::Cross; break;
    case SDL_SYSTEM_CURSOR_WAITARROW: cursorType = CoreCursorType::Wait; break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:  cursorType = CoreCursorType::SizeNorthwestSoutheast; break;
    case SDL_SYSTEM_CURSOR_SIZENESW:  cursorType = CoreCursorType::SizeNortheastSouthwest; break;
    case SDL_SYSTEM_CURSOR_SIZEWE:    cursorType = CoreCursorType::SizeWestEast; break;
    case SDL_SYSTEM_CURSOR_SIZENS:    cursorType = CoreCursorType::SizeNorthSouth; break;
    case SDL_SYSTEM_CURSOR_SIZEALL:   cursorType = CoreCursorType::SizeAll; break;
    case SDL_SYSTEM_CURSOR_NO:        cursorType = CoreCursorType::UniversalNo; break;
    case SDL_SYSTEM_CURSOR_HAND:      cursorType = CoreCursorType::Hand; break;
    }

    cursor = (SDL_Cursor *) SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        /* Create a pointer to a COM reference to a cursor.  The extra
           pointer is used (on top of the COM reference) to allow the cursor
           to be referenced by the SDL_cursor's driverdata field, which is
           a void pointer.
        */
        CoreCursor ^* theCursor = new CoreCursor^(nullptr);
        *theCursor = ref new CoreCursor(cursorType, 0);
        cursor->driverdata = (void *) theCursor;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
WINRT_CreateDefaultCursor()
{
    return WINRT_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
}

static void
WINRT_FreeCursor(SDL_Cursor * cursor)
{
    if (cursor->driverdata) {
        CoreCursor ^* theCursor = (CoreCursor ^*) cursor->driverdata;
        *theCursor = nullptr;       // Release the COM reference to the CoreCursor
        delete theCursor;           // Delete the pointer to the COM reference
    }
    SDL_free(cursor);
}

static int
WINRT_ShowCursor(SDL_Cursor * cursor)
{
    // TODO, WinRT, XAML: make WINRT_ShowCursor work when XAML support is enabled.
    if ( ! CoreWindow::GetForCurrentThread()) {
        return 0;
    }

    if (cursor) {
        CoreCursor ^* theCursor = (CoreCursor ^*) cursor->driverdata;
        CoreWindow::GetForCurrentThread()->PointerCursor = *theCursor;
    } else {
        CoreWindow::GetForCurrentThread()->PointerCursor = nullptr;
    }
    return 0;
}

static int
WINRT_SetRelativeMouseMode(SDL_bool enabled)
{
    WINRT_UsingRelativeMouseMode = enabled;
    return 0;
}

void
WINRT_InitMouse(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    /* DLudwig, Dec 3, 2012: WinRT does not currently provide APIs for
       the following features, AFAIK:
        - custom cursors  (multiple system cursors are, however, available)
        - programmatically moveable cursors
    */

#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
    //mouse->CreateCursor = WINRT_CreateCursor;
    mouse->CreateSystemCursor = WINRT_CreateSystemCursor;
    mouse->ShowCursor = WINRT_ShowCursor;
    mouse->FreeCursor = WINRT_FreeCursor;
    //mouse->WarpMouse = WINRT_WarpMouse;
    mouse->SetRelativeMouseMode = WINRT_SetRelativeMouseMode;

    SDL_SetDefaultCursor(WINRT_CreateDefaultCursor());
#endif
}

void
WINRT_QuitMouse(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_WINRT */

/* vi: set ts=4 sw=4 expandtab: */
