/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_RENDER_METAL && !SDL_RENDER_DISABLED

#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_assert.h"
#include "SDL_syswm.h"
#include "../SDL_sysrender.h"

#ifdef __MACOSX__
#include <Cocoa/Cocoa.h>
#else
#include "../../video/uikit/SDL_uikitmetalview.h"
#endif
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

/* Regenerate these with build-metal-shaders.sh */
#ifdef __MACOSX__
#include "SDL_shaders_metal_osx.h"
#else
#include "SDL_shaders_metal_ios.h"
#endif

/* Apple Metal renderer implementation */

static SDL_Renderer *METAL_CreateRenderer(SDL_Window * window, Uint32 flags);
static void METAL_WindowEvent(SDL_Renderer * renderer,
                           const SDL_WindowEvent *event);
static int METAL_GetOutputSize(SDL_Renderer * renderer, int *w, int *h);
static int METAL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int METAL_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, const void *pixels,
                            int pitch);
static int METAL_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                               const SDL_Rect * rect,
                               const Uint8 *Yplane, int Ypitch,
                               const Uint8 *Uplane, int Upitch,
                               const Uint8 *Vplane, int Vpitch);
static int METAL_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * rect, void **pixels, int *pitch);
static void METAL_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int METAL_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture);
static int METAL_UpdateViewport(SDL_Renderer * renderer);
static int METAL_UpdateClipRect(SDL_Renderer * renderer);
static int METAL_RenderClear(SDL_Renderer * renderer);
static int METAL_RenderDrawPoints(SDL_Renderer * renderer,
                               const SDL_FPoint * points, int count);
static int METAL_RenderDrawLines(SDL_Renderer * renderer,
                              const SDL_FPoint * points, int count);
static int METAL_RenderFillRects(SDL_Renderer * renderer,
                              const SDL_FRect * rects, int count);
static int METAL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_FRect * dstrect);
static int METAL_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                         const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
static int METAL_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                               Uint32 pixel_format, void * pixels, int pitch);
static void METAL_RenderPresent(SDL_Renderer * renderer);
static void METAL_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void METAL_DestroyRenderer(SDL_Renderer * renderer);
static void *METAL_GetMetalLayer(SDL_Renderer * renderer);
static void *METAL_GetMetalCommandEncoder(SDL_Renderer * renderer);

SDL_RenderDriver METAL_RenderDriver = {
    METAL_CreateRenderer,
    {
     "metal",
     (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE),
     2,
     {SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_ABGR8888},

     // !!! FIXME: how do you query Metal for this?
     // (the weakest GPU supported by Metal on iOS has 4k texture max, and
     //  other models might be 2x or 4x more. On macOS, it's 16k across the
     //  board right now.)
#ifdef __MACOSX__
     16384, 16384
#else
     4096, 4096
#endif
    }
};

@interface METAL_RenderData : NSObject
    @property (nonatomic, assign) BOOL beginScene;
    @property (nonatomic, retain) id<MTLDevice> mtldevice;
    @property (nonatomic, retain) id<MTLCommandQueue> mtlcmdqueue;
    @property (nonatomic, retain) id<MTLCommandBuffer> mtlcmdbuffer;
    @property (nonatomic, retain) id<MTLRenderCommandEncoder> mtlcmdencoder;
    @property (nonatomic, retain) id<MTLLibrary> mtllibrary;
    @property (nonatomic, retain) id<CAMetalDrawable> mtlbackbuffer;
    @property (nonatomic, retain) NSMutableArray *mtlpipelineprims;
    @property (nonatomic, retain) NSMutableArray *mtlpipelinecopynearest;
    @property (nonatomic, retain) NSMutableArray *mtlpipelinecopylinear;
    @property (nonatomic, retain) id<MTLBuffer> mtlbufclearverts;
    @property (nonatomic, retain) CAMetalLayer *mtllayer;
    @property (nonatomic, retain) MTLRenderPassDescriptor *mtlpassdesc;
@end

@implementation METAL_RenderData
@end

@interface METAL_TextureData : NSObject
    @property (nonatomic, retain) id<MTLTexture> mtltexture;
    @property (nonatomic, retain) NSMutableArray *mtlpipeline;
@end

@implementation METAL_TextureData
@end

static int
IsMetalAvailable(const SDL_SysWMinfo *syswm)
{
    if (syswm->subsystem != SDL_SYSWM_COCOA && syswm->subsystem != SDL_SYSWM_UIKIT) {
        return SDL_SetError("Metal render target only supports Cocoa and UIKit video targets at the moment.");
    }

    // this checks a weak symbol.
#if (defined(__MACOSX__) && (MAC_OS_X_VERSION_MIN_REQUIRED < 101100))
    if (MTLCreateSystemDefaultDevice == NULL) {  // probably on 10.10 or lower.
        return SDL_SetError("Metal framework not available on this system");
    }
#endif

    return 0;
}

static id<MTLRenderPipelineState>
MakePipelineState(METAL_RenderData *data, NSString *label, NSString *vertfn,
                  NSString *fragfn, const SDL_BlendMode blendmode)
{
    id<MTLFunction> mtlvertfn = [data.mtllibrary newFunctionWithName:vertfn];
    id<MTLFunction> mtlfragfn = [data.mtllibrary newFunctionWithName:fragfn];
    SDL_assert(mtlvertfn != nil);
    SDL_assert(mtlfragfn != nil);

    MTLRenderPipelineDescriptor *mtlpipedesc = [[MTLRenderPipelineDescriptor alloc] init];
    mtlpipedesc.vertexFunction = mtlvertfn;
    mtlpipedesc.fragmentFunction = mtlfragfn;
    mtlpipedesc.colorAttachments[0].pixelFormat = data.mtllayer.pixelFormat;

    switch (blendmode) {
        case SDL_BLENDMODE_BLEND:
            mtlpipedesc.colorAttachments[0].blendingEnabled = YES;
            mtlpipedesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            mtlpipedesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            mtlpipedesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            mtlpipedesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            break;

        case SDL_BLENDMODE_ADD:
            mtlpipedesc.colorAttachments[0].blendingEnabled = YES;
            mtlpipedesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            mtlpipedesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            mtlpipedesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            mtlpipedesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
            break;

        case SDL_BLENDMODE_MOD:
            mtlpipedesc.colorAttachments[0].blendingEnabled = YES;
            mtlpipedesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            mtlpipedesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorZero;
            mtlpipedesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorSourceColor;
            mtlpipedesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            mtlpipedesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
            break;

        default:
            mtlpipedesc.colorAttachments[0].blendingEnabled = NO;
            break;
    }

    mtlpipedesc.label = label;

    NSError *err = nil;
    id<MTLRenderPipelineState> retval = [data.mtldevice newRenderPipelineStateWithDescriptor:mtlpipedesc error:&err];
    SDL_assert(err == nil);
#if !__has_feature(objc_arc)
    [mtlpipedesc release];  // !!! FIXME: can these be reused for each creation, or does the pipeline obtain it?
    [mtlvertfn release];
    [mtlfragfn release];
    [label release];
#endif
    return retval;
}

static void
MakePipelineStates(METAL_RenderData *data, NSMutableArray *states,
                   NSString *label, NSString *vertfn, NSString *fragfn)
{
    [states addObject:MakePipelineState(data, [label stringByAppendingString:@" (blendmode=none)"], vertfn, fragfn, SDL_BLENDMODE_NONE)];
    [states addObject:MakePipelineState(data, [label stringByAppendingString:@" (blendmode=blend)"], vertfn, fragfn, SDL_BLENDMODE_BLEND)];
    [states addObject:MakePipelineState(data, [label stringByAppendingString:@" (blendmode=add)"], vertfn, fragfn, SDL_BLENDMODE_ADD)];
    [states addObject:MakePipelineState(data, [label stringByAppendingString:@" (blendmode=mod)"], vertfn, fragfn, SDL_BLENDMODE_MOD)];
}

static inline id<MTLRenderPipelineState>
ChoosePipelineState(NSMutableArray *states, const SDL_BlendMode blendmode)
{
    switch (blendmode) {
        case SDL_BLENDMODE_BLEND: return (id<MTLRenderPipelineState>)states[1];
        case SDL_BLENDMODE_ADD: return (id<MTLRenderPipelineState>)states[2];
        case SDL_BLENDMODE_MOD: return (id<MTLRenderPipelineState>)states[3];
        default: return (id<MTLRenderPipelineState>)states[0];
    }
    return nil;
}

static SDL_Renderer *
METAL_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Renderer *renderer = NULL;
    METAL_RenderData *data = NULL;
    SDL_SysWMinfo syswm;

    SDL_VERSION(&syswm.version);
    if (!SDL_GetWindowWMInfo(window, &syswm)) {
        return NULL;
    }

    if (IsMetalAvailable(&syswm) == -1) {
        return NULL;
    }

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = [[METAL_RenderData alloc] init];
    data.beginScene = YES;

#if __has_feature(objc_arc)
    renderer->driverdata = (void*)CFBridgingRetain(data);
#else
    renderer->driverdata = data;
#endif
    renderer->window = window;

#ifdef __MACOSX__
    id<MTLDevice> mtldevice = MTLCreateSystemDefaultDevice();  // !!! FIXME: MTLCopyAllDevices() can find other GPUs...
    if (mtldevice == nil) {
        SDL_free(renderer);
#if !__has_feature(objc_arc)
        [data release];
#endif
        SDL_SetError("Failed to obtain Metal device");
        return NULL;
    }

    // !!! FIXME: error checking on all of this.

    NSView *nsview = [syswm.info.cocoa.window contentView];

    // CAMetalLayer is available in QuartzCore starting at OSX 10.11
    CAMetalLayer *layer = [NSClassFromString( @"CAMetalLayer" ) layer];

    layer.device = mtldevice;
    //layer.pixelFormat = MTLPixelFormatBGRA8Unorm;  // !!! FIXME: MTLPixelFormatBGRA8Unorm_sRGB ?
    layer.framebufferOnly = YES;
    //layer.drawableSize = (CGSize) [nsview convertRectToBacking:[nsview bounds]].size;
    //layer.colorspace = nil;

    [nsview setWantsLayer:YES];
    [nsview setLayer:layer];

    [layer retain];
#else
    UIView *view = UIKit_Mtl_AddMetalView(window);
    CAMetalLayer *layer = (CAMetalLayer *)[view layer];
#endif

    data.mtldevice = layer.device;
    data.mtllayer = layer;
    data.mtlcmdqueue = [data.mtldevice newCommandQueue];
    data.mtlcmdqueue.label = @"SDL Metal Renderer";
    data.mtlpassdesc = [MTLRenderPassDescriptor renderPassDescriptor];

    NSError *err = nil;

    // The compiled .metallib is embedded in a static array in a header file
    // but the original shader source code is in SDL_shaders_metal.metal.
    dispatch_data_t mtllibdata = dispatch_data_create(sdl_metallib, sdl_metallib_len, dispatch_get_global_queue(0, 0), ^{});
    data.mtllibrary = [data.mtldevice newLibraryWithData:mtllibdata error:&err];
    SDL_assert(err == nil);
#if !__has_feature(objc_arc)
    dispatch_release(mtllibdata);
#endif
    data.mtllibrary.label = @"SDL Metal renderer shader library";

    data.mtlpipelineprims = [[NSMutableArray alloc] init];
    MakePipelineStates(data, data.mtlpipelineprims, @"SDL primitives pipeline", @"SDL_Solid_vertex", @"SDL_Solid_fragment");
    data.mtlpipelinecopynearest = [[NSMutableArray alloc] init];
    MakePipelineStates(data, data.mtlpipelinecopynearest, @"SDL texture pipeline (nearest)", @"SDL_Copy_vertex", @"SDL_Copy_fragment_nearest");
    data.mtlpipelinecopylinear = [[NSMutableArray alloc] init];
    MakePipelineStates(data, data.mtlpipelinecopylinear, @"SDL texture pipeline (linear)", @"SDL_Copy_vertex", @"SDL_Copy_fragment_linear");

    static const float clearverts[] = { 0, 0,  0, 3,  3, 0 };
    data.mtlbufclearverts = [data.mtldevice newBufferWithBytes:clearverts length:sizeof(clearverts) options:MTLResourceCPUCacheModeWriteCombined];
    data.mtlbufclearverts.label = @"SDL_RenderClear vertices";

    // !!! FIXME: force more clears here so all the drawables are sane to start, and our static buffers are definitely flushed.

    renderer->WindowEvent = METAL_WindowEvent;
    renderer->GetOutputSize = METAL_GetOutputSize;
    renderer->CreateTexture = METAL_CreateTexture;
    renderer->UpdateTexture = METAL_UpdateTexture;
    renderer->UpdateTextureYUV = METAL_UpdateTextureYUV;
    renderer->LockTexture = METAL_LockTexture;
    renderer->UnlockTexture = METAL_UnlockTexture;
    renderer->SetRenderTarget = METAL_SetRenderTarget;
    renderer->UpdateViewport = METAL_UpdateViewport;
    renderer->UpdateClipRect = METAL_UpdateClipRect;
    renderer->RenderClear = METAL_RenderClear;
    renderer->RenderDrawPoints = METAL_RenderDrawPoints;
    renderer->RenderDrawLines = METAL_RenderDrawLines;
    renderer->RenderFillRects = METAL_RenderFillRects;
    renderer->RenderCopy = METAL_RenderCopy;
    renderer->RenderCopyEx = METAL_RenderCopyEx;
    renderer->RenderReadPixels = METAL_RenderReadPixels;
    renderer->RenderPresent = METAL_RenderPresent;
    renderer->DestroyTexture = METAL_DestroyTexture;
    renderer->DestroyRenderer = METAL_DestroyRenderer;
    renderer->GetMetalLayer = METAL_GetMetalLayer;
    renderer->GetMetalCommandEncoder = METAL_GetMetalCommandEncoder;

    renderer->info = METAL_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    // !!! FIXME: how do you control this in Metal?
    renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;

    return renderer;
}

static void METAL_ActivateRenderer(SDL_Renderer * renderer)
{
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    if (data.beginScene) {
        data.beginScene = NO;
        data.mtlbackbuffer = [data.mtllayer nextDrawable];
        SDL_assert(data.mtlbackbuffer);
        data.mtlpassdesc.colorAttachments[0].texture = data.mtlbackbuffer.texture;
        data.mtlpassdesc.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        data.mtlcmdbuffer = [data.mtlcmdqueue commandBuffer];
        data.mtlcmdencoder = [data.mtlcmdbuffer renderCommandEncoderWithDescriptor:data.mtlpassdesc];
        data.mtlcmdencoder.label = @"SDL metal renderer start of frame";

        // Set up our current renderer state for the next frame...
        METAL_UpdateViewport(renderer);
        METAL_UpdateClipRect(renderer);
    }
}

static void
METAL_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
    if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED ||
        event->event == SDL_WINDOWEVENT_SHOWN ||
        event->event == SDL_WINDOWEVENT_HIDDEN) {
        // !!! FIXME: write me
    }
}

static int
METAL_GetOutputSize(SDL_Renderer * renderer, int *w, int *h)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    *w = (int) data.mtlbackbuffer.texture.width;
    *h = (int) data.mtlbackbuffer.texture.height;
    return 0;
}}

static int
METAL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{ @autoreleasepool {
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    MTLPixelFormat mtlpixfmt;

    switch (texture->format) {
        case SDL_PIXELFORMAT_ABGR8888: mtlpixfmt = MTLPixelFormatRGBA8Unorm; break;
        case SDL_PIXELFORMAT_ARGB8888: mtlpixfmt = MTLPixelFormatBGRA8Unorm; break;
        default: return SDL_SetError("Texture format %s not supported by Metal", SDL_GetPixelFormatName(texture->format));
    }

    MTLTextureDescriptor *mtltexdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlpixfmt
                                            width:(NSUInteger)texture->w height:(NSUInteger)texture->h mipmapped:NO];
 
    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
        mtltexdesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    } else {
        mtltexdesc.usage = MTLTextureUsageShaderRead;
    }
    //mtltexdesc.resourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeManaged;
    //mtltexdesc.storageMode = MTLStorageModeManaged;
    
    id<MTLTexture> mtltexture = [data.mtldevice newTextureWithDescriptor:mtltexdesc];
    if (mtltexture == nil) {
        return SDL_SetError("Texture allocation failed");
    }

    METAL_TextureData *texturedata = [[METAL_TextureData alloc] init];
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    if (!hint || *hint == '0' || SDL_strcasecmp(hint, "nearest") == 0) {
        texturedata.mtlpipeline = data.mtlpipelinecopynearest;
    } else {
        texturedata.mtlpipeline = data.mtlpipelinecopylinear;
    }
    texturedata.mtltexture = mtltexture;

    texture->driverdata = (void*)CFBridgingRetain(texturedata);

    return 0;
}}

static int
METAL_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{ @autoreleasepool {
    // !!! FIXME: this is a synchronous call; it doesn't return until data is uploaded in some form.
    // !!! FIXME:  Maybe move this off to a thread that marks the texture as uploaded and only stall the main thread if we try to
    // !!! FIXME:  use this texture before the marking is done? Is it worth it? Or will we basically always be uploading a bunch of
    // !!! FIXME:  stuff way ahead of time and/or using it immediately after upload?
    id<MTLTexture> mtltexture = ((__bridge METAL_TextureData *)texture->driverdata).mtltexture;
    [mtltexture replaceRegion:MTLRegionMake2D(rect->x, rect->y, rect->w, rect->h) mipmapLevel:0 withBytes:pixels bytesPerRow:pitch];
    return 0;
}}

static int
METAL_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * rect,
                    const Uint8 *Yplane, int Ypitch,
                    const Uint8 *Uplane, int Upitch,
                    const Uint8 *Vplane, int Vpitch)
{
    return SDL_Unsupported();  // !!! FIXME
}

static int
METAL_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * rect, void **pixels, int *pitch)
{
    return SDL_Unsupported();   // !!! FIXME: write me
}

static void
METAL_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    // !!! FIXME: write me
}

static int
METAL_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    // commit the current command buffer, so that any work on a render target
    //  will be available to the next one we're about to queue up.
    [data.mtlcmdencoder endEncoding];
    [data.mtlcmdbuffer commit];

    id<MTLTexture> mtltexture = texture ? ((__bridge METAL_TextureData *)texture->driverdata).mtltexture : data.mtlbackbuffer.texture;
    data.mtlpassdesc.colorAttachments[0].texture = mtltexture;
    // !!! FIXME: this can be MTLLoadActionDontCare for textures (not the backbuffer) if SDL doesn't guarantee the texture contents should survive.
    data.mtlpassdesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    data.mtlcmdbuffer = [data.mtlcmdqueue commandBuffer];
    data.mtlcmdencoder = [data.mtlcmdbuffer renderCommandEncoderWithDescriptor:data.mtlpassdesc];
    data.mtlcmdencoder.label = texture ? @"SDL metal renderer render texture" : @"SDL metal renderer backbuffer";

    // The higher level will reset the viewport and scissor after this call returns.

    return 0;
}}

static int
METAL_SetOrthographicProjection(SDL_Renderer *renderer, int w, int h)
{ @autoreleasepool {
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    float projection[4][4];

    if (!w || !h) {
        return 0;
    }

    /* Prepare an orthographic projection */
    projection[0][0] = 2.0f / w;
    projection[0][1] = 0.0f;
    projection[0][2] = 0.0f;
    projection[0][3] = 0.0f;
    projection[1][0] = 0.0f;
    projection[1][1] = -2.0f / h;
    projection[1][2] = 0.0f;
    projection[1][3] = 0.0f;
    projection[2][0] = 0.0f;
    projection[2][1] = 0.0f;
    projection[2][2] = 0.0f;
    projection[2][3] = 0.0f;
    projection[3][0] = -1.0f;
    projection[3][1] = 1.0f;
    projection[3][2] = 0.0f;
    projection[3][3] = 1.0f;

    // !!! FIXME: This should be in a buffer...
    [data.mtlcmdencoder setVertexBytes:projection length:sizeof(float)*16 atIndex:2];
    return 0;
}}

static int
METAL_UpdateViewport(SDL_Renderer * renderer)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    MTLViewport viewport;
    viewport.originX = renderer->viewport.x;
    viewport.originY = renderer->viewport.y;
    viewport.width = renderer->viewport.w;
    viewport.height = renderer->viewport.h;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [data.mtlcmdencoder setViewport:viewport];
    METAL_SetOrthographicProjection(renderer, renderer->viewport.w, renderer->viewport.h);
    return 0;
}}

static int
METAL_UpdateClipRect(SDL_Renderer * renderer)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    MTLScissorRect mtlrect;
    // !!! FIXME: should this care about the viewport?
    if (renderer->clipping_enabled) {
        const SDL_Rect *rect = &renderer->clip_rect;
        mtlrect.x = renderer->viewport.x + rect->x;
        mtlrect.y = renderer->viewport.x + rect->y;
        mtlrect.width = rect->w;
        mtlrect.height = rect->h;
    } else {
        mtlrect.x = renderer->viewport.x;
        mtlrect.y = renderer->viewport.y;
        mtlrect.width = renderer->viewport.w;
        mtlrect.height = renderer->viewport.h;
    }
    if (mtlrect.width > 0 && mtlrect.height > 0) {
        [data.mtlcmdencoder setScissorRect:mtlrect];
    }
    return 0;
}}

static int
METAL_RenderClear(SDL_Renderer * renderer)
{ @autoreleasepool {
    // We could dump the command buffer and force a clear on a new one, but this will respect the scissor state.
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    MTLViewport viewport;  // RenderClear ignores the viewport state, though, so reset that.
    viewport.originX = viewport.originY = 0.0;
    viewport.width = data.mtlpassdesc.colorAttachments[0].texture.width;
    viewport.height = data.mtlpassdesc.colorAttachments[0].texture.height;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;

    // Draw a simple filled fullscreen triangle now.
    METAL_SetOrthographicProjection(renderer, 1, 1);
    [data.mtlcmdencoder setViewport:viewport];
    [data.mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data.mtlpipelineprims, renderer->blendMode)];
    [data.mtlcmdencoder setVertexBuffer:data.mtlbufclearverts offset:0 atIndex:0];
    [data.mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];
    [data.mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

    // reset the viewport for the rest of our usual drawing work...
    viewport.originX = renderer->viewport.x;
    viewport.originY = renderer->viewport.y;
    viewport.width = renderer->viewport.w;
    viewport.height = renderer->viewport.h;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [data.mtlcmdencoder setViewport:viewport];
    METAL_SetOrthographicProjection(renderer, renderer->viewport.w, renderer->viewport.h);

    return 0;
}}

// normalize a value from 0.0f to len into 0.0f to 1.0f.
static inline float
normtex(const float _val, const float len)
{
    const float val = (_val < 0.0f) ? 0.0f : (_val > len) ? len : _val;
    return ((val + 0.5f) / len);
}

static int
DrawVerts(SDL_Renderer * renderer, const SDL_FPoint * points, int count,
          const MTLPrimitiveType primtype)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);

    const size_t vertlen = sizeof(SDL_FPoint) * count;
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    [data.mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data.mtlpipelineprims, renderer->blendMode)];
    [data.mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];
    [data.mtlcmdencoder setVertexBytes:points length:vertlen atIndex:0];
    [data.mtlcmdencoder drawPrimitives:primtype vertexStart:0 vertexCount:count];
    return 0;
}}

static int
METAL_RenderDrawPoints(SDL_Renderer * renderer, const SDL_FPoint * points, int count)
{
    return DrawVerts(renderer, points, count, MTLPrimitiveTypePoint);
}

static int
METAL_RenderDrawLines(SDL_Renderer * renderer, const SDL_FPoint * points, int count)
{
    return DrawVerts(renderer, points, count, MTLPrimitiveTypeLineStrip);
}

static int
METAL_RenderFillRects(SDL_Renderer * renderer, const SDL_FRect * rects, int count)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    [data.mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data.mtlpipelineprims, renderer->blendMode)];
    [data.mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];

    for (int i = 0; i < count; i++, rects++) {
        if ((rects->w <= 0.0f) || (rects->h <= 0.0f)) continue;

        const float verts[] = {
            rects->x, rects->y + rects->h,
            rects->x, rects->y,
            rects->x + rects->w, rects->y + rects->h,
            rects->x + rects->w, rects->y,
        };

        [data.mtlcmdencoder setVertexBytes:verts length:sizeof(verts) atIndex:0];
        [data.mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }

    return 0;
}}

static int
METAL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    METAL_TextureData *texturedata = (__bridge METAL_TextureData *)texture->driverdata;
    const float texw = (float) texturedata.mtltexture.width;
    const float texh = (float) texturedata.mtltexture.height;

    const float xy[] = {
        dstrect->x, dstrect->y + dstrect->h,
        dstrect->x, dstrect->y,
        dstrect->x + dstrect->w, dstrect->y + dstrect->h,
        dstrect->x + dstrect->w, dstrect->y
    };

    const float uv[] = {
        normtex(srcrect->x, texw), normtex(srcrect->y + srcrect->h, texh),
        normtex(srcrect->x, texw), normtex(srcrect->y, texh),
        normtex(srcrect->x + srcrect->w, texw), normtex(srcrect->y + srcrect->h, texh),
        normtex(srcrect->x + srcrect->w, texw), normtex(srcrect->y, texh)
    };

    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (texture->modMode) {
        color[0] = ((float)texture->r) / 255.0f;
        color[1] = ((float)texture->g) / 255.0f;
        color[2] = ((float)texture->b) / 255.0f;
        color[3] = ((float)texture->a) / 255.0f;
    }

    [data.mtlcmdencoder setRenderPipelineState:ChoosePipelineState(texturedata.mtlpipeline, texture->blendMode)];
    [data.mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];
    [data.mtlcmdencoder setFragmentTexture:texturedata.mtltexture atIndex:0];
    [data.mtlcmdencoder setVertexBytes:xy length:sizeof(xy) atIndex:0];
    [data.mtlcmdencoder setVertexBytes:uv length:sizeof(uv) atIndex:1];
    [data.mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    return 0;
}}

static int
METAL_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_FRect * dstrect,
              const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    return SDL_Unsupported();  // !!! FIXME
}

static int
METAL_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    // !!! FIXME: this probably needs to commit the current command buffer, and probably waitUntilCompleted
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    MTLRenderPassColorAttachmentDescriptor *colorAttachment = data.mtlpassdesc.colorAttachments[0];
    id<MTLTexture> mtltexture = colorAttachment.texture;
    MTLRegion mtlregion;

    mtlregion.origin.x = rect->x;
    mtlregion.origin.y = rect->y;
    mtlregion.origin.z = 0;
    mtlregion.size.width = rect->w;
    mtlregion.size.height = rect->w;
    mtlregion.size.depth = 1;

    // we only do BGRA8 or RGBA8 at the moment, so 4 will do.
    const int temp_pitch = rect->w * 4;
    void *temp_pixels = SDL_malloc(temp_pitch * rect->h);
    if (!temp_pixels) {
        return SDL_OutOfMemory();
    }

    [mtltexture getBytes:temp_pixels bytesPerRow:temp_pitch fromRegion:mtlregion mipmapLevel:0];

    const Uint32 temp_format = (mtltexture.pixelFormat == MTLPixelFormatBGRA8Unorm) ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_ABGR8888;
    const int status = SDL_ConvertPixels(rect->w, rect->h, temp_format, temp_pixels, temp_pitch, pixel_format, pixels, pitch);
    SDL_free(temp_pixels);
    return status;
}}

static void
METAL_RenderPresent(SDL_Renderer * renderer)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;

    [data.mtlcmdencoder endEncoding];
    [data.mtlcmdbuffer presentDrawable:data.mtlbackbuffer];
    [data.mtlcmdbuffer commit];
    data.mtlcmdencoder = nil;
    data.mtlcmdbuffer = nil;
    data.mtlbackbuffer = nil;
    data.beginScene = YES;
}}

static void
METAL_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{ @autoreleasepool {
    METAL_TextureData *texturedata = CFBridgingRelease(texture->driverdata);
#if __has_feature(objc_arc)
    texturedata = nil;
#else
    [texturedata.mtltexture release];
    [texturedata release];
#endif
    texture->driverdata = NULL;
}}

static void
METAL_DestroyRenderer(SDL_Renderer * renderer)
{ @autoreleasepool {
    if (renderer->driverdata) {
        METAL_RenderData *data = CFBridgingRelease(renderer->driverdata);

        if (data.mtlcmdencoder != nil) {
            [data.mtlcmdencoder endEncoding];
        }

#if !__has_feature(objc_arc)
        [data.mtlbackbuffer release];
        [data.mtlcmdencoder release];
        [data.mtlcmdbuffer release];
        [data.mtlcmdqueue release];
        for (int i = 0; i < 4; i++) {
            [data.mtlpipelineprims[i] release];
            [data.mtlpipelinecopynearest[i] release];
            [data.mtlpipelinecopylinear[i] release];
        }
        [data.mtlpipelineprims release];
        [data.mtlpipelinecopynearest release];
        [data.mtlpipelinecopylinear release];
        [data.mtlbufclearverts release];
        [data.mtllibrary release];
        [data.mtldevice release];
        [data.mtlpassdesc release];
        [data.mtllayer release];
#endif
    }

    SDL_free(renderer);
}}

void *METAL_GetMetalLayer(SDL_Renderer * renderer)
{ @autoreleasepool {
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    return (__bridge void*)data.mtllayer;
}}

void *METAL_GetMetalCommandEncoder(SDL_Renderer * renderer)
{ @autoreleasepool {
    METAL_ActivateRenderer(renderer);
    METAL_RenderData *data = (__bridge METAL_RenderData *) renderer->driverdata;
    return (__bridge void*)data.mtlcmdencoder;
}}

#endif /* SDL_VIDEO_RENDER_METAL && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
