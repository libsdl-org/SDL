/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_GPU_METAL

/* The Apple Metal driver for the GPU subsystem. */

#include "SDL.h"
#include "SDL_syswm.h"
#include "../SDL_sysgpu.h"
#include "../../video/SDL_sysvideo.h"

#include <Availability.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#ifdef __MACOSX__
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#endif

#if !__has_feature(objc_arc)
#error Please build with ARC support.
#endif


@interface METAL_GpuDeviceData : NSObject
    @property (nonatomic, retain) id<MTLDevice> mtldevice;
    @property (nonatomic, retain) id<MTLCommandQueue> mtlcmdqueue;
@end

@implementation METAL_GpuDeviceData
@end


@interface METAL_GpuWindowData : NSObject
    @property (nonatomic, assign) SDL_MetalView mtlview;
    @property (nonatomic, retain) CAMetalLayer *mtllayer;
    @property (nonatomic, retain) id<CAMetalDrawable> mtldrawable;  // current backbuffer
@end

@implementation METAL_GpuWindowData
@end


@interface METAL_GpuBufferData : NSObject  // this covers CPU and GPU buffers.
    @property (nonatomic, retain) id<MTLBuffer> mtlbuffer;
@end

@implementation METAL_GpuBufferData
@end


@interface METAL_GpuTextureData : NSObject
    @property (nonatomic, retain) id<MTLTexture> mtltexture;
@end

@implementation METAL_GpuTextureData
@end

@interface METAL_GpuShaderData : NSObject
    @property (nonatomic, retain) id<MTLFunction> mtlfunction;
@end

@implementation METAL_GpuShaderData
@end

@interface METAL_GpuPipelineData : NSObject
    @property (nonatomic, retain) id<MTLRenderPipelineState> mtlpipeline;
    @property (nonatomic, retain) id<MTLDepthStencilState> mtldepthstencil;
    // these are part of the SDL GPU pipeline but not MTLRenderPipelineState, so we
    // keep a copy and set them when setting a new pipeline state.
    @property (nonatomic, assign) MTLPrimitiveType mtlprimitive;
    @property (nonatomic, assign) MTLTriangleFillMode mtlfillmode;
    @property (nonatomic, assign) MTLWinding mtlfrontface;
    @property (nonatomic, assign) MTLCullMode mtlcullface;
    @property (nonatomic, assign) float depth_bias;
    @property (nonatomic, assign) float depth_bias_scale;
    @property (nonatomic, assign) float depth_bias_clamp;
    @property (nonatomic, assign) Uint32 front_stencil_reference;
    @property (nonatomic, assign) Uint32 back_stencil_reference;
@end

@implementation METAL_GpuPipelineData
@end

@interface METAL_GpuSamplerData : NSObject
    @property (nonatomic, retain) id<MTLSamplerState> mtlsampler;
@end

@implementation METAL_GpuSamplerData
@end

@interface METAL_GpuCommandBufferData : NSObject
    @property (nonatomic, retain) id<MTLCommandBuffer> mtlcmdbuf;
@end

@implementation METAL_GpuCommandBufferData
@end

@interface METAL_GpuRenderPassData : NSObject
    @property (nonatomic, retain) id<MTLRenderCommandEncoder> mtlpass;
    // current state of things, so we don't re-set a currently set state.
    @property (nonatomic, retain) id<MTLRenderPipelineState> mtlpipeline;
    @property (nonatomic, retain) id<MTLDepthStencilState> mtldepthstencil;
    @property (nonatomic, assign) MTLViewport viewport;
    @property (nonatomic, assign) MTLScissorRect scissor;
    @property (nonatomic, assign) MTLPrimitiveType mtlprimitive;
    @property (nonatomic, assign) MTLTriangleFillMode mtlfillmode;
    @property (nonatomic, assign) MTLWinding mtlfrontface;
    @property (nonatomic, assign) MTLCullMode mtlcullface;
    @property (nonatomic, assign) float depth_bias;
    @property (nonatomic, assign) float depth_bias_scale;
    @property (nonatomic, assign) float depth_bias_clamp;
    @property (nonatomic, assign) Uint32 front_stencil_reference;
    @property (nonatomic, assign) Uint32 back_stencil_reference;
    @property (nonatomic, assign) float blend_constant_red;
    @property (nonatomic, assign) float blend_constant_green;
    @property (nonatomic, assign) float blend_constant_blue;
    @property (nonatomic, assign) float blend_constant_alpha;
@end

@implementation METAL_GpuRenderPassData
@end

@interface METAL_GpuBlitPassData : NSObject
    @property (nonatomic, retain) id<MTLBlitCommandEncoder> mtlpass;
@end

@implementation METAL_GpuBlitPassData
@end

// everything else is wrapped in an Objective-C object to let ARC
// handle memory management and object lifetimes, but this is all
// SDL objects that need to be manually destroyed and an atomic
// that doesn't play well with @properties, so I just went with
// a struct I malloc myself for this one.
typedef struct METAL_GpuFenceData
{
    SDL_atomic_t flag;
    SDL_mutex *mutex;
    SDL_cond *condition;
} METAL_GpuFenceData;

#define METAL_PIXFMT_MAPPINGS \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_B5G6R5, MTLPixelFormatB5G6R5Unorm) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_BGR5A1, MTLPixelFormatBGR5A1Unorm) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_RGBA8, MTLPixelFormatRGBA8Unorm) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_RGBA8_sRGB, MTLPixelFormatRGBA8Unorm_sRGB) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_BGRA8, MTLPixelFormatBGRA8Unorm) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_BGRA8_sRGB, MTLPixelFormatBGRA8Unorm_sRGB) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_Depth24_Stencil8, MTLPixelFormatDepth24Unorm_Stencil8) \
    METAL_MAPPIXFMT(SDL_GPUPIXELFMT_INVALID, MTLPixelFormatInvalid)

static MTLPixelFormat
PixelFormatToMetal(const SDL_GpuPixelFormat fmt)
{
    switch (fmt) {
        #define METAL_MAPPIXFMT(sdlfmt, mtlfmt) case sdlfmt: return mtlfmt;
        METAL_PIXFMT_MAPPINGS
        #undef METAL_MAPPIXFMT
    }

    SDL_SetError("Unsupported pixel format");
    return MTLPixelFormatInvalid;
}

static SDL_GpuPixelFormat
PixelFormatFromMetal(const MTLPixelFormat fmt)
{
    switch (fmt) {
        #define METAL_MAPPIXFMT(sdlfmt, mtlfmt) case mtlfmt: return sdlfmt;
        METAL_PIXFMT_MAPPINGS
        default: break;
        #undef METAL_MAPPIXFMT
    }

    SDL_SetError("Unsupported pixel format");
    return SDL_GPUPIXELFMT_INVALID;
}

static MTLVertexFormat
VertFormatToMetal(const SDL_GpuVertexFormat fmt)
{
    switch (fmt) {
        case SDL_GPUVERTFMT_INVALID: return MTLVertexFormatInvalid;
        case SDL_GPUVERTFMT_UCHAR2: return MTLVertexFormatUChar2;
        case SDL_GPUVERTFMT_UCHAR4: return MTLVertexFormatUChar4;
        case SDL_GPUVERTFMT_CHAR2: return MTLVertexFormatChar2;
        case SDL_GPUVERTFMT_CHAR4: return MTLVertexFormatChar4;
        case SDL_GPUVERTFMT_UCHAR2_NORMALIZED: return MTLVertexFormatUChar2Normalized;
        case SDL_GPUVERTFMT_UCHAR4_NORMALIZED: return MTLVertexFormatUChar4Normalized;
        case SDL_GPUVERTFMT_CHAR2_NORMALIZED: return MTLVertexFormatChar2Normalized;
        case SDL_GPUVERTFMT_CHAR4_NORMALIZED: return MTLVertexFormatChar4Normalized;
        case SDL_GPUVERTFMT_USHORT: return MTLVertexFormatUShort;
        case SDL_GPUVERTFMT_USHORT2: return MTLVertexFormatUShort2;
        case SDL_GPUVERTFMT_USHORT4: return MTLVertexFormatUShort4;
        case SDL_GPUVERTFMT_SHORT: return MTLVertexFormatShort;
        case SDL_GPUVERTFMT_SHORT2: return MTLVertexFormatShort2;
        case SDL_GPUVERTFMT_SHORT4: return MTLVertexFormatShort4;
        case SDL_GPUVERTFMT_USHORT_NORMALIZED: return MTLVertexFormatUShortNormalized;
        case SDL_GPUVERTFMT_USHORT2_NORMALIZED: return MTLVertexFormatUShort2Normalized;
        case SDL_GPUVERTFMT_USHORT4_NORMALIZED: return MTLVertexFormatUShort3Normalized;
        case SDL_GPUVERTFMT_SHORT_NORMALIZED: return MTLVertexFormatShortNormalized;
        case SDL_GPUVERTFMT_SHORT2_NORMALIZED: return MTLVertexFormatShort2Normalized;
        case SDL_GPUVERTFMT_SHORT4_NORMALIZED: return MTLVertexFormatShort4Normalized;
        case SDL_GPUVERTFMT_HALF: return MTLVertexFormatHalf;
        case SDL_GPUVERTFMT_HALF2: return MTLVertexFormatHalf2;
        case SDL_GPUVERTFMT_HALF4: return MTLVertexFormatHalf4;
        case SDL_GPUVERTFMT_FLOAT: return MTLVertexFormatFloat;
        case SDL_GPUVERTFMT_FLOAT2: return MTLVertexFormatFloat2;
        case SDL_GPUVERTFMT_FLOAT3: return MTLVertexFormatFloat3;
        case SDL_GPUVERTFMT_FLOAT4: return MTLVertexFormatFloat4;
        case SDL_GPUVERTFMT_UINT: return MTLVertexFormatUInt;
        case SDL_GPUVERTFMT_UINT2: return MTLVertexFormatUInt2;
        case SDL_GPUVERTFMT_UINT3: return MTLVertexFormatUInt3;
        case SDL_GPUVERTFMT_UINT4: return MTLVertexFormatUInt4;
        case SDL_GPUVERTFMT_INT: return MTLVertexFormatInt;
        case SDL_GPUVERTFMT_INT2: return MTLVertexFormatInt2;
        case SDL_GPUVERTFMT_INT3: return MTLVertexFormatInt3;
        case SDL_GPUVERTFMT_INT4: return MTLVertexFormatInt4;
    }

    SDL_assert(!"Unexpected vertex format");
    return MTLVertexFormatInvalid;
}

static MTLBlendOperation
BlendOpToMetal(const SDL_GpuBlendOperation op)
{
    switch (op) {
        case SDL_GPUBLENDOP_ADD: return MTLBlendOperationAdd;
        case SDL_GPUBLENDOP_SUBTRACT: return MTLBlendOperationSubtract;
        case SDL_GPUBLENDOP_REVERSESUBTRACT: return MTLBlendOperationReverseSubtract;
        case SDL_GPUBLENDOP_MIN: return MTLBlendOperationMin;
        case SDL_GPUBLENDOP_MAX: return MTLBlendOperationMax;
    }

    SDL_assert(!"Unexpected blend operation");
    return MTLBlendOperationAdd;
}

static MTLBlendFactor
BlendFactorToMetal(const SDL_GpuBlendFactor factor)
{
    switch (factor) {
        case SDL_GPUBLENDFACTOR_ZERO: return MTLBlendFactorZero;
        case SDL_GPUBLENDFACTOR_ONE: return MTLBlendFactorOne;
        case SDL_GPUBLENDFACTOR_SOURCECOLOR: return MTLBlendFactorSourceColor;
        case SDL_GPUBLENDFACTOR_ONEMINUSSOURCECOLOR: return MTLBlendFactorOneMinusSourceColor;
        case SDL_GPUBLENDFACTOR_SOURCEALPHA: return MTLBlendFactorSourceAlpha;
        case SDL_GPUBLENDFACTOR_ONEMINUSSOURCEALPHA: return MTLBlendFactorOneMinusSourceAlpha;
        case SDL_GPUBLENDFACTOR_DESTINATIONCOLOR: return MTLBlendFactorDestinationColor;
        case SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONCOLOR: return MTLBlendFactorOneMinusDestinationColor;
        case SDL_GPUBLENDFACTOR_DESTINATIONALPHA: return MTLBlendFactorDestinationAlpha;
        case SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
        case SDL_GPUBLENDFACTOR_SOURCEALPHASATURATED: return MTLBlendFactorSourceAlphaSaturated;
        case SDL_GPUBLENDFACTOR_BLENDCOLOR: return MTLBlendFactorBlendColor;
        case SDL_GPUBLENDFACTOR_ONEMINUSBLENDCOLOR: return MTLBlendFactorOneMinusBlendColor;
        case SDL_GPUBLENDFACTOR_BLENDALPHA: return MTLBlendFactorBlendAlpha;
        case SDL_GPUBLENDFACTOR_ONEMINUSBLENDALPHA: return MTLBlendFactorOneMinusBlendAlpha;
        case SDL_GPUBLENDFACTOR_SOURCE1COLOR: return MTLBlendFactorSource1Color;
        case SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1COLOR: return MTLBlendFactorOneMinusSource1Color;
        case SDL_GPUBLENDFACTOR_SOURCE1ALPHA: return MTLBlendFactorSource1Alpha;
        case SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1ALPHA: return MTLBlendFactorOneMinusSource1Alpha;
    }

    SDL_assert(!"Unexpected blend factor");
    return MTLBlendFactorZero;
}

static MTLPrimitiveTopologyClass
PrimitiveTopologyToMetal(const SDL_GpuPrimitive prim)
{
    switch (prim) {
        case SDL_GPUPRIM_POINT: return MTLPrimitiveTopologyClassPoint;
        case SDL_GPUPRIM_LINE: return MTLPrimitiveTopologyClassLine;
        case SDL_GPUPRIM_LINESTRIP: return MTLPrimitiveTopologyClassLine;
        case SDL_GPUPRIM_TRIANGLE: return MTLPrimitiveTopologyClassTriangle;
        case SDL_GPUPRIM_TRIANGLESTRIP: return MTLPrimitiveTopologyClassTriangle;
    }

    SDL_assert(!"Unexpected primitive topology");
    return MTLPrimitiveTopologyClassUnspecified;
}

static MTLCompareFunction
CompareFunctionToMetal(const SDL_GpuCompareFunction fn)
{
    switch (fn) {
        case SDL_GPUCMPFUNC_NEVER: return MTLCompareFunctionNever;
        case SDL_GPUCMPFUNC_LESS: return MTLCompareFunctionLess;
        case SDL_GPUCMPFUNC_EQUAL: return MTLCompareFunctionEqual;
        case SDL_GPUCMPFUNC_LESSEQUAL: return MTLCompareFunctionLessEqual;
        case SDL_GPUCMPFUNC_GREATER: return MTLCompareFunctionGreater;
        case SDL_GPUCMPFUNC_NOTEQUAL: return MTLCompareFunctionNotEqual;
        case SDL_GPUCMPFUNC_GREATEREQUAL: return MTLCompareFunctionGreaterEqual;
        case SDL_GPUCMPFUNC_ALWAYS: return MTLCompareFunctionAlways;
    }

    SDL_assert(!"Unexpected compare function");
    return MTLCompareFunctionNever;
}

static MTLStencilOperation
StencilOpToMetal(const SDL_GpuStencilOperation op)
{
    switch (op) {
        case SDL_GPUSTENCILOP_KEEP: return MTLStencilOperationKeep;
        case SDL_GPUSTENCILOP_ZERO: return MTLStencilOperationZero;
        case SDL_GPUSTENCILOP_REPLACE: return MTLStencilOperationReplace;
        case SDL_GPUSTENCILOP_INCREMENTCLAMP: return MTLStencilOperationIncrementClamp;
        case SDL_GPUSTENCILOP_DECREMENTCLAMP: return MTLStencilOperationDecrementClamp;
        case SDL_GPUSTENCILOP_INVERT: return MTLStencilOperationInvert;
        case SDL_GPUSTENCILOP_INCREMENTWRAP: return MTLStencilOperationIncrementWrap;
        case SDL_GPUSTENCILOP_DECREMENTWRAP: return MTLStencilOperationDecrementWrap;
    }

    SDL_assert(!"Unexpected stencil operation");
    return MTLStencilOperationKeep;
}

static MTLPrimitiveType
PrimitiveToMetal(const SDL_GpuPrimitive prim)
{
    switch (prim) {
        case SDL_GPUPRIM_POINT: return MTLPrimitiveTypePoint;
        case SDL_GPUPRIM_LINE: return MTLPrimitiveTypeLine;
        case SDL_GPUPRIM_LINESTRIP: return MTLPrimitiveTypeLineStrip;
        case SDL_GPUPRIM_TRIANGLE: return MTLPrimitiveTypeTriangle;
        case SDL_GPUPRIM_TRIANGLESTRIP: return MTLPrimitiveTypeTriangleStrip;
    }

    SDL_assert(!"Unexpected primitive type");
    return MTLPrimitiveTypeTriangleStrip;
}

static MTLTriangleFillMode
FillModeToMetal(const SDL_GpuFillMode fill)
{
    switch (fill) {
        case SDL_GPUFILL_FILL: return MTLTriangleFillModeFill;
        case SDL_GPUFILL_LINE: return MTLTriangleFillModeLines;
    }

    SDL_assert(!"Unexpected fill mode");
    return MTLTriangleFillModeFill;
}

static MTLWinding
FrontFaceToMetal(const SDL_GpuFrontFace winding)
{
    switch (winding) {
        case SDL_GPUFRONTFACE_COUNTER_CLOCKWISE: return MTLWindingCounterClockwise;
        case SDL_GPUFRONTFACE_CLOCKWISE: return MTLWindingClockwise;
    }

    SDL_assert(!"Unexpected winding mode");
    return MTLWindingCounterClockwise;
}

static MTLCullMode
CullFaceToMetal(const SDL_GpuCullFace face)
{
    switch (face) {
        case SDL_GPUCULLFACE_BACK: return MTLCullModeBack;
        case SDL_GPUCULLFACE_FRONT: return MTLCullModeFront;
        case SDL_GPUCULLFACE_NONE: return MTLCullModeNone;
    }

    SDL_assert(!"Unexpected cull mode");
    return MTLCullModeBack;
}


static MTLSamplerAddressMode
SamplerAddressToMetal(const SDL_GpuSamplerAddressMode addrmode)
{
    switch (addrmode) {
        case SDL_GPUSAMPADDR_CLAMPTOEDGE: return MTLSamplerAddressModeClampToEdge;
        case SDL_GPUSAMPADDR_MIRRORCLAMPTOEDGE: return MTLSamplerAddressModeMirrorClampToEdge;
        case SDL_GPUSAMPADDR_REPEAT: return MTLSamplerAddressModeRepeat;
        case SDL_GPUSAMPADDR_MIRRORREPEAT: return MTLSamplerAddressModeMirrorRepeat;
        case SDL_GPUSAMPADDR_CLAMPTOZERO: return MTLSamplerAddressModeClampToZero;
        case SDL_GPUSAMPADDR_CLAMPTOBORDERCOLOR: return MTLSamplerAddressModeClampToBorderColor;
    }

    SDL_assert(!"Unexpected sampler address mode");
    return MTLSamplerAddressModeClampToEdge;
}

static MTLSamplerBorderColor
SamplerBorderColorToMetal(const SDL_GpuSamplerBorderColor color)
{
    switch (color) {
        case SDL_GPUSAMPBORDER_TRANSPARENT_BLACK: return MTLSamplerBorderColorTransparentBlack;
        case SDL_GPUSAMPBORDER_OPAQUE_BLACK: return MTLSamplerBorderColorOpaqueBlack;
        case SDL_GPUSAMPBORDER_OPAQUE_WHITE: return MTLSamplerBorderColorOpaqueWhite;
    }

    SDL_assert(!"Unexpected sampler border color");
    return MTLSamplerBorderColorTransparentBlack;
}

static MTLSamplerMinMagFilter
SamplerMinMagFilterToMetal(const SDL_GpuSamplerMinMagFilter filt)
{
    switch (filt) {
        case SDL_GPUMINMAGFILTER_NEAREST: return MTLSamplerMinMagFilterNearest;
        case SDL_GPUMINMAGFILTER_LINEAR: return MTLSamplerMinMagFilterLinear;
    }

    SDL_assert(!"Unexpected sampler minmag filter");
    return MTLSamplerMinMagFilterNearest;
}

static MTLSamplerMipFilter
SamplerMipFilterToMetal(const SDL_GpuSamplerMipFilter filt)
{
    switch (filt) {
        case SDL_GPUMIPFILTER_NOTMIPMAPPED: return MTLSamplerMipFilterNotMipmapped;
        case SDL_GPUMIPFILTER_NEAREST: return MTLSamplerMipFilterNearest;
        case SDL_GPUMIPFILTER_LINEAR: return MTLSamplerMipFilterLinear;
    }

    SDL_assert(!"Unexpected sampler mip filter");
    return MTLSamplerMipFilterNotMipmapped;
}

static MTLLoadAction
LoadActionToMetal(const SDL_GpuPassInit action)
{
    switch (action) {
        case SDL_GPUPASSINIT_UNDEFINED: return MTLLoadActionDontCare;
        case SDL_GPUPASSINIT_LOAD: return MTLLoadActionLoad;
        case SDL_GPUPASSINIT_CLEAR: return MTLLoadActionClear;
    }

    SDL_assert(!"Unexpected load action");
    return MTLLoadActionDontCare;
}

static MTLIndexType
IndexTypeToMetal(const SDL_GpuIndexType typ)
{
    switch (typ) {
        case SDL_GPUINDEXTYPE_UINT16: return MTLIndexTypeUInt16;
        case SDL_GPUINDEXTYPE_UINT32: return MTLIndexTypeUInt32;
    }

    SDL_assert(!"Unexpected index type");
    return MTLIndexTypeUInt16;
}


static SDL_MetalView
GetWindowView(SDL_Window *window)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) {
#ifdef __MACOSX__
        if (info.subsystem == SDL_SYSWM_COCOA) {
            NSView *view = info.info.cocoa.window.contentView;
            if (view.subviews.count > 0) {
                view = view.subviews[0];
                if (view.tag == SDL_METALVIEW_TAG) {
                    return (SDL_MetalView) CFBridgingRetain(view);
                }
            }
        }
#else
        if (info.subsystem == SDL_SYSWM_UIKIT) {
            UIView *view = info.info.uikit.window.rootViewController.view;
            if (view.tag == SDL_METALVIEW_TAG) {
                return (SDL_MetalView) CFBridgingRetain(view);
            }
        }
#endif
    }
    return nil;
}


static int
METAL_GpuClaimWindow(SDL_GpuDevice *device, SDL_Window *window)
{
    const Uint32 window_flags = SDL_GetWindowFlags(window);
    SDL_bool changed_window = SDL_FALSE;
    METAL_GpuWindowData *windata;
    CAMetalLayer *layer = nil;
    SDL_MetalView view;

    windata = [[METAL_GpuWindowData alloc] init];
    if (windata == nil) {
        return SDL_OutOfMemory();
    }

    windata.mtldrawable = nil;

    if (!(window_flags & SDL_WINDOW_METAL)) {
        changed_window = SDL_TRUE;
        if (SDL_RecreateWindow(window, (window_flags & ~(SDL_WINDOW_VULKAN | SDL_WINDOW_OPENGL)) | SDL_WINDOW_METAL) < 0) {
            return -1;
        }
    }

    view = GetWindowView(window);
    if (view == nil) {
        view = SDL_Metal_CreateView(window);
        if (view == nil) {
            if (changed_window) {
                SDL_RecreateWindow(window, window_flags);
            }
            return -1;
        }
    }

    windata.mtlview = view;

// !!! FIXME: does this need bridging, or can we just assign and let ARC handle it?
#ifdef __MACOSX__
    layer = (CAMetalLayer *)[(__bridge NSView *)windata.mtlview layer];
#else
    layer = (CAMetalLayer *)[(__bridge UIView *)windata.mtlview layer];
#endif

    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) device->driverdata;
    layer.device = devdata.mtldevice;
    layer.framebufferOnly = NO;
    windata.mtllayer = layer;

    window->gpu_driverdata = (void *) CFBridgingRetain(windata);

    return 0;
}

static int
METAL_GpuCreateCpuBuffer(SDL_GpuCpuBuffer *buffer, const void *data)
{
    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) buffer->device->driverdata;
    METAL_GpuBufferData *bufferdata;

    bufferdata = [[METAL_GpuBufferData alloc] init];
    if (bufferdata == nil) {
        return SDL_OutOfMemory();
    }

    if (data != NULL) {
        bufferdata.mtlbuffer = [devdata.mtldevice newBufferWithBytes:data length:buffer->buflen options:MTLResourceStorageModeShared];
    } else {
        bufferdata.mtlbuffer = [devdata.mtldevice newBufferWithLength:buffer->buflen options:MTLResourceStorageModeShared];
    }

    if (bufferdata.mtlbuffer == nil) {
        SDL_SetError("Failed to create Metal buffer!");
    }

    if (buffer->label) {
        bufferdata.mtlbuffer.label = [NSString stringWithUTF8String:buffer->label];
    }

    buffer->driverdata = (void *) CFBridgingRetain(bufferdata);

    return 0;
}

static void
METAL_GpuDestroyCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    CFBridgingRelease(buffer->driverdata);
}

static void *
METAL_GpuLockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    METAL_GpuBufferData *bufdata = (__bridge METAL_GpuBufferData *) buffer->driverdata;
    void *retval = [bufdata.mtlbuffer contents];
    SDL_assert(retval != NULL);  // should only return NULL for private (GPU-only) buffers.
    return retval;
}

static int
METAL_GpuUnlockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    return 0;
}

static int
METAL_GpuCreateBuffer(SDL_GpuBuffer *buffer)
{
    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) buffer->device->driverdata;
    METAL_GpuBufferData *bufferdata;

    bufferdata = [[METAL_GpuBufferData alloc] init];
    if (bufferdata == nil) {
        return SDL_OutOfMemory();
    }

    bufferdata.mtlbuffer = [devdata.mtldevice newBufferWithLength:buffer->buflen options:MTLResourceStorageModePrivate];
    if (bufferdata.mtlbuffer == nil) {
        SDL_SetError("Failed to create Metal buffer!");
    }

    if (buffer->label) {
        bufferdata.mtlbuffer.label = [NSString stringWithUTF8String:buffer->label];
    }

    buffer->driverdata = (void *) CFBridgingRetain(bufferdata);

    return 0;
}

static void
METAL_GpuDestroyBuffer(SDL_GpuBuffer *buffer)
{
    CFBridgingRelease(buffer->driverdata);
}

static int
METAL_GpuCreateTexture(SDL_GpuTexture *texture)
{
    const SDL_GpuTextureDescription *desc = &texture->desc;

    const MTLPixelFormat mtlfmt = PixelFormatToMetal(desc->pixel_format);
    if (mtlfmt == MTLPixelFormatInvalid) {
        return -1;
    }

    MTLTextureType mtltextype;
    SDL_bool is_cube = SDL_FALSE;
    SDL_bool is_array = SDL_FALSE;
    SDL_bool is_3d = SDL_FALSE;
    switch (desc->texture_type) {
        case SDL_GPUTEXTYPE_1D: mtltextype = MTLTextureType1D; break;
        case SDL_GPUTEXTYPE_2D: mtltextype = MTLTextureType2D; break;
        case SDL_GPUTEXTYPE_CUBE: mtltextype = MTLTextureTypeCube; is_cube = SDL_TRUE; break;
        case SDL_GPUTEXTYPE_3D: mtltextype = MTLTextureType3D; is_3d = SDL_TRUE; break;
        case SDL_GPUTEXTYPE_1D_ARRAY: mtltextype = MTLTextureType1DArray; is_array = SDL_TRUE; break;
        case SDL_GPUTEXTYPE_2D_ARRAY: mtltextype = MTLTextureType2DArray; is_array = SDL_TRUE; break;
        case SDL_GPUTEXTYPE_CUBE_ARRAY: mtltextype = MTLTextureTypeCubeArray; is_cube = SDL_TRUE; is_array = SDL_TRUE; break;
        default: return SDL_SetError("Unsupported texture type");
    };

    (void) is_array;  // (we don't actually use this at the moment, silence compiler warning.)

    MTLTextureUsage mtltexusage = (MTLTextureUsage) 0;
    if (desc->usage & SDL_GPUTEXUSAGE_SHADER_READ) {
        mtltexusage |= MTLTextureUsageShaderRead;
    }
    if (desc->usage & SDL_GPUTEXUSAGE_SHADER_WRITE) {
        mtltexusage |= MTLTextureUsageShaderWrite;
    }
    if (desc->usage & SDL_GPUTEXUSAGE_RENDER_TARGET) {
        mtltexusage |= MTLTextureUsageRenderTarget;
    }

    METAL_GpuTextureData *texturedata = [[METAL_GpuTextureData alloc] init];
    if (texturedata == nil) {
        return SDL_OutOfMemory();
    }

    texturedata.mtltexture = nil;

    // !!! FIXME: does ARC know what to do with these, since it doesn't start with "alloc" or "new"?
    MTLTextureDescriptor *mtltexdesc;
    if (is_cube) {
        mtltexdesc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:mtlfmt size:desc->width mipmapped:NO];
    } else {
        mtltexdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlfmt width:desc->width height:desc->height mipmapped:NO];
    }

    if (mtltexdesc == nil) {
        return SDL_OutOfMemory();
    }

    mtltexdesc.textureType = mtltextype;
    mtltexdesc.pixelFormat = mtlfmt;
    mtltexdesc.width = desc->width;
    mtltexdesc.height = is_cube ? desc->width : desc->height;
    mtltexdesc.depth = (is_3d) ? desc->depth_or_slices : 1;
    mtltexdesc.mipmapLevelCount = desc->mipmap_levels;
    mtltexdesc.sampleCount = 1;  // !!! FIXME: multisample support
    mtltexdesc.arrayLength = is_3d ? 1 : desc->depth_or_slices;
    if (is_cube) {
        mtltexdesc.arrayLength /= 6;
    }
    mtltexdesc.resourceOptions = MTLResourceStorageModePrivate;

    // not available in iOS 8.
    if ([mtltexdesc respondsToSelector:@selector(usage)]) {
        mtltexdesc.usage = mtltexusage;
    }

    // these arrived in later releases, but we want the defaults anyhow.
    //mtltexdesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
    //mtltexdesc.hazardTrackingMode = MTLHazardTrackingModeDefault;
    //mtltexdesc.allowGPUOptimizedContents = YES;
    //mtltexdesc.swizzle = blahblahblah;

    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) texture->device->driverdata;
    texturedata.mtltexture = [devdata.mtldevice newTextureWithDescriptor:mtltexdesc];
    if (texturedata.mtltexture == nil) {
        SDL_SetError("Failed to create Metal texture!");
    }

    texturedata.mtltexture.label = [NSString stringWithUTF8String:desc->label];

    texture->driverdata = (void *) CFBridgingRetain(texturedata);

    return 0;
}

static void
METAL_GpuDestroyTexture(SDL_GpuTexture *texture)
{
    CFBridgingRelease(texture->driverdata);
}


// !!! FIXME
static int METAL_GpuCreateShader(SDL_GpuShader *shader, const Uint8 *bytecode, const Uint32 bytecodelen) { return 0; }
static void METAL_GpuDestroyShader(SDL_GpuShader *shader) {}



static int METAL_GpuCreatePipeline(SDL_GpuPipeline *pipeline)
{
    const SDL_GpuPipelineDescription *desc = &pipeline->desc;

    // !!! FIXME: I assume this has to be something depthy, and not RGBy.
    const MTLPixelFormat mtldepthfmt = PixelFormatToMetal(desc->depth_format);
    if ((mtldepthfmt == MTLPixelFormatInvalid) && (desc->depth_format != SDL_GPUPIXELFMT_INVALID)) {
        return SDL_SetError("Invalid depth pixel format");
    }

    // !!! FIXME: I assume this has to be something stencilly, and not RGBy.
    const MTLPixelFormat mtlstencilfmt = PixelFormatToMetal(desc->stencil_format);
    if ((mtlstencilfmt == MTLPixelFormatInvalid) && (desc->stencil_format != SDL_GPUPIXELFMT_INVALID)) {
        return SDL_SetError("Invalid stencil pixel format");
    }

    METAL_GpuPipelineData *pipelinedata = [[METAL_GpuPipelineData alloc] init];
    if (pipelinedata == nil) {
        return SDL_OutOfMemory();
    }

    MTLRenderPipelineDescriptor *mtlpipedesc = [[MTLRenderPipelineDescriptor alloc] init];
    if (mtlpipedesc == nil) {
        return SDL_OutOfMemory();
    }

    MTLVertexDescriptor *mtlvertdesc = [MTLVertexDescriptor vertexDescriptor];
    for (Uint32 i = 0; i < desc->num_vertex_attributes; i++) {
        mtlvertdesc.attributes[i].format = VertFormatToMetal(desc->vertices[i].format);
        mtlvertdesc.attributes[i].offset = desc->vertices[i].offset;
        mtlvertdesc.attributes[i].bufferIndex = desc->vertices[i].index;
        mtlvertdesc.layouts[i].stepFunction = MTLVertexStepFunctionPerVertex;  // !!! FIXME
        mtlvertdesc.layouts[i].stepRate = 1;  // !!! FIXME
        mtlvertdesc.layouts[i].stride = desc->vertices[i].stride;
    }

    for (Uint32 i = 0; i < desc->num_color_attachments; i++) {
        const SDL_GpuPipelineColorAttachmentDescription *sdldesc = &desc->color_attachments[i];
        const MTLPixelFormat mtlfmt = PixelFormatToMetal(sdldesc->pixel_format);
        if (mtlfmt == MTLPixelFormatInvalid) {
            return SDL_SetError("Invalid pixel format in color attachment #%u", (unsigned int) i);
        }

        MTLColorWriteMask writemask = MTLColorWriteMaskNone;
        if (sdldesc->writemask_enabled_red) { writemask |= MTLColorWriteMaskRed; }
        if (sdldesc->writemask_enabled_blue) { writemask |= MTLColorWriteMaskBlue; }
        if (sdldesc->writemask_enabled_green) { writemask |= MTLColorWriteMaskGreen; }
        if (sdldesc->writemask_enabled_alpha) { writemask |= MTLColorWriteMaskAlpha; }

        MTLRenderPipelineColorAttachmentDescriptor *metaldesc = mtlpipedesc.colorAttachments[i];
        metaldesc.pixelFormat = mtlfmt;
        metaldesc.writeMask = writemask;
        metaldesc.blendingEnabled = sdldesc->blending_enabled ? YES : NO;
        metaldesc.alphaBlendOperation = BlendOpToMetal(sdldesc->alpha_blend_op);
        metaldesc.sourceAlphaBlendFactor = BlendFactorToMetal(sdldesc->alpha_src_blend_factor);
        metaldesc.destinationAlphaBlendFactor = BlendFactorToMetal(sdldesc->alpha_dst_blend_factor);
        metaldesc.rgbBlendOperation = BlendOpToMetal(sdldesc->rgb_blend_op);
        metaldesc.sourceRGBBlendFactor = BlendFactorToMetal(sdldesc->rgb_src_blend_factor);
        metaldesc.destinationRGBBlendFactor = BlendFactorToMetal(sdldesc->rgb_dst_blend_factor);
    }

    METAL_GpuShaderData *vshaderdata = (__bridge METAL_GpuShaderData *) desc->vertex_shader->driverdata;
    METAL_GpuShaderData *fshaderdata = (__bridge METAL_GpuShaderData *) desc->fragment_shader->driverdata;

    mtlpipedesc.label = [NSString stringWithUTF8String:desc->label];
    mtlpipedesc.vertexFunction = vshaderdata.mtlfunction;
    mtlpipedesc.fragmentFunction = fshaderdata.mtlfunction;
    mtlpipedesc.vertexDescriptor = mtlvertdesc;
    mtlpipedesc.depthAttachmentPixelFormat = mtldepthfmt;
    mtlpipedesc.stencilAttachmentPixelFormat = mtlstencilfmt;
    mtlpipedesc.sampleCount = 1;  // !!! FIXME: multisampling
    mtlpipedesc.alphaToCoverageEnabled = NO;
    mtlpipedesc.alphaToOneEnabled = NO;
    mtlpipedesc.rasterizationEnabled = YES;
    mtlpipedesc.rasterSampleCount = 1;  // !!! FIXME: multisampling  (also, how is this different from sampleCount?)

    // Not available before iOS 12.
    if ([mtlpipedesc respondsToSelector:@selector(inputPrimitiveTopology)]) {
        mtlpipedesc.inputPrimitiveTopology = PrimitiveTopologyToMetal(desc->primitive);
    }

    // these arrived in later releases, but we _probably_ want the defaults anyhow (and/or we don't support it).
    //mtlpipedesc.maxVertexCallStackDepth = 1;
    //mtlpipedesc.maxFragmentCallStackDepth = 1;
    //mtlpipedesc.vertexBuffers
    //mtlpipedesc.fragmentBuffers
    //mtlpipedesc.maxTessellationFactor
    //mtlpipedesc.tessellationFactorScaleEnabled
    //mtlpipedesc.tessellationFactorFormat
    //mtlpipedesc.tessellationControlPointIndexType
    //mtlpipedesc.tessellationFactorStepFunction
    //mtlpipedesc.tessellationOutputWindingOrder
    //mtlpipedesc.tessellationPartitionMode
    //mtlpipedesc.supportIndirectCommandBuffers
    //mtlpipedesc.maxVertexAmplificationCount
    //mtlpipedesc.supportAddingVertexBinaryFunctions
    //mtlpipedesc.supportAddingFragmentBinaryFunctions
    //mtlpipedesc.binaryArchives
    //mtlpipedesc.vertexLinkedFunctions
    //mtlpipedesc.fragmentLinkedFunctions
    //mtlpipedesc.fragmentPreloadedLibraries
    //mtlpipedesc.vertexPreloadedLibraries

    // !!! FIXME: Hash existing pipelines and reuse them, with reference counting, for when the only things
    // !!! FIXME: that are different are states that exist in SDL_GpuPipeline but not MTLRenderPipelineState.
    // !!! FIXME: Likewise for depth stencil objects.

    // Metal wants to create separate, long-living state objects for
    // depth stencil stuff, so build one to go with pipeline.
    // !!! FIXME: iOS 8 doesn't have -(void)[MTLRenderCommandEncoder setStencilFrontReferenceValue:backReferenceValue:],
    // !!! FIXME: only one that sets them to the same thing, so we should fail if
    // !!! FIXME: stencil_reference_front != stencil_reference_back on that target.
    MTLDepthStencilDescriptor *mtldepthstencildesc = [[MTLDepthStencilDescriptor alloc] init];
    mtldepthstencildesc.label = mtlpipedesc.label;
    mtldepthstencildesc.depthCompareFunction = CompareFunctionToMetal(desc->depth_function);
    mtldepthstencildesc.depthWriteEnabled = desc->depth_write_enabled ? YES : NO;
    mtldepthstencildesc.backFaceStencil = [[MTLStencilDescriptor alloc] init];
    mtldepthstencildesc.backFaceStencil.stencilFailureOperation = StencilOpToMetal(desc->depth_stencil_back.stencil_fail);
    mtldepthstencildesc.backFaceStencil.depthFailureOperation = StencilOpToMetal(desc->depth_stencil_back.depth_fail);
    mtldepthstencildesc.backFaceStencil.depthStencilPassOperation = StencilOpToMetal(desc->depth_stencil_back.depth_and_stencil_pass);
    mtldepthstencildesc.backFaceStencil.stencilCompareFunction = CompareFunctionToMetal(desc->depth_stencil_back.stencil_function);
    mtldepthstencildesc.backFaceStencil.readMask = desc->depth_stencil_back.stencil_read_mask;
    mtldepthstencildesc.backFaceStencil.writeMask = desc->depth_stencil_back.stencil_write_mask;
    mtldepthstencildesc.frontFaceStencil = [[MTLStencilDescriptor alloc] init];
    mtldepthstencildesc.frontFaceStencil.stencilFailureOperation = StencilOpToMetal(desc->depth_stencil_front.stencil_fail);
    mtldepthstencildesc.frontFaceStencil.depthFailureOperation = StencilOpToMetal(desc->depth_stencil_front.depth_fail);
    mtldepthstencildesc.frontFaceStencil.depthStencilPassOperation = StencilOpToMetal(desc->depth_stencil_front.depth_and_stencil_pass);
    mtldepthstencildesc.frontFaceStencil.stencilCompareFunction = CompareFunctionToMetal(desc->depth_stencil_front.stencil_function);
    mtldepthstencildesc.frontFaceStencil.readMask = desc->depth_stencil_front.stencil_read_mask;
    mtldepthstencildesc.frontFaceStencil.writeMask = desc->depth_stencil_front.stencil_write_mask;

    NSError *err = nil;
    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) pipeline->device->driverdata;
    pipelinedata.mtldepthstencil = [devdata.mtldevice newDepthStencilStateWithDescriptor:mtldepthstencildesc];
    pipelinedata.mtlpipeline = [devdata.mtldevice newRenderPipelineStateWithDescriptor:mtlpipedesc error:&err];
    pipelinedata.mtlprimitive = PrimitiveToMetal(desc->primitive);
    pipelinedata.mtlfillmode = FillModeToMetal(desc->fill_mode);
    pipelinedata.mtlfrontface = FrontFaceToMetal(desc->front_face);
    pipelinedata.mtlcullface = CullFaceToMetal(desc->cull_face);
    pipelinedata.depth_bias = desc->depth_bias;
    pipelinedata.depth_bias_scale = desc->depth_bias;
    pipelinedata.depth_bias_clamp = desc->depth_bias_clamp;
    pipelinedata.front_stencil_reference = desc->depth_stencil_front.stencil_reference;
    pipelinedata.back_stencil_reference = desc->depth_stencil_back.stencil_reference;

    SDL_assert(err == nil);  // !!! FIXME: for what reasons can this fail?

    if (pipelinedata.mtldepthstencil == nil) {
        return SDL_SetError("Failed to create Metal depth stencil!");
    } else if (pipelinedata.mtlpipeline == nil) {
        return SDL_SetError("Failed to create Metal pipeline!");
    }

    pipeline->driverdata = (void *) CFBridgingRetain(pipelinedata);

    return 0;
}

static void
METAL_GpuDestroyPipeline(SDL_GpuPipeline *pipeline)
{
    CFBridgingRelease(pipeline->driverdata);
}

static int
METAL_GpuCreateSampler(SDL_GpuSampler *sampler)
{
    const SDL_GpuSamplerDescription *desc = &sampler->desc;

    METAL_GpuSamplerData *samplerdata = [[METAL_GpuSamplerData alloc] init];
    if (samplerdata == nil) {
        return SDL_OutOfMemory();
    }

    MTLSamplerDescriptor *mtlsamplerdesc = [[MTLSamplerDescriptor alloc] init];
    if (mtlsamplerdesc == nil) {
        return SDL_OutOfMemory();
    }

    mtlsamplerdesc.label = [NSString stringWithUTF8String:desc->label];
    mtlsamplerdesc.normalizedCoordinates = YES;
    mtlsamplerdesc.rAddressMode = SamplerAddressToMetal(desc->addrmode_r);
    mtlsamplerdesc.sAddressMode = SamplerAddressToMetal(desc->addrmode_u);
    mtlsamplerdesc.tAddressMode = SamplerAddressToMetal(desc->addrmode_v);
    mtlsamplerdesc.borderColor = SamplerBorderColorToMetal(desc->border_color);
    mtlsamplerdesc.minFilter = SamplerMinMagFilterToMetal(desc->min_filter);
    mtlsamplerdesc.magFilter = SamplerMinMagFilterToMetal(desc->mag_filter);
    mtlsamplerdesc.mipFilter = SamplerMipFilterToMetal(desc->mip_filter);
    mtlsamplerdesc.maxAnisotropy = desc->max_anisotropy;

    // !!! FIXME: add these?
    //mtlsamplerdesc.lodMinClamp
    //mtlsamplerdesc.lodMaxClamp
    //mtlsamplerdesc.lodAverage
    //mtlsamplerdesc.compareFunction
    //mtlsamplerdesc.supportArgumentBuffers

    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) sampler->device->driverdata;
    samplerdata.mtlsampler = [devdata.mtldevice newSamplerStateWithDescriptor:mtlsamplerdesc];
    if (samplerdata.mtlsampler == nil) {
        return SDL_SetError("Failed to create Metal sampler!");
    }

    sampler->driverdata = (void *) CFBridgingRetain(samplerdata);

    return 0;
}

static void
METAL_GpuDestroySampler(SDL_GpuSampler *sampler)
{
    CFBridgingRelease(sampler->driverdata);
}

static int
METAL_GpuCreateCommandBuffer(SDL_GpuCommandBuffer *cmdbuf)
{
    METAL_GpuCommandBufferData *cmdbufdata = [[METAL_GpuCommandBufferData alloc] init];
    if (cmdbufdata == nil) {
        return SDL_OutOfMemory();
    }

    METAL_GpuDeviceData *devdata = (__bridge METAL_GpuDeviceData *) cmdbuf->device->driverdata;
    cmdbufdata.mtlcmdbuf = [devdata.mtlcmdqueue commandBuffer];
    if (cmdbufdata.mtlcmdbuf == nil) {
        return SDL_SetError("Failed to create Metal command buffer!");
    }

    cmdbufdata.mtlcmdbuf.label = [NSString stringWithUTF8String:cmdbuf->label];

    cmdbuf->driverdata = (void *) CFBridgingRetain(cmdbufdata);

    return 0;
}

static int
METAL_GpuStartRenderPass(SDL_GpuRenderPass *pass, Uint32 num_color_attachments, const SDL_GpuColorAttachmentDescription *color_attachments, const SDL_GpuDepthAttachmentDescription *depth_attachment, const SDL_GpuStencilAttachmentDescription *stencil_attachment)
{
    METAL_GpuRenderPassData *passdata = [[METAL_GpuRenderPassData alloc] init];
    if (passdata == nil) {
        return SDL_OutOfMemory();
    }

    MTLRenderPassDescriptor *mtlpassdesc = [MTLRenderPassDescriptor renderPassDescriptor];
    if (mtlpassdesc == nil) {
        return SDL_OutOfMemory();
    }

    for (Uint32 i = 0; i < num_color_attachments; i++) {
        const SDL_GpuColorAttachmentDescription *sdldesc = &color_attachments[i];
        MTLRenderPassColorAttachmentDescriptor *mtldesc = mtlpassdesc.colorAttachments[i];
        METAL_GpuTextureData *texturedata = (__bridge METAL_GpuTextureData *) sdldesc->texture->driverdata;
        mtldesc.texture = texturedata.mtltexture;
        mtldesc.loadAction = LoadActionToMetal(sdldesc->color_init);
        mtldesc.clearColor = MTLClearColorMake(sdldesc->clear_red, sdldesc->clear_green, sdldesc->clear_blue, sdldesc->clear_alpha);

        // !!! FIXME: not used (but maybe should be)...
        //mtldesc.level
        //mtldesc.slice
        //mtldesc.depthPlane
        //mtldesc.storeAction
        //mtldesc.storeActionOptions
        //mtldesc.resolveTexture
        //mtldesc.resolveLevel
        //mtldesc.resolveSlice
        //mtldesc.resolveDepthPlane
    }

    if (depth_attachment) {
        METAL_GpuTextureData *depthtexturedata = (__bridge METAL_GpuTextureData *) depth_attachment->texture->driverdata;
        mtlpassdesc.depthAttachment.texture = depthtexturedata.mtltexture;
        mtlpassdesc.depthAttachment.loadAction = LoadActionToMetal(depth_attachment->depth_init);
        mtlpassdesc.depthAttachment.clearDepth = depth_attachment->clear_depth;
    }

    if (stencil_attachment) {
        METAL_GpuTextureData *stenciltexturedata = (__bridge METAL_GpuTextureData *) stencil_attachment->texture->driverdata;
        mtlpassdesc.stencilAttachment.texture = stenciltexturedata.mtltexture;
        mtlpassdesc.stencilAttachment.loadAction = LoadActionToMetal(stencil_attachment->stencil_init);
        mtlpassdesc.stencilAttachment.clearStencil = stencil_attachment->clear_stencil;
    }

    METAL_GpuCommandBufferData *cmdbufdata = (__bridge METAL_GpuCommandBufferData *) pass->cmdbuf->driverdata;
    passdata.mtlpass = [cmdbufdata.mtlcmdbuf renderCommandEncoderWithDescriptor:mtlpassdesc];
    if (passdata.mtlpass == nil) {
        return SDL_SetError("Failed to create Metal render command encoder!");
    }

    const SDL_GpuTextureDescription *colatt0 = num_color_attachments ? &color_attachments[0].texture->desc : NULL;

    // set up defaults for things that are part of SDL_GpuPipeline, but not part of MTLRenderPipelineState
    passdata.mtlpipeline = nil;
    passdata.mtldepthstencil = nil;
    passdata.mtlprimitive = MTLPrimitiveTypeTriangleStrip;
    passdata.mtlfillmode = MTLTriangleFillModeFill;
    passdata.mtlfrontface = MTLWindingClockwise;
    passdata.mtlcullface = MTLCullModeNone;
    passdata.depth_bias = 0.0f;
    passdata.depth_bias_scale = 0.0f;
    passdata.depth_bias_clamp = 0.0f;
    passdata.front_stencil_reference = 0x00000000;
    passdata.back_stencil_reference = 0x00000000;
    const MTLViewport initialvp = { 0.0, 0.0, colatt0 ? (double) colatt0->width : 0.0, colatt0 ? (double) colatt0->height : 0.0, 0.0, 1.0 };
    passdata.viewport = initialvp;
    const MTLScissorRect initialscis = { 0.0, 0.0, colatt0 ? (double) colatt0->width : 0, colatt0 ? (double) colatt0->height : 0 };
    passdata.scissor = initialscis;
    passdata.blend_constant_red = 0.0f;
    passdata.blend_constant_green = 0.0f;
    passdata.blend_constant_blue = 0.0f;
    passdata.blend_constant_alpha = 0.0f;

    pass->driverdata = (void *) CFBridgingRetain(passdata);

    return 0;
}

static int
METAL_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuPipelineData *pipelinedata = (__bridge METAL_GpuPipelineData *) pipeline->driverdata;

    if (passdata.mtlpipeline != pipelinedata.mtlpipeline) {
        [passdata.mtlpass setRenderPipelineState:pipelinedata.mtlpipeline];
        passdata.mtlpipeline = pipelinedata.mtlpipeline;
    }

    if (passdata.mtldepthstencil != pipelinedata.mtldepthstencil) {
        [passdata.mtlpass setDepthStencilState:pipelinedata.mtldepthstencil];
        passdata.mtldepthstencil = pipelinedata.mtldepthstencil;
    }

    if (passdata.mtlfillmode != pipelinedata.mtlfillmode) {
        [passdata.mtlpass setTriangleFillMode:pipelinedata.mtlfillmode];
        passdata.mtlfillmode = pipelinedata.mtlfillmode;
    }

    if (passdata.mtlfrontface != pipelinedata.mtlfrontface) {
        [passdata.mtlpass setFrontFacingWinding:pipelinedata.mtlfrontface];
        passdata.mtlfrontface = pipelinedata.mtlfrontface;
    }

    if (passdata.mtlcullface != pipelinedata.mtlcullface) {
        [passdata.mtlpass setCullMode:pipelinedata.mtlcullface];
        passdata.mtlcullface = pipelinedata.mtlcullface;

    }

    if ( (passdata.depth_bias != pipelinedata.depth_bias) ||
         (passdata.depth_bias_scale != pipelinedata.depth_bias_scale) ||
         (passdata.depth_bias_clamp != pipelinedata.depth_bias_clamp) ) {
        passdata.depth_bias = pipelinedata.depth_bias;
        passdata.depth_bias_scale = pipelinedata.depth_bias_scale;
        passdata.depth_bias_clamp = pipelinedata.depth_bias_clamp;
        [passdata.mtlpass setDepthBias:passdata.depth_bias slopeScale:passdata.depth_bias_scale clamp:passdata.depth_bias_clamp];
    }

    if ( (passdata.front_stencil_reference != pipelinedata.front_stencil_reference) ||
         (passdata.back_stencil_reference != pipelinedata.back_stencil_reference) ) {
        passdata.front_stencil_reference = pipelinedata.front_stencil_reference;
        passdata.back_stencil_reference = pipelinedata.back_stencil_reference;
        [passdata.mtlpass setStencilFrontReferenceValue:passdata.front_stencil_reference backReferenceValue:passdata.back_stencil_reference];
    }

    passdata.mtlprimitive = pipelinedata.mtlprimitive;  // for future draws.

    return 0;
}

static int
METAL_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, double x, double y, double width, double height, double znear, double zfar)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    if ( (x != passdata.viewport.originX) || (y != passdata.viewport.originY) ||
         (width != passdata.viewport.width) || (height != passdata.viewport.height) ||
         (znear != passdata.viewport.znear) || (zfar != passdata.viewport.zfar) ) {
        const MTLViewport vp = { x, y, width, height, znear, zfar };
        passdata.viewport = vp;
        [passdata.mtlpass setViewport:vp];
    }
    return 0;
}

static int
METAL_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, Uint32 x, Uint32 y, Uint32 width, Uint32 height)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    if ( (x != passdata.scissor.x) || (y != passdata.scissor.y) ||
         (width != passdata.scissor.width) || (height != passdata.scissor.height) ) {
        const MTLScissorRect scis = { x, y, width, height };
        passdata.scissor = scis;
        [passdata.mtlpass setScissorRect:scis];
    }
    return 0;
}

static int
METAL_GpuSetRenderPassBlendConstant(SDL_GpuRenderPass *pass, double dred, double dgreen, double dblue, double dalpha)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    const float red = (const float) dred;
    const float green = (const float) dgreen;
    const float blue = (const float) dblue;
    const float alpha = (const float) dalpha;
    if ( (red != passdata.blend_constant_red) || (green != passdata.blend_constant_green) ||
         (blue != passdata.blend_constant_blue) || (alpha != passdata.blend_constant_alpha) ) {
        passdata.blend_constant_red = red;
        passdata.blend_constant_green = green;
        passdata.blend_constant_blue = blue;
        passdata.blend_constant_alpha = alpha;
        [passdata.mtlpass setBlendColorRed:red green:green blue:blue alpha:alpha];
    }
    return 0;
}

static int
METAL_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuBufferData *bufdata = buffer ? (__bridge METAL_GpuBufferData *) buffer->driverdata : nil;
    [passdata.mtlpass setVertexBuffer:((bufdata == nil) ? nil : bufdata.mtlbuffer) offset:offset atIndex:index];
    return 0;
}

static int
METAL_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuSamplerData *samplerdata = sampler ? (__bridge METAL_GpuSamplerData *) sampler->driverdata : nil;
    [passdata.mtlpass setVertexSamplerState:((samplerdata == nil) ? nil : samplerdata.mtlsampler) atIndex:index];
    return 0;
}

static int
METAL_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuTextureData *texturedata = texture ? (__bridge METAL_GpuTextureData *) texture->driverdata : nil;
    [passdata.mtlpass setVertexTexture:((texturedata == nil) ? nil : texturedata.mtltexture) atIndex:index];
    return 0;
}

static int
METAL_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuBufferData *bufdata = buffer ? (__bridge METAL_GpuBufferData *) buffer->driverdata : nil;
    [passdata.mtlpass setFragmentBuffer:((bufdata == nil) ? nil : bufdata.mtlbuffer) offset:offset atIndex:index];
    return 0;
}

static int
METAL_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuSamplerData *samplerdata = sampler ? (__bridge METAL_GpuSamplerData *) sampler->driverdata : nil;
    [passdata.mtlpass setFragmentSamplerState:((samplerdata == nil) ? nil : samplerdata.mtlsampler) atIndex:index];
    return 0;
}

static int
METAL_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuTextureData *texturedata = texture ? (__bridge METAL_GpuTextureData *) texture->driverdata : nil;
    [passdata.mtlpass setFragmentTexture:((texturedata == nil) ? nil : texturedata.mtltexture) atIndex:index];
    return 0;
}

static int
METAL_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    [passdata.mtlpass drawPrimitives:passdata.mtlprimitive vertexStart:vertex_start vertexCount:vertex_count];
    return 0;
}

static int
METAL_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuBufferData *idxbufdata = (__bridge METAL_GpuBufferData *) index_buffer->driverdata;
    [passdata.mtlpass drawIndexedPrimitives:passdata.mtlprimitive indexCount:index_count indexType:IndexTypeToMetal(index_type) indexBuffer:idxbufdata.mtlbuffer indexBufferOffset:index_offset];
    return 0;
}

static int
METAL_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    [passdata.mtlpass drawPrimitives:passdata.mtlprimitive vertexStart:vertex_start vertexCount:vertex_count instanceCount:instance_count baseInstance:base_instance];
    return 0;
}

static int
METAL_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_vertex, Uint32 base_instance)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    METAL_GpuBufferData *idxbufdata = (__bridge METAL_GpuBufferData *) index_buffer->driverdata;
    [passdata.mtlpass drawIndexedPrimitives:passdata.mtlprimitive indexCount:index_count indexType:IndexTypeToMetal(index_type) indexBuffer:idxbufdata.mtlbuffer indexBufferOffset:index_offset instanceCount:instance_count baseVertex:base_vertex baseInstance:base_instance];
    return 0;
}

static int
METAL_GpuEndRenderPass(SDL_GpuRenderPass *pass)
{
    METAL_GpuRenderPassData *passdata = (__bridge METAL_GpuRenderPassData *) pass->driverdata;
    [passdata.mtlpass endEncoding];
    CFBridgingRelease(pass->driverdata);
    return 0;
}

static int
METAL_GpuStartBlitPass(SDL_GpuBlitPass *pass)
{
    METAL_GpuBlitPassData *passdata = [[METAL_GpuBlitPassData alloc] init];
    if (passdata == nil) {
        return SDL_OutOfMemory();
    }

    METAL_GpuCommandBufferData *cmdbufdata = (__bridge METAL_GpuCommandBufferData *) pass->cmdbuf->driverdata;
    passdata.mtlpass = [cmdbufdata.mtlcmdbuf blitCommandEncoder];
    if (passdata.mtlpass == nil) {
        return SDL_SetError("Failed to create Metal blit command encoder!");
    }

    pass->driverdata = (void *) CFBridgingRetain(passdata);

    return 0;
}

static int
METAL_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                             Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                             SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuTextureData *srctexdata = (__bridge METAL_GpuTextureData *) srctex->driverdata;
    METAL_GpuTextureData *dsttexdata = (__bridge METAL_GpuTextureData *) dsttex->driverdata;
    [passdata.mtlpass copyFromTexture:srctexdata.mtltexture
                      sourceSlice:srcslice sourceLevel:srclevel sourceOrigin:MTLOriginMake(srcx, srcy, srcz) sourceSize:MTLSizeMake(srcw, srch, srcdepth)
                      toTexture:dsttexdata.mtltexture destinationSlice:dstslice destinationLevel:dstlevel destinationOrigin:MTLOriginMake(dstx, dsty, dstz)];
    return 0;
}

static int
METAL_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuBufferData *bufferdata = (__bridge METAL_GpuBufferData *) buffer->driverdata;
    [passdata.mtlpass fillBuffer:bufferdata.mtlbuffer range:NSMakeRange(offset, length) value:value];
    return 0;
}

static int
METAL_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuTextureData *texdata = (__bridge METAL_GpuTextureData *) texture->driverdata;
    [passdata.mtlpass generateMipmapsForTexture:texdata.mtltexture];
    return 0;
}

static int
BlitPassCopyBetweenBuffers(SDL_GpuBlitPass *pass, void *_srcbufdata, Uint32 srcoffset, void *_dstbufdata, Uint32 dstoffset, Uint32 length)
{
    // Metal abstract CPU and GPU buffers behind the same class, so we can copy either direction with the same code.
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuBufferData *srcbufdata = (__bridge METAL_GpuBufferData *) _srcbufdata;
    METAL_GpuBufferData *dstbufdata = (__bridge METAL_GpuBufferData *) _dstbufdata;
    [passdata.mtlpass copyFromBuffer:srcbufdata.mtlbuffer sourceOffset:srcoffset toBuffer:dstbufdata.mtlbuffer destinationOffset:dstoffset size:length];
    return 0;
}

static int
METAL_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    return BlitPassCopyBetweenBuffers(pass, srcbuf->driverdata, srcoffset, dstbuf->driverdata, dstoffset, length);
}

static int
METAL_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    return BlitPassCopyBetweenBuffers(pass, srcbuf->driverdata, srcoffset, dstbuf->driverdata, dstoffset, length);
}

static int
METAL_GpuCopyBufferGpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    return BlitPassCopyBetweenBuffers(pass, srcbuf->driverdata, srcoffset, dstbuf->driverdata, dstoffset, length);
}

static int
METAL_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, Uint32 srcpitch, Uint32 srcimgpitch, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuBufferData *srcbufdata = (__bridge METAL_GpuBufferData *) srcbuf->driverdata;
    METAL_GpuTextureData *dsttexdata = (__bridge METAL_GpuTextureData *) dsttex->driverdata;
    [passdata.mtlpass copyFromBuffer:srcbufdata.mtlbuffer
                      sourceOffset:srcoffset sourceBytesPerRow:srcpitch sourceBytesPerImage:srcimgpitch sourceSize:MTLSizeMake(srcw, srch, srcdepth)
                      toTexture:dsttexdata.mtltexture destinationSlice:dstslice destinationLevel:dstlevel destinationOrigin:MTLOriginMake(dstx, dsty, dstz)];
    return 0;
}

static int
METAL_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    METAL_GpuTextureData *srctexdata = (__bridge METAL_GpuTextureData *) srctex->driverdata;
    METAL_GpuBufferData *dstbufdata = (__bridge METAL_GpuBufferData *) dstbuf->driverdata;
    [passdata.mtlpass copyFromTexture:srctexdata.mtltexture
                      sourceSlice:srcslice sourceLevel:srclevel sourceOrigin:MTLOriginMake(srcx, srcy, srcz) sourceSize:MTLSizeMake(srcw, srch, srcdepth)
                      toBuffer:dstbufdata.mtlbuffer destinationOffset:dstoffset destinationBytesPerRow:dstpitch destinationBytesPerImage:dstimgpitch];
    return 0;
}

static int
METAL_GpuEndBlitPass(SDL_GpuBlitPass *pass)
{
    METAL_GpuBlitPassData *passdata = (__bridge METAL_GpuBlitPassData *) pass->driverdata;
    [passdata.mtlpass endEncoding];
    CFBridgingRelease(pass->driverdata);
    return 0;
}

static int
METAL_GpuSubmitCommandBuffer(SDL_GpuCommandBuffer *cmdbuf, SDL_GpuFence *fence)
{
    METAL_GpuCommandBufferData *cmdbufdata = (__bridge METAL_GpuCommandBufferData *) cmdbuf->driverdata;
    if (fence) {
        [cmdbufdata.mtlcmdbuf addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) fence->driverdata;
            SDL_LockMutex(fencedata->mutex);
            SDL_AtomicSet(&fencedata->flag, 1);
            SDL_CondBroadcast(fencedata->condition);
            SDL_UnlockMutex(fencedata->mutex);
        }];
    }
    [cmdbufdata.mtlcmdbuf commit];
    CFBridgingRelease(cmdbuf->driverdata);
    return 0;
}

static void
METAL_GpuAbandonCommandBuffer(SDL_GpuCommandBuffer *buffer)
{
    CFBridgingRelease(buffer->driverdata);  // !!! FIXME: I guess maybe it abandons if reference count drops to zero...?
}

static int
METAL_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *texture)
{
    METAL_GpuWindowData *windata = (__bridge METAL_GpuWindowData *) window->gpu_driverdata;
    METAL_GpuTextureData *texturedata = nil;

    SDL_assert(windata.mtldrawable == nil);   // higher level should have checked this.

    texturedata = [[METAL_GpuTextureData alloc] init];
    if (texturedata == nil) {
        return SDL_OutOfMemory();
    }

    texturedata.mtltexture = nil;
    windata.mtldrawable = [windata.mtllayer nextDrawable];
    if (windata.mtldrawable == nil) {
        return SDL_SetError("Failed to get next Metal drawable. Your window might be minimized?");
    }
    texturedata.mtltexture = windata.mtldrawable.texture;
    texturedata.mtltexture.label = [NSString stringWithUTF8String:texture->desc.label];

    texture->desc.width = texturedata.mtltexture.width;
    texture->desc.height = texturedata.mtltexture.height;
    texture->desc.pixel_format = PixelFormatFromMetal(texturedata.mtltexture.pixelFormat);
    if (texture->desc.pixel_format == SDL_GPUPIXELFMT_INVALID) {
        SDL_assert(!"Uhoh, we might need to add a new pixel format to SDL_gpu.h");
        windata.mtldrawable = nil;
        return -1;
    }

    texture->driverdata = (void *) CFBridgingRetain(texturedata);
    return 0;
}

static int
METAL_GpuPresent(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *backbuffer, int swapinterval)
{
    METAL_GpuWindowData *windata = (__bridge METAL_GpuWindowData *) window->gpu_driverdata;
    METAL_GpuTextureData *texturedata = (__bridge METAL_GpuTextureData *) backbuffer->driverdata;

    SDL_assert(windata.mtldrawable != nil);  // higher level should have checked this.

    [windata.mtldrawable present];

    // let ARC clean things up.
    texturedata.mtltexture = nil;
    windata.mtldrawable = nil;
    CFBridgingRelease(backbuffer->driverdata);
    backbuffer->driverdata = NULL;  // just in case.
    return 0;
}

static int
METAL_GpuCreateFence(SDL_GpuFence *fence)
{
    METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) SDL_calloc(1, sizeof (METAL_GpuFenceData));
    if (fencedata == NULL) {
        return SDL_OutOfMemory();
    }

    fencedata->mutex = SDL_CreateMutex();
    if (!fencedata->mutex) {
        SDL_free(fencedata);
        return -1;
    }

    fencedata->condition = SDL_CreateCond();
    if (!fencedata->condition) {
        SDL_DestroyMutex(fencedata->mutex);
        SDL_free(fencedata);
        return -1;
    }

    SDL_AtomicSet(&fencedata->flag, 0);

    fence->driverdata = fencedata;

    return 0;
}

static void
METAL_GpuDestroyFence(SDL_GpuFence *fence)
{
    METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) fence->driverdata;
    SDL_DestroyMutex(fencedata->mutex);
    SDL_DestroyCond(fencedata->condition);
    SDL_free(fencedata);
}

static int
METAL_GpuQueryFence(SDL_GpuFence *fence)
{
    METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) fence->driverdata;
    return SDL_AtomicGet(&fencedata->flag);
}

static int
METAL_GpuResetFence(SDL_GpuFence *fence)
{
    METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) fence->driverdata;
    SDL_AtomicSet(&fencedata->flag, 0);
    return 0;
}

static int
METAL_GpuWaitFence(SDL_GpuFence *fence)
{
    METAL_GpuFenceData *fencedata = (METAL_GpuFenceData *) fence->driverdata;

    if (SDL_LockMutex(fencedata->mutex) == -1) {
        return -1;
    }

    while (SDL_AtomicGet(&fencedata->flag) == 0) {
        if (SDL_CondWait(fencedata->condition, fencedata->mutex) == -1) {
            SDL_UnlockMutex(fencedata->mutex);
            return -1;
        }
    }

    SDL_UnlockMutex(fencedata->mutex);

    return 0;
}

static void
METAL_GpuDestroyDevice(SDL_GpuDevice *device)
{
    CFBridgingRelease(device->driverdata);
}

static int
IsMetalAvailable(void)
{
    // !!! FIXME: iOS 8 has Metal but is missing a few small features they fixed in iOS 9.
    // !!! FIXME: It can probably limp along, but we should probably just refuse to run there. It's ancient anyhow.
    SDL_VideoDevice *viddev = SDL_GetVideoDevice();

    SDL_assert(viddev != NULL);

    if ((SDL_strcmp(viddev->name, "cocoa") != 0) && (SDL_strcmp(viddev->name, "uikit") != 0)) {
        return SDL_SetError("Metal GPU driver only supports Cocoa and UIKit video targets at the moment.");
    }

    // this checks a weak symbol.
#if (defined(__MACOSX__) && (MAC_OS_X_VERSION_MIN_REQUIRED < 101100))
    if (MTLCreateSystemDefaultDevice == NULL) {  // probably on 10.10 or lower.
        return SDL_SetError("Metal framework not available on this system");
    }
#endif

    return 0;
}

static int
METAL_GpuCreateDevice(SDL_GpuDevice *device)
{
    METAL_GpuDeviceData *devdata;

    if (IsMetalAvailable() == -1) {
        return -1;
    }

    devdata = [[METAL_GpuDeviceData alloc] init];
    if (!devdata) {
        return SDL_OutOfMemory();
    }

    // !!! FIXME: MTLCopyAllDevices() can find other GPUs on macOS...
    devdata.mtldevice = MTLCreateSystemDefaultDevice();
    if (devdata.mtldevice == nil) {
        return SDL_SetError("Failed to obtain Metal device");
    }

    devdata.mtlcmdqueue = [devdata.mtldevice newCommandQueue];
    if (devdata.mtlcmdqueue == nil) {
        return SDL_SetError("Failed to create Metal command queue");
    }

    if (device->label) {
        devdata.mtlcmdqueue.label = [NSString stringWithUTF8String:device->label];
    }

    device->driverdata = (void *) CFBridgingRetain(devdata);
    device->DestroyDevice = METAL_GpuDestroyDevice;
    device->ClaimWindow = METAL_GpuClaimWindow;
    device->CreateCpuBuffer = METAL_GpuCreateCpuBuffer;
    device->DestroyCpuBuffer = METAL_GpuDestroyCpuBuffer;
    device->LockCpuBuffer = METAL_GpuLockCpuBuffer;
    device->UnlockCpuBuffer = METAL_GpuUnlockCpuBuffer;
    device->CreateBuffer = METAL_GpuCreateBuffer;
    device->DestroyBuffer = METAL_GpuDestroyBuffer;
    device->CreateTexture = METAL_GpuCreateTexture;
    device->DestroyTexture = METAL_GpuDestroyTexture;
    device->CreateShader = METAL_GpuCreateShader;
    device->DestroyShader = METAL_GpuDestroyShader;
    device->CreatePipeline = METAL_GpuCreatePipeline;
    device->DestroyPipeline = METAL_GpuDestroyPipeline;
    device->CreateSampler = METAL_GpuCreateSampler;
    device->DestroySampler = METAL_GpuDestroySampler;
    device->CreateCommandBuffer = METAL_GpuCreateCommandBuffer;
    device->StartRenderPass = METAL_GpuStartRenderPass;
    device->SetRenderPassPipeline = METAL_GpuSetRenderPassPipeline;
    device->SetRenderPassViewport = METAL_GpuSetRenderPassViewport;
    device->SetRenderPassScissor = METAL_GpuSetRenderPassScissor;
    device->SetRenderPassBlendConstant = METAL_GpuSetRenderPassBlendConstant;
    device->SetRenderPassVertexBuffer = METAL_GpuSetRenderPassVertexBuffer;
    device->SetRenderPassVertexSampler = METAL_GpuSetRenderPassVertexSampler;
    device->SetRenderPassVertexTexture = METAL_GpuSetRenderPassVertexTexture;
    device->SetRenderPassFragmentBuffer = METAL_GpuSetRenderPassFragmentBuffer;
    device->SetRenderPassFragmentSampler = METAL_GpuSetRenderPassFragmentSampler;
    device->SetRenderPassFragmentTexture = METAL_GpuSetRenderPassFragmentTexture;
    device->Draw = METAL_GpuDraw;
    device->DrawIndexed = METAL_GpuDrawIndexed;
    device->DrawInstanced = METAL_GpuDrawInstanced;
    device->DrawInstancedIndexed = METAL_GpuDrawInstancedIndexed;
    device->EndRenderPass = METAL_GpuEndRenderPass;
    device->StartBlitPass = METAL_GpuStartBlitPass;
    device->CopyBetweenTextures = METAL_GpuCopyBetweenTextures;
    device->FillBuffer = METAL_GpuFillBuffer;
    device->GenerateMipmaps = METAL_GpuGenerateMipmaps;
    device->CopyBufferCpuToGpu = METAL_GpuCopyBufferCpuToGpu;
    device->CopyBufferGpuToCpu = METAL_GpuCopyBufferGpuToCpu;
    device->CopyBufferGpuToGpu = METAL_GpuCopyBufferGpuToGpu;
    device->CopyFromBufferToTexture = METAL_GpuCopyFromBufferToTexture;
    device->CopyFromTextureToBuffer = METAL_GpuCopyFromTextureToBuffer;
    device->EndBlitPass = METAL_GpuEndBlitPass;
    device->SubmitCommandBuffer = METAL_GpuSubmitCommandBuffer;
    device->AbandonCommandBuffer = METAL_GpuAbandonCommandBuffer;
    device->GetBackbuffer = METAL_GpuGetBackbuffer;
    device->Present = METAL_GpuPresent;
    device->CreateFence = METAL_GpuCreateFence;
    device->DestroyFence = METAL_GpuDestroyFence;
    device->QueryFence = METAL_GpuQueryFence;
    device->ResetFence = METAL_GpuResetFence;
    device->WaitFence = METAL_GpuWaitFence;

    return 0;
}

const SDL_GpuDriver METAL_GpuDriver = {
    "metal", METAL_GpuCreateDevice
};

#endif

/* vi: set ts=4 sw=4 expandtab: */
