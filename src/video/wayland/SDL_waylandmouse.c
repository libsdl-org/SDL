/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "../SDL_sysvideo.h"

#include "SDL_mouse.h"
#include "../../events/SDL_mouse_c.h"
#include "SDL_waylandvideo.h"
#include "../SDL_pixels_c.h"
#include "SDL_waylandevents_c.h"

#include "wayland-cursor.h"
#include "SDL_waylandmouse.h"


typedef struct {
    struct wl_buffer   *buffer;
    struct wl_surface  *surface;

    int                hot_x, hot_y;
    int                w, h;

    /* shm_data is non-NULL for custom cursors.
     * When shm_data is NULL, system_cursor must be valid
     */
    SDL_SystemCursor    system_cursor;
    void               *shm_data;
} Wayland_CursorData;

static SDL_bool
wayland_get_system_cursor(SDL_VideoData *vdata, Wayland_CursorData *cdata, float *scale)
{
    struct wl_cursor_theme *theme = NULL;
    struct wl_cursor *cursor;

    char *xcursor_size;
    int size = 0;

    SDL_Window *focus;
    SDL_WindowData *focusdata;
    int i;

    /* FIXME: We need to be able to query the cursor size from the desktop at
     * some point! For a while this was 32, but when testing on real desktops it
     * seems like most of them default to 24. We'll need a protocol to get this
     * for real, but for now this is a pretty safe bet.
     * -flibit
     */
    xcursor_size = SDL_getenv("XCURSOR_SIZE");
    if (xcursor_size != NULL) {
        size = SDL_atoi(xcursor_size);
    }
    if (size <= 0) {
        size = 24;
    }

    /* First, find the appropriate theme based on the current scale... */
    focus = SDL_GetMouse()->focus;
    if (focus == NULL) {
        /* Nothing to see here, bail. */
        return SDL_FALSE;
    }
    focusdata = focus->driverdata;
    *scale = focusdata->scale_factor;
    size *= focusdata->scale_factor;
    for (i = 0; i < vdata->num_cursor_themes; i += 1) {
        if (vdata->cursor_themes[i].size == size) {
            theme = vdata->cursor_themes[i].theme;
            break;
        }
    }
    if (theme == NULL) {
        vdata->cursor_themes = SDL_realloc(vdata->cursor_themes,
                                           sizeof(SDL_WaylandCursorTheme) * (vdata->num_cursor_themes + 1));
        if (vdata->cursor_themes == NULL) {
            SDL_OutOfMemory();
            return SDL_FALSE;
        }
        theme = WAYLAND_wl_cursor_theme_load(SDL_getenv("XCURSOR_THEME"), size, vdata->shm);
        vdata->cursor_themes[vdata->num_cursor_themes].size = size;
        vdata->cursor_themes[vdata->num_cursor_themes++].theme = theme;
    }

    /* Next, find the cursor from the theme... */
    switch(cdata->system_cursor)
    {
    case SDL_SYSTEM_CURSOR_ARROW:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "left_ptr");
        break;
    case SDL_SYSTEM_CURSOR_IBEAM:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "xterm");
        break;
    case SDL_SYSTEM_CURSOR_WAIT:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "watch");
        break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_WAITARROW:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "watch");
        break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_SIZENESW:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_SIZEWE:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_SIZENS:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_SIZEALL:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    case SDL_SYSTEM_CURSOR_NO:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "xterm");
        break;
    case SDL_SYSTEM_CURSOR_HAND:
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "hand1");
        break;
    default:
        SDL_assert(0);
        return SDL_FALSE;
    }

    /* ... Set the cursor data, finally. */
    cdata->buffer = WAYLAND_wl_cursor_image_get_buffer(cursor->images[0]);
    cdata->hot_x = cursor->images[0]->hotspot_x;
    cdata->hot_y = cursor->images[0]->hotspot_y;
    cdata->w = cursor->images[0]->width;
    cdata->h = cursor->images[0]->height;
    return SDL_TRUE;
}

static int
wayland_create_tmp_file(off_t size)
{
    static const char template[] = "/sdl-shared-XXXXXX";
    char *xdg_path;
    char tmp_path[PATH_MAX];
    int fd;

    xdg_path = SDL_getenv("XDG_RUNTIME_DIR");
    if (!xdg_path) {
        return -1;
    }

    SDL_strlcpy(tmp_path, xdg_path, PATH_MAX);
    SDL_strlcat(tmp_path, template, PATH_MAX);

    fd = mkostemp(tmp_path, O_CLOEXEC);
    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void
mouse_buffer_release(void *data, struct wl_buffer *buffer)
{
}

static const struct wl_buffer_listener mouse_buffer_listener = {
    mouse_buffer_release
};

static int
create_buffer_from_shm(Wayland_CursorData *d,
                       int width,
                       int height,
                       uint32_t format)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *data = (SDL_VideoData *) vd->driverdata;
    struct wl_shm_pool *shm_pool;

    int stride = width * 4;
    int size = stride * height;

    int shm_fd;

    shm_fd = wayland_create_tmp_file(size);
    if (shm_fd < 0)
    {
        return SDL_SetError("Creating mouse cursor buffer failed.");
    }

    d->shm_data = mmap(NULL,
                       size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED,
                       shm_fd,
                       0);
    if (d->shm_data == MAP_FAILED) {
        d->shm_data = NULL;
        close (shm_fd);
        return SDL_SetError("mmap() failed.");
    }

    SDL_assert(d->shm_data != NULL);

    shm_pool = wl_shm_create_pool(data->shm, shm_fd, size);
    d->buffer = wl_shm_pool_create_buffer(shm_pool,
                                          0,
                                          width,
                                          height,
                                          stride,
                                          format);
    wl_buffer_add_listener(d->buffer,
                           &mouse_buffer_listener,
                           d);

    wl_shm_pool_destroy (shm_pool);
    close (shm_fd);

    return 0;
}

static SDL_Cursor *
Wayland_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof (*cursor));
    if (cursor) {
        SDL_VideoDevice *vd = SDL_GetVideoDevice ();
        SDL_VideoData *wd = (SDL_VideoData *) vd->driverdata;
        Wayland_CursorData *data = SDL_calloc (1, sizeof (Wayland_CursorData));
        if (!data) {
            SDL_OutOfMemory();
            SDL_free(cursor);
            return NULL;
        }
        cursor->driverdata = (void *) data;

        /* Allocate shared memory buffer for this cursor */
        if (create_buffer_from_shm (data,
                                    surface->w,
                                    surface->h,
                                    WL_SHM_FORMAT_ARGB8888) < 0)
        {
            SDL_free (cursor->driverdata);
            SDL_free (cursor);
            return NULL;
        }

        /* Wayland requires premultiplied alpha for its surfaces. */
        SDL_PremultiplyAlpha(surface->w, surface->h,
                             surface->format->format, surface->pixels, surface->pitch,
                             SDL_PIXELFORMAT_ARGB8888, data->shm_data, surface->w * 4);

        data->surface = wl_compositor_create_surface(wd->compositor);
        wl_surface_set_user_data(data->surface, NULL);

        data->hot_x = hot_x;
        data->hot_y = hot_y;
        data->w = surface->w;
        data->h = surface->h;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
Wayland_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_VideoData *data = SDL_GetVideoDevice()->driverdata;
    SDL_Cursor *cursor;

    cursor = SDL_calloc(1, sizeof (*cursor));
    if (cursor) {
        Wayland_CursorData *cdata = SDL_calloc (1, sizeof (Wayland_CursorData));
        if (!cdata) {
            SDL_OutOfMemory();
            SDL_free(cursor);
            return NULL;
        }
        cursor->driverdata = (void *) cdata;

        cdata->surface = wl_compositor_create_surface(data->compositor);
        wl_surface_set_user_data(cdata->surface, NULL);

        /* Note that we can't actually set any other cursor properties, as this
         * is output-specific. See wayland_get_system_cursor for the rest!
         */
        cdata->system_cursor = id;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
Wayland_CreateDefaultCursor()
{
    return Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
}

static void
Wayland_FreeCursor(SDL_Cursor *cursor)
{
    Wayland_CursorData *d;

    if (!cursor)
        return;

    d = cursor->driverdata;

    /* Probably not a cursor we own */
    if (!d)
        return;

    if (d->buffer && d->shm_data)
        wl_buffer_destroy(d->buffer);

    if (d->surface)
        wl_surface_destroy(d->surface);

    /* Not sure what's meant to happen to shm_data */
    SDL_free(cursor->driverdata);
    SDL_free(cursor);
}

static int
Wayland_ShowCursor(SDL_Cursor *cursor)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->driverdata;
    struct SDL_WaylandInput *input = d->input;
    struct wl_pointer *pointer = d->pointer;
    float scale = 1.0f;

    if (!pointer)
        return -1;

    if (cursor)
    {
        Wayland_CursorData *data = cursor->driverdata;

        /* TODO: High-DPI custom cursors? -flibit */
        if (data->shm_data == NULL) {
            if (!wayland_get_system_cursor(d, data, &scale)) {
                return -1;
            }
        }

        wl_surface_set_buffer_scale(data->surface, scale);
        wl_pointer_set_cursor(pointer,
                              input->pointer_enter_serial,
                              data->surface,
                              data->hot_x / scale,
                              data->hot_y / scale);
        wl_surface_attach(data->surface, data->buffer, 0, 0);
        wl_surface_damage(data->surface, 0, 0, data->w, data->h);
        wl_surface_commit(data->surface);
    }
    else
    {
        wl_pointer_set_cursor(pointer, input->pointer_enter_serial, NULL, 0, 0);
    }
    
    return 0;
}

static void
Wayland_WarpMouse(SDL_Window *window, int x, int y)
{
    SDL_Unsupported();
}

static int
Wayland_WarpMouseGlobal(int x, int y)
{
    return SDL_Unsupported();
}

static int
Wayland_SetRelativeMouseMode(SDL_bool enabled)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *data = (SDL_VideoData *) vd->driverdata;

    if (enabled)
        return Wayland_input_lock_pointer(data->input);
    else
        return Wayland_input_unlock_pointer(data->input);
}

void
Wayland_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = Wayland_CreateCursor;
    mouse->CreateSystemCursor = Wayland_CreateSystemCursor;
    mouse->ShowCursor = Wayland_ShowCursor;
    mouse->FreeCursor = Wayland_FreeCursor;
    mouse->WarpMouse = Wayland_WarpMouse;
    mouse->WarpMouseGlobal = Wayland_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = Wayland_SetRelativeMouseMode;

    SDL_SetDefaultCursor(Wayland_CreateDefaultCursor());
}

void
Wayland_FiniMouse(SDL_VideoData *data)
{
    int i;
    for (i = 0; i < data->num_cursor_themes; i += 1) {
        WAYLAND_wl_cursor_theme_destroy(data->cursor_themes[i].theme);
    }
    SDL_free(data->cursor_themes);
}

#endif  /* SDL_VIDEO_DRIVER_WAYLAND */

/* vi: set ts=4 sw=4 expandtab: */
