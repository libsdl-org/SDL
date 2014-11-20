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
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include "SDL_uikitopenglview.h"
#include "SDL_uikitmessagebox.h"
#include "SDL_uikitvideo.h"


@implementation SDL_uikitopenglview {

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

@synthesize context;

@synthesize backingWidth;
@synthesize backingHeight;

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
               sRGB:(BOOL)sRGB
       majorVersion:(int)majorVersion
         shareGroup:(EAGLSharegroup*)shareGroup
{
    if ((self = [super initWithFrame:frame])) {
        const BOOL useStencilBuffer = (stencilBits != 0);
        const BOOL useDepthBuffer = (depthBits != 0);
        NSString *colorFormat = nil;

        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.autoresizesSubviews = YES;

        /* The EAGLRenderingAPI enum values currently map 1:1 to major GLES
           versions, and this allows us to handle future OpenGL ES versions.
         */
        EAGLRenderingAPI api = majorVersion;

        context = [[EAGLContext alloc] initWithAPI:api sharegroup:shareGroup];
        if (!context || ![EAGLContext setCurrentContext:context]) {
            SDL_SetError("OpenGL ES %d not supported", majorVersion);
            return nil;
        }

        if (sRGB) {
            /* sRGB EAGL drawable support was added in iOS 7. */
            if (UIKit_IsSystemVersionAtLeast(@"7.0")) {
                colorFormat = kEAGLColorFormatSRGBA8;
            } else {
                SDL_SetError("sRGB drawables are not supported.");
                return nil;
            }
        } else if (rBits >= 8 || gBits >= 8 || bBits >= 8) {
            /* if user specifically requests rbg888 or some color format higher than 16bpp */
            colorFormat = kEAGLColorFormatRGBA8;
        } else {
            /* default case (faster) */
            colorFormat = kEAGLColorFormatRGB565;
        }

        /* Get the layer */
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;

        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = @{
            kEAGLDrawablePropertyRetainedBacking:@(retained),
            kEAGLDrawablePropertyColorFormat:colorFormat
        };

        /* Set the appropriate scale (for retina display support) */
        self.contentScaleFactor = scale;

        /* Create the color Renderbuffer Object */
        glGenRenderbuffers(1, &viewRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);

        if (![context renderbufferStorage:GL_RENDERBUFFER fromDrawable:eaglLayer]) {
            SDL_SetError("Failed to create OpenGL ES drawable");
            return nil;
        }

        /* Create the Framebuffer Object */
        glGenFramebuffers(1, &viewFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);

        /* attach the color renderbuffer to the FBO */
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);

        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

        if ((useDepthBuffer) || (useStencilBuffer)) {
            if (useStencilBuffer) {
                /* Apparently you need to pack stencil and depth into one buffer. */
                depthBufferFormat = GL_DEPTH24_STENCIL8_OES;
            } else if (useDepthBuffer) {
                /* iOS only has 24-bit depth buffers, even with GL_DEPTH_COMPONENT16 */
                depthBufferFormat = GL_DEPTH_COMPONENT24_OES;
            }

            glGenRenderbuffers(1, &depthRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, depthBufferFormat, backingWidth, backingHeight);
            if (useDepthBuffer) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
            }
            if (useStencilBuffer) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
            }
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            SDL_SetError("Failed creating OpenGL ES framebuffer");
            return nil;
        }

        glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    }

    return self;
}

- (GLuint)drawableRenderbuffer
{
    return viewRenderbuffer;
}

- (GLuint)drawableFramebuffer
{
    return viewFramebuffer;
}

- (void)updateFrame
{
    glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &viewRenderbuffer);

    glGenRenderbuffers(1, &viewRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    if (depthRenderbuffer != 0) {
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, depthBufferFormat, backingWidth, backingHeight);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
}

- (void)setAnimationCallback:(int)interval
                    callback:(void (*)(void*))callback
               callbackParam:(void*)callbackParam
{
    [self stopAnimation];

    animationInterval = interval;
    animationCallback = callback;
    animationCallbackParam = callbackParam;

    if (animationCallback) {
        [self startAnimation];
    }
}

- (void)startAnimation
{
    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(doLoop:)];
    [displayLink setFrameInterval:animationInterval];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)stopAnimation
{
    [displayLink invalidate];
    displayLink = nil;
}

- (void)doLoop:(CADisplayLink*)sender
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
       else is responsible for rebinding viewRenderbuffer, to reduce duplicate
       state changes. */
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    [EAGLContext setCurrentContext:context];
    [self updateFrame];
}

- (void)destroyFramebuffer
{
    if (viewFramebuffer != 0) {
        glDeleteFramebuffers(1, &viewFramebuffer);
        viewFramebuffer = 0;
    }

    if (viewRenderbuffer != 0) {
        glDeleteRenderbuffers(1, &viewRenderbuffer);
        viewRenderbuffer = 0;
    }

    if (depthRenderbuffer != 0) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}


- (void)dealloc
{
    if ([EAGLContext currentContext] == context) {
        [self destroyFramebuffer];
        [EAGLContext setCurrentContext:nil];
    }
}

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
