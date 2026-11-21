/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_PUMPKINOS

/* PumpkinOS video driver.

   A PumpkinOS application is a PalmOS program that runs as a task inside the
   PumpkinOS process. Each task owns one screen (a surface in the host pixel
   format) which PumpkinOS composites into its window. This driver exposes that
   task screen as a single SDL window: the application draws into a shadow
   framebuffer and SDL_UpdateWindowSurface() copies the dirty rectangles into
   the task screen and marks them for upload.

   There is no GPU access and no window decoration: the window has exactly
   the size of the task screen. Resizing goes through pumpkin_change_display()
   and takes effect asynchronously, once the PumpkinOS main loop reallocates
   the task screen. */

#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_pumpkinvideo.h"
#include "SDL_pumpkinevents_c.h"
#include "SDL_pumpkinframebuffer_c.h"
#include "../../core/pumpkin/SDL_pumpkin.h"

#define PUMPKIN_VIDEO_DRIVER_NAME "pumpkinos"

// how long CreateWindow/SetWindowSize wait for PumpkinOS to apply a new size
#define PUMPKIN_RESIZE_TIMEOUT_MS 1500

static SDL_PixelFormat PUMPKIN_PixelFormatForEncoding(int encoding)
{
    switch (encoding) {
    case SURFACE_ENCODING_RGB565:
        return SDL_PIXELFORMAT_RGB565;
    case SURFACE_ENCODING_ARGB:
        return SDL_PIXELFORMAT_ARGB8888;
    case SURFACE_ENCODING_ABGR:
        return SDL_PIXELFORMAT_ABGR8888;
    default:
        // palette, gray and mono screens are converted pixel by pixel
        return SDL_PIXELFORMAT_XRGB8888;
    }
}

// Query the task screen; returns true if the size changed since the last call.
bool PUMPKIN_RefreshScreenInfo(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    UInt32 w = 0, h = 0;

    WinScreenGetAttribute(winScreenWidth, &w);
    WinScreenGetAttribute(winScreenHeight, &h);
    if (w == 0 || h == 0) {
        return false;
    }
    if ((int)w == data->screen_w && (int)h == data->screen_h) {
        return false;
    }
    data->screen_w = (int)w;
    data->screen_h = (int)h;
    return true;
}

static void PUMPKIN_UpdateDisplayMode(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    SDL_VideoDisplay *display = SDL_GetVideoDisplay(SDL_GetPrimaryDisplay());
    SDL_DisplayMode mode;

    if (!display) {
        return;
    }
    SDL_zero(mode);
    mode.format = data->format;
    mode.w = data->screen_w;
    mode.h = data->screen_h;
    SDL_SetDesktopDisplayMode(display, &mode);
    SDL_SetCurrentDisplayMode(display, &mode);
}

// Called from the event pump whenever the task screen may have changed size.
void PUMPKIN_CheckScreenSize(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    if (PUMPKIN_RefreshScreenInfo(_this)) {
        PUMPKIN_UpdateDisplayMode(_this);
        if (data->window) {
            SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_RESIZED, data->screen_w, data->screen_h);
        }
    }
}

// Ask PumpkinOS for a new task screen size and wait (pumping events) until it
// is applied or the timeout expires. Returns true if the screen now has the
// requested size.
static bool PUMPKIN_RequestScreenSize(SDL_VideoDevice *_this, int w, int h)
{
    SDL_VideoData *data = _this->internal;
    Uint64 deadline;

    if (w <= 0 || h <= 0) {
        return false;
    }
    if (w == data->screen_w && h == data->screen_h) {
        return true;
    }
    if (pumpkin_change_display(w, h) != 0) {
        return false;
    }

    deadline = SDL_GetTicks() + PUMPKIN_RESIZE_TIMEOUT_MS;
    for (;;) {
        PUMPKIN_PumpEvents(_this);
        if (data->screen_w == w && data->screen_h == h) {
            return true;
        }
        if (data->quit_sent || SDL_GetTicks() >= deadline) {
            return false;
        }
        SDL_Delay(10);
    }
}

static bool PUMPKIN_VideoInit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    SDL_DisplayMode mode;

    if (!PUMPKIN_RefreshScreenInfo(_this)) {
        return SDL_SetError("PumpkinOS: no task screen (SDL must run inside a PumpkinOS application)");
    }
    data->encoding = pumpkin_get_encoding();
    data->format = PUMPKIN_PixelFormatForEncoding(data->encoding);

    SDL_zero(mode);
    mode.format = data->format;
    mode.w = data->screen_w;
    mode.h = data->screen_h;
    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        return false;
    }

    PUMPKIN_InitEvents(_this);
    return true;
}

static void PUMPKIN_VideoQuit(SDL_VideoDevice *_this)
{
    PUMPKIN_ReleaseInput(_this);
}

static bool PUMPKIN_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata;

    if (data->window) {
        return SDL_SetError("PumpkinOS: only one window is supported");
    }

    wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(*wdata));
    if (!wdata) {
        return false;
    }
    window->internal = wdata;

    // The task screen is the window. Try to make it the requested size; if
    // PumpkinOS does not apply it, the window takes the size of the screen.
    if (!PUMPKIN_RequestScreenSize(_this, window->floating.w, window->floating.h)) {
        PUMPKIN_RefreshScreenInfo(_this);
    }
    window->x = 0;
    window->y = 0;
    window->w = data->screen_w;
    window->h = data->screen_h;
    window->flags &= ~(SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

    data->window = window;
    SDL_SetKeyboardFocus(window);
    SDL_SetMouseFocus(window);

    return true;
}

static void PUMPKIN_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata = window->internal;

    if (data->window == window) {
        PUMPKIN_ReleaseInput(_this);
        data->window = NULL;
    }
    if (wdata) {
        if (wdata->shadow) {
            SDL_DestroySurface(wdata->shadow);
        }
        SDL_free(wdata);
    }
    window->internal = NULL;
}

static void PUMPKIN_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->internal;

    if (PUMPKIN_RequestScreenSize(_this, window->pending.w, window->pending.h)) {
        return; // the RESIZED event was sent by the pump
    }
    // not applied: report the real size
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, data->screen_w, data->screen_h);
}

static bool PUMPKIN_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    // the task screen always sits at the origin of its PumpkinOS window
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, 0, 0);
    return true;
}

static bool PUMPKIN_SetClipboardText(SDL_VideoDevice *_this, const char *text)
{
    int len = text ? (int)SDL_strlen(text) : 0;

    if (pumpkin_clipboard_add_text((char *)(text ? text : ""), len) != 0) {
        return SDL_SetError("PumpkinOS: clipboard not available");
    }
    return true;
}

static char *PUMPKIN_GetClipboardText(SDL_VideoDevice *_this)
{
    // PumpkinOS copies at most *len bytes and does not report the full size,
    // so fetch into a generous fixed buffer.
    const int max = 64 * 1024;
    char *buf = (char *)SDL_malloc(max + 1);
    int len = max;

    if (!buf) {
        return NULL;
    }
    if (pumpkin_clipboard_get_text(buf, &len) != 0 || len < 0) {
        len = 0;
    }
    buf[len] = '\0';
    return buf;
}

static bool PUMPKIN_HasClipboardText(SDL_VideoDevice *_this)
{
    char buf[4];
    int len = 1;

    buf[0] = '\0';
    if (pumpkin_clipboard_get_text(buf, &len) != 0) {
        return false;
    }
    return len > 0 && buf[0] != '\0';
}

static void PUMPKIN_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->internal);
    SDL_free(device);
}

static SDL_VideoDevice *PUMPKIN_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_free(device);
        return NULL;
    }
    device->internal = data;
    device->name = "PumpkinOS";

    device->VideoInit = PUMPKIN_VideoInit;
    device->VideoQuit = PUMPKIN_VideoQuit;
    device->PumpEvents = PUMPKIN_PumpEvents;

    device->CreateSDLWindow = PUMPKIN_CreateWindow;
    device->DestroyWindow = PUMPKIN_DestroyWindow;
    device->SetWindowSize = PUMPKIN_SetWindowSize;
    device->SetWindowPosition = PUMPKIN_SetWindowPosition;

    device->CreateWindowFramebuffer = PUMPKIN_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PUMPKIN_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PUMPKIN_DestroyWindowFramebuffer;

    device->SetClipboardText = PUMPKIN_SetClipboardText;
    device->GetClipboardText = PUMPKIN_GetClipboardText;
    device->HasClipboardText = PUMPKIN_HasClipboardText;

    device->free = PUMPKIN_DeleteDevice;
    // no renderer-backed framebuffer: the task screen is written directly
    device->device_caps = 0;

    return device;
}

VideoBootStrap PUMPKIN_bootstrap = {
    PUMPKIN_VIDEO_DRIVER_NAME, "PumpkinOS video driver",
    PUMPKIN_CreateDevice,
    NULL, // no ShowMessageBox implementation
    false
};

#endif // SDL_VIDEO_DRIVER_PUMPKINOS
