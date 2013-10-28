/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_WINDOWS

#include "../../core/windows/SDL_windows.h"

#include "SDL_assert.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_windowsvideo.h"
#include "SDL_windowswindow.h"

/* Dropfile support */
#include <shellapi.h>

/* This is included after SDL_windowsvideo.h, which includes windows.h */
#include "SDL_syswm.h"

/* Windows CE compatibility */
#ifndef SWP_NOCOPYBITS
#define SWP_NOCOPYBITS 0
#endif

/* Fake window to help with DirectInput events. */
HWND SDL_HelperWindow = NULL;
static WCHAR *SDL_HelperWindowClassName = TEXT("SDLHelperWindowInputCatcher");
static WCHAR *SDL_HelperWindowName = TEXT("SDLHelperWindowInputMsgWindow");
static ATOM SDL_HelperWindowClass = 0;

#define STYLE_BASIC         (WS_CLIPSIBLINGS | WS_CLIPCHILDREN)
#define STYLE_FULLSCREEN    (WS_POPUP)
#define STYLE_BORDERLESS    (WS_POPUP)
#define STYLE_NORMAL        (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define STYLE_RESIZABLE     (WS_THICKFRAME | WS_MAXIMIZEBOX)
#define STYLE_MASK          (STYLE_FULLSCREEN | STYLE_BORDERLESS | STYLE_NORMAL | STYLE_RESIZABLE)

static DWORD
GetWindowStyle(SDL_Window * window)
{
    DWORD style = 0;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        style |= STYLE_FULLSCREEN;
    } else {
        if (window->flags & SDL_WINDOW_BORDERLESS) {
            style |= STYLE_BORDERLESS;
        } else {
            style |= STYLE_NORMAL;
        }
        if (window->flags & SDL_WINDOW_RESIZABLE) {
            style |= STYLE_RESIZABLE;
        }
    }
    return style;
}

static void
WIN_SetWindowPositionInternal(_THIS, SDL_Window * window, UINT flags)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    RECT rect;
    DWORD style;
    HWND top;
    BOOL menu;
    int x, y;
    int w, h;

    /* Figure out what the window area will be */
    if (SDL_ShouldAllowTopmost() && (window->flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_INPUT_FOCUS)) == (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_INPUT_FOCUS)) {
        top = HWND_TOPMOST;
    } else {
        top = HWND_NOTOPMOST;
    }
    style = GetWindowLong(hwnd, GWL_STYLE);
    rect.left = 0;
    rect.top = 0;
    rect.right = window->w;
    rect.bottom = window->h;
    menu = (style & WS_CHILDWINDOW) ? FALSE : (GetMenu(hwnd) != NULL);
    AdjustWindowRectEx(&rect, style, menu, 0);
    w = (rect.right - rect.left);
    h = (rect.bottom - rect.top);
    x = window->x + rect.left;
    y = window->y + rect.top;

    SetWindowPos(hwnd, top, x, y, w, h, flags);
}

static int
SetupWindowData(_THIS, SDL_Window * window, HWND hwnd, SDL_bool created)
{
    SDL_VideoData *videodata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *data;

    /* Allocate the window data */
    data = (SDL_WindowData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    data->window = window;
    data->hwnd = hwnd;
    data->hdc = GetDC(hwnd);
    data->created = created;
    data->mouse_button_flags = 0;
    data->videodata = videodata;

    window->driverdata = data;

    /* Associate the data with the window */
    if (!SetProp(hwnd, TEXT("SDL_WindowData"), data)) {
        ReleaseDC(hwnd, data->hdc);
        SDL_free(data);
        return WIN_SetError("SetProp() failed");
    }

    /* Set up the window proc function */
#ifdef GWLP_WNDPROC
    data->wndproc = (WNDPROC) GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    if (data->wndproc == WIN_WindowProc) {
        data->wndproc = NULL;
    } else {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) WIN_WindowProc);
    }
#else
    data->wndproc = (WNDPROC) GetWindowLong(hwnd, GWL_WNDPROC);
    if (data->wndproc == WIN_WindowProc) {
        data->wndproc = NULL;
    } else {
        SetWindowLong(hwnd, GWL_WNDPROC, (LONG_PTR) WIN_WindowProc);
    }
#endif

    /* Fill in the SDL window with the window data */
    {
        RECT rect;
        if (GetClientRect(hwnd, &rect)) {
            int w = rect.right;
            int h = rect.bottom;
            if ((window->w && window->w != w) || (window->h && window->h != h)) {
                /* We tried to create a window larger than the desktop and Windows didn't allow it.  Override! */
                WIN_SetWindowPositionInternal(_this, window, SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE);
            } else {
                window->w = w;
                window->h = h;
            }
        }
    }
    {
        POINT point;
        point.x = 0;
        point.y = 0;
        if (ClientToScreen(hwnd, &point)) {
            window->x = point.x;
            window->y = point.y;
        }
    }
    {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        if (style & WS_VISIBLE) {
            window->flags |= SDL_WINDOW_SHOWN;
        } else {
            window->flags &= ~SDL_WINDOW_SHOWN;
        }
        if (style & (WS_BORDER | WS_THICKFRAME)) {
            window->flags &= ~SDL_WINDOW_BORDERLESS;
        } else {
            window->flags |= SDL_WINDOW_BORDERLESS;
        }
        if (style & WS_THICKFRAME) {
            window->flags |= SDL_WINDOW_RESIZABLE;
        } else {
            window->flags &= ~SDL_WINDOW_RESIZABLE;
        }
#ifdef WS_MAXIMIZE
        if (style & WS_MAXIMIZE) {
            window->flags |= SDL_WINDOW_MAXIMIZED;
        } else
#endif
        {
            window->flags &= ~SDL_WINDOW_MAXIMIZED;
        }
#ifdef WS_MINIMIZE
        if (style & WS_MINIMIZE) {
            window->flags |= SDL_WINDOW_MINIMIZED;
        } else
#endif
        {
            window->flags &= ~SDL_WINDOW_MINIMIZED;
        }
    }
    if (GetFocus() == hwnd) {
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_SetKeyboardFocus(data->window);

        if (window->flags & SDL_WINDOW_INPUT_GRABBED) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            ClientToScreen(hwnd, (LPPOINT) & rect);
            ClientToScreen(hwnd, (LPPOINT) & rect + 1);
            ClipCursor(&rect);
        }
    }

    /* Enable multi-touch */
    if (videodata->RegisterTouchWindow) {
        videodata->RegisterTouchWindow(hwnd, (TWF_FINETOUCH|TWF_WANTPALM));
    }

    /* Enable dropping files */
    DragAcceptFiles(hwnd, TRUE);

    /* All done! */
    return 0;
}

int
WIN_CreateWindow(_THIS, SDL_Window * window)
{
    HWND hwnd;
    RECT rect;
    DWORD style = STYLE_BASIC;
    int x, y;
    int w, h;

    style |= GetWindowStyle(window);

    /* Figure out what the window area will be */
    rect.left = window->x;
    rect.top = window->y;
    rect.right = window->x + window->w;
    rect.bottom = window->y + window->h;
    AdjustWindowRectEx(&rect, style, FALSE, 0);
    x = rect.left;
    y = rect.top;
    w = (rect.right - rect.left);
    h = (rect.bottom - rect.top);

    hwnd =
        CreateWindow(SDL_Appname, TEXT(""), style, x, y, w, h, NULL, NULL,
                     SDL_Instance, NULL);
    if (!hwnd) {
        return WIN_SetError("Couldn't create window");
    }

    WIN_PumpEvents(_this);

    if (SetupWindowData(_this, window, hwnd, SDL_TRUE) < 0) {
        DestroyWindow(hwnd);
        return -1;
    }
#if SDL_VIDEO_OPENGL_WGL
    if (window->flags & SDL_WINDOW_OPENGL) {
        if (WIN_GL_SetupWindow(_this, window) < 0) {
            WIN_DestroyWindow(_this, window);
            return -1;
        }
    }
#endif
    return 0;
}

int
WIN_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    HWND hwnd = (HWND) data;
    LPTSTR title;
    int titleLen;

    /* Query the title from the existing window */
    titleLen = GetWindowTextLength(hwnd);
    title = SDL_stack_alloc(TCHAR, titleLen + 1);
    if (title) {
        titleLen = GetWindowText(hwnd, title, titleLen);
    } else {
        titleLen = 0;
    }
    if (titleLen > 0) {
        window->title = WIN_StringToUTF8(title);
    }
    if (title) {
        SDL_stack_free(title);
    }

    if (SetupWindowData(_this, window, hwnd, SDL_FALSE) < 0) {
        return -1;
    }
    return 0;
}

void
WIN_SetWindowTitle(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    LPTSTR title;

    if (window->title) {
        title = WIN_UTF8ToString(window->title);
    } else {
        title = NULL;
    }
    SetWindowText(hwnd, title ? title : TEXT(""));
    SDL_free(title);
}

void
WIN_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    HICON hicon = NULL;
    BYTE *icon_bmp;
    int icon_len, y;
    SDL_RWops *dst;

    /* Create temporary bitmap buffer */
    icon_len = 40 + icon->h * icon->w * 4;
    icon_bmp = SDL_stack_alloc(BYTE, icon_len);
    dst = SDL_RWFromMem(icon_bmp, icon_len);
    if (!dst) {
        SDL_stack_free(icon_bmp);
        return;
    }

    /* Write the BITMAPINFO header */
    SDL_WriteLE32(dst, 40);
    SDL_WriteLE32(dst, icon->w);
    SDL_WriteLE32(dst, icon->h * 2);
    SDL_WriteLE16(dst, 1);
    SDL_WriteLE16(dst, 32);
    SDL_WriteLE32(dst, BI_RGB);
    SDL_WriteLE32(dst, icon->h * icon->w * 4);
    SDL_WriteLE32(dst, 0);
    SDL_WriteLE32(dst, 0);
    SDL_WriteLE32(dst, 0);
    SDL_WriteLE32(dst, 0);

    /* Write the pixels upside down into the bitmap buffer */
    SDL_assert(icon->format->format == SDL_PIXELFORMAT_ARGB8888);
    y = icon->h;
    while (y--) {
        Uint8 *src = (Uint8 *) icon->pixels + y * icon->pitch;
        SDL_RWwrite(dst, src, icon->pitch, 1);
    }

    hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);

    SDL_RWclose(dst);
    SDL_stack_free(icon_bmp);

    /* Set the icon for the window */
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hicon);

    /* Set the icon in the task manager (should we do this?) */
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) hicon);
}

void
WIN_SetWindowPosition(_THIS, SDL_Window * window)
{
    WIN_SetWindowPositionInternal(_this, window, SWP_NOCOPYBITS | SWP_NOSIZE | SWP_NOACTIVATE);
}

void
WIN_SetWindowSize(_THIS, SDL_Window * window)
{
    WIN_SetWindowPositionInternal(_this, window, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOACTIVATE);
}

void
WIN_ShowWindow(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    ShowWindow(hwnd, SW_SHOW);
}

void
WIN_HideWindow(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    ShowWindow(hwnd, SW_HIDE);
}

void
WIN_RaiseWindow(_THIS, SDL_Window * window)
{
    WIN_SetWindowPositionInternal(_this, window, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE);

    /* Raising the window while alt-tabbed can cause it to be minimized for some reason? */
    WIN_RestoreWindow(_this, window);
}

void
WIN_MaximizeWindow(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    ShowWindow(hwnd, SW_MAXIMIZE);
}

void
WIN_MinimizeWindow(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    ShowWindow(hwnd, SW_MINIMIZE);
}

void
WIN_SetWindowBordered(_THIS, SDL_Window * window, SDL_bool bordered)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);

    if (bordered) {
        style &= ~STYLE_BORDERLESS;
        style |= STYLE_NORMAL;
    } else {
        style &= ~STYLE_NORMAL;
        style |= STYLE_BORDERLESS;
    }

    SetWindowLong(hwnd, GWL_STYLE, style);
    WIN_SetWindowPositionInternal(_this, window, SWP_NOCOPYBITS | SWP_FRAMECHANGED | SWP_NOREPOSITION | SWP_NOZORDER |SWP_NOACTIVATE | SWP_NOSENDCHANGING);
}

void
WIN_RestoreWindow(_THIS, SDL_Window * window)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;

    ShowWindow(hwnd, SW_RESTORE);
}

void
WIN_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    HWND hwnd = data->hwnd;
    RECT rect;
    SDL_Rect bounds;
    DWORD style;
    HWND top;
    BOOL menu;
    int x, y;
    int w, h;

    if (SDL_ShouldAllowTopmost() && (window->flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_INPUT_FOCUS)) == (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_INPUT_FOCUS)) {
        top = HWND_TOPMOST;
    } else {
        top = HWND_NOTOPMOST;
    }

    style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~STYLE_MASK;
    style |= GetWindowStyle(window);

    WIN_GetDisplayBounds(_this, display, &bounds);

    if (fullscreen) {
        x = bounds.x;
        y = bounds.y;
        w = bounds.w;
        h = bounds.h;
    } else {
        rect.left = 0;
        rect.top = 0;
        rect.right = window->windowed.w;
        rect.bottom = window->windowed.h;
        menu = (style & WS_CHILDWINDOW) ? FALSE : (GetMenu(hwnd) != NULL);
        AdjustWindowRectEx(&rect, style, menu, 0);
        w = (rect.right - rect.left);
        h = (rect.bottom - rect.top);
        x = window->windowed.x + rect.left;
        y = window->windowed.y + rect.top;
    }
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, top, x, y, w, h, SWP_NOCOPYBITS);
}

int
WIN_SetWindowGammaRamp(_THIS, SDL_Window * window, const Uint16 * ramp)
{
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;
    HDC hdc;
    BOOL succeeded = FALSE;

    hdc = CreateDC(data->DeviceName, NULL, NULL, NULL);
    if (hdc) {
        succeeded = SetDeviceGammaRamp(hdc, (LPVOID)ramp);
        if (!succeeded) {
            WIN_SetError("SetDeviceGammaRamp()");
        }
        DeleteDC(hdc);
    }
    return succeeded ? 0 : -1;
}

int
WIN_GetWindowGammaRamp(_THIS, SDL_Window * window, Uint16 * ramp)
{
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;
    HDC hdc;
    BOOL succeeded = FALSE;

    hdc = CreateDC(data->DeviceName, NULL, NULL, NULL);
    if (hdc) {
        succeeded = GetDeviceGammaRamp(hdc, (LPVOID)ramp);
        if (!succeeded) {
            WIN_SetError("GetDeviceGammaRamp()");
        }
        DeleteDC(hdc);
    }
    return succeeded ? 0 : -1;
}

void
WIN_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;

    if (grabbed) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        ClientToScreen(hwnd, (LPPOINT) & rect);
        ClientToScreen(hwnd, (LPPOINT) & rect + 1);
        ClipCursor(&rect);
    } else {
        ClipCursor(NULL);
    }

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        UINT flags = SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE;

        if (!(window->flags & SDL_WINDOW_SHOWN)) {
            flags |= SWP_NOACTIVATE;
        }
        WIN_SetWindowPositionInternal(_this, window, flags);
    }
}

void
WIN_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (data) {
        ReleaseDC(data->hwnd, data->hdc);
        if (data->created) {
            DestroyWindow(data->hwnd);
        } else {
            /* Restore any original event handler... */
            if (data->wndproc != NULL) {
#ifdef GWLP_WNDPROC
                SetWindowLongPtr(data->hwnd, GWLP_WNDPROC,
                                 (LONG_PTR) data->wndproc);
#else
                SetWindowLong(data->hwnd, GWL_WNDPROC,
                              (LONG_PTR) data->wndproc);
#endif
            }
        }
        SDL_free(data);
    }
}

SDL_bool
WIN_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    HWND hwnd = ((SDL_WindowData *) window->driverdata)->hwnd;
    if (info->version.major <= SDL_MAJOR_VERSION) {
        info->subsystem = SDL_SYSWM_WINDOWS;
        info->info.win.window = hwnd;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}


/*
 * Creates a HelperWindow used for DirectInput events.
 */
int
SDL_HelperWindowCreate(void)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wce;

    /* Make sure window isn't created twice. */
    if (SDL_HelperWindow != NULL) {
        return 0;
    }

    /* Create the class. */
    SDL_zero(wce);
    wce.lpfnWndProc = DefWindowProc;
    wce.lpszClassName = (LPCWSTR) SDL_HelperWindowClassName;
    wce.hInstance = hInstance;

    /* Register the class. */
    SDL_HelperWindowClass = RegisterClass(&wce);
    if (SDL_HelperWindowClass == 0) {
        return WIN_SetError("Unable to create Helper Window Class");
    }

    /* Create the window. */
    SDL_HelperWindow = CreateWindowEx(0, SDL_HelperWindowClassName,
                                      SDL_HelperWindowName,
                                      WS_OVERLAPPED, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, HWND_MESSAGE, NULL,
                                      hInstance, NULL);
    if (SDL_HelperWindow == NULL) {
        UnregisterClass(SDL_HelperWindowClassName, hInstance);
        return WIN_SetError("Unable to create Helper Window");
    }

    return 0;
}


/*
 * Destroys the HelperWindow previously created with SDL_HelperWindowCreate.
 */
void
SDL_HelperWindowDestroy(void)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    /* Destroy the window. */
    if (SDL_HelperWindow != NULL) {
        if (DestroyWindow(SDL_HelperWindow) == 0) {
            WIN_SetError("Unable to destroy Helper Window");
            return;
        }
        SDL_HelperWindow = NULL;
    }

    /* Unregister the class. */
    if (SDL_HelperWindowClass != 0) {
        if ((UnregisterClass(SDL_HelperWindowClassName, hInstance)) == 0) {
            WIN_SetError("Unable to destroy Helper Window Class");
            return;
        }
        SDL_HelperWindowClass = 0;
    }
}

void WIN_OnWindowEnter(_THIS, SDL_Window * window)
{
#ifdef WM_MOUSELEAVE
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    TRACKMOUSEEVENT trackMouseEvent;

    if (!data || !data->hwnd) {
        /* The window wasn't fully initialized */
        return;
    }

    trackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
    trackMouseEvent.dwFlags = TME_LEAVE;
    trackMouseEvent.hwndTrack = data->hwnd;

    TrackMouseEvent(&trackMouseEvent);
#endif /* WM_MOUSELEAVE */
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
