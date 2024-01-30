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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "../SDL_sysvideo.h"

#include "SDL_waylandmessagebox.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandutil.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "wayland-cursor.h"

#include <locale.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SDL_FORK_MESSAGEBOX 1
#define SDL_SET_LOCALE      1

#if SDL_FORK_MESSAGEBOX
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#ifdef HAVE_LIBDONNELL_H

void RegistryGlobalHandler(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
	SDL_MessageBoxDataWayland *messageboxdata;
	
	messageboxdata = (SDL_MessageBoxDataWayland *)data;
    if (SDL_strcmp(interface, wl_shm_interface.name) == 0) {
        messageboxdata->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
    } else if (SDL_strcmp(interface, wl_compositor_interface.name) == 0) {
		messageboxdata->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
    } else if (SDL_strcmp(interface, wl_seat_interface.name) == 0) {
		messageboxdata->seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 4);
	} else if (SDL_strcmp(interface, xdg_wm_base_interface.name) == 0) {
        messageboxdata->wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(messageboxdata->wm_base, &messageboxdata->wm_base_listener, NULL);
	} else if (SDL_strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		messageboxdata->decoration_manager = wl_registry_bind(wl_registry, name, &zxdg_decoration_manager_v1_interface, 1);
		messageboxdata->has_decoration_manager = SDL_TRUE;
    }
}

void RegistryGlobalRemoveHandler(void *data, struct wl_registry *wl_registry, uint32_t name) {

}

void WMBasePing(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

void FreeBuffer(void *data, struct wl_buffer *wl_buffer) {
    wl_buffer_destroy(wl_buffer);
}

struct wl_buffer *ConvertToBuffer(SDL_MessageBoxDataWayland *wdata) {
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	uint32_t *data;
    unsigned int stride;
    unsigned int size;
    unsigned int j;
    unsigned int i;
	int fd;
	
    stride = wdata->generic->buffer->width * 4;
    size = stride * wdata->generic->buffer->height;

	fd = wayland_create_tmp_file(size);
    if (fd == -1) {
        return NULL;
    }
     
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    pool = wl_shm_create_pool(wdata->shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, wdata->generic->buffer->width, wdata->generic->buffer->height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

	for (i = 0; i < wdata->generic->buffer->height; i++) {
		for (j = 0; j < wdata->generic->buffer->width; j++) {
			DonnellPixel *pixel;

			pixel = Donnell_ImageBuffer_GetPixel(wdata->generic->buffer, j, i);			
			data[i * wdata->generic->buffer->width + j] = (((int)pixel->alpha)<<24) + (((int)pixel->red)<<16) + (((int)pixel->green)<<8) + ((int)pixel->blue);
			Donnell_Pixel_Free(pixel);
		}
	}
	
    munmap(data, size);
	wl_buffer_add_listener(buffer, &wdata->buffer_listener, NULL);
    return buffer;
}

void DrawMessagebox(SDL_MessageBoxDataWayland *messageboxdata) {
    struct wl_buffer *buffer;

	buffer = ConvertToBuffer(messageboxdata);
    wl_surface_attach(messageboxdata->surface, buffer, 0, 0);
    wl_surface_commit(messageboxdata->surface);
	wl_surface_damage(messageboxdata->surface, 0, 0, messageboxdata->generic->buffer->width, messageboxdata->generic->buffer->height);
}

void SurfaceConfigure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
	DrawMessagebox((SDL_MessageBoxDataWayland *)data);
}

void PointerEnter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x,wl_fixed_t y)
{
	SDL_MessageBoxDataWayland *messageboxdata;

	messageboxdata = (SDL_MessageBoxDataWayland *)data;
	
	wl_pointer_set_cursor(pointer, serial, messageboxdata->cursor_surface, messageboxdata->cursor_image->hotspot_x, messageboxdata->cursor_image->hotspot_y);
}

void PointerLeave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{ 
	SDL_MessageBoxDataWayland *messageboxdata;
	int i;
	
	messageboxdata = (SDL_MessageBoxDataWayland *)data;	

	for (i = 0; i < messageboxdata->generic->messageboxdata->numbuttons; i++) {
		messageboxdata->generic->buttons[i].button_state = DONNELL_BUTTON_STATE_NORMAL;
	}	
	
	SDL_RenderGenericMessageBox(messageboxdata->generic, SDL_TRUE);	
	DrawMessagebox(messageboxdata);
}

void PointerMotion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{ 
	SDL_MessageBoxDataWayland *messageboxdata;
	SDL_bool redraw;
	int ix;
	int iy;
	int cy;
	int cx;
	int i;
	
	messageboxdata = (SDL_MessageBoxDataWayland *)data;	
	messageboxdata->last_x = ix = wl_fixed_to_int(x);
	messageboxdata->last_y = iy = wl_fixed_to_int(y);
	redraw = SDL_FALSE;
	
	for (i = 0; i < messageboxdata->generic->messageboxdata->numbuttons; i++) {
		cy = messageboxdata->generic->buttons[i].button_rect.y + messageboxdata->generic->buttons[i].button_rect.h;
		cx = messageboxdata->generic->buttons[i].button_rect.x + messageboxdata->generic->buttons[i].button_rect.w;
		
		if (!(messageboxdata->generic->buttons[i].button_state & DONNELL_BUTTON_STATE_NORMAL)) {
			redraw = SDL_TRUE;	
		}
		messageboxdata->generic->buttons[i].button_state = DONNELL_BUTTON_STATE_NORMAL;
		
		if (ix > messageboxdata->generic->buttons[i].button_rect.x && ix < cx && iy > messageboxdata->generic->buttons[i].button_rect.y && iy < cy) {
			messageboxdata->generic->buttons[i].button_state = DONNELL_BUTTON_STATE_HOVER;
			redraw = SDL_TRUE;
		}
	}	
	
	if (redraw) {
		SDL_RenderGenericMessageBox(messageboxdata->generic, SDL_TRUE);	
		DrawMessagebox(messageboxdata);
	}
}

void PointerButton(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{ 
	SDL_MessageBoxDataWayland *messageboxdata;
	SDL_bool redraw;
	int i;
	int cx;
	int cy;
	
	messageboxdata = (SDL_MessageBoxDataWayland *)data;	
	redraw = SDL_FALSE;
	
	if (button == 0x110) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			for (i = 0; i < messageboxdata->generic->messageboxdata->numbuttons; i++) {
				cy = messageboxdata->generic->buttons[i].button_rect.y + messageboxdata->generic->buttons[i].button_rect.h;
				cx = messageboxdata->generic->buttons[i].button_rect.x + messageboxdata->generic->buttons[i].button_rect.w;
				if (messageboxdata->last_x > messageboxdata->generic->buttons[i].button_rect.x && messageboxdata->last_x < cx && messageboxdata->last_y > messageboxdata->generic->buttons[i].button_rect.y && messageboxdata->last_y < cy) {
					messageboxdata->generic->buttons[i].button_state = DONNELL_BUTTON_STATE_PRESSED;
					redraw = SDL_TRUE;
					messageboxdata->should_close = SDL_TRUE;
					messageboxdata->button = i;
				}
			}
		} else {
			if (messageboxdata-> should_close) {
				messageboxdata->running = SDL_FALSE;
			}
		}
	}
	
	if (redraw) {
		SDL_RenderGenericMessageBox(messageboxdata->generic, SDL_TRUE);	
		DrawMessagebox(messageboxdata);
	}
}

void PointerAxis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{ 
	
}


#ifdef HAVE_LIBDECOR_H
void LibDecorError(struct libdecor *context, enum libdecor_error error, const char *message) {
	exit(EXIT_FAILURE);
}

void LibDecorConfigure(struct libdecor_frame *frame, struct libdecor_configuration *configuration, void *user_data)
{
	SDL_MessageBoxDataWayland *messageboxdata;
	int width, height;
	enum libdecor_window_state window_state;
	struct libdecor_state *state;

	messageboxdata = (SDL_MessageBoxDataWayland *)user_data;

	if (!libdecor_configuration_get_window_state(configuration, &window_state)) {
		window_state = LIBDECOR_WINDOW_STATE_NONE;
	}
	messageboxdata->state = window_state;
	if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
		width = messageboxdata->generic->buffer->width;
		height = messageboxdata->generic->buffer->height;
	}

	state = libdecor_state_new(width, height);
	libdecor_frame_commit(frame, state, configuration);
	libdecor_state_free(state);
	DrawMessagebox(messageboxdata);
}

void LibDecorClose(struct libdecor_frame *frame, void *user_data) {
	SDL_MessageBoxDataWayland *messageboxdata;
	messageboxdata = (SDL_MessageBoxDataWayland *)user_data;
	messageboxdata->running = SDL_FALSE;
}

void LibDecorCommit(struct libdecor_frame *frame, void *user_data) {
	SDL_MessageBoxDataWayland *messageboxdata;
	messageboxdata = (SDL_MessageBoxDataWayland *)user_data;
	wl_surface_commit(messageboxdata->surface);
}

void LibDecorDismiss(struct libdecor_frame *frame, const char *seat_name, void *user_data) {
	
}

#endif

int Wayland_MessageBoxInit(SDL_MessageBoxDataWayland *messageboxdata, int *buttonid) {
    messageboxdata->display = WAYLAND_wl_display_connect(NULL);
    if (!messageboxdata->display) {
        return SDL_SetError("Couldn't open display");
    }
	
	messageboxdata->button = -1;
	messageboxdata->has_decoration_manager = SDL_FALSE;
	messageboxdata->running = SDL_TRUE;
    messageboxdata->registry_listener.global = RegistryGlobalHandler;
    messageboxdata->registry_listener.global_remove = RegistryGlobalRemoveHandler;
    messageboxdata->wm_base_listener.ping = WMBasePing;
    messageboxdata->surface_listener.configure = SurfaceConfigure,
	messageboxdata->buffer_listener.release = FreeBuffer;
	messageboxdata->pointer_listener.enter = PointerEnter;
    messageboxdata->pointer_listener.leave = PointerLeave;
    messageboxdata->pointer_listener.motion = PointerMotion;
    messageboxdata->pointer_listener.button = PointerButton;
    messageboxdata->pointer_listener.axis = PointerAxis;

    messageboxdata->registry = wl_display_get_registry(messageboxdata->display);
    wl_registry_add_listener(messageboxdata->registry, &messageboxdata->registry_listener, messageboxdata);
    WAYLAND_wl_display_roundtrip(messageboxdata->display);
           
	messageboxdata->pointer = wl_seat_get_pointer(messageboxdata->seat);
	wl_pointer_add_listener(messageboxdata->pointer, &messageboxdata->pointer_listener, messageboxdata);

    messageboxdata->surface = wl_compositor_create_surface(messageboxdata->compositor);
    
#ifdef HAVE_LIBDECOR_H
	messageboxdata->libdecor_iface.error = LibDecorError;
	messageboxdata->libdecor = libdecor_new(messageboxdata->display, &messageboxdata->libdecor_iface);
	if (messageboxdata->libdecor) {
		wl_surface_commit(messageboxdata->surface);
		messageboxdata->frame_iface.configure = LibDecorConfigure;
		messageboxdata->frame_iface.close = LibDecorClose;
		messageboxdata->frame_iface.commit = LibDecorCommit;
		messageboxdata->frame_iface.dismiss_popup = LibDecorDismiss;
		messageboxdata->frame = libdecor_decorate(messageboxdata->libdecor, messageboxdata->surface, &messageboxdata->frame_iface, messageboxdata);
		libdecor_frame_set_app_id(messageboxdata->frame, "SDL_MESSAGEBOX");
		libdecor_frame_set_title(messageboxdata->frame, messageboxdata->generic->messageboxdata->title);
		libdecor_frame_unset_capabilities(messageboxdata->frame,LIBDECOR_ACTION_RESIZE | LIBDECOR_ACTION_MINIMIZE | LIBDECOR_ACTION_FULLSCREEN);
		libdecor_frame_set_capabilities(messageboxdata->frame, LIBDECOR_ACTION_CLOSE | LIBDECOR_ACTION_MOVE);
		libdecor_frame_set_max_content_size(messageboxdata->frame, messageboxdata->generic->buffer->width, messageboxdata->generic->buffer->height);
		libdecor_frame_set_min_content_size(messageboxdata->frame, messageboxdata->generic->buffer->width, messageboxdata->generic->buffer->height);			
		libdecor_frame_map(messageboxdata->frame);	
	} else {
#endif
	messageboxdata->surface_xdg = xdg_wm_base_get_xdg_surface(messageboxdata->wm_base, messageboxdata->surface);
    xdg_surface_add_listener(messageboxdata->surface_xdg, &messageboxdata->surface_listener, messageboxdata);
    
	messageboxdata->toplevel = xdg_surface_get_toplevel(messageboxdata->surface_xdg);
	xdg_toplevel_set_title(messageboxdata->toplevel, messageboxdata->generic->messageboxdata->title);	
    WAYLAND_wl_display_roundtrip(messageboxdata->display);
    
	if (messageboxdata->has_decoration_manager) {
		messageboxdata->server_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(messageboxdata->decoration_manager, messageboxdata->toplevel);
		zxdg_toplevel_decoration_v1_set_mode(messageboxdata->server_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
#ifdef HAVE_LIBDECOR_H
	}
#endif

	messageboxdata->cursor_theme = WAYLAND_wl_cursor_theme_load(NULL, 24, messageboxdata->shm);
	messageboxdata->cursor = WAYLAND_wl_cursor_theme_get_cursor(messageboxdata->cursor_theme, "left_ptr");
	messageboxdata->cursor_image = messageboxdata->cursor->images[0];
    messageboxdata->cursor_buffer = WAYLAND_wl_cursor_image_get_buffer(messageboxdata->cursor_image);
	messageboxdata->cursor_surface = wl_compositor_create_surface(messageboxdata->compositor);
	wl_surface_attach(messageboxdata->cursor_surface, messageboxdata->cursor_buffer, 0, 0);
	wl_surface_commit(messageboxdata->cursor_surface);
	wl_surface_commit(messageboxdata->surface);
		    
	return 0;
}

int Wayland_ShowMessageBoxImpl(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    SDL_MessageBoxDataWayland data;
#if SDL_SET_LOCALE
    char *origlocale;
#endif

    if (!SDL_WAYLAND_LoadSymbols()) {
        return -1;
    }

#if SDL_SET_LOCALE
    origlocale = setlocale(LC_ALL, NULL);
    if (origlocale) {
        origlocale = SDL_strdup(origlocale);
        if (!origlocale) {
            return -1;
        }
        (void)setlocale(LC_ALL, "");
    }
#endif

    SDL_zero(data);
    
	data.generic = SDL_CreateGenericMessageBoxData(messageboxdata, 1);
	SDL_RenderGenericMessageBox(data.generic, SDL_FALSE);	
	Wayland_MessageBoxInit(&data, buttonid);
	
#ifdef HAVE_LIBDECOR_H
	while (libdecor_dispatch(data.libdecor, 500) >= 0) {
		if(!data.running) {
			break;
		}
	}
#else
    while (WAYLAND_wl_display_dispatch(data.display)) {
		if(!data.running) {
			break;
		}
    }
#endif

	*buttonid = data.button;
	SDL_DestroyGenericMessageBoxData(data.generic);

#if SDL_SET_LOCALE
    if (origlocale) {
        (void)setlocale(LC_ALL, origlocale);
        SDL_free(origlocale);
    }
#endif
	return 0;
}
#endif

int Wayland_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    if (!SDL_getenv("WAYLAND_DISPLAY")) {
        const char *session = SDL_getenv("XDG_SESSION_TYPE");
        if (session && SDL_strcasecmp(session, "wayland") != 0) {
            return SDL_SetError("Not on a wayland display");
        }
    }

#ifndef HAVE_LIBDONNELL_H
    return SDL_ShowGenericMessageBox(messageboxdata, buttonid);
#else
#if SDL_FORK_MESSAGEBOX
    /* Use a child process to protect against setlocale(). Annoying. */
    pid_t pid;
    int fds[2];
    int status = 0;

    if (pipe(fds) == -1) {
        return Wayland_ShowMessageBoxImpl(messageboxdata, buttonid); /* oh well. */
    }

    pid = fork();
    if (pid == -1) { /* failed */
        close(fds[0]);
        close(fds[1]);
        return Wayland_ShowMessageBoxImpl(messageboxdata, buttonid); /* oh well. */
    } else if (pid == 0) {                                       /* we're the child */
        int exitcode = 0;
        close(fds[0]);
        status = Wayland_ShowMessageBoxImpl(messageboxdata, buttonid);
        if (write(fds[1], &status, sizeof(int)) != sizeof(int)) {
            exitcode = 1;
        } else if (write(fds[1], buttonid, sizeof(int)) != sizeof(int)) {
            exitcode = 1;
        }
        close(fds[1]);
        _exit(exitcode); /* don't run atexit() stuff, static destructors, etc. */
    } else {             /* we're the parent */
        pid_t rc;
        close(fds[1]);
        do {
            rc = waitpid(pid, &status, 0);
        } while ((rc == -1) && (errno == EINTR));

        SDL_assert(rc == pid); /* not sure what to do if this fails. */

        if ((rc == -1) || (!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
            status = SDL_SetError("msgbox child process failed");
        } else if ((read(fds[0], &status, sizeof(int)) != sizeof(int)) ||
                   (read(fds[0], buttonid, sizeof(int)) != sizeof(int))) {
            status = SDL_SetError("read from msgbox child process failed");
            *buttonid = 0;
        }
        close(fds[0]);

        return status;
    }
#else
    return Wayland_ShowMessageBoxImpl(messageboxdata, buttonid);
#endif
    return 0;
#endif
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
