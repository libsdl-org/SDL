/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_VISIONOS

#include "SDL_UIKitBridge-objc.h"
#include "SDL_UIKitBridge-swift.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitwindow.h"
#include "SDL_uikitmetalview.h"
#include "../../events/SDL_events_c.h"


// Called from Swift scene delegates when window size changes
void SDL_VisionOS_SendSizeChanged(long width, long height)
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (window) {
        SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
        CGRect bounds = CGRectMake(0, 0, width, height);

        // Update the UIWindow
        data.uiwindow.frame = bounds;

        // Update the view
        UIView *view = data.viewcontroller.view;
        view.bounds = bounds;

        // Update the metal layer
        if ([view isKindOfClass:[SDL_uikitmetalview class]]) {
            SDL_uikitmetalview *metalview = (SDL_uikitmetalview *)view;

            [metalview updateDrawableSize];
        }

        // Send the resize event
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, (int)width, (int)height);
    }
}

// Called from Swift scene delegates to get the initial curvature
float SDL_VisionOS_GetCurvature()
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (window) {
        SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
        return data.curvature;
    }
    return 0.0f;
}

// Called from Swift scene delegates when window curvature changes
void SDL_VisionOS_SendCurvatureChanged(float curvature)
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (window) {
        SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
        if (curvature != data.curvature) {
            data.curvature = curvature;
            SDL_SetFloatProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_CURVATURE_FLOAT, curvature);
            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_CURVATURE_CHANGED, (int)curvature, 0);
        }
    }
}

static bool SDL_pointer_mode;

void SDL_VisionOS_SendPointerMode(bool enabled)
{
    SDL_pointer_mode = enabled;
}

bool SDL_VisionOS_PointerModeEnabled()
{
    return SDL_pointer_mode;
}

// Called from Swift scene delegates when visionOS delivers a touch event
void SDL_VisionOS_SendTouch(NSTimeInterval timestamp, SDL_FingerID fingerID, Uint32 eventType, float x, float y)
{
    const SDL_TouchID directTouchId = 1;
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (!window) {
        return;
    }

    float pressure;
    if (eventType == SDL_EVENT_FINGER_DOWN || eventType == SDL_EVENT_FINGER_MOTION) {
        pressure = 1.0f;
    } else {
        pressure = 0.0f;
    }
    if (eventType == SDL_EVENT_FINGER_MOTION) {
        SDL_SendTouchMotion(UIKit_GetEventTimestamp(timestamp), directTouchId, fingerID, window, x, y, pressure);
    } else {
        SDL_SendTouch(UIKit_GetEventTimestamp(timestamp), directTouchId, fingerID, window, (SDL_EventType)eventType, x, y, pressure);
    }
}

// MARK: - RealityKit Content Hosting

// Called from Swift to register the RealityKit hosting object with the SDL window.
void SDL_VisionOS_SetWindowRealityKitHosting(id hosting)
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "VISIONOS: No focused window for RealityKit hosting");
        return;
    }

    SDL_UIKitWindowData *windowData = (__bridge SDL_UIKitWindowData *)window->internal;
    windowData.curvedContentHosting = hosting;

    // Updating curvedContentHosting updates the view controller so that the "container background" is hidden.
    // On visionOS, this gets rid of the default glass background effect (not wanted for our content).
    [windowData.viewcontroller setNeedsUpdateOfPreferredContainerBackgroundStyle];

    //SDL_Log("VISIONOS: RealityKit hosting registered");
}

bool SDL_UIKit_HasCurvedWindow()
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (window) {
        return SDL_UIKit_IsCurvedWindow(window);
    }
    return false;
}

bool SDL_UIKit_IsCurvedWindow(SDL_Window *window)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    return data && data.curvedContentHosting;
}

id<MTLTexture> SDL_UIKit_GetCurvedDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    if (!data || !data.curvedContentHosting) {
        return nil;
    }

    id hosting = data.curvedContentHosting;
    SEL getTextureSelector = NSSelectorFromString(@"getDisplayTexture:width:height:pixelFormat:");
    if (![hosting respondsToSelector:getTextureSelector]) {
        return nil;
    }

    NSMethodSignature *signature = [hosting methodSignatureForSelector:getTextureSelector];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:getTextureSelector];
    [invocation setTarget:hosting];

    long arg_width = width;
    long arg_height = height;
    [invocation setArgument:&commandBuffer atIndex:2];
    [invocation setArgument:&arg_width atIndex:3];
    [invocation setArgument:&arg_height atIndex:4];
    [invocation setArgument:&pixelFormat atIndex:5];
    [invocation invoke];

    __unsafe_unretained id temp = nil;
    [invocation getReturnValue:&temp];
    id<MTLTexture> texture = temp;
    return texture;
}

#endif /* SDL_PLATFORM_VISIONOS */
