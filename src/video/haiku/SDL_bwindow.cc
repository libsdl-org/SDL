/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_HAIKU
#include "../SDL_sysvideo.h"

#include "SDL_BWin.h"
#include <new>

/* Define a path to window's BWIN data */
#ifdef __cplusplus
extern "C" {
#endif

static SDL_INLINE SDL_BWin *_ToBeWin(SDL_Window *window) {
    return (SDL_BWin *)(window->driverdata);
}

static SDL_INLINE SDL_BLooper *_GetBeLooper() {
    return SDL_Looper;
}

static int _InitWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props) {
    uint32 flags = 0;
    window_look look = B_TITLED_WINDOW_LOOK;

    BRect bounds(
        window->x,
        window->y,
        window->x + window->w - 1,    //BeWindows have an off-by-one px w/h thing
        window->y + window->h - 1
    );

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        /* TODO: Add support for this flag */
        printf(__FILE__": %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__LINE__);
    }
    if (window->flags & SDL_WINDOW_OPENGL) {
        /* TODO: Add support for this flag */
    }
    if (!(window->flags & SDL_WINDOW_RESIZABLE)) {
        flags |= B_NOT_RESIZABLE | B_NOT_ZOOMABLE;
    }
    if (window->flags & SDL_WINDOW_BORDERLESS) {
        look = B_NO_BORDER_WINDOW_LOOK;
    }

    SDL_BWin *bwin = new(std::nothrow) SDL_BWin(bounds, look, flags);
    if (!bwin) {
        return -1;
    }

    window->driverdata = (SDL_WindowData *)bwin;
    int32 winID = _GetBeLooper()->GetID(window);
    bwin->SetID(winID);

    return 0;
}

int HAIKU_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props) {
    if (_InitWindow(_this, window, create_props) < 0) {
        return -1;
    }

    /* Start window loop */
    _ToBeWin(window)->Show();
    return 0;
}

void HAIKU_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_SET_TITLE);
    msg.AddString("window-title", window->title);
    _ToBeWin(window)->PostMessage(&msg);
}

int HAIKU_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_MOVE_WINDOW);
    msg.AddInt32("window-x", window->floating.x);
    msg.AddInt32("window-y", window->floating.y);
    _ToBeWin(window)->PostMessage(&msg);
    return 0;
}

void HAIKU_SetWindowSize(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_RESIZE_WINDOW);
    msg.AddInt32("window-w", window->floating.w - 1);
    msg.AddInt32("window-h", window->floating.h - 1);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window * window, SDL_bool bordered) {
    BMessage msg(BWIN_SET_BORDERED);
    msg.AddBool("window-border", bordered != SDL_FALSE);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window * window, SDL_bool resizable) {
    BMessage msg(BWIN_SET_RESIZABLE);
    msg.AddBool("window-resizable", resizable != SDL_FALSE);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_ShowWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_SHOW_WINDOW);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_HideWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_HIDE_WINDOW);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_RaiseWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_SHOW_WINDOW);    /* Activate this window and move to front */
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_MAXIMIZE_WINDOW);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_MINIMIZE_WINDOW);
    _ToBeWin(window)->PostMessage(&msg);
}

void HAIKU_RestoreWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_RESTORE_WINDOW);
    _ToBeWin(window)->PostMessage(&msg);
}

int HAIKU_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window * window,
        SDL_VideoDisplay * display, SDL_FullscreenOp fullscreen) {
    /* Haiku tracks all video display information */
    BMessage msg(BWIN_FULLSCREEN);
    msg.AddBool("fullscreen", !!fullscreen);
    _ToBeWin(window)->PostMessage(&msg);
    return 0;
}


void HAIKU_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window * window) {
    BMessage msg(BWIN_MINIMUM_SIZE_WINDOW);
    msg.AddInt32("window-w", window->w -1);
    msg.AddInt32("window-h", window->h -1);
    _ToBeWin(window)->PostMessage(&msg);
}

int HAIKU_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window * window, SDL_bool grabbed) {
    /* TODO: Implement this! */
    return SDL_Unsupported();
}

int HAIKU_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window) {
    if (modal_window->parent && modal_window->parent != parent_window) {
        /* Remove from the subset of a previous parent. */
        _ToBeWin(modal_window)->RemoveFromSubset(_ToBeWin(modal_window->parent));
    }

    if (parent_window) {
        _ToBeWin(modal_window)->SetLook(B_MODAL_WINDOW_LOOK);
        _ToBeWin(modal_window)->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
        _ToBeWin(modal_window)->AddToSubset(_ToBeWin(parent_window));
    } else {
        window_look look = (modal_window->flags & SDL_WINDOW_BORDERLESS) ? B_NO_BORDER_WINDOW_LOOK : B_TITLED_WINDOW_LOOK;
        _ToBeWin(modal_window)->SetLook(look);
        _ToBeWin(modal_window)->SetFeel(B_NORMAL_WINDOW_FEEL);
    }

    return 0;
}

void HAIKU_DestroyWindow(SDL_VideoDevice *_this, SDL_Window * window) {
    _ToBeWin(window)->LockLooper();    /* This MUST be locked */
    _GetBeLooper()->ClearID(_ToBeWin(window));
    _ToBeWin(window)->Quit();
    window->driverdata = NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* SDL_VIDEO_DRIVER_HAIKU */
