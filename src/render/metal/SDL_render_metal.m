/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

// these are in SDL_shaders_metal.c, regenerate it with build-metal-shaders.sh
extern const unsigned char sdl_metallib[];
extern const unsigned int sdl_metallib_len;


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

SDL_RenderDriver METAL_RenderDriver = {
    METAL_CreateRenderer,
    {
     "metal",
     (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE),
     2,
     {SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_ABGR8888},
     4096,  // !!! FIXME: how do you query Metal for this?
     4096}
};

typedef struct METAL_BufferList
{
    id<MTLBuffer> mtlbuffer;
    struct METAL_BufferPool *next;
} METAL_BufferList;

typedef struct
{
    id<MTLDevice> mtldevice;
    id<MTLCommandQueue> mtlcmdqueue;
    id<MTLCommandBuffer> mtlcmdbuffer;
    id<MTLRenderCommandEncoder> mtlcmdencoder;
    id<MTLLibrary> mtllibrary;
    id<CAMetalDrawable> mtlbackbuffer;
    id<MTLRenderPipelineState> mtlpipelineprims[4];
    id<MTLRenderPipelineState> mtlpipelinecopy[4];
    id<MTLBuffer> mtlbufclearverts;
    CAMetalLayer *mtllayer;
    MTLRenderPassDescriptor *mtlpassdesc;
} METAL_RenderData;


static int
IsMetalAvailable(const SDL_SysWMinfo *syswm)
{
    if (syswm->subsystem != SDL_SYSWM_COCOA) {  // !!! FIXME: SDL_SYSWM_UIKIT for iOS, too!
        return SDL_SetError("Metal render target only supports Cocoa video target at the moment.");
    }

    // this checks a weak symbol.
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101100
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
    id<MTLFunction> mtlvertfn = [data->mtllibrary newFunctionWithName:vertfn];
    id<MTLFunction> mtlfragfn = [data->mtllibrary newFunctionWithName:fragfn];
    SDL_assert(mtlvertfn != nil);
    SDL_assert(mtlfragfn != nil);

    MTLRenderPipelineDescriptor *mtlpipedesc = [[MTLRenderPipelineDescriptor alloc] init];
    mtlpipedesc.vertexFunction = mtlvertfn;
    mtlpipedesc.fragmentFunction = mtlfragfn;
    mtlpipedesc.colorAttachments[0].pixelFormat = data->mtlbackbuffer.texture.pixelFormat;

    switch (blendmode) {
        case SDL_BLENDMODE_NONE:
            mtlpipedesc.colorAttachments[0].blendingEnabled = NO;
            break;

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
    }

    mtlpipedesc.label = label;

    NSError *err = nil;
    id<MTLRenderPipelineState> retval = [data->mtldevice newRenderPipelineStateWithDescriptor:mtlpipedesc error:&err];
    SDL_assert(err == nil);
    [mtlpipedesc release];  // !!! FIXME: can these be reused for each creation, or does the pipeline obtain it?
    [mtlvertfn release];
    [mtlfragfn release];
    [label release];

    return retval;
}

static void
MakePipelineStates(METAL_RenderData *data, id<MTLRenderPipelineState> *states,
                   NSString *label, NSString *vertfn, NSString *fragfn)
{
    int i = 0;
    states[i++] = MakePipelineState(data, [label stringByAppendingString:@" (blendmode=none)"], vertfn, fragfn, SDL_BLENDMODE_NONE);
    states[i++] = MakePipelineState(data, [label stringByAppendingString:@" (blendmode=blend)"], vertfn, fragfn, SDL_BLENDMODE_BLEND);
    states[i++] = MakePipelineState(data, [label stringByAppendingString:@" (blendmode=add)"], vertfn, fragfn, SDL_BLENDMODE_ADD);
    states[i++] = MakePipelineState(data, [label stringByAppendingString:@" (blendmode=mod)"], vertfn, fragfn, SDL_BLENDMODE_MOD);
}

static inline id<MTLRenderPipelineState>
ChoosePipelineState(id<MTLRenderPipelineState> *states, const SDL_BlendMode blendmode)
{
    switch (blendmode) {
        case SDL_BLENDMODE_NONE: return states[0];
        case SDL_BLENDMODE_BLEND: return states[1];
        case SDL_BLENDMODE_ADD: return states[2];
        case SDL_BLENDMODE_MOD: return states[3];
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

    data = (METAL_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return NULL;
    }

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_free(data);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->driverdata = data;
    renderer->window = window;

    data->mtldevice = MTLCreateSystemDefaultDevice();  // !!! FIXME: MTLCopyAllDevices() can find other GPUs...
    if (data->mtldevice == nil) {
        SDL_free(renderer);
        SDL_free(data);
        SDL_SetError("Failed to obtain Metal device");
        return NULL;
    }

    // !!! FIXME: error checking on all of this.

    NSView *nsview = [syswm.info.cocoa.window contentView];

    // !!! FIXME: on iOS, we need to override +[UIView layerClass] to return [CAMetalLayer class] right from the start, and that's more complicated.
    CAMetalLayer *layer = [CAMetalLayer layer];

    layer.device = data->mtldevice;
    //layer.pixelFormat = MTLPixelFormatBGRA8Unorm;  // !!! FIXME: MTLPixelFormatBGRA8Unorm_sRGB ?
    layer.framebufferOnly = YES;
    //layer.drawableSize = (CGSize) [nsview convertRectToBacking:[nsview bounds]].size;
    //layer.colorspace = nil;

    [nsview setWantsLayer:YES];
    [nsview setLayer:layer];

    [layer retain];
    data->mtllayer = layer;
    data->mtlcmdqueue = [data->mtldevice newCommandQueue];
    data->mtlcmdqueue.label = @"SDL Metal Renderer";

    data->mtlpassdesc = [MTLRenderPassDescriptor renderPassDescriptor];  // !!! FIXME: is this autoreleased?

    // we don't specify a depth or stencil buffer because the render API doesn't currently use them.
    MTLRenderPassColorAttachmentDescriptor *colorAttachment = data->mtlpassdesc.colorAttachments[0];
    data->mtlbackbuffer = [data->mtllayer nextDrawable];
    colorAttachment.texture = data->mtlbackbuffer.texture;
    colorAttachment.loadAction = MTLLoadActionClear;
    colorAttachment.clearColor = MTLClearColorMake(0.0f, 0.0f, 0.0f, 1.0f);
    data->mtlcmdbuffer = [data->mtlcmdqueue commandBuffer];

    // Just push a clear to the screen to start so we're in a good state.
    data->mtlcmdencoder = [data->mtlcmdbuffer renderCommandEncoderWithDescriptor:data->mtlpassdesc];
    data->mtlcmdencoder.label = @"Initial drawable clear";

    METAL_RenderPresent(renderer);

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

    renderer->info = METAL_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    // !!! FIXME: how do you control this in Metal?
    renderer->info.flags |= SDL_RENDERER_PRESENTVSYNC;

    NSError *err = nil;

    // The compiled .metallib is embedded in a static array in SDL_shaders_metal.c,
    //  but the original shader source code is in SDL_shaders_metal.metal.
    dispatch_data_t mtllibdata = dispatch_data_create(sdl_metallib, sdl_metallib_len, dispatch_get_global_queue(0, 0), ^{});
    data->mtllibrary = [data->mtldevice newLibraryWithData:mtllibdata error:&err];
    SDL_assert(err == nil);
    dispatch_release(mtllibdata);
    data->mtllibrary.label = @"SDL Metal renderer shader library";

    MakePipelineStates(data, data->mtlpipelineprims, @"SDL primitives pipeline", @"SDL_Simple_vertex", @"SDL_Simple_fragment");
    MakePipelineStates(data, data->mtlpipelinecopy, @"SDL_RenderCopy pipeline", @"SDL_Copy_vertex", @"SDL_Copy_fragment");

    static const float clearverts[] = { -1, -1, -1, 1, 1, 1, 1, -1, -1, -1 };
    data->mtlbufclearverts = [data->mtldevice newBufferWithBytes:clearverts length:sizeof(clearverts) options:MTLResourceCPUCacheModeWriteCombined|MTLResourceStorageModePrivate];
    data->mtlbufclearverts.label = @"SDL_RenderClear vertices";

    // !!! FIXME: force more clears here so all the drawables are sane to start, and our static buffers are definitely flushed.

    return renderer;
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
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    *w = (int) data->mtlbackbuffer.texture.width;
    *h = (int) data->mtlbackbuffer.texture.height;
    return 0;
}

static int
METAL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    MTLPixelFormat mtlpixfmt;

    switch (texture->format) {
        case SDL_PIXELFORMAT_ABGR8888: mtlpixfmt = MTLPixelFormatRGBA8Unorm; break;
        case SDL_PIXELFORMAT_ARGB8888: mtlpixfmt = MTLPixelFormatBGRA8Unorm; break;
        default: return SDL_SetError("Texture format %s not supported by Metal", SDL_GetPixelFormatName(texture->format));
    }

    // !!! FIXME: autorelease or nah?
    MTLTextureDescriptor *mtltexdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlpixfmt
                                            width:(NSUInteger)texture->w height:(NSUInteger)texture->h mipmapped:NO];

    id<MTLTexture> mtltexture = [data->mtldevice newTextureWithDescriptor:mtltexdesc];
    [mtltexdesc release];
    if (mtltexture == nil) {
        return SDL_SetError("Texture allocation failed");
    }

    texture->driverdata = mtltexture;

    return 0;
}

static int
METAL_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{
    // !!! FIXME: this is a synchronous call; it doesn't return until data is uploaded in some form.
    // !!! FIXME:  Maybe move this off to a thread that marks the texture as uploaded and only stall the main thread if we try to
    // !!! FIXME:  use this texture before the marking is done? Is it worth it? Or will we basically always be uploading a bunch of
    // !!! FIXME:  stuff way ahead of time and/or using it immediately after upload?
    id<MTLTexture> mtltexture = (id<MTLTexture>) texture->driverdata;
    [mtltexture replaceRegion:MTLRegionMake2D(rect->x, rect->y, rect->w, rect->h) mipmapLevel:0 withBytes:pixels bytesPerRow:pitch];
    return 0;
}

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
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    id<MTLTexture> mtltexture = texture ? (id<MTLTexture>) texture->driverdata : nil;
    data->mtlpassdesc.colorAttachments[0].texture = mtltexture;
    return 0;
}

static int
METAL_UpdateViewport(SDL_Renderer * renderer)
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    if (data->mtlcmdencoder != nil) {
        MTLViewport viewport;
        viewport.originX = renderer->viewport.x;
        viewport.originY = renderer->viewport.y;
        viewport.width = renderer->viewport.w;
        viewport.height = renderer->viewport.h;
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        [data->mtlcmdencoder setViewport:viewport];
    }
    return 0;
}

static int
METAL_UpdateClipRect(SDL_Renderer * renderer)
{
    // !!! FIXME: should this care about the viewport?
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    if (data->mtlcmdencoder != nil) {
        MTLScissorRect mtlrect;
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
        [data->mtlcmdencoder setScissorRect:mtlrect];
    }
    return 0;
}

static int
METAL_RenderClear(SDL_Renderer * renderer)
{
    // We could dump the command buffer and force a clear on a new one, but this will respect the scissor state.
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    MTLViewport viewport;  // RenderClear ignores the viewport state, though, so reset that.
    viewport.originX = viewport.originY = 0.0;
    viewport.width = data->mtlbackbuffer.texture.width;
    viewport.height = data->mtlbackbuffer.texture.height;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;

    // Draw as if we're doing a simple filled rect to the screen now.
    [data->mtlcmdencoder setViewport:viewport];
    [data->mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data->mtlpipelineprims, renderer->blendMode)];
    [data->mtlcmdencoder setVertexBuffer:data->mtlbufclearverts offset:0 atIndex:0];
    [data->mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];
    [data->mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:5];

    // reset the viewport for the rest of our usual drawing work...
    viewport.originX = renderer->viewport.x;
    viewport.originY = renderer->viewport.y;
    viewport.width = renderer->viewport.w;
    viewport.height = renderer->viewport.h;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [data->mtlcmdencoder setViewport:viewport];

    return 0;
}

// normalize a value from 0.0f to len into -1.0f to 1.0f.
static inline float
norm(const float _val, const float len)
{
    const float val = (_val < 0.0f) ? 0.0f : (_val > len) ? len : _val;
    return ((val / len) * 2.0f) - 1.0f;  // !!! FIXME: is this right?
}

// normalize a value from 0.0f to len into -1.0f to 1.0f.
static inline float
normy(const float _val, const float len)
{
    return norm(len - ((_val < 0.0f) ? 0.0f : (_val > len) ? len : _val), len);
}

// normalize a value from 0.0f to len into 0.0f to 1.0f.
static inline float
normtex(const float _val, const float len)
{
    const float val = (_val < 0.0f) ? 0.0f : (_val > len) ? len : _val;
    return (val / len);
}

static int
DrawVerts(SDL_Renderer * renderer, const SDL_FPoint * points, int count,
          const MTLPrimitiveType primtype)
{
    const size_t vertlen = (sizeof (float) * 2) * count;
    float *verts = SDL_malloc(vertlen);
    if (!verts) {
        return SDL_OutOfMemory();
    }

    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    [data->mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data->mtlpipelineprims, renderer->blendMode)];
    [data->mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];

    const float w = (float) data->mtlpassdesc.colorAttachments[0].texture.width;
    const float h = (float) data->mtlpassdesc.colorAttachments[0].texture.height;

    // !!! FIXME: we can convert this in the shader. This will save the malloc and for-loop, but we still need to upload.
    float *ptr = verts;
    for (int i = 0; i < count; i++, points++) {
        *ptr = norm(points->x, w); ptr++;
        *ptr = normy(points->y, h); ptr++;
    }

    [data->mtlcmdencoder setVertexBytes:verts length:vertlen atIndex:0];
    [data->mtlcmdencoder drawPrimitives:primtype vertexStart:0 vertexCount:count];

    SDL_free(verts);
    return 0;
}

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
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;

    // !!! FIXME: render color should live in a dedicated uniform buffer.
    const float color[4] = { ((float)renderer->r) / 255.0f, ((float)renderer->g) / 255.0f, ((float)renderer->b) / 255.0f, ((float)renderer->a) / 255.0f };

    [data->mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data->mtlpipelineprims, renderer->blendMode)];
    [data->mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];

    const float w = (float) data->mtlpassdesc.colorAttachments[0].texture.width;
    const float h = (float) data->mtlpassdesc.colorAttachments[0].texture.height;

    for (int i = 0; i < count; i++, rects++) {
        if ((rects->w <= 0.0f) || (rects->h <= 0.0f)) continue;

        const float verts[] = {
            norm(rects->x, w), normy(rects->y + rects->h, h),
            norm(rects->x, w), normy(rects->y, h),
            norm(rects->x + rects->w, w), normy(rects->y, h),
            norm(rects->x, w), normy(rects->y + rects->h, h),
            norm(rects->x + rects->w, w), normy(rects->y + rects->h, h)
        };

        [data->mtlcmdencoder setVertexBytes:verts length:sizeof(verts) atIndex:0];
        [data->mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:5];
    }

    return 0;
}

static int
METAL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    id<MTLTexture> mtltexture = (id<MTLTexture>) texture->driverdata;
    const float w = (float) data->mtlpassdesc.colorAttachments[0].texture.width;
    const float h = (float) data->mtlpassdesc.colorAttachments[0].texture.height;
    const float texw = (float) mtltexture.width;
    const float texh = (float) mtltexture.height;

    const float xy[] = {
        norm(dstrect->x, w), normy(dstrect->y + dstrect->h, h),
        norm(dstrect->x, w), normy(dstrect->y, h),
        norm(dstrect->x + dstrect->w, w), normy(dstrect->y, h),
        norm(dstrect->x, w), normy(dstrect->y + dstrect->h, h),
        norm(dstrect->x + dstrect->w, w), normy(dstrect->y + dstrect->h, h)
    };

    const float uv[] = {
        normtex(srcrect->x, texw), normtex(srcrect->y + srcrect->h, texh),
        normtex(srcrect->x, texw), normtex(srcrect->y, texh),
        normtex(srcrect->x + srcrect->w, texw), normtex(srcrect->y, texh),
        normtex(srcrect->x, texw), normtex(srcrect->y + srcrect->h, texh),
        normtex(srcrect->x + srcrect->w, texw), normtex(srcrect->y + srcrect->h, texh)
    };

    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (texture->modMode) {
        color[0] = ((float)texture->r) / 255.0f;
        color[1] = ((float)texture->g) / 255.0f;
        color[2] = ((float)texture->b) / 255.0f;
        color[3] = ((float)texture->a) / 255.0f;
    }

    [data->mtlcmdencoder setRenderPipelineState:ChoosePipelineState(data->mtlpipelinecopy, texture->blendMode)];
    [data->mtlcmdencoder setFragmentBytes:color length:sizeof(color) atIndex:0];
    [data->mtlcmdencoder setFragmentTexture:mtltexture atIndex:0];
    [data->mtlcmdencoder setVertexBytes:xy length:sizeof(xy) atIndex:0];
    [data->mtlcmdencoder setVertexBytes:uv length:sizeof(uv) atIndex:1];
    [data->mtlcmdencoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:5];

    return 0;
}

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
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    MTLRenderPassColorAttachmentDescriptor *colorAttachment = data->mtlpassdesc.colorAttachments[0];
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
}

static void
METAL_RenderPresent(SDL_Renderer * renderer)
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;
    MTLRenderPassColorAttachmentDescriptor *colorAttachment = data->mtlpassdesc.colorAttachments[0];
    id<CAMetalDrawable> mtlbackbuffer = data->mtlbackbuffer;

    [data->mtlcmdencoder endEncoding];
    [data->mtlcmdbuffer presentDrawable:mtlbackbuffer];

    [data->mtlcmdbuffer addCompletedHandler:^(id <MTLCommandBuffer> mtlcmdbuffer){
        [mtlbackbuffer release];
    }];

    [data->mtlcmdbuffer commit];

    // Start next frame, once we can.
    // we don't specify a depth or stencil buffer because the render API doesn't currently use them.
    data->mtlbackbuffer = [data->mtllayer nextDrawable];
    SDL_assert(data->mtlbackbuffer);
    colorAttachment.texture = data->mtlbackbuffer.texture;
    colorAttachment.loadAction = MTLLoadActionDontCare;
    data->mtlcmdbuffer = [data->mtlcmdqueue commandBuffer];
    data->mtlcmdencoder = [data->mtlcmdbuffer renderCommandEncoderWithDescriptor:data->mtlpassdesc];
    data->mtlcmdencoder.label = @"SDL metal renderer frame";

    // Set up our current renderer state for the next frame...
    METAL_UpdateViewport(renderer);
    METAL_UpdateClipRect(renderer);
}

static void
METAL_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    id<MTLTexture> mtltexture = (id<MTLTexture>) texture->driverdata;
    [mtltexture release];
    texture->driverdata = NULL;
}

static void
METAL_DestroyRenderer(SDL_Renderer * renderer)
{
    METAL_RenderData *data = (METAL_RenderData *) renderer->driverdata;

    if (data) {
        int i;
        [data->mtlcmdencoder endEncoding];
        [data->mtlcmdencoder release];
        [data->mtlcmdbuffer release];
        [data->mtlcmdqueue release];
        for (i = 0; i < 4; i++) {
            [data->mtlpipelineprims[i] release];
            [data->mtlpipelinecopy[i] release];
        }
        [data->mtlbufclearverts release];
        [data->mtllibrary release];
        [data->mtldevice release];
        [data->mtlpassdesc release];
        [data->mtllayer release];
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_METAL && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
