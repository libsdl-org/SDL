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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"
#include "../../core/unix/SDL_appid.h"
#include "../SDL_egl_c.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandvideo.h"
#include "../../SDL_hints_c.h"

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif


static SDL_bool FloatEqual(float a, float b)
{
    const float diff = SDL_fabsf(a - b);
    const float largest = SDL_max(SDL_fabsf(a), SDL_fabsf(b));

    return diff <= largest * SDL_FLT_EPSILON;
}

/* According to the Wayland spec:
 *
 * "If the [fullscreen] surface doesn't cover the whole output, the compositor will
 * position the surface in the center of the output and compensate with border fill
 * covering the rest of the output. The content of the border fill is undefined, but
 * should be assumed to be in some way that attempts to blend into the surrounding area
 * (e.g. solid black)."
 *
 * - KDE, as of 5.27, still doesn't do this
 * - GNOME prior to 43 didn't do this (older versions are still found in many LTS distros)
 *
 * Default to 'stretch' for now, until things have moved forward enough that the default
 * can be changed to 'aspect'.
 */
enum WaylandModeScale
{
    WAYLAND_MODE_SCALE_UNDEFINED,
    WAYLAND_MODE_SCALE_ASPECT,
    WAYLAND_MODE_SCALE_STRETCH,
    WAYLAND_MODE_SCALE_NONE
};

static enum WaylandModeScale GetModeScaleMethod()
{
    static enum WaylandModeScale scale_mode = WAYLAND_MODE_SCALE_UNDEFINED;

    if (scale_mode == WAYLAND_MODE_SCALE_UNDEFINED) {
        const char *scale_hint = SDL_GetHint(SDL_HINT_VIDEO_WAYLAND_MODE_SCALING);

        if (scale_hint) {
            if (!SDL_strcasecmp(scale_hint, "aspect")) {
                scale_mode = WAYLAND_MODE_SCALE_ASPECT;
            } else if (!SDL_strcasecmp(scale_hint, "none")) {
                scale_mode = WAYLAND_MODE_SCALE_NONE;
            } else {
                scale_mode = WAYLAND_MODE_SCALE_STRETCH;
            }
        } else {
            scale_mode = WAYLAND_MODE_SCALE_STRETCH;
        }
    }

    return scale_mode;
}

static SDL_bool SurfaceScaleIsFractional(SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    const float scale_value = !(window->fullscreen_exclusive) ? data->windowed_scale_factor : window->current_fullscreen_mode.pixel_density;
    return !FloatEqual(SDL_roundf(scale_value), scale_value);
}

static SDL_bool WindowNeedsViewport(SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *video = wind->waylandData;

    /*
     * A viewport is only required when scaling is enabled and:
     *  - The surface scale is fractional.
     *  - An exclusive fullscreen mode is being emulated and the mode does not match the requested output size.
     */
    if (video->viewporter) {
        if (SurfaceScaleIsFractional(window)) {
            return SDL_TRUE;
        } else if (window->fullscreen_exclusive) {
            if (window->current_fullscreen_mode.w != wind->requested_window_width || window->current_fullscreen_mode.h != wind->requested_window_height) {
                return SDL_TRUE;
            }
        }
    }

    return SDL_FALSE;
}

static void GetBufferSize(SDL_Window *window, int *width, int *height)
{
    SDL_WindowData *data = window->driverdata;
    int buf_width;
    int buf_height;

    /* Exclusive fullscreen modes always have a pixel density of 1 */
    if (data->is_fullscreen && window->fullscreen_exclusive) {
        buf_width = window->current_fullscreen_mode.w;
        buf_height = window->current_fullscreen_mode.h;
    } else {
        /* Round fractional backbuffer sizes halfway away from zero. */
        buf_width = (int)SDL_lroundf(data->requested_window_width * data->windowed_scale_factor);
        buf_height = (int)SDL_lroundf(data->requested_window_height * data->windowed_scale_factor);
    }

    if (width) {
        *width = buf_width;
    }
    if (height) {
        *height = buf_height;
    }
}

static void SetDrawSurfaceViewport(SDL_Window *window, int src_width, int src_height, int dst_width, int dst_height)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *video = wind->waylandData;

    if (video->viewporter) {
        if (!wind->draw_viewport) {
            wind->draw_viewport = wp_viewporter_get_viewport(video->viewporter, wind->surface);
        }

        wp_viewport_set_source(wind->draw_viewport, wl_fixed_from_int(0), wl_fixed_from_int(0), wl_fixed_from_int(src_width), wl_fixed_from_int(src_height));
        wp_viewport_set_destination(wind->draw_viewport, dst_width, dst_height);
    }
}

static void UnsetDrawSurfaceViewport(SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    if (wind->draw_viewport) {
        wp_viewport_destroy(wind->draw_viewport);
        wind->draw_viewport = NULL;
    }
}

static void SetMinMaxDimensions(SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;
    int min_width, min_height, max_width, max_height;

    if ((window->flags & SDL_WINDOW_FULLSCREEN) || wind->fullscreen_deadline_count) {
        min_width = 0;
        min_height = 0;
        max_width = 0;
        max_height = 0;
    } else if (window->flags & SDL_WINDOW_RESIZABLE) {
        min_width = SDL_max(window->min_w, wind->system_min_required_width);
        min_height = SDL_max(window->min_h, wind->system_min_required_height);
        max_width = window->max_w ? SDL_max(window->max_w, wind->system_min_required_width) : 0;
        max_height = window->max_h ? SDL_max(window->max_h, wind->system_min_required_height) : 0;
    } else {
        min_width = wind->wl_window_width;
        min_height = wind->wl_window_height;
        max_width = wind->wl_window_width;
        max_height = wind->wl_window_height;
    }

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.initial_configure_seen || !wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        /* No need to change these values if the window is non-resizable,
         * as libdecor will just overwrite them internally.
         */
        if (libdecor_frame_has_capability(wind->shell_surface.libdecor.frame, LIBDECOR_ACTION_RESIZE)) {
            libdecor_frame_set_min_content_size(wind->shell_surface.libdecor.frame,
                                                min_width,
                                                min_height);
            libdecor_frame_set_max_content_size(wind->shell_surface.libdecor.frame,
                                                max_width,
                                                max_height);
        }
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_min_size(wind->shell_surface.xdg.roleobj.toplevel,
                                  min_width,
                                  min_height);
        xdg_toplevel_set_max_size(wind->shell_surface.xdg.roleobj.toplevel,
                                  max_width,
                                  max_height);
    }
}

static void EnsurePopupPositionIsValid(SDL_Window *window, int *x, int *y)
{
    int adj_count = 0;

    /* Per the xdg-positioner spec, child popup windows must intersect or at
     * least be partially adjacent to the parent window.
     *
     * Failure to ensure this on a compositor that enforces this restriction
     * can result in behavior ranging from the window being spuriously closed
     * to a protocol violation.
     */
    if (*x + window->driverdata->wl_window_width < 0) {
        *x = -window->w;
        ++adj_count;
    }
    if (*y + window->driverdata->wl_window_height < 0) {
        *y = -window->h;
        ++adj_count;
    }
    if (*x > window->parent->driverdata->wl_window_width) {
        *x = window->parent->driverdata->wl_window_width;
        ++adj_count;
    }
    if (*y > window->parent->driverdata->wl_window_height) {
        *y = window->parent->driverdata->wl_window_height;
        ++adj_count;
    }

    /* If adjustment was required on the x and y axes, the popup is aligned with
     * the parent corner-to-corner and is neither overlapping nor adjacent, so it
     * must be nudged by 1 to be considered adjacent.
     */
    if (adj_count > 1) {
        *x += *x < 0 ? 1 : -1;
    }
}

static void GetPopupPosition(SDL_Window *popup, int x, int y, int *adj_x, int *adj_y)
{
    /* Adjust the popup positioning, if necessary */
#ifdef HAVE_LIBDECOR_H
    if (popup->parent->driverdata->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        libdecor_frame_translate_coordinate(popup->parent->driverdata->shell_surface.libdecor.frame,
                                            x, y, adj_x, adj_y);
    } else
#endif
    {
        *adj_x = x;
        *adj_y = y;
    }
}

static void RepositionPopup(SDL_Window *window, SDL_bool use_current_position)
{
    SDL_WindowData *wind = window->driverdata;

    if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP &&
        wind->shell_surface.xdg.roleobj.popup.positioner &&
        xdg_popup_get_version(wind->shell_surface.xdg.roleobj.popup.popup) >= XDG_POPUP_REPOSITION_SINCE_VERSION) {
        int orig_x = use_current_position ? window->x : window->floating.x;
        int orig_y = use_current_position ? window->y : window->floating.y;
        int x, y;

        EnsurePopupPositionIsValid(window, &orig_x, &orig_y);
        GetPopupPosition(window, orig_x, orig_y, &x, &y);
        xdg_positioner_set_anchor_rect(wind->shell_surface.xdg.roleobj.popup.positioner, 0, 0, window->parent->driverdata->wl_window_width, window->parent->driverdata->wl_window_height);
        xdg_positioner_set_size(wind->shell_surface.xdg.roleobj.popup.positioner, wind->wl_window_width, wind->wl_window_height);
        xdg_positioner_set_offset(wind->shell_surface.xdg.roleobj.popup.positioner, x, y);
        xdg_popup_reposition(wind->shell_surface.xdg.roleobj.popup.popup,
                             wind->shell_surface.xdg.roleobj.popup.positioner,
                             0);
    }
}

static void ConfigureWindowGeometry(SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    SDL_VideoData *viddata = data->waylandData;
    const int old_dw = data->drawable_width;
    const int old_dh = data->drawable_height;
    int window_width, window_height;
    SDL_bool window_size_changed;
    SDL_bool drawable_size_changed;

    /* Set the drawable backbuffer size. */
    GetBufferSize(window, &data->drawable_width, &data->drawable_height);
    drawable_size_changed = data->drawable_width != old_dw || data->drawable_height != old_dh;

    if (data->egl_window && drawable_size_changed) {
        WAYLAND_wl_egl_window_resize(data->egl_window,
                                     data->drawable_width,
                                     data->drawable_height,
                                     0, 0);
    }

    if (data->is_fullscreen && window->fullscreen_exclusive) {
        int output_width = data->requested_window_width;
        int output_height = data->requested_window_height;
        window_width = window->current_fullscreen_mode.w;
        window_height = window->current_fullscreen_mode.h;

        switch (GetModeScaleMethod()) {
        case WAYLAND_MODE_SCALE_NONE:
            /* The Wayland spec states that the advertised fullscreen dimensions are a maximum.
             * Windows can request a smaller size, but exceeding these dimensions is a protocol violation,
             * thus, modes that exceed the output size still need to be scaled with a viewport.
             */
            if (window_width <= output_width && window_height <= output_height) {
                output_width = window_width;
                output_height = window_height;

                break;
            }
            SDL_FALLTHROUGH;
        case WAYLAND_MODE_SCALE_ASPECT:
        {
            const float output_ratio = (float)output_width / (float)output_height;
            const float mode_ratio = (float)window_width / (float)window_height;

            if (output_ratio > mode_ratio) {
                output_width = SDL_lroundf((float)window_width * ((float)output_height / (float)window_height));
            } else if (output_ratio < mode_ratio) {
                output_height = SDL_lroundf((float)window_height * ((float)output_width / (float)window_width));
            }
        } break;
        default:
            break;
        }

        window_size_changed = window_width != window->w || window_height != window->h ||
            data->wl_window_width != output_width || data->wl_window_height != output_height;

        if (window_size_changed || drawable_size_changed) {
            if (WindowNeedsViewport(window)) {
                /* Set the buffer scale to 1 since a viewport will be used. */
                wl_surface_set_buffer_scale(data->surface, 1);
                SetDrawSurfaceViewport(window, data->drawable_width, data->drawable_height,
                                       output_width, output_height);

                data->wl_window_width = output_width;
                data->wl_window_height = output_height;
            } else {
                /* Calculate the integer scale from the mode and output. */
                const int32_t int_scale = SDL_max(window->current_fullscreen_mode.w / output_width, 1);

                UnsetDrawSurfaceViewport(window);
                wl_surface_set_buffer_scale(data->surface, int_scale);

                data->wl_window_width = window->current_fullscreen_mode.w;
                data->wl_window_height = window->current_fullscreen_mode.h;
            }

            data->pointer_scale_x = (float)window_width / (float)data->wl_window_width;
            data->pointer_scale_y = (float)window_height / (float)data->wl_window_height;
        }
    } else {
        window_width = data->requested_window_width;
        window_height = data->requested_window_height;

        window_size_changed = window_width != data->wl_window_width || window_height != data->wl_window_height;

        if (window_size_changed || drawable_size_changed) {
            if (WindowNeedsViewport(window)) {
                wl_surface_set_buffer_scale(data->surface, 1);
                SetDrawSurfaceViewport(window, data->drawable_width, data->drawable_height,
                                       window_width, window_height);
            } else if (window->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) {
                UnsetDrawSurfaceViewport(window);

                /* Don't change this if DPI awareness flag is unset, as an application may have set this manually. */
                wl_surface_set_buffer_scale(data->surface, (int32_t)data->windowed_scale_factor);
            }

            /* Clamp the physical window size to the system minimum required size. */
            data->wl_window_width = SDL_max(window_width, data->system_min_required_width);
            data->wl_window_height = SDL_max(window_height, data->system_min_required_height);

            data->pointer_scale_x = 1.0f;
            data->pointer_scale_y = 1.0f;
        }
    }

    /*
     * The surface geometry, opaque region and pointer confinement region only
     * need to be recalculated if the output size has changed.
     */
    if (window_size_changed) {
        struct wl_region *region;

        /* libdecor does this internally on frame commits, so it's only needed for xdg surfaces. */
        if (data->shell_surface_type != WAYLAND_SURFACE_LIBDECOR && data->shell_surface.xdg.surface) {
            xdg_surface_set_window_geometry(data->shell_surface.xdg.surface, 0, 0, data->wl_window_width, data->wl_window_height);
        }

        if (!(window->flags & SDL_WINDOW_TRANSPARENT)) {
            region = wl_compositor_create_region(viddata->compositor);
            wl_region_add(region, 0, 0,
                          data->wl_window_width, data->wl_window_height);
            wl_surface_set_opaque_region(data->surface, region);
            wl_region_destroy(region);
        }

        /* Ensure that child popup windows are still in bounds. */
        for (SDL_Window *child = window->first_child; child; child = child->next_sibling) {
            RepositionPopup(child, SDL_TRUE);
        }

        if (data->confined_pointer) {
            Wayland_input_confine_pointer(viddata->input, window);
        }
    }

    /* Update the min/max dimensions, primarily if the state was changed, and for non-resizable
     * xdg-toplevel windows where the limits should match the window size.
     */
    SetMinMaxDimensions(window);

    /* Unconditionally send the window and drawable size, the video core will deduplicate when required. */
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window_width, window_height);
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, data->drawable_width, data->drawable_height);

    /* Send an exposure event if the window is in the shown state and the size has changed,
     * even if the window is occluded, as the client needs to commit a new frame for the
     * changes to take effect.
     *
     * The occlusion state is immediately set again afterward, if necessary.
     */
    if (data->surface_status == WAYLAND_SURFACE_STATUS_SHOWN) {
        if ((drawable_size_changed || window_size_changed) ||
            (!data->suspended && (window->flags & SDL_WINDOW_OCCLUDED))) {
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
        }

        if (data->suspended) {
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_OCCLUDED, 0, 0);
        }
    }
}

static void CommitLibdecorFrame(SDL_Window *window)
{
#ifdef HAVE_LIBDECOR_H
    SDL_WindowData *wind = window->driverdata;

    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR && wind->shell_surface.libdecor.frame) {
        struct libdecor_state *state = libdecor_state_new(wind->wl_window_width, wind->wl_window_height);
        libdecor_frame_commit(wind->shell_surface.libdecor.frame, state, NULL);
        libdecor_state_free(state);
    }
#endif
}

static void fullscreen_deadline_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    /* Get the window from the ID as it may have been destroyed */
    SDL_WindowID windowID = (SDL_WindowID)((uintptr_t)data);
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window && window->driverdata) {
        window->driverdata->fullscreen_deadline_count--;
        SetMinMaxDimensions(window);
    }

    wl_callback_destroy(callback);
}

static struct wl_callback_listener fullscreen_deadline_listener = {
    fullscreen_deadline_handler
};

static void FlushFullscreenEvents(SDL_Window *window)
{
    while (window->driverdata->fullscreen_deadline_count) {
        WAYLAND_wl_display_roundtrip(window->driverdata->waylandData->display);
    }
}

static void SetFullscreen(SDL_Window *window, struct wl_output *output)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_VideoData *viddata = wind->waylandData;

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }

        ++wind->fullscreen_deadline_count;
        if (output) {
            Wayland_SetWindowResizable(SDL_GetVideoDevice(), window, SDL_TRUE);
            wl_surface_commit(wind->surface);

            libdecor_frame_set_fullscreen(wind->shell_surface.libdecor.frame, output);
        } else {
            libdecor_frame_unset_fullscreen(wind->shell_surface.libdecor.frame);
        }
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }

        ++wind->fullscreen_deadline_count;
        if (output) {
            Wayland_SetWindowResizable(SDL_GetVideoDevice(), window, SDL_TRUE);
            wl_surface_commit(wind->surface);

            xdg_toplevel_set_fullscreen(wind->shell_surface.xdg.roleobj.toplevel, output);
        } else {
            xdg_toplevel_unset_fullscreen(wind->shell_surface.xdg.roleobj.toplevel);
        }
    }

    /* Queue a deadline event */
    struct wl_callback *cb = wl_display_sync(viddata->display);
    wl_callback_add_listener(cb, &fullscreen_deadline_listener, (void *)((uintptr_t)window->id));
}

static void UpdateWindowFullscreen(SDL_Window *window, SDL_bool fullscreen)
{
    SDL_WindowData *wind = window->driverdata;

    wind->is_fullscreen = fullscreen;

    if (fullscreen) {
        if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
            SDL_copyp(&window->current_fullscreen_mode, &window->requested_fullscreen_mode);
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_ENTER_FULLSCREEN, 0, 0);
            SDL_UpdateFullscreenMode(window, SDL_TRUE, SDL_FALSE);

            /* Unconditionally set the output for exclusive fullscreen windows when entering
             * fullscreen from a compositor event, as where the compositor will actually
             * place the fullscreen window is unknown.
             */
            if (window->fullscreen_exclusive && !wind->fullscreen_was_positioned) {
                SDL_VideoDisplay *disp = SDL_GetVideoDisplay(window->current_fullscreen_mode.displayID);
                if (disp) {
                    wind->fullscreen_was_positioned = SDL_TRUE;
                    SetFullscreen(window, disp->driverdata->output);
                }
            }
        }
    } else {
        /* Don't change the fullscreen flags if the window is hidden or being hidden. */
        if ((window->flags & SDL_WINDOW_FULLSCREEN) && !window->is_hiding && !(window->flags & SDL_WINDOW_HIDDEN)) {
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN, 0, 0);
            SDL_UpdateFullscreenMode(window, SDL_FALSE, SDL_FALSE);
            wind->fullscreen_was_positioned = SDL_FALSE;
        }
    }
}

static const struct wl_callback_listener surface_frame_listener;

static void surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;

    /*
     * wl_surface.damage_buffer is the preferred method of setting the damage region
     * on compositor version 4 and above.
     */
    if (wl_compositor_get_version(wind->waylandData->compositor) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(wind->surface, 0, 0,
                                 wind->drawable_width, wind->drawable_height);
    } else {
        wl_surface_damage(wind->surface, 0, 0,
                          wind->wl_window_width, wind->wl_window_height);
    }

    if (wind->surface_status == WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME) {
        wind->surface_status = WAYLAND_SURFACE_STATUS_SHOWN;

        /* If any child windows are waiting on this window to be shown, show them now */
        for (SDL_Window *w = wind->sdlwindow->first_child; w; w = w->next_sibling) {
            if (w->driverdata->surface_status == WAYLAND_SURFACE_STATUS_SHOW_PENDING) {
                Wayland_ShowWindow(SDL_GetVideoDevice(), w);
            }
        }

        /* If the window was initially set to the suspended state, send the occluded event now,
         * as we don't want to mark the window as occluded until at least one frame has been submitted.
         */
        if (wind->suspended) {
            SDL_SendWindowEvent(wind->sdlwindow, SDL_EVENT_WINDOW_OCCLUDED, 0, 0);
        }
    }

    wl_callback_destroy(cb);
    wind->surface_frame_callback = wl_surface_frame(wind->surface);
    wl_callback_add_listener(wind->surface_frame_callback, &surface_frame_listener, data);
}

static const struct wl_callback_listener surface_frame_listener = {
    surface_frame_done
};

static const struct wl_callback_listener gles_swap_frame_listener;

static void gles_swap_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_AtomicSet(&wind->swap_interval_ready, 1); /* mark window as ready to present again. */

    /* reset this callback to fire again once a new frame was presented and compositor wants the next one. */
    wind->gles_swap_frame_callback = wl_surface_frame(wind->gles_swap_frame_surface_wrapper);
    wl_callback_destroy(cb);
    wl_callback_add_listener(wind->gles_swap_frame_callback, &gles_swap_frame_listener, data);
}

static const struct wl_callback_listener gles_swap_frame_listener = {
    gles_swap_frame_done
};

static void handle_configure_xdg_shell_surface(void *data, struct xdg_surface *xdg, uint32_t serial)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_Window *window = wind->sdlwindow;

    ConfigureWindowGeometry(window);
    xdg_surface_ack_configure(xdg, serial);

    wind->shell_surface.xdg.initial_configure_seen = SDL_TRUE;
}

static const struct xdg_surface_listener shell_surface_listener_xdg = {
    handle_configure_xdg_shell_surface
};

static void handle_configure_xdg_toplevel(void *data,
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
    SDL_bool floating = SDL_TRUE;
    SDL_bool tiled = SDL_FALSE;
    SDL_bool active = SDL_FALSE;
    SDL_bool suspended = SDL_FALSE;
    wl_array_for_each (state, states) {
        switch (*state) {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            fullscreen = SDL_TRUE;
            floating = SDL_FALSE;
            break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            maximized = SDL_TRUE;
            floating = SDL_FALSE;
            break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
            active = SDL_TRUE;
            break;
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
        case XDG_TOPLEVEL_STATE_TILED_TOP:
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            tiled = SDL_TRUE;
            floating = SDL_FALSE;
            break;
        case XDG_TOPLEVEL_STATE_SUSPENDED:
            suspended = SDL_TRUE;
            break;
        default:
            break;
        }
    }

    UpdateWindowFullscreen(window, fullscreen);

    /* Always send a maximized/restore event; if the event is redundant it will
     * automatically be discarded (see src/events/SDL_windowevents.c)
     *
     * No, we do not get minimize events from xdg-shell, however, the minimized
     * state can be programmatically set. The meaning of 'minimized' is compositor
     * dependent, but in general, we can assume that the flag should remain set until
     * the next focused configure event occurs.
     */
    if (active || !(window->flags & SDL_WINDOW_MINIMIZED)) {
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            /* If we were minimized, send a restored event before possibly sending maximized. */
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
        }
        SDL_SendWindowEvent(window,
                            (maximized && !fullscreen) ? SDL_EVENT_WINDOW_MAXIMIZED : SDL_EVENT_WINDOW_RESTORED,
                            0, 0);
    }

    if (!fullscreen) {
        /* xdg_toplevel spec states that this is a suggestion.
         * Ignore if less than or greater than max/min size.
         */
        if (window->flags & SDL_WINDOW_RESIZABLE) {
            if ((floating && !wind->floating) ||
                width == 0 || height == 0) {
                /* This happens when we're being restored from a
                 * non-floating state, so use the cached floating size here.
                 */
                width = window->floating.w;
                height = window->floating.h;
            }
        } else {
            /* If we're a fixed-size window, we know our size for sure.
             * Always assume the configure is wrong.
             */
            if (floating) {
                width = window->floating.w;
                height = window->floating.h;
            } else {
                width = window->windowed.w;
                height = window->windowed.h;
            }
        }

        /* The content limits are only a hint, which the compositor is free to ignore,
         * so apply them manually when appropriate.
         *
         * Per the spec, maximized windows must have their exact dimensions respected,
         * thus they must not be resized, or a protocol violation can occur.
         */
        if (!maximized) {
            if (window->max_w > 0) {
                width = SDL_min(width, window->max_w);
            }
            width = SDL_max(width, window->min_w);

            if (window->max_h > 0) {
                height = SDL_min(height, window->max_h);
            }
            height = SDL_max(height, window->min_h);
        }
    } else {
        if (width == 0 || height == 0) {
            width = wind->requested_window_width;
            height = wind->requested_window_height;
        }
    }

    /* Don't update the dimensions if they haven't changed, or they could overwrite
     * a new size set programmatically with old dimensions.
     */
    if (width != wind->last_configure_width || height != wind->last_configure_height) {
        wind->requested_window_width = width;
        wind->requested_window_height = height;
    }

    wind->last_configure_width = width;
    wind->last_configure_height = height;
    wind->floating = floating;
    wind->suspended = suspended;
    wind->active = active;
    window->state_not_floating = tiled;

    if (wind->surface_status == WAYLAND_SURFACE_STATUS_WAITING_FOR_CONFIGURE) {
        wind->surface_status = WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME;
    }
}

static void handle_close_xdg_toplevel(void *data, struct xdg_toplevel *xdg_toplevel)
{
    SDL_WindowData *window = (SDL_WindowData *)data;
    SDL_SendWindowEvent(window->sdlwindow, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
}

static void handle_xdg_configure_toplevel_bounds(void *data,
                                                 struct xdg_toplevel *xdg_toplevel,
                                                 int32_t width, int32_t height)
{
    /* NOP */
}

void handle_xdg_toplevel_wm_capabilities(void *data,
                                         struct xdg_toplevel *xdg_toplevel,
                                         struct wl_array *capabilities)
{
    /* NOP */
}

static const struct xdg_toplevel_listener toplevel_listener_xdg = {
    handle_configure_xdg_toplevel,
    handle_close_xdg_toplevel,
    handle_xdg_configure_toplevel_bounds, /* Version 4 */
    handle_xdg_toplevel_wm_capabilities   /* Version 5 */
};

static void handle_configure_xdg_popup(void *data,
                                       struct xdg_popup *xdg_popup,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    int offset_x, offset_y;

    /* Adjust the position if it was offset for libdecor */
    GetPopupPosition(wind->sdlwindow, 0, 0, &offset_x, &offset_y);
    x -= offset_x;
    y -= offset_y;

    wind->requested_window_width = width;
    wind->requested_window_height = height;

    SDL_SendWindowEvent(wind->sdlwindow, SDL_EVENT_WINDOW_MOVED, x, y);

    if (wind->surface_status == WAYLAND_SURFACE_STATUS_WAITING_FOR_CONFIGURE) {
        wind->surface_status = WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME;
    }
}

static void handle_done_xdg_popup(void *data, struct xdg_popup *xdg_popup)
{
    SDL_WindowData *window = (SDL_WindowData *)data;
    SDL_SendWindowEvent(window->sdlwindow, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
}

static void handle_repositioned_xdg_popup(void *data,
                                          struct xdg_popup *xdg_popup,
                                          uint32_t token)
{
    /* No-op, configure does all the work we care about */
}

static const struct xdg_popup_listener popup_listener_xdg = {
    handle_configure_xdg_popup,
    handle_done_xdg_popup,
    handle_repositioned_xdg_popup
};

static void handle_configure_zxdg_decoration(void *data,
                                             struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1,
                                             uint32_t mode)
{
    SDL_Window *window = (SDL_Window *)data;
    SDL_WindowData *driverdata = window->driverdata;
    SDL_VideoDevice *device = SDL_GetVideoDevice();

    /* If the compositor tries to force CSD anyway, bail on direct XDG support
     * and fall back to libdecor, it will handle these events from then on.
     *
     * To do this we have to fully unmap, then map with libdecor loaded.
     */
    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
        if (window->flags & SDL_WINDOW_BORDERLESS) {
            /* borderless windows do request CSD, so we got what we wanted */
            return;
        }
        if (!Wayland_LoadLibdecor(driverdata->waylandData, SDL_TRUE)) {
            /* libdecor isn't available, so no borders for you... oh well */
            return;
        }
        WAYLAND_wl_display_roundtrip(driverdata->waylandData->display);

        Wayland_HideWindow(device, window);
        SDL_zero(driverdata->shell_surface);
        driverdata->shell_surface_type = WAYLAND_SURFACE_LIBDECOR;

        Wayland_ShowWindow(device, window);
    }
}

static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
    handle_configure_zxdg_decoration
};

#ifdef HAVE_LIBDECOR_H
/*
 * XXX: Hack for older versions of libdecor that lack the function to query the
 *      minimum content size limit. The internal limits must always be overridden
 *      to ensure that very small windows don't cause errors or crashes.
 *
 *      On libdecor >= 0.1.2, which exposes the function to get the minimum content
 *      size limit, this function is a no-op.
 *
 *      Can be removed if the minimum required version of libdecor is raised to
 *      0.1.2 or higher.
 */
static void OverrideLibdecorLimits(SDL_Window *window)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR
    if (!libdecor_frame_get_min_content_size) {
        libdecor_frame_set_min_content_size(window->driverdata->shell_surface.libdecor.frame, window->min_w, window->min_h);
    }
#elif !defined(SDL_HAVE_LIBDECOR_VER_0_2_0)
    libdecor_frame_set_min_content_size(window->driverdata->shell_surface.libdecor.frame, window->min_w, window->min_h);
#endif
}

/*
 * NOTE: Retrieves the minimum content size limits, if the function for doing so is available.
 *       On versions of libdecor that lack the minimum content size retrieval function, this
 *       function is a no-op.
 *
 *       Can be replaced with a direct call if the minimum required version of libdecor is raised
 *       to 0.1.2 or higher.
 */
static void LibdecorGetMinContentSize(struct libdecor_frame *frame, int *min_w, int *min_h)
{
#ifdef SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR
    if (libdecor_frame_get_min_content_size != NULL) {
        libdecor_frame_get_min_content_size(frame, min_w, min_h);
    }
#elif defined(SDL_HAVE_LIBDECOR_VER_0_2_0)
    libdecor_frame_get_min_content_size(frame, min_w, min_h);
#endif
}

static void decoration_frame_configure(struct libdecor_frame *frame,
                                       struct libdecor_configuration *configuration,
                                       void *user_data)
{
    SDL_WindowData *wind = (SDL_WindowData *)user_data;
    SDL_Window *window = wind->sdlwindow;
    struct libdecor_state *state;

    enum libdecor_window_state window_state;
    int width, height;

    SDL_bool prev_fullscreen = wind->is_fullscreen;
    SDL_bool active = SDL_FALSE;
    SDL_bool fullscreen = SDL_FALSE;
    SDL_bool maximized = SDL_FALSE;
    SDL_bool tiled = SDL_FALSE;
    SDL_bool suspended = SDL_FALSE;
    SDL_bool floating;

    static const enum libdecor_window_state tiled_states = (LIBDECOR_WINDOW_STATE_TILED_LEFT | LIBDECOR_WINDOW_STATE_TILED_RIGHT |
                                                            LIBDECOR_WINDOW_STATE_TILED_TOP | LIBDECOR_WINDOW_STATE_TILED_BOTTOM);

    /* Window State */
    if (libdecor_configuration_get_window_state(configuration, &window_state)) {
        fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
        maximized = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
        active = (window_state & LIBDECOR_WINDOW_STATE_ACTIVE) != 0;
        tiled = (window_state & tiled_states) != 0;
#ifdef SDL_HAVE_LIBDECOR_VER_0_2_0
        suspended = (window_state & LIBDECOR_WINDOW_STATE_SUSPENDED) != 0;
#endif
    }
    floating = !(fullscreen || maximized || tiled);

    UpdateWindowFullscreen(window, fullscreen);

    /* Always send a maximized/restore event; if the event is redundant it will
     * automatically be discarded (see src/events/SDL_windowevents.c)
     *
     * No, we do not get minimize events from libdecor, however, the minimized
     * state can be programmatically set. The meaning of 'minimized' is compositor
     * dependent, but in general, we can assume that the flag should remain set until
     * the next focused configure event occurs.
     */
    if (active || !(window->flags & SDL_WINDOW_MINIMIZED)) {
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            /* If we were minimized, send a restored event before possibly sending maximized. */
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
        }
        SDL_SendWindowEvent(window,
                            (maximized && !fullscreen) ? SDL_EVENT_WINDOW_MAXIMIZED : SDL_EVENT_WINDOW_RESTORED,
                            0, 0);
    }

    /* For fullscreen or fixed-size windows we know our size.
     * Always assume the configure is wrong.
     */
    if (fullscreen) {
        /* FIXME: We have been explicitly told to respect the fullscreen size
         * parameters here, even though they are known to be wrong on GNOME at
         * bare minimum. If this is wrong, don't blame us, we were explicitly
         * told to do this.
         */
        if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
            width = wind->requested_window_width;
            height = wind->requested_window_height;
        }
    } else {
        if (!(window->flags & SDL_WINDOW_RESIZABLE)) {
            if (floating) {
                width = window->floating.w;
                height = window->floating.h;
            } else {
                width = window->windowed.w;
                height = window->windowed.h;
            }

            OverrideLibdecorLimits(window);
        } else {
            /*
             * XXX: libdecor can send bogus content sizes that are +/- the height
             *      of the title bar when hiding a window or transitioning from
             *      non-floating to floating state, which distorts the window size.
             *
             *      Ignore any size values from libdecor in these scenarios in
             *      favor of the cached window size.
             *
             *      https://gitlab.gnome.org/jadahl/libdecor/-/issues/40
             */
            const SDL_bool use_cached_size = !maximized && !tiled &&
                                             ((floating && !wind->floating) ||
                                              (window->is_hiding || (window->flags & SDL_WINDOW_HIDDEN)));

            /* This will never set 0 for width/height unless the function returns false */
            if (use_cached_size || !libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
                if (floating) {
                    /* This usually happens when we're being restored from a
                     * non-floating state, so use the cached floating size here.
                     */
                    width = window->floating.w;
                    height = window->floating.h;
                } else {
                    width = window->windowed.w;
                    height = window->windowed.h;
                }
            }
        }

        /* The content limits are only a hint, which the compositor is free to ignore,
         * so apply them manually when appropriate.
         *
         * Per the spec, maximized windows must have their exact dimensions respected,
         * thus they must not be resized, or a protocol violation can occur.
         */
        if (!maximized) {
            if (window->max_w > 0) {
                width = SDL_min(width, window->max_w);
            }
            width = SDL_max(width, window->min_w);

            if (window->max_h > 0) {
                height = SDL_min(height, window->max_h);
            }
            height = SDL_max(height, window->min_h);
        }
    }

    /* Don't update the dimensions if they haven't changed, or they could overwrite
     * a new size set programmatically with old dimensions.
     */
    if (width != wind->last_configure_width || height != wind->last_configure_height) {
        wind->requested_window_width = width;
        wind->requested_window_height = height;
    }

    /* Store the new state. */
    wind->last_configure_width = width;
    wind->last_configure_height = height;
    wind->floating = floating;
    wind->suspended = suspended;
    wind->active = active;
    window->state_not_floating = tiled;

    /* Calculate the new window geometry */
    ConfigureWindowGeometry(window);

    /* ... then commit the changes on the libdecor side. */
    state = libdecor_state_new(wind->wl_window_width, wind->wl_window_height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    if (!wind->shell_surface.libdecor.initial_configure_seen) {
        LibdecorGetMinContentSize(frame, &wind->system_min_required_width, &wind->system_min_required_height);
        wind->shell_surface.libdecor.initial_configure_seen = SDL_TRUE;
    }
    if (wind->surface_status == WAYLAND_SURFACE_STATUS_WAITING_FOR_CONFIGURE) {
        wind->surface_status = WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME;
    }

    /* Update the resize capability if this config event was the result of the
     * compositor taking a window out of fullscreen. Since this will change the
     * capabilities and commit a new frame state with the last known content
     * dimension, this has to be called after the new state has been committed
     * and the new content dimensions were updated.
     */
    if (prev_fullscreen && !wind->is_fullscreen) {
        Wayland_SetWindowResizable(SDL_GetVideoDevice(), window,
                                   !!(window->flags & SDL_WINDOW_RESIZABLE));
    }
}

static void decoration_frame_close(struct libdecor_frame *frame, void *user_data)
{
    SDL_SendWindowEvent(((SDL_WindowData *)user_data)->sdlwindow, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
}

static void decoration_frame_commit(struct libdecor_frame *frame, void *user_data)
{
    /* libdecor decoration subsurfaces are synchronous, so the client needs to
     * commit a frame to trigger an update of the decoration surfaces.
     */
    SDL_WindowData *wind = (SDL_WindowData *)user_data;
    if (!wind->suspended && wind->surface_status == WAYLAND_SURFACE_STATUS_SHOWN) {
        SDL_SendWindowEvent(wind->sdlwindow, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
    }
}

static struct libdecor_frame_interface libdecor_frame_interface = {
    decoration_frame_configure,
    decoration_frame_close,
    decoration_frame_commit,
};
#endif

static void Wayland_HandlePreferredScaleChanged(SDL_WindowData *window_data, float factor)
{
    const float old_factor = window_data->windowed_scale_factor;

    if (!(window_data->sdlwindow->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)) {
        /* Scale will always be 1, just ignore this */
        return;
    }

    if (!FloatEqual(factor, old_factor)) {
        window_data->windowed_scale_factor = factor;
        ConfigureWindowGeometry(window_data->sdlwindow);
    }
}

static void Wayland_MaybeUpdateScaleFactor(SDL_WindowData *window)
{
    float factor;
    int i;

    /* If the fractional scale protocol is present or the core protocol supports the
     * preferred buffer scale event, the compositor will tell explicitly the application
     * what scale it wants via these events, so don't try to determine the scale factor
     * from which displays the surface has entered.
     */
    if (window->fractional_scale || wl_surface_get_version(window->surface) >= WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION) {
        return;
    }

    if (window->num_outputs != 0) {
        /* Check every display's factor, use the highest */
        factor = 0.0f;
        for (i = 0; i < window->num_outputs; i++) {
            SDL_DisplayData *driverdata = window->outputs[i];
            factor = SDL_max(factor, driverdata->scale_factor);
        }
    } else {
        /* No monitor (somehow)? Just fall back. */
        factor = window->windowed_scale_factor;
    }

    Wayland_HandlePreferredScaleChanged(window, factor);
}

/* While we can't get window position from the compositor, we do at least know
 * what monitor we're on, so let's send move events that put the window at the
 * center of the whatever display the wl_surface_listener events give us.
 */
static void Wayland_move_window(SDL_Window *window, SDL_DisplayData *driverdata)
{
    SDL_WindowData *wind = window->driverdata;
    SDL_DisplayID *displays;
    int i;

    displays = SDL_GetDisplays(NULL);
    if (displays) {
        for (i = 0; displays[i]; ++i) {
            if (SDL_GetDisplayDriverData(displays[i]) == driverdata) {
                /* We want to send a very very specific combination here:
                 *
                 * 1. A coordinate that tells the application what display we're on
                 * 2. Exactly (0, 0)
                 *
                 * Part 1 is useful information but is also really important for
                 * ensuring we end up on the right display for fullscreen, while
                 * part 2 is important because numerous applications use a specific
                 * combination of GetWindowPosition and GetGlobalMouseState, and of
                 * course neither are supported by Wayland. Since global mouse will
                 * fall back to just GetMouseState, we need the window position to
                 * be zero so the cursor math works without it going off in some
                 * random direction. See UE5 Editor for a notable example of this!
                 *
                 * This may be an issue some day if we're ever able to implement
                 * SDL_GetDisplayUsableBounds!
                 *
                 * -flibit
                 */
                SDL_Rect bounds;

                wind->last_displayID = displays[i];
                if (wind->shell_surface_type != WAYLAND_SURFACE_XDG_POPUP) {
                    /* Need to catch up on fullscreen state here, as the video core may try to update
                     * the fullscreen window, which on Wayland involves a set fullscreen call, which
                     * can overwrite older pending state.
                     */
                    FlushFullscreenEvents(window);

                    SDL_GetDisplayBounds(wind->last_displayID, &bounds);
                    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, bounds.x, bounds.y);
                }
                break;
            }
        }
        SDL_free(displays);
    }
}

static void handle_surface_enter(void *data, struct wl_surface *surface,
                                 struct wl_output *output)
{
    SDL_WindowData *window = data;
    SDL_DisplayData *driverdata = wl_output_get_user_data(output);
    SDL_DisplayData **new_outputs;

    if (!SDL_WAYLAND_own_output(output) || !SDL_WAYLAND_own_surface(surface)) {
        return;
    }

    new_outputs = SDL_realloc(window->outputs,
                              sizeof(SDL_DisplayData *) * (window->num_outputs + 1));
    if (!new_outputs) {
        return;
    }
    window->outputs = new_outputs;
    window->outputs[window->num_outputs++] = driverdata;

    /* Update the scale factor after the move so that fullscreen outputs are updated. */
    Wayland_move_window(window->sdlwindow, driverdata);
    Wayland_MaybeUpdateScaleFactor(window);
}

static void handle_surface_leave(void *data, struct wl_surface *surface,
                                 struct wl_output *output)
{
    SDL_WindowData *window = data;
    int i, send_move_event = 0;
    SDL_DisplayData *driverdata = wl_output_get_user_data(output);

    if (!SDL_WAYLAND_own_output(output) || !SDL_WAYLAND_own_surface(surface)) {
        return;
    }

    for (i = 0; i < window->num_outputs; i++) {
        if (window->outputs[i] == driverdata) { /* remove this one */
            if (i == (window->num_outputs - 1)) {
                window->outputs[i] = NULL;
                send_move_event = 1;
            } else {
                SDL_memmove(&window->outputs[i],
                            &window->outputs[i + 1],
                            sizeof(SDL_DisplayData *) * ((window->num_outputs - i) - 1));
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
                            window->outputs[window->num_outputs - 1]);
    }

    Wayland_MaybeUpdateScaleFactor(window);
}

static void handle_preferred_buffer_scale(void *data, struct wl_surface *wl_surface, int32_t factor)
{
    SDL_WindowData *wind = data;

    /* The spec is unclear on how this interacts with the fractional scaling protocol,
     * so, for now, assume that the fractional scaling protocol takes priority and
     * only listen to this event if the fractional scaling protocol is not present.
     */
    if (!wind->fractional_scale) {
        Wayland_HandlePreferredScaleChanged(data, (float)factor);
    }
}

static void handle_preferred_buffer_transform(void *data, struct wl_surface *wl_surface, uint32_t transform)
{
    /* Nothing to do here. */
}

static const struct wl_surface_listener surface_listener = {
    handle_surface_enter,
    handle_surface_leave,
    handle_preferred_buffer_scale,
    handle_preferred_buffer_transform
};

static void handle_preferred_fractional_scale(void *data, struct wp_fractional_scale_v1 *wp_fractional_scale_v1, uint32_t scale)
{
    const float factor = scale / 120.; /* 120 is a magic number defined in the spec as a common denominator */
    Wayland_HandlePreferredScaleChanged(data, factor);
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    handle_preferred_fractional_scale
};

static void SetKeyboardFocus(SDL_Window *window)
{
    SDL_Window *kb_focus = SDL_GetKeyboardFocus();
    SDL_Window *topmost = window;

    /* Find the topmost parent */
    while (topmost->parent) {
        topmost = topmost->parent;
    }

    topmost->driverdata->keyboard_focus = window;

    /* Clear the mouse capture flags before changing keyboard focus */
    if (kb_focus) {
        kb_focus->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;
    }
    window->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;
    SDL_SetKeyboardFocus(window);
}

int Wayland_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0; /* just succeed, the real work is done elsewhere. */
}

int Wayland_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window)
{
    SDL_VideoData *viddata = _this->driverdata;
    SDL_WindowData *modal_data = modal_window->driverdata;
    SDL_WindowData *parent_data = parent_window->driverdata;

    if (modal_data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP || parent_data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
        return SDL_SetError("Modal/Parent was a popup, not a toplevel");
    }

#ifdef HAVE_LIBDECOR_H
    if (viddata->shell.libdecor) {
        if (!modal_data->shell_surface.libdecor.frame) {
            return SDL_SetError("Modal window was hidden");
        }
        if (!parent_data->shell_surface.libdecor.frame) {
            return SDL_SetError("Parent window was hidden");
        }
        libdecor_frame_set_parent(modal_data->shell_surface.libdecor.frame,
                                  parent_data->shell_surface.libdecor.frame);
    } else
#endif
        if (viddata->shell.xdg) {
        if (modal_data->shell_surface.xdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Modal window was hidden");
        }
        if (parent_data->shell_surface.xdg.roleobj.toplevel == NULL) {
            return SDL_SetError("Parent window was hidden");
        }
        xdg_toplevel_set_parent(modal_data->shell_surface.xdg.roleobj.toplevel,
                                parent_data->shell_surface.xdg.roleobj.toplevel);
    } else {
        return SDL_Unsupported();
    }

    return 0;
}

static void show_hide_sync_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    /* Get the window from the ID as it may have been destroyed */
    SDL_WindowID windowID = (SDL_WindowID)((uintptr_t)data);
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window && window->driverdata) {
        SDL_WindowData *wind = window->driverdata;
        wind->show_hide_sync_required = SDL_FALSE;
    }

    wl_callback_destroy(callback);
}

static struct wl_callback_listener show_hide_sync_listener = {
    show_hide_sync_handler
};

void Wayland_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *c = _this->driverdata;
    SDL_WindowData *data = window->driverdata;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

    /* Custom surfaces don't get toplevels and are always considered 'shown'; nothing to do here. */
    if (data->shell_surface_type == WAYLAND_SURFACE_CUSTOM) {
        return;
    }

    /* If this is a child window, the parent *must* be in the final shown state,
     * meaning that it has received a configure event, followed by a frame callback.
     * If not, a race condition can result, with effects ranging from the child
     * window to spuriously closing to protocol errors.
     *
     * If waiting on the parent window, set the pending status and the window will
     * be shown when the parent is in the shown state.
     */
    if (window->parent) {
        if (window->parent->driverdata->surface_status != WAYLAND_SURFACE_STATUS_SHOWN) {
            data->surface_status = WAYLAND_SURFACE_STATUS_SHOW_PENDING;
            return;
        }
    }

    /* The window was hidden, but the sync point hasn't yet been reached.
     * Pump events to avoid a possible protocol violation.
     */
    if (data->show_hide_sync_required) {
        WAYLAND_wl_display_roundtrip(c->display);
    }

    data->surface_status = WAYLAND_SURFACE_STATUS_WAITING_FOR_CONFIGURE;

    /* Detach any previous buffers before resetting everything, otherwise when
     * calling this a second time you'll get an annoying protocol error!
     *
     * FIXME: This was originally moved to HideWindow, which _should_ make
     * sense, but for whatever reason UE5's popups require that this actually
     * be in both places at once? Possibly from renderers making commits? I can't
     * fully remember if this location caused crashes or if I was fixing a pair
     * of Hide/Show calls. In any case, UE gives us a pretty good test and having
     * both detach calls passes. This bug may be relevant if I'm wrong:
     *
     * https://bugs.kde.org/show_bug.cgi?id=448856
     *
     * -flibit
     */
    wl_surface_attach(data->surface, NULL, 0, 0);
    wl_surface_commit(data->surface);

    /* Create the shell surface and map the toplevel/popup */
#ifdef HAVE_LIBDECOR_H
    if (data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        data->shell_surface.libdecor.frame = libdecor_decorate(c->shell.libdecor,
                                                               data->surface,
                                                               &libdecor_frame_interface,
                                                               data);
        if (!data->shell_surface.libdecor.frame) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create libdecor frame!");
        } else {
            libdecor_frame_set_app_id(data->shell_surface.libdecor.frame, data->app_id);
            libdecor_frame_map(data->shell_surface.libdecor.frame);

            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_SURFACE_POINTER, libdecor_frame_get_xdg_surface(data->shell_surface.libdecor.frame));
            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, libdecor_frame_get_xdg_toplevel(data->shell_surface.libdecor.frame));
        }
    } else
#endif
    if (data->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL || data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
        data->shell_surface.xdg.surface = xdg_wm_base_get_xdg_surface(c->shell.xdg, data->surface);
        xdg_surface_set_user_data(data->shell_surface.xdg.surface, data);
        xdg_surface_add_listener(data->shell_surface.xdg.surface, &shell_surface_listener_xdg, data);
        SDL_SetProperty(SDL_GetWindowProperties(window), SDL_PROPERTY_WINDOW_WAYLAND_XDG_SURFACE_POINTER, data->shell_surface.xdg.surface);

        if (data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
            SDL_Window *parent = window->parent;
            SDL_WindowData *parent_data = parent->driverdata;
            struct xdg_surface *parent_xdg_surface = NULL;
            int position_x = 0, position_y = 0;

            /* Configure the popup parameters */
#ifdef HAVE_LIBDECOR_H
            if (parent_data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
                parent_xdg_surface = libdecor_frame_get_xdg_surface(parent_data->shell_surface.libdecor.frame);
            } else
#endif
            if (parent_data->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL ||
                    parent_data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
                parent_xdg_surface = parent_data->shell_surface.xdg.surface;
            }

            /* Set up the positioner for the popup and configure the constraints */
            data->shell_surface.xdg.roleobj.popup.positioner = xdg_wm_base_create_positioner(c->shell.xdg);
            xdg_positioner_set_anchor(data->shell_surface.xdg.roleobj.popup.positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
            xdg_positioner_set_anchor_rect(data->shell_surface.xdg.roleobj.popup.positioner, 0, 0, parent->w, parent->h);
            xdg_positioner_set_constraint_adjustment(data->shell_surface.xdg.roleobj.popup.positioner,
                                                     XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y);
            xdg_positioner_set_gravity(data->shell_surface.xdg.roleobj.popup.positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
            xdg_positioner_set_size(data->shell_surface.xdg.roleobj.popup.positioner, window->w, window->h);

            /* Set the popup initial position */
            EnsurePopupPositionIsValid(window, &window->x, &window->y);
            GetPopupPosition(window, window->x, window->y, &position_x, &position_y);
            xdg_positioner_set_offset(data->shell_surface.xdg.roleobj.popup.positioner, position_x, position_y);

            /* Assign the popup role */
            data->shell_surface.xdg.roleobj.popup.popup = xdg_surface_get_popup(data->shell_surface.xdg.surface,
                                                                                parent_xdg_surface,
                                                                                data->shell_surface.xdg.roleobj.popup.positioner);
            xdg_popup_add_listener(data->shell_surface.xdg.roleobj.popup.popup, &popup_listener_xdg, data);

            if (window->flags & SDL_WINDOW_TOOLTIP) {
                struct wl_region *region;

                /* Tooltips can't be interacted with, so turn off the input region to avoid blocking anything behind them */
                region = wl_compositor_create_region(c->compositor);
                wl_region_add(region, 0, 0, 0, 0);
                wl_surface_set_input_region(data->surface, region);
                wl_region_destroy(region);
            } else if (window->flags & SDL_WINDOW_POPUP_MENU) {
                if (window->parent == SDL_GetKeyboardFocus()) {
                    SetKeyboardFocus(window);
                }
            }

            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_POPUP_POINTER, data->shell_surface.xdg.roleobj.popup.popup);
            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_POSITIONER_POINTER, data->shell_surface.xdg.roleobj.popup.positioner);
        } else {
            data->shell_surface.xdg.roleobj.toplevel = xdg_surface_get_toplevel(data->shell_surface.xdg.surface);
            xdg_toplevel_set_app_id(data->shell_surface.xdg.roleobj.toplevel, data->app_id);
            xdg_toplevel_add_listener(data->shell_surface.xdg.roleobj.toplevel, &toplevel_listener_xdg, data);
            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, data->shell_surface.xdg.roleobj.toplevel);
        }
    }

    /* Restore state that was set prior to this call */
    Wayland_SetWindowTitle(_this, window);

    /* We have to wait until the surface gets a "configure" event, or use of
     * this surface will fail. This is a new rule for xdg_shell.
     */
#ifdef HAVE_LIBDECOR_H
    if (data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (data->shell_surface.libdecor.frame) {
            while (!data->shell_surface.libdecor.initial_configure_seen) {
                WAYLAND_wl_display_flush(c->display);
                WAYLAND_wl_display_dispatch(c->display);
            }
        }
    } else
#endif
        if (data->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP || data->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        /* Unlike libdecor we need to call this explicitly to prevent a deadlock.
         * libdecor will call this as part of their configure event!
         * -flibit
         */
        wl_surface_commit(data->surface);
        if (data->shell_surface.xdg.surface) {
            while (!data->shell_surface.xdg.initial_configure_seen) {
                WAYLAND_wl_display_flush(c->display);
                WAYLAND_wl_display_dispatch(c->display);
            }
        }

        /* Create the window decorations */
        if (data->shell_surface_type != WAYLAND_SURFACE_XDG_POPUP && data->shell_surface.xdg.roleobj.toplevel && c->decoration_manager) {
            data->server_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(c->decoration_manager, data->shell_surface.xdg.roleobj.toplevel);
            zxdg_toplevel_decoration_v1_add_listener(data->server_decoration,
                                                     &decoration_listener,
                                                     window);
        }

        /* Set the geometry */
        xdg_surface_set_window_geometry(data->shell_surface.xdg.surface, 0, 0, data->wl_window_width, data->wl_window_height);
    } else {
        /* Nothing to see here, just commit. */
        wl_surface_commit(data->surface);
    }

    /* Unlike the rest of window state we have to set this _after_ flushing the
     * display, because we need to create the decorations before possibly hiding
     * them immediately afterward.
     */
#ifdef HAVE_LIBDECOR_H
    if (data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        /* Libdecor plugins can enforce minimum window sizes, so adjust if the initial window size is too small. */
        if (window->windowed.w < data->system_min_required_width ||
            window->windowed.h < data->system_min_required_height) {

            /* Warn if the window frame will be larger than the content surface. */
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "Window dimensions (%i, %i) are smaller than the system enforced minimum (%i, %i); window borders will be larger than the content surface.",
                        window->windowed.w, window->windowed.h, data->system_min_required_width, data->system_min_required_height);

            data->wl_window_width = SDL_max(window->windowed.w, data->system_min_required_width);
            data->wl_window_height = SDL_max(window->windowed.h, data->system_min_required_height);
            CommitLibdecorFrame(window);
        }
    }
#endif
    Wayland_SetWindowResizable(_this, window, !!(window->flags & SDL_WINDOW_RESIZABLE));
    Wayland_SetWindowBordered(_this, window, !(window->flags & SDL_WINDOW_BORDERLESS));


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

    data->show_hide_sync_required = SDL_TRUE;
    struct wl_callback *cb = wl_display_sync(_this->driverdata->display);
    wl_callback_add_listener(cb, &show_hide_sync_listener, (void*)((uintptr_t)window->id));

    /* Send an exposure event to signal that the client should draw. */
    if (data->surface_status == WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME) {
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
    }
}

static void Wayland_ReleasePopup(SDL_VideoDevice *_this, SDL_Window *popup)
{
    SDL_WindowData *popupdata;

    /* Basic sanity checks to weed out the weird popup closures */
    if (!popup || popup->magic != &_this->window_magic) {
        return;
    }
    popupdata = popup->driverdata;
    if (!popupdata) {
        return;
    }

    /* This may already be freed by a parent popup! */
    if (popupdata->shell_surface.xdg.roleobj.popup.popup == NULL) {
        return;
    }

    if (popup->flags & SDL_WINDOW_POPUP_MENU) {
        if (popup == SDL_GetKeyboardFocus()) {
            SDL_Window *new_focus = popup->parent;

            /* Find the highest level window that isn't being hidden or destroyed. */
            while (new_focus->parent && (new_focus->is_hiding || new_focus->is_destroying)) {
                new_focus = new_focus->parent;
            }

            SetKeyboardFocus(new_focus);
        }
    }

    xdg_popup_destroy(popupdata->shell_surface.xdg.roleobj.popup.popup);
    xdg_positioner_destroy(popupdata->shell_surface.xdg.roleobj.popup.positioner);
    popupdata->shell_surface.xdg.roleobj.popup.popup = NULL;
    popupdata->shell_surface.xdg.roleobj.popup.positioner = NULL;

    SDL_PropertiesID props = SDL_GetWindowProperties(popup);
    SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_POPUP_POINTER, NULL);
    SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_POSITIONER_POINTER, NULL);
}

void Wayland_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

    /* Custom surfaces have nothing to destroy and are always considered to be 'shown'; nothing to do here. */
    if (wind->shell_surface_type == WAYLAND_SURFACE_CUSTOM) {
        return;
    }

    /* The window was shown, but the sync point hasn't yet been reached.
     * Pump events to avoid a possible protocol violation.
     */
    if (wind->show_hide_sync_required) {
        WAYLAND_wl_display_roundtrip(data->display);
    }

    wind->surface_status = WAYLAND_SURFACE_STATUS_HIDDEN;

    if (wind->server_decoration) {
        zxdg_toplevel_decoration_v1_destroy(wind->server_decoration);
        wind->server_decoration = NULL;
    }

    /* Be sure to detach after this is done, otherwise ShowWindow crashes! */
    if (wind->shell_surface_type != WAYLAND_SURFACE_XDG_POPUP) {
        wl_surface_attach(wind->surface, NULL, 0, 0);
        wl_surface_commit(wind->surface);
    }

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (wind->shell_surface.libdecor.frame) {
            libdecor_frame_unref(wind->shell_surface.libdecor.frame);
            wind->shell_surface.libdecor.frame = NULL;

            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_SURFACE_POINTER, NULL);
            SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, NULL);
        }
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
        Wayland_ReleasePopup(_this, window);
    } else if (wind->shell_surface.xdg.roleobj.toplevel) {
        xdg_toplevel_destroy(wind->shell_surface.xdg.roleobj.toplevel);
        wind->shell_surface.xdg.roleobj.toplevel = NULL;
        SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, NULL);
    }
    if (wind->shell_surface.xdg.surface) {
        xdg_surface_destroy(wind->shell_surface.xdg.surface);
        wind->shell_surface.xdg.surface = NULL;
        SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_XDG_SURFACE_POINTER, NULL);
    }

    wind->show_hide_sync_required = SDL_TRUE;
    struct wl_callback *cb = wl_display_sync(_this->driverdata->display);
    wl_callback_add_listener(cb, &show_hide_sync_listener, (void*)((uintptr_t)window->id));
}

static void handle_xdg_activation_done(void *data,
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
static void Wayland_activate_window(SDL_VideoData *data, SDL_WindowData *target_wind, SDL_bool set_serial)
{
    struct SDL_WaylandInput * input = data->input;
    SDL_Window *focus = SDL_GetKeyboardFocus();
    struct wl_surface *requesting_surface = focus ? focus->driverdata->surface : NULL;

    if (data->activation_manager) {
        if (target_wind->activation_token) {
            /* We're about to overwrite this with a new request */
            xdg_activation_token_v1_destroy(target_wind->activation_token);
        }

        target_wind->activation_token = xdg_activation_v1_get_activation_token(data->activation_manager);
        xdg_activation_token_v1_add_listener(target_wind->activation_token,
                                             &activation_listener_xdg,
                                             target_wind);

        /* Note that we are not setting the app_id here.
         *
         * Hypothetically we could set the app_id from data->classname, but
         * that part of the API is for _external_ programs, not ourselves.
         *
         * -flibit
         */
        if (requesting_surface) {
            /* This specifies the surface from which the activation request is originating, not the activation target surface. */
            xdg_activation_token_v1_set_surface(target_wind->activation_token, requesting_surface);
        }
        if (set_serial && input && input->seat) {
            xdg_activation_token_v1_set_serial(target_wind->activation_token, input->last_implicit_grab_serial, input->seat);
        }
        xdg_activation_token_v1_commit(target_wind->activation_token);
    }
}

void Wayland_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    Wayland_activate_window(_this->driverdata, window->driverdata, SDL_TRUE);
}

int Wayland_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation)
{
    /* Not setting the serial will specify 'urgency' without switching focus as per
     * https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/9#note_854977
     */
    Wayland_activate_window(_this->driverdata, window->driverdata, SDL_FALSE);
    return 0;
}

static void fullscreen_configure_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    /* Get the window from the ID as it may have been destroyed */
    SDL_WindowID windowID = (SDL_WindowID)((uintptr_t)data);
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window && window->driverdata && window->driverdata->is_fullscreen) {
        ConfigureWindowGeometry(window);
        CommitLibdecorFrame(window);
    }

    wl_callback_destroy(callback);
}

static struct wl_callback_listener fullscreen_configure_listener = {
    fullscreen_configure_handler
};

int Wayland_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window,
                                 SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    SDL_WindowData *wind = window->driverdata;
    struct wl_output *output = display->driverdata->output;

    /* Custom surfaces have no toplevel to make fullscreen. */
    if (wind->shell_surface_type == WAYLAND_SURFACE_CUSTOM) {
        return -1;
    }

    if (wind->show_hide_sync_required) {
        WAYLAND_wl_display_roundtrip(_this->driverdata->display);
    }

    /* Flushing old events pending a new one, ignore this request. */
    if (wind->drop_fullscreen_requests) {
        return 0;
    }

    wind->drop_fullscreen_requests = SDL_TRUE;
    FlushFullscreenEvents(window);
    wind->drop_fullscreen_requests = SDL_FALSE;

    /* Don't send redundant fullscreen set/unset events. */
    if (fullscreen != wind->is_fullscreen) {
        wind->fullscreen_was_positioned = fullscreen;
        SetFullscreen(window, fullscreen ? output : NULL);
    } else if (wind->is_fullscreen) {
        /*
         * If the window is already fullscreen, this is likely a request to switch between
         * fullscreen and fullscreen desktop, change outputs, or change the video mode.
         *
         * If the window is already positioned on the target output, just update the
         * window geometry.
         */
        if (wind->last_displayID != display->id) {
            wind->fullscreen_was_positioned = SDL_TRUE;
            SetFullscreen(window, output);
        } else {
            /* Queue a configure event */
            struct wl_callback *cb = wl_display_sync(_this->driverdata->display);
            wl_callback_add_listener(cb, &fullscreen_configure_listener, (void *)((uintptr_t)window->id));
        }
    }

    return 1;
}

void Wayland_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        libdecor_frame_unset_maximized(wind->shell_surface.libdecor.frame);
    } else
#endif
        /* Note that xdg-shell does NOT provide a way to unset minimize! */
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
            if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
                return; /* Can't do anything yet, wait for ShowWindow */
            }
            xdg_toplevel_unset_maximized(wind->shell_surface.xdg.roleobj.toplevel);
        }
}

void Wayland_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered)
{
    SDL_WindowData *wind = window->driverdata;
    const SDL_VideoData *viddata = (const SDL_VideoData *)_this->driverdata;

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (wind->shell_surface.libdecor.frame) {
            libdecor_frame_set_visibility(wind->shell_surface.libdecor.frame, bordered);
        }
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if ((viddata->decoration_manager) && (wind->server_decoration)) {
            const enum zxdg_toplevel_decoration_v1_mode mode = bordered ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
            zxdg_toplevel_decoration_v1_set_mode(wind->server_decoration, mode);
        }
    }
}

void Wayland_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable)
{
#ifdef HAVE_LIBDECOR_H
    const SDL_WindowData *wind = window->driverdata;

    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        if (libdecor_frame_has_capability(wind->shell_surface.libdecor.frame, LIBDECOR_ACTION_RESIZE)) {
            if (!resizable) {
                libdecor_frame_unset_capabilities(wind->shell_surface.libdecor.frame, LIBDECOR_ACTION_RESIZE);
            }
        } else if (resizable) {
            libdecor_frame_set_capabilities(wind->shell_surface.libdecor.frame, LIBDECOR_ACTION_RESIZE);
        }
    }
#endif

    /* When changing the resize capability on libdecor windows, the limits must always
     * be reapplied, as when libdecor changes states, it overwrites the values internally.
     */
    SetMinMaxDimensions(window);
    CommitLibdecorFrame(window);
}

void Wayland_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    if (wind->show_hide_sync_required) {
        WAYLAND_wl_display_roundtrip(_this->driverdata->display);
    }

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        libdecor_frame_set_maximized(wind->shell_surface.libdecor.frame);
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_maximized(wind->shell_surface.xdg.roleobj.toplevel);
    }
}

void Wayland_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    /* TODO: Check compositor capabilities to see if minimizing is supported */
#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (!wind->shell_surface.libdecor.frame) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        libdecor_frame_set_minimized(wind->shell_surface.libdecor.frame);
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if (wind->shell_surface.xdg.roleobj.toplevel == NULL) {
            return; /* Can't do anything yet, wait for ShowWindow */
        }
        xdg_toplevel_set_minimized(wind->shell_surface.xdg.roleobj.toplevel);
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
    }
}

void Wayland_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;

    /* This may look suspiciously like SetWindowGrab, despite SetMouseRect not
     * implicitly doing a grab. And you're right! Wayland doesn't let us mess
     * around with mouse focus whatsoever, so it just happens to be that the
     * work that we can do in these two functions ends up being the same.
     *
     * Just know that this call lets you confine with a rect, SetWindowGrab
     * lets you confine without a rect.
     */
    if (SDL_RectEmpty(&window->mouse_rect) && !(window->flags & SDL_WINDOW_MOUSE_GRABBED)) {
        Wayland_input_unconfine_pointer(data->input, window);
    } else {
        Wayland_input_confine_pointer(data->input, window);
    }
}

void Wayland_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    SDL_VideoData *data = _this->driverdata;

    if (grabbed) {
        Wayland_input_confine_pointer(data->input, window);
    } else if (SDL_RectEmpty(&window->mouse_rect)) {
        Wayland_input_unconfine_pointer(data->input, window);
    }
}

void Wayland_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    SDL_VideoData *data = _this->driverdata;

    if (grabbed) {
        Wayland_input_grab_keyboard(window, data->input);
    } else {
        Wayland_input_ungrab_keyboard(window);
    }
}

int Wayland_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *data;
    SDL_VideoData *c;
    const SDL_bool custom_surface_role = SDL_GetBooleanProperty(create_props, SDL_PROPERTY_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, SDL_FALSE);
    const SDL_bool create_egl_window = !!(window->flags & SDL_WINDOW_OPENGL) ||
                                       SDL_GetBooleanProperty(create_props, SDL_PROPERTY_WINDOW_CREATE_WAYLAND_CREATE_EGL_WINDOW_BOOLEAN, SDL_FALSE);

    data = SDL_calloc(1, sizeof(*data));
    if (!data) {
        return -1;
    }

    c = _this->driverdata;
    window->driverdata = data;

    if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        window->x = 0;
    }
    if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        window->y = 0;
    }

    if (SDL_WINDOW_IS_POPUP(window)) {
        EnsurePopupPositionIsValid(window, &window->x, &window->y);
    }

    data->waylandData = c;
    data->sdlwindow = window;

    data->windowed_scale_factor = 1.0f;

    if (window->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) {
        int i;
        for (i = 0; i < _this->num_displays; i++) {
            float scale = _this->displays[i]->driverdata->scale_factor;
            data->windowed_scale_factor = SDL_max(data->windowed_scale_factor, scale);
        }
    }

    data->outputs = NULL;
    data->num_outputs = 0;

    /* Cache the app_id at creation time, as it may change before the window is mapped. */
    data->app_id = SDL_strdup(SDL_GetAppID());

    data->requested_window_width = window->w;
    data->requested_window_height = window->h;

    data->surface = wl_compositor_create_surface(c->compositor);
    wl_surface_add_listener(data->surface, &surface_listener, data);

    SDL_WAYLAND_register_surface(data->surface);

    /* Must be called before EGL configuration to set the drawable backbuffer size. */
    ConfigureWindowGeometry(window);

    /* Fire a callback when the compositor wants a new frame rendered.
     * Right now this only matters for OpenGL; we use this callback to add a
     * wait timeout that avoids getting deadlocked by the compositor when the
     * window isn't visible.
     */
    if (window->flags & SDL_WINDOW_OPENGL) {
        data->gles_swap_frame_event_queue = WAYLAND_wl_display_create_queue(data->waylandData->display);
        data->gles_swap_frame_surface_wrapper = WAYLAND_wl_proxy_create_wrapper(data->surface);
        WAYLAND_wl_proxy_set_queue((struct wl_proxy *)data->gles_swap_frame_surface_wrapper, data->gles_swap_frame_event_queue);
        data->gles_swap_frame_callback = wl_surface_frame(data->gles_swap_frame_surface_wrapper);
        wl_callback_add_listener(data->gles_swap_frame_callback, &gles_swap_frame_listener, data);
    }

    /* Fire a callback when the compositor wants a new frame to set the surface damage region. */
    data->surface_frame_callback = wl_surface_frame(data->surface);
    wl_callback_add_listener(data->surface_frame_callback, &surface_frame_listener, data);

    if (window->flags & SDL_WINDOW_TRANSPARENT) {
        if (_this->gl_config.alpha_size == 0) {
            _this->gl_config.alpha_size = 8;
        }
    }

    if (create_egl_window) {
        data->egl_window = WAYLAND_wl_egl_window_create(data->surface, data->drawable_width, data->drawable_height);
    }

#ifdef SDL_VIDEO_OPENGL_EGL
    if (window->flags & SDL_WINDOW_OPENGL) {
        /* Create the GLES window surface */
        data->egl_surface = SDL_EGL_CreateSurface(_this, window, (NativeWindowType)data->egl_window);

        if (data->egl_surface == EGL_NO_SURFACE) {
            return -1; /* SDL_EGL_CreateSurface should have set error */
        }
    }
#endif

    if (c->relative_mouse_mode) {
        Wayland_input_lock_pointer(c->input);
    }

    if (c->fractional_scale_manager) {
        data->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(c->fractional_scale_manager, data->surface);
        wp_fractional_scale_v1_add_listener(data->fractional_scale,
                                            &fractional_scale_listener, data);
    }

    /* We may need to create an idle inhibitor for this new window */
    Wayland_SuspendScreenSaver(_this);

    if (!custom_surface_role) {
#ifdef HAVE_LIBDECOR_H
        if (c->shell.libdecor && !SDL_WINDOW_IS_POPUP(window)) {
            data->shell_surface_type = WAYLAND_SURFACE_LIBDECOR;
        } else
#endif
            if (c->shell.xdg) {
            if (SDL_WINDOW_IS_POPUP(window)) {
                data->shell_surface_type = WAYLAND_SURFACE_XDG_POPUP;
            } else {
                data->shell_surface_type = WAYLAND_SURFACE_XDG_TOPLEVEL;
            }
        } /* All other cases will be WAYLAND_SURFACE_UNKNOWN */
    } else {
        /* Roleless surfaces are always considered to be in the shown state by the backend. */
        data->shell_surface_type = WAYLAND_SURFACE_CUSTOM;
        data->surface_status = WAYLAND_SURFACE_STATUS_SHOWN;
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_DISPLAY_POINTER, data->waylandData->display);
    SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_SURFACE_POINTER, data->surface);
    SDL_SetProperty(props, SDL_PROPERTY_WINDOW_WAYLAND_EGL_WINDOW_POINTER, data->egl_window);

    data->hit_test_result = SDL_HITTEST_NORMAL;

    return 0;
}

void Wayland_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* Will be committed when Wayland_SetWindowSize() is called by the video core. */
    SetMinMaxDimensions(window);
}

void Wayland_SetWindowMaximumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* Will be committed when Wayland_SetWindowSize() is called by the video core. */
    SetMinMaxDimensions(window);
}

int Wayland_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    /* Only popup windows can be positioned relative to the parent. */
    if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
        if (wind->shell_surface.xdg.roleobj.popup.popup &&
            xdg_popup_get_version(wind->shell_surface.xdg.roleobj.popup.popup) < XDG_POPUP_REPOSITION_SINCE_VERSION) {
            return SDL_Unsupported();
        }

        RepositionPopup(window, SDL_FALSE);
        return 0;
    } else if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR || wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        const int x = window->floating.x;
        const int y = window->floating.y;

        /* Catch up on any pending state before attempting to change the fullscreen window
         * display via a set fullscreen call to make sure the window doesn't have a pending
         * leave fullscreen event that it might override.
         */
        FlushFullscreenEvents(window);

        /* XXX: Need to restore this after the roundtrip, as the requested coordinates might
         *      have been overwritten by the 'real' coordinates if a display enter/leave event
         *      occurred.
         *
         * The common pattern:
         *
         * SDL_SetWindowPosition();
         * SDL_SetWindowFullscreen();
         *
         * for positioning a desktop fullscreen window won't work without this.
         */
        window->floating.x = x;
        window->floating.y = y;

        if (wind->is_fullscreen) {
            SDL_VideoDisplay *display = SDL_GetVideoDisplayForFullscreenWindow(window);
            if (display && wind->last_displayID != display->id) {
                struct wl_output *output = display->driverdata->output;
                SetFullscreen(window, output);

                return 0;
            }
        }
    }
    return SDL_SetError("wayland cannot position non-popup windows");
}

static void size_event_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    /* Get the window from the ID as it may have been destroyed */
    SDL_WindowID windowID = (SDL_WindowID)((uintptr_t)data);
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    if (window && window->driverdata) {
        SDL_WindowData *wind = window->driverdata;

        /* Fullscreen windows do not get explicitly resized, and not strictly
         * obeying the size of maximized windows is a protocol violation.
         */
        if (!(window->flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
            wind->requested_window_width = wind->pending_size_event.width;
            wind->requested_window_height = wind->pending_size_event.height;

            ConfigureWindowGeometry(window);
        }

        /* Always commit, as this may be in response to a min/max limit change. */
        CommitLibdecorFrame(window);
    }

    wl_callback_destroy(callback);
}

static struct wl_callback_listener size_event_listener = {
    size_event_handler
};

void Wayland_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;

    if (wind->shell_surface_type != WAYLAND_SURFACE_CUSTOM) {
        /* Queue an event to send the window size. */
        struct wl_callback *cb = wl_display_sync(_this->driverdata->display);

        wind->pending_size_event.width = window->floating.w;
        wind->pending_size_event.height = window->floating.h;
        wl_callback_add_listener(cb, &size_event_listener, (void *)((uintptr_t)window->id));
    } else {
        /* We are being informed of a size change on a custom surface, just configure. */
        wind->requested_window_width = window->floating.w;
        wind->requested_window_height = window->floating.h;

        ConfigureWindowGeometry(window);
    }
}

void Wayland_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data;
    if (window->driverdata) {
        data = window->driverdata;
        *w = data->drawable_width;
        *h = data->drawable_height;
    }
}

void Wayland_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->driverdata;
    const char *title = window->title ? window->title : "";

#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR && wind->shell_surface.libdecor.frame) {
        libdecor_frame_set_title(wind->shell_surface.libdecor.frame, title);
    } else
#endif
        if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL && wind->shell_surface.xdg.roleobj.toplevel) {
        xdg_toplevel_set_title(wind->shell_surface.xdg.roleobj.toplevel, title);
    }
}

int Wayland_SyncWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    WAYLAND_wl_display_roundtrip(_this->driverdata->display);
    return 0;
}

void Wayland_ShowWindowSystemMenu(SDL_Window *window, int x, int y)
{
    SDL_WindowData *wind = window->driverdata;
#ifdef HAVE_LIBDECOR_H
    if (wind->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
        if (wind->shell_surface.libdecor.frame) {
            libdecor_frame_show_window_menu(wind->shell_surface.libdecor.frame, wind->waylandData->input->seat, wind->waylandData->input->last_implicit_grab_serial, x, y);
        }
    } else
#endif
    if (wind->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
        if (wind->shell_surface.xdg.roleobj.toplevel) {
            xdg_toplevel_show_window_menu(wind->shell_surface.xdg.roleobj.toplevel, wind->waylandData->input->seat, wind->waylandData->input->last_implicit_grab_serial, x, y);
        }
    }
}

int Wayland_SuspendScreenSaver(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;

#ifdef SDL_USE_LIBDBUS
    if (SDL_DBus_ScreensaverInhibit(_this->suspend_screensaver)) {
        return 0;
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

    return 0;
}

void Wayland_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_WindowData *wind = window->driverdata;

    if (data && wind) {
#ifdef SDL_VIDEO_OPENGL_EGL
        if (wind->egl_surface) {
            SDL_EGL_DestroySurface(_this, wind->egl_surface);
        }
#endif
        if (wind->egl_window) {
            WAYLAND_wl_egl_window_destroy(wind->egl_window);
        }

        if (wind->idle_inhibitor) {
            zwp_idle_inhibitor_v1_destroy(wind->idle_inhibitor);
        }

        if (wind->activation_token) {
            xdg_activation_token_v1_destroy(wind->activation_token);
        }

        if (wind->draw_viewport) {
            wp_viewport_destroy(wind->draw_viewport);
        }

        if (wind->fractional_scale) {
            wp_fractional_scale_v1_destroy(wind->fractional_scale);
        }

        SDL_free(wind->outputs);
        SDL_free(wind->app_id);

        if (wind->gles_swap_frame_callback) {
            wl_callback_destroy(wind->gles_swap_frame_callback);
            WAYLAND_wl_proxy_wrapper_destroy(wind->gles_swap_frame_surface_wrapper);
            WAYLAND_wl_event_queue_destroy(wind->gles_swap_frame_event_queue);
        }

        if (wind->surface_frame_callback) {
            wl_callback_destroy(wind->surface_frame_callback);
        }

        wl_surface_destroy(wind->surface);

        SDL_free(wind);
        WAYLAND_wl_display_flush(data->display);
    }
    window->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
