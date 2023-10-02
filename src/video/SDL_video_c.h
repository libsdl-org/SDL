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

#ifndef SDL_video_c_h_
#define SDL_video_c_h_

#include "SDL_internal.h"

/**
 * Initialize the video subsystem, optionally specifying a video driver.
 *
 * This function initializes the video subsystem, setting up a connection to
 * the window manager, etc, and determines the available display modes and
 * pixel formats, but does not initialize a window or graphics mode.
 *
 * If you use this function and you haven't used the SDL_INIT_VIDEO flag with
 * either SDL_Init() or SDL_InitSubSystem(), you should call SDL_VideoQuit()
 * before calling SDL_Quit().
 *
 * It is safe to call this function multiple times. SDL_VideoInit() will call
 * SDL_VideoQuit() itself if the video subsystem has already been initialized.
 *
 * You can use SDL_GetNumVideoDrivers() and SDL_GetVideoDriver() to find a
 * specific `driver_name`.
 *
 * \param driver_name the name of a video driver to initialize, or NULL for
 *                    the default driver
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 */
extern int SDL_VideoInit(const char *driver_name);

/**
 * Shut down the video subsystem, if initialized with SDL_VideoInit().
 *
 * This function closes all windows, and restores the original video mode.
 */
extern void SDL_VideoQuit(void);

extern int SDL_SetWindowTextureVSync(SDL_Window *window, int vsync);

extern int SDL_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

#endif /* SDL_video_c_h_ */
