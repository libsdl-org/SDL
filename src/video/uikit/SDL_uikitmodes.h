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
#include "SDL_internal.h"

#ifndef SDL_uikitmodes_h_
#define SDL_uikitmodes_h_

#include "SDL_uikitvideo.h"

@interface SDL_UIKitDisplayData : NSObject

#if !TARGET_OS_XR
- (instancetype)initWithScreen:(UIScreen *)screen;
@property(nonatomic, strong) UIScreen *uiscreen;
#endif

@end

@interface SDL_UIKitDisplayModeData : NSObject
#if !TARGET_OS_XR
@property(nonatomic, strong) UIScreenMode *uiscreenmode;
#endif

@end

#if !TARGET_OS_XR
extern SDL_bool UIKit_IsDisplayLandscape(UIScreen *uiscreen);
#endif

extern int UIKit_InitModes(SDL_VideoDevice *_this);
#if !TARGET_OS_XR
extern int UIKit_AddDisplay(UIScreen *uiscreen, SDL_bool send_event);
extern void UIKit_DelDisplay(UIScreen *uiscreen);
#endif
extern int UIKit_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display);
extern int UIKit_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
extern void UIKit_QuitModes(SDL_VideoDevice *_this);
extern int UIKit_GetDisplayUsableBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect);

// because visionOS does not have a screen
// we create a fake 1080p display to maintain compatibility.
#if TARGET_OS_XR
#define SDL_XR_SCREENWIDTH 1920
#define SDL_XR_SCREENHEIGHT 1080
#endif

#endif /* SDL_uikitmodes_h_ */
