/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

/* *INDENT-OFF* */

SDL_MIR_MODULE(MIR_CLIENT)
SDL_MIR_SYM(MirDisplayConfiguration*,mir_connection_create_display_config,(MirConnection *connection))
SDL_MIR_SYM(MirSurface *,mir_connection_create_surface_sync,(MirConnection *connection, MirSurfaceParameters const *params))
SDL_MIR_SYM(void,mir_connection_get_available_surface_formats,(MirConnection* connection, MirPixelFormat* formats, unsigned const int format_size, unsigned int *num_valid_formats))
SDL_MIR_SYM(MirEGLNativeDisplayType,mir_connection_get_egl_native_display,(MirConnection *connection))
SDL_MIR_SYM(MirBool,mir_connection_is_valid,(MirConnection *connection))
SDL_MIR_SYM(void,mir_connection_release,(MirConnection *connection))
SDL_MIR_SYM(MirConnection *,mir_connect_sync,(char const *server, char const *app_name))
SDL_MIR_SYM(void,mir_display_config_destroy,(MirDisplayConfiguration* display_configuration))
SDL_MIR_SYM(MirEGLNativeWindowType,mir_surface_get_egl_native_window,(MirSurface *surface))
SDL_MIR_SYM(char const *,mir_surface_get_error_message,(MirSurface *surface))
SDL_MIR_SYM(void,mir_surface_get_graphics_region,(MirSurface *surface, MirGraphicsRegion *graphics_region))
SDL_MIR_SYM(void,mir_surface_get_parameters,(MirSurface *surface, MirSurfaceParameters *parameters))
SDL_MIR_SYM(MirBool,mir_surface_is_valid,(MirSurface *surface))
SDL_MIR_SYM(void,mir_surface_release_sync,(MirSurface *surface))
SDL_MIR_SYM(void,mir_surface_set_event_handler,(MirSurface *surface, MirEventDelegate const *event_handler))
SDL_MIR_SYM(MirWaitHandle*,mir_surface_set_type,(MirSurface *surface, MirSurfaceType type))
SDL_MIR_SYM(MirWaitHandle*,mir_surface_set_state,(MirSurface *surface, MirSurfaceState state))
SDL_MIR_SYM(void,mir_surface_swap_buffers_sync,(MirSurface *surface))

SDL_MIR_MODULE(XKBCOMMON)
SDL_MIR_SYM(int,xkb_keysym_to_utf8,(xkb_keysym_t keysym, char *buffer, size_t size))

/* *INDENT-ON* */

/* vi: set ts=4 sw=4 expandtab: */
