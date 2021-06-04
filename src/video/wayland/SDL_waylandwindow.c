/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_hints.h"

#include "xdg-shell-client-protocol.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"

static float get_window_scale_factor(SDL_Window *window) {
      return ((SDL_WindowData*)window->driverdata)->scale_factor;
}

static void
CommitMinMaxDimensions(SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *data = wind->waylandData;
    int min_width, min_height, max_width, max_height;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        min_width = 0;
        min_height = 0;
        max_width = 0;
        max_height = 0;
    } else if (window->flags & SDL_WINDOW_RESIZABLE) {
        min_width = window->min_w;
        min_height = window->min_h;
        max_width = window->max_w;
        max_height = window->max_h;
    } else {
        min_width = window->w;
        min_height = window->h;
        max_width = window->w;
        max_height = window->h;
    }

    if (data->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_min_size(wind->shell_surface.xdg.roleobj.toplevel,
                                  min_width,
                                  min_height);
        xdg_toplevel_set_max_size(wind->shell_surface.xdg.roleobj.toplevel,
                                  max_width,
                                  max_height);
    } else if (data->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        zxdg_toplevel_v6_set_min_size(wind->shell_surface.zxdg.roleobj.toplevel,
                                      min_width,
                                      min_height);
        zxdg_toplevel_v6_set_max_size(wind->shell_surface.zxdg.roleobj.toplevel,
                                      max_width,
                                      max_height);
    }

    wl_surface_commit(wind->surface);
}

static void
SetFullscreen(SDL_Window *window, struct wl_output *output)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *viddata = wind->waylandData;

    /* The desktop may try to enforce min/max sizes here, so turn them off for
     * fullscreen and on (if applicable) for windowed
     */
    CommitMinMaxDimensions(window);

    if (viddata->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        if (output) {
            xdg_toplevel_set_fullscreen(wind->shell_surface.xdg.roleobj.toplevel, output);
        } else {
            xdg_toplevel_unset_fullscreen(wind->shell_surface.xdg.roleobj.toplevel);
        }
    } else if (viddata->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        if (output) {
            zxdg_toplevel_v6_set_fullscreen(wind->shell_surface.zxdg.roleobj.toplevel, output);
        } else {
            zxdg_toplevel_v6_unset_fullscreen(wind->shell_surface.zxdg.roleobj.toplevel);
        }
    } else {
        if (wind->shell_surface.wl == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        if (output) {
            wl_shell_surface_set_fullscreen(wind->shell_surface.wl,
                                            WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                            0, output);
        } else {
            wl_shell_surface_set_toplevel(wind->shell_surface.wl);
        }
    }

    WAYLAND_wl_display_flush(viddata->display);
}

/* On modern desktops, we probably will use the xdg-shell protocol instead
   of wl_shell, but wl_shell might be useful on older Wayland installs that
   don't have the newer protocol, or embedded things that don't have a full
   window manager. */

static void
handle_ping_wl_shell_surface(void *data, struct wl_shell_surface *shell_surface,
            uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure_wl_shell_surface(void *data, struct wl_shell_surface *shell_surface,
                 uint32_t edges, int32_t width, int32_t height)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;

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

    wind->resize.width = width;
    wind->resize.height = height;
    wind->resize.pending = SDL_TRUE;

    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        Wayland_HandlePendingResize(window);  /* OpenGL windows handle this in SwapWindow */
    }
}

static void
handle_popup_done_wl_shell_surface(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener_wl = {
    handle_ping_wl_shell_surface,
    handle_configure_wl_shell_surface,
    handle_popup_done_wl_shell_surface
};


static const struct wl_callback_listener surface_frame_listener;

static void
handle_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    SDL_WindowData *wind = (SDL_WindowData *) data;
    SDL_AtomicSet(&wind->swap_interval_ready, 1);  /* mark window as ready to present again. */

    /* reset this callback to fire again once a new frame was presented and compositor wants the next one. */
    wind->frame_callback = wl_surface_frame(wind->surface);
    wl_callback_destroy(cb);
    wl_callback_add_listener(wind->frame_callback, &surface_frame_listener, data);
}

static const struct wl_callback_listener surface_frame_listener = {
    handle_surface_frame_done
};


static void
handle_configure_zxdg_shell_surface(void *data, struct zxdg_surface_v6 *zxdg, uint32_t serial)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;
    struct wl_region *region;

    if (!wind->shell_surface.zxdg.initial_configure_seen) {
        window->w = 0;
        window->h = 0;
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, wind->resize.width, wind->resize.height);
        window->w = wind->resize.width;
        window->h = wind->resize.height;

        wl_surface_set_buffer_scale(wind->surface, get_window_scale_factor(window));
        if (wind->egl_window) {
            WAYLAND_wl_egl_window_resize(wind->egl_window, window->w * get_window_scale_factor(window), window->h * get_window_scale_factor(window), 0, 0);
        }

        zxdg_surface_v6_ack_configure(zxdg, serial);

        region = wl_compositor_create_region(wind->waylandData->compositor);
        wl_region_add(region, 0, 0, window->w, window->h);
        wl_surface_set_opaque_region(wind->surface, region);
        wl_region_destroy(region);

        wind->shell_surface.zxdg.initial_configure_seen = SDL_TRUE;
    } else {
        wind->resize.pending = SDL_TRUE;
        wind->resize.configure = SDL_TRUE;
        wind->resize.serial = serial;
        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            Wayland_HandlePendingResize(window);  /* OpenGL windows handle this in SwapWindow */
        }
    }
}

static const struct zxdg_surface_v6_listener shell_surface_listener_zxdg = {
    handle_configure_zxdg_shell_surface
};


static void
handle_configure_zxdg_toplevel(void *data,
              struct zxdg_toplevel_v6 *zxdg_toplevel_v6,
              int32_t width,
              int32_t height,
              struct wl_array *states)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;

    enum zxdg_toplevel_v6_state *state;
    SDL_bool fullscreen = SDL_FALSE;
    SDL_bool maximized = SDL_FALSE;
    wl_array_for_each(state, states) {
        if (*state == ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN) {
            fullscreen = SDL_TRUE;
        } else if (*state == ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED) {
            maximized = SDL_TRUE;
        }
    }

    if (!fullscreen) {
        if (window->flags & SDL_WINDOW_FULLSCREEN) {
            /* We might need to re-enter fullscreen after being restored from minimized */
            SDL_WaylandOutputData *driverdata = (SDL_WaylandOutputData *) SDL_GetDisplayForWindow(window)->driverdata;
            SetFullscreen(window, driverdata->output);
            fullscreen = SDL_TRUE;
        }

        if (width == 0 || height == 0) {
            width = window->windowed.w;
            height = window->windowed.h;
        }

        /* zxdg_toplevel spec states that this is a suggestion.
           Ignore if less than or greater than max/min size. */

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
            wind->resize.width = window->w;
            wind->resize.height = window->h;
            return;
        }
    }

    /* Always send a maximized/restore event; if the event is redundant it will
     * automatically be discarded (see src/events/SDL_windowevents.c).
     *
     * No, we do not get minimize events from zxdg-shell.
     */
    if (!fullscreen) {
        SDL_SendWindowEvent(window,
                            maximized ?
                                SDL_WINDOWEVENT_MAXIMIZED :
                                SDL_WINDOWEVENT_RESTORED,
                            0, 0);
    }

    if (width == 0 || height == 0) {
        wind->resize.width = window->w;
        wind->resize.height = window->h;
        return;
    }

    wind->resize.width = width;
    wind->resize.height = height;
}

static void
handle_close_zxdg_toplevel(void *data, struct zxdg_toplevel_v6 *zxdg_toplevel_v6)
{
    SDL_WindowData *window = (SDL_WindowData *)data;
    SDL_SendWindowEvent(window->sdlwindow, SDL_WINDOWEVENT_CLOSE, 0, 0);
}

static const struct zxdg_toplevel_v6_listener toplevel_listener_zxdg = {
    handle_configure_zxdg_toplevel,
    handle_close_zxdg_toplevel
};



static void
handle_configure_xdg_shell_surface(void *data, struct xdg_surface *xdg, uint32_t serial)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;
    struct wl_region *region;

    if (!wind->shell_surface.xdg.initial_configure_seen) {
        window->w = 0;
        window->h = 0;
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, wind->resize.width, wind->resize.height);
        window->w = wind->resize.width;
        window->h = wind->resize.height;

        wl_surface_set_buffer_scale(wind->surface, get_window_scale_factor(window));
        if (wind->egl_window) {
            WAYLAND_wl_egl_window_resize(wind->egl_window, window->w * get_window_scale_factor(window), window->h * get_window_scale_factor(window), 0, 0);
        }

        xdg_surface_ack_configure(xdg, serial);

        region = wl_compositor_create_region(wind->waylandData->compositor);
        wl_region_add(region, 0, 0, window->w, window->h);
        wl_surface_set_opaque_region(wind->surface, region);
        wl_region_destroy(region);

        wind->shell_surface.xdg.initial_configure_seen = SDL_TRUE;
    } else {
        wind->resize.pending = SDL_TRUE;
        wind->resize.configure = SDL_TRUE;
        wind->resize.serial = serial;
        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            Wayland_HandlePendingResize(window);  /* OpenGL windows handle this in SwapWindow */
        }
    }
}

static const struct xdg_surface_listener shell_surface_listener_xdg = {
    handle_configure_xdg_shell_surface
};


static void
handle_configure_xdg_toplevel(void *data,
              struct xdg_toplevel *xdg_toplevel,
              int32_t width,
              int32_t height,
              struct wl_array *states)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;

    enum xdg_toplevel_state *state;
    SDL_bool fullscreen = SDL_FALSE;
    SDL_bool maximized = SDL_FALSE;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
            fullscreen = SDL_TRUE;
        } else if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) {
            maximized = SDL_TRUE;
        }
    }

    if (!fullscreen) {
        if (window->flags & SDL_WINDOW_FULLSCREEN) {
            /* We might need to re-enter fullscreen after being restored from minimized */
            SDL_WaylandOutputData *driverdata = (SDL_WaylandOutputData *) SDL_GetDisplayForWindow(window)->driverdata;
            SetFullscreen(window, driverdata->output);
            fullscreen = SDL_TRUE;
        }

        if (width == 0 || height == 0) {
            width = window->windowed.w;
            height = window->windowed.h;
        }

        /* xdg_toplevel spec states that this is a suggestion.
           Ignore if less than or greater than max/min size. */

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
            wind->resize.width = window->w;
            wind->resize.height = window->h;
            return;
        }
    }

    /* Always send a maximized/restore event; if the event is redundant it will
     * automatically be discarded (see src/events/SDL_windowevents.c)
     *
     * No, we do not get minimize events from xdg-shell.
     */
    if (!fullscreen) {
        SDL_SendWindowEvent(window,
                            maximized ?
                                SDL_WINDOWEVENT_MAXIMIZED :
                                SDL_WINDOWEVENT_RESTORED,
                            0, 0);
    }

    if (width == 0 || height == 0) {
        wind->resize.width = window->w;
        wind->resize.height = window->h;
        return;
    }

    wind->resize.width = width;
    wind->resize.height = height;
}

static void
handle_close_xdg_toplevel(void *data, struct xdg_toplevel *xdg_toplevel)
{
    SDL_WindowData *window = (SDL_WindowData *)data;
    SDL_SendWindowEvent(window->sdlwindow, SDL_WINDOWEVENT_CLOSE, 0, 0);
}

static const struct xdg_toplevel_listener toplevel_listener_xdg = {
    handle_configure_xdg_toplevel,
    handle_close_xdg_toplevel
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

static void
update_scale_factor(SDL_WindowData *window) {
   float old_factor = window->scale_factor, new_factor = 0.0;
   int i;

   if (!(window->sdlwindow->flags & SDL_WINDOW_ALLOW_HIGHDPI)) {
       return;
   }

   if (!window->num_outputs) {
       new_factor = old_factor;
   }

   if (FULLSCREEN_VISIBLE(window->sdlwindow)) {
       SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window->sdlwindow);
       SDL_WaylandOutputData* driverdata = display->driverdata;
       new_factor = driverdata->scale_factor;
   }

   for (i = 0; i < window->num_outputs; i++) {
       SDL_WaylandOutputData* driverdata = wl_output_get_user_data(window->outputs[i]);
       float factor = driverdata->scale_factor;
       if (factor > new_factor) {
           new_factor = factor;
       }
   }

   if (new_factor != old_factor) {
       /* force the resize event to trigger, as the logical size didn't change */
       window->resize.width = window->sdlwindow->w;
       window->resize.height = window->sdlwindow->h;
       window->resize.scale_factor = new_factor;
       window->resize.pending = SDL_TRUE;
       if (!(window->sdlwindow->flags & SDL_WINDOW_OPENGL)) {
           Wayland_HandlePendingResize(window->sdlwindow);  /* OpenGL windows handle this in SwapWindow */
       }
   }
}

/* While we can't get window position from the compositor, we do at least know
 * what monitor we're on, so let's send move events that put the window at the
 * center of the whatever display the wl_surface_listener events give us.
 */
static void
Wayland_move_window(SDL_Window *window,
                    SDL_WaylandOutputData *driverdata)
{
    int i, numdisplays = SDL_GetNumVideoDisplays();
    for (i = 0; i < numdisplays; i += 1) {
        if (SDL_GetDisplay(i)->driverdata == driverdata) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MOVED,
                                SDL_WINDOWPOS_CENTERED_DISPLAY(i),
                                SDL_WINDOWPOS_CENTERED_DISPLAY(i));
            break;
        }
    }
}

static void
handle_surface_enter(void *data, struct wl_surface *surface,
                     struct wl_output *output)
{
    SDL_WindowData *window = data;

    window->outputs = SDL_realloc(window->outputs, (window->num_outputs + 1) * sizeof *window->outputs);
    window->outputs[window->num_outputs++] = output;
    update_scale_factor(window);

    Wayland_move_window(window->sdlwindow, wl_output_get_user_data(output));
}

static void
handle_surface_leave(void *data, struct wl_surface *surface,
                     struct wl_output *output)
{
    SDL_WindowData *window = data;
    int i, send_move_event = 0;

    for (i = 0; i < window->num_outputs; i++) {
        if (window->outputs[i] == output) {  /* remove this one */
            if (i == (window->num_outputs-1)) {
                window->outputs[i] = NULL;
                send_move_event = 1;
            } else {
                SDL_memmove(&window->outputs[i], &window->outputs[i+1], sizeof (output) * ((window->num_outputs - i) - 1));
            }
            window->num_outputs--;
            i--;
        }
    }

    if (window->num_outputs == 0) {
       SDL_free(window->outputs);
       window->outputs = NULL;
    } else if (send_move_event) {
        Wayland_move_window(window->sdlwindow,
                            wl_output_get_user_data(window->outputs[window->num_outputs - 1]));
    }

    update_scale_factor(window);
}

static const struct wl_surface_listener surface_listener = {
    handle_surface_enter,
    handle_surface_leave
};

SDL_bool
Wayland_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    const Uint32 version = SDL_VERSIONNUM((Uint32)info->version.major,
                                          (Uint32)info->version.minor,
                                          (Uint32)info->version.patch);

    /* Before 2.0.6, it was possible to build an SDL with Wayland support
       (SDL_SysWMinfo will be large enough to hold Wayland info), but build
       your app against SDL headers that didn't have Wayland support
       (SDL_SysWMinfo could be smaller than Wayland needs. This would lead
       to an app properly using SDL_GetWindowWMInfo() but we'd accidentally
       overflow memory on the stack or heap. To protect against this, we've
       padded out the struct unconditionally in the headers and Wayland will
       just return an error for older apps using this function. Those apps
       will need to be recompiled against newer headers or not use Wayland,
       maybe by forcing SDL_VIDEODRIVER=x11. */
    if (version < SDL_VERSIONNUM(2, 0, 6)) {
        info->subsystem = SDL_SYSWM_UNKNOWN;
        SDL_SetError("Version must be 2.0.6 or newer");
        return SDL_FALSE;
    }

    info->info.wl.display = data->waylandData->display;
    info->info.wl.surface = data->surface;
    info->info.wl.shell_surface = data->shell_surface.wl;
    if (version >= SDL_VERSIONNUM(2, 0, 15)) {
        info->info.wl.egl_window = data->egl_window;
    }
    info->subsystem = SDL_SYSWM_WAYLAND;

    return SDL_TRUE;
}

int
Wayland_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0;  /* just succeed, the real work is done elsewhere. */
}

int
Wayland_SetWindowModalFor(_THIS, SDL_Window *modal_window, SDL_Window *parent_window)
{
    const SDL_VideoData *viddata = (const SDL_VideoData *) _this->driverdata;
    SDL_WindowData *modal_data = modal_window->driverdata;
    SDL_WindowData *parent_data = parent_window->driverdata;

    if (viddata->shell.xdg) {
        if (modal_data->shell_surface.xdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Modal window was hidden");
        }
        if (parent_data->shell_surface.xdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Parent window was hidden");
        }
        xdg_toplevel_set_parent(modal_data->shell_surface.xdg.roleobj.toplevel,
                                parent_data->shell_surface.xdg.roleobj.toplevel);
    } else if (viddata->shell.zxdg) {
        if (modal_data->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Modal window was hidden");
        }
        if (parent_data->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Parent window was hidden");
        }
        zxdg_toplevel_v6_set_parent(modal_data->shell_surface.zxdg.roleobj.toplevel,
                                    parent_data->shell_surface.zxdg.roleobj.toplevel);
    } else {
        return SDL_Unsupported();
    }

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
    return 0;
}

void Wayland_ShowWindow(_THIS, SDL_Window *window)
{
    SDL_VideoData *c = _this->driverdata;
    SDL_WindowData *data = window->driverdata;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);

    /* Detach any previous buffers before resetting everything, otherwise when
     * calling this a second time you'll get an annoying protocol error
     */
    wl_surface_attach(data->surface, NULL, 0, 0);
    wl_surface_commit(data->surface);

    /* Create the shell surface and map the toplevel */
    if (c->shell.xdg) {
        data->shell_surface.xdg.surface = xdg_wm_base_get_xdg_surface(c->shell.xdg, data->surface);
        xdg_surface_set_user_data(data->shell_surface.xdg.surface, data);
        xdg_surface_add_listener(data->shell_surface.xdg.surface, &shell_surface_listener_xdg, data);

        /* !!! FIXME: add popup role */
        data->shell_surface.xdg.roleobj.toplevel = xdg_surface_get_toplevel(data->shell_surface.xdg.surface);
        xdg_toplevel_set_app_id(data->shell_surface.xdg.roleobj.toplevel, c->classname);
        xdg_toplevel_add_listener(data->shell_surface.xdg.roleobj.toplevel, &toplevel_listener_xdg, data);
    } else if (c->shell.zxdg) {
        data->shell_surface.zxdg.surface = zxdg_shell_v6_get_xdg_surface(c->shell.zxdg, data->surface);
        zxdg_surface_v6_set_user_data(data->shell_surface.zxdg.surface, data);
        zxdg_surface_v6_add_listener(data->shell_surface.zxdg.surface, &shell_surface_listener_zxdg, data);

        /* !!! FIXME: add popup role */
        data->shell_surface.zxdg.roleobj.toplevel = zxdg_surface_v6_get_toplevel(data->shell_surface.zxdg.surface);
        zxdg_toplevel_v6_add_listener(data->shell_surface.zxdg.roleobj.toplevel, &toplevel_listener_zxdg, data);
        zxdg_toplevel_v6_set_app_id(data->shell_surface.zxdg.roleobj.toplevel, c->classname);
    } else {
        data->shell_surface.wl = wl_shell_get_shell_surface(c->shell.wl, data->surface);
        wl_shell_surface_set_class(data->shell_surface.wl, c->classname);
        wl_shell_surface_set_user_data(data->shell_surface.wl, data);
        wl_shell_surface_add_listener(data->shell_surface.wl, &shell_surface_listener_wl, data);
    }

    /* Restore state that was set prior to this call */
    Wayland_SetWindowTitle(_this, window);
    if (window->flags & SDL_WINDOW_MAXIMIZED) {
        Wayland_MaximizeWindow(_this, window);
    }
    if (window->flags & SDL_WINDOW_MINIMIZED) {
        Wayland_MinimizeWindow(_this, window);
    }
    Wayland_SetWindowFullscreen(_this, window, display, (window->flags & SDL_WINDOW_FULLSCREEN) != 0);

    /* We have to wait until the surface gets a "configure" event, or use of
     * this surface will fail. This is a new rule for xdg_shell.
     */
    if (c->shell.xdg) {
        if (data->shell_surface.xdg.surface) {
            while (!data->shell_surface.xdg.initial_configure_seen) {
                WAYLAND_wl_display_flush(c->display);
                WAYLAND_wl_display_dispatch(c->display);
            }
        }

        /* Create the window decorations */
        if (data->shell_surface.xdg.roleobj.toplevel && c->decoration_manager) {
            data->server_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(c->decoration_manager, data->shell_surface.xdg.roleobj.toplevel);
        }
    } else if (c->shell.zxdg) {
        if (data->shell_surface.zxdg.surface) {
            while (!data->shell_surface.zxdg.initial_configure_seen) {
                WAYLAND_wl_display_flush(c->display);
                WAYLAND_wl_display_dispatch(c->display);
            }
        }
    }

    /* Unlike the rest of window state we have to set this _after_ flushing the
     * display, because we need to create the decorations before possibly hiding
     * them immediately afterward. But don't call it redundantly, the protocol
     * may not interpret a redundant call nicely and cause weird stuff to happen
     */
    if (window->flags & SDL_WINDOW_BORDERLESS) {
        Wayland_SetWindowBordered(_this, window, SDL_FALSE);
    }

    /* We're finally done putting the window together, raise if possible */
    if (c->activation_manager) {
        /* Note that we don't check for empty strings, as that is still
         * considered a valid activation token!
         */
        const char *activation_token = SDL_getenv("XDG_ACTIVATION_TOKEN");
        if (activation_token) {
            xdg_activation_v1_activate(c->activation_manager,
                                       activation_token,
                                       data->surface);

            /* Clear this variable, per the protocol's request */
            unsetenv("XDG_ACTIVATION_TOKEN");
        }
    }
}

void Wayland_HideWindow(_THIS, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;

    if (wind->server_decoration) {
       zxdg_toplevel_decoration_v1_destroy(wind->server_decoration);
       wind->server_decoration = NULL;
    }

    if (data->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel) {
            xdg_toplevel_destroy(wind->shell_surface.xdg.roleobj.toplevel);
            wind->shell_surface.xdg.roleobj.toplevel = NULL;
        }
        if (wind->shell_surface.xdg.surface) {
            xdg_surface_destroy(wind->shell_surface.xdg.surface);
            wind->shell_surface.xdg.surface = NULL;
        }
    } else if (data->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel) {
            zxdg_toplevel_v6_destroy(wind->shell_surface.zxdg.roleobj.toplevel);
            wind->shell_surface.zxdg.roleobj.toplevel = NULL;
        }
        if (wind->shell_surface.zxdg.surface) {
            zxdg_surface_v6_destroy(wind->shell_surface.zxdg.surface);
            wind->shell_surface.zxdg.surface = NULL;
        }
    } else {
        if (wind->shell_surface.wl) {
            wl_shell_surface_destroy(wind->shell_surface.wl);
            wind->shell_surface.wl = NULL;
        }
    }
}

static void
handle_xdg_activation_done(void *data,
                           struct xdg_activation_token_v1 *xdg_activation_token_v1,
                           const char *token)
{
    SDL_WindowData *window = data;
    if (xdg_activation_token_v1 == window->activation_token) {
        xdg_activation_v1_activate(window->waylandData->activation_manager,
                                   token,
                                   window->surface);
        xdg_activation_token_v1_destroy(window->activation_token);
        window->activation_token = NULL;
    }
}

static const struct xdg_activation_token_v1_listener activation_listener_xdg = {
    handle_xdg_activation_done
};

/* The xdg-activation protocol considers "activation" to be one of two things:
 *
 * 1: Raising a window to the top and flashing the titlebar
 * 2: Flashing the titlebar while keeping the window where it is
 *
 * As you might expect from Wayland, the general policy is to go with #2 unless
 * the client can prove to the compositor beyond a reasonable doubt that raising
 * the window will not be malicuous behavior.
 *
 * For SDL this means RaiseWindow and FlashWindow both use the same protocol,
 * but in different ways: RaiseWindow will provide as _much_ information as
 * possible while FlashWindow will provide as _little_ information as possible,
 * to nudge the compositor into doing what we want.
 *
 * This isn't _strictly_ what the protocol says will happen, but this is what
 * current implementations are doing (as of writing, YMMV in the far distant
 * future).
 *
 * -flibit
 */
static void
Wayland_activate_window(SDL_VideoData *data, SDL_WindowData *wind,
                        struct wl_surface *surface,
                        uint32_t serial, struct wl_seat *seat)
{
    if (data->activation_manager) {
        if (wind->activation_token != NULL) {
            /* We're about to overwrite this with a new request */
            xdg_activation_token_v1_destroy(wind->activation_token);
        }

        wind->activation_token = xdg_activation_v1_get_activation_token(data->activation_manager);
        xdg_activation_token_v1_add_listener(wind->activation_token,
                                             &activation_listener_xdg,
                                             wind);

        /* Note that we are not setting the app_id or serial here.
         *
         * Hypothetically we could set the app_id from data->classname, but
         * that part of the API is for _external_ programs, not ourselves.
         *
         * -flibit
         */
        if (surface != NULL) {
            xdg_activation_token_v1_set_surface(wind->activation_token, surface);
        }
        if (seat != NULL) {
            xdg_activation_token_v1_set_serial(wind->activation_token, serial, seat);
        }
        xdg_activation_token_v1_commit(wind->activation_token);
    }
}

void
Wayland_RaiseWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    /* FIXME: This Raise event is arbitrary and doesn't come from an event, so
     * it's actually very likely that this token will be ignored! Maybe add
     * support for passing serials (and the associated seat) so this can have
     * a better chance of actually raising the window.
     * -flibit
     */
    Wayland_activate_window(_this->driverdata,
                            wind,
                            wind->surface,
                            0,
                            NULL);
}

int
Wayland_FlashWindow(_THIS, SDL_Window *window, Uint32 flash_count)
{
    Wayland_activate_window(_this->driverdata,
                            window->driverdata,
                            NULL,
                            0,
                            NULL);
    return 0;
}

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
static void SDLCALL
QtExtendedSurface_OnHintChanged(void *userdata, const char *name,
        const char *oldValue, const char *newValue)
{
    struct qt_extended_surface *qt_extended_surface = userdata;

    if (name == NULL) {
        return;
    }

    if (strcmp(name, SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION) == 0) {
        int32_t orientation = QT_EXTENDED_SURFACE_ORIENTATION_PRIMARYORIENTATION;

        if (newValue != NULL) {
            if (strcmp(newValue, "portrait") == 0) {
                orientation = QT_EXTENDED_SURFACE_ORIENTATION_PORTRAITORIENTATION;
            } else if (strcmp(newValue, "landscape") == 0) {
                orientation = QT_EXTENDED_SURFACE_ORIENTATION_LANDSCAPEORIENTATION;
            } else if (strcmp(newValue, "inverted-portrait") == 0) {
                orientation = QT_EXTENDED_SURFACE_ORIENTATION_INVERTEDPORTRAITORIENTATION;
            } else if (strcmp(newValue, "inverted-landscape") == 0) {
                orientation = QT_EXTENDED_SURFACE_ORIENTATION_INVERTEDLANDSCAPEORIENTATION;
            }
        }

        qt_extended_surface_set_content_orientation(qt_extended_surface, orientation);
    } else if (strcmp(name, SDL_HINT_QTWAYLAND_WINDOW_FLAGS) == 0) {
        uint32_t flags = 0;

        if (newValue != NULL) {
            char *tmp = strdup(newValue);
            char *saveptr = NULL;

            char *flag = strtok_r(tmp, " ", &saveptr);
            while (flag) {
                if (strcmp(flag, "OverridesSystemGestures") == 0) {
                    flags |= QT_EXTENDED_SURFACE_WINDOWFLAG_OVERRIDESSYSTEMGESTURES;
                } else if (strcmp(flag, "StaysOnTop") == 0) {
                    flags |= QT_EXTENDED_SURFACE_WINDOWFLAG_STAYSONTOP;
                } else if (strcmp(flag, "BypassWindowManager") == 0) {
                    // See https://github.com/qtproject/qtwayland/commit/fb4267103d
                    flags |= 4 /* QT_EXTENDED_SURFACE_WINDOWFLAG_BYPASSWINDOWMANAGER */;
                }

                flag = strtok_r(NULL, " ", &saveptr);
            }

            free(tmp);
        }

        qt_extended_surface_set_window_flags(qt_extended_surface, flags);
    }
}

static void QtExtendedSurface_Subscribe(struct qt_extended_surface *surface, const char *name)
{
    SDL_AddHintCallback(name, QtExtendedSurface_OnHintChanged, surface);
}

static void QtExtendedSurface_Unsubscribe(struct qt_extended_surface *surface, const char *name)
{
    SDL_DelHintCallback(name, QtExtendedSurface_OnHintChanged, surface);
}
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

void
Wayland_SetWindowFullscreen(_THIS, SDL_Window * window,
                            SDL_VideoDisplay * _display, SDL_bool fullscreen)
{
    struct wl_output *output = ((SDL_WaylandOutputData*) _display->driverdata)->output;
    SetFullscreen(window, fullscreen ? output : NULL);
}

void
Wayland_RestoreWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;
    const SDL_VideoData *viddata = (const SDL_VideoData *) _this->driverdata;

    /* Set this flag now even if we never actually maximized, eventually
     * ShowWindow will take care of it along with the other window state.
     */
    window->flags &= ~SDL_WINDOW_MAXIMIZED;

    /* Note that xdg-shell does NOT provide a way to unset minimize! */
    if (viddata->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_unset_maximized(wind->shell_surface.xdg.roleobj.toplevel);
    } else if (viddata->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        zxdg_toplevel_v6_unset_maximized(wind->shell_surface.zxdg.roleobj.toplevel);
    } else {
        if (wind->shell_surface.wl == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        wl_shell_surface_set_toplevel(wind->shell_surface.wl);
    }

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void
Wayland_SetWindowBordered(_THIS, SDL_Window * window, SDL_bool bordered)
{
    SDL_WindowData *wind = window->driverdata;
    const SDL_VideoData *viddata = (const SDL_VideoData *) _this->driverdata;
    if ((viddata->decoration_manager) && (wind->server_decoration)) {
        const enum zxdg_toplevel_decoration_v1_mode mode = bordered ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        zxdg_toplevel_decoration_v1_set_mode(wind->server_decoration, mode);
    }
}

void
Wayland_SetWindowResizable(_THIS, SDL_Window * window, SDL_bool resizable)
{
    CommitMinMaxDimensions(window);
}

void
Wayland_MaximizeWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *viddata = (SDL_VideoData *) _this->driverdata;

    if (!(window->flags & SDL_WINDOW_RESIZABLE)) {
        return;
    }

    /* Set this flag now even if we don't actually maximize yet, eventually
     * ShowWindow will take care of it along with the other window state.
     */
    window->flags |= SDL_WINDOW_MAXIMIZED;

    if (viddata->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_maximized(wind->shell_surface.xdg.roleobj.toplevel);
    } else if (viddata->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        zxdg_toplevel_v6_set_maximized(wind->shell_surface.zxdg.roleobj.toplevel);
    } else {
        if (wind->shell_surface.wl == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        wl_shell_surface_set_maximized(wind->shell_surface.wl, NULL);
    }

    WAYLAND_wl_display_flush( viddata->display );
}

void
Wayland_MinimizeWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *viddata = (SDL_VideoData *) _this->driverdata;

    if (viddata->shell.xdg) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_minimized(wind->shell_surface.xdg.roleobj.toplevel);
    } else if (viddata->shell.zxdg) {
        if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        zxdg_toplevel_v6_set_minimized(wind->shell_surface.zxdg.roleobj.toplevel);
    }

    WAYLAND_wl_display_flush(viddata->display);
}

void
Wayland_SetWindowMouseGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (grabbed) {
        Wayland_input_confine_pointer(window, data->input);
    } else {
        Wayland_input_unconfine_pointer(data->input);
    }
}

void
Wayland_SetWindowKeyboardGrab(_THIS, SDL_Window *window, SDL_bool grabbed)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (grabbed) {
        Wayland_input_grab_keyboard(window, data->input);
    } else {
        Wayland_input_ungrab_keyboard(window);
    }
}

int Wayland_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    SDL_VideoData *c;
    struct wl_region *region;

    data = SDL_calloc(1, sizeof *data);
    if (data == NULL)
        return SDL_OutOfMemory();

    c = _this->driverdata;
    window->driverdata = data;

    if (!(window->flags & SDL_WINDOW_VULKAN)) {
        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            SDL_GL_LoadLibrary(NULL);
            window->flags |= SDL_WINDOW_OPENGL;
        }
    }

    if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        window->x = 0;
    }
    if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        window->y = 0;
    }

    data->waylandData = c;
    data->sdlwindow = window;

    data->scale_factor = 1.0;

    if (window->flags & SDL_WINDOW_ALLOW_HIGHDPI) {
        int i;
        for (i=0; i < SDL_GetVideoDevice()->num_displays; i++) {
            float scale = ((SDL_WaylandOutputData*)SDL_GetVideoDevice()->displays[i].driverdata)->scale_factor;
            if (scale > data->scale_factor) {
                data->scale_factor = scale;
            }
        }
    }

    data->resize.pending = SDL_FALSE;
    data->resize.width = window->w;
    data->resize.height = window->h;
    data->resize.scale_factor = data->scale_factor;

    data->outputs = NULL;
    data->num_outputs = 0;

    data->surface =
        wl_compositor_create_surface(c->compositor);
    wl_surface_add_listener(data->surface, &surface_listener, data);

    /* Fire a callback when the compositor wants a new frame rendered.
     * Right now this only matters for OpenGL; we use this callback to add a
     * wait timeout that avoids getting deadlocked by the compositor when the
     * window isn't visible.
     */
    if (window->flags & SDL_WINDOW_OPENGL) {
        wl_callback_add_listener(wl_surface_frame(data->surface), &surface_frame_listener, data);
    }

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
    if (c->surface_extension) {
        data->extended_surface = qt_surface_extension_get_extended_surface(
                c->surface_extension, data->surface);

        QtExtendedSurface_Subscribe(data->extended_surface, SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION);
        QtExtendedSurface_Subscribe(data->extended_surface, SDL_HINT_QTWAYLAND_WINDOW_FLAGS);
    }
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

    if (window->flags & SDL_WINDOW_OPENGL) {
        data->egl_window = WAYLAND_wl_egl_window_create(data->surface,
                                            window->w * data->scale_factor, window->h * data->scale_factor);

        /* Create the GLES window surface */
        data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) data->egl_window);
    
        if (data->egl_surface == EGL_NO_SURFACE) {
            return SDL_SetError("failed to create an EGL window surface");
        }
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

    wl_surface_commit(data->surface);
    WAYLAND_wl_display_flush(c->display);

    /* We may need to create an idle inhibitor for this new window */
    Wayland_SuspendScreenSaver(_this);

    return 0;
}


void
Wayland_HandlePendingResize(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (data->resize.pending) {
        struct wl_region *region;
        if (data->scale_factor != data->resize.scale_factor) {
            window->w = 0;
            window->h = 0;
        }
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, data->resize.width, data->resize.height);
        window->w = data->resize.width;
        window->h = data->resize.height;
        data->scale_factor = data->resize.scale_factor;
        wl_surface_set_buffer_scale(data->surface, data->scale_factor);
        if (data->egl_window) {
            WAYLAND_wl_egl_window_resize(data->egl_window, window->w * data->scale_factor, window->h * data->scale_factor, 0, 0);
        }

        if (data->resize.configure) {
           if (data->waylandData->shell.xdg) {
              xdg_surface_ack_configure(data->shell_surface.xdg.surface, data->resize.serial);
           } else if (data->waylandData->shell.zxdg) {
              zxdg_surface_v6_ack_configure(data->shell_surface.zxdg.surface, data->resize.serial);
           }
           data->resize.configure = SDL_FALSE;
        }

        region = wl_compositor_create_region(data->waylandData->compositor);
        wl_region_add(region, 0, 0, window->w, window->h);
        wl_surface_set_opaque_region(data->surface, region);
        wl_region_destroy(region);

        data->resize.pending = SDL_FALSE;
    }
}

void
Wayland_SetWindowMinimumSize(_THIS, SDL_Window * window)
{
    CommitMinMaxDimensions(window);
}

void
Wayland_SetWindowMaximumSize(_THIS, SDL_Window * window)
{
    CommitMinMaxDimensions(window);
}

void Wayland_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;
    struct wl_region *region;

    wl_surface_set_buffer_scale(wind->surface, get_window_scale_factor(window));

    if (wind->egl_window) {
        WAYLAND_wl_egl_window_resize(wind->egl_window, window->w * get_window_scale_factor(window), window->h * get_window_scale_factor(window), 0, 0);
    }

    region = wl_compositor_create_region(data->compositor);
    wl_region_add(region, 0, 0, window->w, window->h);
    wl_surface_set_opaque_region(wind->surface, region);
    wl_region_destroy(region);
}

void Wayland_SetWindowTitle(_THIS, SDL_Window * window)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *viddata = (SDL_VideoData *) _this->driverdata;

    if (window->title != NULL) {
        if (viddata->shell.xdg) {
            if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
                return; /* Can't do anything yet, wait for ShowWindow */
            }
            xdg_toplevel_set_title(wind->shell_surface.xdg.roleobj.toplevel, window->title);
        } else if (viddata->shell.zxdg) {
            if (wind->shell_surface.zxdg.roleobj.toplevel == NULL) {
                return; /* Can't do anything yet, wait for ShowWindow */
            }
            zxdg_toplevel_v6_set_title(wind->shell_surface.zxdg.roleobj.toplevel, window->title);
        } else {
            if (wind->shell_surface.wl == NULL) {
                return; /* Can'd do anything yet, wait for ShowWindow */
            }
            wl_shell_surface_set_title(wind->shell_surface.wl, window->title);
        }
    }

    WAYLAND_wl_display_flush( ((SDL_VideoData*)_this->driverdata)->display );
}

void
Wayland_SuspendScreenSaver(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;

#if SDL_USE_LIBDBUS
    if (SDL_DBus_ScreensaverInhibit(_this->suspend_screensaver)) {
        return;
    }
#endif

    /* The idle_inhibit_unstable_v1 protocol suspends the screensaver
       on a per wl_surface basis, but SDL assumes that suspending
       the screensaver can be done independently of any window.
       
       To reconcile these differences, we propagate the idle inhibit
       state to each window. If there is no window active, we will
       be able to inhibit idle once the first window is created.
    */
    if (data->idle_inhibit_manager) {
        SDL_Window *window = _this->windows;
        while (window) {
            SDL_WindowData *win_data = window->driverdata;

            if (_this->suspend_screensaver && !win_data->idle_inhibitor) {
                win_data->idle_inhibitor =
                    zwp_idle_inhibit_manager_v1_create_inhibitor(data->idle_inhibit_manager,
                                                                 win_data->surface);
            } else if (!_this->suspend_screensaver && win_data->idle_inhibitor) {
                zwp_idle_inhibitor_v1_destroy(win_data->idle_inhibitor);
                win_data->idle_inhibitor = NULL;
            }

            window = window->next;
        }
    }
}

void Wayland_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;

    if (data) {
        if (wind->egl_surface) {
            SDL_EGL_DestroySurface(_this, wind->egl_surface);
        }
        if (wind->egl_window) {
            WAYLAND_wl_egl_window_destroy(wind->egl_window);
        }

        if (wind->idle_inhibitor) {
            zwp_idle_inhibitor_v1_destroy(wind->idle_inhibitor);
        }

        if (wind->activation_token) {
            xdg_activation_token_v1_destroy(wind->activation_token);
        }

        SDL_free(wind->outputs);

        if (wind->frame_callback) {
            wl_callback_destroy(wind->frame_callback);
        }

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
        if (wind->extended_surface) {
            QtExtendedSurface_Unsubscribe(wind->extended_surface, SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION);
            QtExtendedSurface_Unsubscribe(wind->extended_surface, SDL_HINT_QTWAYLAND_WINDOW_FLAGS);
            qt_extended_surface_destroy(wind->extended_surface);
        }
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */
        wl_surface_destroy(wind->surface);

        SDL_free(wind);
        WAYLAND_wl_display_flush(data->display);
    }
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
