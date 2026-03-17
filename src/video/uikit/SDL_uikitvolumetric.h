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
#ifndef SDL_uikitvolumetric_h_
#define SDL_uikitvolumetric_h_

#import "SDL_uikitwindow.h"
#import <Metal/Metal.h>

/**
 * Check if a window is using volumetric rendering mode.
 *
 * @param window The SDL window to check.
 * @return true if the window is volumetric, false otherwise.
 */
extern bool SDL_UIKit_IsVolumetricWindow(SDL_Window *window);

/**
 * Set the curvature of a volumetric window.
 *
 * @param window The SDL window to modify.
 * @param curvature The curvature factor (0.0 = flat, 1.0 = maximum curve).
 * @return true on success, false on failure.
 */
extern bool SDL_UIKit_SetWindowCurvature(SDL_Window *window, float curvature);

/**
 * Get the curvature of a volumetric window.
 *
 * @param window The SDL window to query.
 * @return The current curvature factor (0.0-1.0), or 0.0 if not volumetric.
 */
extern float SDL_UIKit_GetWindowCurvature(SDL_Window *window);

/**
 * Get the volumetric display texture.
 */
id<MTLTexture> SDL_UIKit_GetVolumetricDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat);

/**
 * Update the volumetric scene content texture with new rendering output.
 *
 * This should be called after SDL has finished rendering a frame.
 *
 * @param window The SDL window.
 * @param texture The Metal texture containing the rendered content.
 */
extern void SDL_UIKit_UpdateVolumetricTexture(SDL_Window *window, id<MTLTexture> texture);

/**
 * Get the Metal device used for volumetric rendering.
 *
 * @param window The SDL window.
 * @return The Metal device, or nil if not available.
 */
extern id<MTLDevice> SDL_UIKit_GetVolumetricMetalDevice(SDL_Window *window);

#endif /* SDL_uikitvolumetric_h_ */
