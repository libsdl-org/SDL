/*
  Simple DirectMedia Layer
  Copyright (C) 2026 BlackBerry Limited

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
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_qnx.h"

#include <errno.h>

static screen_context_t context;
static screen_event_t   event;
static bool video_initialized = false;

screen_context_t * getContext()
{
    return &context;
}

screen_event_t * getEvent()
{
    return &event;
}

/**
 * Initializes the QNX video plugin.
 * Creates the Screen context and event handles used for all window operations
 * by the plugin.
 * @param   SDL_VideoDevice *_this
 * @return  true if successful, false on error
 */
static bool videoInit(SDL_VideoDevice *_this)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode  display_mode;

    int size[2];
    int index, index2;
    int display_count, display_mode_count, active;

    screen_display_t      *screen_display;
    screen_display_mode_t *screen_display_mode;

    SDL_PixelFormat *pixel_format = getPixelFormat();

    if (video_initialized) {
        return true;
    }

    if (screen_create_context(&context, 0) < 0) {
        return false;
    }

    if (screen_create_event(&event) < 0) {
        return false;
    }

    /* start with no displays and increment as attached displays are found */
    if (screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &display_count) < 0) {
        return false;
    }

    screen_display = SDL_calloc(display_count, sizeof(screen_display_t));
    if (screen_display == NULL) {
        return false;
    }

    if (screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, (void **)screen_display) < 0) {
        return false;
    }

    /* create SDL displays based on display info from the screen API */
    for (index = 0; index < display_count; index++) {
        active = 0;
        if (screen_get_display_property_iv(screen_display[index], SCREEN_PROPERTY_ATTACHED, &active) < 0) {
            SDL_free(screen_display);
            return false;
        }

        if (active) {
            if (screen_get_display_property_iv(screen_display[index], SCREEN_PROPERTY_SIZE, size) < 0) {
                SDL_free(screen_display);
                return false;
            }

            SDL_zero(display);

            SDL_zero(display_mode);
            display_mode.w = size[0];
            display_mode.h = size[1];
            display_mode.refresh_rate = 60;
            // FIXME: Get screen format from the SCREEN_PROPERTY_FORMATS API
            //        instead with a heuristic to determine which is preferred.
            //        This should eliminate the need for a global tracking this
            //        in SDL_qnxgl.c
            display_mode.format = *pixel_format;
            display_mode.internal = NULL;

            // Added to current_mode when the display is added.
            display.desktop_mode = display_mode;

            if (!SDL_AddVideoDisplay(&display, false)) {
                SDL_free(screen_display);
                return false;
            }

            /* create SDL display imodes based on display mode info from the display */
            if (screen_get_display_property_iv(screen_display[index], SCREEN_PROPERTY_MODE_COUNT, &display_mode_count) < 0) {
                SDL_free(screen_display);
                return false;
            }

            screen_display_mode = SDL_calloc(display_mode_count, sizeof(screen_display_mode_t));
            if (screen_display_mode == NULL) {
                SDL_free(screen_display);
                return SDL_OutOfMemory();
            }

            if(screen_get_display_modes(screen_display[index], display_mode_count, screen_display_mode) < 0) {
                SDL_free(screen_display);
                SDL_free(screen_display_mode);
                return false;
            }

            for (index2 = 0; index2 < display_mode_count; index2++) {
                SDL_zero(display_mode);
                display_mode.w = screen_display_mode[index2].width;
                display_mode.h = screen_display_mode[index2].height;
                display_mode.refresh_rate = screen_display_mode[index2].refresh;
                display_mode.format = *pixel_format;

                if (!SDL_AddFullscreenDisplayMode(_this->displays[_this->num_displays-1], &display_mode)) {
                    break;
                }
            }

            SDL_free(screen_display_mode);
        }
    }

    initMouse(_this);

    // Assume we have a mouse and keyboard
    SDL_AddKeyboard(SDL_DEFAULT_KEYBOARD_ID, NULL, false);
    SDL_AddMouse(SDL_DEFAULT_MOUSE_ID, NULL, false);

    video_initialized = true;

    SDL_free(screen_display);

    return true;
}

static void videoQuit(SDL_VideoDevice *_this)
{
    if (video_initialized) {
        screen_destroy_event(event);
        screen_destroy_context(context);
        video_initialized = false;

        quitMouse(_this);
    }
}

/**
 * Creates a new native Screen window and associates it with the given SDL
 * window.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window to initialize
 * @return  true if successful, false on error
 */
static bool createWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData   *impl;
    SDL_VideoDisplay *display = NULL;
    SDL_DisplayMode  mode;

    int             size[2];
    int             numbufs;
    int             format;
    int             usage;
    int             has_focus_i;

    int             *screen_format = getScreenFormat();
    SDL_PixelFormat *pixel_format = getPixelFormat();

    impl = SDL_calloc(1, sizeof(*impl));
    if (!impl) {
        return false;
    }

    // Create a native window.
    if (screen_create_window(&impl->window, context) < 0) {
        goto fail;
    }

    // Set the native window's size to match the SDL window.
    size[0] = window->w;
    size[1] = window->h;

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE,
                                      size) < 0) {
        goto fail;
    }

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE,
                                      size) < 0) {
        goto fail;
    }

    if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_FORMAT,
                                      &format) < 0) {
        format = SCREEN_FORMAT_RGBX8888;
    }

    // Create window buffer(s).
    if (window->flags & SDL_WINDOW_OPENGL) {
        if (!glInitConfig(impl, &format)) {
            goto fail;
        }
        numbufs = 2;

        usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_OPENGL_ES3;
        if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_USAGE,
                                          &usage) < 0) {
            return false;
        }
    } else {
        *screen_format = format;
        *pixel_format = screenToPixelFormat(*screen_format);

        numbufs = 1;
    }

    // We now know what the pixel format is, so we need to provide it to the
    // right SDL APIs.
    display = SDL_GetVideoDisplayForWindow(window);
    if (display) {
        if (display->current_mode) {
            SDL_copyp(&mode, display->current_mode);
            mode.format = *pixel_format;
            display->desktop_mode = mode;
            SDL_SetCurrentDisplayMode(display, &mode);
        }
    }

    // Set pixel format.
    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_FORMAT,
                                      &format) < 0) {
        goto fail;
    }

    // Create buffer(s).
    if (screen_create_window_buffers(impl->window, numbufs>0?numbufs:1) < 0) {
        goto fail;
    }

    // Get initial focus state. Fallback to true.
    if(screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_FOCUS, &has_focus_i) < 0){
        impl->has_focus = true;
    } else {
        impl->has_focus = (bool)has_focus_i;
    }

    window->internal = impl;

    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_QNX_WINDOW_POINTER, impl->window);

    return true;

fail:
    if (impl->window) {
        screen_destroy_window(impl->window);
    }

    SDL_free(impl);
    return false;
}

/**
 * Gets a pointer to the Screen buffer associated with the given window. Note
 * that the buffer is actually created in createWindow().
 * @param       SDL_VideoDevice *_this
 * @param       window  SDL window to get the buffer for
 * @param[out]  pixels  Holds a pointer to the window's buffer
 * @param[out]  format  Holds the pixel format for the buffer
 * @param[out]  pitch   Holds the number of bytes per line
 * @return  true if successful, false on error
 */
static bool createWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window * window, SDL_PixelFormat * format,
                        void ** pixels, int *pitch)
{
    int             buffer_count;
    SDL_WindowData  *impl = (SDL_WindowData *)window->internal;
    screen_buffer_t *buffer;
    SDL_PixelFormat *pixel_format = getPixelFormat();

    if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_COUNT,
                                      &buffer_count) < 0) {
        return false;
    }
    buffer = SDL_calloc(buffer_count, sizeof(screen_buffer_t));

    // Get a pointer to the buffer's memory.
    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      (void **)buffer) < 0) {
        return false;
    }

    if (screen_get_buffer_property_pv(*buffer, SCREEN_PROPERTY_POINTER,
                                      pixels) < 0) {
        return false;
    }

    // Set format and pitch.
    if (screen_get_buffer_property_iv(*buffer, SCREEN_PROPERTY_STRIDE,
                                      pitch) < 0) {
        return false;
    }

    *format = *pixel_format;
    return true;
}

/**
 * Informs the window manager that the window needs to be updated.
 * @param   SDL_VideoDevice *_this
 * @param   window      The window to update
 * @param   rects       An array of reectangular areas to update
 * @param   numrects    Rect array length
 * @return  true if successful, false on error
 */
static bool updateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects,
                        int numrects)
{
    int buffer_count, *rects_int;
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    screen_buffer_t *buffer;

    if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_COUNT,
                                      &buffer_count) < 0) {
        return false;
    }
    buffer = SDL_calloc(buffer_count, sizeof(screen_buffer_t));

    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      buffer) < 0) {
        return false;
    }

    if(numrects>0){
        rects_int = SDL_calloc(4*numrects, sizeof(int));

        for(int i = 0; i < numrects; i++){
            rects_int[4*i]   = rects[i].x;
            rects_int[4*i+1] = rects[i].y;
            rects_int[4*i+2] = rects[i].w;
            rects_int[4*i+3] = rects[i].h;
        }

        if(screen_post_window(impl->window, buffer[0], numrects, rects_int, 0)) {
            return false;
        }
        if(screen_flush_context(context, 0)) {
            return false;
        }
    }
    return true;
}

/**
 * Runs the main event loop.
 * @param   SDL_VideoDevice *_this
 */
static void pumpEvents(SDL_VideoDevice *_this)
{
    SDL_Window      *window;
    SDL_WindowData   *impl;
    int             type;
    int             has_focus_i;
    bool            has_focus;

    // Let apps know the state of focus.
    for (window = _this->windows; window; window = window->next) {
        impl = (SDL_WindowData *)window->internal;
        if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_FOCUS, &has_focus_i) < 0){
            continue;
        }
        has_focus = (bool)has_focus_i;

        if (impl->has_focus != has_focus) {
            SDL_SendWindowEvent(window, (has_focus ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST), 0, 0);
            SDL_SendWindowEvent(window, (has_focus ? SDL_EVENT_WINDOW_MOUSE_ENTER : SDL_EVENT_WINDOW_MOUSE_LEAVE), 0, 0);
            // Update the SDL mouse to track the window it's focused on.
            SDL_SetMouseFocus(window);
            SDL_SetKeyboardFocus(window);
        }
    }

    for (;;) {
        if (screen_get_event(context, event, 0) < 0) {
            break;
        }

        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_TYPE, &type)
            < 0) {
            break;
        }

        if (type == SCREEN_EVENT_NONE) {
            break;
        }

        switch (type) {
        case SCREEN_EVENT_KEYBOARD:
            handleKeyboardEvent(event);
            break;

        case SCREEN_EVENT_POINTER:
            handlePointerEvent(event);
            break;

        default:
            break;
        }
    }
}

/**
 * Updates the size of the native window using the geometry of the SDL window.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window to update
 */
static void setWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    int             size[2];

    size[0] = window->pending.w;
    size[1] = window->pending.h;

    if (screen_destroy_window_buffers(impl->window) < 0) {
        return;
    }
    impl->resize = 1;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE, size);

    screen_create_window_buffers(impl->window, (window->flags & SDL_WINDOW_OPENGL) ? 2 : 1);
}

/**
 * Makes the native window associated with the given SDL window visible.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window to update
 */
static void showWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    const int       visible = 1;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
                                  &visible);
}

/**
 * Makes the native window associated with the given SDL window invisible.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window to update
 */
static void hideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    const int       visible = 0;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
        &visible);
}

/**
 * Destroys the native window associated with the given SDL window.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window that is being destroyed
 */
static void destroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;

    if (impl) {
        screen_destroy_window(impl->window);
        window->internal = NULL;
    }
}

/**
 * Frees the plugin object created by createDevice().
 * @param   device  Plugin object to free
 */
static void deleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

/**
 * Creates the QNX video plugin used by SDL.
 * @return  Initialized device if successful, NULL otherwise
 */
static SDL_VideoDevice *createDevice(void)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    device->internal = NULL;
    device->VideoInit = videoInit;
    device->VideoQuit = videoQuit;
    device->CreateSDLWindow = createWindow;
    device->CreateWindowFramebuffer = createWindowFramebuffer;
    device->UpdateWindowFramebuffer = updateWindowFramebuffer;
    device->SetWindowSize = setWindowSize;
    device->ShowWindow = showWindow;
    device->HideWindow = hideWindow;
    device->PumpEvents = pumpEvents;
    device->DestroyWindow = destroyWindow;

    device->GL_LoadLibrary = glLoadLibrary;
    device->GL_GetProcAddress = glGetProcAddress;
    device->GL_CreateContext = glCreateContext;
    device->GL_SetSwapInterval = glSetSwapInterval;
    device->GL_SwapWindow = glSwapWindow;
    device->GL_MakeCurrent = glMakeCurrent;
    device->GL_DestroyContext = glDeleteContext;
    device->GL_UnloadLibrary = glUnloadLibrary;

    device->free = deleteDevice;
    return device;
}

VideoBootStrap QNX_bootstrap = {
    "qnx", "QNX Screen",
    createDevice,
    NULL // no ShowMessageBox implementation
};
