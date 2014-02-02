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

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL_assert.h"
#include "SDL_uikitmodes.h"


BOOL SDL_UIKit_supports_multiple_displays = NO;


static int
UIKit_AllocateDisplayModeData(SDL_DisplayMode * mode,
    UIScreenMode * uiscreenmode, CGFloat scale)
{
    SDL_DisplayModeData *data = NULL;

    if (uiscreenmode != nil) {
        /* Allocate the display mode data */
        data = (SDL_DisplayModeData *) SDL_malloc(sizeof(*data));
        if (!data) {
            return SDL_OutOfMemory();
        }

        data->uiscreenmode = uiscreenmode;
        [data->uiscreenmode retain];

        data->scale = scale;
    }

    mode->driverdata = data;

    return 0;
}

static void
UIKit_FreeDisplayModeData(SDL_DisplayMode * mode)
{
    if (!SDL_UIKit_supports_multiple_displays) {
        /* Not on at least iPhoneOS 3.2 (versions prior to iPad). */
        SDL_assert(mode->driverdata == NULL);
    } else if (mode->driverdata != NULL) {
        SDL_DisplayModeData *data = (SDL_DisplayModeData *)mode->driverdata;
        [data->uiscreenmode release];
        SDL_free(data);
        mode->driverdata = NULL;
    }
}

static int
UIKit_AddSingleDisplayMode(SDL_VideoDisplay * display, int w, int h,
    UIScreenMode * uiscreenmode, CGFloat scale)
{
    SDL_DisplayMode mode;
    SDL_zero(mode);

    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.refresh_rate = 0;
    if (UIKit_AllocateDisplayModeData(&mode, uiscreenmode, scale) < 0) {
        return -1;
    }

    mode.w = w;
    mode.h = h;
    if (SDL_AddDisplayMode(display, &mode)) {
        return 0;
    } else {
        UIKit_FreeDisplayModeData(&mode);
        return -1;
    }
}

static int
UIKit_AddDisplayMode(SDL_VideoDisplay * display, int w, int h, CGFloat scale,
                     UIScreenMode * uiscreenmode, SDL_bool addRotation)
{
    if (UIKit_AddSingleDisplayMode(display, w, h, uiscreenmode, scale) < 0) {
        return -1;
    }

    if (addRotation) {
        /* Add the rotated version */
        if (UIKit_AddSingleDisplayMode(display, h, w, uiscreenmode, scale) < 0) {
            return -1;
        }
    }

    return 0;
}

static int
UIKit_AddDisplay(UIScreen *uiscreen)
{
    CGSize size = [uiscreen bounds].size;

    /* Make sure the width/height are oriented correctly */
    if (UIKit_IsDisplayLandscape(uiscreen) != (size.width > size.height)) {
        CGFloat height = size.width;
        size.width = size.height;
        size.height = height;
    }

    /* When dealing with UIKit all coordinates are specified in terms of
     * what Apple refers to as points. On earlier devices without the
     * so called "Retina" display, there is a one to one mapping between
     * points and pixels. In other cases [UIScreen scale] indicates the
     * relationship between points and pixels. Since SDL has no notion
     * of points, we must compensate in all cases where dealing with such
     * units.
     */
    CGFloat scale;
    if ([UIScreen instancesRespondToSelector:@selector(scale)]) {
        scale = [uiscreen scale]; /* iOS >= 4.0 */
    } else {
        scale = 1.0f; /* iOS < 4.0 */
    }

    SDL_VideoDisplay display;
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = (int)(size.width * scale);
    mode.h = (int)(size.height * scale);

    UIScreenMode * uiscreenmode = nil;
    /* UIScreenMode showed up in 3.2 (the iPad and later). We're
     * misusing this supports_multiple_displays flag here for that.
     */
    if (SDL_UIKit_supports_multiple_displays) {
        uiscreenmode = [uiscreen currentMode];
    }

    if (UIKit_AllocateDisplayModeData(&mode, uiscreenmode, scale) < 0) {
        return -1;
    }

    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;

    /* Allocate the display data */
    SDL_DisplayData *data = (SDL_DisplayData *) SDL_malloc(sizeof(*data));
    if (!data) {
        UIKit_FreeDisplayModeData(&display.desktop_mode);
        return SDL_OutOfMemory();
    }

    [uiscreen retain];
    data->uiscreen = uiscreen;
    data->scale = scale;

    display.driverdata = data;
    SDL_AddVideoDisplay(&display);

    return 0;
}

SDL_bool
UIKit_IsDisplayLandscape(UIScreen *uiscreen)
{
    if (uiscreen == [UIScreen mainScreen]) {
        return UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation]);
    } else {
        CGSize size = [uiscreen bounds].size;
        return (size.width > size.height);
    }
}

int
UIKit_InitModes(_THIS)
{
    /* this tells us whether we are running on ios >= 3.2 */
    SDL_UIKit_supports_multiple_displays = [UIScreen instancesRespondToSelector:@selector(currentMode)];

    /* Add the main screen. */
    if (UIKit_AddDisplay([UIScreen mainScreen]) < 0) {
        return -1;
    }

    /* If this is iPhoneOS < 3.2, all devices are one screen, 320x480 pixels. */
    /*  The iPad added both a larger main screen and the ability to use
     *  external displays. So, add the other displays (screens in UI speak).
     */
    if (SDL_UIKit_supports_multiple_displays) {
        for (UIScreen *uiscreen in [UIScreen screens]) {
            /* Only add the other screens */
            if (uiscreen != [UIScreen mainScreen]) {
                if (UIKit_AddDisplay(uiscreen) < 0) {
                    return -1;
                }
            }
        }
    }

    /* We're done! */
    return 0;
}

void
UIKit_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;

    SDL_bool isLandscape = UIKit_IsDisplayLandscape(data->uiscreen);
    SDL_bool addRotation = (data->uiscreen == [UIScreen mainScreen]);

    if (SDL_UIKit_supports_multiple_displays) {
        /* availableModes showed up in 3.2 (the iPad and later). We should only
         * land here for at least that version of the OS.
         */
        for (UIScreenMode *uimode in [data->uiscreen availableModes]) {
            CGSize size = [uimode size];
            int w = (int)size.width;
            int h = (int)size.height;

            /* Make sure the width/height are oriented correctly */
            if (isLandscape != (w > h)) {
                int tmp = w;
                w = h;
                h = tmp;
            }

            /* Add the native screen resolution. */
            UIKit_AddDisplayMode(display, w, h, data->scale, uimode, addRotation);

            if (data->scale != 1.0f) {
                /* Add the native screen resolution divided by its scale.
                 * This is so devices capable of e.g. 640x960 also advertise 320x480.
                 */
                UIKit_AddDisplayMode(display,
                    (int)(size.width / data->scale),
                    (int)(size.height / data->scale),
                    1.0f, uimode, addRotation);
            }
        }
    } else {
        const CGSize size = [data->uiscreen bounds].size;
        int w = (int)size.width;
        int h = (int)size.height;

        /* Make sure the width/height are oriented correctly */
        if (isLandscape != (w > h)) {
            int tmp = w;
            w = h;
            h = tmp;
        }

        UIKit_AddDisplayMode(display, w, h, 1.0f, nil, addRotation);
    }
}

int
UIKit_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;

    if (!SDL_UIKit_supports_multiple_displays) {
        /* Not on at least iPhoneOS 3.2 (versions prior to iPad). */
        SDL_assert(mode->driverdata == NULL);
    } else {
        SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;
        [data->uiscreen setCurrentMode:modedata->uiscreenmode];

        if (data->uiscreen == [UIScreen mainScreen]) {
            if (mode->w > mode->h) {
                if (!UIKit_IsDisplayLandscape(data->uiscreen)) {
                    [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeRight animated:NO];
                }
            } else if (mode->w < mode->h) {
                if (UIKit_IsDisplayLandscape(data->uiscreen)) {
                    [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationPortrait animated:NO];
                }
            }
        }
    }
    return 0;
}

void
UIKit_QuitModes(_THIS)
{
    /* Release Objective-C objects, so higher level doesn't free() them. */
    int i, j;
    for (i = 0; i < _this->num_displays; i++) {
        SDL_VideoDisplay *display = &_this->displays[i];

        UIKit_FreeDisplayModeData(&display->desktop_mode);
        for (j = 0; j < display->num_display_modes; j++) {
            SDL_DisplayMode *mode = &display->display_modes[j];
            UIKit_FreeDisplayModeData(mode);
        }

        SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;
        [data->uiscreen release];
        SDL_free(data);
        display->driverdata = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
