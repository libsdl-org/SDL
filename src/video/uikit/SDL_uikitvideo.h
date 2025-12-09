/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
#ifndef SDL_uikitvideo_h_
#define SDL_uikitvideo_h_

#include "../SDL_sysvideo.h"

#ifdef __OBJC__

#include <UIKit/UIKit.h>

@interface SDL_UIKitVideoData : NSObject

@property(nonatomic, assign) id pasteboardObserver;

@property(nonatomic, assign) bool setting_clipboard;

@end

#ifdef SDL_PLATFORM_VISIONOS
extern CGRect UIKit_ComputeViewFrame(SDL_Window *window);
#else
extern CGRect UIKit_ComputeViewFrame(SDL_Window *window, UIScreen *screen);
#endif

extern API_AVAILABLE(ios(13.0)) UIWindowScene *UIKit_GetActiveWindowScene(void);

extern void UIKit_SetGameControllerInteraction(bool enabled);
extern void UIKit_SetViewGameControllerInteraction(UIView *view, bool enabled);

#endif // __OBJC__

extern bool UIKit_SuspendScreenSaver(SDL_VideoDevice *_this);

extern void UIKit_ForceUpdateHomeIndicator(void);

extern bool UIKit_IsSystemVersionAtLeast(double version);

extern SDL_SystemTheme UIKit_GetSystemTheme(void);

#endif // SDL_uikitvideo_h_
