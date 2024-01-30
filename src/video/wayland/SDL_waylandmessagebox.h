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

#include "../SDL_genericmessagebox.h"

#ifndef SDL_waylandmessagebox_h_
#define SDL_waylandmessagebox_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "../SDL_sysvideo.h"

#include "SDL_waylandvideo.h"
#include "wayland-cursor.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#ifdef HAVE_LIBDONNELL_H
typedef struct SDL_MessageBoxDataWayland
{
	int last_x;
	int last_y;
	int button;
	
	SDL_bool has_decoration_manager;
	SDL_bool running;
	SDL_bool should_close;
	
	struct wl_surface *cursor_surface;
	struct wl_cursor_image *cursor_image;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *cursor;
	struct wl_buffer *cursor_buffer;
	struct wl_pointer *pointer;

    struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	struct xdg_surface *surface_xdg;
	struct xdg_wm_base *wm_base;
	struct xdg_toplevel *toplevel;
    struct zxdg_decoration_manager_v1 *decoration_manager;	
	struct zxdg_toplevel_decoration_v1 *server_decoration;

	struct xdg_wm_base_listener wm_base_listener;
	struct xdg_surface_listener surface_listener;
	struct wl_registry_listener registry_listener;
	struct wl_buffer_listener buffer_listener;
	struct wl_pointer_listener pointer_listener;
	
#ifdef HAVE_LIBDECOR_H
	struct libdecor *libdecor;
	struct libdecor_interface libdecor_iface;
	struct libdecor_frame* frame;
	struct libdecor_frame_interface frame_iface;
	enum libdecor_window_state state;
#endif

    SDL_MessageBoxDataGeneric *generic;
} SDL_MessageBoxDataWayland;
#endif

extern int Wayland_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);

#endif /* SDL_VIDEO_DRIVER_WAYLAND */

#endif /* SDL_waylandmessagebox_h_ */
