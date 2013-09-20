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
#include "SDL_config.h"

/* NSOpenGL implementation of SDL OpenGL support */

#if SDL_VIDEO_OPENGL_CGL
#include "SDL_cocoavideo.h"
#include "SDL_cocoaopengl.h"

#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "SDL_loadso.h"
#include "SDL_opengl.h"

#define DEFAULT_OPENGL  "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
/* New methods for converting to and from backing store pixels, taken from
 * AppKite/NSView.h in 10.8 SDK. */
@interface NSView (Backing)
- (NSPoint)convertPointToBacking:(NSPoint)aPoint;
- (NSPoint)convertPointFromBacking:(NSPoint)aPoint;
- (NSSize)convertSizeToBacking:(NSSize)aSize;
- (NSSize)convertSizeFromBacking:(NSSize)aSize;
- (NSRect)convertRectToBacking:(NSRect)aRect;
- (NSRect)convertRectFromBacking:(NSRect)aRect;
@end
#endif

#ifndef kCGLPFAOpenGLProfile
#define kCGLPFAOpenGLProfile 99
#endif
#ifndef kCGLOGLPVersion_Legacy
#define kCGLOGLPVersion_Legacy 0x1000
#endif
#ifndef kCGLOGLPVersion_3_2_Core
#define kCGLOGLPVersion_3_2_Core 0x3200
#endif

@implementation SDLOpenGLContext : NSOpenGLContext

- (id)initWithFormat:(NSOpenGLPixelFormat *)format
        shareContext:(NSOpenGLContext *)share
{
    self = [super initWithFormat:format shareContext:share];
    if (self) {
        SDL_AtomicSet(&self->dirty, 0);
        self->window = NULL;
    }
    return self;
}

- (void)scheduleUpdate
{
    SDL_AtomicAdd(&self->dirty, 1);
}

/* This should only be called on the thread on which a user is using the context. */
- (void)updateIfNeeded
{
    int value = SDL_AtomicSet(&self->dirty, 0);
    if (value > 0) {
        /* We call the real underlying update here, since -[SDLOpenGLContext update] just calls us. */
        [super update];
    }
}

/* This should only be called on the thread on which a user is using the context. */
- (void)update
{
    /* This ensures that regular 'update' calls clear the atomic dirty flag. */
    [self scheduleUpdate];
    [self updateIfNeeded];
}

/* Updates the drawable for the contexts and manages related state. */
- (void)setWindow:(SDL_Window *)newWindow
{
    if (self->window) {
        SDL_WindowData *oldwindowdata = (SDL_WindowData *)self->window->driverdata;

        /* Make sure to remove us from the old window's context list, or we'll get scheduled updates from it too. */
        NSMutableArray *contexts = oldwindowdata->nscontexts;
        @synchronized (contexts) {
            [contexts removeObject:self];
        }
    }

    self->window = newWindow;

    if (newWindow) {
        SDL_WindowData *windowdata = (SDL_WindowData *)newWindow->driverdata;

        /* Now sign up for scheduled updates for the new window. */
        NSMutableArray *contexts = windowdata->nscontexts;
        @synchronized (contexts) {
            [contexts addObject:self];
        }

        if ([self view] != [windowdata->nswindow contentView]) {
            [self setView:[windowdata->nswindow contentView]];
            [self scheduleUpdate];
        }
    } else {
        [self clearDrawable];
        [self scheduleUpdate];
    }
}

@end


int
Cocoa_GL_LoadLibrary(_THIS, const char *path)
{
    /* Load the OpenGL library */
    if (path == NULL) {
        path = SDL_getenv("SDL_OPENGL_LIBRARY");
    }
    if (path == NULL) {
        path = DEFAULT_OPENGL;
    }
    _this->gl_config.dll_handle = SDL_LoadObject(path);
    if (!_this->gl_config.dll_handle) {
        return -1;
    }
    SDL_strlcpy(_this->gl_config.driver_path, path,
                SDL_arraysize(_this->gl_config.driver_path));
    return 0;
}

void *
Cocoa_GL_GetProcAddress(_THIS, const char *proc)
{
    return SDL_LoadFunction(_this->gl_config.dll_handle, proc);
}

void
Cocoa_GL_UnloadLibrary(_THIS)
{
    SDL_UnloadObject(_this->gl_config.dll_handle);
    _this->gl_config.dll_handle = NULL;
}

SDL_GLContext
Cocoa_GL_CreateContext(_THIS, SDL_Window * window)
{
    const int wantver = (_this->gl_config.major_version << 8) |
                        (_this->gl_config.minor_version);
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    NSAutoreleasePool *pool;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayData *displaydata = (SDL_DisplayData *)display->driverdata;
    NSOpenGLPixelFormatAttribute attr[32];
    NSOpenGLPixelFormat *fmt;
    SDLOpenGLContext *context;
    NSOpenGLContext *share_context = nil;
    int i = 0;

    if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
        SDL_SetError ("OpenGL ES not supported on this platform");
        return NULL;
    }

    /* Sadly, we'll have to update this as life progresses, since we need to
       set an enum for context profiles, not a context version number */
    if (wantver > 0x0302) {
        SDL_SetError ("OpenGL > 3.2 is not supported on this platform");
        return NULL;
    }

    pool = [[NSAutoreleasePool alloc] init];

    /* specify a profile if we're on Lion (10.7) or later. */
    if (data->osversion >= 0x1070) {
        NSOpenGLPixelFormatAttribute profile = kCGLOGLPVersion_Legacy;
        if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_CORE) {
            if (wantver == 0x0302) {
                profile = kCGLOGLPVersion_3_2_Core;
            }
        }
        attr[i++] = kCGLPFAOpenGLProfile;
        attr[i++] = profile;
    }

    attr[i++] = NSOpenGLPFAColorSize;
    attr[i++] = SDL_BYTESPERPIXEL(display->current_mode.format)*8;

    attr[i++] = NSOpenGLPFADepthSize;
    attr[i++] = _this->gl_config.depth_size;

    if (_this->gl_config.double_buffer) {
        attr[i++] = NSOpenGLPFADoubleBuffer;
    }

    if (_this->gl_config.stereo) {
        attr[i++] = NSOpenGLPFAStereo;
    }

    if (_this->gl_config.stencil_size) {
        attr[i++] = NSOpenGLPFAStencilSize;
        attr[i++] = _this->gl_config.stencil_size;
    }

    if ((_this->gl_config.accum_red_size +
         _this->gl_config.accum_green_size +
         _this->gl_config.accum_blue_size +
         _this->gl_config.accum_alpha_size) > 0) {
        attr[i++] = NSOpenGLPFAAccumSize;
        attr[i++] = _this->gl_config.accum_red_size + _this->gl_config.accum_green_size + _this->gl_config.accum_blue_size + _this->gl_config.accum_alpha_size;
    }

    if (_this->gl_config.multisamplebuffers) {
        attr[i++] = NSOpenGLPFASampleBuffers;
        attr[i++] = _this->gl_config.multisamplebuffers;
    }

    if (_this->gl_config.multisamplesamples) {
        attr[i++] = NSOpenGLPFASamples;
        attr[i++] = _this->gl_config.multisamplesamples;
        attr[i++] = NSOpenGLPFANoRecovery;
    }

    if (_this->gl_config.accelerated >= 0) {
        if (_this->gl_config.accelerated) {
            attr[i++] = NSOpenGLPFAAccelerated;
        } else {
            attr[i++] = NSOpenGLPFARendererID;
            attr[i++] = kCGLRendererGenericFloatID;
        }
    }

    attr[i++] = NSOpenGLPFAScreenMask;
    attr[i++] = CGDisplayIDToOpenGLDisplayMask(displaydata->display);
    attr[i] = 0;

    fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
    if (fmt == nil) {
        SDL_SetError ("Failed creating OpenGL pixel format");
        [pool release];
        return NULL;
    }

    if (_this->gl_config.share_with_current_context) {
        share_context = (NSOpenGLContext*)SDL_GL_GetCurrentContext();
    }

    context = [[SDLOpenGLContext alloc] initWithFormat:fmt shareContext:share_context];

    [fmt release];

    if (context == nil) {
        SDL_SetError ("Failed creating OpenGL context");
        [pool release];
        return NULL;
    }

    [pool release];

    if ( Cocoa_GL_MakeCurrent(_this, window, context) < 0 ) {
        Cocoa_GL_DeleteContext(_this, context);
        return NULL;
    }

    return context;
}

int
Cocoa_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    NSAutoreleasePool *pool;

    pool = [[NSAutoreleasePool alloc] init];

    if (context) {
        SDLOpenGLContext *nscontext = (SDLOpenGLContext *)context;
        [nscontext setWindow:window];
        [nscontext updateIfNeeded];
        [nscontext makeCurrentContext];
    } else {
        [NSOpenGLContext clearCurrentContext];
    }

    [pool release];
    return 0;
}

void
Cocoa_GL_GetDrawableSize(_THIS, SDL_Window * window, int * w, int * h)
{
    SDL_WindowData *windata = (SDL_WindowData *) window->driverdata;
    NSView *contentView = [windata->nswindow contentView];
    NSRect viewport = [contentView bounds];

    /* This gives us the correct viewport for a Retina-enabled view, only
     * supported on 10.7+. */
    if ([contentView respondsToSelector:@selector(convertRectToBacking:)]) {
        viewport = [contentView convertRectToBacking:viewport];
    }

    if (w) {
        *w = viewport.size.width;
    }

    if (h) {
        *h = viewport.size.height;
    }
}

int
Cocoa_GL_SetSwapInterval(_THIS, int interval)
{
    NSAutoreleasePool *pool;
    NSOpenGLContext *nscontext;
    GLint value;
    int status;

    if (interval < 0) {  /* no extension for this on Mac OS X at the moment. */
        return SDL_SetError("Late swap tearing currently unsupported");
    }

    pool = [[NSAutoreleasePool alloc] init];

    nscontext = (NSOpenGLContext*)SDL_GL_GetCurrentContext();
    if (nscontext != nil) {
        value = interval;
        [nscontext setValues:&value forParameter:NSOpenGLCPSwapInterval];
        status = 0;
    } else {
        status = SDL_SetError("No current OpenGL context");
    }

    [pool release];
    return status;
}

int
Cocoa_GL_GetSwapInterval(_THIS)
{
    NSAutoreleasePool *pool;
    NSOpenGLContext *nscontext;
    GLint value;
    int status = 0;

    pool = [[NSAutoreleasePool alloc] init];

    nscontext = (NSOpenGLContext*)SDL_GL_GetCurrentContext();
    if (nscontext != nil) {
        [nscontext getValues:&value forParameter:NSOpenGLCPSwapInterval];
        status = (int)value;
    }

    [pool release];
    return status;
}

void
Cocoa_GL_SwapWindow(_THIS, SDL_Window * window)
{
    NSAutoreleasePool *pool;

    pool = [[NSAutoreleasePool alloc] init];

    SDLOpenGLContext* nscontext = (SDLOpenGLContext*)SDL_GL_GetCurrentContext();
    [nscontext flushBuffer];
    [nscontext updateIfNeeded];

    [pool release];
}

void
Cocoa_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    NSAutoreleasePool *pool;
    SDLOpenGLContext *nscontext = (SDLOpenGLContext *)context;

    pool = [[NSAutoreleasePool alloc] init];

    [nscontext setWindow:NULL];
    [nscontext release];

    [pool release];
}

#endif /* SDL_VIDEO_OPENGL_CGL */

/* vi: set ts=4 sw=4 expandtab: */
