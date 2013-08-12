/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#ifndef _SDL_uikitwindow_h
#define _SDL_uikitwindow_h

#include "../SDL_sysvideo.h"
#import "SDL_uikitvideo.h"
#import "SDL_uikitopenglview.h"
#import "SDL_uikitviewcontroller.h"

typedef struct SDL_WindowData SDL_WindowData;

extern int UIKit_CreateWindow(_THIS, SDL_Window * window);
extern void UIKit_ShowWindow(_THIS, SDL_Window * window);
extern void UIKit_HideWindow(_THIS, SDL_Window * window);
extern void UIKit_RaiseWindow(_THIS, SDL_Window * window);
extern void UIKit_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
extern void UIKit_DestroyWindow(_THIS, SDL_Window * window);
extern SDL_bool UIKit_GetWindowWMInfo(_THIS, SDL_Window * window,
                                      struct SDL_SysWMinfo * info);

@class UIWindow;

struct SDL_WindowData
{
    UIWindow *uiwindow;
    SDL_uikitopenglview *view;
    SDL_uikitviewcontroller *viewcontroller;
};

#endif /* _SDL_uikitwindow_h */

/* vi: set ts=4 sw=4 expandtab: */
