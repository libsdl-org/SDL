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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL_uikitview.h"

#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../events/SDL_events_c.h"

#import "SDL_uikitappdelegate.h"
#import "SDL_uikitmodes.h"
#import "SDL_uikitwindow.h"

@implementation SDL_uikitview {
    SDL_Window *sdlwindow;

    SDL_TouchID touchId;
    UITouch * __weak firstFingerDown;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.autoresizesSubviews = YES;

        self.multipleTouchEnabled = YES;

        touchId = 1;
        SDL_AddTouch(touchId, "");
    }

    return self;
}

- (void)setSDLWindow:(SDL_Window *)window
{
    SDL_WindowData *data = nil;

    if (window == sdlwindow) {
        return;
    }

    /* Remove ourself from the old window. */
    if (sdlwindow) {
        SDL_uikitview *view = nil;
        data = (__bridge SDL_WindowData *) sdlwindow->driverdata;

        [data.views removeObject:self];

        [self removeFromSuperview];

        /* Restore the next-oldest view in the old window. */
        view = data.views.lastObject;

        data.viewcontroller.view = view;

        data.uiwindow.rootViewController = nil;
        data.uiwindow.rootViewController = data.viewcontroller;

        [data.uiwindow layoutIfNeeded];
    }

    /* Add ourself to the new window. */
    if (window) {
        data = (__bridge SDL_WindowData *) window->driverdata;

        /* Make sure the SDL window has a strong reference to this view. */
        [data.views addObject:self];

        /* Replace the view controller's old view with this one. */
        [data.viewcontroller.view removeFromSuperview];
        data.viewcontroller.view = self;

        /* The root view controller handles rotation and the status bar.
         * Assigning it also adds the controller's view to the window. We
         * explicitly re-set it to make sure the view is properly attached to
         * the window. Just adding the sub-view if the root view controller is
         * already correct causes orientation issues on iOS 7 and below. */
        data.uiwindow.rootViewController = nil;
        data.uiwindow.rootViewController = data.viewcontroller;

        /* The view's bounds may not be correct until the next event cycle. That
         * might happen after the current dimensions are queried, so we force a
         * layout now to immediately update the bounds. */
        [data.uiwindow layoutIfNeeded];
    }

    sdlwindow = window;
}

- (CGPoint)touchLocation:(UITouch *)touch shouldNormalize:(BOOL)normalize
{
    CGPoint point = [touch locationInView:self];

    if (normalize) {
        CGRect bounds = self.bounds;
        point.x /= bounds.size.width;
        point.y /= bounds.size.height;
    }

    return point;
}

- (float)pressureForTouch:(UITouch *)touch
{
#ifdef __IPHONE_9_0
    if ([touch respondsToSelector:@selector(force)]) {
        return (float) touch.force;
    }
#endif

    return 1.0f;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        float pressure = [self pressureForTouch:touch];

        if (!firstFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send mouse moved event */
            SDL_SendMouseMotion(sdlwindow, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);

            /* send mouse down event */
            SDL_SendMouseButton(sdlwindow, SDL_TOUCH_MOUSEID, SDL_PRESSED, SDL_BUTTON_LEFT);

            firstFingerDown = touch;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouch(touchId, (SDL_FingerID)((size_t)touch),
                      SDL_TRUE, locationInView.x, locationInView.y, pressure);
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        float pressure = [self pressureForTouch:touch];

        if (touch == firstFingerDown) {
            /* send mouse up */
            SDL_SendMouseButton(sdlwindow, SDL_TOUCH_MOUSEID, SDL_RELEASED, SDL_BUTTON_LEFT);
            firstFingerDown = nil;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouch(touchId, (SDL_FingerID)((size_t)touch),
                      SDL_FALSE, locationInView.x, locationInView.y, pressure);
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        float pressure = [self pressureForTouch:touch];

        if (touch == firstFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send moved event */
            SDL_SendMouseMotion(sdlwindow, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouchMotion(touchId, (SDL_FingerID)((size_t)touch),
                            locationInView.x, locationInView.y, pressure);
    }
}

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
