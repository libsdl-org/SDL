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

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/gltypes.h>
#import "SDL_uikitview.h"
/*
    This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
    The view content is basically an EAGL surface you render your OpenGL scene into.
    Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
 */
@interface SDL_uikitopenglview : SDL_uikitview

- (id)initWithFrame:(CGRect)frame
              scale:(CGFloat)scale
      retainBacking:(BOOL)retained
              rBits:(int)rBits
              gBits:(int)gBits
              bBits:(int)bBits
              aBits:(int)aBits
          depthBits:(int)depthBits
        stencilBits:(int)stencilBits
               sRGB:(BOOL)sRGB
       majorVersion:(int)majorVersion
         shareGroup:(EAGLSharegroup*)shareGroup;

@property (nonatomic, strong, readonly) EAGLContext *context;

/* The width and height of the drawable in pixels (as opposed to points.) */
@property (nonatomic, readonly) int backingWidth;
@property (nonatomic, readonly) int backingHeight;

@property (nonatomic, readonly) GLuint drawableRenderbuffer;
@property (nonatomic, readonly) GLuint drawableFramebuffer;

- (void)swapBuffers;
- (void)setCurrentContext;

- (void)updateFrame;

- (void)setDebugLabels;

- (void)setAnimationCallback:(int)interval
                    callback:(void (*)(void*))callback
               callbackParam:(void*)callbackParam;

- (void)startAnimation;
- (void)stopAnimation;

- (void)doLoop:(CADisplayLink*)sender;

@end

/* vi: set ts=4 sw=4 expandtab: */
