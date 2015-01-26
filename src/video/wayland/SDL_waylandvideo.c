/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_stdinc.h"
#include "../../events/SDL_events_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandopengles.h"
#include "SDL_waylandmouse.h"
#include "SDL_waylandtouch.h"

#include <fcntl.h>
#include <xkbcommon/xkbcommon.h>

#include "SDL_waylanddyn.h"
#include <wayland-util.h>

#define WAYLANDVID_DRIVER_NAME "wayland"

struct wayland_mode {
    SDL_DisplayMode mode;
    struct wl_list link;
};

/* Initialization/Query functions */
static int
Wayland_VideoInit(_THIS);

static void
Wayland_GetDisplayModes(_THIS, SDL_VideoDisplay *sdl_display);
static int
Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);

static void
Wayland_VideoQuit(_THIS);

/* Wayland driver bootstrap functions */
static int
Wayland_Available(void)
{
    struct wl_display *display = NULL;
    if (SDL_WAYLAND_LoadSymbols()) {
        display = WAYLAND_wl_display_connect(NULL);
        if (display != NULL) {
            WAYLAND_wl_display_disconnect(display);
        }
        SDL_WAYLAND_UnloadSymbols();
    }

    return (display != NULL);
}

static void
Wayland_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
    SDL_WAYLAND_UnloadSymbols();
}

static SDL_VideoDevice *
Wayland_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    
    if (!SDL_WAYLAND_LoadSymbols()) {
        return NULL;
    }

    /* Initialize all variables that we clean on shutdown */
    device = SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_WAYLAND_UnloadSymbols();
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    device->VideoInit = Wayland_VideoInit;
    device->VideoQuit = Wayland_VideoQuit;
    device->SetDisplayMode = Wayland_SetDisplayMode;
    device->GetDisplayModes = Wayland_GetDisplayModes;
    device->GetWindowWMInfo = Wayland_GetWindowWMInfo;

    device->PumpEvents = Wayland_PumpEvents;

    device->GL_SwapWindow = Wayland_GLES_SwapWindow;
    device->GL_GetSwapInterval = Wayland_GLES_GetSwapInterval;
    device->GL_SetSwapInterval = Wayland_GLES_SetSwapInterval;
    device->GL_MakeCurrent = Wayland_GLES_MakeCurrent;
    device->GL_CreateContext = Wayland_GLES_CreateContext;
    device->GL_LoadLibrary = Wayland_GLES_LoadLibrary;
    device->GL_UnloadLibrary = Wayland_GLES_UnloadLibrary;
    device->GL_GetProcAddress = Wayland_GLES_GetProcAddress;
    device->GL_DeleteContext = Wayland_GLES_DeleteContext;

    device->CreateWindow = Wayland_CreateWindow;
    device->ShowWindow = Wayland_ShowWindow;
    device->SetWindowFullscreen = Wayland_SetWindowFullscreen;
    device->SetWindowSize = Wayland_SetWindowSize;
    device->DestroyWindow = Wayland_DestroyWindow;

    device->free = Wayland_DeleteDevice;

    return device;
}

VideoBootStrap Wayland_bootstrap = {
    WAYLANDVID_DRIVER_NAME, "SDL Wayland video driver",
    Wayland_Available, Wayland_CreateDevice
};

static void
wayland_add_mode(SDL_VideoData *d, SDL_DisplayMode m)
{
    struct wayland_mode *mode;

    /* Check for duplicate mode */
    wl_list_for_each(mode, &d->modes_list, link)
        if (mode->mode.w == m.w && mode->mode.h == m.h &&
	    mode->mode.refresh_rate == m.refresh_rate)
	    return;

    /* Add new mode to the list */
    mode = (struct wayland_mode *) SDL_calloc(1, sizeof *mode);

    if (!mode)
	return;

    mode->mode = m;
    WAYLAND_wl_list_insert(&d->modes_list, &mode->link);
}

static void
display_handle_geometry(void *data,
                        struct wl_output *output,
                        int x, int y,
                        int physical_width,
                        int physical_height,
                        int subpixel,
                        const char *make,
                        const char *model,
                        int transform)

{
    SDL_VideoData *d = data;

    d->screen_allocation.x = x;
    d->screen_allocation.y = y;
}

static void
display_handle_mode(void *data,
                    struct wl_output *wl_output,
                    uint32_t flags,
                    int width,
                    int height,
                    int refresh)
{
    SDL_VideoData *d = data;
    SDL_DisplayMode mode;

    SDL_zero(mode);
    mode.w = width;
    mode.h = height;
    mode.refresh_rate = refresh / 1000;

    wayland_add_mode(d, mode);

    if (flags & WL_OUTPUT_MODE_CURRENT) {
        d->screen_allocation.width = width;
        d->screen_allocation.height = height;
    }
}

static const struct wl_output_listener output_listener = {
    display_handle_geometry,
    display_handle_mode
};

static void
shm_handle_format(void *data,
                  struct wl_shm *shm,
                  uint32_t format)
{
    SDL_VideoData *d = data;

    d->shm_formats |= (1 << format);
}

static const struct wl_shm_listener shm_listener = {
    shm_handle_format
};

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
static void
windowmanager_hints(void *data, struct qt_windowmanager *qt_windowmanager,
        int32_t show_is_fullscreen)
{
}

static void
windowmanager_quit(void *data, struct qt_windowmanager *qt_windowmanager)
{
    SDL_SendQuit();
}

static const struct qt_windowmanager_listener windowmanager_listener = {
    windowmanager_hints,
    windowmanager_quit,
};
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

static void
display_handle_global(void *data, struct wl_registry *registry, uint32_t id,
					const char *interface, uint32_t version)
{
    SDL_VideoData *d = data;
    
    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor = wl_registry_bind(d->registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_output") == 0) {
        d->output = wl_registry_bind(d->registry, id, &wl_output_interface, 1);
        wl_output_add_listener(d->output, &output_listener, d);
    } else if (strcmp(interface, "wl_seat") == 0) {
        Wayland_display_add_input(d, id);
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->shell = wl_registry_bind(d->registry, id, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        d->cursor_theme = WAYLAND_wl_cursor_theme_load(NULL, 32, d->shm);
        d->default_cursor = WAYLAND_wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
        wl_shm_add_listener(d->shm, &shm_listener, d);
    
#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
    } else if (strcmp(interface, "qt_touch_extension") == 0) {
        Wayland_touch_create(d, id);
    } else if (strcmp(interface, "qt_surface_extension") == 0) {
        d->surface_extension = wl_registry_bind(registry, id,
                &qt_surface_extension_interface, 1);
    } else if (strcmp(interface, "qt_windowmanager") == 0) {
        d->windowmanager = wl_registry_bind(registry, id,
                &qt_windowmanager_interface, 1);
        qt_windowmanager_add_listener(d->windowmanager, &windowmanager_listener, d);
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */
    }
}

static const struct wl_registry_listener registry_listener = {
	display_handle_global
};

int
Wayland_VideoInit(_THIS)
{
    SDL_VideoData *data;
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;
    int i;
    
    data = malloc(sizeof *data);
    if (data == NULL)
        return 0;
    memset(data, 0, sizeof *data);

    _this->driverdata = data;

    WAYLAND_wl_list_init(&data->modes_list);
    
    data->display = WAYLAND_wl_display_connect(NULL);
    if (data->display == NULL) {
        SDL_SetError("Failed to connect to a Wayland display");
        return 0;
    }

    data->registry = wl_display_get_registry(data->display);
   
    if ( data->registry == NULL) {
        SDL_SetError("Failed to get the Wayland registry");
        return 0;
    }
    
    wl_registry_add_listener(data->registry, &registry_listener, data);

    for (i=0; i < 100; i++) {
        if (data->screen_allocation.width != 0 || WAYLAND_wl_display_get_error(data->display) != 0) {
            break;
        }
        WAYLAND_wl_display_dispatch(data->display);
    }
    
    if (data->screen_allocation.width == 0) {
        SDL_SetError("Failed while waiting for screen allocation: %d ", WAYLAND_wl_display_get_error(data->display));
        return 0;
    }

    data->xkb_context = WAYLAND_xkb_context_new(0);
    if (!data->xkb_context) {
        SDL_SetError("Failed to create XKB context");
        return 0;
    }

    /* Use a fake 32-bpp desktop mode */
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.w = data->screen_allocation.width;
    mode.h = data->screen_allocation.height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    wayland_add_mode(data, mode);
    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;
    display.driverdata = NULL;
    SDL_AddVideoDisplay(&display);

    Wayland_InitMouse ();

    WAYLAND_wl_display_flush(data->display);

    return 0;
}

static void
Wayland_GetDisplayModes(_THIS, SDL_VideoDisplay *sdl_display)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_DisplayMode mode;
    struct wayland_mode *m;

    Wayland_PumpEvents(_this);

    wl_list_for_each(m, &data->modes_list, link) {
        m->mode.format = SDL_PIXELFORMAT_RGB888;
        SDL_AddDisplayMode(sdl_display, &m->mode);
        m->mode.format = SDL_PIXELFORMAT_RGBA8888;
        SDL_AddDisplayMode(sdl_display, &m->mode);
    }

    mode.w = data->screen_allocation.width;
    mode.h = data->screen_allocation.height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;

    mode.format = SDL_PIXELFORMAT_RGB888;
    SDL_AddDisplayMode(sdl_display, &mode);
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    SDL_AddDisplayMode(sdl_display, &mode);
}

static int
Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void
Wayland_VideoQuit(_THIS)
{
    SDL_VideoData *data = _this->driverdata;
    struct wayland_mode *t, *m;

    Wayland_FiniMouse ();

    if (data->output)
        wl_output_destroy(data->output);

    Wayland_display_destroy_input(data);

    if (data->xkb_context) {
        WAYLAND_xkb_context_unref(data->xkb_context);
        data->xkb_context = NULL;
    }
#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
    if (data->windowmanager)
        qt_windowmanager_destroy(data->windowmanager);

    if (data->surface_extension)
        qt_surface_extension_destroy(data->surface_extension);

    Wayland_touch_destroy(data);
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */

    if (data->shm)
        wl_shm_destroy(data->shm);

    if (data->cursor_theme)
        WAYLAND_wl_cursor_theme_destroy(data->cursor_theme);

    if (data->shell)
        wl_shell_destroy(data->shell);

    if (data->compositor)
        wl_compositor_destroy(data->compositor);

    if (data->display) {
        WAYLAND_wl_display_flush(data->display);
        WAYLAND_wl_display_disconnect(data->display);
    }
    
    wl_list_for_each_safe(m, t, &data->modes_list, link) {
        WAYLAND_wl_list_remove(&m->link);
        free(m);
    }


    free(data);
    _this->driverdata = NULL;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */

/* vi: set ts=4 sw=4 expandtab: */
