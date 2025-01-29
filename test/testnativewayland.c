/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "testnative.h"

#ifdef TEST_NATIVE_WAYLAND

#include <SDL3/SDL.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

static void *native_userdata_ptr = (void *)0xBAADF00D;
static const char *native_surface_tag = "SDL_NativeSurfaceTag";

static void *CreateWindowWayland(int w, int h);
static void DestroyWindowWayland(void *window);

NativeWindowFactory WaylandWindowFactory = {
    "wayland",
    CreateWindowWayland,
    DestroyWindowWayland
};

/* Encapsulated in a struct to silence shadow variable warnings */
static struct _state
{
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
} state;

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(state.xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
    /* NOP */
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    SDL_Event event;
    SDL_zero(event);

    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
    /* NOP */
}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities)
{
    /* NOP */
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(state.xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
    if (SDL_strcmp(interface, wl_compositor_interface.name) == 0) {
        state.wl_compositor = wl_registry_bind(state.wl_registry, name, &wl_compositor_interface, SDL_min(version, 4));
    } else if (SDL_strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state.xdg_wm_base = wl_registry_bind(state.wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state.xdg_wm_base, &xdg_wm_base_listener, NULL);
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
    /* NOP */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void *CreateWindowWayland(int w, int h)
{
    /* Export the display object from SDL and use it to create a registry object,
     * which will enumerate the wl_compositor and xdg_wm_base protocols.
     */
    state.wl_display = SDL_GetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, NULL);

    if (!state.wl_display) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid 'wl_display' object!");
        goto error;
    }

    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, NULL);

    /* Roundtrip to enumerate registry objects. */
    wl_display_roundtrip(state.wl_display);

    /* Protocol sanity check */
    if (!state.wl_compositor) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "'wl_compositor' protocol not found!");
        goto error;
    }
    if (!state.xdg_wm_base) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "'xdg_wm_base' protocol not found!");
        goto error;
    }

    /* Crate the backing wl_surface for the window. */
    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);

    /* Set the native tag and userdata values, which should be the same at exit. */
    wl_proxy_set_tag((struct wl_proxy *)state.wl_surface, &native_surface_tag);
    wl_surface_set_user_data(state.wl_surface, native_userdata_ptr);

    /* Create the xdg_surface from the wl_surface. */
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, NULL);

    /* Create the xdg_toplevel from the xdg_surface. */
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(state.xdg_toplevel, "Native Wayland Window");

    /* Return the wl_surface to be wrapped in an SDL_Window. */
    return state.wl_surface;

error:
    if (state.xdg_toplevel) {
        xdg_toplevel_destroy(state.xdg_toplevel);
        state.xdg_toplevel = NULL;
    }
    if (state.xdg_surface) {
        xdg_surface_destroy(state.xdg_surface);
        state.xdg_surface = NULL;
    }
    if (state.wl_surface) {
        wl_surface_destroy(state.wl_surface);
        state.wl_surface = NULL;
    }
    if (state.xdg_wm_base) {
        xdg_wm_base_destroy(state.xdg_wm_base);
        state.xdg_wm_base = NULL;
    }
    if (state.wl_compositor) {
        wl_compositor_destroy(state.wl_compositor);
        state.wl_compositor = NULL;
    }
    if (state.wl_registry) {
        wl_registry_destroy(state.wl_registry);
        state.wl_registry = NULL;
    }

    return NULL;
}

static void DestroyWindowWayland(void *window)
{
    if (state.xdg_toplevel) {
        xdg_toplevel_destroy(state.xdg_toplevel);
        state.xdg_toplevel = NULL;
    }
    if (state.xdg_surface) {
        xdg_surface_destroy(state.xdg_surface);
        state.xdg_surface = NULL;
    }
    if (state.wl_surface) {
        /* Surface sanity check; these should be unmodified. */
        if (wl_proxy_get_tag((struct wl_proxy *)state.wl_surface) != &native_surface_tag) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "The wl_surface tag was modified, this indicates a problem inside of SDL.");
        }
        if (wl_surface_get_user_data(state.wl_surface) != native_userdata_ptr) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "The wl_surface user data was modified, this indicates a problem inside of SDL.");
        }

        wl_surface_destroy(state.wl_surface);
        state.wl_surface = NULL;
    }
    if (state.xdg_wm_base) {
        xdg_wm_base_destroy(state.xdg_wm_base);
        state.xdg_wm_base = NULL;
    }
    if (state.wl_compositor) {
        wl_compositor_destroy(state.wl_compositor);
        state.wl_compositor = NULL;
    }
    if (state.wl_registry) {
        wl_registry_destroy(state.wl_registry);
        state.wl_registry = NULL;
    }
}

#endif
