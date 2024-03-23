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

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)

#include "SDL_windowsvideo.h"
#include "SDL_windowsevents.h"

#include "../SDL_video_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../joystick/usb_ids.h"

DWORD SDL_last_warp_time = 0;
HCURSOR SDL_cursor = NULL;
static SDL_Cursor *SDL_blank_cursor = NULL;

static int rawInputEnableCount = 0;

typedef struct
{
    HANDLE ready_event;
    HANDLE done_event;
    HANDLE thread;
} RawMouseThreadData;

static RawMouseThreadData thread_data = {
    INVALID_HANDLE_VALUE,
    INVALID_HANDLE_VALUE,
    INVALID_HANDLE_VALUE
};

static DWORD WINAPI WIN_RawMouseThread(LPVOID param)
{
    RAWINPUTDEVICE rawMouse;
    HWND window;

    window = CreateWindowEx(0, TEXT("Message"), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!window) {
        return 0;
    }

    rawMouse.usUsagePage = USB_USAGEPAGE_GENERIC_DESKTOP;
    rawMouse.usUsage = USB_USAGE_GENERIC_MOUSE;
    rawMouse.dwFlags = 0;
    rawMouse.hwndTarget = window;

    if (!RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse))) {
        DestroyWindow(window);
        return 0;
    }

    /* Make sure we get mouse events as soon as possible */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    /* Tell the parent we're ready to go! */
    SetEvent(thread_data.ready_event);

    for ( ; ; ) {
        if (MsgWaitForMultipleObjects(1, &thread_data.done_event, 0, INFINITE, QS_RAWINPUT) != WAIT_OBJECT_0 + 1) {
            break;
        }

        /* Clear the queue status so MsgWaitForMultipleObjects() will wait again */
        (void)GetQueueStatus(QS_RAWINPUT);

        WIN_PollRawMouseInput();
    }

    rawMouse.dwFlags |= RIDEV_REMOVE;
    RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse));

    DestroyWindow(window);

    return 0;
}

static void CleanupRawMouseThreadData(void)
{
    if (thread_data.thread != INVALID_HANDLE_VALUE) {
        SetEvent(thread_data.done_event);
        WaitForSingleObject(thread_data.thread, 500);
        CloseHandle(thread_data.thread);
        thread_data.thread = INVALID_HANDLE_VALUE;
    }

    if (thread_data.ready_event != INVALID_HANDLE_VALUE) {
        CloseHandle(thread_data.ready_event);
        thread_data.ready_event = INVALID_HANDLE_VALUE;
    }

    if (thread_data.done_event != INVALID_HANDLE_VALUE) {
        CloseHandle(thread_data.done_event);
        thread_data.done_event = INVALID_HANDLE_VALUE;
    }
}

static int ToggleRawInput(SDL_bool enabled)
{
    int result = -1;

    if (enabled) {
        rawInputEnableCount++;
        if (rawInputEnableCount > 1) {
            return 0; /* already done. */
        }
    } else {
        if (rawInputEnableCount == 0) {
            return 0; /* already done. */
        }
        rawInputEnableCount--;
        if (rawInputEnableCount > 0) {
            return 0; /* not time to disable yet */
        }
    }

    if (enabled) {
        HANDLE handles[2];

        thread_data.ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (thread_data.ready_event == INVALID_HANDLE_VALUE) {
            WIN_SetError("CreateEvent");
            goto done;
        }

        thread_data.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (thread_data.done_event == INVALID_HANDLE_VALUE) {
            WIN_SetError("CreateEvent");
            goto done;
        }

        thread_data.thread = CreateThread(NULL, 0, WIN_RawMouseThread, &thread_data, 0, NULL);
        if (thread_data.thread == INVALID_HANDLE_VALUE) {
            WIN_SetError("CreateThread");
            goto done;
        }

        /* Wait for the thread to signal ready or exit */
        handles[0] = thread_data.ready_event;
        handles[1] = thread_data.thread;
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0) {
            SDL_SetError("Couldn't set up raw input handling");
            goto done;
        }
        result = 0;
    } else {
        CleanupRawMouseThreadData();
        result = 0;
    }

done:
    if (enabled && result < 0) {
        CleanupRawMouseThreadData();

        /* Reset rawInputEnableCount so we can try again */
        rawInputEnableCount = 0;
    }
    return result;
}

static SDL_Cursor *WIN_CreateDefaultCursor()
{
    SDL_Cursor *cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        cursor->driverdata = LoadCursor(NULL, IDC_ARROW);
    }

    return cursor;
}

static SDL_bool IsMonochromeSurface(SDL_Surface *surface)
{
    int x, y;
    Uint8 r, g, b, a;

    SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);

    for (y = 0; y < surface->h; y++) {
        for (x = 0; x < surface->w; x++) {
            SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);

            /* Black or white pixel. */
            if (!((r == 0x00 && g == 0x00 && b == 0x00) || (r == 0xff && g == 0xff && b == 0xff))) {
                return SDL_FALSE;
            }

            /* Transparent or opaque pixel. */
            if (!(a == 0x00 || a == 0xff)) {
                return SDL_FALSE;
            }
        }
    }

    return SDL_TRUE;
}

static HBITMAP CreateColorBitmap(SDL_Surface *surface)
{
    HBITMAP bitmap;
    BITMAPINFO bi;
    void *pixels;

    SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);

    SDL_zero(bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = surface->w;
    bi.bmiHeader.biHeight = -surface->h; /* Invert height to make the top-down DIB. */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!bitmap || !pixels) {
        WIN_SetError("CreateDIBSection()");
        return NULL;
    }

    SDL_memcpy(pixels, surface->pixels, surface->pitch * surface->h);

    return bitmap;
}

/* Generate bitmap with a mask and optional monochrome image data.
 *
 * For info on the expected mask format see:
 * https://devblogs.microsoft.com/oldnewthing/20101018-00/?p=12513
 */
static HBITMAP CreateMaskBitmap(SDL_Surface *surface, SDL_bool is_monochrome)
{
    HBITMAP bitmap;
    SDL_bool isstack;
    void *pixels;
    int x, y;
    Uint8 r, g, b, a;
    Uint8 *dst;
    const int pitch = ((surface->w + 15) & ~15) / 8;
    const int size = pitch * surface->h;
    static const unsigned char masks[] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };

    SDL_assert(surface->format->format == SDL_PIXELFORMAT_ARGB8888);

    pixels = SDL_small_alloc(Uint8, size * (is_monochrome ? 2 : 1), &isstack);
    if (!pixels) {
        return NULL;
    }

    dst = (Uint8 *)pixels;

    /* Make the mask completely transparent. */
    SDL_memset(dst, 0xff, size);
    if (is_monochrome) {
        SDL_memset(dst + size, 0x00, size);
    }

    for (y = 0; y < surface->h; y++, dst += pitch) {
        for (x = 0; x < surface->w; x++) {
            SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);

            if (a != 0) {
                /* Reset bit of an opaque pixel. */
                dst[x >> 3] &= ~masks[x & 7];
            }

            if (is_monochrome && !(r == 0x00 && g == 0x00 && b == 0x00)) {
                /* Set bit of white or inverted pixel. */
                dst[size + (x >> 3)] |= masks[x & 7];
            }
        }
    }

    bitmap = CreateBitmap(surface->w, surface->h * (is_monochrome ? 2 : 1), 1, 1, pixels);
    SDL_small_free(pixels, isstack);
    if (!bitmap) {
        WIN_SetError("CreateBitmap()");
        return NULL;
    }

    return bitmap;
}

static SDL_Cursor *WIN_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    HCURSOR hcursor;
    SDL_Cursor *cursor;
    ICONINFO ii;
    SDL_bool is_monochrome = IsMonochromeSurface(surface);

    SDL_zero(ii);
    ii.fIcon = FALSE;
    ii.xHotspot = (DWORD)hot_x;
    ii.yHotspot = (DWORD)hot_y;
    ii.hbmMask = CreateMaskBitmap(surface, is_monochrome);
    ii.hbmColor = is_monochrome ? NULL : CreateColorBitmap(surface);

    if (!ii.hbmMask || (!is_monochrome && !ii.hbmColor)) {
        return NULL;
    }

    hcursor = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmMask);
    if (ii.hbmColor) {
        DeleteObject(ii.hbmColor);
    }

    if (!hcursor) {
        WIN_SetError("CreateIconIndirect()");
        return NULL;
    }

    cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        cursor->driverdata = hcursor;
    } else {
        DestroyCursor(hcursor);
    }

    return cursor;
}

static SDL_Cursor *WIN_CreateBlankCursor()
{
    SDL_Cursor *cursor = NULL;
    SDL_Surface *surface = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        cursor = WIN_CreateCursor(surface, 0, 0);
        SDL_DestroySurface(surface);
    }
    return cursor;
}

static SDL_Cursor *WIN_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor;
    LPCTSTR name;

    switch (id) {
    default:
        SDL_assert(0);
        return NULL;
    case SDL_SYSTEM_CURSOR_ARROW:
        name = IDC_ARROW;
        break;
    case SDL_SYSTEM_CURSOR_IBEAM:
        name = IDC_IBEAM;
        break;
    case SDL_SYSTEM_CURSOR_WAIT:
        name = IDC_WAIT;
        break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        name = IDC_CROSS;
        break;
    case SDL_SYSTEM_CURSOR_WAITARROW:
        name = IDC_WAIT;
        break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:
        name = IDC_SIZENWSE;
        break;
    case SDL_SYSTEM_CURSOR_SIZENESW:
        name = IDC_SIZENESW;
        break;
    case SDL_SYSTEM_CURSOR_SIZEWE:
        name = IDC_SIZEWE;
        break;
    case SDL_SYSTEM_CURSOR_SIZENS:
        name = IDC_SIZENS;
        break;
    case SDL_SYSTEM_CURSOR_SIZEALL:
        name = IDC_SIZEALL;
        break;
    case SDL_SYSTEM_CURSOR_NO:
        name = IDC_NO;
        break;
    case SDL_SYSTEM_CURSOR_HAND:
        name = IDC_HAND;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT:
        name = IDC_SIZENWSE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOP:
        name = IDC_SIZENS;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT:
        name = IDC_SIZENESW;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_RIGHT:
        name = IDC_SIZEWE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT:
        name = IDC_SIZENWSE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOM:
        name = IDC_SIZENS;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT:
        name = IDC_SIZENESW;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_LEFT:
        name = IDC_SIZEWE;
        break;
    }

    cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        HCURSOR hcursor;

        hcursor = LoadCursor(NULL, name);

        cursor->driverdata = hcursor;
    }

    return cursor;
}

static void WIN_FreeCursor(SDL_Cursor *cursor)
{
    HCURSOR hcursor = (HCURSOR)cursor->driverdata;

    DestroyCursor(hcursor);
    SDL_free(cursor);
}

static int WIN_ShowCursor(SDL_Cursor *cursor)
{
    if (!cursor) {
        cursor = SDL_blank_cursor;
    }
    if (cursor) {
        SDL_cursor = (HCURSOR)cursor->driverdata;
    } else {
        SDL_cursor = NULL;
    }
    if (SDL_GetMouseFocus() != NULL) {
        SetCursor(SDL_cursor);
    }
    return 0;
}

void WIN_SetCursorPos(int x, int y)
{
    /* We need to jitter the value because otherwise Windows will occasionally inexplicably ignore the SetCursorPos() or SendInput() */
    SetCursorPos(x, y);
    SetCursorPos(x + 1, y);
    SetCursorPos(x, y);

    /* Flush any mouse motion prior to or associated with this warp */
#ifdef _MSC_VER /* We explicitly want to use GetTickCount(), not GetTickCount64() */
#pragma warning(push)
#pragma warning(disable : 28159)
#endif
    SDL_last_warp_time = GetTickCount();
    if (!SDL_last_warp_time) {
        SDL_last_warp_time = 1;
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

static int WIN_WarpMouse(SDL_Window *window, float x, float y)
{
    SDL_WindowData *data = window->driverdata;
    HWND hwnd = data->hwnd;
    POINT pt;

    /* Don't warp the mouse while we're doing a modal interaction */
    if (data->in_title_click || data->focus_click_pending) {
        return 0;
    }

    pt.x = (int)SDL_roundf(x);
    pt.y = (int)SDL_roundf(y);
    ClientToScreen(hwnd, &pt);
    WIN_SetCursorPos(pt.x, pt.y);

    /* Send the exact mouse motion associated with this warp */
    SDL_SendMouseMotion(0, window, SDL_GLOBAL_MOUSE_ID, SDL_FALSE, x, y);
    return 0;
}

static int WIN_WarpMouseGlobal(float x, float y)
{
    POINT pt;

    pt.x = (int)SDL_roundf(x);
    pt.y = (int)SDL_roundf(y);
    SetCursorPos(pt.x, pt.y);
    return 0;
}

static int WIN_SetRelativeMouseMode(SDL_bool enabled)
{
    return ToggleRawInput(enabled);
}

static int WIN_CaptureMouse(SDL_Window *window)
{
    if (window) {
        SDL_WindowData *data = window->driverdata;
        SetCapture(data->hwnd);
    } else {
        SDL_Window *focus_window = SDL_GetMouseFocus();

        if (focus_window) {
            SDL_WindowData *data = focus_window->driverdata;
            if (!data->mouse_tracked) {
                SDL_SetMouseFocus(NULL);
            }
        }
        ReleaseCapture();
    }

    return 0;
}

static Uint32 WIN_GetGlobalMouseState(float *x, float *y)
{
    Uint32 retval = 0;
    POINT pt = { 0, 0 };
    SDL_bool swapButtons = GetSystemMetrics(SM_SWAPBUTTON) != 0;

    GetCursorPos(&pt);
    *x = (float)pt.x;
    *y = (float)pt.y;

    retval |= GetAsyncKeyState(!swapButtons ? VK_LBUTTON : VK_RBUTTON) & 0x8000 ? SDL_BUTTON_LMASK : 0;
    retval |= GetAsyncKeyState(!swapButtons ? VK_RBUTTON : VK_LBUTTON) & 0x8000 ? SDL_BUTTON_RMASK : 0;
    retval |= GetAsyncKeyState(VK_MBUTTON) & 0x8000 ? SDL_BUTTON_MMASK : 0;
    retval |= GetAsyncKeyState(VK_XBUTTON1) & 0x8000 ? SDL_BUTTON_X1MASK : 0;
    retval |= GetAsyncKeyState(VK_XBUTTON2) & 0x8000 ? SDL_BUTTON_X2MASK : 0;

    return retval;
}

void WIN_InitMouse(SDL_VideoDevice *_this)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = WIN_CreateCursor;
    mouse->CreateSystemCursor = WIN_CreateSystemCursor;
    mouse->ShowCursor = WIN_ShowCursor;
    mouse->FreeCursor = WIN_FreeCursor;
    mouse->WarpMouse = WIN_WarpMouse;
    mouse->WarpMouseGlobal = WIN_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = WIN_SetRelativeMouseMode;
    mouse->CaptureMouse = WIN_CaptureMouse;
    mouse->GetGlobalMouseState = WIN_GetGlobalMouseState;

    SDL_SetDefaultCursor(WIN_CreateDefaultCursor());

    SDL_blank_cursor = WIN_CreateBlankCursor();

    WIN_UpdateMouseSystemScale();
}

void WIN_QuitMouse(SDL_VideoDevice *_this)
{
    if (rawInputEnableCount) { /* force RAWINPUT off here. */
        rawInputEnableCount = 1;
        ToggleRawInput(SDL_FALSE);
    }

    if (SDL_blank_cursor) {
        WIN_FreeCursor(SDL_blank_cursor);
        SDL_blank_cursor = NULL;
    }
}

/* For a great description of how the enhanced mouse curve works, see:
 * https://superuser.com/questions/278362/windows-mouse-acceleration-curve-smoothmousexcurve-and-smoothmouseycurve
 * http://www.esreality.com/?a=post&id=1846538/
 */
static SDL_bool LoadFiveFixedPointFloats(const BYTE *bytes, float *values)
{
    int i;

    for (i = 0; i < 5; ++i) {
        float fraction = (float)((Uint16)bytes[1] << 8 | bytes[0]) / 65535.0f;
        float value = (float)(((Uint16)bytes[3] << 8) | bytes[2]) + fraction;
        *values++ = value;
        bytes += 8;
    }
    return SDL_TRUE;
}

static void WIN_SetEnhancedMouseScale(int mouse_speed)
{
    float scale = (float)mouse_speed / 10.0f;
    HKEY hKey;
    DWORD dwType = REG_BINARY;
    BYTE value[40];
    DWORD length = sizeof(value);
    int i;
    float xpoints[5];
    float ypoints[5];
    float scale_points[10];
    const int dpi = USER_DEFAULT_SCREEN_DPI; // FIXME, how do we handle different monitors with different DPI?
    const float display_factor = 3.5f * (150.0f / dpi);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"SmoothMouseXCurve", 0, &dwType, value, &length) == ERROR_SUCCESS &&
            LoadFiveFixedPointFloats(value, xpoints) &&
            RegQueryValueExW(hKey, L"SmoothMouseYCurve", 0, &dwType, value, &length) == ERROR_SUCCESS &&
            LoadFiveFixedPointFloats(value, ypoints)) {
            for (i = 0; i < 5; ++i) {
                float gain;
                if (xpoints[i] > 0.0f) {
                    gain = (ypoints[i] / xpoints[i]) * scale;
                } else {
                    gain = 0.0f;
                }
                scale_points[i * 2] = xpoints[i];
                scale_points[i * 2 + 1] = gain / display_factor;
                // SDL_Log("Point %d = %f,%f\n", i, scale_points[i * 2], scale_points[i * 2 + 1]);
            }
            SDL_SetMouseSystemScale(SDL_arraysize(scale_points), scale_points);
        }
        RegCloseKey(hKey);
    }
}

static void WIN_SetLinearMouseScale(int mouse_speed)
{
    static float mouse_speed_scale[] = {
        0.0f,
        1 / 32.0f,
        1 / 16.0f,
        1 / 8.0f,
        2 / 8.0f,
        3 / 8.0f,
        4 / 8.0f,
        5 / 8.0f,
        6 / 8.0f,
        7 / 8.0f,
        1.0f,
        1.25f,
        1.5f,
        1.75f,
        2.0f,
        2.25f,
        2.5f,
        2.75f,
        3.0f,
        3.25f,
        3.5f
    };

    if (mouse_speed > 0 && mouse_speed < SDL_arraysize(mouse_speed_scale)) {
        SDL_SetMouseSystemScale(1, &mouse_speed_scale[mouse_speed]);
    }
}

void WIN_UpdateMouseSystemScale()
{
    int mouse_speed;
    int params[3] = { 0, 0, 0 };

    if (SystemParametersInfo(SPI_GETMOUSESPEED, 0, &mouse_speed, 0) &&
        SystemParametersInfo(SPI_GETMOUSE, 0, params, 0)) {
        if (params[2]) {
            WIN_SetEnhancedMouseScale(mouse_speed);
        } else {
            WIN_SetLinearMouseScale(mouse_speed);
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
