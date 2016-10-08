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

#if SDL_VIDEO_DRIVER_WAYLAND && SDL_VIDEO_OPENGL_EGL

#include "../SDL_sysvideo.h"
#include "../../events/SDL_windowevents_c.h"
#include "../SDL_egl_c.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandtouch.h"
#include "SDL_waylanddyn.h"

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
            uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
                 uint32_t edges, int32_t width, int32_t height)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;
    struct wl_region *region;

    /* wl_shell_surface spec states that this is a suggestion.
       Ignore if less than or greater than max/min size. */

    if (width == 0 || height == 0) {
        return;
    }

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        if ((window->flags & SDL_WINDOW_RESIZABLE)) {
            if (window->max_w > 0) {
                width = SDL_min(width, window->max_w);
            } 
            width = SDL_max(width, window->min_w);

            if (window->max_h > 0) {
                height = SDL_min(height, window->max_h);
            }
            height = SDL_max(height, window->min_h);
        } else {
            return;
        }
    }

    if (width == window->w && height == window->h) {
        return;
    }

    window->w = width;
    window->h = height;
    WAYLAND_wl_egl_window_resize(wind->egl_window, window->w, window->h, 0, 0);

    region = wl_compositor_create_region(wind->waylandData->compositor);
    wl_region_add(region, 0, 0, window->w, window->h);
    wl_surface_set_opaque_region(wind->surface, region);
    wl_region_destroy(region);
    SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, window->w, window->h);
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
static void
handle_onscreen_visibility(void *data,
        struct qt_extended_surface *qt_extended_surface, int32_t visible)
{
}

static void
handle_set_generic_property(void *data,
        struct qt_extended_surface *qt_extended_surface, const char *name,
        struct wl_array *value)
{
}

static void
handle_close(void *data, struct qt_extended_surface *qt_extended_surface)
{
    SDL_WindowData *window = (SDL_WindowData *)data;
    SDL_SendWindowEvent(window->sdlwindow, SDL_WINDOWEVENT_CLOSE, 0, 0);
}

static const struct qt_extended_surface_listener extended_surface_listener = {
    handle_onscreen_visibility,
    handle_set_generic_property,
    handle_close,
};
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

SDL_bool
Wayland_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    info->info.wl.display = data->waylandData->display;
    info->info.wl.surface = data->surface;
    info->info.wl.shell_surface = data->shell_surface;
    info->subsystem = SDL_SYSWM_WAYLAND;

    return SDL_TRUE;
}

int
Wayland_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0;  /* just succeed, the real work is done elsewhere. */
}

void Wayland_ShowWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    if (window->flags & SDL_WINDOW_FULLSCREEN)
        wl_shell_surface_set_fullscreen(wind->shell_surface,
                                        WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                        0, (struct wl_output *)window->fullscreen_mode.driverdata);
    else
        wl_shell_surface_set_toplevel(wind->shell_surface);

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void
Wayland_SetWindowFullscreen(_THIS, SDL_Window * window,
                            SDL_VideoDisplay * _display, SDL_bool fullscreen)
{
    SDL_WindowData *wind = window->driverdata;

    if (fullscreen)
        wl_shell_surface_set_fullscreen(wind->shell_surface,
                                        WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE,
                                        0, (struct wl_output *)_display->driverdata);
    else
        wl_shell_surface_set_toplevel(wind->shell_surface);

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void
Wayland_RestoreWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;

    wl_shell_surface_set_toplevel(wind->shell_surface);

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void
Wayland_MaximizeWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;

    wl_shell_surface_set_maximized(wind->shell_surface, NULL);

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

int Wayland_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    SDL_VideoData *c;
    struct wl_region *region;

    data = calloc(1, sizeof *data);
    if (data == NULL)
        return SDL_OutOfMemory();

    c = _this->driverdata;
    window->driverdata = data;

    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_GL_LoadLibrary(NULL);
        window->flags |= SDL_WINDOW_OPENGL;
    }

    if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        window->x = 0;
    }
    if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        window->y = 0;
    }

    data->waylandData = c;
    data->sdlwindow = window;

    data->surface =
        wl_compositor_create_surface(c->compositor);
    wl_surface_set_user_data(data->surface, data);
    data->shell_surface = wl_shell_get_shell_surface(c->shell,
                                                     data->surface);
    wl_shell_surface_set_class (data->shell_surface, c->classname);
#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH    
    if (c->surface_extension) {
        data->extended_surface = qt_surface_extension_get_extended_surface(
                c->surface_extension, data->surface);
    }
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

    data->egl_window = WAYLAND_wl_egl_window_create(data->surface,
                                            window->w, window->h);

    /* Create the GLES window surface */
    data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) data->egl_window);
    
    if (data->egl_surface == EGL_NO_SURFACE) {
        return SDL_SetError("failed to create a window surface");
    }

    if (data->shell_surface) {
        wl_shell_surface_set_user_data(data->shell_surface, data);
        wl_shell_surface_add_listener(data->shell_surface,
                                      &shell_surface_listener, data);
    }

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
    if (data->extended_surface) {
        qt_extended_surface_set_user_data(data->extended_surface, data);
        qt_extended_surface_add_listener(data->extended_surface,
                                         &extended_surface_listener, data);
    }
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

    region = wl_compositor_create_region(c->compositor);
    wl_region_add(region, 0, 0, window->w, window->h);
    wl_surface_set_opaque_region(data->surface, region);
    wl_region_destroy(region);

    if (c->relative_mouse_mode) {
        Wayland_input_lock_pointer(c->input);
    }

    WAYLAND_wl_display_flush(c->display);

    return 0;
}

void Wayland_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;
    struct wl_region *region;

    WAYLAND_wl_egl_window_resize(wind->egl_window, window->w, window->h, 0, 0);

    region =wl_compositor_create_region(data->compositor);
    wl_region_add(region, 0, 0, window->w, window->h);
    wl_surface_set_opaque_region(wind->surface, region);
    wl_region_destroy(region);
}

void Wayland_SetWindowTitle(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;
    
    if (window->title != NULL) {
        wl_shell_surface_set_title(wind->shell_surface, window->title);
    }

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void Wayland_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;

    if (data) {
        SDL_EGL_DestroySurface(_this, wind->egl_surface);
        WAYLAND_wl_egl_window_destroy(wind->egl_window);

        if (wind->shell_surface)
            wl_shell_surface_destroy(wind->shell_surface);

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
        if (wind->extended_surface)
            qt_extended_surface_destroy(wind->extended_surface);
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */
        wl_surface_destroy(wind->surface);

        SDL_free(wind);
        WAYLAND_wl_display_flush(data->display);
    }
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
