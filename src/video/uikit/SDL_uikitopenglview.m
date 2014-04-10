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

#include <QuartzCore/QuartzCore.h>
#include <OpenGLES/EAGLDrawable.h>
#include "SDL_uikitopenglview.h"
#include "SDL_uikitmessagebox.h"


@implementation SDL_uikitopenglview

@synthesize context;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

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
      shareGroup:(EAGLSharegroup*)shareGroup
{
    depthBufferFormat = 0;

    if ((self = [super initWithFrame:frame])) {
        const BOOL useStencilBuffer = (stencilBits != 0);
        const BOOL useDepthBuffer = (depthBits != 0);
        NSString *colorFormat = nil;

        /* The EAGLRenderingAPI enum values currently map 1:1 to major GLES
           versions, and this allows us to handle future OpenGL ES versions.
         */
        EAGLRenderingAPI api = majorVersion;

        if (rBits == 8 && gBits == 8 && bBits == 8) {
            /* if user specifically requests rbg888 or some color format higher than 16bpp */
            colorFormat = kEAGLColorFormatRGBA8;
        } else {
            /* default case (faster) */
            colorFormat = kEAGLColorFormatRGB565;
        }

        /* Get the layer */
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool: retained], kEAGLDrawablePropertyRetainedBacking, colorFormat, kEAGLDrawablePropertyColorFormat, nil];

        context = [[EAGLContext alloc] initWithAPI:api sharegroup:shareGroup];
        if (!context || ![EAGLContext setCurrentContext:context]) {
            [self release];
            SDL_SetError("OpenGL ES %d not supported", majorVersion);
            return nil;
        }

        /* Set the appropriate scale (for retina display support) */
        if ([self respondsToSelector:@selector(contentScaleFactor)])
            self.contentScaleFactor = scale;

        /* create the buffers */
        glGenFramebuffersOES(1, &viewFramebuffer);
        glGenRenderbuffersOES(1, &viewRenderbuffer);

        glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
        [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);

        glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
        glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

        if ((useDepthBuffer) || (useStencilBuffer)) {
            if (useStencilBuffer) {
                /* Apparently you need to pack stencil and depth into one buffer. */
                depthBufferFormat = GL_DEPTH24_STENCIL8_OES;
            } else if (useDepthBuffer) {
                /* iOS only has 24-bit depth buffers, even with GL_DEPTH_COMPONENT16_OES */
                depthBufferFormat = GL_DEPTH_COMPONENT24_OES;
            }

            glGenRenderbuffersOES(1, &depthRenderbuffer);
            glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
            glRenderbufferStorageOES(GL_RENDERBUFFER_OES, depthBufferFormat, backingWidth, backingHeight);
            if (useDepthBuffer) {
                glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
            }
            if (useStencilBuffer) {
                glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_STENCIL_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
            }
        }

        if (glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
            return NO;
        }

        glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
        /* end create buffers */

        self.autoresizingMask = (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
        self.autoresizesSubviews = YES;
    }
    return self;
}

- (void)updateFrame
{
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, 0);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, 0);
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);

    glGenRenderbuffersOES(1, &viewRenderbuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);

    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

    if (depthRenderbuffer != 0) {
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
        glRenderbufferStorageOES(GL_RENDERBUFFER_OES, depthBufferFormat, backingWidth, backingHeight);
    }

    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
}

- (void)setAnimationCallback:(int)interval
    callback:(void (*)(void*))callback
    callbackParam:(void*)callbackParam
{
    [self stopAnimation];

    animationInterval = interval;
    animationCallback = callback;
    animationCallbackParam = callbackParam;

    if (animationCallback)
        [self startAnimation];
}

- (void)startAnimation
{
    /* CADisplayLink is API new to iPhone SDK 3.1.
     * Compiling against earlier versions will result in a warning, but can be dismissed
     * if the system version runtime check for CADisplayLink exists in -initWithCoder:.
     */
    displayLink = [NSClassFromString(@"CADisplayLink") displayLinkWithTarget:self selector:@selector(doLoop:)];
    [displayLink setFrameInterval:animationInterval];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)stopAnimation
{
    [displayLink invalidate];
    displayLink = nil;
}

- (void)doLoop:(id)sender
{
    /* Don't run the game loop while a messagebox is up */
    if (!UIKit_ShowingMessageBox()) {
        animationCallback(animationCallbackParam);
    }
}

- (void)setCurrentContext
{
    [EAGLContext setCurrentContext:context];
}


- (void)swapBuffers
{
    /* viewRenderbuffer should always be bound here. Code that binds something
        else is responsible for rebinding viewRenderbuffer, to reduce
        duplicate state changes. */
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}


- (void)layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    [self updateFrame];
}

- (void)destroyFramebuffer
{
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;

    if (depthRenderbuffer) {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}


- (void)dealloc
{
    [self destroyFramebuffer];
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }
    [context release];
    [super dealloc];
}

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
