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

#ifdef SDL_VIDEO_DRIVER_WINDOWS

#include "../../core/windows/SDL_windows.h"

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../SDL_hints_c.h"

#include "SDL_windowsvideo.h"
#include "SDL_windowswindow.h"

/* Dropfile support */
#include <shellapi.h>

/* DWM setting support */
typedef HRESULT (WINAPI *DwmSetWindowAttribute_t)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
typedef HRESULT (WINAPI *DwmGetWindowAttribute_t)(HWND hwnd, DWORD dwAttribute, PVOID pvAttribute, DWORD cbAttribute);

/* Dark mode support*/
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* Corner rounding support  (Win 11+) */
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE 
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
typedef enum {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3
} DWM_WINDOW_CORNER_PREFERENCE;

/* Border Color support (Win 11+) */
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

#ifndef DWMWA_COLOR_DEFAULT
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#endif

#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif

/* Transparent window support */
#ifndef DWM_BB_ENABLE
#define DWM_BB_ENABLE 0x00000001
#endif
#ifndef DWM_BB_BLURREGION
#define DWM_BB_BLURREGION 0x00000002
#endif
typedef struct
{
    DWORD flags;
    BOOL enable;
    HRGN blur_region;
    BOOL transition_on_maxed;
} DWM_BLURBEHIND;
typedef HRESULT(WINAPI *DwmEnableBlurBehindWindow_t)(HWND hwnd, const DWM_BLURBEHIND *pBlurBehind);

/* Windows CE compatibility */
#ifndef SWP_NOCOPYBITS
#define SWP_NOCOPYBITS 0
#endif

/* An undocumented message to create a popup system menu
 * - wParam is always 0
 * - lParam = MAKELONG(x, y) where x and y are the screen coordinates where the menu should be displayed
 */
#ifndef WM_POPUPSYSTEMMENU
#define WM_POPUPSYSTEMMENU 0x313
#endif

/* #define HIGHDPI_DEBUG */

/* Fake window to help with DirectInput events. */
HWND SDL_HelperWindow = NULL;
static const TCHAR *SDL_HelperWindowClassName = TEXT("SDLHelperWindowInputCatcher");
static const TCHAR *SDL_HelperWindowName = TEXT("SDLHelperWindowInputMsgWindow");
static ATOM SDL_HelperWindowClass = 0;

/* For borderless Windows, still want the following flag:
   - WS_MINIMIZEBOX: window will respond to Windows minimize commands sent to all windows, such as windows key + m, shaking title bar, etc.
   Additionally, non-fullscreen windows can add:
   - WS_CAPTION: this seems to enable the Windows minimize animation
   - WS_SYSMENU: enables system context menu on task bar
   This will also cause the task bar to overlap the window and other windowed behaviors, so only use this for windows that shouldn't appear to be fullscreen
 */

#define STYLE_BASIC               (WS_CLIPSIBLINGS | WS_CLIPCHILDREN)
#define STYLE_FULLSCREEN          (WS_POPUP | WS_MINIMIZEBOX)
#define STYLE_BORDERLESS          (WS_POPUP | WS_MINIMIZEBOX)
#define STYLE_BORDERLESS_WINDOWED (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define STYLE_NORMAL              (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define STYLE_RESIZABLE           (WS_THICKFRAME | WS_MAXIMIZEBOX)
#define STYLE_MASK                (STYLE_FULLSCREEN | STYLE_BORDERLESS | STYLE_NORMAL | STYLE_RESIZABLE)

static DWORD GetWindowStyle(SDL_Window *window)
{
    DWORD style = 0;

    if (SDL_WINDOW_IS_POPUP(window)) {
        style |= WS_POPUP;
    } else if (window->flags & SDL_WINDOW_FULLSCREEN) {
        style |= STYLE_FULLSCREEN;
    } else {
        if (window->flags & SDL_WINDOW_BORDERLESS) {
            /* This behavior more closely matches other platform where the window is borderless
               but still interacts with the window manager (e.g. task bar shows above it, it can
               be resized to fit within usable desktop area, etc.)
             */
            if (SDL_GetHintBoolean("SDL_BORDERLESS_WINDOWED_STYLE", SDL_TRUE)) {
                style |= STYLE_BORDERLESS_WINDOWED;
            } else {
                style |= STYLE_BORDERLESS;
            }
        } else {
            style |= STYLE_NORMAL;
        }

        if (window->flags & SDL_WINDOW_RESIZABLE) {
            /* You can have a borderless resizable window, but Windows doesn't always draw it correctly,
               see https://bugzilla.libsdl.org/show_bug.cgi?id=4466
             */
            if (!(window->flags & SDL_WINDOW_BORDERLESS) ||
                SDL_GetHintBoolean("SDL_BORDERLESS_RESIZABLE_STYLE", SDL_FALSE)) {
                style |= STYLE_RESIZABLE;
            }
        }

        /* Need to set initialize minimize style, or when we call ShowWindow with WS_MINIMIZE it will activate a random window */
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            style |= WS_MINIMIZE;
        }
    }
    return style;
}

static DWORD GetWindowStyleEx(SDL_Window *window)
{
    DWORD style = 0;

    if (SDL_WINDOW_IS_POPUP(window) || (window->flags & SDL_WINDOW_UTILITY)) {
        style |= WS_EX_TOOLWINDOW;
    }
    if (SDL_WINDOW_IS_POPUP(window) || (window->flags & SDL_WINDOW_NOT_FOCUSABLE)) {
        style |= WS_EX_NOACTIVATE;
    }
    return style;
}

/**
 * Returns arguments to pass to SetWindowPos - the window rect, including frame, in Windows coordinates.
 * Can be called before we have a HWND.
 */
static int WIN_AdjustWindowRectWithStyle(SDL_Window *window, DWORD style, DWORD styleEx, BOOL menu, int *x, int *y, int *width, int *height, SDL_WindowRect rect_type)
{
    SDL_VideoData *videodata = SDL_GetVideoDevice() ? SDL_GetVideoDevice()->driverdata : NULL;
    RECT rect;

    /* Client rect, in points */
    switch (rect_type) {
        case SDL_WINDOWRECT_CURRENT:
            SDL_RelativeToGlobalForWindow(window, window->x, window->y, x, y);
            *width = window->w;
            *height = window->h;
            break;
        case SDL_WINDOWRECT_WINDOWED:
            SDL_RelativeToGlobalForWindow(window, window->windowed.x, window->windowed.y, x, y);
            *width = window->windowed.w;
            *height = window->windowed.h;
            break;
        case SDL_WINDOWRECT_FLOATING:
            SDL_RelativeToGlobalForWindow(window, window->floating.x, window->floating.y, x, y);
            *width = window->floating.w;
            *height = window->floating.h;
            break;
        default:
            /* Should never be here */
            SDL_assert_release(SDL_FALSE);
            *width = 0;
            *height = 0;
    }

    /* Copy the client size in pixels into this rect structure,
       which we'll then adjust with AdjustWindowRectEx */
    rect.left = 0;
    rect.top = 0;
    rect.right = *width;
    rect.bottom = *height;

    /* borderless windows will have WM_NCCALCSIZE return 0 for the non-client area. When this happens, it looks like windows will send a resize message
       expanding the window client area to the previous window + chrome size, so shouldn't need to adjust the window size for the set styles.
     */
    if (!(window->flags & SDL_WINDOW_BORDERLESS)) {
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
        AdjustWindowRectEx(&rect, style, menu, 0);
#else
        if (WIN_IsPerMonitorV2DPIAware(SDL_GetVideoDevice())) {
            /* With per-monitor v2, the window border/titlebar size depend on the DPI, so we need to call AdjustWindowRectExForDpi instead of
               AdjustWindowRectEx. */
            if (videodata) {
                UINT frame_dpi;
                SDL_WindowData *data = window->driverdata;
                frame_dpi = (data && videodata->GetDpiForWindow) ? videodata->GetDpiForWindow(data->hwnd) : USER_DEFAULT_SCREEN_DPI;
                if (videodata->AdjustWindowRectExForDpi(&rect, style, menu, styleEx, frame_dpi) == 0) {
                    return WIN_SetError("AdjustWindowRectExForDpi()");
                }
            }
        } else {
            if (AdjustWindowRectEx(&rect, style, menu, styleEx) == 0) {
                return WIN_SetError("AdjustWindowRectEx()");
            }
        }
#endif
    }

    /* Final rect in Windows screen space, including the frame */
    *x += rect.left;
    *y += rect.top;
    *width = (rect.right - rect.left);
    *height = (rect.bottom - rect.top);

#ifdef HIGHDPI_DEBUG
    SDL_Log("WIN_AdjustWindowRectWithStyle: in: %d, %d, %dx%d, returning: %d, %d, %dx%d, used dpi %d for frame calculation",
            (rect_type == SDL_WINDOWRECT_FLOATING ? window->floating.x : rect_type == SDL_WINDOWRECT_WINDOWED ? window->windowed.x : window->x),
            (rect_type == SDL_WINDOWRECT_FLOATING ? window->floating.y : rect_type == SDL_WINDOWRECT_WINDOWED ? window->windowed.y : window->y),
            (rect_type == SDL_WINDOWRECT_FLOATING ? window->floating.w : rect_type == SDL_WINDOWRECT_WINDOWED ? window->windowed.w : window->w),
            (rect_type == SDL_WINDOWRECT_FLOATING ? window->floating.h : rect_type == SDL_WINDOWRECT_WINDOWED ? window->windowed.h : window->h),
            *x, *y, *width, *height, frame_dpi);
#endif
    return 0;
}

int WIN_AdjustWindowRect(SDL_Window *window, int *x, int *y, int *width, int *height, SDL_WindowRect rect_type)
{
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    DWORD style, styleEx;
    BOOL menu;

    style = GetWindowLong(hwnd, GWL_STYLE);
    styleEx = GetWindowLong(hwnd, GWL_EXSTYLE);
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    menu = FALSE;
#else
    menu = (style & WS_CHILDWINDOW) ? FALSE : (GetMenu(hwnd) != NULL);
#endif
    return WIN_AdjustWindowRectWithStyle(window, style, styleEx, menu, x, y, width, height, rect_type);
}

int WIN_AdjustWindowRectForHWND(HWND hwnd, LPRECT lpRect, UINT frame_dpi)
{
    SDL_VideoDevice *videodevice = SDL_GetVideoDevice();
    SDL_VideoData *videodata = videodevice ? videodevice->driverdata : NULL;
    DWORD style, styleEx;
    BOOL menu;

    style = GetWindowLong(hwnd, GWL_STYLE);
    styleEx = GetWindowLong(hwnd, GWL_EXSTYLE);
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    menu = FALSE;
#else
    menu = (style & WS_CHILDWINDOW) ? FALSE : (GetMenu(hwnd) != NULL);
#endif

#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    AdjustWindowRectEx(lpRect, style, menu, styleEx);
#else
    if (WIN_IsPerMonitorV2DPIAware(videodevice)) {
        /* With per-monitor v2, the window border/titlebar size depend on the DPI, so we need to call AdjustWindowRectExForDpi instead of AdjustWindowRectEx. */
        if (!frame_dpi) {
            frame_dpi = videodata->GetDpiForWindow ? videodata->GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
        }
        if (!videodata->AdjustWindowRectExForDpi(lpRect, style, menu, styleEx, frame_dpi)) {
            return WIN_SetError("AdjustWindowRectExForDpi()");
        }
    } else {
        if (!AdjustWindowRectEx(lpRect, style, menu, styleEx)) {
            return WIN_SetError("AdjustWindowRectEx()");
        }
    }
#endif
    return 0;
}

int WIN_SetWindowPositionInternal(SDL_Window *window, UINT flags, SDL_WindowRect rect_type)
{
    SDL_Window *child_window;
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    HWND top;
    int x, y;
    int w, h;
    int result = 0;

    /* Figure out what the window area will be */
    if (SDL_ShouldAllowTopmost() && (window->flags & SDL_WINDOW_ALWAYS_ON_TOP)) {
        top = HWND_TOPMOST;
    } else {
        top = HWND_NOTOPMOST;
    }

    WIN_AdjustWindowRect(window, &x, &y, &w, &h, rect_type);

    data->expected_resize = SDL_TRUE;
    if (SetWindowPos(hwnd, top, x, y, w, h, flags) == 0) {
        result = WIN_SetError("SetWindowPos()");
    }
    data->expected_resize = SDL_FALSE;

    /* Update any child windows */
    for (child_window = window->first_child; child_window; child_window = child_window->next_sibling) {
        if (WIN_SetWindowPositionInternal(child_window, flags, SDL_WINDOWRECT_CURRENT) < 0) {
            result = -1;
        }
    }
    return result;
}

static void SDLCALL WIN_MouseRelativeModeCenterChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_WindowData *data = (SDL_WindowData *)userdata;
    data->mouse_relative_mode_center = SDL_GetStringBoolean(hint, SDL_TRUE);
}

static SDL_WindowEraseBackgroundMode GetEraseBackgroundModeHint()
{
    const char *hint = SDL_GetHint(SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE);
    if (!hint)
        return SDL_ERASEBACKGROUNDMODE_INITIAL;

    if (SDL_strstr(hint, "never"))
        return SDL_ERASEBACKGROUNDMODE_NEVER;

    if (SDL_strstr(hint, "initial"))
        return SDL_ERASEBACKGROUNDMODE_INITIAL;

    if (SDL_strstr(hint, "always"))
        return SDL_ERASEBACKGROUNDMODE_ALWAYS;

    int mode = SDL_GetStringInteger(hint, 1);
    if (mode < 0 || mode > 2) {
        SDL_Log("GetEraseBackgroundModeHint: invalid value for SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE. Fallback to default");
        return SDL_ERASEBACKGROUNDMODE_INITIAL;
    }

    return mode;
}

static int SetupWindowData(SDL_VideoDevice *_this, SDL_Window *window, HWND hwnd, HWND parent)
{
    SDL_VideoData *videodata = _this->driverdata;
    SDL_WindowData *data;

    /* Allocate the window data */
    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return -1;
    }
    data->window = window;
    data->hwnd = hwnd;
    data->parent = parent;
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    data->hdc = (HDC)data->hwnd;
#else
    data->hdc = GetDC(hwnd);
#endif
    data->hinstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    data->mouse_button_flags = (WPARAM)-1;
    data->last_pointer_update = (LPARAM)-1;
    data->videodata = videodata;
    data->initializing = SDL_TRUE;
    data->last_displayID = window->last_displayID;
    data->dwma_border_color = DWMWA_COLOR_DEFAULT;
    data->hint_erase_background_mode = GetEraseBackgroundModeHint();

    if (SDL_GetHintBoolean("SDL_WINDOW_RETAIN_CONTENT", SDL_FALSE)) {
        data->copybits_flag = 0;
    } else {
        data->copybits_flag = SWP_NOCOPYBITS;
    }

#ifdef HIGHDPI_DEBUG
    SDL_Log("SetupWindowData: initialized data->scaling_dpi to %d", data->scaling_dpi);
#endif

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, WIN_MouseRelativeModeCenterChanged, data);

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    /* Associate the data with the window */
    if (!SetProp(hwnd, TEXT("SDL_WindowData"), data)) {
        ReleaseDC(hwnd, data->hdc);
        SDL_free(data);
        return WIN_SetError("SetProp() failed");
    }
#endif
    
    window->driverdata = data;

    /* Set up the window proc function */
#ifdef GWLP_WNDPROC
    data->wndproc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    if (data->wndproc == WIN_WindowProc) {
        data->wndproc = NULL;
    } else {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WIN_WindowProc);
    }
#else
    data->wndproc = (WNDPROC)GetWindowLong(hwnd, GWL_WNDPROC);
    if (data->wndproc == WIN_WindowProc) {
        data->wndproc = NULL;
    } else {
        SetWindowLong(hwnd, GWL_WNDPROC, (LONG_PTR)WIN_WindowProc);
    }
#endif

    /* Fill in the SDL window with the window state */
    {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        if (style & WS_VISIBLE) {
            window->flags &= ~SDL_WINDOW_HIDDEN;
        } else {
            window->flags |= SDL_WINDOW_HIDDEN;
        }
        if (style & WS_POPUP) {
            window->flags |= SDL_WINDOW_BORDERLESS;
        } else {
            window->flags &= ~SDL_WINDOW_BORDERLESS;
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
    if (!(window->flags & SDL_WINDOW_MINIMIZED)) {
        RECT rect;
        if (GetClientRect(hwnd, &rect) && !WIN_IsRectEmpty(&rect)) {
            int w = rect.right;
            int h = rect.bottom;

            if (window->flags & SDL_WINDOW_EXTERNAL) {
                window->floating.w = window->windowed.w = window->w = w;
                window->floating.h = window->windowed.h = window->h = h;
            } else if ((window->windowed.w && window->windowed.w != w) || (window->windowed.h && window->windowed.h != h)) {
                /* We tried to create a window larger than the desktop and Windows didn't allow it.  Override! */
                int x, y;
                /* Figure out what the window area will be */
                WIN_AdjustWindowRect(window, &x, &y, &w, &h, SDL_WINDOWRECT_FLOATING);
                data->expected_resize = SDL_TRUE;
                SetWindowPos(hwnd, NULL, x, y, w, h, data->copybits_flag | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                data->expected_resize = SDL_FALSE;
            } else {
                window->w = w;
                window->h = h;
            }
        }
    }
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    if (!(window->flags & SDL_WINDOW_MINIMIZED)) {
        POINT point;
        point.x = 0;
        point.y = 0;
        if (ClientToScreen(hwnd, &point)) {
            if (window->flags & SDL_WINDOW_EXTERNAL) {
                window->floating.x = window->windowed.x = point.x;
                window->floating.y = window->windowed.y = point.y;
            }
            window->x = point.x;
            window->y = point.y;
        }
    }

    WIN_UpdateWindowICCProfile(window, SDL_FALSE);
#endif

#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    window->flags |= SDL_WINDOW_INPUT_FOCUS;
#else
    if (GetFocus() == hwnd) {
        window->flags |= SDL_WINDOW_INPUT_FOCUS;
        SDL_SetKeyboardFocus(window);
        WIN_UpdateClipCursor(window);
    }
#endif

    if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
        WIN_SetWindowAlwaysOnTop(_this, window, SDL_TRUE);
    } else {
        WIN_SetWindowAlwaysOnTop(_this, window, SDL_FALSE);
    }

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    /* Enable multi-touch */
    if (videodata->RegisterTouchWindow) {
        videodata->RegisterTouchWindow(hwnd, (TWF_FINETOUCH | TWF_WANTPALM));
    }
#endif

    if (data->parent && !window->parent) {
        data->destroy_parent_with_window = SDL_TRUE;
    }

    data->initializing = SDL_FALSE;

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    if (window->flags & SDL_WINDOW_EXTERNAL) {
        /* Query the title from the existing window */
        LPTSTR title;
        int titleLen;
        SDL_bool isstack;

        titleLen = GetWindowTextLength(hwnd);
        title = SDL_small_alloc(TCHAR, titleLen + 1, &isstack);
        if (title) {
            titleLen = GetWindowText(hwnd, title, titleLen + 1);
        } else {
            titleLen = 0;
        }
        if (titleLen > 0) {
            window->title = WIN_StringToUTF8(title);
        }
        if (title) {
            SDL_small_free(title, isstack);
        }
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    SDL_SetProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, data->hwnd);
    SDL_SetProperty(props, SDL_PROP_WINDOW_WIN32_HDC_POINTER, data->hdc);
    SDL_SetProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, data->hinstance);

    /* All done! */
    return 0;
}

static void CleanupWindowData(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;

    if (data) {
        SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, WIN_MouseRelativeModeCenterChanged, data);

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
        if (data->ICMFileName) {
            SDL_free(data->ICMFileName);
        }
        if (data->keyboard_hook) {
            UnhookWindowsHookEx(data->keyboard_hook);
        }
        ReleaseDC(data->hwnd, data->hdc);
        RemoveProp(data->hwnd, TEXT("SDL_WindowData"));
#endif
        if (!(window->flags & SDL_WINDOW_EXTERNAL)) {
            DestroyWindow(data->hwnd);
            if (data->destroy_parent_with_window && data->parent) {
                DestroyWindow(data->parent);
            }
        } else {
            /* Restore any original event handler... */
            if (data->wndproc) {
#ifdef GWLP_WNDPROC
                SetWindowLongPtr(data->hwnd, GWLP_WNDPROC,
                                 (LONG_PTR)data->wndproc);
#else
                SetWindowLong(data->hwnd, GWL_WNDPROC,
                              (LONG_PTR)data->wndproc);
#endif
            }
        }
        SDL_free(data);
    }
    window->driverdata = NULL;
}

static void WIN_ConstrainPopup(SDL_Window *window)
{
    /* Clamp popup windows to the output borders */
    if (SDL_WINDOW_IS_POPUP(window)) {
        SDL_Window *w;
        SDL_DisplayID displayID;
        SDL_Rect rect;
        int abs_x = window->floating.x;
        int abs_y = window->floating.y;
        int offset_x = 0, offset_y = 0;

        /* Calculate the total offset from the parents */
        for (w = window->parent; w->parent; w = w->parent) {
            offset_x += w->x;
            offset_y += w->y;
        }

        offset_x += w->x;
        offset_y += w->y;
        abs_x += offset_x;
        abs_y += offset_y;

        /* Constrain the popup window to the display of the toplevel parent */
        displayID = SDL_GetDisplayForWindow(w);
        SDL_GetDisplayBounds(displayID, &rect);
        if (abs_x + window->floating.w > rect.x + rect.w) {
            abs_x -= (abs_x + window->floating.w) - (rect.x + rect.w);
        }
        if (abs_y + window->floating.h > rect.y + rect.h) {
            abs_y -= (abs_y + window->floating.h) - (rect.y + rect.h);
        }
        abs_x = SDL_max(abs_x, rect.x);
        abs_y = SDL_max(abs_y, rect.y);

        window->floating.x = abs_x - offset_x;
        window->floating.y = abs_y - offset_y;
    }
}

static void WIN_SetKeyboardFocus(SDL_Window *window)
{
    SDL_Window *topmost = window;

    /* Find the topmost parent */
    while (topmost->parent) {
        topmost = topmost->parent;
    }

    topmost->driverdata->keyboard_focus = window;
    SDL_SetKeyboardFocus(window);
}

int WIN_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    HWND hwnd = (HWND)SDL_GetProperty(create_props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, SDL_GetProperty(create_props, "sdl2-compat.external_window", NULL));
    HWND parent = NULL;
    if (hwnd) {
        window->flags |= SDL_WINDOW_EXTERNAL;

        if (SetupWindowData(_this, window, hwnd, parent) < 0) {
            return -1;
        }
    } else {
        DWORD style = STYLE_BASIC;
        DWORD styleEx = 0;
        int x, y;
        int w, h;

        if (SDL_WINDOW_IS_POPUP(window)) {
            parent = window->parent->driverdata->hwnd;
        } else if (window->flags & SDL_WINDOW_UTILITY) {
            parent = CreateWindow(SDL_Appname, TEXT(""), STYLE_BASIC, 0, 0, 32, 32, NULL, NULL, SDL_Instance, NULL);
        }

        style |= GetWindowStyle(window);
        styleEx |= GetWindowStyleEx(window);

        /* Figure out what the window area will be */
        WIN_ConstrainPopup(window);
        WIN_AdjustWindowRectWithStyle(window, style, styleEx, FALSE, &x, &y, &w, &h, SDL_WINDOWRECT_FLOATING);

        hwnd = CreateWindowEx(styleEx, SDL_Appname, TEXT(""), style,
                              x, y, w, h, parent, NULL, SDL_Instance, NULL);
        if (!hwnd) {
            return WIN_SetError("Couldn't create window");
        }

        WIN_UpdateDarkModeForHWND(hwnd);

        WIN_PumpEvents(_this);

        if (SetupWindowData(_this, window, hwnd, parent) < 0) {
            DestroyWindow(hwnd);
            if (parent) {
                DestroyWindow(parent);
            }
            return -1;
        }

        /* Inform Windows of the frame change so we can respond to WM_NCCALCSIZE */
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);

        if (window->flags & SDL_WINDOW_MINIMIZED) {
            /* TODO: We have to clear SDL_WINDOW_HIDDEN here to ensure the window flags match the window state. The
               window is already shown after this and windows with WS_MINIMIZE do not generate a WM_SHOWWINDOW. This
               means you can't currently create a window that is initially hidden and is minimized when shown.
            */
            window->flags &= ~SDL_WINDOW_HIDDEN;
            ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
        }
    }

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    /* FIXME: does not work on all hardware configurations with different renders (i.e. hybrid GPUs) */
    if (window->flags & SDL_WINDOW_TRANSPARENT) {
        void *handle = SDL_LoadObject("dwmapi.dll");
        if (handle) {
            DwmEnableBlurBehindWindow_t DwmEnableBlurBehindWindowFunc = (DwmEnableBlurBehindWindow_t)SDL_LoadFunction(handle, "DwmEnableBlurBehindWindow");
            if (DwmEnableBlurBehindWindowFunc) {
                /* The region indicates which part of the window will be blurred and rest will be transparent. This
                   is because the alpha value of the window will be used for non-blurred areas
                   We can use (-1, -1, 0, 0) boundary to make sure no pixels are being blurred
                */
                HRGN rgn = CreateRectRgn(-1, -1, 0, 0);
                DWM_BLURBEHIND bb;
                bb.flags = (DWM_BB_ENABLE | DWM_BB_BLURREGION);
                bb.enable = TRUE;
                bb.blur_region = rgn;
                bb.transition_on_maxed = FALSE;
                DwmEnableBlurBehindWindowFunc(hwnd, &bb);
                DeleteObject(rgn);
            }
            SDL_UnloadObject(handle);
        }
    }

    HWND share_hwnd = (HWND)SDL_GetProperty(create_props, SDL_PROP_WINDOW_CREATE_WIN32_PIXEL_FORMAT_HWND_POINTER, NULL);
    if (share_hwnd) {
        HDC hdc = GetDC(share_hwnd);
        int pixel_format = GetPixelFormat(hdc);
        PIXELFORMATDESCRIPTOR pfd;

        SDL_zero(pfd);
        DescribePixelFormat(hdc, pixel_format, sizeof(pfd), &pfd);
        ReleaseDC(share_hwnd, hdc);

        if (!SetPixelFormat(window->driverdata->hdc, pixel_format, &pfd)) {
            WIN_DestroyWindow(_this, window);
            return WIN_SetError("SetPixelFormat()");
        }
    } else {
        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            return 0;
        }

        /* The rest of this macro mess is for OpenGL or OpenGL ES windows */
#ifdef SDL_VIDEO_OPENGL_ES2
        if ((_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES ||
             SDL_GetHintBoolean(SDL_HINT_VIDEO_FORCE_EGL, SDL_FALSE)) &&
#ifdef SDL_VIDEO_OPENGL_WGL
            (!_this->gl_data || WIN_GL_UseEGL(_this))
#endif /* SDL_VIDEO_OPENGL_WGL */
        ) {
#ifdef SDL_VIDEO_OPENGL_EGL
            if (WIN_GLES_SetupWindow(_this, window) < 0) {
                WIN_DestroyWindow(_this, window);
                return -1;
            }
            return 0;
#else
            return SDL_SetError("Could not create GLES window surface (EGL support not configured)");
#endif /* SDL_VIDEO_OPENGL_EGL */
        }
#endif /* SDL_VIDEO_OPENGL_ES2 */

#ifdef SDL_VIDEO_OPENGL_WGL
        if (WIN_GL_SetupWindow(_this, window) < 0) {
            WIN_DestroyWindow(_this, window);
            return -1;
        }
#else
        return SDL_SetError("Could not create GL window (WGL support not configured)");
#endif
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

    return 0;
}

void WIN_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    HWND hwnd = window->driverdata->hwnd;
    LPTSTR title = WIN_UTF8ToString(window->title);
    SetWindowText(hwnd, title);
    SDL_free(title);
#endif
}

int WIN_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    HWND hwnd = window->driverdata->hwnd;
    HICON hicon = NULL;
    BYTE *icon_bmp;
    int icon_len, mask_len, row_len, y;
    BITMAPINFOHEADER *bmi;
    Uint8 *dst;
    SDL_bool isstack;
    int retval = 0;

    /* Create temporary buffer for ICONIMAGE structure */
    SDL_COMPILE_TIME_ASSERT(WIN_SetWindowIcon_uses_BITMAPINFOHEADER_to_prepare_an_ICONIMAGE, sizeof(BITMAPINFOHEADER) == 40);
    mask_len = (icon->h * (icon->w + 7) / 8);
    icon_len = sizeof(BITMAPINFOHEADER) + icon->h * icon->w * sizeof(Uint32) + mask_len;
    icon_bmp = SDL_small_alloc(BYTE, icon_len, &isstack);

    /* Write the BITMAPINFO header */
    bmi = (BITMAPINFOHEADER *)icon_bmp;
    bmi->biSize = SDL_Swap32LE(sizeof(BITMAPINFOHEADER));
    bmi->biWidth = SDL_Swap32LE(icon->w);
    bmi->biHeight = SDL_Swap32LE(icon->h * 2);
    bmi->biPlanes = SDL_Swap16LE(1);
    bmi->biBitCount = SDL_Swap16LE(32);
    bmi->biCompression = SDL_Swap32LE(BI_RGB);
    bmi->biSizeImage = SDL_Swap32LE(icon->h * icon->w * sizeof(Uint32));
    bmi->biXPelsPerMeter = SDL_Swap32LE(0);
    bmi->biYPelsPerMeter = SDL_Swap32LE(0);
    bmi->biClrUsed = SDL_Swap32LE(0);
    bmi->biClrImportant = SDL_Swap32LE(0);

    /* Write the pixels upside down into the bitmap buffer */
    SDL_assert(icon->format->format == SDL_PIXELFORMAT_ARGB8888);
    dst = &icon_bmp[sizeof(BITMAPINFOHEADER)];
    row_len = icon->w * sizeof(Uint32);
    y = icon->h;
    while (y--) {
        Uint8 *src = (Uint8 *)icon->pixels + y * icon->pitch;
        SDL_memcpy(dst, src, row_len);
        dst += row_len;
    }

    /* Write the mask */
    SDL_memset(icon_bmp + icon_len - mask_len, 0xFF, mask_len);

    hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);

    SDL_small_free(icon_bmp, isstack);

    if (!hicon) {
        retval = SDL_SetError("SetWindowIcon() failed, error %08X", (unsigned int)GetLastError());
    }

    /* Set the icon for the window */
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

    /* Set the icon in the task manager (should we do this?) */
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
    return retval;
#else
    return SDL_Unsupported();
#endif
}

int WIN_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* HighDPI support: removed SWP_NOSIZE. If the move results in a DPI change, we need to allow
     * the window to resize (e.g. AdjustWindowRectExForDpi frame sizes are different).
     */
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        if (!(window->flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
            WIN_ConstrainPopup(window);
            return WIN_SetWindowPositionInternal(window,
                                                 window->driverdata->copybits_flag | SWP_NOZORDER | SWP_NOOWNERZORDER |
                                                 SWP_NOACTIVATE, SDL_WINDOWRECT_FLOATING);
        } else {
            window->driverdata->floating_rect_pending = SDL_TRUE;
        }
    } else {
        return SDL_UpdateFullscreenMode(window, SDL_TRUE, SDL_TRUE);
    }

    return 0;
}

void WIN_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (!(window->flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
        WIN_SetWindowPositionInternal(window, window->driverdata->copybits_flag | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE, SDL_WINDOWRECT_FLOATING);
    } else {
        window->driverdata->floating_rect_pending = SDL_TRUE;
    }
}

int WIN_GetWindowBordersSize(SDL_VideoDevice *_this, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    HWND hwnd = window->driverdata->hwnd;
    RECT rcClient;

    /* rcClient stores the size of the inner window, while rcWindow stores the outer size relative to the top-left
     * screen position; so the top/left values of rcClient are always {0,0} and bottom/right are {height,width} */
    GetClientRect(hwnd, &rcClient);

    *top = rcClient.top;
    *left = rcClient.left;
    *bottom = rcClient.bottom;
    *right = rcClient.right;

    return 0;
#else  /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/
    HWND hwnd = window->driverdata->hwnd;
    RECT rcClient, rcWindow;
    POINT ptDiff;

    /* rcClient stores the size of the inner window, while rcWindow stores the outer size relative to the top-left
     * screen position; so the top/left values of rcClient are always {0,0} and bottom/right are {height,width} */
    if (!GetClientRect(hwnd, &rcClient)) {
        return SDL_SetError("GetClientRect() failed, error %08X", (unsigned int)GetLastError());
    }

    if (!GetWindowRect(hwnd, &rcWindow)) {
        return SDL_SetError("GetWindowRect() failed, error %08X", (unsigned int)GetLastError());
    }

    /* convert the top/left values to make them relative to
     * the window; they will end up being slightly negative */
    ptDiff.y = rcWindow.top;
    ptDiff.x = rcWindow.left;

    if (!ScreenToClient(hwnd, &ptDiff)) {
        return SDL_SetError("ScreenToClient() failed, error %08X", (unsigned int)GetLastError());
    }

    rcWindow.top = ptDiff.y;
    rcWindow.left = ptDiff.x;

    /* convert the bottom/right values to make them relative to the window,
     * these will be slightly bigger than the inner width/height */
    ptDiff.y = rcWindow.bottom;
    ptDiff.x = rcWindow.right;

    if (!ScreenToClient(hwnd, &ptDiff)) {
        return SDL_SetError("ScreenToClient() failed, error %08X", (unsigned int)GetLastError());
    }

    rcWindow.bottom = ptDiff.y;
    rcWindow.right = ptDiff.x;

    /* Now that both the inner and outer rects use the same coordinate system we can subtract them to get the border size.
     * Keep in mind that the top/left coordinates of rcWindow are negative because the border lies slightly before {0,0},
     * so switch them around because SDL3 wants them in positive. */
    *top = rcClient.top - rcWindow.top;
    *left = rcClient.left - rcWindow.left;
    *bottom = rcWindow.bottom - rcClient.bottom;
    *right = rcWindow.right - rcClient.right;

    return 0;
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/
}

void WIN_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h)
{
    const SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    RECT rect;

    if (GetClientRect(hwnd, &rect) && !WIN_IsRectEmpty(&rect)) {
        *w = rect.right;
        *h = rect.bottom;
    } else if (window->last_pixel_w && window->last_pixel_h) {
        *w = window->last_pixel_w;
        *h = window->last_pixel_h;
    } else {
        /* Probably created minimized, use the restored size */
        *w = window->floating.w;
        *h = window->floating.h;
    }
}

void WIN_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    DWORD style;
    HWND hwnd;

    SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, SDL_TRUE);

    if (window->parent) {
        /* Update our position in case our parent moved while we were hidden */
        WIN_SetWindowPosition(_this, window);
    }

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    if (window->flags & SDL_WINDOW_MODAL) {
        EnableWindow(window->parent->driverdata->hwnd, FALSE);
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

    hwnd = window->driverdata->hwnd;
    style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style & WS_EX_NOACTIVATE) {
        bActivate = SDL_FALSE;
    }
    if (bActivate) {
        ShowWindow(hwnd, SW_SHOW);
    } else {
        /* Use SetWindowPos instead of ShowWindow to avoid activating the parent window if this is a child window */
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, window->driverdata->copybits_flag | SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }

    if (window->flags & SDL_WINDOW_POPUP_MENU && bActivate) {
        if (window->parent == SDL_GetKeyboardFocus()) {
            WIN_SetKeyboardFocus(window);
        }
    }
}

void WIN_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    HWND hwnd = window->driverdata->hwnd;

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    if (window->flags & SDL_WINDOW_MODAL) {
        EnableWindow(window->parent->driverdata->hwnd, TRUE);
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

    ShowWindow(hwnd, SW_HIDE);

    /* Transfer keyboard focus back to the parent */
    if (window->flags & SDL_WINDOW_POPUP_MENU) {
        if (window == SDL_GetKeyboardFocus()) {
            SDL_Window *new_focus = window->parent;

            /* Find the highest level window that isn't being hidden or destroyed. */
            while (new_focus->parent && (new_focus->is_hiding || new_focus->is_destroying)) {
                new_focus = new_focus->parent;
            }

            WIN_SetKeyboardFocus(new_focus);
        }
    }
}

void WIN_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    /* If desired, raise the window more forcefully.
     * Technique taken from http://stackoverflow.com/questions/916259/ .
     * Specifically, http://stackoverflow.com/a/34414846 .
     *
     * The issue is that Microsoft has gone through a lot of trouble to make it
     * nearly impossible to programmatically move a window to the foreground,
     * for "security" reasons. Apparently, the following song-and-dance gets
     * around their objections. */
    SDL_bool bForce = SDL_GetHintBoolean(SDL_HINT_FORCE_RAISEWINDOW, SDL_FALSE);
    SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, SDL_TRUE);

    HWND hCurWnd = NULL;
    DWORD dwMyID = 0u;
    DWORD dwCurID = 0u;

    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    if (bForce) {
        hCurWnd = GetForegroundWindow();
        dwMyID = GetCurrentThreadId();
        dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
        ShowWindow(hwnd, SW_RESTORE);
        AttachThreadInput(dwCurID, dwMyID, TRUE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        if (!SDL_ShouldAllowTopmost() || !(window->flags & SDL_WINDOW_ALWAYS_ON_TOP)) {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        }
    }
    if (bActivate) {
        SetForegroundWindow(hwnd);
        if (window->flags & SDL_WINDOW_POPUP_MENU) {
            if (window->parent == SDL_GetKeyboardFocus()) {
                WIN_SetKeyboardFocus(window);
            }
        }
    } else {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, data->copybits_flag | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }
    if (bForce) {
        AttachThreadInput(dwCurID, dwMyID, FALSE);
        SetFocus(hwnd);
        SetActiveWindow(hwnd);
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/
}

void WIN_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        HWND hwnd = data->hwnd;
        data->expected_resize = SDL_TRUE;
        ShowWindow(hwnd, SW_MAXIMIZE);
        data->expected_resize = SDL_FALSE;

        /* Clamp the maximized window size to the max window size.
         * This is automatic if maximizing from the window controls.
         */
        if (window->max_w || window->max_h) {
            int fx, fy, fw, fh;

            window->windowed.w = window->max_w ? SDL_min(window->w, window->max_w) : window->windowed.w;
            window->windowed.h = window->max_h ? SDL_min(window->h, window->max_h) : window->windowed.h;
            WIN_AdjustWindowRect(window, &fx, &fy, &fw, &fh, SDL_WINDOWRECT_WINDOWED);

            data->expected_resize = SDL_TRUE;
            SetWindowPos(hwnd, HWND_TOP, fx, fy, fw, fh, data->copybits_flag | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
            data->expected_resize = SDL_FALSE;
        }
    }else {
        data->windowed_mode_was_maximized = SDL_TRUE;
    }
}

void WIN_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    HWND hwnd = window->driverdata->hwnd;
    ShowWindow(hwnd, SW_MINIMIZE);
}

void WIN_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered)
{
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    DWORD style;

    style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~STYLE_MASK;
    style |= GetWindowStyle(window);

    data->in_border_change = SDL_TRUE;
    SetWindowLong(hwnd, GWL_STYLE, style);
    WIN_SetWindowPositionInternal(window, data->copybits_flag | SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE, SDL_WINDOWRECT_CURRENT);
    data->in_border_change = SDL_FALSE;
}

void WIN_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable)
{
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    DWORD style;

    style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~STYLE_MASK;
    style |= GetWindowStyle(window);

    SetWindowLong(hwnd, GWL_STYLE, style);
}

void WIN_SetWindowAlwaysOnTop(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool on_top)
{
    WIN_SetWindowPositionInternal(window, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE, SDL_WINDOWRECT_CURRENT);
}

void WIN_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        HWND hwnd = data->hwnd;
        data->expected_resize = SDL_TRUE;
        ShowWindow(hwnd, SW_RESTORE);
        data->expected_resize = SDL_FALSE;
    } else {
        data->windowed_mode_was_maximized = SDL_FALSE;
    }
}

static DWM_WINDOW_CORNER_PREFERENCE WIN_UpdateCornerRoundingForHWND(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE cornerPref)
{
    DWM_WINDOW_CORNER_PREFERENCE oldPref = DWMWCP_DEFAULT;

    void *handle = SDL_LoadObject("dwmapi.dll");
    if (handle) {
        DwmGetWindowAttribute_t DwmGetWindowAttributeFunc = (DwmGetWindowAttribute_t)SDL_LoadFunction(handle, "DwmGetWindowAttribute");
        DwmSetWindowAttribute_t DwmSetWindowAttributeFunc = (DwmSetWindowAttribute_t)SDL_LoadFunction(handle, "DwmSetWindowAttribute");
        if (DwmGetWindowAttributeFunc && DwmSetWindowAttributeFunc) {
            DwmGetWindowAttributeFunc(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &oldPref, sizeof(oldPref));
            DwmSetWindowAttributeFunc(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));            
        }
		
        SDL_UnloadObject(handle);
    }

    return oldPref;
}

static COLORREF WIN_UpdateBorderColorForHWND(HWND hwnd, COLORREF colorRef)
{
    COLORREF oldPref = DWMWA_COLOR_DEFAULT;

    void *handle = SDL_LoadObject("dwmapi.dll");
    if (handle) {
        DwmGetWindowAttribute_t DwmGetWindowAttributeFunc = (DwmGetWindowAttribute_t)SDL_LoadFunction(handle, "DwmGetWindowAttribute");
        DwmSetWindowAttribute_t DwmSetWindowAttributeFunc = (DwmSetWindowAttribute_t)SDL_LoadFunction(handle, "DwmSetWindowAttribute");
        if (DwmGetWindowAttributeFunc && DwmSetWindowAttributeFunc) {
            DwmGetWindowAttributeFunc(hwnd, DWMWA_BORDER_COLOR, &oldPref, sizeof(oldPref));
            DwmSetWindowAttributeFunc(hwnd, DWMWA_BORDER_COLOR, &colorRef, sizeof(colorRef));
        }
		
        SDL_UnloadObject(handle);
    }

    return oldPref;
}

/**
 * Reconfigures the window to fill the given display, if fullscreen is true, otherwise restores the window.
 */
int WIN_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    SDL_DisplayData *displaydata = display->driverdata;
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    MONITORINFO minfo;
    DWORD style, styleEx;
    HWND top;
    int x, y;
    int w, h;
    SDL_bool enterMaximized = SDL_FALSE;

#ifdef HIGHDPI_DEBUG
    SDL_Log("WIN_SetWindowFullscreen: %d", (int)fullscreen);
#endif

    /* Early out if already not in fullscreen, or the styling on
     * external windows may end up being overridden.
     */
    if (!(window->flags & SDL_WINDOW_FULLSCREEN) && !fullscreen) {
        return 0;
    }

    if (SDL_ShouldAllowTopmost() && (window->flags & SDL_WINDOW_ALWAYS_ON_TOP)) {
        top = HWND_TOPMOST;
    } else {
        top = HWND_NOTOPMOST;
    }

    /* Use GetMonitorInfo instead of WIN_GetDisplayBounds because we want the
       monitor bounds in Windows coordinates (pixels) rather than SDL coordinates (points). */
    SDL_zero(minfo);
    minfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(displaydata->MonitorHandle, &minfo)) {
        SDL_SetError("GetMonitorInfo failed");
        return -1;
    }

    SDL_SendWindowEvent(window, fullscreen ? SDL_EVENT_WINDOW_ENTER_FULLSCREEN : SDL_EVENT_WINDOW_LEAVE_FULLSCREEN, 0, 0);
    style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~STYLE_MASK;
    style |= GetWindowStyle(window);
    styleEx = GetWindowLong(hwnd, GWL_EXSTYLE);

    if (fullscreen) {
        x = minfo.rcMonitor.left;
        y = minfo.rcMonitor.top;
        w = minfo.rcMonitor.right - minfo.rcMonitor.left;
        h = minfo.rcMonitor.bottom - minfo.rcMonitor.top;

        /* Unset the maximized flag.  This fixes
           https://bugzilla.libsdl.org/show_bug.cgi?id=3215
        */
        if (style & WS_MAXIMIZE) {
            data->windowed_mode_was_maximized = SDL_TRUE;
            style &= ~WS_MAXIMIZE;
        }

        /* Disable corner rounding & border color (Windows 11+) so the window fills the full screen */
        data->windowed_mode_corner_rounding = WIN_UpdateCornerRoundingForHWND(hwnd, DWMWCP_DONOTROUND);
        data->dwma_border_color = WIN_UpdateBorderColorForHWND(hwnd, DWMWA_COLOR_NONE);
    } else {
        BOOL menu;

        WIN_UpdateCornerRoundingForHWND(hwnd, data->windowed_mode_corner_rounding);
        WIN_UpdateBorderColorForHWND(hwnd, data->dwma_border_color);

        /* Restore window-maximization state, as applicable.
           Special care is taken to *not* do this if and when we're
           alt-tab'ing away (to some other window; as indicated by
           in_window_deactivation), otherwise
           https://bugzilla.libsdl.org/show_bug.cgi?id=3215 can reproduce!
        */
        if (data->windowed_mode_was_maximized && !data->in_window_deactivation) {
            style |= WS_MAXIMIZE;
            enterMaximized = SDL_TRUE;
        }

        menu = (style & WS_CHILDWINDOW) ? FALSE : (GetMenu(hwnd) != NULL);
        WIN_AdjustWindowRectWithStyle(window, style, styleEx, menu,
                                      &x, &y,
                                      &w, &h,
                                      data->windowed_mode_was_maximized ? SDL_WINDOWRECT_WINDOWED : SDL_WINDOWRECT_FLOATING);
        data->windowed_mode_was_maximized = SDL_FALSE;
    }
    SetWindowLong(hwnd, GWL_STYLE, style);
    data->expected_resize = SDL_TRUE;

    if (!enterMaximized) {
        SetWindowPos(hwnd, top, x, y, w, h, data->copybits_flag | SWP_NOACTIVATE);
    } else {
        WIN_MaximizeWindow(_this, window);
    }

    data->expected_resize = SDL_FALSE;

#ifdef HIGHDPI_DEBUG
    SDL_Log("WIN_SetWindowFullscreen: %d finished. Set window to %d,%d, %dx%d", (int)fullscreen, x, y, w, h);
#endif

#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/
    return 0;
}

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
void WIN_UpdateWindowICCProfile(SDL_Window *window, SDL_bool send_event)
{
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);

    if (displaydata) {
        HDC hdc = CreateDCW(displaydata->DeviceName, NULL, NULL, NULL);
        if (hdc) {
            WCHAR fileName[MAX_PATH];
            DWORD fileNameSize = SDL_arraysize(fileName);
            if (GetICMProfileW(hdc, &fileNameSize, fileName)) {
                /* fileNameSize includes '\0' on return */
                if (!data->ICMFileName ||
                    SDL_wcscmp(data->ICMFileName, fileName) != 0) {
                    if (data->ICMFileName) {
                        SDL_free(data->ICMFileName);
                    }
                    data->ICMFileName = SDL_wcsdup(fileName);
                    if (send_event) {
                        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_ICCPROF_CHANGED, 0, 0);
                    }
                }
            }
            DeleteDC(hdc);
        }
    }
}

void *WIN_GetWindowICCProfile(SDL_VideoDevice *_this, SDL_Window *window, size_t *size)
{
    SDL_WindowData *data = window->driverdata;
    char *filename_utf8;
    void *iccProfileData = NULL;

    filename_utf8 = WIN_StringToUTF8(data->ICMFileName);
    if (filename_utf8) {
        iccProfileData = SDL_LoadFile(filename_utf8, size);
        if (!iccProfileData) {
            SDL_SetError("Could not open ICC profile");
        }
        SDL_free(filename_utf8);
    }
    return iccProfileData;
}

static void WIN_GrabKeyboard(SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    HMODULE module;

    if (data->keyboard_hook) {
        return;
    }

    /* SetWindowsHookEx() needs to know which module contains the hook we
       want to install. This is complicated by the fact that SDL can be
       linked statically or dynamically. Fortunately XP and later provide
       this nice API that will go through the loaded modules and find the
       one containing our code.
    */
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPTSTR)WIN_KeyboardHookProc,
                           &module)) {
        return;
    }

    /* Capture a snapshot of the current keyboard state before the hook */
    if (!GetKeyboardState(data->videodata->pre_hook_key_state)) {
        return;
    }

    /* To grab the keyboard, we have to install a low-level keyboard hook to
       intercept keys that would normally be captured by the OS. Intercepting
       all key events on the system is rather invasive, but it's what Microsoft
       actually documents that you do to capture these.
    */
    data->keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, WIN_KeyboardHookProc, module, 0);
}

void WIN_UngrabKeyboard(SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;

    if (data->keyboard_hook) {
        UnhookWindowsHookEx(data->keyboard_hook);
        data->keyboard_hook = NULL;
    }
}

int WIN_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window)
{
    WIN_UpdateClipCursor(window);
    return 0;
}

int WIN_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    WIN_UpdateClipCursor(window);
    return 0;
}

int WIN_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    if (grabbed) {
        WIN_GrabKeyboard(window);
    } else {
        WIN_UngrabKeyboard(window);
    }

    return 0;
}
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

void WIN_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    CleanupWindowData(_this, window);
}

/*
 * Creates a HelperWindow used for DirectInput.
 */
int SDL_HelperWindowCreate(void)
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
    wce.lpszClassName = SDL_HelperWindowClassName;
    wce.hInstance = hInstance;

    /* Register the class. */
    SDL_HelperWindowClass = RegisterClass(&wce);
    if (SDL_HelperWindowClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return WIN_SetError("Unable to create Helper Window Class");
    }

    /* Create the window. */
    SDL_HelperWindow = CreateWindowEx(0, SDL_HelperWindowClassName,
                                      SDL_HelperWindowName,
                                      WS_OVERLAPPED, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, HWND_MESSAGE, NULL,
                                      hInstance, NULL);
    if (!SDL_HelperWindow) {
        UnregisterClass(SDL_HelperWindowClassName, hInstance);
        return WIN_SetError("Unable to create Helper Window");
    }

    return 0;
}

/*
 * Destroys the HelperWindow previously created with SDL_HelperWindowCreate.
 */
void SDL_HelperWindowDestroy(void)
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

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
void WIN_OnWindowEnter(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;

    if (!data || !data->hwnd) {
        /* The window wasn't fully initialized */
        return;
    }

    if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
        WIN_SetWindowPositionInternal(window, data->copybits_flag | SWP_NOSIZE | SWP_NOACTIVATE, SDL_WINDOWRECT_CURRENT);
    }
}

static BOOL GetClientScreenRect(HWND hwnd, RECT *rect)
{
    return GetClientRect(hwnd, rect) &&             /* RECT( left , top , right , bottom )   */
           ClientToScreen(hwnd, (LPPOINT)rect) &&   /* POINT( left , top )                    */
           ClientToScreen(hwnd, (LPPOINT)rect + 1); /*             POINT( right , bottom )   */
}

void WIN_UpdateClipCursor(SDL_Window *window)
{
    SDL_VideoDevice *videodevice = SDL_GetVideoDevice();
    SDL_WindowData *data = window->driverdata;
    SDL_Mouse *mouse = SDL_GetMouse();
    RECT rect, clipped_rect;

    if (data->in_title_click || data->focus_click_pending) {
        return;
    }
    if (data->skip_update_clipcursor) {
        return;
    }
    if (!GetClipCursor(&clipped_rect)) {
        return;
    }

    if ((mouse->relative_mode || (window->flags & SDL_WINDOW_MOUSE_GRABBED) ||
         (window->mouse_rect.w > 0 && window->mouse_rect.h > 0)) &&
        (window->flags & SDL_WINDOW_INPUT_FOCUS)) {
        if (mouse->relative_mode && !mouse->relative_mode_warp && data->mouse_relative_mode_center) {
            if (GetClientScreenRect(data->hwnd, &rect)) {
                /* WIN_WarpCursor() jitters by +1, and remote desktop warp wobble is +/- 1 */
                LONG remote_desktop_adjustment = GetSystemMetrics(SM_REMOTESESSION) ? 2 : 0;
                LONG cx, cy;

                cx = (rect.left + rect.right) / 2;
                cy = (rect.top + rect.bottom) / 2;

                /* Make an absurdly small clip rect */
                rect.left = cx - remote_desktop_adjustment;
                rect.right = cx + 1 + remote_desktop_adjustment;
                rect.top = cy;
                rect.bottom = cy + 1;

                if (SDL_memcmp(&rect, &clipped_rect, sizeof(rect)) != 0) {
                    if (ClipCursor(&rect)) {
                        data->cursor_clipped_rect = rect;
                    }
                }
            }
        } else {
            if (GetClientScreenRect(data->hwnd, &rect)) {
                if (window->mouse_rect.w > 0 && window->mouse_rect.h > 0) {
                    SDL_Rect mouse_rect_win_client;
                    RECT mouse_rect, intersection;

                    /* mouse_rect_win_client is the mouse rect in Windows client space */
                    mouse_rect_win_client = window->mouse_rect;

                    /* mouse_rect is the rect in Windows screen space */
                    mouse_rect.left = rect.left + mouse_rect_win_client.x;
                    mouse_rect.top = rect.top + mouse_rect_win_client.y;
                    mouse_rect.right = mouse_rect.left + mouse_rect_win_client.w;
                    mouse_rect.bottom = mouse_rect.top + mouse_rect_win_client.h;
                    if (IntersectRect(&intersection, &rect, &mouse_rect)) {
                        SDL_memcpy(&rect, &intersection, sizeof(rect));
                    } else if (window->flags & SDL_WINDOW_MOUSE_GRABBED) {
                        /* Mouse rect was invalid, just do the normal grab */
                    } else {
                        SDL_zero(rect);
                    }
                }
                if (SDL_memcmp(&rect, &clipped_rect, sizeof(rect)) != 0) {
                    if (!WIN_IsRectEmpty(&rect)) {
                        if (ClipCursor(&rect)) {
                            data->cursor_clipped_rect = rect;
                        }
                    } else {
                        ClipCursor(NULL);
                        SDL_zero(data->cursor_clipped_rect);
                    }
                }
            }
        }
    } else {
        SDL_bool unclip_cursor = SDL_FALSE;

        /* If the cursor is clipped to the screen, clear the clip state */
        if (!videodevice ||
            (clipped_rect.left == videodevice->desktop_bounds.x &&
             clipped_rect.top == videodevice->desktop_bounds.y)) {
            unclip_cursor = SDL_TRUE;
        } else {
            POINT first, second;

            first.x = clipped_rect.left;
            first.y = clipped_rect.top;
            second.x = clipped_rect.right - 1;
            second.y = clipped_rect.bottom - 1;
            if (PtInRect(&data->cursor_clipped_rect, first) &&
                PtInRect(&data->cursor_clipped_rect, second)) {
                unclip_cursor = SDL_TRUE;
            }
        }
        if (unclip_cursor) {
            ClipCursor(NULL);
            SDL_zero(data->cursor_clipped_rect);
        }
    }
    data->last_updated_clipcursor = SDL_GetTicks();
}

int WIN_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0; /* just succeed, the real work is done elsewhere. */
}
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

int WIN_SetWindowOpacity(SDL_VideoDevice *_this, SDL_Window *window, float opacity)
{
#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
    return -1;
#else
    const SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    const LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);

    SDL_assert(style != 0);

    if (opacity == 1.0f) {
        /* want it fully opaque, just mark it unlayered if necessary. */
        if (style & WS_EX_LAYERED) {
            if (SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_LAYERED) == 0) {
                return WIN_SetError("SetWindowLong()");
            }
        }
    } else {
        const BYTE alpha = (BYTE)((int)(opacity * 255.0f));
        /* want it transparent, mark it layered if necessary. */
        if (!(style & WS_EX_LAYERED)) {
            if (SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED) == 0) {
                return WIN_SetError("SetWindowLong()");
            }
        }

        if (SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA) == 0) {
            return WIN_SetError("SetLayeredWindowAttributes()");
        }
    }

    return 0;
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/
}

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
void WIN_AcceptDragAndDrop(SDL_Window *window, SDL_bool accept)
{
    const SDL_WindowData *data = window->driverdata;
    DragAcceptFiles(data->hwnd, accept ? TRUE : FALSE);
}

int WIN_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation)
{
    FLASHWINFO desc;

    SDL_zero(desc);
    desc.cbSize = sizeof(desc);
    desc.hwnd = window->driverdata->hwnd;
    switch (operation) {
    case SDL_FLASH_CANCEL:
        desc.dwFlags = FLASHW_STOP;
        break;
    case SDL_FLASH_BRIEFLY:
        desc.dwFlags = FLASHW_TRAY;
        desc.uCount = 1;
        break;
    case SDL_FLASH_UNTIL_FOCUSED:
        desc.dwFlags = (FLASHW_TRAY | FLASHW_TIMERNOFG);
        break;
    default:
        return SDL_Unsupported();
    }

    FlashWindowEx(&desc);

    return 0;
}

void WIN_ShowWindowSystemMenu(SDL_Window *window, int x, int y)
{
    const SDL_WindowData *data = window->driverdata;
    POINT pt;

    pt.x = x;
    pt.y = y;
    ClientToScreen(data->hwnd, &pt);
    SendMessage(data->hwnd, WM_POPUPSYSTEMMENU, 0, MAKELPARAM(pt.x, pt.y));
}

int WIN_SetWindowFocusable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool focusable)
{
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    const LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);

    SDL_assert(style != 0);

    if (focusable) {
        if (style & WS_EX_NOACTIVATE) {
            if (SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_NOACTIVATE) == 0) {
                return WIN_SetError("SetWindowLong()");
            }
        }
    } else {
        if (!(style & WS_EX_NOACTIVATE)) {
            if (SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_NOACTIVATE) == 0) {
                return WIN_SetError("SetWindowLong()");
            }
        }
    }

    return 0;
}
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

void WIN_UpdateDarkModeForHWND(HWND hwnd)
{
    void *handle = SDL_LoadObject("dwmapi.dll");
    if (handle) {
        DwmSetWindowAttribute_t DwmSetWindowAttributeFunc = (DwmSetWindowAttribute_t)SDL_LoadFunction(handle, "DwmSetWindowAttribute");
        if (DwmSetWindowAttributeFunc) {
            /* FIXME: Do we need to traverse children? */
            BOOL value = (SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK) ? TRUE : FALSE;
            DwmSetWindowAttributeFunc(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        }
        SDL_UnloadObject(handle);
    }
}

int WIN_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    SDL_WindowData *modal_data = modal_window->driverdata;
    const LONG_PTR parent_hwnd = (LONG_PTR)(parent_window ? parent_window->driverdata->hwnd : NULL);
    const LONG_PTR old_ptr = GetWindowLongPtr(modal_data->hwnd, GWLP_HWNDPARENT);
    const DWORD style = GetWindowLong(modal_data->hwnd, GWL_STYLE);

    if (old_ptr == parent_hwnd) {
        return 0;
    }

    /* Reenable the old parent window. */
    if (old_ptr) {
        EnableWindow((HWND)old_ptr, TRUE);
    }

    if (!(style & WS_CHILD)) {
        /* Despite the name, this changes the *owner* of a toplevel window, not
         * the parent of a child window.
         *
         * https://devblogs.microsoft.com/oldnewthing/20100315-00/?p=14613
         */
        SetWindowLongPtr(modal_data->hwnd, GWLP_HWNDPARENT, parent_hwnd);
    } else {
        SetParent(modal_data->hwnd, (HWND)parent_hwnd);
    }

    /* Disable the new parent window if the modal window is visible. */
    if (!(modal_window->flags & SDL_WINDOW_HIDDEN) && parent_hwnd) {
        EnableWindow((HWND)parent_hwnd, FALSE);
    }
#endif /*!defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)*/

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
