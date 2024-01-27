/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include "icon.h"

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define NUM_SPRITES   100
#define MAX_SPEED     1

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *sprite;
static SDL_FRect positions[NUM_SPRITES];
static SDL_FRect velocities[NUM_SPRITES];
static int sprite_w, sprite_h;
static int done;

static SDL_Texture *CreateTexture(SDL_Renderer *r, unsigned char *data, unsigned int len, int *w, int *h)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_RWops *src = SDL_RWFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_RW(src, SDL_TRUE);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 255, 255, 255));

            texture = SDL_CreateTextureFromSurface(r, surface);
            *w = surface->w;
            *h = surface->h;
            SDL_DestroySurface(surface);
        }
    }
    return texture;
}

static void MoveSprites(void)
{
    int i;
    int window_w;
    int window_h;
    SDL_FRect *position, *velocity;

    /* Get the window size */
    SDL_GetWindowSizeInPixels(window, &window_w, &window_h);

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderClear(renderer);

    /* Move the sprite, bounce at the wall, and draw */
    for (i = 0; i < NUM_SPRITES; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (window_w - sprite_w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (window_h - sprite_h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }

        /* Blit the sprite onto the screen */
        SDL_RenderTexture(renderer, sprite, NULL, position);
    }

    /* Update the screen! */
    SDL_RenderPresent(renderer);
}

static int InitSprites(void)
{
    /* Create the sprite texture and initialize the sprite positions */
    sprite = CreateTexture(renderer, icon_bmp, icon_bmp_len, &sprite_w, &sprite_h);

    if (!sprite) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create sprite texture");
        return -1;
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = (float)(rand() % (WINDOW_WIDTH - sprite_w));
        positions[i].y = (float)(rand() % (WINDOW_HEIGHT - sprite_h));
        positions[i].w = (float)sprite_w;
        positions[i].h = (float)sprite_h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (!velocities[i].x && !velocities[i].y) {
            velocities[i].x = (float)((rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED);
            velocities[i].y = (float)((rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED);
        }
    }

    return 0;
}

/* Encapsulated in a struct to silence shadow variable warnings */
static struct _state
{
    /* These are owned by SDL and must not be destroyed! */
    struct wl_display *wl_display;
    struct wl_surface *wl_surface;

    /* These are owned by the application and need to be cleaned up on exit. */
    struct wl_registry *wl_registry;
    struct xdg_wm_base *xdg_wm_base;
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
    done = 1;
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
    if (SDL_strcmp(interface, xdg_wm_base_interface.name) == 0) {
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

int main(int argc, char **argv)
{
    int ret = -1;
    SDL_PropertiesID props;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return -1;
    }

    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Video driver must be 'wayland', not '%s'", SDL_GetCurrentVideoDriver());
        goto exit;
    }

    /* Create a window with the custom surface role property set. */
    props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, SDL_TRUE);   /* Roleless surface */
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, SDL_TRUE);                        /* OpenGL enabled */
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, WINDOW_WIDTH);                       /* Default width */
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, WINDOW_HEIGHT);                     /* Default height */
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, SDL_TRUE);            /* Handle DPI scaling internally */
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Wayland custom surface role test"); /* Default title */

    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed");
        goto exit;
    }

    /* Create the renderer */
    renderer = SDL_CreateRenderer(window, NULL, 0);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer creation failed");
        goto exit;
    }

    /* Get the display object and use it to create a registry object, which will enumerate the xdg_wm_base protocol. */
    state.wl_display = SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, NULL);

    /* Roundtrip to enumerate registry objects. */
    wl_display_roundtrip(state.wl_display);

    if (!state.xdg_wm_base) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "'xdg_wm_base' protocol not found!");
        goto exit;
    }

    /* Get the wl_surface object from the SDL_Window, and create a toplevel window with it. */
    state.wl_surface = SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);

    /* Create the xdg_surface from the wl_surface. */
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, NULL);

    /* Create the xdg_toplevel from the xdg_surface. */
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(state.xdg_toplevel, SDL_GetWindowTitle(window));

    /* Initialize the sprites. */
    if (InitSprites() < 0) {
        goto exit;
    }

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    done = 1;
                    break;
                case SDLK_EQUALS:
                    /* Ctrl+ enlarges the window */
                    if (event.key.keysym.mod & SDL_KMOD_CTRL) {
                        int w, h;
                        SDL_GetWindowSize(window, &w, &h);
                        SDL_SetWindowSize(window, w * 2, h * 2);
                    }
                    break;
                case SDLK_MINUS:
                    /* Ctrl- shrinks the window */
                    if (event.key.keysym.mod & SDL_KMOD_CTRL) {
                        int w, h;
                        SDL_GetWindowSize(window, &w, &h);
                        SDL_SetWindowSize(window, w / 2, h / 2);
                    }
                    break;
                default:
                    break;
                }
            }
        }

        /* Draw the sprites */
        MoveSprites();
    }

    ret = 0;

exit:
    /* The display and surface handles obtained from SDL are owned by SDL and must *NOT* be destroyed here! */
    if (state.xdg_toplevel) {
        xdg_toplevel_destroy(state.xdg_toplevel);
        state.xdg_toplevel = NULL;
    }
    if (state.xdg_surface) {
        xdg_surface_destroy(state.xdg_surface);
        state.xdg_surface = NULL;
    }
    if (state.xdg_wm_base) {
        xdg_wm_base_destroy(state.xdg_wm_base);
        state.xdg_wm_base = NULL;
    }
    if (state.wl_registry) {
        wl_registry_destroy(state.wl_registry);
        state.wl_registry = NULL;
    }

    /* Destroy the SDL resources */
    if (sprite) {
        SDL_DestroyTexture(sprite);
        sprite = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    SDL_Quit();
    return ret;
}
