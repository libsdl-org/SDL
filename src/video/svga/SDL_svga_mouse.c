/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2020 Jay Petacat <jay@jayschwa.net>

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

#if SDL_VIDEO_DRIVER_SVGA

#include "SDL_svga_mouse.h"

#include <dpmi.h>

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"

#define MOUSE_INTERRUPT 0x33

static Uint8 DOS_MouseButtons[] = {
    SDL_BUTTON_LEFT,
    SDL_BUTTON_RIGHT,
    SDL_BUTTON_MIDDLE,
};

static SDL_Cursor *
DOS_CreateCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));

    return cursor;
}

static SDL_Cursor *
DOS_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));

    return cursor;
}

static void
DOS_FreeCursor(SDL_Cursor * cursor)
{
    SDL_free(cursor);
}

static int
DOS_ShowCursor(SDL_Cursor * cursor)
{
    __dpmi_regs r;

    r.x.ax = cursor ? 1 : 2;

    return __dpmi_int(MOUSE_INTERRUPT, &r);
}

static void
DOS_WarpMouse(SDL_Window * window, int x, int y)
{
}

void
DOS_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    __dpmi_regs r;

    r.x.ax = 0;

    if (__dpmi_int(MOUSE_INTERRUPT, &r) || r.x.ax != 0xFFFF) {
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "DOS: No mouse installed");
        return;
    }

    mouse->CreateCursor = DOS_CreateCursor;
    mouse->CreateSystemCursor = DOS_CreateSystemCursor;
    mouse->ShowCursor = DOS_ShowCursor;
    mouse->FreeCursor = DOS_FreeCursor;
    mouse->WarpMouse = DOS_WarpMouse;

    // SDL_SetDefaultCursor(DEFAULT_CURSOR);
}

void
DOS_QuitMouse(void)
{
}

void DOS_PollMouse(void)
{
    static int last_button_status;
    int button_status, i;
    __dpmi_regs r;

    /* TODO: Determine if movement happened using interrupt callback. */

    r.x.ax = 0xB;

    if (__dpmi_int(MOUSE_INTERRUPT, &r)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DOS: Failed to query mouse position");
        DOS_QuitMouse();
        return;
    }

    if (r.x.cx || r.x.dx) {
        SDL_SendMouseMotion(NULL, 0, SDL_TRUE, (Sint16)r.x.cx, (Sint16)r.x.dx);
    }

    r.x.ax = 3;

    if (__dpmi_int(MOUSE_INTERRUPT, &r)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "DOS: Failed to query mouse state");
        DOS_QuitMouse();
        return;
    }

    button_status = r.x.bx;

    for (i = 0; i < SDL_arraysize(DOS_MouseButtons); i++) {
        int mask = 1 << i;
        int diff = button_status ^ last_button_status;

        if (diff & mask) {
            Uint8 state = button_status & mask ? SDL_PRESSED : SDL_RELEASED;
            SDL_SendMouseButton(NULL, 0, state, DOS_MouseButtons[i]);
        }
    }

    last_button_status = button_status;
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
