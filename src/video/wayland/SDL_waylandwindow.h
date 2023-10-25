/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_waylandwindow_h_
#define SDL_waylandwindow_h_

#include "../SDL_sysvideo.h"
#include "../../events/SDL_touch_c.h"

#include "SDL_waylandvideo.h"

struct SDL_WaylandInput;

struct SDL_WindowData
{
    SDL_Window *sdlwindow;
    SDL_VideoData *waylandData;
    struct wl_surface *surface;
    struct wl_callback *gles_swap_frame_callback;
    struct wl_event_queue *gles_swap_frame_event_queue;
    struct wl_surface *gles_swap_frame_surface_wrapper;
    struct wl_callback *surface_frame_callback;

    union
    {
#ifdef HAVE_LIBDECOR_H
        struct
        {
            struct libdecor_frame *frame;
            SDL_bool initial_configure_seen;
        } libdecor;
#endif
        struct
        {
            struct xdg_surface *surface;
            union
            {
                struct xdg_toplevel *toplevel;
                struct
                {
                    struct xdg_popup *popup;
                    struct xdg_positioner *positioner;
                } popup;
            } roleobj;
            SDL_bool initial_configure_seen;
        } xdg;
    } shell_surface;
    enum
    {
        WAYLAND_SURFACE_UNKNOWN = 0,
        WAYLAND_SURFACE_XDG_TOPLEVEL,
        WAYLAND_SURFACE_XDG_POPUP,
        WAYLAND_SURFACE_LIBDECOR
    } shell_surface_type;
    enum
    {
        WAYLAND_SURFACE_STATUS_HIDDEN = 0,
        WAYLAND_SURFACE_STATUS_WAITING_FOR_CONFIGURE,
        WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME,
        WAYLAND_SURFACE_STATUS_SHOW_PENDING,
        WAYLAND_SURFACE_STATUS_SHOWN
    } surface_status;

    struct wl_egl_window *egl_window;
    struct SDL_WaylandInput *keyboard_device;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
    struct zwp_locked_pointer_v1 *locked_pointer;
    struct zwp_confined_pointer_v1 *confined_pointer;
    struct zxdg_toplevel_decoration_v1 *server_decoration;
    struct zwp_keyboard_shortcuts_inhibitor_v1 *key_inhibitor;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;
    struct xdg_activation_token_v1 *activation_token;
    struct wp_viewport *draw_viewport;
    struct wp_fractional_scale_v1 *fractional_scale;

    SDL_AtomicInt swap_interval_ready;

    SDL_DisplayData **outputs;
    int num_outputs;

    SDL_Window *keyboard_focus;

    char *app_id;
    float windowed_scale_factor;
    float pointer_scale_x;
    float pointer_scale_y;

    struct
    {
        int width, height;
    } pending_size_event;

    int last_configure_width, last_configure_height;
    int requested_window_width, requested_window_height;
    int drawable_width, drawable_height;
    int wl_window_width, wl_window_height;
    int system_min_required_width;
    int system_min_required_height;
    SDL_DisplayID last_displayID;
    int fullscreen_deadline_count;
    SDL_bool floating : 1;
    SDL_bool suspended : 1;
    SDL_bool active : 1;
    SDL_bool is_fullscreen : 1;
    SDL_bool drop_fullscreen_requests : 1;
    SDL_bool fullscreen_was_positioned : 1;
    SDL_bool show_hide_sync_required : 1;

    SDL_HitTestResult hit_test_result;
};

extern void Wayland_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_HideWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern int Wayland_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window,
                                        SDL_VideoDisplay *_display,
                                        SDL_bool fullscreen);
extern void Wayland_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed);
extern void Wayland_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed);
extern void Wayland_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered);
extern void Wayland_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable);
extern int Wayland_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
extern int Wayland_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_SetWindowMaximumSize(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h);
extern int Wayland_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window);
extern void Wayland_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern void Wayland_ShowWindowSystemMenu(SDL_Window *window, int x, int y);
extern void Wayland_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern int Wayland_SuspendScreenSaver(SDL_VideoDevice *_this);

extern int Wayland_SetWindowHitTest(SDL_Window *window, SDL_bool enabled);
extern int Wayland_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation);
extern int Wayland_SyncWindow(SDL_VideoDevice *_this, SDL_Window *window);

#endif /* SDL_waylandwindow_h_ */
