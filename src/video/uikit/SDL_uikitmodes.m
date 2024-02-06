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

#ifdef SDL_VIDEO_DRIVER_UIKIT

#include "SDL_uikitmodes.h"

#include "../../events/SDL_events_c.h"

#import <sys/utsname.h>

@implementation SDL_UIKitDisplayData

#ifndef SDL_PLATFORM_VISIONOS
- (instancetype)initWithScreen:(UIScreen *)screen
{
    if (self = [super init]) {
        self.uiscreen = screen;
    }
    return self;
}
@synthesize uiscreen;
#endif

@end

@implementation SDL_UIKitDisplayModeData

#ifndef SDL_PLATFORM_VISIONOS
@synthesize uiscreenmode;
#endif

@end

@interface SDL_DisplayWatch : NSObject
@end

#ifndef SDL_PLATFORM_VISIONOS
@implementation SDL_DisplayWatch

+ (void)start
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(screenConnected:)
                   name:UIScreenDidConnectNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(screenDisconnected:)
                   name:UIScreenDidDisconnectNotification
                 object:nil];
}

+ (void)stop
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    [center removeObserver:self
                      name:UIScreenDidConnectNotification
                    object:nil];
    [center removeObserver:self
                      name:UIScreenDidDisconnectNotification
                    object:nil];
}

+ (void)screenConnected:(NSNotification *)notification
{
    UIScreen *uiscreen = [notification object];
    UIKit_AddDisplay(uiscreen, SDL_TRUE);
}

+ (void)screenDisconnected:(NSNotification *)notification
{
    UIScreen *uiscreen = [notification object];
    UIKit_DelDisplay(uiscreen);
}

@end
#endif

#ifndef SDL_PLATFORM_VISIONOS
static int UIKit_AllocateDisplayModeData(SDL_DisplayMode *mode,
                                         UIScreenMode *uiscreenmode)
{
    SDL_UIKitDisplayModeData *data = nil;

    if (uiscreenmode != nil) {
        /* Allocate the display mode data */
        data = [[SDL_UIKitDisplayModeData alloc] init];
        if (!data) {
            return SDL_OutOfMemory();
        }

        data.uiscreenmode = uiscreenmode;
    }

    mode->driverdata = (void *)CFBridgingRetain(data);

    return 0;
}
#endif

static void UIKit_FreeDisplayModeData(SDL_DisplayMode *mode)
{
    if (mode->driverdata != NULL) {
        CFRelease(mode->driverdata);
        mode->driverdata = NULL;
    }
}

#ifndef SDL_PLATFORM_VISIONOS
static float UIKit_GetDisplayModeRefreshRate(UIScreen *uiscreen)
{
#ifdef __IPHONE_10_3
    if ([uiscreen respondsToSelector:@selector(maximumFramesPerSecond)]) {
        return (float)uiscreen.maximumFramesPerSecond;
    }
#endif
    return 0.0f;
}

static int UIKit_AddSingleDisplayMode(SDL_VideoDisplay *display, int w, int h,
                                      UIScreen *uiscreen, UIScreenMode *uiscreenmode)
{
    SDL_DisplayMode mode;

    SDL_zero(mode);
    if (UIKit_AllocateDisplayModeData(&mode, uiscreenmode) < 0) {
        return -1;
    }

    mode.w = w;
    mode.h = h;
    mode.pixel_density = uiscreen.nativeScale;
    mode.refresh_rate = UIKit_GetDisplayModeRefreshRate(uiscreen);
    mode.format = SDL_PIXELFORMAT_ABGR8888;

    if (SDL_AddFullscreenDisplayMode(display, &mode)) {
        return 0;
    } else {
        UIKit_FreeDisplayModeData(&mode);
        return -1;
    }
}

static int UIKit_AddDisplayMode(SDL_VideoDisplay *display, int w, int h,
                                UIScreen *uiscreen, UIScreenMode *uiscreenmode, SDL_bool addRotation)
{
    if (UIKit_AddSingleDisplayMode(display, w, h, uiscreen, uiscreenmode) < 0) {
        return -1;
    }

    if (addRotation) {
        /* Add the rotated version */
        if (UIKit_AddSingleDisplayMode(display, h, w, uiscreen, uiscreenmode) < 0) {
            return -1;
        }
    }

    return 0;
}

static CGSize GetUIScreenModeSize(UIScreen *uiscreen, UIScreenMode *mode)
{
    /* For devices such as iPhone 6/7/8 Plus, the UIScreenMode reported by iOS
     * isn't the physical pixels of the display, but rather the point size times
     * the scale. For example, on iOS 12.2 on iPhone 8 Plus the physical pixel
     * resolution is 1080x1920, the size reported by mode.size is 1242x2208,
     * the size in points is 414x736, the scale property is 3.0, and the
     * nativeScale property is ~2.6087 (ie 1920.0 / 736.0).
     *
     * What we want for the mode size is the point size, and the pixel density
     * is the native scale.
     *
     * Note that the iOS Simulator doesn't have this behavior for those devices.
     * https://github.com/libsdl-org/SDL/issues/3220
     */
    CGSize size = mode.size;

    size.width = SDL_round(size.width / uiscreen.scale);
    size.height = SDL_round(size.height / uiscreen.scale);

    return size;
}

int UIKit_AddDisplay(UIScreen *uiscreen, SDL_bool send_event)
{
    UIScreenMode *uiscreenmode = uiscreen.currentMode;
    CGSize size = GetUIScreenModeSize(uiscreen, uiscreenmode);
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    /* Make sure the width/height are oriented correctly */
    if (UIKit_IsDisplayLandscape(uiscreen) != (size.width > size.height)) {
        CGFloat height = size.width;
        size.width = size.height;
        size.height = height;
    }

    SDL_zero(mode);
    mode.w = (int)size.width;
    mode.h = (int)size.height;
    mode.pixel_density = uiscreen.nativeScale;
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.refresh_rate = UIKit_GetDisplayModeRefreshRate(uiscreen);

    if (UIKit_AllocateDisplayModeData(&mode, uiscreenmode) < 0) {
        return -1;
    }

    SDL_zero(display);
#ifndef SDL_PLATFORM_TVOS
    if (uiscreen == [UIScreen mainScreen]) {
        /* The natural orientation (used by sensors) is portrait */
        display.natural_orientation = SDL_ORIENTATION_PORTRAIT;
    } else
#endif
    if (UIKit_IsDisplayLandscape(uiscreen)) {
        display.natural_orientation = SDL_ORIENTATION_LANDSCAPE;
    } else {
        display.natural_orientation = SDL_ORIENTATION_PORTRAIT;
    }
    display.desktop_mode = mode;

#ifndef SDL_PLATFORM_TVOS
    if (@available(iOS 16.0, *)) {
        if (uiscreen.potentialEDRHeadroom > 1.0f) {
            display.HDR.enabled = SDL_TRUE;
            display.HDR.SDR_whitelevel = 80.0f; /* SDR content is always at scRGB 1.0 */
        }
    }
#endif /* !SDL_PLATFORM_TVOS */

    /* Allocate the display data */
#ifdef SDL_PLATFORM_VISIONOS
    SDL_UIKitDisplayData *data = [[SDL_UIKitDisplayData alloc] init];
#else
    SDL_UIKitDisplayData *data = [[SDL_UIKitDisplayData alloc] initWithScreen:uiscreen];
#endif
    if (!data) {
        UIKit_FreeDisplayModeData(&display.desktop_mode);
        return SDL_OutOfMemory();
    }

    display.driverdata = (SDL_DisplayData *)CFBridgingRetain(data);
    if (SDL_AddVideoDisplay(&display, send_event) == 0) {
        return -1;
    }
    return 0;
}
#endif

#ifdef SDL_PLATFORM_VISIONOS
int UIKit_AddDisplay(SDL_bool send_event){
    CGSize size = CGSizeMake(SDL_XR_SCREENWIDTH, SDL_XR_SCREENHEIGHT);
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    SDL_zero(mode);
    mode.w = (int)size.width;
    mode.h = (int)size.height;
    mode.pixel_density = 1;
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.refresh_rate = 60;

    display.natural_orientation = SDL_ORIENTATION_LANDSCAPE;

    display.desktop_mode = mode;

    SDL_UIKitDisplayData *data = [[SDL_UIKitDisplayData alloc] init];

    if (!data) {
        UIKit_FreeDisplayModeData(&display.desktop_mode);
        return SDL_OutOfMemory();
    }

    display.driverdata = (SDL_DisplayData *)CFBridgingRetain(data);
    if (SDL_AddVideoDisplay(&display, send_event) == 0) {
        return -1;
    }
    return 0;
}
#endif

#ifndef SDL_PLATFORM_VISIONOS

void UIKit_DelDisplay(UIScreen *uiscreen)
{
    SDL_DisplayID *displays;
    int i;

    displays = SDL_GetDisplays(NULL);
    if (displays) {
        for (i = 0; displays[i]; ++i) {
            SDL_VideoDisplay *display = SDL_GetVideoDisplay(displays[i]);
            SDL_UIKitDisplayData *data = (__bridge SDL_UIKitDisplayData *)display->driverdata;

            if (data && data.uiscreen == uiscreen) {
                CFRelease(display->driverdata);
                SDL_DelVideoDisplay(displays[i], SDL_FALSE);
                break;
            }
        }
        SDL_free(displays);
    }
}

SDL_bool UIKit_IsDisplayLandscape(UIScreen *uiscreen)
{
#ifndef SDL_PLATFORM_TVOS
    if (uiscreen == [UIScreen mainScreen]) {
        return UIInterfaceOrientationIsLandscape([UIApplication sharedApplication].statusBarOrientation);
    } else
#endif /* !SDL_PLATFORM_TVOS */
    {
        CGSize size = uiscreen.bounds.size;
        return (size.width > size.height);
    }
}
#endif
int UIKit_InitModes(SDL_VideoDevice *_this)
{
    @autoreleasepool {
#ifdef SDL_PLATFORM_VISIONOS
        UIKit_AddDisplay(SDL_FALSE);
#else
        for (UIScreen *uiscreen in [UIScreen screens]) {
            if (UIKit_AddDisplay(uiscreen, SDL_FALSE) < 0) {
                return -1;
            }
        }
#endif

#if !defined(SDL_PLATFORM_TVOS) && !defined(SDL_PLATFORM_VISIONOS)
        SDL_OnApplicationDidChangeStatusBarOrientation();
#endif

#ifndef SDL_PLATFORM_VISIONOS
        [SDL_DisplayWatch start];
#endif
    }

    return 0;
}

int UIKit_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display)
{
#ifndef SDL_PLATFORM_VISIONOS
    @autoreleasepool {
        SDL_UIKitDisplayData *data = (__bridge SDL_UIKitDisplayData *)display->driverdata;

        SDL_bool isLandscape = UIKit_IsDisplayLandscape(data.uiscreen);
        SDL_bool addRotation = (data.uiscreen == [UIScreen mainScreen]);
        NSArray *availableModes = nil;

#ifdef SDL_PLATFORM_TVOS
        addRotation = SDL_FALSE;
        availableModes = @[ data.uiscreen.currentMode ];
#else
        availableModes = data.uiscreen.availableModes;
#endif

        for (UIScreenMode *uimode in availableModes) {
            CGSize size = GetUIScreenModeSize(data.uiscreen, uimode);
            int w = size.width;
            int h = size.height;

            /* Make sure the width/height are oriented correctly */
            if (isLandscape != (w > h)) {
                int tmp = w;
                w = h;
                h = tmp;
            }

            UIKit_AddDisplayMode(display, w, h, data.uiscreen, uimode, addRotation);
        }
    }
#endif
    return 0;
}

int UIKit_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
#ifndef SDL_PLATFORM_VISIONOS
    @autoreleasepool {
        SDL_UIKitDisplayData *data = (__bridge SDL_UIKitDisplayData *)display->driverdata;

#ifndef SDL_PLATFORM_TVOS
        SDL_UIKitDisplayModeData *modedata = (__bridge SDL_UIKitDisplayModeData *)mode->driverdata;
        [data.uiscreen setCurrentMode:modedata.uiscreenmode];
#endif

        if (data.uiscreen == [UIScreen mainScreen]) {
            /* [UIApplication setStatusBarOrientation:] no longer works reliably
             * in recent iOS versions, so we can't rotate the screen when setting
             * the display mode. */
            if (mode->w > mode->h) {
                if (!UIKit_IsDisplayLandscape(data.uiscreen)) {
                    return SDL_SetError("Screen orientation does not match display mode size");
                }
            } else if (mode->w < mode->h) {
                if (UIKit_IsDisplayLandscape(data.uiscreen)) {
                    return SDL_SetError("Screen orientation does not match display mode size");
                }
            }
        }
    }
#endif
    return 0;
}

int UIKit_GetDisplayUsableBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    @autoreleasepool {
        SDL_UIKitDisplayData *data = (__bridge SDL_UIKitDisplayData *)display->driverdata;
#ifdef SDL_PLATFORM_VISIONOS
        CGRect frame = CGRectMake(0, 0, SDL_XR_SCREENWIDTH, SDL_XR_SCREENHEIGHT);
#else
        CGRect frame = data.uiscreen.bounds;
#endif

        /* the default function iterates displays to make a fake offset,
         as if all the displays were side-by-side, which is fine for iOS. */
        if (SDL_GetDisplayBounds(display->id, rect) < 0) {
            return -1;
        }

        rect->x += frame.origin.x;
        rect->y += frame.origin.y;
        rect->w = frame.size.width;
        rect->h = frame.size.height;
    }

    return 0;
}

void UIKit_QuitModes(SDL_VideoDevice *_this)
{
#ifndef SDL_PLATFORM_VISIONOS
    [SDL_DisplayWatch stop];
#endif

    /* Release Objective-C objects, so higher level doesn't free() them. */
    int i, j;
    @autoreleasepool {
        for (i = 0; i < _this->num_displays; i++) {
            SDL_VideoDisplay *display = _this->displays[i];

            UIKit_FreeDisplayModeData(&display->desktop_mode);
            for (j = 0; j < display->num_fullscreen_modes; j++) {
                SDL_DisplayMode *mode = &display->fullscreen_modes[j];
                UIKit_FreeDisplayModeData(mode);
            }

            if (display->driverdata != NULL) {
                CFRelease(display->driverdata);
                display->driverdata = NULL;
            }
        }
    }
}

#if !defined(SDL_PLATFORM_TVOS) && !defined(SDL_PLATFORM_VISIONOS)
void SDL_OnApplicationDidChangeStatusBarOrientation(void)
{
    BOOL isLandscape = UIInterfaceOrientationIsLandscape([UIApplication sharedApplication].statusBarOrientation);
    SDL_VideoDisplay *display = SDL_GetVideoDisplay(SDL_GetPrimaryDisplay());

    if (display) {
        SDL_DisplayMode *mode = &display->desktop_mode;
        SDL_DisplayOrientation orientation = SDL_ORIENTATION_UNKNOWN;
        int i;

        /* The desktop display mode should be kept in sync with the screen
         * orientation so that updating a window's fullscreen state to
         * fullscreen desktop keeps the window dimensions in the
         * correct orientation. */
        if (isLandscape != (mode->w > mode->h)) {
            int height = mode->w;
            mode->w = mode->h;
            mode->h = height;
        }

        /* Same deal with the fullscreen modes */
        for (i = 0; i < display->num_fullscreen_modes; ++i) {
            mode = &display->fullscreen_modes[i];
            if (isLandscape != (mode->w > mode->h)) {
                int height = mode->w;
                mode->w = mode->h;
                mode->h = height;
            }
        }

        switch ([UIApplication sharedApplication].statusBarOrientation) {
        case UIInterfaceOrientationPortrait:
            orientation = SDL_ORIENTATION_PORTRAIT;
            break;
        case UIInterfaceOrientationPortraitUpsideDown:
            orientation = SDL_ORIENTATION_PORTRAIT_FLIPPED;
            break;
        case UIInterfaceOrientationLandscapeLeft:
            /* Bug: UIInterfaceOrientationLandscapeLeft/Right are reversed - http://openradar.appspot.com/7216046 */
            orientation = SDL_ORIENTATION_LANDSCAPE_FLIPPED;
            break;
        case UIInterfaceOrientationLandscapeRight:
            /* Bug: UIInterfaceOrientationLandscapeLeft/Right are reversed - http://openradar.appspot.com/7216046 */
            orientation = SDL_ORIENTATION_LANDSCAPE;
            break;
        default:
            break;
        }
        SDL_SendDisplayEvent(display, SDL_EVENT_DISPLAY_ORIENTATION, orientation);
    }
}
#endif /* !SDL_PLATFORM_TVOS */

#endif /* SDL_VIDEO_DRIVER_UIKIT */
