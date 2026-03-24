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
 * SDL_UIKitVisionOSScene
 *
 * Manages a visionOS scene for presenting SDL content using SwiftUI.
 * Supports both volumetric windows and immersive spaces via mode parameter.
 */
@interface SDL_UIKitVisionOSScene : NSObject

/**
 * Whether the scene is currently presented.
 */
@property (nonatomic, assign, readonly) BOOL isPresented;

/**
 * The current curvature factor (0.0-1.0).
 */
@property (nonatomic, assign, readonly) CGFloat curvature;

/**
 * Initialize the scene manager.
 *
 * @param windowScene The window scene context
 * @param device Metal device for rendering
 * @param curvature Initial curvature factor (0.0-1.0)
 * @param size Size of the content
 * @param mainSceneSession The main scene session (kept alive for restoration)
 */
- (instancetype)initWithWindowScene:(UIWindowScene *)windowScene
                          curvature:(CGFloat)curvature
                               size:(CGSize)size
                   mainSceneSession:(UISceneSession *)mainSceneSession;

/**
 * Present the scene.
 */
- (void)present;

/**
 * Dismiss the scene.
 */
- (void)dismiss;

/**
 * Get the texture being displayed
 */
- (id<MTLTexture>)getDisplayTexture:(id<MTLCommandBuffer>)commandBuffer
                              width:(int)width
                             height:(int)height
                        pixelFormat:(MTLPixelFormat)pixelFormat;

/**
 * Update the size of the display.
 *
 * @param size New size
 */
- (void)setSize:(CGSize)size;

/**
 * Update the curvature of the display.
 *
 * @param curvature New curvature factor (0.0-1.0)
 */
- (void)setCurvature:(CGFloat)curvature;

/**
 * Set the scene session (called by scene delegate when scene connects).
 *
 * @param session The scene session
 */
- (void)setSceneSession:(UISceneSession *)session;

@end


/**
 * Check if a window is using volumetric rendering mode.
 *
 * @param window The SDL window to check.
 * @return true if the window is volumetric, false otherwise.
 */
bool SDL_UIKit_IsImmersiveWindow(SDL_Window *window);

/**
 * Get the volumetric display texture.
 */
id<MTLTexture> SDL_UIKit_GetImmersiveDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat);

#endif /* SDL_uikitvisionosscene_h_ */
