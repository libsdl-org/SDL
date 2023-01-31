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

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL_uikitmodes.h"

#include "../../events/SDL_events_c.h"

#import <sys/utsname.h>

@implementation SDL_DisplayData

- (instancetype)initWithScreen:(UIScreen *)screen
{
    if (self = [super init]) {
        self.uiscreen = screen;

        /*
         * A well up to date list of device info can be found here:
         * https://github.com/lmirosevic/GBDeviceInfo/blob/master/GBDeviceInfo/GBDeviceInfo_iOS.m
         */
        NSDictionary *devices = @{
            @"iPhone1,1" : @163,
            @"iPhone1,2" : @163,
            @"iPhone2,1" : @163,
            @"iPhone3,1" : @326,
            @"iPhone3,2" : @326,
            @"iPhone3,3" : @326,
            @"iPhone4,1" : @326,
            @"iPhone5,1" : @326,
            @"iPhone5,2" : @326,
            @"iPhone5,3" : @326,
            @"iPhone5,4" : @326,
            @"iPhone6,1" : @326,
            @"iPhone6,2" : @326,
            @"iPhone7,1" : @401,
            @"iPhone7,2" : @326,
            @"iPhone8,1" : @326,
            @"iPhone8,2" : @401,
            @"iPhone8,4" : @326,
            @"iPhone9,1" : @326,
            @"iPhone9,2" : @401,
            @"iPhone9,3" : @326,
            @"iPhone9,4" : @401,
            @"iPhone10,1" : @326,
            @"iPhone10,2" : @401,
            @"iPhone10,3" : @458,
            @"iPhone10,4" : @326,
            @"iPhone10,5" : @401,
            @"iPhone10,6" : @458,
            @"iPhone11,2" : @458,
            @"iPhone11,4" : @458,
            @"iPhone11,6" : @458,
            @"iPhone11,8" : @326,
            @"iPhone12,1" : @326,
            @"iPhone12,3" : @458,
            @"iPhone12,5" : @458,
            @"iPad1,1" : @132,
            @"iPad2,1" : @132,
            @"iPad2,2" : @132,
            @"iPad2,3" : @132,
            @"iPad2,4" : @132,
            @"iPad2,5" : @163,
            @"iPad2,6" : @163,
            @"iPad2,7" : @163,
            @"iPad3,1" : @264,
            @"iPad3,2" : @264,
            @"iPad3,3" : @264,
            @"iPad3,4" : @264,
            @"iPad3,5" : @264,
            @"iPad3,6" : @264,
            @"iPad4,1" : @264,
            @"iPad4,2" : @264,
            @"iPad4,3" : @264,
            @"iPad4,4" : @326,
            @"iPad4,5" : @326,
            @"iPad4,6" : @326,
            @"iPad4,7" : @326,
            @"iPad4,8" : @326,
            @"iPad4,9" : @326,
            @"iPad5,1" : @326,
            @"iPad5,2" : @326,
            @"iPad5,3" : @264,
            @"iPad5,4" : @264,
            @"iPad6,3" : @264,
            @"iPad6,4" : @264,
            @"iPad6,7" : @264,
            @"iPad6,8" : @264,
            @"iPad6,11" : @264,
            @"iPad6,12" : @264,
            @"iPad7,1" : @264,
            @"iPad7,2" : @264,
            @"iPad7,3" : @264,
            @"iPad7,4" : @264,
            @"iPad7,5" : @264,
            @"iPad7,6" : @264,
            @"iPad7,11" : @264,
            @"iPad7,12" : @264,
            @"iPad8,1" : @264,
            @"iPad8,2" : @264,
            @"iPad8,3" : @264,
            @"iPad8,4" : @264,
            @"iPad8,5" : @264,
            @"iPad8,6" : @264,
            @"iPad8,7" : @264,
            @"iPad8,8" : @264,
            @"iPad11,1" : @326,
            @"iPad11,2" : @326,
            @"iPad11,3" : @326,
            @"iPad11,4" : @326,
            @"iPod1,1" : @163,
            @"iPod2,1" : @163,
            @"iPod3,1" : @163,
            @"iPod4,1" : @326,
            @"iPod5,1" : @326,
            @"iPod7,1" : @326,
            @"iPod9,1" : @326,
        };

        struct utsname systemInfo;
        uname(&systemInfo);
        NSString *deviceName =
            [NSString stringWithCString:systemInfo.machine
                               encoding:NSUTF8StringEncoding];
        id foundDPI = devices[deviceName];
        if (foundDPI) {
            self.screenDPI = (float)[foundDPI integerValue];
        } else {
            /*
             * Estimate the DPI based on the screen scale multiplied by the base DPI for the device
             * type (e.g. based on iPhone 1 and iPad 1)
             */
            float scale = (float)screen.nativeScale;
            float defaultDPI;
            if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
                defaultDPI = 132.0f;
            } else if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone) {
                defaultDPI = 163.0f;
            } else {
                defaultDPI = 160.0f;
            }
            self.screenDPI = scale * defaultDPI;
        }
    }
    return self;
}

@synthesize uiscreen;
@synthesize screenDPI;

@end

@implementation SDL_DisplayModeData

@synthesize uiscreenmode;

@end

@interface SDL_DisplayWatch : NSObject
@end

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

static int UIKit_AllocateDisplayModeData(SDL_DisplayMode *mode,
                                         UIScreenMode *uiscreenmode)
{
    SDL_DisplayModeData *data = nil;

    if (uiscreenmode != nil) {
        /* Allocate the display mode data */
        data = [[SDL_DisplayModeData alloc] init];
        if (!data) {
            return SDL_OutOfMemory();
        }

        data.uiscreenmode = uiscreenmode;
    }

    mode->driverdata = (void *)CFBridgingRetain(data);

    return 0;
}

static void UIKit_FreeDisplayModeData(SDL_DisplayMode *mode)
{
    if (mode->driverdata != NULL) {
        CFRelease(mode->driverdata);
        mode->driverdata = NULL;
    }
}

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

    mode.pixel_w = w;
    mode.pixel_h = h;
    mode.display_scale = uiscreen.nativeScale;
    mode.refresh_rate = UIKit_GetDisplayModeRefreshRate(uiscreen);
    mode.format = SDL_PIXELFORMAT_ABGR8888;

    if (SDL_AddDisplayMode(display, &mode)) {
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

static CGSize GetUIScreenModePixelSize(UIScreen *uiscreen, UIScreenMode *mode)
{
    /* For devices such as iPhone 6/7/8 Plus, the UIScreenMode reported by iOS
     * isn't the physical pixels of the display, but rather the point size times
     * the scale. For example, on iOS 12.2 on iPhone 8 Plus the physical pixel
     * resolution is 1080x1920, the size reported by mode.size is 1242x2208,
     * the size in points is 414x736, the scale property is 3.0, and the
     * nativeScale property is ~2.6087 (ie 1920.0 / 736.0). So we need a bit of
     * math to convert from retina pixels (point size multiplied by scale) to
     * real pixels.
     * Note that the iOS Simulator doesn't have this behavior for those devices.
     * https://github.com/libsdl-org/SDL/issues/3220
     */
    CGSize size = mode.size;
    CGFloat scale = uiscreen.nativeScale / uiscreen.scale;

    size.width = SDL_round(size.width * scale);
    size.height = SDL_round(size.height * scale);

    return size;
}

int UIKit_AddDisplay(UIScreen *uiscreen, SDL_bool send_event)
{
    UIScreenMode *uiscreenmode = uiscreen.currentMode;
    CGSize size = GetUIScreenModePixelSize(uiscreen, uiscreenmode);
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    /* Make sure the width/height are oriented correctly */
    if (UIKit_IsDisplayLandscape(uiscreen) != (size.width > size.height)) {
        CGFloat height = size.width;
        size.width = size.height;
        size.height = height;
    }

    SDL_zero(mode);
    mode.pixel_w = (int)size.width;
    mode.pixel_h = (int)size.height;
    mode.display_scale = uiscreen.nativeScale;
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.refresh_rate = UIKit_GetDisplayModeRefreshRate(uiscreen);

    if (UIKit_AllocateDisplayModeData(&mode, uiscreenmode) < 0) {
        return -1;
    }

    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;

    /* Allocate the display data */
    SDL_DisplayData *data = [[SDL_DisplayData alloc] initWithScreen:uiscreen];
    if (!data) {
        UIKit_FreeDisplayModeData(&display.desktop_mode);
        return SDL_OutOfMemory();
    }

    display.driverdata = data;
    if (SDL_AddVideoDisplay(&display, send_event) == 0) {
        return -1;
    }
    return 0;
}

void UIKit_DelDisplay(UIScreen *uiscreen)
{
    SDL_DisplayID *displays;
    int i;

    displays = SDL_GetDisplays(NULL);
    if (displays) {
        for (i = 0; displays[i]; ++i) {
            SDL_VideoDisplay *display = SDL_GetVideoDisplay(displays[i]);
            SDL_DisplayData *data = display->driverdata;

            if (data && data.uiscreen == uiscreen) {
                display->driverdata = nil;
                SDL_DelVideoDisplay(displays[i]);
                return;
            }
        }
        SDL_free(displays);
    }
}

SDL_bool
UIKit_IsDisplayLandscape(UIScreen *uiscreen)
{
#if !TARGET_OS_TV
    if (uiscreen == [UIScreen mainScreen]) {
        return UIInterfaceOrientationIsLandscape([UIApplication sharedApplication].statusBarOrientation);
    } else
#endif /* !TARGET_OS_TV */
    {
        CGSize size = uiscreen.bounds.size;
        return (size.width > size.height);
    }
}

int UIKit_InitModes(_THIS)
{
    @autoreleasepool {
        for (UIScreen *uiscreen in [UIScreen screens]) {
            if (UIKit_AddDisplay(uiscreen, SDL_FALSE) < 0) {
                return -1;
            }
        }
#if !TARGET_OS_TV
        SDL_OnApplicationDidChangeStatusBarOrientation();
#endif

        [SDL_DisplayWatch start];
    }

    return 0;
}

void UIKit_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    @autoreleasepool {
        SDL_DisplayData *data = display->driverdata;

        SDL_bool isLandscape = UIKit_IsDisplayLandscape(data.uiscreen);
        SDL_bool addRotation = (data.uiscreen == [UIScreen mainScreen]);
        NSArray *availableModes = nil;

#if TARGET_OS_TV
        addRotation = SDL_FALSE;
        availableModes = @[ data.uiscreen.currentMode ];
#else
        availableModes = data.uiscreen.availableModes;
#endif

        for (UIScreenMode *uimode in availableModes) {
            CGSize size = GetUIScreenModePixelSize(data.uiscreen, uimode);
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
}

int UIKit_GetDisplayPhysicalDPI(_THIS, SDL_VideoDisplay *display, float *ddpi, float *hdpi, float *vdpi)
{
    @autoreleasepool {
        SDL_DisplayData *data = display->driverdata;
        float dpi = data.screenDPI;

        if (ddpi) {
            *ddpi = dpi * (float)SDL_sqrt(2.0);
        }
        if (hdpi) {
            *hdpi = dpi;
        }
        if (vdpi) {
            *vdpi = dpi;
        }
    }

    return 0;
}

int UIKit_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    @autoreleasepool {
        SDL_DisplayData *data = display->driverdata;

#if !TARGET_OS_TV
        SDL_DisplayModeData *modedata = (__bridge SDL_DisplayModeData *)mode->driverdata;
        [data.uiscreen setCurrentMode:modedata.uiscreenmode];
#endif

        if (data.uiscreen == [UIScreen mainScreen]) {
            /* [UIApplication setStatusBarOrientation:] no longer works reliably
             * in recent iOS versions, so we can't rotate the screen when setting
             * the display mode. */
            if (mode->pixel_w > mode->pixel_h) {
                if (!UIKit_IsDisplayLandscape(data.uiscreen)) {
                    return SDL_SetError("Screen orientation does not match display mode size");
                }
            } else if (mode->pixel_w < mode->pixel_h) {
                if (UIKit_IsDisplayLandscape(data.uiscreen)) {
                    return SDL_SetError("Screen orientation does not match display mode size");
                }
            }
        }
    }

    return 0;
}

int UIKit_GetDisplayUsableBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    @autoreleasepool {
        SDL_DisplayData *data = display->driverdata;
        CGRect frame = data.uiscreen.bounds;

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

void UIKit_QuitModes(_THIS)
{
    [SDL_DisplayWatch stop];

    /* Release Objective-C objects, so higher level doesn't free() them. */
    int i, j;
    @autoreleasepool {
        for (i = 0; i < _this->num_displays; i++) {
            SDL_VideoDisplay *display = &_this->displays[i];

            UIKit_FreeDisplayModeData(&display->desktop_mode);
            for (j = 0; j < display->num_display_modes; j++) {
                SDL_DisplayMode *mode = &display->display_modes[j];
                UIKit_FreeDisplayModeData(mode);
            }

            if (display->driverdata) {
                display->driverdata = nil;
            }
        }
    }
}

#if !TARGET_OS_TV
void SDL_OnApplicationDidChangeStatusBarOrientation()
{
    BOOL isLandscape = UIInterfaceOrientationIsLandscape([UIApplication sharedApplication].statusBarOrientation);
    SDL_VideoDisplay *display = SDL_GetVideoDisplay(SDL_GetPrimaryDisplay());

    if (display) {
        SDL_DisplayMode *desktopmode = &display->desktop_mode;
        SDL_DisplayMode *currentmode = &display->current_mode;
        SDL_DisplayOrientation orientation = SDL_ORIENTATION_UNKNOWN;

        /* The desktop display mode should be kept in sync with the screen
         * orientation so that updating a window's fullscreen state to
         * SDL_WINDOW_FULLSCREEN_DESKTOP keeps the window dimensions in the
         * correct orientation. */
        if (isLandscape != (desktopmode->pixel_w > desktopmode->pixel_h)) {
            int height = desktopmode->pixel_w;
            desktopmode->pixel_w = desktopmode->pixel_h;
            desktopmode->pixel_h = height;
            height = desktopmode->screen_w;
            desktopmode->screen_w = desktopmode->screen_h;
            desktopmode->screen_h = height;
        }

        /* Same deal with the current mode + SDL_GetCurrentDisplayMode. */
        if (isLandscape != (currentmode->pixel_w > currentmode->pixel_h)) {
            int height = currentmode->pixel_w;
            currentmode->pixel_w = currentmode->pixel_h;
            currentmode->pixel_h = height;
            height = currentmode->screen_w;
            currentmode->screen_w = currentmode->screen_h;
            currentmode->screen_h = height;
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
#endif /* !TARGET_OS_TV */

#endif /* SDL_VIDEO_DRIVER_UIKIT */
