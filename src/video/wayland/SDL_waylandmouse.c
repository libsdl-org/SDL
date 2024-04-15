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
#include "../SDL_video_c.h"

#include "../../events/SDL_mouse_c.h"
#include "SDL_waylandvideo.h"
#include "../SDL_pixels_c.h"
#include "SDL_waylandevents_c.h"

#include "wayland-cursor.h"
#include "SDL_waylandmouse.h"
#include "SDL_waylandshmbuffer.h"

#include "cursor-shape-v1-client-protocol.h"

#include "../../SDL_hints_c.h"

static SDL_Cursor *sys_cursors[SDL_HITTEST_RESIZE_LEFT + 1];

static int Wayland_SetRelativeMouseMode(SDL_bool enabled);

typedef struct
{
    struct Wayland_SHMBuffer shmBuffer;
    struct wl_surface *surface;

    int hot_x, hot_y;
    int w, h;

    /* shmBuffer.shm_data is non-NULL for custom cursors.
     * When shm_data is NULL, system_cursor must be valid
     */
    SDL_SystemCursor system_cursor;
} Wayland_CursorData;

static int dbus_cursor_size;
static char *dbus_cursor_theme;

static void Wayland_FreeCursorThemes(SDL_VideoData *vdata)
{
    for (int i = 0; i < vdata->num_cursor_themes; i += 1) {
        WAYLAND_wl_cursor_theme_destroy(vdata->cursor_themes[i].theme);
    }
    vdata->num_cursor_themes = 0;
    SDL_free(vdata->cursor_themes);
    vdata->cursor_themes = NULL;
}

#ifdef SDL_USE_LIBDBUS

#include "../../core/linux/SDL_dbus.h"

#define CURSOR_NODE        "org.freedesktop.portal.Desktop"
#define CURSOR_PATH        "/org/freedesktop/portal/desktop"
#define CURSOR_INTERFACE   "org.freedesktop.portal.Settings"
#define CURSOR_NAMESPACE   "org.gnome.desktop.interface"
#define CURSOR_SIGNAL_NAME "SettingChanged"
#define CURSOR_SIZE_KEY    "cursor-size"
#define CURSOR_THEME_KEY   "cursor-theme"

static DBusMessage *Wayland_ReadDBusProperty(SDL_DBusContext *dbus, const char *key)
{
    static const char *iface = "org.gnome.desktop.interface";

    DBusMessage *reply = NULL;
    DBusMessage *msg = dbus->message_new_method_call(CURSOR_NODE,
                                                     CURSOR_PATH,
                                                     CURSOR_INTERFACE,
                                                     "Read"); /* Method */

    if (msg) {
        if (dbus->message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID)) {
            reply = dbus->connection_send_with_reply_and_block(dbus->session_conn, msg, DBUS_TIMEOUT_USE_DEFAULT, NULL);
        }
        dbus->message_unref(msg);
    }

    return reply;
}

static SDL_bool Wayland_ParseDBusReply(SDL_DBusContext *dbus, DBusMessage *reply, int type, void *value)
{
    DBusMessageIter iter[3];

    dbus->message_iter_init(reply, &iter[0]);
    if (dbus->message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT) {
        return SDL_FALSE;
    }

    dbus->message_iter_recurse(&iter[0], &iter[1]);
    if (dbus->message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT) {
        return SDL_FALSE;
    }

    dbus->message_iter_recurse(&iter[1], &iter[2]);
    if (dbus->message_iter_get_arg_type(&iter[2]) != type) {
        return SDL_FALSE;
    }

    dbus->message_iter_get_basic(&iter[2], value);

    return SDL_TRUE;
}

static DBusHandlerResult Wayland_DBusCursorMessageFilter(DBusConnection *conn, DBusMessage *msg, void *data)
{
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    SDL_VideoData *vdata = (SDL_VideoData *)data;

    if (dbus->message_is_signal(msg, CURSOR_INTERFACE, CURSOR_SIGNAL_NAME)) {
        DBusMessageIter signal_iter, variant_iter;
        const char *namespace, *key;

        dbus->message_iter_init(msg, &signal_iter);
        /* Check if the parameters are what we expect */
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING) {
            goto not_our_signal;
        }
        dbus->message_iter_get_basic(&signal_iter, &namespace);
        if (SDL_strcmp(CURSOR_NAMESPACE, namespace) != 0) {
            goto not_our_signal;
        }
        if (!dbus->message_iter_next(&signal_iter)) {
            goto not_our_signal;
        }
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING) {
            goto not_our_signal;
        }
        dbus->message_iter_get_basic(&signal_iter, &key);
        if (SDL_strcmp(CURSOR_SIZE_KEY, key) == 0) {
            int new_cursor_size;

            if (!dbus->message_iter_next(&signal_iter)) {
                goto not_our_signal;
            }
            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_VARIANT) {
                goto not_our_signal;
            }
            dbus->message_iter_recurse(&signal_iter, &variant_iter);
            if (dbus->message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_INT32) {
                goto not_our_signal;
            }
            dbus->message_iter_get_basic(&variant_iter, &new_cursor_size);

            if (dbus_cursor_size != new_cursor_size) {
                dbus_cursor_size = new_cursor_size;
                SDL_SetCursor(NULL); /* Force cursor update */
            }
        } else if (SDL_strcmp(CURSOR_THEME_KEY, key) == 0) {
            const char *new_cursor_theme = NULL;

            if (!dbus->message_iter_next(&signal_iter)) {
                goto not_our_signal;
            }
            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_VARIANT) {
                goto not_our_signal;
            }
            dbus->message_iter_recurse(&signal_iter, &variant_iter);
            if (dbus->message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_STRING) {
                goto not_our_signal;
            }
            dbus->message_iter_get_basic(&variant_iter, &new_cursor_theme);

            if (!dbus_cursor_theme || !new_cursor_theme || SDL_strcmp(dbus_cursor_theme, new_cursor_theme) != 0) {
                SDL_free(dbus_cursor_theme);
                if (new_cursor_theme) {
                    dbus_cursor_theme = SDL_strdup(new_cursor_theme);
                } else {
                    dbus_cursor_theme = NULL;
                }

                /* Purge the current cached themes and force a cursor refresh. */
                Wayland_FreeCursorThemes(vdata);
                SDL_SetCursor(NULL);
            }
        } else {
            goto not_our_signal;
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

not_our_signal:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void Wayland_DBusInitCursorProperties(SDL_VideoData *vdata)
{
    DBusMessage *reply;
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    SDL_bool add_filter = SDL_FALSE;

    if (!dbus) {
        return;
    }

    if ((reply = Wayland_ReadDBusProperty(dbus, CURSOR_SIZE_KEY))) {
        if (Wayland_ParseDBusReply(dbus, reply, DBUS_TYPE_INT32, &dbus_cursor_size)) {
            add_filter = SDL_TRUE;
        }
        dbus->message_unref(reply);
    }

    if ((reply = Wayland_ReadDBusProperty(dbus, CURSOR_THEME_KEY))) {
        const char *temp = NULL;
        if (Wayland_ParseDBusReply(dbus, reply, DBUS_TYPE_STRING, &temp)) {
            add_filter = SDL_TRUE;

            if (temp) {
                dbus_cursor_theme = SDL_strdup(temp);
            }
        }
        dbus->message_unref(reply);
    }

    /* Only add the filter if at least one of the settings we want is present. */
    if (add_filter) {
        dbus->bus_add_match(dbus->session_conn,
                            "type='signal', interface='" CURSOR_INTERFACE "',"
                            "member='" CURSOR_SIGNAL_NAME "', arg0='" CURSOR_NAMESPACE "'",
                            NULL);
        dbus->connection_add_filter(dbus->session_conn, &Wayland_DBusCursorMessageFilter, vdata, NULL);
        dbus->connection_flush(dbus->session_conn);
    }
}

static void Wayland_DBusFinishCursorProperties()
{
    SDL_free(dbus_cursor_theme);
    dbus_cursor_theme = NULL;
}

#endif

static SDL_bool wayland_get_system_cursor(SDL_VideoData *vdata, Wayland_CursorData *cdata, float *scale)
{
    struct wl_cursor_theme *theme = NULL;
    struct wl_cursor *cursor;
    const char *css_name = "default";
    const char *fallback_name = NULL;

    int size = dbus_cursor_size;

    SDL_Window *focus;
    SDL_WindowData *focusdata;
    int i;

    /* Fallback envvar if the DBus properties don't exist */
    if (size <= 0) {
        const char *xcursor_size = SDL_getenv("XCURSOR_SIZE");
        if (xcursor_size) {
            size = SDL_atoi(xcursor_size);
        }
    }
    if (size <= 0) {
        size = 24;
    }
    /* First, find the appropriate theme based on the current scale... */
    focus = SDL_GetMouse()->focus;
    if (!focus) {
        /* Nothing to see here, bail. */
        return SDL_FALSE;
    }
    focusdata = focus->driverdata;

    /* Cursors use integer scaling. */
    *scale = SDL_ceilf(focusdata->windowed_scale_factor);
    size *= *scale;
    for (i = 0; i < vdata->num_cursor_themes; i += 1) {
        if (vdata->cursor_themes[i].size == size) {
            theme = vdata->cursor_themes[i].theme;
            break;
        }
    }
    if (!theme) {
        const char *xcursor_theme = dbus_cursor_theme;

        SDL_WaylandCursorTheme *new_cursor_themes = SDL_realloc(vdata->cursor_themes,
                                                                sizeof(SDL_WaylandCursorTheme) * (vdata->num_cursor_themes + 1));
        if (!new_cursor_themes) {
            return SDL_FALSE;
        }
        vdata->cursor_themes = new_cursor_themes;

        /* Fallback envvar if the DBus properties don't exist */
        if (!xcursor_theme) {
            xcursor_theme = SDL_getenv("XCURSOR_THEME");
        }

        theme = WAYLAND_wl_cursor_theme_load(xcursor_theme, size, vdata->shm);
        vdata->cursor_themes[vdata->num_cursor_themes].size = size;
        vdata->cursor_themes[vdata->num_cursor_themes++].theme = theme;
    }

    css_name = SDL_GetCSSCursorName(cdata->system_cursor, &fallback_name);
    cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, css_name);
    if (!cursor && fallback_name) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, fallback_name);
    }

    /* Fallback to the default cursor if the chosen one wasn't found */
    if (!cursor) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "default");
    }
    /* Try the old X11 name as a last resort */
    if (!cursor) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "left_ptr");
    }
    if (!cursor) {
        return SDL_FALSE;
    }

    /* ... Set the cursor data, finally. */
    cdata->shmBuffer.wl_buffer = WAYLAND_wl_cursor_image_get_buffer(cursor->images[0]);
    cdata->hot_x = cursor->images[0]->hotspot_x;
    cdata->hot_y = cursor->images[0]->hotspot_y;
    cdata->w = cursor->images[0]->width;
    cdata->h = cursor->images[0]->height;
    return SDL_TRUE;
}

static SDL_Cursor *Wayland_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        SDL_VideoDevice *vd = SDL_GetVideoDevice();
        SDL_VideoData *wd = vd->driverdata;
        Wayland_CursorData *data = SDL_calloc(1, sizeof(Wayland_CursorData));
        if (!data) {
            SDL_free(cursor);
            return NULL;
        }
        cursor->driverdata = (void *)data;

        /* Allocate shared memory buffer for this cursor */
        if (Wayland_AllocSHMBuffer(surface->w, surface->h, &data->shmBuffer) != 0) {
            SDL_free(cursor->driverdata);
            SDL_free(cursor);
            return NULL;
        }

        /* Wayland requires premultiplied alpha for its surfaces. */
        SDL_PremultiplyAlpha(surface->w, surface->h,
                             surface->format->format, surface->pixels, surface->pitch,
                             SDL_PIXELFORMAT_ARGB8888, data->shmBuffer.shm_data, surface->w * 4);

        data->surface = wl_compositor_create_surface(wd->compositor);
        wl_surface_set_user_data(data->surface, NULL);

        data->hot_x = hot_x;
        data->hot_y = hot_y;
        data->w = surface->w;
        data->h = surface->h;
    }

    return cursor;
}

static SDL_Cursor *Wayland_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_VideoData *data = SDL_GetVideoDevice()->driverdata;
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));
    if (cursor) {
        Wayland_CursorData *cdata = SDL_calloc(1, sizeof(Wayland_CursorData));
        if (!cdata) {
            SDL_free(cursor);
            return NULL;
        }
        cursor->driverdata = (void *)cdata;

        /* The surface is only necessary if the cursor shape manager is not present.
         *
         * Note that we can't actually set any other cursor properties, as this
         * is output-specific. See wayland_get_system_cursor for the rest!
         */
        if (!data->cursor_shape_manager) {
            cdata->surface = wl_compositor_create_surface(data->compositor);
            wl_surface_set_user_data(cdata->surface, NULL);
        }

        cdata->system_cursor = id;
    }

    return cursor;
}

static SDL_Cursor *Wayland_CreateDefaultCursor(void)
{
    return Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
}

static void Wayland_FreeCursorData(Wayland_CursorData *d)
{
    /* Buffers for system cursors must not be destroyed. */
    if (d->shmBuffer.shm_data) {
        Wayland_ReleaseSHMBuffer(&d->shmBuffer);
    } else {
        d->shmBuffer.wl_buffer = NULL;
    }

    if (d->surface) {
        wl_surface_destroy(d->surface);
        d->surface = NULL;
    }
}

static void Wayland_FreeCursor(SDL_Cursor *cursor)
{
    if (!cursor) {
        return;
    }

    /* Probably not a cursor we own */
    if (!cursor->driverdata) {
        return;
    }

    Wayland_FreeCursorData((Wayland_CursorData *)cursor->driverdata);

    SDL_free(cursor->driverdata);
    SDL_free(cursor);
}

static void Wayland_SetSystemCursorShape(struct SDL_WaylandInput *input, SDL_SystemCursor id)
{
    Uint32 shape;

    switch (id) {
    case SDL_SYSTEM_CURSOR_ARROW:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
        break;
    case SDL_SYSTEM_CURSOR_IBEAM:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
        break;
    case SDL_SYSTEM_CURSOR_WAIT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
        break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR;
        break;
    case SDL_SYSTEM_CURSOR_WAITARROW:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS;
        break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SIZENESW:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SIZEWE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SIZENS:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SIZEALL:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL;
        break;
    case SDL_SYSTEM_CURSOR_NO:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
        break;
    case SDL_SYSTEM_CURSOR_HAND:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOP:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_RIGHT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOM:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_WINDOW_LEFT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE;
        break;
    default:
        SDL_assert(0); /* Should never be here... */
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    }

    wp_cursor_shape_device_v1_set_shape(input->cursor_shape, input->pointer_enter_serial, shape);
}

static int Wayland_ShowCursor(SDL_Cursor *cursor)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->driverdata;
    struct SDL_WaylandInput *input = d->input;
    struct wl_pointer *pointer = d->pointer;
    float scale = 1.0f;

    if (!pointer) {
        return -1;
    }

    if (cursor) {
        Wayland_CursorData *data = cursor->driverdata;

        /* TODO: High-DPI custom cursors? -flibit */
        if (!data->shmBuffer.shm_data) {
            if (input->cursor_shape) {
                Wayland_SetSystemCursorShape(input, data->system_cursor);
                return 0;
            } else if (!wayland_get_system_cursor(d, data, &scale)) {
                return -1;
            }
        }

        wl_surface_set_buffer_scale(data->surface, scale);
        wl_pointer_set_cursor(pointer,
                              input->pointer_enter_serial,
                              data->surface,
                              data->hot_x / scale,
                              data->hot_y / scale);
        wl_surface_attach(data->surface, data->shmBuffer.wl_buffer, 0, 0);
        wl_surface_damage(data->surface, 0, 0, data->w, data->h);
        wl_surface_commit(data->surface);

        input->cursor_visible = SDL_TRUE;

        if (input->relative_mode_override) {
            Wayland_input_unlock_pointer(input);
            input->relative_mode_override = SDL_FALSE;
        }

    } else {
        input->cursor_visible = SDL_FALSE;
        wl_pointer_set_cursor(pointer, input->pointer_enter_serial, NULL, 0, 0);
    }

    return 0;
}

static int Wayland_WarpMouse(SDL_Window *window, float x, float y)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->driverdata;
    struct SDL_WaylandInput *input = d->input;

    if (input->cursor_visible == SDL_TRUE) {
        return SDL_Unsupported();
    } else if (input->warp_emulation_prohibited) {
        return SDL_Unsupported();
    } else {
        if (!d->relative_mouse_mode) {
            Wayland_input_lock_pointer(input);
            input->relative_mode_override = SDL_TRUE;
        }
    }
    return 0;
}

static int Wayland_SetRelativeMouseMode(SDL_bool enabled)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *data = vd->driverdata;

    if (enabled) {
        /* Disable mouse warp emulation if it's enabled. */
        if (data->input->relative_mode_override) {
            data->input->relative_mode_override = SDL_FALSE;
        }

        /* If the app has used relative mode before, it probably shouldn't
         * also be emulating it using repeated mouse warps, so disable
         * mouse warp emulation by default.
         */
        data->input->warp_emulation_prohibited = SDL_TRUE;
        return Wayland_input_lock_pointer(data->input);
    } else {
        return Wayland_input_unlock_pointer(data->input);
    }
}

static void SDLCALL Wayland_EmulateMouseWarpChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)userdata;

    input->warp_emulation_prohibited = !SDL_GetStringBoolean(hint, !input->warp_emulation_prohibited);
}

/* Wayland doesn't support getting the true global cursor position, but it can
 * be faked well enough for what most applications use it for: querying the
 * global cursor coordinates and transforming them to the window-relative
 * coordinates manually.
 *
 * The global position is derived by taking the cursor position relative to the
 * toplevel window, and offsetting it by the origin of the output the window is
 * currently considered to be on. The cursor position and button state when the
 * cursor is outside an application window are unknown, but this gives 'correct'
 * coordinates when the window has focus, which is good enough for most
 * applications.
 */
static Uint32 SDLCALL Wayland_GetGlobalMouseState(float *x, float *y)
{
    SDL_Window *focus = SDL_GetMouseFocus();
    Uint32 ret = 0;

    if (focus) {
        int off_x, off_y;

        ret = SDL_GetMouseState(x, y);
        SDL_RelativeToGlobalForWindow(focus, focus->x, focus->y, &off_x, &off_y);
        *x += off_x;
        *y += off_y;
    }

    return ret;
}

#if 0  /* TODO RECONNECT: See waylandvideo.c for more information! */
static void Wayland_RecreateCursor(SDL_Cursor *cursor, SDL_VideoData *vdata)
{
    Wayland_CursorData *cdata = (Wayland_CursorData *) cursor->driverdata;

    /* Probably not a cursor we own */
    if (cdata == NULL) {
        return;
    }

    Wayland_FreeCursorData(cdata);

    /* We're not currently freeing this, so... yolo? */
    if (cdata->shm_data != NULL) {
        void *old_data_pointer = cdata->shm_data;
        int stride = cdata->w * 4;

        create_buffer_from_shm(cdata, cdata->w, cdata->h, WL_SHM_FORMAT_ARGB8888);

        SDL_memcpy(cdata->shm_data, old_data_pointer, stride * cdata->h);
    }
    cdata->surface = wl_compositor_create_surface(vdata->compositor);
    wl_surface_set_user_data(cdata->surface, NULL);
}

void Wayland_RecreateCursors(void)
{
    SDL_Cursor *cursor;
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_VideoData *vdata = SDL_GetVideoDevice()->driverdata;

    if (vdata && vdata->cursor_themes) {
        SDL_free(vdata->cursor_themes);
        vdata->cursor_themes = NULL;
        vdata->num_cursor_themes = 0;
    }

    if (mouse == NULL) {
        return;
    }

    for (cursor = mouse->cursors; cursor != NULL; cursor = cursor->next) {
        Wayland_RecreateCursor(cursor, vdata);
    }
    if (mouse->def_cursor) {
        Wayland_RecreateCursor(mouse->def_cursor, vdata);
    }
    if (mouse->cur_cursor) {
        Wayland_RecreateCursor(mouse->cur_cursor, vdata);
        if (mouse->cursor_shown) {
            Wayland_ShowCursor(mouse->cur_cursor);
        }
    }
}
#endif /* 0 */

void Wayland_InitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->driverdata;
    struct SDL_WaylandInput *input = d->input;

    mouse->CreateCursor = Wayland_CreateCursor;
    mouse->CreateSystemCursor = Wayland_CreateSystemCursor;
    mouse->ShowCursor = Wayland_ShowCursor;
    mouse->FreeCursor = Wayland_FreeCursor;
    mouse->WarpMouse = Wayland_WarpMouse;
    mouse->SetRelativeMouseMode = Wayland_SetRelativeMouseMode;
    mouse->GetGlobalMouseState = Wayland_GetGlobalMouseState;

    input->relative_mode_override = SDL_FALSE;
    input->cursor_visible = SDL_TRUE;

    SDL_HitTestResult r = SDL_HITTEST_NORMAL;
    while (r <= SDL_HITTEST_RESIZE_LEFT) {
        switch (r) {
        case SDL_HITTEST_NORMAL:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
            break;
        case SDL_HITTEST_DRAGGABLE:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
            break;
        case SDL_HITTEST_RESIZE_TOPLEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_TOPLEFT);
            break;
        case SDL_HITTEST_RESIZE_TOP:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_TOP);
            break;
        case SDL_HITTEST_RESIZE_TOPRIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_TOPRIGHT);
            break;
        case SDL_HITTEST_RESIZE_RIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_RIGHT);
            break;
        case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_BOTTOMRIGHT);
            break;
        case SDL_HITTEST_RESIZE_BOTTOM:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_BOTTOM);
            break;
        case SDL_HITTEST_RESIZE_BOTTOMLEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_BOTTOMLEFT);
            break;
        case SDL_HITTEST_RESIZE_LEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_WINDOW_LEFT);
            break;
        }
        r++;
    }

#ifdef SDL_USE_LIBDBUS
    Wayland_DBusInitCursorProperties(d);
#endif

    SDL_SetDefaultCursor(Wayland_CreateDefaultCursor());

    SDL_AddHintCallback(SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP,
                        Wayland_EmulateMouseWarpChanged, input);
}

void Wayland_FiniMouse(SDL_VideoData *data)
{
    struct SDL_WaylandInput *input = data->input;
    int i;

    Wayland_FreeCursorThemes(data);

#ifdef SDL_USE_LIBDBUS
    Wayland_DBusFinishCursorProperties();
#endif

    SDL_DelHintCallback(SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP,
                        Wayland_EmulateMouseWarpChanged, input);

    for (i = 0; i < SDL_arraysize(sys_cursors); i++) {
        Wayland_FreeCursor(sys_cursors[i]);
        sys_cursors[i] = NULL;
    }
}

void Wayland_SetHitTestCursor(SDL_HitTestResult rc)
{
    if (rc == SDL_HITTEST_NORMAL || rc == SDL_HITTEST_DRAGGABLE) {
        SDL_SetCursor(NULL);
    } else {
        Wayland_ShowCursor(sys_cursors[rc]);
    }
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
