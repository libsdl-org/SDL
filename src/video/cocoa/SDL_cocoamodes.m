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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_COCOA

#include "SDL_cocoavideo.h"

/* We need this for IODisplayCreateInfoDictionary and kIODisplayOnlyPreferredName */
#include <IOKit/graphics/IOGraphicsLib.h>

/* We need this for CVDisplayLinkGetNominalOutputVideoRefreshPeriod */
#include <CoreVideo/CVBase.h>
#include <CoreVideo/CVDisplayLink.h>

/* we need this for ShowMenuBar() and HideMenuBar(). */
#include <Carbon/Carbon.h>

/* This gets us MAC_OS_X_VERSION_MIN_REQUIRED... */
#include <AvailabilityMacros.h>


static void
Cocoa_ToggleMenuBar(const BOOL show)
{
    /* !!! FIXME: keep an eye on this.
     * ShowMenuBar/HideMenuBar is officially unavailable for 64-bit binaries.
     *  It happens to work, as of 10.7, but we're going to see if
     *  we can just simply do without it on newer OSes...
     */
#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1070) && !defined(__LP64__)
    if (show) {
        ShowMenuBar();
    } else {
        HideMenuBar();
    }
#endif
}


/* !!! FIXME: clean out the pre-10.6 code when it makes sense to do so. */
#define FORCE_OLD_API 0

#if FORCE_OLD_API
#undef MAC_OS_X_VERSION_MIN_REQUIRED
#define MAC_OS_X_VERSION_MIN_REQUIRED 1050
#endif

static BOOL
IS_SNOW_LEOPARD_OR_LATER()
{
#if FORCE_OLD_API
    return NO;
#else
    return floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_5;
#endif
}

static int
CG_SetError(const char *prefix, CGDisplayErr result)
{
    const char *error;

    switch (result) {
    case kCGErrorFailure:
        error = "kCGErrorFailure";
        break;
    case kCGErrorIllegalArgument:
        error = "kCGErrorIllegalArgument";
        break;
    case kCGErrorInvalidConnection:
        error = "kCGErrorInvalidConnection";
        break;
    case kCGErrorInvalidContext:
        error = "kCGErrorInvalidContext";
        break;
    case kCGErrorCannotComplete:
        error = "kCGErrorCannotComplete";
        break;
    case kCGErrorNotImplemented:
        error = "kCGErrorNotImplemented";
        break;
    case kCGErrorRangeCheck:
        error = "kCGErrorRangeCheck";
        break;
    case kCGErrorTypeCheck:
        error = "kCGErrorTypeCheck";
        break;
    case kCGErrorInvalidOperation:
        error = "kCGErrorInvalidOperation";
        break;
    case kCGErrorNoneAvailable:
        error = "kCGErrorNoneAvailable";
        break;
    default:
        error = "Unknown Error";
        break;
    }
    return SDL_SetError("%s: %s", prefix, error);
}

static SDL_bool
GetDisplayMode(_THIS, const void *moderef, CVDisplayLinkRef link, SDL_DisplayMode *mode)
{
    SDL_DisplayModeData *data;
    long width = 0;
    long height = 0;
    long bpp = 0;
    long refreshRate = 0;

    data = (SDL_DisplayModeData *) SDL_malloc(sizeof(*data));
    if (!data) {
        return SDL_FALSE;
    }
    data->moderef = moderef;

    if (IS_SNOW_LEOPARD_OR_LATER()) {
        CGDisplayModeRef vidmode = (CGDisplayModeRef) moderef;
        CFStringRef fmt = CGDisplayModeCopyPixelEncoding(vidmode);
        width = (long) CGDisplayModeGetWidth(vidmode);
        height = (long) CGDisplayModeGetHeight(vidmode);
        refreshRate = (long) (CGDisplayModeGetRefreshRate(vidmode) + 0.5);

        if (CFStringCompare(fmt, CFSTR(IO32BitDirectPixels),
                            kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            bpp = 32;
        } else if (CFStringCompare(fmt, CFSTR(IO16BitDirectPixels),
                            kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            bpp = 16;
        } else if (CFStringCompare(fmt, CFSTR(kIO30BitDirectPixels),
                            kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            bpp = 30;
        } else {
            bpp = 0;  /* ignore 8-bit and such for now. */
        }

        CFRelease(fmt);
    }

    #if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    if (!IS_SNOW_LEOPARD_OR_LATER()) {
        CFNumberRef number;
        double refresh;
        CFDictionaryRef vidmode = (CFDictionaryRef) moderef;
        number = CFDictionaryGetValue(vidmode, kCGDisplayWidth);
        CFNumberGetValue(number, kCFNumberLongType, &width);
        number = CFDictionaryGetValue(vidmode, kCGDisplayHeight);
        CFNumberGetValue(number, kCFNumberLongType, &height);
        number = CFDictionaryGetValue(vidmode, kCGDisplayBitsPerPixel);
        CFNumberGetValue(number, kCFNumberLongType, &bpp);
        number = CFDictionaryGetValue(vidmode, kCGDisplayRefreshRate);
        CFNumberGetValue(number, kCFNumberDoubleType, &refresh);
        refreshRate = (long) (refresh + 0.5);
    }
    #endif

    /* CGDisplayModeGetRefreshRate returns 0 for many non-CRT displays. */
    if (refreshRate == 0 && link != NULL) {
        CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
        if ((time.flags & kCVTimeIsIndefinite) == 0 && time.timeValue != 0) {
            refreshRate = (long) ((time.timeScale / (double) time.timeValue) + 0.5);
        }
    }

    mode->format = SDL_PIXELFORMAT_UNKNOWN;
    switch (bpp) {
    case 16:
        mode->format = SDL_PIXELFORMAT_ARGB1555;
        break;
    case 30:
        mode->format = SDL_PIXELFORMAT_ARGB2101010;
        break;
    case 32:
        mode->format = SDL_PIXELFORMAT_ARGB8888;
        break;
    case 8: /* We don't support palettized modes now */
    default: /* Totally unrecognizable bit depth. */
        return SDL_FALSE;
    }
    mode->w = width;
    mode->h = height;
    mode->refresh_rate = refreshRate;
    mode->driverdata = data;
    return SDL_TRUE;
}

static void
Cocoa_ReleaseDisplayMode(_THIS, const void *moderef)
{
    if (IS_SNOW_LEOPARD_OR_LATER()) {
        CGDisplayModeRelease((CGDisplayModeRef) moderef);  /* NULL is ok */
    }
}

static void
Cocoa_ReleaseDisplayModeList(_THIS, CFArrayRef modelist)
{
    if (IS_SNOW_LEOPARD_OR_LATER()) {
        CFRelease(modelist);  /* NULL is ok */
    }
}

static const char *
Cocoa_GetDisplayName(CGDirectDisplayID displayID)
{
    CFDictionaryRef deviceInfo = IODisplayCreateInfoDictionary(CGDisplayIOServicePort(displayID), kIODisplayOnlyPreferredName);
    NSDictionary *localizedNames = [(NSDictionary *)deviceInfo objectForKey:[NSString stringWithUTF8String:kDisplayProductName]];
    const char* displayName = NULL;

    if ([localizedNames count] > 0) {
        displayName = SDL_strdup([[localizedNames objectForKey:[[localizedNames allKeys] objectAtIndex:0]] UTF8String]);
    }
    CFRelease(deviceInfo);
    return displayName;
}

void
Cocoa_InitModes(_THIS)
{ @autoreleasepool
{
    CGDisplayErr result;
    CGDirectDisplayID *displays;
    CGDisplayCount numDisplays;
    int pass, i;

    result = CGGetOnlineDisplayList(0, NULL, &numDisplays);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGGetOnlineDisplayList()", result);
        return;
    }
    displays = SDL_stack_alloc(CGDirectDisplayID, numDisplays);
    result = CGGetOnlineDisplayList(numDisplays, displays, &numDisplays);
    if (result != kCGErrorSuccess) {
        CG_SetError("CGGetOnlineDisplayList()", result);
        SDL_stack_free(displays);
        return;
    }

    /* Pick up the primary display in the first pass, then get the rest */
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < numDisplays; ++i) {
            SDL_VideoDisplay display;
            SDL_DisplayData *displaydata;
            SDL_DisplayMode mode;
            const void *moderef = NULL;
            CVDisplayLinkRef link = NULL;

            if (pass == 0) {
                if (!CGDisplayIsMain(displays[i])) {
                    continue;
                }
            } else {
                if (CGDisplayIsMain(displays[i])) {
                    continue;
                }
            }

            if (CGDisplayMirrorsDisplay(displays[i]) != kCGNullDirectDisplay) {
                continue;
            }

            if (IS_SNOW_LEOPARD_OR_LATER()) {
                moderef = CGDisplayCopyDisplayMode(displays[i]);
            }

            #if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
            if (!IS_SNOW_LEOPARD_OR_LATER()) {
                moderef = CGDisplayCurrentMode(displays[i]);
            }
            #endif

            if (!moderef) {
                continue;
            }

            displaydata = (SDL_DisplayData *) SDL_malloc(sizeof(*displaydata));
            if (!displaydata) {
                Cocoa_ReleaseDisplayMode(_this, moderef);
                continue;
            }
            displaydata->display = displays[i];

            CVDisplayLinkCreateWithCGDisplay(displays[i], &link);

            SDL_zero(display);
            /* this returns a stddup'ed string */
            display.name = (char *)Cocoa_GetDisplayName(displays[i]);
            if (!GetDisplayMode(_this, moderef, link, &mode)) {
                CVDisplayLinkRelease(link);
                Cocoa_ReleaseDisplayMode(_this, moderef);
                SDL_free(display.name);
                SDL_free(displaydata);
                continue;
            }

            CVDisplayLinkRelease(link);

            display.desktop_mode = mode;
            display.current_mode = mode;
            display.driverdata = displaydata;
            SDL_AddVideoDisplay(&display);
            SDL_free(display.name);
        }
    }
    SDL_stack_free(displays);
}}

int
Cocoa_GetDisplayBounds(_THIS, SDL_VideoDisplay * display, SDL_Rect * rect)
{
    SDL_DisplayData *displaydata = (SDL_DisplayData *) display->driverdata;
    CGRect cgrect;

    cgrect = CGDisplayBounds(displaydata->display);
    rect->x = (int)cgrect.origin.x;
    rect->y = (int)cgrect.origin.y;
    rect->w = (int)cgrect.size.width;
    rect->h = (int)cgrect.size.height;
    return 0;
}

void
Cocoa_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;
    CFArrayRef modes = NULL;

    if (IS_SNOW_LEOPARD_OR_LATER()) {
        modes = CGDisplayCopyAllDisplayModes(data->display, NULL);
    }

    #if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    if (!IS_SNOW_LEOPARD_OR_LATER()) {
        modes = CGDisplayAvailableModes(data->display);
    }
    #endif

    if (modes) {
        CVDisplayLinkRef link = NULL;
        const CFIndex count = CFArrayGetCount(modes);
        CFIndex i;

        CVDisplayLinkCreateWithCGDisplay(data->display, &link);

        for (i = 0; i < count; i++) {
            const void *moderef = CFArrayGetValueAtIndex(modes, i);
            SDL_DisplayMode mode;
            if (GetDisplayMode(_this, moderef, link, &mode)) {
                if (IS_SNOW_LEOPARD_OR_LATER()) {
                    CGDisplayModeRetain((CGDisplayModeRef) moderef);
                }
                SDL_AddDisplayMode(display, &mode);
            }
        }

        CVDisplayLinkRelease(link);
        Cocoa_ReleaseDisplayModeList(_this, modes);
    }
}

static CGError
Cocoa_SwitchMode(_THIS, CGDirectDisplayID display, const void *mode)
{
    if (IS_SNOW_LEOPARD_OR_LATER()) {
        return CGDisplaySetDisplayMode(display, (CGDisplayModeRef) mode, NULL);
    }
 
    #if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    if (!IS_SNOW_LEOPARD_OR_LATER()) {
        return CGDisplaySwitchToMode(display, (CFDictionaryRef) mode);
    }
    #endif

    return kCGErrorFailure;
}

int
Cocoa_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    SDL_DisplayData *displaydata = (SDL_DisplayData *) display->driverdata;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) mode->driverdata;
    CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
    CGError result;

    /* Fade to black to hide resolution-switching flicker */
    if (CGAcquireDisplayFadeReservation(5, &fade_token) == kCGErrorSuccess) {
        CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
    }

    if (data == display->desktop_mode.driverdata) {
        /* Restoring desktop mode */
        Cocoa_SwitchMode(_this, displaydata->display, data->moderef);

        if (CGDisplayIsMain(displaydata->display)) {
            CGReleaseAllDisplays();
        } else {
            CGDisplayRelease(displaydata->display);
        }

        if (CGDisplayIsMain(displaydata->display)) {
            Cocoa_ToggleMenuBar(YES);
        }
    } else {
        /* Put up the blanking window (a window above all other windows) */
        if (CGDisplayIsMain(displaydata->display)) {
            /* If we don't capture all displays, Cocoa tries to rearrange windows... *sigh* */
            result = CGCaptureAllDisplays();
        } else {
            result = CGDisplayCapture(displaydata->display);
        }
        if (result != kCGErrorSuccess) {
            CG_SetError("CGDisplayCapture()", result);
            goto ERR_NO_CAPTURE;
        }

        /* Do the physical switch */
        result = Cocoa_SwitchMode(_this, displaydata->display, data->moderef);
        if (result != kCGErrorSuccess) {
            CG_SetError("CGDisplaySwitchToMode()", result);
            goto ERR_NO_SWITCH;
        }

        /* Hide the menu bar so it doesn't intercept events */
        if (CGDisplayIsMain(displaydata->display)) {
            Cocoa_ToggleMenuBar(NO);
        }
    }

    /* Fade in again (asynchronously) */
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade(fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(fade_token);
    }

    return 0;

    /* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NO_SWITCH:
    CGDisplayRelease(displaydata->display);
ERR_NO_CAPTURE:
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade (fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
        CGReleaseDisplayFadeReservation(fade_token);
    }
    return -1;
}

void
Cocoa_QuitModes(_THIS)
{
    int i, j;

    for (i = 0; i < _this->num_displays; ++i) {
        SDL_VideoDisplay *display = &_this->displays[i];
        SDL_DisplayModeData *mode;

        if (display->current_mode.driverdata != display->desktop_mode.driverdata) {
            Cocoa_SetDisplayMode(_this, display, &display->desktop_mode);
        }

        mode = (SDL_DisplayModeData *) display->desktop_mode.driverdata;
        Cocoa_ReleaseDisplayMode(_this, mode->moderef);

        for (j = 0; j < display->num_display_modes; j++) {
            mode = (SDL_DisplayModeData*) display->display_modes[j].driverdata;
            Cocoa_ReleaseDisplayMode(_this, mode->moderef);
        }

    }
    Cocoa_ToggleMenuBar(YES);
}

#endif /* SDL_VIDEO_DRIVER_COCOA */

/* vi: set ts=4 sw=4 expandtab: */
