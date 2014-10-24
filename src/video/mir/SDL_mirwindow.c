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

/*
  Contributed by Brandon Schaefer, <brandon.schaefer@canonical.com>
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MIR

#include "../SDL_egl_c.h"
#include "../SDL_sysvideo.h"

#include "SDL_mirevents.h"
#include "SDL_mirwindow.h"

#include "SDL_mirdyn.h"

int
IsSurfaceValid(MIR_Window* mir_window)
{
    if (!MIR_mir_surface_is_valid(mir_window->surface)) {
        const char* error = MIR_mir_surface_get_error_message(mir_window->surface);
        return SDL_SetError("Failed to created a mir surface: %s", error);
    }

    return 0;
}

MirPixelFormat
FindValidPixelFormat(MIR_Data* mir_data)
{
    unsigned int pf_size = 32;
    unsigned int valid_formats;
    unsigned int f;

    MirPixelFormat formats[pf_size];
    MIR_mir_connection_get_available_surface_formats(mir_data->connection, formats,
                                                 pf_size, &valid_formats);

    for (f = 0; f < valid_formats; f++) {
        MirPixelFormat cur_pf = formats[f];

        if (cur_pf == mir_pixel_format_abgr_8888 ||
            cur_pf == mir_pixel_format_xbgr_8888 ||
            cur_pf == mir_pixel_format_argb_8888 ||
            cur_pf == mir_pixel_format_xrgb_8888) {

            return cur_pf;
        }
    }

    return mir_pixel_format_invalid;
}

int
MIR_CreateWindow(_THIS, SDL_Window* window)
{
    MIR_Window* mir_window;
    MIR_Data* mir_data;

    MirSurfaceParameters surfaceparm =
    {
        .name = "MirSurface",
        .width = window->w,
        .height = window->h,
        .pixel_format = mir_pixel_format_invalid,
        .buffer_usage = mir_buffer_usage_hardware,
        .output_id = mir_display_output_id_invalid
    };

    MirEventDelegate delegate = {
        MIR_HandleInput,
        window
    };

    mir_window = SDL_calloc(1, sizeof(MIR_Window));
    if (!mir_window)
        return SDL_OutOfMemory();

    mir_data = _this->driverdata;
    window->driverdata = mir_window;

    if (mir_data->software)
        surfaceparm.buffer_usage = mir_buffer_usage_software;

    if (window->x == SDL_WINDOWPOS_UNDEFINED)
        window->x = 0;

    if (window->y == SDL_WINDOWPOS_UNDEFINED)
        window->y = 0;

    mir_window->mir_data = mir_data;
    mir_window->sdl_window = window;

    surfaceparm.pixel_format = FindValidPixelFormat(mir_data);
    if (surfaceparm.pixel_format == mir_pixel_format_invalid) {
        return SDL_SetError("Failed to find a valid pixel format.");
    }

    mir_window->surface = MIR_mir_connection_create_surface_sync(mir_data->connection, &surfaceparm);
    if (!MIR_mir_surface_is_valid(mir_window->surface)) {
        const char* error = MIR_mir_surface_get_error_message(mir_window->surface);
        return SDL_SetError("Failed to created a mir surface: %s", error);
    }

    if (window->flags & SDL_WINDOW_OPENGL) {
        EGLNativeWindowType egl_native_window =
                        (EGLNativeWindowType)MIR_mir_surface_get_egl_native_window(mir_window->surface);

        mir_window->egl_surface = SDL_EGL_CreateSurface(_this, egl_native_window);

        if (mir_window->egl_surface == EGL_NO_SURFACE) {
            return SDL_SetError("Failed to created a window surface %p",
                                _this->egl_data->egl_display);
        }
    }
    else {
        mir_window->egl_surface = EGL_NO_SURFACE;
    }

    MIR_mir_surface_set_event_handler(mir_window->surface, &delegate);

    return 0;
}

void
MIR_DestroyWindow(_THIS, SDL_Window* window)
{
    MIR_Data* mir_data = _this->driverdata;
    MIR_Window* mir_window = window->driverdata;

    if (mir_data) {
        SDL_EGL_DestroySurface(_this, mir_window->egl_surface);
        MIR_mir_surface_release_sync(mir_window->surface);

        SDL_free(mir_window);
    }
    window->driverdata = NULL;
}

SDL_bool
MIR_GetWindowWMInfo(_THIS, SDL_Window* window, SDL_SysWMinfo* info)
{
    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        MIR_Window* mir_window = window->driverdata;

        info->subsystem = SDL_SYSWM_MIR;
        info->info.mir.connection = mir_window->mir_data->connection;
        info->info.mir.surface = mir_window->surface;

        return SDL_TRUE;
    }

    return SDL_FALSE;
}

void
MIR_SetWindowFullscreen(_THIS, SDL_Window* window,
                        SDL_VideoDisplay* display,
                        SDL_bool fullscreen)
{
    MIR_Window* mir_window = window->driverdata;

    if (IsSurfaceValid(mir_window) < 0)
        return;

    if (fullscreen) {
        MIR_mir_surface_set_state(mir_window->surface, mir_surface_state_fullscreen);
    } else {
        MIR_mir_surface_set_state(mir_window->surface, mir_surface_state_restored);
    }
}

void
MIR_MaximizeWindow(_THIS, SDL_Window* window)
{
    MIR_Window* mir_window = window->driverdata;

    if (IsSurfaceValid(mir_window) < 0)
        return;

    MIR_mir_surface_set_state(mir_window->surface, mir_surface_state_maximized);
}

void
MIR_MinimizeWindow(_THIS, SDL_Window* window)
{
    MIR_Window* mir_window = window->driverdata;

    if (IsSurfaceValid(mir_window) < 0)
        return;

    MIR_mir_surface_set_state(mir_window->surface, mir_surface_state_minimized);
}

void
MIR_RestoreWindow(_THIS, SDL_Window * window)
{
    MIR_Window* mir_window = window->driverdata;

    if (IsSurfaceValid(mir_window) < 0)
        return;

    MIR_mir_surface_set_state(mir_window->surface, mir_surface_state_restored);
}

#endif /* SDL_VIDEO_DRIVER_MIR */

/* vi: set ts=4 sw=4 expandtab: */
