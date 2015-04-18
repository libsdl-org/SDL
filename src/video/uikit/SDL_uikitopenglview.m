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

#include <OpenGLES/EAGLDrawable.h>
#include <OpenGLES/ES2/glext.h>
#import "SDL_uikitopenglview.h"
#include "SDL_uikitwindow.h"

@implementation SDLEAGLContext

@end

@implementation SDL_uikitopenglview {
    /* The renderbuffer and framebuffer used to render to this layer. */
    GLuint viewRenderbuffer, viewFramebuffer;

    /* The depth buffer that is attached to viewFramebuffer, if it exists. */
    GLuint depthRenderbuffer;

    /* format of depthRenderbuffer */
    GLenum depthBufferFormat;
}

@synthesize context;
@synthesize backingWidth;
@synthesize backingHeight;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame
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

        /* The EAGLRenderingAPI enum values currently map 1:1 to major GLES
         * versions, and this allows us to handle future OpenGL ES versions. */
        EAGLRenderingAPI api = majorVersion;

        context = [[SDLEAGLContext alloc] initWithAPI:api sharegroup:shareGroup];
        if (!context || ![EAGLContext setCurrentContext:context]) {
            SDL_SetError("OpenGL ES %d not supported", majorVersion);
            return nil;
        }

        context.sdlView = self;

        if (sRGB) {
            /* sRGB EAGL drawable support was added in iOS 7. */
            if (UIKit_IsSystemVersionAtLeast(7.0)) {
                colorFormat = kEAGLColorFormatSRGBA8;
            } else {
                SDL_SetError("sRGB drawables are not supported.");
                return nil;
            }
        } else if (rBits >= 8 || gBits >= 8 || bBits >= 8) {
            /* if user specifically requests rbg888 or some color format higher than 16bpp */
            colorFormat = kEAGLColorFormatRGBA8;
        } else {
            /* default case (potentially faster) */
            colorFormat = kEAGLColorFormatRGB565;
        }

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
                /* iOS only uses 32-bit float (exposed as fixed point 24-bit)
                 * depth buffers. */
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

        [self setDebugLabels];
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
    GLint prevRenderbuffer = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRenderbuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    if (depthRenderbuffer != 0) {
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, depthBufferFormat, backingWidth, backingHeight);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, prevRenderbuffer);
}

- (void)setDebugLabels
{
    if (viewFramebuffer != 0) {
        glLabelObjectEXT(GL_FRAMEBUFFER, viewFramebuffer, 0, "context FBO");
    }

    if (viewRenderbuffer != 0) {
        glLabelObjectEXT(GL_RENDERBUFFER, viewRenderbuffer, 0, "context color buffer");
    }

    if (depthRenderbuffer != 0) {
        if (depthBufferFormat == GL_DEPTH24_STENCIL8_OES) {
            glLabelObjectEXT(GL_RENDERBUFFER, depthRenderbuffer, 0, "context depth-stencil buffer");
        } else {
            glLabelObjectEXT(GL_RENDERBUFFER, depthRenderbuffer, 0, "context depth buffer");
        }
    }
}

- (void)setCurrentContext
{
    [EAGLContext setCurrentContext:context];
}

- (void)swapBuffers
{
    /* viewRenderbuffer should always be bound here. Code that binds something
     * else is responsible for rebinding viewRenderbuffer, to reduce duplicate
     * state changes. */
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    int width  = (int) (self.bounds.size.width * self.contentScaleFactor);
    int height = (int) (self.bounds.size.height * self.contentScaleFactor);

    /* Update the color and depth buffer storage if the layer size has changed. */
    if (width != backingWidth || height != backingHeight) {
        EAGLContext *prevContext = [EAGLContext currentContext];
        if (prevContext != context) {
            [EAGLContext setCurrentContext:context];
        }

        [self updateFrame];

        if (prevContext != context) {
            [EAGLContext setCurrentContext:prevContext];
        }
    }
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
