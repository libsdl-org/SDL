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
#ifndef SDL_uikitvisionosscene_h_
#define SDL_uikitvisionosscene_h_

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>

/**
 * Return true if the curved content pointer mode is enabled
 */
bool SDL_VisionOS_PointerModeEnabled();

/**
 * Check if any window is using curved content mode (UIHostingController-based).
 */
bool SDL_UIKit_HasCurvedWindow();

/**
 * Check if a window is using curved content mode (UIHostingController-based).
 *
 * @param window The SDL window to check.
 * @return true if the window is in curved mode, false otherwise.
 */
bool SDL_UIKit_IsCurvedWindow(SDL_Window *window);

/**
 * Get the curved content display texture.
 */
id<MTLTexture> SDL_UIKit_GetCurvedDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat);

#endif /* SDL_uikitvisionosscene_h_ */
