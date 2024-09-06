/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_internal.h"

#ifdef SDL_GPU_METAL

#include <Metal/Metal.h>
#include <QuartzCore/CoreAnimation.h>

#include "../SDL_sysgpu.h"

// Defines

#define METAL_MAX_BUFFER_COUNT      31
#define WINDOW_PROPERTY_DATA        "SDL_GPUMetalWindowPropertyData"
#define SDL_GPU_SHADERSTAGE_COMPUTE 2

#define TRACK_RESOURCE(resource, type, array, count, capacity) \
    Uint32 i;                                                  \
                                                               \
    for (i = 0; i < command_buffer->count; i += 1) {           \
        if (command_buffer->array[i] == resource) {            \
            return;                                            \
        }                                                      \
    }                                                          \
                                                               \
    if (command_buffer->count == command_buffer->capacity) {   \
        command_buffer->capacity += 1;                         \
        command_buffer->array = SDL_realloc(                   \
            command_buffer->array,                             \
            command_buffer->capacity * sizeof(type));          \
    }                                                          \
    command_buffer->array[command_buffer->count] = resource;   \
    command_buffer->count += 1;                                \
    SDL_AtomicIncRef(&resource->referenceCount);

// Blit Shaders

#include "Metal_Blit.h"

// Forward Declarations

static void METAL_Wait(SDL_GPURenderer *driverData);
static void METAL_ReleaseWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window);
static void METAL_INTERNAL_DestroyBlitResources(SDL_GPURenderer *driverData);

// Conversions

static MTLPixelFormat SDLToMetal_SurfaceFormat[] = {
    MTLPixelFormatA8Unorm,      // A8_UNORM
    MTLPixelFormatR8Unorm,      // R8_UNORM
    MTLPixelFormatRG8Unorm,     // R8G8_UNORM
    MTLPixelFormatRGBA8Unorm,   // R8G8B8A8_UNORM
    MTLPixelFormatR16Unorm,     // R16_UNORM
    MTLPixelFormatRG16Unorm,    // R16G16_UNORM
    MTLPixelFormatRGBA16Unorm,  // R16G16B16A16_UNORM
    MTLPixelFormatRGB10A2Unorm, // A2R10G10B10_UNORM
    MTLPixelFormatB5G6R5Unorm,  // B5G6R5_UNORM
    MTLPixelFormatBGR5A1Unorm,  // B5G5R5A1_UNORM
    MTLPixelFormatABGR4Unorm,   // B4G4R4A4_UNORM
    MTLPixelFormatBGRA8Unorm,   // B8G8R8A8_UNORM
#ifdef SDL_PLATFORM_MACOS
    MTLPixelFormatBC1_RGBA,       // BC1_UNORM
    MTLPixelFormatBC2_RGBA,       // BC2_UNORM
    MTLPixelFormatBC3_RGBA,       // BC3_UNORM
    MTLPixelFormatBC4_RUnorm,     // BC4_UNORM
    MTLPixelFormatBC5_RGUnorm,    // BC5_UNORM
    MTLPixelFormatBC7_RGBAUnorm,  // BC7_UNORM
    MTLPixelFormatBC6H_RGBFloat,  // BC6H_FLOAT
    MTLPixelFormatBC6H_RGBUfloat, // BC6H_UFLOAT
#else
    MTLPixelFormatInvalid, // BC1_UNORM
    MTLPixelFormatInvalid, // BC2_UNORM
    MTLPixelFormatInvalid, // BC3_UNORM
    MTLPixelFormatInvalid, // BC4_UNORM
    MTLPixelFormatInvalid, // BC5_UNORM
    MTLPixelFormatInvalid, // BC7_UNORM
    MTLPixelFormatInvalid, // BC6H_FLOAT
    MTLPixelFormatInvalid, // BC6H_UFLOAT
#endif
    MTLPixelFormatR8Snorm,         // R8_SNORM
    MTLPixelFormatRG8Snorm,        // R8G8_SNORM
    MTLPixelFormatRGBA8Snorm,      // R8G8B8A8_SNORM
    MTLPixelFormatR16Snorm,        // R16_SNORM
    MTLPixelFormatRG16Snorm,       // R16G16_SNORM
    MTLPixelFormatRGBA16Snorm,     // R16G16B16A16_SNORM
    MTLPixelFormatR16Float,        // R16_FLOAT
    MTLPixelFormatRG16Float,       // R16G16_FLOAT
    MTLPixelFormatRGBA16Float,     // R16G16B16A16_FLOAT
    MTLPixelFormatR32Float,        // R32_FLOAT
    MTLPixelFormatRG32Float,       // R32G32_FLOAT
    MTLPixelFormatRGBA32Float,     // R32G32B32A32_FLOAT
    MTLPixelFormatRG11B10Float,    // R11G11B10_UFLOAT
    MTLPixelFormatR8Uint,          // R8_UINT
    MTLPixelFormatRG8Uint,         // R8G8_UINT
    MTLPixelFormatRGBA8Uint,       // R8G8B8A8_UINT
    MTLPixelFormatR16Uint,         // R16_UINT
    MTLPixelFormatRG16Uint,        // R16G16_UINT
    MTLPixelFormatRGBA16Uint,      // R16G16B16A16_UINT
    MTLPixelFormatR8Sint,          // R8_UINT
    MTLPixelFormatRG8Sint,         // R8G8_UINT
    MTLPixelFormatRGBA8Sint,       // R8G8B8A8_UINT
    MTLPixelFormatR16Sint,         // R16_UINT
    MTLPixelFormatRG16Sint,        // R16G16_UINT
    MTLPixelFormatRGBA16Sint,      // R16G16B16A16_UINT
    MTLPixelFormatRGBA8Unorm_sRGB, // R8G8B8A8_UNORM_SRGB
    MTLPixelFormatBGRA8Unorm_sRGB, // B8G8R8A8_UNORM_SRGB
#ifdef SDL_PLATFORM_MACOS
    MTLPixelFormatBC1_RGBA_sRGB,      // BC1_UNORM_SRGB
    MTLPixelFormatBC2_RGBA_sRGB,      // BC2_UNORM_SRGB
    MTLPixelFormatBC3_RGBA_sRGB,      // BC3_UNORM_SRGB
    MTLPixelFormatBC7_RGBAUnorm_sRGB, // BC7_UNORM_SRGB
#else
    MTLPixelFormatInvalid, // BC1_UNORM_SRGB
    MTLPixelFormatInvalid, // BC2_UNORM_SRGB
    MTLPixelFormatInvalid, // BC3_UNORM_SRGB
    MTLPixelFormatInvalid, // BC7_UNORM_SRGB
#endif
    MTLPixelFormatDepth16Unorm, // D16_UNORM
#ifdef SDL_PLATFORM_MACOS
    MTLPixelFormatDepth24Unorm_Stencil8, // D24_UNORM
#else
    MTLPixelFormatInvalid, // D24_UNORM
#endif
    MTLPixelFormatDepth32Float, // D32_FLOAT
#ifdef SDL_PLATFORM_MACOS
    MTLPixelFormatDepth24Unorm_Stencil8, // D24_UNORM_S8_UINT
#else
    MTLPixelFormatInvalid, // D24_UNORM_S8_UINT
#endif
    MTLPixelFormatDepth32Float_Stencil8, // D32_FLOAT_S8_UINT
};
SDL_COMPILE_TIME_ASSERT(SDLToMetal_SurfaceFormat, SDL_arraysize(SDLToMetal_SurfaceFormat) == SDL_GPU_TEXTUREFORMAT_MAX);

static MTLVertexFormat SDLToMetal_VertexFormat[] = {
    MTLVertexFormatInt,               // INT
    MTLVertexFormatInt2,              // INT2
    MTLVertexFormatInt3,              // INT3
    MTLVertexFormatInt4,              // INT4
    MTLVertexFormatUInt,              // UINT
    MTLVertexFormatUInt2,             // UINT2
    MTLVertexFormatUInt3,             // UINT3
    MTLVertexFormatUInt4,             // UINT4
    MTLVertexFormatFloat,             // FLOAT
    MTLVertexFormatFloat2,            // FLOAT2
    MTLVertexFormatFloat3,            // FLOAT3
    MTLVertexFormatFloat4,            // FLOAT4
    MTLVertexFormatChar2,             // BYTE2
    MTLVertexFormatChar4,             // BYTE4
    MTLVertexFormatUChar2,            // UBYTE2
    MTLVertexFormatUChar4,            // UBYTE4
    MTLVertexFormatChar2Normalized,   // BYTE2_NORM
    MTLVertexFormatChar4Normalized,   // BYTE4_NORM
    MTLVertexFormatUChar2Normalized,  // UBYTE2_NORM
    MTLVertexFormatUChar4Normalized,  // UBYTE4_NORM
    MTLVertexFormatShort2,            // SHORT2
    MTLVertexFormatShort4,            // SHORT4
    MTLVertexFormatUShort2,           // USHORT2
    MTLVertexFormatUShort4,           // USHORT4
    MTLVertexFormatShort2Normalized,  // SHORT2_NORM
    MTLVertexFormatShort4Normalized,  // SHORT4_NORM
    MTLVertexFormatUShort2Normalized, // USHORT2_NORM
    MTLVertexFormatUShort4Normalized, // USHORT4_NORM
    MTLVertexFormatHalf2,             // HALF2
    MTLVertexFormatHalf4              // HALF4
};

static MTLIndexType SDLToMetal_IndexType[] = {
    MTLIndexTypeUInt16, // 16BIT
    MTLIndexTypeUInt32, // 32BIT
};

static MTLPrimitiveType SDLToMetal_PrimitiveType[] = {
    MTLPrimitiveTypePoint,        // POINTLIST
    MTLPrimitiveTypeLine,         // LINELIST
    MTLPrimitiveTypeLineStrip,    // LINESTRIP
    MTLPrimitiveTypeTriangle,     // TRIANGLELIST
    MTLPrimitiveTypeTriangleStrip // TRIANGLESTRIP
};

static MTLTriangleFillMode SDLToMetal_PolygonMode[] = {
    MTLTriangleFillModeFill,  // FILL
    MTLTriangleFillModeLines, // LINE
};

static MTLCullMode SDLToMetal_CullMode[] = {
    MTLCullModeNone,  // NONE
    MTLCullModeFront, // FRONT
    MTLCullModeBack,  // BACK
};

static MTLWinding SDLToMetal_FrontFace[] = {
    MTLWindingCounterClockwise, // COUNTER_CLOCKWISE
    MTLWindingClockwise,        // CLOCKWISE
};

static MTLBlendFactor SDLToMetal_BlendFactor[] = {
    MTLBlendFactorZero,                     // ZERO
    MTLBlendFactorOne,                      // ONE
    MTLBlendFactorSourceColor,              // SRC_COLOR
    MTLBlendFactorOneMinusSourceColor,      // ONE_MINUS_SRC_COLOR
    MTLBlendFactorDestinationColor,         // DST_COLOR
    MTLBlendFactorOneMinusDestinationColor, // ONE_MINUS_DST_COLOR
    MTLBlendFactorSourceAlpha,              // SRC_ALPHA
    MTLBlendFactorOneMinusSourceAlpha,      // ONE_MINUS_SRC_ALPHA
    MTLBlendFactorDestinationAlpha,         // DST_ALPHA
    MTLBlendFactorOneMinusDestinationAlpha, // ONE_MINUS_DST_ALPHA
    MTLBlendFactorBlendColor,               // CONSTANT_COLOR
    MTLBlendFactorOneMinusBlendColor,       // ONE_MINUS_CONSTANT_COLOR
    MTLBlendFactorSourceAlphaSaturated,     // SRC_ALPHA_SATURATE
};

static MTLBlendOperation SDLToMetal_BlendOp[] = {
    MTLBlendOperationAdd,             // ADD
    MTLBlendOperationSubtract,        // SUBTRACT
    MTLBlendOperationReverseSubtract, // REVERSE_SUBTRACT
    MTLBlendOperationMin,             // MIN
    MTLBlendOperationMax,             // MAX
};

static MTLCompareFunction SDLToMetal_CompareOp[] = {
    MTLCompareFunctionNever,        // NEVER
    MTLCompareFunctionLess,         // LESS
    MTLCompareFunctionEqual,        // EQUAL
    MTLCompareFunctionLessEqual,    // LESS_OR_EQUAL
    MTLCompareFunctionGreater,      // GREATER
    MTLCompareFunctionNotEqual,     // NOT_EQUAL
    MTLCompareFunctionGreaterEqual, // GREATER_OR_EQUAL
    MTLCompareFunctionAlways,       // ALWAYS
};

static MTLStencilOperation SDLToMetal_StencilOp[] = {
    MTLStencilOperationKeep,           // KEEP
    MTLStencilOperationZero,           // ZERO
    MTLStencilOperationReplace,        // REPLACE
    MTLStencilOperationIncrementClamp, // INCREMENT_AND_CLAMP
    MTLStencilOperationDecrementClamp, // DECREMENT_AND_CLAMP
    MTLStencilOperationInvert,         // INVERT
    MTLStencilOperationIncrementWrap,  // INCREMENT_AND_WRAP
    MTLStencilOperationDecrementWrap,  // DECREMENT_AND_WRAP
};

static MTLSamplerAddressMode SDLToMetal_SamplerAddressMode[] = {
    MTLSamplerAddressModeRepeat,       // REPEAT
    MTLSamplerAddressModeMirrorRepeat, // MIRRORED_REPEAT
    MTLSamplerAddressModeClampToEdge   // CLAMP_TO_EDGE
};

static MTLSamplerMinMagFilter SDLToMetal_MinMagFilter[] = {
    MTLSamplerMinMagFilterNearest, // NEAREST
    MTLSamplerMinMagFilterLinear,  // LINEAR
};

static MTLSamplerMipFilter SDLToMetal_MipFilter[] = {
    MTLSamplerMipFilterNearest, // NEAREST
    MTLSamplerMipFilterLinear,  // LINEAR
};

static MTLLoadAction SDLToMetal_LoadOp[] = {
    MTLLoadActionLoad,     // LOAD
    MTLLoadActionClear,    // CLEAR
    MTLLoadActionDontCare, // DONT_CARE
};

static MTLVertexStepFunction SDLToMetal_StepFunction[] = {
    MTLVertexStepFunctionPerVertex,
    MTLVertexStepFunctionPerInstance,
};

static NSUInteger SDLToMetal_SampleCount[] = {
    1, // SDL_GPU_SAMPLECOUNT_1
    2, // SDL_GPU_SAMPLECOUNT_2
    4, // SDL_GPU_SAMPLECOUNT_4
    8  // SDL_GPU_SAMPLECOUNT_8
};

static MTLTextureType SDLToMetal_TextureType[] = {
    MTLTextureType2D,      // SDL_GPU_TEXTURETYPE_2D
    MTLTextureType2DArray, // SDL_GPU_TEXTURETYPE_2D_ARRAY
    MTLTextureType3D,      // SDL_GPU_TEXTURETYPE_3D
    MTLTextureTypeCube     // SDL_GPU_TEXTURETYPE_CUBE
};

static SDL_GPUTextureFormat SwapchainCompositionToFormat[] = {
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,      // SDR
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB, // SDR_LINEAR
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,  // HDR_EXTENDED_LINEAR
    SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,   // HDR10_ST2048
};

static CFStringRef SwapchainCompositionToColorSpace[4]; // initialized on device creation

static MTLStoreAction SDLToMetal_StoreOp(
    SDL_GPUStoreOp store_op,
    Uint8 isMultisample)
{
    if (isMultisample) {
        if (store_op == SDL_GPU_STOREOP_STORE) {
            return MTLStoreActionStoreAndMultisampleResolve;
        } else {
            return MTLStoreActionMultisampleResolve;
        }
    } else {
        if (store_op == SDL_GPU_STOREOP_STORE) {
            return MTLStoreActionStore;
        } else {
            return MTLStoreActionDontCare;
        }
    }
};

static MTLColorWriteMask SDLToMetal_ColorWriteMask(
    SDL_GPUColorComponentFlags mask)
{
    MTLColorWriteMask result = 0;
    if (mask & SDL_GPU_COLORCOMPONENT_R) {
        result |= MTLColorWriteMaskRed;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_G) {
        result |= MTLColorWriteMaskGreen;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_B) {
        result |= MTLColorWriteMaskBlue;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_A) {
        result |= MTLColorWriteMaskAlpha;
    }
    return result;
}

// Structs

typedef struct MetalTexture
{
    id<MTLTexture> handle;
    id<MTLTexture> msaaHandle;
    SDL_AtomicInt referenceCount;
} MetalTexture;

typedef struct MetalTextureContainer
{
    TextureCommonHeader header;

    MetalTexture *activeTexture;
    Uint8 canBeCycled;

    Uint32 textureCapacity;
    Uint32 textureCount;
    MetalTexture **textures;

    char *debugName;
} MetalTextureContainer;

typedef struct MetalFence
{
    SDL_AtomicInt complete;
} MetalFence;

typedef struct MetalWindowData
{
    SDL_Window *window;
    SDL_MetalView view;
    CAMetalLayer *layer;
    id<CAMetalDrawable> drawable;
    MetalTexture texture;
    MetalTextureContainer textureContainer;
} MetalWindowData;

typedef struct MetalShader
{
    id<MTLLibrary> library;
    id<MTLFunction> function;

    Uint32 num_samplers;
    Uint32 num_uniform_buffers;
    Uint32 num_storage_buffers;
    Uint32 num_storage_textures;
} MetalShader;

typedef struct MetalGraphicsPipeline
{
    id<MTLRenderPipelineState> handle;

    Uint32 sample_mask;

    SDL_GPURasterizerState rasterizer_state;
    SDL_GPUPrimitiveType primitive_type;

    id<MTLDepthStencilState> depth_stencil_state;

    Uint32 vertexSamplerCount;
    Uint32 vertexUniformBufferCount;
    Uint32 vertexStorageBufferCount;
    Uint32 vertexStorageTextureCount;

    Uint32 fragmentSamplerCount;
    Uint32 fragmentUniformBufferCount;
    Uint32 fragmentStorageBufferCount;
    Uint32 fragmentStorageTextureCount;
} MetalGraphicsPipeline;

typedef struct MetalComputePipeline
{
    id<MTLComputePipelineState> handle;
    Uint32 num_readonly_storage_textures;
    Uint32 num_writeonly_storage_textures;
    Uint32 num_readonly_storage_buffers;
    Uint32 num_writeonly_storage_buffers;
    Uint32 num_uniform_buffers;
    Uint32 threadcount_x;
    Uint32 threadcount_y;
    Uint32 threadcount_z;
} MetalComputePipeline;

typedef struct MetalBuffer
{
    id<MTLBuffer> handle;
    SDL_AtomicInt referenceCount;
} MetalBuffer;

typedef struct MetalBufferContainer
{
    MetalBuffer *activeBuffer;
    Uint32 size;

    Uint32 bufferCapacity;
    Uint32 bufferCount;
    MetalBuffer **buffers;

    bool isPrivate;
    bool isWriteOnly;
    char *debugName;
} MetalBufferContainer;

typedef struct MetalUniformBuffer
{
    id<MTLBuffer> handle;
    Uint32 writeOffset;
    Uint32 drawOffset;
} MetalUniformBuffer;

typedef struct MetalRenderer MetalRenderer;

typedef struct MetalCommandBuffer
{
    CommandBufferCommonHeader common;
    MetalRenderer *renderer;

    // Native Handle
    id<MTLCommandBuffer> handle;

    // Presentation
    MetalWindowData **windowDatas;
    Uint32 windowDataCount;
    Uint32 windowDataCapacity;

    // Render Pass
    id<MTLRenderCommandEncoder> renderEncoder;
    MetalGraphicsPipeline *graphics_pipeline;
    MetalBuffer *indexBuffer;
    Uint32 indexBufferOffset;
    SDL_GPUIndexElementSize index_element_size;

    // Copy Pass
    id<MTLBlitCommandEncoder> blitEncoder;

    // Compute Pass
    id<MTLComputeCommandEncoder> computeEncoder;
    MetalComputePipeline *compute_pipeline;

    // Resource slot state
    bool needVertexSamplerBind;
    bool needVertexStorageTextureBind;
    bool needVertexStorageBufferBind;
    bool needVertexUniformBind;

    bool needFragmentSamplerBind;
    bool needFragmentStorageTextureBind;
    bool needFragmentStorageBufferBind;
    bool needFragmentUniformBind;

    bool needComputeTextureBind;
    bool needComputeBufferBind;
    bool needComputeUniformBind;

    id<MTLSamplerState> vertexSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    id<MTLTexture> vertexTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    id<MTLTexture> vertexStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    id<MTLBuffer> vertexStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    id<MTLSamplerState> fragmentSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    id<MTLTexture> fragmentTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    id<MTLTexture> fragmentStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    id<MTLBuffer> fragmentStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    id<MTLTexture> computeReadOnlyTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    id<MTLBuffer> computeReadOnlyBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];
    id<MTLTexture> computeWriteOnlyTextures[MAX_COMPUTE_WRITE_TEXTURES];
    id<MTLBuffer> computeWriteOnlyBuffers[MAX_COMPUTE_WRITE_BUFFERS];

    // Uniform buffers
    MetalUniformBuffer *vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    MetalUniformBuffer *fragmentUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    MetalUniformBuffer *computeUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];

    MetalUniformBuffer **usedUniformBuffers;
    Uint32 usedUniformBufferCount;
    Uint32 usedUniformBufferCapacity;

    // Fences
    MetalFence *fence;
    Uint8 autoReleaseFence;

    // Reference Counting
    MetalBuffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    MetalTexture **usedTextures;
    Uint32 usedTextureCount;
    Uint32 usedTextureCapacity;
} MetalCommandBuffer;

typedef struct MetalSampler
{
    id<MTLSamplerState> handle;
} MetalSampler;

typedef struct BlitPipeline
{
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUTextureFormat format;
} BlitPipeline;

struct MetalRenderer
{
    // Reference to the parent device
    SDL_GPUDevice *sdlGPUDevice;

    id<MTLDevice> device;
    id<MTLCommandQueue> queue;

    bool debug_mode;

    MetalWindowData **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;

    MetalCommandBuffer **availableCommandBuffers;
    Uint32 availableCommandBufferCount;
    Uint32 availableCommandBufferCapacity;

    MetalCommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCount;
    Uint32 submittedCommandBufferCapacity;

    MetalFence **availableFences;
    Uint32 availableFenceCount;
    Uint32 availableFenceCapacity;

    MetalUniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    MetalBufferContainer **bufferContainersToDestroy;
    Uint32 bufferContainersToDestroyCount;
    Uint32 bufferContainersToDestroyCapacity;

    MetalTextureContainer **textureContainersToDestroy;
    Uint32 textureContainersToDestroyCount;
    Uint32 textureContainersToDestroyCapacity;

    // Blit
    SDL_GPUShader *blitVertexShader;
    SDL_GPUShader *blitFrom2DShader;
    SDL_GPUShader *blitFrom2DArrayShader;
    SDL_GPUShader *blitFrom3DShader;
    SDL_GPUShader *blitFromCubeShader;

    SDL_GPUSampler *blitNearestSampler;
    SDL_GPUSampler *blitLinearSampler;

    BlitPipelineCacheEntry *blitPipelines;
    Uint32 blitPipelineCount;
    Uint32 blitPipelineCapacity;

    // Mutexes
    SDL_Mutex *submitLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *acquireUniformBufferLock;
    SDL_Mutex *disposeLock;
    SDL_Mutex *fenceLock;
    SDL_Mutex *windowLock;
};

// Helper Functions

static Uint32 METAL_INTERNAL_GetVertexBufferIndex(Uint32 binding)
{
    return METAL_MAX_BUFFER_COUNT - 1 - binding;
}

// FIXME: This should be moved into SDL_sysgpu.h
static inline Uint32 METAL_INTERNAL_NextHighestAlignment(
    Uint32 n,
    Uint32 align)
{
    return align * ((n + align - 1) / align);
}

// Quit

static void METAL_DestroyDevice(SDL_GPUDevice *device)
{
    MetalRenderer *renderer = (MetalRenderer *)device->driverData;

    // Flush any remaining GPU work...
    METAL_Wait(device->driverData);

    // Release the window data
    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        METAL_ReleaseWindow(device->driverData, renderer->claimedWindows[i]->window);
    }
    SDL_free(renderer->claimedWindows);

    // Release the blit resources
    METAL_INTERNAL_DestroyBlitResources(device->driverData);

    // Release uniform buffers
    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i]->handle = nil;
        SDL_free(renderer->uniformBufferPool[i]);
    }
    SDL_free(renderer->uniformBufferPool);

    // Release destroyed resource lists
    SDL_free(renderer->bufferContainersToDestroy);
    SDL_free(renderer->textureContainersToDestroy);

    // Release command buffer infrastructure
    for (Uint32 i = 0; i < renderer->availableCommandBufferCount; i += 1) {
        MetalCommandBuffer *command_buffer = renderer->availableCommandBuffers[i];
        SDL_free(command_buffer->usedBuffers);
        SDL_free(command_buffer->usedTextures);
        SDL_free(command_buffer->usedUniformBuffers);
        SDL_free(command_buffer->windowDatas);
        SDL_free(command_buffer);
    }
    SDL_free(renderer->availableCommandBuffers);
    SDL_free(renderer->submittedCommandBuffers);

    // Release fence infrastructure
    for (Uint32 i = 0; i < renderer->availableFenceCount; i += 1) {
        SDL_free(renderer->availableFences[i]);
    }
    SDL_free(renderer->availableFences);

    // Release the mutexes
    SDL_DestroyMutex(renderer->submitLock);
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->acquireUniformBufferLock);
    SDL_DestroyMutex(renderer->disposeLock);
    SDL_DestroyMutex(renderer->fenceLock);
    SDL_DestroyMutex(renderer->windowLock);

    // Release the command queue
    renderer->queue = nil;

    // Free the primary structures
    SDL_free(renderer);
    SDL_free(device);
}

// Resource tracking

static void METAL_INTERNAL_TrackBuffer(
    MetalCommandBuffer *command_buffer,
    MetalBuffer *buffer)
{
    TRACK_RESOURCE(
        buffer,
        MetalBuffer *,
        usedBuffers,
        usedBufferCount,
        usedBufferCapacity);
}

static void METAL_INTERNAL_TrackTexture(
    MetalCommandBuffer *command_buffer,
    MetalTexture *texture)
{
    TRACK_RESOURCE(
        texture,
        MetalTexture *,
        usedTextures,
        usedTextureCount,
        usedTextureCapacity);
}

static void METAL_INTERNAL_TrackUniformBuffer(
    MetalCommandBuffer *command_buffer,
    MetalUniformBuffer *uniformBuffer)
{
    Uint32 i;
    for (i = 0; i < command_buffer->usedUniformBufferCount; i += 1) {
        if (command_buffer->usedUniformBuffers[i] == uniformBuffer) {
            return;
        }
    }

    if (command_buffer->usedUniformBufferCount == command_buffer->usedUniformBufferCapacity) {
        command_buffer->usedUniformBufferCapacity += 1;
        command_buffer->usedUniformBuffers = SDL_realloc(
            command_buffer->usedUniformBuffers,
            command_buffer->usedUniformBufferCapacity * sizeof(MetalUniformBuffer *));
    }

    command_buffer->usedUniformBuffers[command_buffer->usedUniformBufferCount] = uniformBuffer;
    command_buffer->usedUniformBufferCount += 1;
}

// Shader Compilation

typedef struct MetalLibraryFunction
{
    id<MTLLibrary> library;
    id<MTLFunction> function;
} MetalLibraryFunction;

// This function assumes that it's called from within an autorelease pool
static MetalLibraryFunction METAL_INTERNAL_CompileShader(
    MetalRenderer *renderer,
    SDL_GPUShaderFormat format,
    const Uint8 *code,
    size_t code_size,
    const char *entrypoint_name)
{
    MetalLibraryFunction libraryFunction = { nil, nil };
    id<MTLLibrary> library;
    NSError *error;
    dispatch_data_t data;
    id<MTLFunction> function;

    if (format == SDL_GPU_SHADERFORMAT_MSL) {
        NSString *codeString = [[NSString alloc]
            initWithBytes:code
                   length:code_size
                 encoding:NSUTF8StringEncoding];
        library = [renderer->device
            newLibraryWithSource:codeString
                         options:nil
                           error:&error];
    } else if (format == SDL_GPU_SHADERFORMAT_METALLIB) {
        data = dispatch_data_create(
            code,
            code_size,
            dispatch_get_global_queue(0, 0),
            ^{ /* do nothing */ });
        library = [renderer->device newLibraryWithData:data error:&error];
    } else {
        SDL_assert(!"SDL_gpu.c should have already validated this!");
        return libraryFunction;
    }

    if (library == nil) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "Creating MTLLibrary failed: %s",
            [[error description] cStringUsingEncoding:[NSString defaultCStringEncoding]]);
        return libraryFunction;
    } else if (error != nil) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "Creating MTLLibrary failed: %s",
            [[error description] cStringUsingEncoding:[NSString defaultCStringEncoding]]);
    }

    function = [library newFunctionWithName:@(entrypoint_name)];
    if (function == nil) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "Creating MTLFunction failed");
        return libraryFunction;
    }

    libraryFunction.library = library;
    libraryFunction.function = function;
    return libraryFunction;
}

// Disposal

static void METAL_INTERNAL_DestroyTextureContainer(
    MetalTextureContainer *container)
{
    for (Uint32 i = 0; i < container->textureCount; i += 1) {
        container->textures[i]->handle = nil;
        container->textures[i]->msaaHandle = nil;
        SDL_free(container->textures[i]);
    }
    if (container->debugName != NULL) {
        SDL_free(container->debugName);
    }
    SDL_free(container->textures);
    SDL_free(container);
}

static void METAL_ReleaseTexture(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture)
{
    MetalRenderer *renderer = (MetalRenderer *)driverData;
    MetalTextureContainer *container = (MetalTextureContainer *)texture;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->textureContainersToDestroy,
        MetalTextureContainer *,
        renderer->textureContainersToDestroyCount + 1,
        renderer->textureContainersToDestroyCapacity,
        renderer->textureContainersToDestroyCapacity + 1);

    renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount] = container;
    renderer->textureContainersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void METAL_ReleaseSampler(
    SDL_GPURenderer *driverData,
    SDL_GPUSampler *sampler)
{
    @autoreleasepool {
        MetalSampler *metalSampler = (MetalSampler *)sampler;
        metalSampler->handle = nil;
        SDL_free(metalSampler);
    }
}

static void METAL_INTERNAL_DestroyBufferContainer(
    MetalBufferContainer *container)
{
    for (Uint32 i = 0; i < container->bufferCount; i += 1) {
        container->buffers[i]->handle = nil;
        SDL_free(container->buffers[i]);
    }
    if (container->debugName != NULL) {
        SDL_free(container->debugName);
    }
    SDL_free(container->buffers);
    SDL_free(container);
}

static void METAL_ReleaseBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer)
{
    MetalRenderer *renderer = (MetalRenderer *)driverData;
    MetalBufferContainer *container = (MetalBufferContainer *)buffer;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->bufferContainersToDestroy,
        MetalBufferContainer *,
        renderer->bufferContainersToDestroyCount + 1,
        renderer->bufferContainersToDestroyCapacity,
        renderer->bufferContainersToDestroyCapacity + 1);

    renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount] = container;
    renderer->bufferContainersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void METAL_ReleaseTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transfer_buffer)
{
    METAL_ReleaseBuffer(
        driverData,
        (SDL_GPUBuffer *)transfer_buffer);
}

static void METAL_ReleaseShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader)
{
    @autoreleasepool {
        MetalShader *metalShader = (MetalShader *)shader;
        metalShader->function = nil;
        metalShader->library = nil;
        SDL_free(metalShader);
    }
}

static void METAL_ReleaseComputePipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUComputePipeline *compute_pipeline)
{
    @autoreleasepool {
        MetalComputePipeline *metalComputePipeline = (MetalComputePipeline *)compute_pipeline;
        metalComputePipeline->handle = nil;
        SDL_free(metalComputePipeline);
    }
}

static void METAL_ReleaseGraphicsPipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUGraphicsPipeline *graphics_pipeline)
{
    @autoreleasepool {
        MetalGraphicsPipeline *metalGraphicsPipeline = (MetalGraphicsPipeline *)graphics_pipeline;
        metalGraphicsPipeline->handle = nil;
        metalGraphicsPipeline->depth_stencil_state = nil;
        SDL_free(metalGraphicsPipeline);
    }
}

// Pipeline Creation

static SDL_GPUComputePipeline *METAL_CreateComputePipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUComputePipelineCreateInfo *createinfo)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalLibraryFunction libraryFunction;
        id<MTLComputePipelineState> handle;
        MetalComputePipeline *pipeline;
        NSError *error;

        libraryFunction = METAL_INTERNAL_CompileShader(
            renderer,
            createinfo->format,
            createinfo->code,
            createinfo->code_size,
            createinfo->entrypoint_name);

        if (libraryFunction.library == nil || libraryFunction.function == nil) {
            return NULL;
        }

        handle = [renderer->device newComputePipelineStateWithFunction:libraryFunction.function error:&error];
        if (error != NULL) {
            SDL_LogError(
                SDL_LOG_CATEGORY_GPU,
                "Creating compute pipeline failed: %s", [[error description] UTF8String]);
            return NULL;
        }

        pipeline = SDL_calloc(1, sizeof(MetalComputePipeline));
        pipeline->handle = handle;
        pipeline->num_readonly_storage_textures = createinfo->num_readonly_storage_textures;
        pipeline->num_writeonly_storage_textures = createinfo->num_writeonly_storage_textures;
        pipeline->num_readonly_storage_buffers = createinfo->num_readonly_storage_buffers;
        pipeline->num_writeonly_storage_buffers = createinfo->num_writeonly_storage_buffers;
        pipeline->num_uniform_buffers = createinfo->num_uniform_buffers;
        pipeline->threadcount_x = createinfo->threadcount_x;
        pipeline->threadcount_y = createinfo->threadcount_y;
        pipeline->threadcount_z = createinfo->threadcount_z;

        return (SDL_GPUComputePipeline *)pipeline;
    }
}

static SDL_GPUGraphicsPipeline *METAL_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUGraphicsPipelineCreateInfo *createinfo)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalShader *vertex_shader = (MetalShader *)createinfo->vertex_shader;
        MetalShader *fragment_shader = (MetalShader *)createinfo->fragment_shader;
        MTLRenderPipelineDescriptor *pipelineDescriptor;
        const SDL_GPUColorAttachmentBlendState *blend_state;
        MTLVertexDescriptor *vertexDescriptor;
        Uint32 binding;
        MTLDepthStencilDescriptor *depthStencilDescriptor;
        MTLStencilDescriptor *frontStencilDescriptor = NULL;
        MTLStencilDescriptor *backStencilDescriptor = NULL;
        id<MTLDepthStencilState> depth_stencil_state = nil;
        id<MTLRenderPipelineState> pipelineState = nil;
        NSError *error = NULL;
        MetalGraphicsPipeline *result = NULL;

        pipelineDescriptor = [MTLRenderPipelineDescriptor new];

        // Blend

        for (Uint32 i = 0; i < createinfo->attachment_info.num_color_attachments; i += 1) {
            blend_state = &createinfo->attachment_info.color_attachment_descriptions[i].blend_state;

            pipelineDescriptor.colorAttachments[i].pixelFormat = SDLToMetal_SurfaceFormat[createinfo->attachment_info.color_attachment_descriptions[i].format];
            pipelineDescriptor.colorAttachments[i].writeMask = SDLToMetal_ColorWriteMask(blend_state->color_write_mask);
            pipelineDescriptor.colorAttachments[i].blendingEnabled = blend_state->enable_blend;
            pipelineDescriptor.colorAttachments[i].rgbBlendOperation = SDLToMetal_BlendOp[blend_state->color_blend_op];
            pipelineDescriptor.colorAttachments[i].alphaBlendOperation = SDLToMetal_BlendOp[blend_state->alpha_blend_op];
            pipelineDescriptor.colorAttachments[i].sourceRGBBlendFactor = SDLToMetal_BlendFactor[blend_state->src_color_blendfactor];
            pipelineDescriptor.colorAttachments[i].sourceAlphaBlendFactor = SDLToMetal_BlendFactor[blend_state->src_alpha_blendfactor];
            pipelineDescriptor.colorAttachments[i].destinationRGBBlendFactor = SDLToMetal_BlendFactor[blend_state->dst_color_blendfactor];
            pipelineDescriptor.colorAttachments[i].destinationAlphaBlendFactor = SDLToMetal_BlendFactor[blend_state->dst_alpha_blendfactor];
        }

        // Multisample

        pipelineDescriptor.rasterSampleCount = SDLToMetal_SampleCount[createinfo->multisample_state.sample_count];

        // Depth Stencil

        if (createinfo->attachment_info.has_depth_stencil_attachment) {
            pipelineDescriptor.depthAttachmentPixelFormat = SDLToMetal_SurfaceFormat[createinfo->attachment_info.depth_stencil_format];

            if (createinfo->depth_stencil_state.enable_stencil_test) {
                pipelineDescriptor.stencilAttachmentPixelFormat = SDLToMetal_SurfaceFormat[createinfo->attachment_info.depth_stencil_format];

                frontStencilDescriptor = [MTLStencilDescriptor new];
                frontStencilDescriptor.stencilCompareFunction = SDLToMetal_CompareOp[createinfo->depth_stencil_state.front_stencil_state.compare_op];
                frontStencilDescriptor.stencilFailureOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.front_stencil_state.fail_op];
                frontStencilDescriptor.depthStencilPassOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.front_stencil_state.pass_op];
                frontStencilDescriptor.depthFailureOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.front_stencil_state.depth_fail_op];
                frontStencilDescriptor.readMask = createinfo->depth_stencil_state.compare_mask;
                frontStencilDescriptor.writeMask = createinfo->depth_stencil_state.write_mask;

                backStencilDescriptor = [MTLStencilDescriptor new];
                backStencilDescriptor.stencilCompareFunction = SDLToMetal_CompareOp[createinfo->depth_stencil_state.back_stencil_state.compare_op];
                backStencilDescriptor.stencilFailureOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.back_stencil_state.fail_op];
                backStencilDescriptor.depthStencilPassOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.back_stencil_state.pass_op];
                backStencilDescriptor.depthFailureOperation = SDLToMetal_StencilOp[createinfo->depth_stencil_state.back_stencil_state.depth_fail_op];
                backStencilDescriptor.readMask = createinfo->depth_stencil_state.compare_mask;
                backStencilDescriptor.writeMask = createinfo->depth_stencil_state.write_mask;
            }

            depthStencilDescriptor = [MTLDepthStencilDescriptor new];
            depthStencilDescriptor.depthCompareFunction = createinfo->depth_stencil_state.enable_depth_test ? SDLToMetal_CompareOp[createinfo->depth_stencil_state.compare_op] : MTLCompareFunctionAlways;
            depthStencilDescriptor.depthWriteEnabled = createinfo->depth_stencil_state.enable_depth_write;
            depthStencilDescriptor.frontFaceStencil = frontStencilDescriptor;
            depthStencilDescriptor.backFaceStencil = backStencilDescriptor;

            depth_stencil_state = [renderer->device newDepthStencilStateWithDescriptor:depthStencilDescriptor];
        }

        // Shaders

        pipelineDescriptor.vertexFunction = vertex_shader->function;
        pipelineDescriptor.fragmentFunction = fragment_shader->function;

        // Vertex Descriptor

        if (createinfo->vertex_input_state.num_vertex_bindings > 0) {
            vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];

            for (Uint32 i = 0; i < createinfo->vertex_input_state.num_vertex_attributes; i += 1) {
                Uint32 loc = createinfo->vertex_input_state.vertex_attributes[i].location;
                vertexDescriptor.attributes[loc].format = SDLToMetal_VertexFormat[createinfo->vertex_input_state.vertex_attributes[i].format];
                vertexDescriptor.attributes[loc].offset = createinfo->vertex_input_state.vertex_attributes[i].offset;
                vertexDescriptor.attributes[loc].bufferIndex = METAL_INTERNAL_GetVertexBufferIndex(createinfo->vertex_input_state.vertex_attributes[i].binding);
            }

            for (Uint32 i = 0; i < createinfo->vertex_input_state.num_vertex_bindings; i += 1) {
                binding = METAL_INTERNAL_GetVertexBufferIndex(createinfo->vertex_input_state.vertex_bindings[i].binding);
                vertexDescriptor.layouts[binding].stepFunction = SDLToMetal_StepFunction[createinfo->vertex_input_state.vertex_bindings[i].input_rate];
                vertexDescriptor.layouts[binding].stepRate = (createinfo->vertex_input_state.vertex_bindings[i].input_rate == SDL_GPU_VERTEXINPUTRATE_INSTANCE) ? createinfo->vertex_input_state.vertex_bindings[i].instance_step_rate : 1;
                vertexDescriptor.layouts[binding].stride = createinfo->vertex_input_state.vertex_bindings[i].pitch;
            }

            pipelineDescriptor.vertexDescriptor = vertexDescriptor;
        }

        // Create the graphics pipeline

        pipelineState = [renderer->device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (error != NULL) {
            SDL_LogError(
                SDL_LOG_CATEGORY_GPU,
                "Creating render pipeline failed: %s", [[error description] UTF8String]);
            return NULL;
        }

        result = SDL_calloc(1, sizeof(MetalGraphicsPipeline));
        result->handle = pipelineState;
        result->sample_mask = createinfo->multisample_state.sample_mask;
        result->depth_stencil_state = depth_stencil_state;
        result->rasterizer_state = createinfo->rasterizer_state;
        result->primitive_type = createinfo->primitive_type;
        result->vertexSamplerCount = vertex_shader->num_samplers;
        result->vertexUniformBufferCount = vertex_shader->num_uniform_buffers;
        result->vertexStorageBufferCount = vertex_shader->num_storage_buffers;
        result->vertexStorageTextureCount = vertex_shader->num_storage_textures;
        result->fragmentSamplerCount = fragment_shader->num_samplers;
        result->fragmentUniformBufferCount = fragment_shader->num_uniform_buffers;
        result->fragmentStorageBufferCount = fragment_shader->num_storage_buffers;
        result->fragmentStorageTextureCount = fragment_shader->num_storage_textures;
        return (SDL_GPUGraphicsPipeline *)result;
    }
}

// Debug Naming

static void METAL_SetBufferName(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer,
    const char *text)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalBufferContainer *container = (MetalBufferContainer *)buffer;
        size_t textLength = SDL_strlen(text) + 1;

        if (renderer->debug_mode) {
            container->debugName = SDL_realloc(
                container->debugName,
                textLength);

            SDL_utf8strlcpy(
                container->debugName,
                text,
                textLength);

            for (Uint32 i = 0; i < container->bufferCount; i += 1) {
                container->buffers[i]->handle.label = @(text);
            }
        }
    }
}

static void METAL_SetTextureName(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture,
    const char *text)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalTextureContainer *container = (MetalTextureContainer *)texture;
        size_t textLength = SDL_strlen(text) + 1;

        if (renderer->debug_mode) {
            container->debugName = SDL_realloc(
                container->debugName,
                textLength);

            SDL_utf8strlcpy(
                container->debugName,
                text,
                textLength);

            for (Uint32 i = 0; i < container->textureCount; i += 1) {
                container->textures[i]->handle.label = @(text);
            }
        }
    }
}

static void METAL_InsertDebugLabel(
    SDL_GPUCommandBuffer *command_buffer,
    const char *text)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        NSString *label = @(text);

        if (metalCommandBuffer->renderEncoder) {
            [metalCommandBuffer->renderEncoder insertDebugSignpost:label];
        } else if (metalCommandBuffer->blitEncoder) {
            [metalCommandBuffer->blitEncoder insertDebugSignpost:label];
        } else if (metalCommandBuffer->computeEncoder) {
            [metalCommandBuffer->computeEncoder insertDebugSignpost:label];
        } else {
            // Metal doesn't have insertDebugSignpost for command buffers...
            [metalCommandBuffer->handle pushDebugGroup:label];
            [metalCommandBuffer->handle popDebugGroup];
        }
    }
}

static void METAL_PushDebugGroup(
    SDL_GPUCommandBuffer *command_buffer,
    const char *name)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        NSString *label = @(name);

        if (metalCommandBuffer->renderEncoder) {
            [metalCommandBuffer->renderEncoder pushDebugGroup:label];
        } else if (metalCommandBuffer->blitEncoder) {
            [metalCommandBuffer->blitEncoder pushDebugGroup:label];
        } else if (metalCommandBuffer->computeEncoder) {
            [metalCommandBuffer->computeEncoder pushDebugGroup:label];
        } else {
            [metalCommandBuffer->handle pushDebugGroup:label];
        }
    }
}

static void METAL_PopDebugGroup(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;

        if (metalCommandBuffer->renderEncoder) {
            [metalCommandBuffer->renderEncoder popDebugGroup];
        } else if (metalCommandBuffer->blitEncoder) {
            [metalCommandBuffer->blitEncoder popDebugGroup];
        } else if (metalCommandBuffer->computeEncoder) {
            [metalCommandBuffer->computeEncoder popDebugGroup];
        } else {
            [metalCommandBuffer->handle popDebugGroup];
        }
    }
}

// Resource Creation

static SDL_GPUSampler *METAL_CreateSampler(
    SDL_GPURenderer *driverData,
    const SDL_GPUSamplerCreateInfo *createinfo)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
        id<MTLSamplerState> sampler;
        MetalSampler *metalSampler;

        samplerDesc.rAddressMode = SDLToMetal_SamplerAddressMode[createinfo->address_mode_u];
        samplerDesc.sAddressMode = SDLToMetal_SamplerAddressMode[createinfo->address_mode_v];
        samplerDesc.tAddressMode = SDLToMetal_SamplerAddressMode[createinfo->address_mode_w];
        samplerDesc.minFilter = SDLToMetal_MinMagFilter[createinfo->min_filter];
        samplerDesc.magFilter = SDLToMetal_MinMagFilter[createinfo->mag_filter];
        samplerDesc.mipFilter = SDLToMetal_MipFilter[createinfo->mipmap_mode]; // FIXME: Is this right with non-mipmapped samplers?
        samplerDesc.lodMinClamp = createinfo->min_lod;
        samplerDesc.lodMaxClamp = createinfo->max_lod;
        samplerDesc.maxAnisotropy = (NSUInteger)((createinfo->enable_anisotropy) ? createinfo->max_anisotropy : 1);
        samplerDesc.compareFunction = (createinfo->enable_compare) ? SDLToMetal_CompareOp[createinfo->compare_op] : MTLCompareFunctionAlways;
        samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack; // arbitrary, unused

        sampler = [renderer->device newSamplerStateWithDescriptor:samplerDesc];
        if (sampler == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create sampler");
            return NULL;
        }

        metalSampler = (MetalSampler *)SDL_calloc(1, sizeof(MetalSampler));
        metalSampler->handle = sampler;
        return (SDL_GPUSampler *)metalSampler;
    }
}

static SDL_GPUShader *METAL_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *createinfo)
{
    @autoreleasepool {
        MetalLibraryFunction libraryFunction;
        MetalShader *result;

        libraryFunction = METAL_INTERNAL_CompileShader(
            (MetalRenderer *)driverData,
            createinfo->format,
            createinfo->code,
            createinfo->code_size,
            createinfo->entrypoint_name);

        if (libraryFunction.library == nil || libraryFunction.function == nil) {
            return NULL;
        }

        result = SDL_calloc(1, sizeof(MetalShader));
        result->library = libraryFunction.library;
        result->function = libraryFunction.function;
        result->num_samplers = createinfo->num_samplers;
        result->num_storage_buffers = createinfo->num_storage_buffers;
        result->num_storage_textures = createinfo->num_storage_textures;
        result->num_uniform_buffers = createinfo->num_uniform_buffers;
        return (SDL_GPUShader *)result;
    }
}

// This function assumes that it's called from within an autorelease pool
static MetalTexture *METAL_INTERNAL_CreateTexture(
    MetalRenderer *renderer,
    const SDL_GPUTextureCreateInfo *createinfo)
{
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];
    id<MTLTexture> texture;
    id<MTLTexture> msaaTexture = NULL;
    MetalTexture *metalTexture;

    textureDescriptor.textureType = SDLToMetal_TextureType[createinfo->type];
    textureDescriptor.pixelFormat = SDLToMetal_SurfaceFormat[createinfo->format];
    // This format isn't natively supported so let's swizzle!
    if (createinfo->format == SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM) {
        textureDescriptor.swizzle = MTLTextureSwizzleChannelsMake(
            MTLTextureSwizzleBlue,
            MTLTextureSwizzleGreen,
            MTLTextureSwizzleRed,
            MTLTextureSwizzleAlpha);
    }

    textureDescriptor.width = createinfo->width;
    textureDescriptor.height = createinfo->height;
    textureDescriptor.depth = (createinfo->type == SDL_GPU_TEXTURETYPE_3D) ? createinfo->layer_count_or_depth : 1;
    textureDescriptor.mipmapLevelCount = createinfo->num_levels;
    textureDescriptor.sampleCount = 1;
    textureDescriptor.arrayLength = (createinfo->type == SDL_GPU_TEXTURETYPE_2D_ARRAY) ? createinfo->layer_count_or_depth : 1;
    textureDescriptor.storageMode = MTLStorageModePrivate;

    textureDescriptor.usage = 0;
    if (createinfo->usage_flags & (SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                   SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        textureDescriptor.usage |= MTLTextureUsageRenderTarget;
    }
    if (createinfo->usage_flags & (SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                   SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ |
                                   SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ)) {
        textureDescriptor.usage |= MTLTextureUsageShaderRead;
    }
    if (createinfo->usage_flags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE) {
        textureDescriptor.usage |= MTLTextureUsageShaderWrite;
    }

    texture = [renderer->device newTextureWithDescriptor:textureDescriptor];
    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create MTLTexture!");
        return NULL;
    }

    // Create the MSAA texture, if needed
    if (createinfo->sample_count > SDL_GPU_SAMPLECOUNT_1 && createinfo->type == SDL_GPU_TEXTURETYPE_2D) {
        textureDescriptor.textureType = MTLTextureType2DMultisample;
        textureDescriptor.sampleCount = SDLToMetal_SampleCount[createinfo->sample_count];
        textureDescriptor.usage = MTLTextureUsageRenderTarget;

        msaaTexture = [renderer->device newTextureWithDescriptor:textureDescriptor];
        if (msaaTexture == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create MSAA MTLTexture!");
            return NULL;
        }
    }

    metalTexture = (MetalTexture *)SDL_calloc(1, sizeof(MetalTexture));
    metalTexture->handle = texture;
    metalTexture->msaaHandle = msaaTexture;
    SDL_AtomicSet(&metalTexture->referenceCount, 0);
    return metalTexture;
}

static bool METAL_SupportsSampleCount(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUSampleCount sample_count)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        NSUInteger mtlSampleCount = SDLToMetal_SampleCount[sample_count];
        return [renderer->device supportsTextureSampleCount:mtlSampleCount];
    }
}

static SDL_GPUTexture *METAL_CreateTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *createinfo)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalTextureContainer *container;
        MetalTexture *texture;

        texture = METAL_INTERNAL_CreateTexture(
            renderer,
            createinfo);

        if (texture == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture!");
            return NULL;
        }

        container = SDL_calloc(1, sizeof(MetalTextureContainer));
        container->canBeCycled = 1;
        container->header.info = *createinfo;
        container->activeTexture = texture;
        container->textureCapacity = 1;
        container->textureCount = 1;
        container->textures = SDL_calloc(
            container->textureCapacity, sizeof(MetalTexture *));
        container->textures[0] = texture;
        container->debugName = NULL;

        return (SDL_GPUTexture *)container;
    }
}

// This function assumes that it's called from within an autorelease pool
static MetalTexture *METAL_INTERNAL_PrepareTextureForWrite(
    MetalRenderer *renderer,
    MetalTextureContainer *container,
    bool cycle)
{
    Uint32 i;

    // Cycle the active texture handle if needed
    if (cycle && container->canBeCycled) {
        for (i = 0; i < container->textureCount; i += 1) {
            if (SDL_AtomicGet(&container->textures[i]->referenceCount) == 0) {
                container->activeTexture = container->textures[i];
                return container->activeTexture;
            }
        }

        EXPAND_ARRAY_IF_NEEDED(
            container->textures,
            MetalTexture *,
            container->textureCount + 1,
            container->textureCapacity,
            container->textureCapacity + 1);

        container->textures[container->textureCount] = METAL_INTERNAL_CreateTexture(
            renderer,
            &container->header.info);
        container->textureCount += 1;

        container->activeTexture = container->textures[container->textureCount - 1];

        if (renderer->debug_mode && container->debugName != NULL) {
            container->activeTexture->handle.label = @(container->debugName);
        }
    }

    return container->activeTexture;
}

// This function assumes that it's called from within an autorelease pool
static MetalBuffer *METAL_INTERNAL_CreateBuffer(
    MetalRenderer *renderer,
    Uint32 size,
    MTLResourceOptions resourceOptions)
{
    id<MTLBuffer> bufferHandle;
    MetalBuffer *metalBuffer;

    // Storage buffers have to be 4-aligned, so might as well align them all
    size = METAL_INTERNAL_NextHighestAlignment(size, 4);

    bufferHandle = [renderer->device newBufferWithLength:size options:resourceOptions];
    if (bufferHandle == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not create buffer");
        return NULL;
    }

    metalBuffer = SDL_calloc(1, sizeof(MetalBuffer));
    metalBuffer->handle = bufferHandle;
    SDL_AtomicSet(&metalBuffer->referenceCount, 0);

    return metalBuffer;
}

// This function assumes that it's called from within an autorelease pool
static MetalBufferContainer *METAL_INTERNAL_CreateBufferContainer(
    MetalRenderer *renderer,
    Uint32 size,
    bool isPrivate,
    bool isWriteOnly)
{
    MetalBufferContainer *container = SDL_calloc(1, sizeof(MetalBufferContainer));
    MTLResourceOptions resourceOptions;

    container->size = size;
    container->bufferCapacity = 1;
    container->bufferCount = 1;
    container->buffers = SDL_calloc(
        container->bufferCapacity, sizeof(MetalBuffer *));
    container->isPrivate = isPrivate;
    container->isWriteOnly = isWriteOnly;
    container->debugName = NULL;

    if (isPrivate) {
        resourceOptions = MTLResourceStorageModePrivate;
    } else {
        if (isWriteOnly) {
            resourceOptions = MTLResourceCPUCacheModeWriteCombined;
        } else {
            resourceOptions = MTLResourceCPUCacheModeDefaultCache;
        }
    }

    container->buffers[0] = METAL_INTERNAL_CreateBuffer(
        renderer,
        size,
        resourceOptions);
    container->activeBuffer = container->buffers[0];

    return container;
}

static SDL_GPUBuffer *METAL_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usage_flags,
    Uint32 size)
{
    @autoreleasepool {
        return (SDL_GPUBuffer *)METAL_INTERNAL_CreateBufferContainer(
            (MetalRenderer *)driverData,
            size,
            true,
            false);
    }
}

static SDL_GPUTransferBuffer *METAL_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage,
    Uint32 size)
{
    @autoreleasepool {
        return (SDL_GPUTransferBuffer *)METAL_INTERNAL_CreateBufferContainer(
            (MetalRenderer *)driverData,
            size,
            false,
            usage == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
    }
}

// This function assumes that it's called from within an autorelease pool
static MetalUniformBuffer *METAL_INTERNAL_CreateUniformBuffer(
    MetalRenderer *renderer,
    Uint32 size)
{
    MetalUniformBuffer *uniformBuffer;
    id<MTLBuffer> bufferHandle;

    bufferHandle = [renderer->device newBufferWithLength:size options:MTLResourceCPUCacheModeWriteCombined];
    if (bufferHandle == nil) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not create uniform buffer");
        return NULL;
    }

    uniformBuffer = SDL_calloc(1, sizeof(MetalUniformBuffer));
    uniformBuffer->handle = bufferHandle;
    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;

    return uniformBuffer;
}

// This function assumes that it's called from within an autorelease pool
static MetalBuffer *METAL_INTERNAL_PrepareBufferForWrite(
    MetalRenderer *renderer,
    MetalBufferContainer *container,
    bool cycle)
{
    MTLResourceOptions resourceOptions;
    Uint32 i;

    // Cycle if needed
    if (cycle && SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0) {
        for (i = 0; i < container->bufferCount; i += 1) {
            if (SDL_AtomicGet(&container->buffers[i]->referenceCount) == 0) {
                container->activeBuffer = container->buffers[i];
                return container->activeBuffer;
            }
        }

        EXPAND_ARRAY_IF_NEEDED(
            container->buffers,
            MetalBuffer *,
            container->bufferCount + 1,
            container->bufferCapacity,
            container->bufferCapacity + 1);

        if (container->isPrivate) {
            resourceOptions = MTLResourceStorageModePrivate;
        } else {
            if (container->isWriteOnly) {
                resourceOptions = MTLResourceCPUCacheModeWriteCombined;
            } else {
                resourceOptions = MTLResourceCPUCacheModeDefaultCache;
            }
        }

        container->buffers[container->bufferCount] = METAL_INTERNAL_CreateBuffer(
            renderer,
            container->size,
            resourceOptions);
        container->bufferCount += 1;

        container->activeBuffer = container->buffers[container->bufferCount - 1];

        if (renderer->debug_mode && container->debugName != NULL) {
            container->activeBuffer->handle.label = @(container->debugName);
        }
    }

    return container->activeBuffer;
}

// TransferBuffer Data

static void *METAL_MapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transfer_buffer,
    bool cycle)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalBufferContainer *container = (MetalBufferContainer *)transfer_buffer;
        MetalBuffer *buffer = METAL_INTERNAL_PrepareBufferForWrite(renderer, container, cycle);
        return [buffer->handle contents];
    }
}

static void METAL_UnmapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transfer_buffer)
{
#ifdef SDL_PLATFORM_MACOS
    @autoreleasepool {
        // FIXME: Is this necessary?
        MetalBufferContainer *container = (MetalBufferContainer *)transfer_buffer;
        MetalBuffer *buffer = container->activeBuffer;
        if (buffer->handle.storageMode == MTLStorageModeManaged) {
            [buffer->handle didModifyRange:NSMakeRange(0, container->size)];
        }
    }
#endif
}

// Copy Pass

static void METAL_BeginCopyPass(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        metalCommandBuffer->blitEncoder = [metalCommandBuffer->handle blitCommandEncoder];
    }
}

static void METAL_UploadToTexture(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUTextureTransferInfo *source,
    const SDL_GPUTextureRegion *destination,
    bool cycle)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MetalBufferContainer *bufferContainer = (MetalBufferContainer *)source->transfer_buffer;
        MetalTextureContainer *textureContainer = (MetalTextureContainer *)destination->texture;

        MetalTexture *metalTexture = METAL_INTERNAL_PrepareTextureForWrite(renderer, textureContainer, cycle);

        [metalCommandBuffer->blitEncoder
                 copyFromBuffer:bufferContainer->activeBuffer->handle
                   sourceOffset:source->offset
              sourceBytesPerRow:BytesPerRow(destination->w, textureContainer->header.info.format)
            sourceBytesPerImage:BytesPerImage(destination->w, destination->h, textureContainer->header.info.format)
                     sourceSize:MTLSizeMake(destination->w, destination->h, destination->d)
                      toTexture:metalTexture->handle
               destinationSlice:destination->layer
               destinationLevel:destination->mip_level
              destinationOrigin:MTLOriginMake(destination->x, destination->y, destination->z)];

        METAL_INTERNAL_TrackTexture(metalCommandBuffer, metalTexture);
        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, bufferContainer->activeBuffer);
    }
}

static void METAL_UploadToBuffer(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUTransferBufferLocation *source,
    const SDL_GPUBufferRegion *destination,
    bool cycle)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MetalBufferContainer *transferContainer = (MetalBufferContainer *)source->transfer_buffer;
        MetalBufferContainer *bufferContainer = (MetalBufferContainer *)destination->buffer;

        MetalBuffer *metalBuffer = METAL_INTERNAL_PrepareBufferForWrite(
            renderer,
            bufferContainer,
            cycle);

        [metalCommandBuffer->blitEncoder
               copyFromBuffer:transferContainer->activeBuffer->handle
                 sourceOffset:source->offset
                     toBuffer:metalBuffer->handle
            destinationOffset:destination->offset
                         size:destination->size];

        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, metalBuffer);
        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, transferContainer->activeBuffer);
    }
}

static void METAL_CopyTextureToTexture(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUTextureLocation *source,
    const SDL_GPUTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    bool cycle)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MetalTextureContainer *srcContainer = (MetalTextureContainer *)source->texture;
        MetalTextureContainer *dstContainer = (MetalTextureContainer *)destination->texture;

        MetalTexture *srcTexture = srcContainer->activeTexture;
        MetalTexture *dstTexture = METAL_INTERNAL_PrepareTextureForWrite(
            renderer,
            dstContainer,
            cycle);

        [metalCommandBuffer->blitEncoder
              copyFromTexture:srcTexture->handle
                  sourceSlice:source->layer
                  sourceLevel:source->mip_level
                 sourceOrigin:MTLOriginMake(source->x, source->y, source->z)
                   sourceSize:MTLSizeMake(w, h, d)
                    toTexture:dstTexture->handle
             destinationSlice:destination->layer
             destinationLevel:destination->mip_level
            destinationOrigin:MTLOriginMake(destination->x, destination->y, destination->z)];

        METAL_INTERNAL_TrackTexture(metalCommandBuffer, srcTexture);
        METAL_INTERNAL_TrackTexture(metalCommandBuffer, dstTexture);
    }
}

static void METAL_CopyBufferToBuffer(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUBufferLocation *source,
    const SDL_GPUBufferLocation *destination,
    Uint32 size,
    bool cycle)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MetalBufferContainer *srcContainer = (MetalBufferContainer *)source->buffer;
        MetalBufferContainer *dstContainer = (MetalBufferContainer *)destination->buffer;

        MetalBuffer *srcBuffer = srcContainer->activeBuffer;
        MetalBuffer *dstBuffer = METAL_INTERNAL_PrepareBufferForWrite(
            renderer,
            dstContainer,
            cycle);

        [metalCommandBuffer->blitEncoder
               copyFromBuffer:srcBuffer->handle
                 sourceOffset:source->offset
                     toBuffer:dstBuffer->handle
            destinationOffset:destination->offset
                         size:size];

        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, srcBuffer);
        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, dstBuffer);
    }
}

static void METAL_DownloadFromTexture(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUTextureRegion *source,
    const SDL_GPUTextureTransferInfo *destination)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MetalTextureContainer *textureContainer = (MetalTextureContainer *)source->texture;
        MetalTexture *metalTexture = textureContainer->activeTexture;
        MetalBufferContainer *bufferContainer = (MetalBufferContainer *)destination->transfer_buffer;
        Uint32 bufferStride = destination->pixels_per_row;
        Uint32 bufferImageHeight = destination->rows_per_layer;
        Uint32 bytesPerRow, bytesPerDepthSlice;

        MetalBuffer *dstBuffer = METAL_INTERNAL_PrepareBufferForWrite(
            renderer,
            bufferContainer,
            false);

        MTLOrigin regionOrigin = MTLOriginMake(
            source->x,
            source->y,
            source->z);

        MTLSize regionSize = MTLSizeMake(
            source->w,
            source->h,
            source->d);

        if (bufferStride == 0 || bufferImageHeight == 0) {
            bufferStride = source->w;
            bufferImageHeight = source->h;
        }

        bytesPerRow = BytesPerRow(bufferStride, textureContainer->header.info.format);
        bytesPerDepthSlice = bytesPerRow * bufferImageHeight;

        [metalCommandBuffer->blitEncoder
                     copyFromTexture:metalTexture->handle
                         sourceSlice:source->layer
                         sourceLevel:source->mip_level
                        sourceOrigin:regionOrigin
                          sourceSize:regionSize
                            toBuffer:dstBuffer->handle
                   destinationOffset:destination->offset
              destinationBytesPerRow:bytesPerRow
            destinationBytesPerImage:bytesPerDepthSlice];

        METAL_INTERNAL_TrackTexture(metalCommandBuffer, metalTexture);
        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, dstBuffer);
    }
}

static void METAL_DownloadFromBuffer(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUBufferRegion *source,
    const SDL_GPUTransferBufferLocation *destination)
{
    SDL_GPUBufferLocation sourceLocation;
    sourceLocation.buffer = source->buffer;
    sourceLocation.offset = source->offset;

    METAL_CopyBufferToBuffer(
        command_buffer,
        &sourceLocation,
        (SDL_GPUBufferLocation *)destination,
        source->size,
        false);
}

static void METAL_EndCopyPass(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        [metalCommandBuffer->blitEncoder endEncoding];
        metalCommandBuffer->blitEncoder = nil;
    }
}

static void METAL_GenerateMipmaps(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUTexture *texture)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalTextureContainer *container = (MetalTextureContainer *)texture;
        MetalTexture *metalTexture = container->activeTexture;

        METAL_BeginCopyPass(command_buffer);
        [metalCommandBuffer->blitEncoder
            generateMipmapsForTexture:metalTexture->handle];
        METAL_EndCopyPass(command_buffer);

        METAL_INTERNAL_TrackTexture(metalCommandBuffer, metalTexture);
    }
}

// Graphics State

static void METAL_INTERNAL_AllocateCommandBuffers(
    MetalRenderer *renderer,
    Uint32 allocateCount)
{
    MetalCommandBuffer *command_buffer;

    renderer->availableCommandBufferCapacity += allocateCount;

    renderer->availableCommandBuffers = SDL_realloc(
        renderer->availableCommandBuffers,
        sizeof(MetalCommandBuffer *) * renderer->availableCommandBufferCapacity);

    for (Uint32 i = 0; i < allocateCount; i += 1) {
        command_buffer = SDL_calloc(1, sizeof(MetalCommandBuffer));
        command_buffer->renderer = renderer;

        // The native Metal command buffer is created in METAL_AcquireCommandBuffer

        command_buffer->windowDataCapacity = 1;
        command_buffer->windowDataCount = 0;
        command_buffer->windowDatas = SDL_calloc(
            command_buffer->windowDataCapacity, sizeof(MetalWindowData *));

        // Reference Counting
        command_buffer->usedBufferCapacity = 4;
        command_buffer->usedBufferCount = 0;
        command_buffer->usedBuffers = SDL_calloc(
            command_buffer->usedBufferCapacity, sizeof(MetalBuffer *));

        command_buffer->usedTextureCapacity = 4;
        command_buffer->usedTextureCount = 0;
        command_buffer->usedTextures = SDL_calloc(
            command_buffer->usedTextureCapacity, sizeof(MetalTexture *));

        renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = command_buffer;
        renderer->availableCommandBufferCount += 1;
    }
}

static MetalCommandBuffer *METAL_INTERNAL_GetInactiveCommandBufferFromPool(
    MetalRenderer *renderer)
{
    MetalCommandBuffer *command_buffer;

    if (renderer->availableCommandBufferCount == 0) {
        METAL_INTERNAL_AllocateCommandBuffers(
            renderer,
            renderer->availableCommandBufferCapacity);
    }

    command_buffer = renderer->availableCommandBuffers[renderer->availableCommandBufferCount - 1];
    renderer->availableCommandBufferCount -= 1;

    return command_buffer;
}

static Uint8 METAL_INTERNAL_CreateFence(
    MetalRenderer *renderer)
{
    MetalFence *fence;

    fence = SDL_calloc(1, sizeof(MetalFence));
    SDL_AtomicSet(&fence->complete, 0);

    // Add it to the available pool
    // FIXME: Should this be EXPAND_IF_NEEDED?
    if (renderer->availableFenceCount >= renderer->availableFenceCapacity) {
        renderer->availableFenceCapacity *= 2;

        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            sizeof(MetalFence *) * renderer->availableFenceCapacity);
    }

    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    return 1;
}

static Uint8 METAL_INTERNAL_AcquireFence(
    MetalRenderer *renderer,
    MetalCommandBuffer *command_buffer)
{
    MetalFence *fence;

    // Acquire a fence from the pool
    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == 0) {
        if (!METAL_INTERNAL_CreateFence(renderer)) {
            SDL_UnlockMutex(renderer->fenceLock);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create fence!");
            return 0;
        }
    }

    fence = renderer->availableFences[renderer->availableFenceCount - 1];
    renderer->availableFenceCount -= 1;

    SDL_UnlockMutex(renderer->fenceLock);

    // Associate the fence with the command buffer
    command_buffer->fence = fence;
    SDL_AtomicSet(&fence->complete, 0); // FIXME: Is this right?

    return 1;
}

static SDL_GPUCommandBuffer *METAL_AcquireCommandBuffer(
    SDL_GPURenderer *driverData)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalCommandBuffer *command_buffer;

        SDL_LockMutex(renderer->acquireCommandBufferLock);

        command_buffer = METAL_INTERNAL_GetInactiveCommandBufferFromPool(renderer);
        command_buffer->handle = [renderer->queue commandBuffer];

        command_buffer->graphics_pipeline = NULL;
        command_buffer->compute_pipeline = NULL;
        for (Uint32 i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
            command_buffer->vertexUniformBuffers[i] = NULL;
            command_buffer->fragmentUniformBuffers[i] = NULL;
            command_buffer->computeUniformBuffers[i] = NULL;
        }

        // FIXME: Do we actually need to set this?
        command_buffer->needVertexSamplerBind = true;
        command_buffer->needVertexStorageTextureBind = true;
        command_buffer->needVertexStorageBufferBind = true;
        command_buffer->needVertexUniformBind = true;
        command_buffer->needFragmentSamplerBind = true;
        command_buffer->needFragmentStorageTextureBind = true;
        command_buffer->needFragmentStorageBufferBind = true;
        command_buffer->needFragmentUniformBind = true;
        command_buffer->needComputeBufferBind = true;
        command_buffer->needComputeTextureBind = true;
        command_buffer->needComputeUniformBind = true;

        METAL_INTERNAL_AcquireFence(renderer, command_buffer);
        command_buffer->autoReleaseFence = 1;

        SDL_UnlockMutex(renderer->acquireCommandBufferLock);

        return (SDL_GPUCommandBuffer *)command_buffer;
    }
}

// This function assumes that it's called from within an autorelease pool
static MetalUniformBuffer *METAL_INTERNAL_AcquireUniformBufferFromPool(
    MetalCommandBuffer *command_buffer)
{
    MetalRenderer *renderer = command_buffer->renderer;
    MetalUniformBuffer *uniformBuffer;

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    if (renderer->uniformBufferPoolCount > 0) {
        uniformBuffer = renderer->uniformBufferPool[renderer->uniformBufferPoolCount - 1];
        renderer->uniformBufferPoolCount -= 1;
    } else {
        uniformBuffer = METAL_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    METAL_INTERNAL_TrackUniformBuffer(command_buffer, uniformBuffer);

    return uniformBuffer;
}

static void METAL_INTERNAL_ReturnUniformBufferToPool(
    MetalRenderer *renderer,
    MetalUniformBuffer *uniformBuffer)
{
    if (renderer->uniformBufferPoolCount >= renderer->uniformBufferPoolCapacity) {
        renderer->uniformBufferPoolCapacity *= 2;
        renderer->uniformBufferPool = SDL_realloc(
            renderer->uniformBufferPool,
            renderer->uniformBufferPoolCapacity * sizeof(MetalUniformBuffer *));
    }

    renderer->uniformBufferPool[renderer->uniformBufferPoolCount] = uniformBuffer;
    renderer->uniformBufferPoolCount += 1;

    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;
}

static void METAL_SetViewport(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUViewport *viewport)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MTLViewport metalViewport;

        metalViewport.originX = viewport->x;
        metalViewport.originY = viewport->y;
        metalViewport.width = viewport->w;
        metalViewport.height = viewport->h;
        metalViewport.znear = viewport->minDepth;
        metalViewport.zfar = viewport->maxDepth;

        [metalCommandBuffer->renderEncoder setViewport:metalViewport];
    }
}

static void METAL_SetScissor(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_Rect *scissor)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MTLScissorRect metalScissor;

        metalScissor.x = scissor->x;
        metalScissor.y = scissor->y;
        metalScissor.width = scissor->w;
        metalScissor.height = scissor->h;

        [metalCommandBuffer->renderEncoder setScissorRect:metalScissor];
    }
}

static void METAL_SetBlendConstants(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_FColor blend_constants)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        [metalCommandBuffer->renderEncoder setBlendColorRed:blend_constants.r
                                                      green:blend_constants.g
                                                       blue:blend_constants.b
                                                      alpha:blend_constants.a];
    }
}

static void METAL_SetStencilReference(
    SDL_GPUCommandBuffer *command_buffer,
    Uint8 reference)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        [metalCommandBuffer->renderEncoder setStencilReferenceValue:reference];
    }
}

static void METAL_BeginRenderPass(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUColorAttachmentInfo *color_attachment_infos,
    Uint32 num_color_attachments,
    const SDL_GPUDepthStencilAttachmentInfo *depth_stencil_attachment_info)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;
        MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        Uint32 vpWidth = UINT_MAX;
        Uint32 vpHeight = UINT_MAX;
        SDL_GPUViewport viewport;
        SDL_Rect scissorRect;

        for (Uint32 i = 0; i < num_color_attachments; i += 1) {
            MetalTextureContainer *container = (MetalTextureContainer *)color_attachment_infos[i].texture;
            MetalTexture *texture = METAL_INTERNAL_PrepareTextureForWrite(
                renderer,
                container,
                color_attachment_infos[i].cycle);

            if (texture->msaaHandle) {
                passDescriptor.colorAttachments[i].texture = texture->msaaHandle;
                passDescriptor.colorAttachments[i].resolveTexture = texture->handle;
            } else {
                passDescriptor.colorAttachments[i].texture = texture->handle;
            }
            passDescriptor.colorAttachments[i].level = color_attachment_infos[i].mip_level;
            if (container->header.info.type == SDL_GPU_TEXTURETYPE_3D) {
                passDescriptor.colorAttachments[i].depthPlane = color_attachment_infos[i].layer_or_depth_plane;
            } else {
                passDescriptor.colorAttachments[i].slice = color_attachment_infos[i].layer_or_depth_plane;
            }
            passDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
                color_attachment_infos[i].clear_color.r,
                color_attachment_infos[i].clear_color.g,
                color_attachment_infos[i].clear_color.b,
                color_attachment_infos[i].clear_color.a);
            passDescriptor.colorAttachments[i].loadAction = SDLToMetal_LoadOp[color_attachment_infos[i].load_op];
            passDescriptor.colorAttachments[i].storeAction = SDLToMetal_StoreOp(
                color_attachment_infos[i].store_op,
                texture->msaaHandle ? 1 : 0);

            METAL_INTERNAL_TrackTexture(metalCommandBuffer, texture);
        }

        if (depth_stencil_attachment_info != NULL) {
            MetalTextureContainer *container = (MetalTextureContainer *)depth_stencil_attachment_info->texture;
            MetalTexture *texture = METAL_INTERNAL_PrepareTextureForWrite(
                renderer,
                container,
                depth_stencil_attachment_info->cycle);

            if (texture->msaaHandle) {
                passDescriptor.depthAttachment.texture = texture->msaaHandle;
                passDescriptor.depthAttachment.resolveTexture = texture->handle;
            } else {
                passDescriptor.depthAttachment.texture = texture->handle;
            }
            passDescriptor.depthAttachment.loadAction = SDLToMetal_LoadOp[depth_stencil_attachment_info->load_op];
            passDescriptor.depthAttachment.storeAction = SDLToMetal_StoreOp(
                depth_stencil_attachment_info->store_op,
                texture->msaaHandle ? 1 : 0);
            passDescriptor.depthAttachment.clearDepth = depth_stencil_attachment_info->clear_value.depth;

            if (IsStencilFormat(container->header.info.format)) {
                if (texture->msaaHandle) {
                    passDescriptor.stencilAttachment.texture = texture->msaaHandle;
                    passDescriptor.stencilAttachment.resolveTexture = texture->handle;
                } else {
                    passDescriptor.stencilAttachment.texture = texture->handle;
                }
                passDescriptor.stencilAttachment.loadAction = SDLToMetal_LoadOp[depth_stencil_attachment_info->load_op];
                passDescriptor.stencilAttachment.storeAction = SDLToMetal_StoreOp(
                    depth_stencil_attachment_info->store_op,
                    texture->msaaHandle ? 1 : 0);
                passDescriptor.stencilAttachment.clearStencil = depth_stencil_attachment_info->clear_value.stencil;
            }

            METAL_INTERNAL_TrackTexture(metalCommandBuffer, texture);
        }

        metalCommandBuffer->renderEncoder = [metalCommandBuffer->handle renderCommandEncoderWithDescriptor:passDescriptor];

        // The viewport cannot be larger than the smallest attachment.
        for (Uint32 i = 0; i < num_color_attachments; i += 1) {
            MetalTextureContainer *container = (MetalTextureContainer *)color_attachment_infos[i].texture;
            Uint32 w = container->header.info.width >> color_attachment_infos[i].mip_level;
            Uint32 h = container->header.info.height >> color_attachment_infos[i].mip_level;

            if (w < vpWidth) {
                vpWidth = w;
            }

            if (h < vpHeight) {
                vpHeight = h;
            }
        }

        if (depth_stencil_attachment_info != NULL) {
            MetalTextureContainer *container = (MetalTextureContainer *)depth_stencil_attachment_info->texture;
            Uint32 w = container->header.info.width;
            Uint32 h = container->header.info.height;

            if (w < vpWidth) {
                vpWidth = w;
            }

            if (h < vpHeight) {
                vpHeight = h;
            }
        }

        // Set sensible default states
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = vpWidth;
        viewport.h = vpHeight;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        METAL_SetViewport(command_buffer, &viewport);

        scissorRect.x = 0;
        scissorRect.y = 0;
        scissorRect.w = vpWidth;
        scissorRect.h = vpHeight;
        METAL_SetScissor(command_buffer, &scissorRect);

        METAL_SetBlendConstants(
            command_buffer,
            (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f });

        METAL_SetStencilReference(
            command_buffer,
            0);
    }
}

static void METAL_BindGraphicsPipeline(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUGraphicsPipeline *graphics_pipeline)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalGraphicsPipeline *metalGraphicsPipeline = (MetalGraphicsPipeline *)graphics_pipeline;
        SDL_GPURasterizerState *rast = &metalGraphicsPipeline->rasterizer_state;

        metalCommandBuffer->graphics_pipeline = metalGraphicsPipeline;

        [metalCommandBuffer->renderEncoder setRenderPipelineState:metalGraphicsPipeline->handle];

        // Apply rasterizer state
        [metalCommandBuffer->renderEncoder setTriangleFillMode:SDLToMetal_PolygonMode[metalGraphicsPipeline->rasterizer_state.fill_mode]];
        [metalCommandBuffer->renderEncoder setCullMode:SDLToMetal_CullMode[metalGraphicsPipeline->rasterizer_state.cull_mode]];
        [metalCommandBuffer->renderEncoder setFrontFacingWinding:SDLToMetal_FrontFace[metalGraphicsPipeline->rasterizer_state.frontFace]];
        [metalCommandBuffer->renderEncoder
            setDepthBias:((rast->enable_depth_bias) ? rast->depth_bias_constant_factor : 0)
              slopeScale:((rast->enable_depth_bias) ? rast->depth_bias_slope_factor : 0)
              clamp:((rast->enable_depth_bias) ? rast->depth_bias_clamp : 0)];

        // Apply depth-stencil state
        if (metalGraphicsPipeline->depth_stencil_state != NULL) {
            [metalCommandBuffer->renderEncoder
                setDepthStencilState:metalGraphicsPipeline->depth_stencil_state];
        }

        for (Uint32 i = 0; i < metalGraphicsPipeline->vertexUniformBufferCount; i += 1) {
            if (metalCommandBuffer->vertexUniformBuffers[i] == NULL) {
                metalCommandBuffer->vertexUniformBuffers[i] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                    metalCommandBuffer);
            }
        }

        for (Uint32 i = 0; i < metalGraphicsPipeline->fragmentUniformBufferCount; i += 1) {
            if (metalCommandBuffer->fragmentUniformBuffers[i] == NULL) {
                metalCommandBuffer->fragmentUniformBuffers[i] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                    metalCommandBuffer);
            }
        }

        metalCommandBuffer->needVertexUniformBind = true;
        metalCommandBuffer->needFragmentUniformBind = true;
    }
}

static void METAL_BindVertexBuffers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_binding,
    const SDL_GPUBufferBinding *bindings,
    Uint32 num_bindings)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        id<MTLBuffer> metalBuffers[MAX_BUFFER_BINDINGS];
        NSUInteger bufferOffsets[MAX_BUFFER_BINDINGS];
        NSRange range = NSMakeRange(METAL_INTERNAL_GetVertexBufferIndex(first_binding), num_bindings);

        if (range.length == 0) {
            return;
        }

        for (Uint32 i = 0; i < range.length; i += 1) {
            MetalBuffer *currentBuffer = ((MetalBufferContainer *)bindings[i].buffer)->activeBuffer;
            NSUInteger bindingIndex = range.length - 1 - i;
            metalBuffers[bindingIndex] = currentBuffer->handle;
            bufferOffsets[bindingIndex] = bindings[i].offset;
            METAL_INTERNAL_TrackBuffer(metalCommandBuffer, currentBuffer);
        }

        [metalCommandBuffer->renderEncoder setVertexBuffers:metalBuffers offsets:bufferOffsets withRange:range];
    }
}

static void METAL_BindIndexBuffer(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUBufferBinding *pBinding,
    SDL_GPUIndexElementSize index_element_size)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    metalCommandBuffer->indexBuffer = ((MetalBufferContainer *)pBinding->buffer)->activeBuffer;
    metalCommandBuffer->indexBufferOffset = pBinding->offset;
    metalCommandBuffer->index_element_size = index_element_size;

    METAL_INTERNAL_TrackBuffer(metalCommandBuffer, metalCommandBuffer->indexBuffer);
}

static void METAL_BindVertexSamplers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalTextureContainer *textureContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        textureContainer = (MetalTextureContainer *)texture_sampler_bindings[i].texture;

        METAL_INTERNAL_TrackTexture(
            metalCommandBuffer,
            textureContainer->activeTexture);

        metalCommandBuffer->vertexSamplers[first_slot + i] =
            ((MetalSampler *)texture_sampler_bindings[i].sampler)->handle;

        metalCommandBuffer->vertexTextures[first_slot + i] =
            textureContainer->activeTexture->handle;
    }

    metalCommandBuffer->needVertexSamplerBind = true;
}

static void METAL_BindVertexStorageTextures(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalTextureContainer *textureContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        textureContainer = (MetalTextureContainer *)storage_textures[i];

        METAL_INTERNAL_TrackTexture(
            metalCommandBuffer,
            textureContainer->activeTexture);

        metalCommandBuffer->vertexStorageTextures[first_slot + i] =
            textureContainer->activeTexture->handle;
    }

    metalCommandBuffer->needVertexStorageTextureBind = true;
}

static void METAL_BindVertexStorageBuffers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalBufferContainer *bufferContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        bufferContainer = (MetalBufferContainer *)storage_buffers[i];

        METAL_INTERNAL_TrackBuffer(
            metalCommandBuffer,
            bufferContainer->activeBuffer);

        metalCommandBuffer->vertexStorageBuffers[first_slot + i] =
            bufferContainer->activeBuffer->handle;
    }

    metalCommandBuffer->needVertexStorageBufferBind = true;
}

static void METAL_BindFragmentSamplers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalTextureContainer *textureContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        textureContainer = (MetalTextureContainer *)texture_sampler_bindings[i].texture;

        METAL_INTERNAL_TrackTexture(
            metalCommandBuffer,
            textureContainer->activeTexture);

        metalCommandBuffer->fragmentSamplers[first_slot + i] =
            ((MetalSampler *)texture_sampler_bindings[i].sampler)->handle;

        metalCommandBuffer->fragmentTextures[first_slot + i] =
            textureContainer->activeTexture->handle;
    }

    metalCommandBuffer->needFragmentSamplerBind = true;
}

static void METAL_BindFragmentStorageTextures(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalTextureContainer *textureContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        textureContainer = (MetalTextureContainer *)storage_textures[i];

        METAL_INTERNAL_TrackTexture(
            metalCommandBuffer,
            textureContainer->activeTexture);

        metalCommandBuffer->fragmentStorageTextures[first_slot + i] =
            textureContainer->activeTexture->handle;
    }

    metalCommandBuffer->needFragmentStorageTextureBind = true;
}

static void METAL_BindFragmentStorageBuffers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalBufferContainer *bufferContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        bufferContainer = (MetalBufferContainer *)storage_buffers[i];

        METAL_INTERNAL_TrackBuffer(
            metalCommandBuffer,
            bufferContainer->activeBuffer);

        metalCommandBuffer->fragmentStorageBuffers[first_slot + i] =
            bufferContainer->activeBuffer->handle;
    }

    metalCommandBuffer->needFragmentStorageBufferBind = true;
}

// This function assumes that it's called from within an autorelease pool
static void METAL_INTERNAL_BindGraphicsResources(
    MetalCommandBuffer *command_buffer)
{
    MetalGraphicsPipeline *graphics_pipeline = command_buffer->graphics_pipeline;
    NSUInteger offsets[MAX_STORAGE_BUFFERS_PER_STAGE] = { 0 };

    // Vertex Samplers+Textures

    if (graphics_pipeline->vertexSamplerCount > 0 && command_buffer->needVertexSamplerBind) {
        [command_buffer->renderEncoder setVertexSamplerStates:command_buffer->vertexSamplers
                                                    withRange:NSMakeRange(0, graphics_pipeline->vertexSamplerCount)];
        [command_buffer->renderEncoder setVertexTextures:command_buffer->vertexTextures
                                               withRange:NSMakeRange(0, graphics_pipeline->vertexSamplerCount)];
        command_buffer->needVertexSamplerBind = false;
    }

    // Vertex Storage Textures

    if (graphics_pipeline->vertexStorageTextureCount > 0 && command_buffer->needVertexStorageTextureBind) {
        [command_buffer->renderEncoder setVertexTextures:command_buffer->vertexStorageTextures
                                               withRange:NSMakeRange(graphics_pipeline->vertexSamplerCount,
                                                                     graphics_pipeline->vertexStorageTextureCount)];
        command_buffer->needVertexStorageTextureBind = false;
    }

    // Vertex Storage Buffers

    if (graphics_pipeline->vertexStorageBufferCount > 0 && command_buffer->needVertexStorageBufferBind) {
        [command_buffer->renderEncoder setVertexBuffers:command_buffer->vertexStorageBuffers
                                                offsets:offsets
                                              withRange:NSMakeRange(graphics_pipeline->vertexUniformBufferCount,
                                                                    graphics_pipeline->vertexStorageBufferCount)];
        command_buffer->needVertexStorageBufferBind = false;
    }

    // Vertex Uniform Buffers

    if (graphics_pipeline->vertexUniformBufferCount > 0 && command_buffer->needVertexUniformBind) {
        for (Uint32 i = 0; i < graphics_pipeline->vertexUniformBufferCount; i += 1) {
            [command_buffer->renderEncoder
                setVertexBuffer:command_buffer->vertexUniformBuffers[i]->handle
                         offset:command_buffer->vertexUniformBuffers[i]->drawOffset
                        atIndex:i];
        }
        command_buffer->needVertexUniformBind = false;
    }

    // Fragment Samplers+Textures

    if (graphics_pipeline->fragmentSamplerCount > 0 && command_buffer->needFragmentSamplerBind) {
        [command_buffer->renderEncoder setFragmentSamplerStates:command_buffer->fragmentSamplers
                                                      withRange:NSMakeRange(0, graphics_pipeline->fragmentSamplerCount)];
        [command_buffer->renderEncoder setFragmentTextures:command_buffer->fragmentTextures
                                                 withRange:NSMakeRange(0, graphics_pipeline->fragmentSamplerCount)];
        command_buffer->needFragmentSamplerBind = false;
    }

    // Fragment Storage Textures

    if (graphics_pipeline->fragmentStorageTextureCount > 0 && command_buffer->needFragmentStorageTextureBind) {
        [command_buffer->renderEncoder setFragmentTextures:command_buffer->fragmentStorageTextures
                                                 withRange:NSMakeRange(graphics_pipeline->fragmentSamplerCount,
                                                                       graphics_pipeline->fragmentStorageTextureCount)];
        command_buffer->needFragmentStorageTextureBind = false;
    }

    // Fragment Storage Buffers

    if (graphics_pipeline->fragmentStorageBufferCount > 0 && command_buffer->needFragmentStorageBufferBind) {
        [command_buffer->renderEncoder setFragmentBuffers:command_buffer->fragmentStorageBuffers
                                                  offsets:offsets
                                                withRange:NSMakeRange(graphics_pipeline->fragmentUniformBufferCount,
                                                                      graphics_pipeline->fragmentStorageBufferCount)];
        command_buffer->needFragmentStorageBufferBind = false;
    }

    // Fragment Uniform Buffers
    if (graphics_pipeline->fragmentUniformBufferCount > 0 && command_buffer->needFragmentUniformBind) {
        for (Uint32 i = 0; i < graphics_pipeline->fragmentUniformBufferCount; i += 1) {
            [command_buffer->renderEncoder
                setFragmentBuffer:command_buffer->fragmentUniformBuffers[i]->handle
                           offset:command_buffer->fragmentUniformBuffers[i]->drawOffset
                          atIndex:i];
        }
        command_buffer->needFragmentUniformBind = false;
    }
}

// This function assumes that it's called from within an autorelease pool
static void METAL_INTERNAL_BindComputeResources(
    MetalCommandBuffer *command_buffer)
{
    MetalComputePipeline *compute_pipeline = command_buffer->compute_pipeline;
    NSUInteger offsets[MAX_STORAGE_BUFFERS_PER_STAGE] = { 0 }; // 8 is the max for both read and write-only

    if (command_buffer->needComputeTextureBind) {
        // Bind read-only textures
        if (compute_pipeline->num_readonly_storage_textures > 0) {
            [command_buffer->computeEncoder setTextures:command_buffer->computeReadOnlyTextures
                                              withRange:NSMakeRange(0, compute_pipeline->num_readonly_storage_textures)];
        }

        // Bind write-only textures
        if (compute_pipeline->num_writeonly_storage_textures > 0) {
            [command_buffer->computeEncoder setTextures:command_buffer->computeWriteOnlyTextures
                                              withRange:NSMakeRange(
                                                            compute_pipeline->num_readonly_storage_textures,
                                                            compute_pipeline->num_writeonly_storage_textures)];
        }
        command_buffer->needComputeTextureBind = false;
    }

    if (command_buffer->needComputeBufferBind) {
        // Bind read-only buffers
        if (compute_pipeline->num_readonly_storage_buffers > 0) {
            [command_buffer->computeEncoder setBuffers:command_buffer->computeReadOnlyBuffers
                                               offsets:offsets
                                             withRange:NSMakeRange(compute_pipeline->num_uniform_buffers,
                                                                   compute_pipeline->num_readonly_storage_buffers)];
        }
        // Bind write-only buffers
        if (compute_pipeline->num_writeonly_storage_buffers > 0) {
            [command_buffer->computeEncoder setBuffers:command_buffer->computeWriteOnlyBuffers
                                               offsets:offsets
                                             withRange:NSMakeRange(
                                                           compute_pipeline->num_uniform_buffers +
                                                               compute_pipeline->num_readonly_storage_buffers,
                                                           compute_pipeline->num_writeonly_storage_buffers)];
        }
        command_buffer->needComputeBufferBind = false;
    }

    if (command_buffer->needComputeUniformBind) {
        for (Uint32 i = 0; i < compute_pipeline->num_uniform_buffers; i += 1) {
            [command_buffer->computeEncoder
                setBuffer:command_buffer->computeUniformBuffers[i]->handle
                   offset:command_buffer->computeUniformBuffers[i]->drawOffset
                  atIndex:i];
        }

        command_buffer->needComputeUniformBind = false;
    }
}

static void METAL_DrawIndexedPrimitives(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 num_indices,
    Uint32 num_instances,
    Uint32 first_index,
    Sint32 vertex_offset,
    Uint32 first_instance)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        SDL_GPUPrimitiveType primitive_type = metalCommandBuffer->graphics_pipeline->primitive_type;
        Uint32 indexSize = IndexSize(metalCommandBuffer->index_element_size);

        METAL_INTERNAL_BindGraphicsResources(metalCommandBuffer);

        [metalCommandBuffer->renderEncoder
            drawIndexedPrimitives:SDLToMetal_PrimitiveType[primitive_type]
                       indexCount:num_indices
                        indexType:SDLToMetal_IndexType[metalCommandBuffer->index_element_size]
                      indexBuffer:metalCommandBuffer->indexBuffer->handle
                indexBufferOffset:metalCommandBuffer->indexBufferOffset + (first_index * indexSize)
                    instanceCount:num_instances
                       baseVertex:vertex_offset
                     baseInstance:first_instance];
    }
}

static void METAL_DrawPrimitives(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 num_vertices,
    Uint32 num_instances,
    Uint32 first_vertex,
    Uint32 first_instance)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        SDL_GPUPrimitiveType primitive_type = metalCommandBuffer->graphics_pipeline->primitive_type;

        METAL_INTERNAL_BindGraphicsResources(metalCommandBuffer);

        [metalCommandBuffer->renderEncoder
            drawPrimitives:SDLToMetal_PrimitiveType[primitive_type]
               vertexStart:first_vertex
               vertexCount:num_vertices
             instanceCount:num_instances
              baseInstance:first_instance];
    }
}

static void METAL_DrawPrimitivesIndirect(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 draw_count,
    Uint32 pitch)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalBuffer *metalBuffer = ((MetalBufferContainer *)buffer)->activeBuffer;
        SDL_GPUPrimitiveType primitive_type = metalCommandBuffer->graphics_pipeline->primitive_type;

        METAL_INTERNAL_BindGraphicsResources(metalCommandBuffer);

        /* Metal: "We have multi-draw at home!"
         * Multi-draw at home:
         */
        for (Uint32 i = 0; i < draw_count; i += 1) {
            [metalCommandBuffer->renderEncoder
                      drawPrimitives:SDLToMetal_PrimitiveType[primitive_type]
                      indirectBuffer:metalBuffer->handle
                indirectBufferOffset:offset + (pitch * i)];
        }

        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, metalBuffer);
    }
}

static void METAL_DrawIndexedPrimitivesIndirect(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 draw_count,
    Uint32 pitch)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalBuffer *metalBuffer = ((MetalBufferContainer *)buffer)->activeBuffer;
        SDL_GPUPrimitiveType primitive_type = metalCommandBuffer->graphics_pipeline->primitive_type;

        METAL_INTERNAL_BindGraphicsResources(metalCommandBuffer);

        for (Uint32 i = 0; i < draw_count; i += 1) {
            [metalCommandBuffer->renderEncoder
                drawIndexedPrimitives:SDLToMetal_PrimitiveType[primitive_type]
                            indexType:SDLToMetal_IndexType[metalCommandBuffer->index_element_size]
                          indexBuffer:metalCommandBuffer->indexBuffer->handle
                    indexBufferOffset:metalCommandBuffer->indexBufferOffset
                       indirectBuffer:metalBuffer->handle
                 indirectBufferOffset:offset + (pitch * i)];
        }

        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, metalBuffer);
    }
}

static void METAL_EndRenderPass(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        [metalCommandBuffer->renderEncoder endEncoding];
        metalCommandBuffer->renderEncoder = nil;

        for (Uint32 i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i += 1) {
            metalCommandBuffer->vertexSamplers[i] = nil;
            metalCommandBuffer->vertexTextures[i] = nil;
            metalCommandBuffer->fragmentSamplers[i] = nil;
            metalCommandBuffer->fragmentTextures[i] = nil;
        }
        for (Uint32 i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
            metalCommandBuffer->vertexStorageTextures[i] = nil;
            metalCommandBuffer->fragmentStorageTextures[i] = nil;
        }
        for (Uint32 i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
            metalCommandBuffer->vertexStorageBuffers[i] = nil;
            metalCommandBuffer->fragmentStorageBuffers[i] = nil;
        }
    }
}

// This function assumes that it's called from within an autorelease pool
static void METAL_INTERNAL_PushUniformData(
    MetalCommandBuffer *metalCommandBuffer,
    SDL_GPUShaderStage shaderStage,
    Uint32 slot_index,
    const void *data,
    Uint32 length)
{
    MetalUniformBuffer *metalUniformBuffer;
    Uint32 alignedDataLength;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        if (metalCommandBuffer->vertexUniformBuffers[slot_index] == NULL) {
            metalCommandBuffer->vertexUniformBuffers[slot_index] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                metalCommandBuffer);
        }
        metalUniformBuffer = metalCommandBuffer->vertexUniformBuffers[slot_index];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        if (metalCommandBuffer->fragmentUniformBuffers[slot_index] == NULL) {
            metalCommandBuffer->fragmentUniformBuffers[slot_index] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                metalCommandBuffer);
        }
        metalUniformBuffer = metalCommandBuffer->fragmentUniformBuffers[slot_index];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        if (metalCommandBuffer->computeUniformBuffers[slot_index] == NULL) {
            metalCommandBuffer->computeUniformBuffers[slot_index] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                metalCommandBuffer);
        }
        metalUniformBuffer = metalCommandBuffer->computeUniformBuffers[slot_index];
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
        return;
    }

    alignedDataLength = METAL_INTERNAL_NextHighestAlignment(
        length,
        256);

    if (metalUniformBuffer->writeOffset + alignedDataLength >= UNIFORM_BUFFER_SIZE) {
        metalUniformBuffer = METAL_INTERNAL_AcquireUniformBufferFromPool(
            metalCommandBuffer);

        metalUniformBuffer->writeOffset = 0;
        metalUniformBuffer->drawOffset = 0;

        if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
            metalCommandBuffer->vertexUniformBuffers[slot_index] = metalUniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
            metalCommandBuffer->fragmentUniformBuffers[slot_index] = metalUniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
            metalCommandBuffer->computeUniformBuffers[slot_index] = metalUniformBuffer;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
            return;
        }
    }

    metalUniformBuffer->drawOffset = metalUniformBuffer->writeOffset;

    SDL_memcpy(
        (metalUniformBuffer->handle).contents + metalUniformBuffer->writeOffset,
        data,
        length);

    metalUniformBuffer->writeOffset += alignedDataLength;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        metalCommandBuffer->needVertexUniformBind = true;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        metalCommandBuffer->needFragmentUniformBind = true;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        metalCommandBuffer->needComputeUniformBind = true;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
    }
}

static void METAL_PushVertexUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length)
{
    @autoreleasepool {
        METAL_INTERNAL_PushUniformData(
            (MetalCommandBuffer *)command_buffer,
            SDL_GPU_SHADERSTAGE_VERTEX,
            slot_index,
            data,
            length);
    }
}

static void METAL_PushFragmentUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length)
{
    @autoreleasepool {
        METAL_INTERNAL_PushUniformData(
            (MetalCommandBuffer *)command_buffer,
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            slot_index,
            data,
            length);
    }
}

// Blit

static void METAL_Blit(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUBlitRegion *source,
    const SDL_GPUBlitRegion *destination,
    SDL_FlipMode flip_mode,
    SDL_GPUFilter filter,
    bool cycle)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalRenderer *renderer = (MetalRenderer *)metalCommandBuffer->renderer;

    SDL_GPU_BlitCommon(
        command_buffer,
        source,
        destination,
        flip_mode,
        filter,
        cycle,
        renderer->blitLinearSampler,
        renderer->blitNearestSampler,
        renderer->blitVertexShader,
        renderer->blitFrom2DShader,
        renderer->blitFrom2DArrayShader,
        renderer->blitFrom3DShader,
        renderer->blitFromCubeShader,
        &renderer->blitPipelines,
        &renderer->blitPipelineCount,
        &renderer->blitPipelineCapacity);
}

// Compute State

static void METAL_BeginComputePass(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUStorageTextureWriteOnlyBinding *storage_texture_bindings,
    Uint32 num_storage_texture_bindings,
    const SDL_GPUStorageBufferWriteOnlyBinding *storage_buffer_bindings,
    Uint32 num_storage_buffer_bindings)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalTextureContainer *textureContainer;
        MetalTexture *texture;
        id<MTLTexture> textureView;
        MetalBufferContainer *bufferContainer;
        MetalBuffer *buffer;

        metalCommandBuffer->computeEncoder = [metalCommandBuffer->handle computeCommandEncoder];

        for (Uint32 i = 0; i < num_storage_texture_bindings; i += 1) {
            textureContainer = (MetalTextureContainer *)storage_texture_bindings[i].texture;

            texture = METAL_INTERNAL_PrepareTextureForWrite(
                metalCommandBuffer->renderer,
                textureContainer,
                storage_texture_bindings[i].cycle);

            METAL_INTERNAL_TrackTexture(metalCommandBuffer, texture);

            textureView = [texture->handle newTextureViewWithPixelFormat:SDLToMetal_SurfaceFormat[textureContainer->header.info.format]
                                                             textureType:SDLToMetal_TextureType[textureContainer->header.info.type]
                                                                  levels:NSMakeRange(storage_texture_bindings[i].mip_level, 1)
                                                                  slices:NSMakeRange(storage_texture_bindings[i].layer, 1)];

            metalCommandBuffer->computeWriteOnlyTextures[i] = textureView;
            metalCommandBuffer->needComputeTextureBind = true;
        }

        for (Uint32 i = 0; i < num_storage_buffer_bindings; i += 1) {
            bufferContainer = (MetalBufferContainer *)storage_buffer_bindings[i].buffer;

            buffer = METAL_INTERNAL_PrepareBufferForWrite(
                metalCommandBuffer->renderer,
                bufferContainer,
                storage_buffer_bindings[i].cycle);

            METAL_INTERNAL_TrackBuffer(
                metalCommandBuffer,
                buffer);

            metalCommandBuffer->computeWriteOnlyBuffers[i] = buffer->handle;
            metalCommandBuffer->needComputeBufferBind = true;
        }
    }
}

static void METAL_BindComputePipeline(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUComputePipeline *compute_pipeline)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalComputePipeline *pipeline = (MetalComputePipeline *)compute_pipeline;

        metalCommandBuffer->compute_pipeline = pipeline;

        [metalCommandBuffer->computeEncoder setComputePipelineState:pipeline->handle];

        for (Uint32 i = 0; i < pipeline->num_uniform_buffers; i += 1) {
            if (metalCommandBuffer->computeUniformBuffers[i] == NULL) {
                metalCommandBuffer->computeUniformBuffers[i] = METAL_INTERNAL_AcquireUniformBufferFromPool(
                    metalCommandBuffer);
            }
        }

        metalCommandBuffer->needComputeUniformBind = true;
    }
}

static void METAL_BindComputeStorageTextures(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalTextureContainer *textureContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        textureContainer = (MetalTextureContainer *)storage_textures[i];

        METAL_INTERNAL_TrackTexture(
            metalCommandBuffer,
            textureContainer->activeTexture);

        metalCommandBuffer->computeReadOnlyTextures[first_slot + i] =
            textureContainer->activeTexture->handle;
    }

    metalCommandBuffer->needComputeTextureBind = true;
}

static void METAL_BindComputeStorageBuffers(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalBufferContainer *bufferContainer;

    for (Uint32 i = 0; i < num_bindings; i += 1) {
        bufferContainer = (MetalBufferContainer *)storage_buffers[i];

        METAL_INTERNAL_TrackBuffer(
            metalCommandBuffer,
            bufferContainer->activeBuffer);

        metalCommandBuffer->computeReadOnlyBuffers[first_slot + i] =
            bufferContainer->activeBuffer->handle;
    }

    metalCommandBuffer->needComputeBufferBind = true;
}

static void METAL_PushComputeUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length)
{
    @autoreleasepool {
        METAL_INTERNAL_PushUniformData(
            (MetalCommandBuffer *)command_buffer,
            SDL_GPU_SHADERSTAGE_COMPUTE,
            slot_index,
            data,
            length);
    }
}

static void METAL_DispatchCompute(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 groupcount_x,
    Uint32 groupcount_y,
    Uint32 groupcount_z)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MTLSize threadgroups = MTLSizeMake(groupcount_x, groupcount_y, groupcount_z);
        MTLSize threadsPerThreadgroup = MTLSizeMake(
            metalCommandBuffer->compute_pipeline->threadcount_x,
            metalCommandBuffer->compute_pipeline->threadcount_y,
            metalCommandBuffer->compute_pipeline->threadcount_z);

        METAL_INTERNAL_BindComputeResources(metalCommandBuffer);

        [metalCommandBuffer->computeEncoder
             dispatchThreadgroups:threadgroups
            threadsPerThreadgroup:threadsPerThreadgroup];
    }
}

static void METAL_DispatchComputeIndirect(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUBuffer *buffer,
    Uint32 offset)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalBuffer *metalBuffer = ((MetalBufferContainer *)buffer)->activeBuffer;
        MTLSize threadsPerThreadgroup = MTLSizeMake(
            metalCommandBuffer->compute_pipeline->threadcount_x,
            metalCommandBuffer->compute_pipeline->threadcount_y,
            metalCommandBuffer->compute_pipeline->threadcount_z);

        METAL_INTERNAL_BindComputeResources(metalCommandBuffer);

        [metalCommandBuffer->computeEncoder
            dispatchThreadgroupsWithIndirectBuffer:metalBuffer->handle
                              indirectBufferOffset:offset
                             threadsPerThreadgroup:threadsPerThreadgroup];

        METAL_INTERNAL_TrackBuffer(metalCommandBuffer, metalBuffer);
    }
}

static void METAL_EndComputePass(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        [metalCommandBuffer->computeEncoder endEncoding];
        metalCommandBuffer->computeEncoder = nil;

        for (Uint32 i = 0; i < MAX_COMPUTE_WRITE_TEXTURES; i += 1) {
            metalCommandBuffer->computeWriteOnlyTextures[i] = nil;
        }
        for (Uint32 i = 0; i < MAX_COMPUTE_WRITE_BUFFERS; i += 1) {
            metalCommandBuffer->computeWriteOnlyBuffers[i] = nil;
        }
        for (Uint32 i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
            metalCommandBuffer->computeReadOnlyTextures[i] = nil;
        }
        for (Uint32 i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
            metalCommandBuffer->computeReadOnlyBuffers[i] = nil;
        }
    }
}

// Fence Cleanup

static void METAL_INTERNAL_ReleaseFenceToPool(
    MetalRenderer *renderer,
    MetalFence *fence)
{
    SDL_LockMutex(renderer->fenceLock);

    // FIXME: Should this use EXPAND_IF_NEEDED?
    if (renderer->availableFenceCount == renderer->availableFenceCapacity) {
        renderer->availableFenceCapacity *= 2;
        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            renderer->availableFenceCapacity * sizeof(MetalFence *));
    }
    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    SDL_UnlockMutex(renderer->fenceLock);
}

static void METAL_ReleaseFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    METAL_INTERNAL_ReleaseFenceToPool(
        (MetalRenderer *)driverData,
        (MetalFence *)fence);
}

// Cleanup

static void METAL_INTERNAL_CleanCommandBuffer(
    MetalRenderer *renderer,
    MetalCommandBuffer *command_buffer)
{
    Uint32 i;

    // Reference Counting
    for (i = 0; i < command_buffer->usedBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&command_buffer->usedBuffers[i]->referenceCount);
    }
    command_buffer->usedBufferCount = 0;

    for (i = 0; i < command_buffer->usedTextureCount; i += 1) {
        (void)SDL_AtomicDecRef(&command_buffer->usedTextures[i]->referenceCount);
    }
    command_buffer->usedTextureCount = 0;

    // Uniform buffers are now available

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (i = 0; i < command_buffer->usedUniformBufferCount; i += 1) {
        METAL_INTERNAL_ReturnUniformBufferToPool(
            renderer,
            command_buffer->usedUniformBuffers[i]);
    }
    command_buffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    // Reset presentation
    command_buffer->windowDataCount = 0;

    // Reset bindings
    command_buffer->indexBuffer = NULL;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i += 1) {
        command_buffer->vertexSamplers[i] = nil;
        command_buffer->vertexTextures[i] = nil;
        command_buffer->fragmentSamplers[i] = nil;
        command_buffer->fragmentTextures[i] = nil;
    }
    for (i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
        command_buffer->vertexStorageTextures[i] = nil;
        command_buffer->fragmentStorageTextures[i] = nil;
        command_buffer->computeReadOnlyTextures[i] = nil;
    }
    for (i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
        command_buffer->vertexStorageBuffers[i] = nil;
        command_buffer->fragmentStorageBuffers[i] = nil;
        command_buffer->computeReadOnlyBuffers[i] = nil;
    }
    for (i = 0; i < MAX_COMPUTE_WRITE_TEXTURES; i += 1) {
        command_buffer->computeWriteOnlyTextures[i] = nil;
    }
    for (i = 0; i < MAX_COMPUTE_WRITE_BUFFERS; i += 1) {
        command_buffer->computeWriteOnlyBuffers[i] = nil;
    }

    // The fence is now available (unless SubmitAndAcquireFence was called)
    if (command_buffer->autoReleaseFence) {
        METAL_ReleaseFence(
            (SDL_GPURenderer *)renderer,
            (SDL_GPUFence *)command_buffer->fence);
    }

    // Return command buffer to pool
    SDL_LockMutex(renderer->acquireCommandBufferLock);
    // FIXME: Should this use EXPAND_IF_NEEDED?
    if (renderer->availableCommandBufferCount == renderer->availableCommandBufferCapacity) {
        renderer->availableCommandBufferCapacity += 1;
        renderer->availableCommandBuffers = SDL_realloc(
            renderer->availableCommandBuffers,
            renderer->availableCommandBufferCapacity * sizeof(MetalCommandBuffer *));
    }
    renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = command_buffer;
    renderer->availableCommandBufferCount += 1;
    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    // Remove this command buffer from the submitted list
    for (i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        if (renderer->submittedCommandBuffers[i] == command_buffer) {
            renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
            renderer->submittedCommandBufferCount -= 1;
        }
    }
}

// This function assumes that it's called from within an autorelease pool
static void METAL_INTERNAL_PerformPendingDestroys(
    MetalRenderer *renderer)
{
    Sint32 referenceCount = 0;
    Sint32 i;
    Uint32 j;

    for (i = renderer->bufferContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->bufferContainersToDestroy[i]->bufferCount; j += 1) {
            referenceCount += SDL_AtomicGet(&renderer->bufferContainersToDestroy[i]->buffers[j]->referenceCount);
        }

        if (referenceCount == 0) {
            METAL_INTERNAL_DestroyBufferContainer(
                renderer->bufferContainersToDestroy[i]);

            renderer->bufferContainersToDestroy[i] = renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount - 1];
            renderer->bufferContainersToDestroyCount -= 1;
        }
    }

    for (i = renderer->textureContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->textureContainersToDestroy[i]->textureCount; j += 1) {
            referenceCount += SDL_AtomicGet(&renderer->textureContainersToDestroy[i]->textures[j]->referenceCount);
        }

        if (referenceCount == 0) {
            METAL_INTERNAL_DestroyTextureContainer(
                renderer->textureContainersToDestroy[i]);

            renderer->textureContainersToDestroy[i] = renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount - 1];
            renderer->textureContainersToDestroyCount -= 1;
        }
    }
}

// Fences

static void METAL_WaitForFences(
    SDL_GPURenderer *driverData,
    bool wait_all,
    SDL_GPUFence *const *fences,
    Uint32 num_fences)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        bool waiting;

        if (wait_all) {
            for (Uint32 i = 0; i < num_fences; i += 1) {
                while (!SDL_AtomicGet(&((MetalFence *)fences[i])->complete)) {
                    // Spin!
                }
            }
        } else {
            waiting = 1;
            while (waiting) {
                for (Uint32 i = 0; i < num_fences; i += 1) {
                    if (SDL_AtomicGet(&((MetalFence *)fences[i])->complete) > 0) {
                        waiting = 0;
                        break;
                    }
                }
            }
        }

        METAL_INTERNAL_PerformPendingDestroys(renderer);
    }
}

static bool METAL_QueryFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    MetalFence *metalFence = (MetalFence *)fence;
    return SDL_AtomicGet(&metalFence->complete) == 1;
}

// Window and Swapchain Management

static MetalWindowData *METAL_INTERNAL_FetchWindowData(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (MetalWindowData *)SDL_GetPointerProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static bool METAL_SupportsSwapchainComposition(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchain_composition)
{
#ifndef SDL_PLATFORM_MACOS
    if (swapchain_composition == SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048) {
        return false;
    }
#endif

    if (@available(macOS 11.0, *)) {
        return true;
    } else {
        return swapchain_composition != SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048;
    }
}

// This function assumes that it's called from within an autorelease pool
static Uint8 METAL_INTERNAL_CreateSwapchain(
    MetalRenderer *renderer,
    MetalWindowData *windowData,
    SDL_GPUSwapchainComposition swapchain_composition,
    SDL_GPUPresentMode present_mode)
{
    CGColorSpaceRef colorspace;
    CGSize drawableSize;

    windowData->view = SDL_Metal_CreateView(windowData->window);
    windowData->drawable = nil;

    windowData->layer = (__bridge CAMetalLayer *)(SDL_Metal_GetLayer(windowData->view));
    windowData->layer.device = renderer->device;
#ifdef SDL_PLATFORM_MACOS
    windowData->layer.displaySyncEnabled = (present_mode != SDL_GPU_PRESENTMODE_IMMEDIATE);
#endif
    windowData->layer.pixelFormat = SDLToMetal_SurfaceFormat[SwapchainCompositionToFormat[swapchain_composition]];
#ifndef SDL_PLATFORM_TVOS
    windowData->layer.wantsExtendedDynamicRangeContent = (swapchain_composition != SDL_GPU_SWAPCHAINCOMPOSITION_SDR);
#endif

    colorspace = CGColorSpaceCreateWithName(SwapchainCompositionToColorSpace[swapchain_composition]);
    windowData->layer.colorspace = colorspace;
    CGColorSpaceRelease(colorspace);

    windowData->texture.handle = nil; // This will be set in AcquireSwapchainTexture.

    // Precache blit pipelines for the swapchain format
    for (Uint32 i = 0; i < 4; i += 1) {
        SDL_GPU_FetchBlitPipeline(
            renderer->sdlGPUDevice,
            (SDL_GPUTextureType)i,
            SwapchainCompositionToFormat[swapchain_composition],
            renderer->blitVertexShader,
            renderer->blitFrom2DShader,
            renderer->blitFrom2DArrayShader,
            renderer->blitFrom3DShader,
            renderer->blitFromCubeShader,
            &renderer->blitPipelines,
            &renderer->blitPipelineCount,
            &renderer->blitPipelineCapacity);
    }

    // Set up the texture container
    SDL_zero(windowData->textureContainer);
    windowData->textureContainer.canBeCycled = 0;
    windowData->textureContainer.activeTexture = &windowData->texture;
    windowData->textureContainer.textureCapacity = 1;
    windowData->textureContainer.textureCount = 1;
    windowData->textureContainer.header.info.format = SwapchainCompositionToFormat[swapchain_composition];
    windowData->textureContainer.header.info.num_levels = 1;
    windowData->textureContainer.header.info.layer_count_or_depth = 1;
    windowData->textureContainer.header.info.type = SDL_GPU_TEXTURETYPE_2D;
    windowData->textureContainer.header.info.usage_flags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    drawableSize = windowData->layer.drawableSize;
    windowData->textureContainer.header.info.width = (Uint32)drawableSize.width;
    windowData->textureContainer.header.info.height = (Uint32)drawableSize.height;

    return 1;
}

static bool METAL_SupportsPresentMode(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUPresentMode present_mode)
{
    switch (present_mode) {
#ifdef SDL_PLATFORM_MACOS
    case SDL_GPU_PRESENTMODE_IMMEDIATE:
#endif
    case SDL_GPU_PRESENTMODE_VSYNC:
        return true;
    default:
        return false;
    }
}

static bool METAL_ClaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(window);

        if (windowData == NULL) {
            windowData = (MetalWindowData *)SDL_calloc(1, sizeof(MetalWindowData));
            windowData->window = window;

            if (METAL_INTERNAL_CreateSwapchain(renderer, windowData, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
                SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

                SDL_LockMutex(renderer->windowLock);

                if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                    renderer->claimedWindowCapacity *= 2;
                    renderer->claimedWindows = SDL_realloc(
                        renderer->claimedWindows,
                        renderer->claimedWindowCapacity * sizeof(MetalWindowData *));
                }
                renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
                renderer->claimedWindowCount += 1;

                SDL_UnlockMutex(renderer->windowLock);

                return true;
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not create swapchain, failed to claim window!");
                SDL_free(windowData);
                return false;
            }
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Window already claimed!");
            return false;
        }
    }
}

static void METAL_ReleaseWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(window);

        if (windowData == NULL) {
            return;
        }

        METAL_Wait(driverData);
        SDL_Metal_DestroyView(windowData->view);

        SDL_LockMutex(renderer->windowLock);
        for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1) {
            if (renderer->claimedWindows[i]->window == window) {
                renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
                renderer->claimedWindowCount -= 1;
                break;
            }
        }
        SDL_UnlockMutex(renderer->windowLock);

        SDL_free(windowData);

        SDL_ClearProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA);
    }
}

static SDL_GPUTexture *METAL_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_Window *window,
    Uint32 *w,
    Uint32 *h)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalWindowData *windowData;
        CGSize drawableSize;

        windowData = METAL_INTERNAL_FetchWindowData(window);
        if (windowData == NULL) {
            return NULL;
        }

        // Get the drawable and its underlying texture
        windowData->drawable = [windowData->layer nextDrawable];
        windowData->texture.handle = [windowData->drawable texture];

        // Update the window size
        drawableSize = windowData->layer.drawableSize;
        windowData->textureContainer.header.info.width = (Uint32)drawableSize.width;
        windowData->textureContainer.header.info.height = (Uint32)drawableSize.height;

        // Send the dimensions to the out parameters.
        *w = (Uint32)drawableSize.width;
        *h = (Uint32)drawableSize.height;

        // Set up presentation
        if (metalCommandBuffer->windowDataCount == metalCommandBuffer->windowDataCapacity) {
            metalCommandBuffer->windowDataCapacity += 1;
            metalCommandBuffer->windowDatas = SDL_realloc(
                metalCommandBuffer->windowDatas,
                metalCommandBuffer->windowDataCapacity * sizeof(MetalWindowData *));
        }
        metalCommandBuffer->windowDatas[metalCommandBuffer->windowDataCount] = windowData;
        metalCommandBuffer->windowDataCount += 1;

        // Return the swapchain texture
        return (SDL_GPUTexture *)&windowData->textureContainer;
    }
}

static SDL_GPUTextureFormat METAL_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Cannot get swapchain format, window has not been claimed!");
        return 0;
    }

    return windowData->textureContainer.header.info.format;
}

static bool METAL_SetSwapchainParameters(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchain_composition,
    SDL_GPUPresentMode present_mode)
{
    @autoreleasepool {
        MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(window);
        CGColorSpaceRef colorspace;

        if (windowData == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Cannot set swapchain parameters, window has not been claimed!");
            return false;
        }

        if (!METAL_SupportsSwapchainComposition(driverData, window, swapchain_composition)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Swapchain composition not supported!");
            return false;
        }

        if (!METAL_SupportsPresentMode(driverData, window, present_mode)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Present mode not supported!");
            return false;
        }

        METAL_Wait(driverData);

#ifdef SDL_PLATFORM_MACOS
        windowData->layer.displaySyncEnabled = (present_mode != SDL_GPU_PRESENTMODE_IMMEDIATE);
#endif
        windowData->layer.pixelFormat = SDLToMetal_SurfaceFormat[SwapchainCompositionToFormat[swapchain_composition]];
#ifndef SDL_PLATFORM_TVOS
        windowData->layer.wantsExtendedDynamicRangeContent = (swapchain_composition != SDL_GPU_SWAPCHAINCOMPOSITION_SDR);
#endif

        colorspace = CGColorSpaceCreateWithName(SwapchainCompositionToColorSpace[swapchain_composition]);
        windowData->layer.colorspace = colorspace;
        CGColorSpaceRelease(colorspace);

        windowData->textureContainer.header.info.format = SwapchainCompositionToFormat[swapchain_composition];

        return true;
    }
}

// Submission

static void METAL_Submit(
    SDL_GPUCommandBuffer *command_buffer)
{
    @autoreleasepool {
        MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
        MetalRenderer *renderer = metalCommandBuffer->renderer;

        SDL_LockMutex(renderer->submitLock);

        // Enqueue present requests, if applicable
        for (Uint32 i = 0; i < metalCommandBuffer->windowDataCount; i += 1) {
            [metalCommandBuffer->handle presentDrawable:metalCommandBuffer->windowDatas[i]->drawable];
            metalCommandBuffer->windowDatas[i]->drawable = nil;
        }

        // Notify the fence when the command buffer has completed
        [metalCommandBuffer->handle addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
          SDL_AtomicIncRef(&metalCommandBuffer->fence->complete);
        }];

        // Submit the command buffer
        [metalCommandBuffer->handle commit];
        metalCommandBuffer->handle = nil;

        // Mark the command buffer as submitted
        if (renderer->submittedCommandBufferCount >= renderer->submittedCommandBufferCapacity) {
            renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

            renderer->submittedCommandBuffers = SDL_realloc(
                renderer->submittedCommandBuffers,
                sizeof(MetalCommandBuffer *) * renderer->submittedCommandBufferCapacity);
        }
        renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = metalCommandBuffer;
        renderer->submittedCommandBufferCount += 1;

        // Check if we can perform any cleanups
        for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
            if (SDL_AtomicGet(&renderer->submittedCommandBuffers[i]->fence->complete)) {
                METAL_INTERNAL_CleanCommandBuffer(
                    renderer,
                    renderer->submittedCommandBuffers[i]);
            }
        }

        METAL_INTERNAL_PerformPendingDestroys(renderer);

        SDL_UnlockMutex(renderer->submitLock);
    }
}

static SDL_GPUFence *METAL_SubmitAndAcquireFence(
    SDL_GPUCommandBuffer *command_buffer)
{
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer *)command_buffer;
    MetalFence *fence = metalCommandBuffer->fence;

    metalCommandBuffer->autoReleaseFence = 0;
    METAL_Submit(command_buffer);

    return (SDL_GPUFence *)fence;
}

static void METAL_Wait(
    SDL_GPURenderer *driverData)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;
        MetalCommandBuffer *command_buffer;

        /*
         * Wait for all submitted command buffers to complete.
         * Sort of equivalent to vkDeviceWaitIdle.
         */
        for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
            while (!SDL_AtomicGet(&renderer->submittedCommandBuffers[i]->fence->complete)) {
                // Spin!
            }
        }

        SDL_LockMutex(renderer->submitLock);

        for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
            command_buffer = renderer->submittedCommandBuffers[i];
            METAL_INTERNAL_CleanCommandBuffer(renderer, command_buffer);
        }

        METAL_INTERNAL_PerformPendingDestroys(renderer);

        SDL_UnlockMutex(renderer->submitLock);
    }
}

// Format Info

static bool METAL_SupportsTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUTextureType type,
    SDL_GPUTextureUsageFlags usage)
{
    @autoreleasepool {
        MetalRenderer *renderer = (MetalRenderer *)driverData;

        // Only depth textures can be used as... depth textures
        if ((usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            if (!IsDepthFormat(format)) {
                return false;
            }
        }

        switch (format) {
        // Apple GPU exclusive
        case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
        case SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM:
        case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
            return [renderer->device supportsFamily:MTLGPUFamilyApple1];

        // Requires BC compression support
        case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM:
        case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT:
        case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT:
        case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB:
        case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB:
        case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB:
        case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB:
#ifdef SDL_PLATFORM_MACOS
            if (@available(macOS 11.0, *)) {
                return (
                    [renderer->device supportsBCTextureCompression] &&
                    !(usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET));
            } else {
                return false;
            }
#else
            // FIXME: iOS 16.4+ allows these formats!
            return false;
#endif

        // Requires D24S8 support
        case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
        case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
#ifdef SDL_PLATFORM_MACOS
            return [renderer->device isDepth24Stencil8PixelFormatSupported];
#else
            return false;
#endif

        default:
            return true;
        }
    }
}

// Device Creation

static bool METAL_PrepareDriver(SDL_VideoDevice *_this)
{
    // FIXME: Add a macOS / iOS version check! Maybe support >= 10.14?
    return (_this->Metal_CreateView != NULL);
}

static void METAL_INTERNAL_InitBlitResources(
    MetalRenderer *renderer)
{
    SDL_GPUShaderCreateInfo shaderModuleCreateInfo;
    SDL_GPUSamplerCreateInfo createinfo;

    // Allocate the dynamic blit pipeline list
    renderer->blitPipelineCapacity = 2;
    renderer->blitPipelineCount = 0;
    renderer->blitPipelines = SDL_calloc(
        renderer->blitPipelineCapacity, sizeof(BlitPipelineCacheEntry));

    // Fullscreen vertex shader
    SDL_zero(shaderModuleCreateInfo);
    shaderModuleCreateInfo.code = FullscreenVert_metallib;
    shaderModuleCreateInfo.code_size = FullscreenVert_metallib_len;
    shaderModuleCreateInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shaderModuleCreateInfo.format = SDL_GPU_SHADERFORMAT_METALLIB;
    shaderModuleCreateInfo.entrypoint_name = "FullscreenVert";

    renderer->blitVertexShader = METAL_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderModuleCreateInfo);

    if (renderer->blitVertexShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile vertex shader for blit!");
    }

    // BlitFrom2D fragment shader
    shaderModuleCreateInfo.code = BlitFrom2D_metallib;
    shaderModuleCreateInfo.code_size = BlitFrom2D_metallib_len;
    shaderModuleCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderModuleCreateInfo.entrypoint_name = "BlitFrom2D";
    shaderModuleCreateInfo.num_samplers = 1;
    shaderModuleCreateInfo.num_uniform_buffers = 1;

    renderer->blitFrom2DShader = METAL_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderModuleCreateInfo);

    if (renderer->blitFrom2DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2D fragment shader!");
    }

    // BlitFrom2DArray fragment shader
    shaderModuleCreateInfo.code = BlitFrom2DArray_metallib;
    shaderModuleCreateInfo.code_size = BlitFrom2DArray_metallib_len;
    shaderModuleCreateInfo.entrypoint_name = "BlitFrom2DArray";

    renderer->blitFrom2DArrayShader = METAL_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderModuleCreateInfo);

    if (renderer->blitFrom2DArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2DArray fragment shader!");
    }

    // BlitFrom3D fragment shader
    shaderModuleCreateInfo.code = BlitFrom3D_metallib;
    shaderModuleCreateInfo.code_size = BlitFrom3D_metallib_len;
    shaderModuleCreateInfo.entrypoint_name = "BlitFrom3D";

    renderer->blitFrom3DShader = METAL_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderModuleCreateInfo);

    if (renderer->blitFrom3DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom3D fragment shader!");
    }

    // BlitFromCube fragment shader
    shaderModuleCreateInfo.code = BlitFromCube_metallib;
    shaderModuleCreateInfo.code_size = BlitFromCube_metallib_len;
    shaderModuleCreateInfo.entrypoint_name = "BlitFromCube";

    renderer->blitFromCubeShader = METAL_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderModuleCreateInfo);

    if (renderer->blitFromCubeShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFromCube fragment shader!");
    }

    // Create samplers
    createinfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createinfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createinfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    createinfo.enable_anisotropy = 0;
    createinfo.enable_compare = 0;
    createinfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    createinfo.min_filter = SDL_GPU_FILTER_NEAREST;
    createinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    createinfo.mip_lod_bias = 0.0f;
    createinfo.min_lod = 0;
    createinfo.max_lod = 1000;
    createinfo.max_anisotropy = 1.0f;
    createinfo.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

    renderer->blitNearestSampler = METAL_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &createinfo);

    if (renderer->blitNearestSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit nearest sampler!");
    }

    createinfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    createinfo.min_filter = SDL_GPU_FILTER_LINEAR;
    createinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

    renderer->blitLinearSampler = METAL_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &createinfo);

    if (renderer->blitLinearSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit linear sampler!");
    }
}

static void METAL_INTERNAL_DestroyBlitResources(
    SDL_GPURenderer *driverData)
{
    MetalRenderer *renderer = (MetalRenderer *)driverData;
    METAL_ReleaseSampler(driverData, renderer->blitLinearSampler);
    METAL_ReleaseSampler(driverData, renderer->blitNearestSampler);
    METAL_ReleaseShader(driverData, renderer->blitVertexShader);
    METAL_ReleaseShader(driverData, renderer->blitFrom2DShader);
    METAL_ReleaseShader(driverData, renderer->blitFrom2DArrayShader);
    METAL_ReleaseShader(driverData, renderer->blitFrom3DShader);
    METAL_ReleaseShader(driverData, renderer->blitFromCubeShader);

    for (Uint32 i = 0; i < renderer->blitPipelineCount; i += 1) {
        METAL_ReleaseGraphicsPipeline(driverData, renderer->blitPipelines[i].pipeline);
    }
    SDL_free(renderer->blitPipelines);
}

static SDL_GPUDevice *METAL_CreateDevice(bool debug_mode, bool preferLowPower, SDL_PropertiesID props)
{
    @autoreleasepool {
        MetalRenderer *renderer;

        // Allocate and zero out the renderer
        renderer = (MetalRenderer *)SDL_calloc(1, sizeof(MetalRenderer));

        // Create the Metal device and command queue
#ifdef SDL_PLATFORM_MACOS
        if (preferLowPower) {
            NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
            for (id<MTLDevice> device in devices) {
                if (device.isLowPower) {
                    renderer->device = device;
                    break;
                }
            }
        }
#endif
        if (renderer->device == NULL) {
            renderer->device = MTLCreateSystemDefaultDevice();
        }
        renderer->queue = [renderer->device newCommandQueue];

        // Print driver info
        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SDL_GPU Driver: Metal");
        SDL_LogInfo(
            SDL_LOG_CATEGORY_GPU,
            "Metal Device: %s",
            [renderer->device.name UTF8String]);

        // Remember debug mode
        renderer->debug_mode = debug_mode;

        // Set up colorspace array
        SwapchainCompositionToColorSpace[0] = kCGColorSpaceSRGB;
        SwapchainCompositionToColorSpace[1] = kCGColorSpaceSRGB;
        SwapchainCompositionToColorSpace[2] = kCGColorSpaceExtendedLinearSRGB;
        if (@available(macOS 11.0, *)) {
            SwapchainCompositionToColorSpace[3] = kCGColorSpaceITUR_2100_PQ;
        } else {
            SwapchainCompositionToColorSpace[3] = NULL;
        }

        // Create mutexes
        renderer->submitLock = SDL_CreateMutex();
        renderer->acquireCommandBufferLock = SDL_CreateMutex();
        renderer->acquireUniformBufferLock = SDL_CreateMutex();
        renderer->disposeLock = SDL_CreateMutex();
        renderer->fenceLock = SDL_CreateMutex();
        renderer->windowLock = SDL_CreateMutex();

        // Create command buffer pool
        METAL_INTERNAL_AllocateCommandBuffers(renderer, 2);

        // Create fence pool
        renderer->availableFenceCapacity = 2;
        renderer->availableFences = SDL_calloc(
            renderer->availableFenceCapacity, sizeof(MetalFence *));

        // Create uniform buffer pool
        renderer->uniformBufferPoolCapacity = 32;
        renderer->uniformBufferPoolCount = 32;
        renderer->uniformBufferPool = SDL_calloc(
            renderer->uniformBufferPoolCapacity, sizeof(MetalUniformBuffer *));

        for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
            renderer->uniformBufferPool[i] = METAL_INTERNAL_CreateUniformBuffer(
                renderer,
                UNIFORM_BUFFER_SIZE);
        }

        // Create deferred destroy arrays
        renderer->bufferContainersToDestroyCapacity = 2;
        renderer->bufferContainersToDestroyCount = 0;
        renderer->bufferContainersToDestroy = SDL_calloc(
            renderer->bufferContainersToDestroyCapacity, sizeof(MetalBufferContainer *));

        renderer->textureContainersToDestroyCapacity = 2;
        renderer->textureContainersToDestroyCount = 0;
        renderer->textureContainersToDestroy = SDL_calloc(
            renderer->textureContainersToDestroyCapacity, sizeof(MetalTextureContainer *));

        // Create claimed window list
        renderer->claimedWindowCapacity = 1;
        renderer->claimedWindows = SDL_calloc(
            renderer->claimedWindowCapacity, sizeof(MetalWindowData *));

        // Initialize blit resources
        METAL_INTERNAL_InitBlitResources(renderer);

        SDL_GPUDevice *result = SDL_calloc(1, sizeof(SDL_GPUDevice));
        ASSIGN_DRIVER(METAL)
        result->driverData = (SDL_GPURenderer *)renderer;
        renderer->sdlGPUDevice = result;

        return result;
    }
}

SDL_GPUBootstrap MetalDriver = {
    "metal",
    SDL_GPU_DRIVER_METAL,
    SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
    METAL_PrepareDriver,
    METAL_CreateDevice
};

#endif // SDL_GPU_METAL
