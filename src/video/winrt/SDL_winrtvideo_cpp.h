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

/* Windows includes: */
#include <windows.h>
#ifdef __cplusplus_winrt
#include <agile.h>
#endif

/* SDL includes: */
#include "SDL_video.h"
#include "SDL_events.h"

extern "C" {
#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"
}


/* The global, WinRT, SDL Window.
   For now, SDL/WinRT only supports one window (due to platform limitations of
   WinRT.
*/
extern SDL_Window * WINRT_GlobalSDLWindow;

/* The global, WinRT, video device. */
extern SDL_VideoDevice * WINRT_GlobalSDLVideoDevice;

/* Creates a display mode for Plain Direct3D (non-XAML) apps, using the lone, native window's settings.

   Pass in an allocated SDL_DisplayMode field to store the data in.

   This function will return 0 on success, -1 on failure.

   If this function succeeds, be sure to call SDL_free on the
   SDL_DisplayMode's driverdata field.
*/
extern int WINRT_CalcDisplayModeUsingNativeWindow(SDL_DisplayMode * mode);

/* Duplicates a display mode, copying over driverdata as necessary */
extern int WINRT_DuplicateDisplayMode(SDL_DisplayMode * dest, const SDL_DisplayMode * src);

/* Display mode internals */
typedef struct
{
    Windows::Graphics::Display::DisplayOrientations currentOrientation;
} SDL_DisplayModeData;

#ifdef __cplusplus_winrt

/* Internal window data */
struct SDL_WindowData
{
    SDL_Window *sdlWindow;
    Platform::Agile<Windows::UI::Core::CoreWindow> coreWindow;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
};

#endif // ifdef __cplusplus_winrt
