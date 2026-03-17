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

#import "SDL_uikitvolumetric.h"
#import "SDL_uikitwindow.h"
#import "SDL_uikitview.h"
#import "SDL_uikitvisionosscene.h"
#import <Metal/Metal.h>

bool SDL_UIKit_IsVolumetricWindow(SDL_Window *window)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;

    return data && data.visionOSScene;
}

bool SDL_UIKit_SetWindowCurvature(SDL_Window *window, float curvature)
{
    if (!SDL_UIKit_IsVolumetricWindow(window)) {
        return SDL_SetError("Window is not a volumetric window");
    }

    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    if (!data || !data.visionOSScene) {
        return SDL_SetError("Window does not have a volumetric scene");
    }

    curvature = SDL_clamp(curvature, 0.0f, 1.0f);
    [data.visionOSScene setCurvature:curvature];

    return true;
}

float SDL_UIKit_GetWindowCurvature(SDL_Window *window)
{
    SDL_Log("SDL_UIKit_GetWindowCurvature: Called for window %p", window);

    if (!SDL_UIKit_IsVolumetricWindow(window)) {
        return 0.0f;
    }

    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    if (!data || !data.visionOSScene) {
        SDL_Log("SDL_UIKit_GetWindowCurvature: Volumetric scene not found, returning 0.0");
        return 0.0f;
    }

    return (float)data.visionOSScene.curvature;
}

id<MTLTexture> SDL_UIKit_GetVolumetricDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    if (!data || !data.visionOSScene) {
        return;
    }

    return [data.visionOSScene getDisplayTexture:commandBuffer
                                           width:width
                                          height:height
                                     pixelFormat:pixelFormat];
}

id<MTLDevice> SDL_UIKit_GetVolumetricMetalDevice(SDL_Window *window)
{
    if (!SDL_UIKit_IsVolumetricWindow(window)) {
        return nil;
    }

    // Return the system default Metal device for volumetric rendering
    return MTLCreateSystemDefaultDevice();
}

// Public API implementations

bool SDL_SetVisionOSWindowCurvature(SDL_Window *window, float curvature)
{
    return SDL_UIKit_SetWindowCurvature(window, curvature);
}

float SDL_GetVisionOSWindowCurvature(SDL_Window *window)
{
    return SDL_UIKit_GetWindowCurvature(window);
}

#endif /* SDL_PLATFORM_VISIONOS */
