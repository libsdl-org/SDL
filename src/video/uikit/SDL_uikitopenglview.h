/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import "SDL_uikitview.h"
/*
    This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
    The view content is basically an EAGL surface you render your OpenGL scene into.
    Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
 */
@interface SDL_uikitopenglview : SDL_uikitview {

@private
    /* The pixel dimensions of the backbuffer */
    GLint backingWidth;
    GLint backingHeight;

    EAGLContext *context;

    /* OpenGL names for the renderbuffer and framebuffers used to render to this view */
    GLuint viewRenderbuffer, viewFramebuffer;

    /* OpenGL name for the depth buffer that is attached to viewFramebuffer, if it exists (0 if it does not exist) */
    GLuint depthRenderbuffer;

    /* format of depthRenderbuffer */
    GLenum depthBufferFormat;

    id displayLink;
    int animationInterval;
    void (*animationCallback)(void*);
    void *animationCallbackParam;
}

@property (nonatomic, retain, readonly) EAGLContext *context;

- (void)swapBuffers;
- (void)setCurrentContext;

- (id)initWithFrame:(CGRect)frame
    scale:(CGFloat)scale
    retainBacking:(BOOL)retained
    rBits:(int)rBits
    gBits:(int)gBits
    bBits:(int)bBits
    aBits:(int)aBits
    depthBits:(int)depthBits
    stencilBits:(int)stencilBits
    majorVersion:(int)majorVersion
    shareGroup:(EAGLSharegroup*)shareGroup;

- (void)updateFrame;

- (void)setAnimationCallback:(int)interval
    callback:(void (*)(void*))callback
    callbackParam:(void*)callbackParam;

- (void)startAnimation;
- (void)stopAnimation;

- (void)doLoop:(id)sender;

@end

/* vi: set ts=4 sw=4 expandtab: */
