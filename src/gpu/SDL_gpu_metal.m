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

#if SDL_GPU_METAL

#include <Metal/Metal.h>
#include <QuartzCore/CoreAnimation.h>

#include "SDL_gpu_driver.h"

 /* Defines */

#define WINDOW_PROPERTY_DATA "SDL_GpuMetalWindowPropertyData"

#define NOT_IMPLEMENTED SDL_assert(0 && "Not implemented!");

/* Conversions */

static MTLPixelFormat SDLToMetal_SurfaceFormat[] =
{
	MTLPixelFormatRGBA8Unorm,	/* R8G8B8A8 */
	MTLPixelFormatBGRA8Unorm,	/* B8G8R8A8 */
	MTLPixelFormatB5G6R5Unorm,	/* R5G6B5 */ /* FIXME: Swizzle? */
	MTLPixelFormatA1BGR5Unorm,  /* A1R5G5B5 */ /* FIXME: Swizzle? */
	MTLPixelFormatABGR4Unorm,   /* B4G4R4A4 */ /* FIXME: Swizzle? */
	MTLPixelFormatBC1_RGBA,     /* BC1 */
	MTLPixelFormatBC3_RGBA,     /* BC3 */
	MTLPixelFormatInvalid,	    /* BC5 */ /* FIXME: What is this...? */
	MTLPixelFormatRG8Snorm,	    /* R8G8_SNORM */
	MTLPixelFormatRGBA8Snorm,	/* R8G8B8A8_SNORM */
	MTLPixelFormatRGB10A2Unorm,	/* A2R10G10B10 */ /* FIXME: Swizzle? */
	MTLPixelFormatRG16Unorm,	/* R16G16 */
	MTLPixelFormatRGBA16Unorm,	/* R16G16B16A16 */
	MTLPixelFormatR8Unorm,	    /* R8 */
	MTLPixelFormatR32Float,	    /* R32_SFLOAT */
	MTLPixelFormatRG32Float,	/* R32G32_SFLOAT */
	MTLPixelFormatRGBA32Float,	/* R32G32B32A32_SFLOAT */
	MTLPixelFormatR16Float,     /* R16_SFLOAT */
	MTLPixelFormatRG16Float,	/* R16G16_SFLOAT */
	MTLPixelFormatRGBA16Float,	/* R16G16B16A16_SFLOAT */
	MTLPixelFormatDepth16Unorm,	/* D16 */
	MTLPixelFormatDepth32Float,	/* D32 */
	MTLPixelFormatDepth24Unorm_Stencil8,/* D16S8 */ /* FIXME: Uhhhh */
	MTLPixelFormatDepth32Float_Stencil8	/* D32S8 */
};

static MTLVertexFormat SDLToMetal_VertexFormat[] =
{
    MTLVertexFormatUInt,    /* UINT */
	MTLVertexFormatFloat,	/* FLOAT */
    MTLVertexFormatFloat2,	/* VECTOR2 */
    MTLVertexFormatFloat3,	/* VECTOR3 */
    MTLVertexFormatFloat4,	/* VECTOR4 */
	MTLVertexFormatUChar4Normalized,	/* COLOR */
	MTLVertexFormatUChar4,	/* BYTE4 */
	MTLVertexFormatShort2,	/* SHORT2 */
    MTLVertexFormatShort4,	/* SHORT4 */
	MTLVertexFormatShort2Normalized,	/* NORMALIZEDSHORT2 */
    MTLVertexFormatShort4Normalized,	/* NORMALIZEDSHORT4 */
	MTLVertexFormatHalf2,	/* HALFVECTOR2 */
    MTLVertexFormatHalf4,	/* HALFVECTOR4 */
};

#if 0
static MTLIndexType SDLToMetal_IndexType[] =
{
	MTLIndexTypeUInt16,	/* 16BIT */
	MTLIndexTypeUInt32,	/* 32BIT */
};
#endif

static MTLPrimitiveType SDLToMetal_PrimitiveType[] =
{
	MTLPrimitiveTypePoint,	        /* POINTLIST */
	MTLPrimitiveTypeLine,	        /* LINELIST */
	MTLPrimitiveTypeLineStrip,	    /* LINESTRIP */
	MTLPrimitiveTypeTriangle,	    /* TRIANGLELIST */
	MTLPrimitiveTypeTriangleStrip	/* TRIANGLESTRIP */
};

static MTLTriangleFillMode SDLToMetal_PolygonMode[] =
{
	MTLTriangleFillModeFill,	/* FILL */
	MTLTriangleFillModeLines,	/* LINE */
};

static MTLCullMode SDLToMetal_CullMode[] =
{
	MTLCullModeNone,	/* NONE */
	MTLCullModeFront,	/* FRONT */
	MTLCullModeBack,	/* BACK */
};

static MTLWinding SDLToMetal_FrontFace[] =
{
	MTLWindingCounterClockwise,	/* COUNTER_CLOCKWISE */
	MTLWindingClockwise,	/* CLOCKWISE */
};

static MTLBlendFactor SDLToMetal_BlendFactor[] =
{
	MTLBlendFactorZero,	                /* ZERO */
	MTLBlendFactorOne,	                /* ONE */
	MTLBlendFactorSourceColor,	        /* SRC_COLOR */
	MTLBlendFactorOneMinusSourceColor,	/* ONE_MINUS_SRC_COLOR */
	MTLBlendFactorDestinationColor,	    /* DST_COLOR */
	MTLBlendFactorOneMinusDestinationColor,	/* ONE_MINUS_DST_COLOR */
	MTLBlendFactorSourceAlpha,	        /* SRC_ALPHA */
	MTLBlendFactorOneMinusSourceAlpha,	/* ONE_MINUS_SRC_ALPHA */
	MTLBlendFactorDestinationAlpha,	    /* DST_ALPHA */
	MTLBlendFactorOneMinusDestinationAlpha,	/* ONE_MINUS_DST_ALPHA */
	MTLBlendFactorBlendColor,	        /* CONSTANT_COLOR */
	MTLBlendFactorOneMinusBlendColor,	/* ONE_MINUS_CONSTANT_COLOR */
	MTLBlendFactorSourceAlphaSaturated,	/* SRC_ALPHA_SATURATE */
};

static MTLBlendOperation SDLToMetal_BlendOp[] =
{
	MTLBlendOperationAdd,	/* ADD */
	MTLBlendOperationSubtract,	/* SUBTRACT */
	MTLBlendOperationReverseSubtract,	/* REVERSE_SUBTRACT */
	MTLBlendOperationMin,	/* MIN */
	MTLBlendOperationMax,	/* MAX */
};

#if 0
static MTLCompareFunction SDLToMetal_CompareOp[] =
{
	MTLCompareFunctionNever,	    /* NEVER */
	MTLCompareFunctionLess,	        /* LESS */
	MTLCompareFunctionEqual,	    /* EQUAL */
	MTLCompareFunctionLessEqual,	/* LESS_OR_EQUAL */
	MTLCompareFunctionGreater,	    /* GREATER */
	MTLCompareFunctionNotEqual,	    /* NOT_EQUAL */
	MTLCompareFunctionGreaterEqual,	/* GREATER_OR_EQUAL */
	MTLCompareFunctionAlways,	    /* ALWAYS */
};
#endif

#if 0
static MTLStencilOperation SDLToMetal_StencilOp[] =
{
	MTLStencilOperationKeep,	        /* KEEP */
	MTLStencilOperationZero,	        /* ZERO */
	MTLStencilOperationReplace,	        /* REPLACE */
	MTLStencilOperationIncrementClamp,	/* INCREMENT_AND_CLAMP */
	MTLStencilOperationDecrementClamp,	/* DECREMENT_AND_CLAMP */
	MTLStencilOperationInvert,	        /* INVERT */
	MTLStencilOperationIncrementWrap,	/* INCREMENT_AND_WRAP */
	MTLStencilOperationDecrementWrap,	/* DECREMENT_AND_WRAP */
};
#endif

#if 0
static MTLSamplerAddressMode SDLToMetal_SamplerAddressMode[] =
{
	MTLSamplerAddressModeRepeat,	        /* REPEAT */
	MTLSamplerAddressModeMirrorRepeat,	    /* MIRRORED_REPEAT */
	MTLSamplerAddressModeClampToEdge,	    /* CLAMP_TO_EDGE */
	MTLSamplerAddressModeClampToBorderColor,/* CLAMP_TO_BORDER */
};
#endif

#if 0
static MTLSamplerBorderColor SDLToMetal_BorderColor[] =
{
	MTLSamplerBorderColorTransparentBlack,	/* FLOAT_TRANSPARENT_BLACK */
	MTLSamplerBorderColorTransparentBlack,	/* INT_TRANSPARENT_BLACK */
	MTLSamplerBorderColorOpaqueBlack,	/* FLOAT_OPAQUE_BLACK */
	MTLSamplerBorderColorOpaqueBlack,	/* INT_OPAQUE_BLACK */
	MTLSamplerBorderColorOpaqueWhite,	/* FLOAT_OPAQUE_WHITE */
    MTLSamplerBorderColorOpaqueWhite,	/* INT_OPAQUE_WHITE */
};
#endif

static MTLLoadAction SDLToMetal_LoadOp[] =
{
    MTLLoadActionLoad,  /* LOAD */
    MTLLoadActionClear, /* CLEAR */
    MTLLoadActionDontCare,  /* DONT_CARE */
};

static MTLVertexStepFunction SDLToMetal_StepFunction[] =
{
    MTLVertexStepFunctionPerVertex,
    MTLVertexStepFunctionPerInstance,
};

static MTLStoreAction SDLToMetal_StoreOp(
    SDL_GpuStoreOp storeOp,
    Uint8 isMultisample
) {
    if (isMultisample)
    {
        if (storeOp == SDL_GPU_STOREOP_STORE)
        {
            return MTLStoreActionStoreAndMultisampleResolve;
        }
        else
        {
            return MTLStoreActionMultisampleResolve;
        }
    }
    else
    {
        if (storeOp == SDL_GPU_STOREOP_STORE)
        {
            return MTLStoreActionStore;
        }
        else
        {
            return MTLStoreActionDontCare;
        }
    }
};

static MTLColorWriteMask SDLToMetal_ColorWriteMask(
    SDL_GpuColorComponentFlagBits mask
) {
    MTLColorWriteMask result = 0;
    if (mask & SDL_GPU_COLORCOMPONENT_R_BIT)
    {
        result |= MTLColorWriteMaskRed;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_G_BIT)
    {
        result |= MTLColorWriteMaskGreen;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_B_BIT)
    {
        result |= MTLColorWriteMaskBlue;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_A_BIT)
    {
        result |= MTLColorWriteMaskAlpha;
    }
    return result;
}

/* Structs */

typedef struct MetalTexture
{
    id<MTLTexture> handle;

    /* Basic Info */
    /* FIXME: Move this to container! */
    SDL_GpuTextureFormat format;
    Uint32 width;
    Uint32 height;
    Uint32 depth;
    Uint32 levelCount;
    Uint32 layerCount;
    Uint8 isCube;
    Uint8 isRenderTarget;
} MetalTexture;

typedef struct MetalTextureContainer
{
    SDL_GpuTextureCreateInfo createInfo;
    MetalTexture *activeTexture;
    Uint8 canBeCycled;

    Uint32 textureCapacity;
    Uint32 textureCount;
    MetalTexture **textures;

    char *debugName;
} MetalTextureContainer;

typedef struct MetalWindowData
{
    SDL_Window *windowHandle;
    SDL_MetalView view;
    CAMetalLayer *layer;
    id<CAMetalDrawable> drawable;
    MetalTexture texture;
    MetalTextureContainer textureContainer;
} MetalWindowData;

typedef struct MetalShaderModule
{
    id<MTLLibrary> library;
    id<MTLFunction> function;
} MetalShaderModule;

typedef struct MetalGraphicsPipeline
{
    id<MTLRenderPipelineState> handle;
    SDL_GpuPrimitiveType primitiveType;
    float blendConstants[4];
    Uint32 sampleMask;
    SDL_GpuRasterizerState rasterizerState;
} MetalGraphicsPipeline;

typedef struct MetalFence
{
    Uint8 complete;
} MetalFence;

typedef struct MetalCommandBuffer
{
    id<MTLCommandBuffer> handle;
    id<MTLRenderCommandEncoder> renderEncoder;
    MetalWindowData *windowData;
    MetalFence *fence;
    Uint8 autoReleaseFence;
    SDL_GpuPrimitiveType primitiveType;
} MetalCommandBuffer;

typedef struct MetalRenderer
{
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    
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

    SDL_Mutex *submitLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *fenceLock;
    SDL_Mutex *windowLock;
} MetalRenderer;

/* Forward Declarations */

static void METAL_UnclaimWindow(
    SDL_GpuRenderer *driverData,
    void *windowHandle
);

/* Quit */

static void METAL_DestroyDevice(SDL_GpuDevice *device)
{
    MetalRenderer *renderer = (MetalRenderer*) device->driverData;

    /* Flush any remaining GPU work... */
    /* FIXME: METAL_Wait(device->driverData); */

    /* Release the window data */
    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1)
    {
        METAL_UnclaimWindow(device->driverData, renderer->claimedWindows[i]->windowHandle);
    }
    SDL_free(renderer->claimedWindows);

    /* Release command buffer infrastructure */
    for (Uint32 i = 0; i < renderer->availableCommandBufferCount; i += 1)
    {
        MetalCommandBuffer *commandBuffer = renderer->availableCommandBuffers[i];
        commandBuffer->handle = nil;
        /* FIXME: Anything else? */
        SDL_free(commandBuffer);
    }
    SDL_free(renderer->availableCommandBuffers);
    SDL_free(renderer->submittedCommandBuffers);

    /* Release fence infrastructure */
    for (Uint32 i = 0; i < renderer->availableFenceCount; i += 1)
    {
        MetalFence *fence = renderer->availableFences[i];
        /* FIXME: What to do here? */
        SDL_free(fence);
    }
    SDL_free(renderer->availableFences);

    /* Release the mutexes */
    SDL_DestroyMutex(renderer->submitLock);
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->fenceLock);
    SDL_DestroyMutex(renderer->windowLock);

    /* Release the device and associated objects */
    renderer->queue = nil;
    renderer->device = nil;
    
    /* Free the primary structures */
    SDL_free(renderer);
    SDL_free(device);
}

/* State Creation */

static SDL_GpuComputePipeline* METAL_CreateComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuComputeShaderInfo *computeShaderInfo
) {
    NOT_IMPLEMENTED
    return NULL;
}

static SDL_GpuGraphicsPipeline* METAL_CreateGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    MetalShaderModule *vertexShaderModule = (MetalShaderModule*) pipelineCreateInfo->vertexShaderInfo.shaderModule;
    MetalShaderModule *fragmentShaderModule = (MetalShaderModule*) pipelineCreateInfo->fragmentShaderInfo.shaderModule;
    MTLRenderPipelineDescriptor *pipelineDescriptor;
    SDL_GpuColorAttachmentBlendState *blendState;
    MTLVertexDescriptor *vertexDescriptor;
    Uint32 binding;
    id<MTLRenderPipelineState> pipelineState;
    NSError *error = NULL;
    MetalGraphicsPipeline *result = NULL;
    
    pipelineDescriptor = [MTLRenderPipelineDescriptor new];
    
    /* Blend */
    for (Uint32 i = 0; i < pipelineCreateInfo->attachmentInfo.colorAttachmentCount; i += 1)
    {
        blendState = &pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].blendState;
        
        pipelineDescriptor.colorAttachments[i].pixelFormat = SDLToMetal_SurfaceFormat[pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].format];
        pipelineDescriptor.colorAttachments[i].writeMask = SDLToMetal_ColorWriteMask(blendState->colorWriteMask);
        pipelineDescriptor.colorAttachments[i].blendingEnabled = blendState->blendEnable;
        pipelineDescriptor.colorAttachments[i].rgbBlendOperation = SDLToMetal_BlendOp[blendState->colorBlendOp];
        pipelineDescriptor.colorAttachments[i].alphaBlendOperation = SDLToMetal_BlendOp[blendState->alphaBlendOp];
        pipelineDescriptor.colorAttachments[i].sourceRGBBlendFactor = SDLToMetal_BlendFactor[blendState->srcColorBlendFactor];
        pipelineDescriptor.colorAttachments[i].sourceAlphaBlendFactor = SDLToMetal_BlendFactor[blendState->srcAlphaBlendFactor];
        pipelineDescriptor.colorAttachments[i].destinationRGBBlendFactor = SDLToMetal_BlendFactor[blendState->dstColorBlendFactor];
        pipelineDescriptor.colorAttachments[i].destinationAlphaBlendFactor = SDLToMetal_BlendFactor[blendState->dstAlphaBlendFactor];
    }
    
    /* FIXME: Multisample */

    /* FIXME: Depth-Stencil */
    
    /* Vertex Shader */
    pipelineDescriptor.vertexFunction = vertexShaderModule->function;
    
    /* Vertex Descriptor */
    if (pipelineCreateInfo->vertexInputState.vertexBindingCount > 0)
    {
        vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        
        for (Uint32 i = 0; i < pipelineCreateInfo->vertexInputState.vertexAttributeCount; i += 1)
        {
            vertexDescriptor.attributes[i].format = SDLToMetal_VertexFormat[pipelineCreateInfo->vertexInputState.vertexAttributes[i].format];
            vertexDescriptor.attributes[i].offset = SDLToMetal_VertexFormat[pipelineCreateInfo->vertexInputState.vertexAttributes[i].offset];
            vertexDescriptor.attributes[i].bufferIndex = SDLToMetal_VertexFormat[pipelineCreateInfo->vertexInputState.vertexAttributes[i].binding];
        }
        
        for (Uint32 i = 0; i < pipelineCreateInfo->vertexInputState.vertexBindingCount; i += 1)
        {
            binding = pipelineCreateInfo->vertexInputState.vertexBindings[i].binding;
            vertexDescriptor.layouts[binding].stepFunction = SDLToMetal_StepFunction[pipelineCreateInfo->vertexInputState.vertexBindings[i].inputRate];
            vertexDescriptor.layouts[binding].stride = pipelineCreateInfo->vertexInputState.vertexBindings[i].stride;
        }
        
        pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    }
    
    /* Fragment Shader */
    pipelineDescriptor.fragmentFunction = fragmentShaderModule->function;
    
    /* Create the graphics pipeline */
    pipelineState = [renderer->device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error != NULL)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Creating render pipeline failed: %s",
                [[error description] cStringUsingEncoding:[NSString defaultCStringEncoding]]);
        return NULL;
    }
    
    result = SDL_malloc(sizeof(MetalGraphicsPipeline));
    result->handle = pipelineState;
    result->primitiveType = pipelineCreateInfo->primitiveType;
    SDL_memcpy(result->blendConstants, pipelineCreateInfo->blendConstants, sizeof(result->blendConstants));
    result->sampleMask = pipelineCreateInfo->multisampleState.sampleMask;
    result->rasterizerState = pipelineCreateInfo->rasterizerState;
	return (SDL_GpuGraphicsPipeline*) result;
}

static SDL_GpuSampler* METAL_CreateSampler(
	SDL_GpuRenderer *driverData,
	SDL_GpuSamplerStateCreateInfo *samplerStateCreateInfo
) {
	NOT_IMPLEMENTED
	return NULL;
}

static SDL_GpuShaderModule* METAL_CreateShaderModule(
    SDL_GpuRenderer *driverData,
    SDL_GpuShaderModuleCreateInfo *shaderModuleCreateInfo
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    NSString *sourceString = [NSString
        stringWithCString:(const char*) shaderModuleCreateInfo->code
        encoding:[NSString defaultCStringEncoding]];
    id<MTLLibrary> library = nil;
    NSError *error = NULL;
    MetalShaderModule *result = NULL;
    
    library = [renderer->device
               newLibraryWithSource:sourceString
               options:nil /* FIXME: Do we need any compile options? */
               error:&error];
    
    if (error != NULL)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Creating library failed: %s",
                [[error description] cStringUsingEncoding:[NSString defaultCStringEncoding]]);
        return NULL;
    }
    
    result = SDL_malloc(sizeof(MetalShaderModule));
    result->library = library;
    result->function = [library newFunctionWithName:@"main0"];
    return (SDL_GpuShaderModule*) result;
}

static SDL_GpuTexture* METAL_CreateTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuTextureCreateInfo *textureCreateInfo
) {
	NOT_IMPLEMENTED
	return NULL;
}

static SDL_GpuBuffer* METAL_CreateGpuBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuBufferUsageFlags usageFlags,
	Uint32 sizeInBytes
) {
	NOT_IMPLEMENTED
	return NULL;
}

static SDL_GpuTransferBuffer* METAL_CreateTransferBuffer(
	SDL_GpuRenderer *driverData,
    SDL_GpuTransferUsage usage,
    Uint32 sizeInBytes
) {
	NOT_IMPLEMENTED
	return NULL;
}

/* Debug Naming */

static void METAL_SetGpuBufferName(
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *buffer,
    const char *text
) {
    NOT_IMPLEMENTED
}

static void METAL_SetTextureName(
    SDL_GpuRenderer *driverData,
    SDL_GpuTexture *texture,
    const char *text
) {
    NOT_IMPLEMENTED
}

    
/* Disposal */

static void METAL_QueueDestroyTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuTexture *texture
) {
    NOT_IMPLEMENTED
}

static void METAL_QueueDestroySampler(
	SDL_GpuRenderer *driverData,
	SDL_GpuSampler *sampler
) {
	NOT_IMPLEMENTED
}

static void METAL_QueueDestroyGpuBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuBuffer *gpuBuffer
) {
    NOT_IMPLEMENTED
}

static void METAL_QueueDestroyTransferBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuTransferBuffer *transferBuffer
) {
	NOT_IMPLEMENTED
}

static void METAL_QueueDestroyShaderModule(
	SDL_GpuRenderer *driverData,
	SDL_GpuShaderModule *shaderModule
) {
    MetalShaderModule *metalShaderModule = (MetalShaderModule*) shaderModule;
    metalShaderModule->function = nil;
    metalShaderModule->library = nil;
    SDL_free(metalShaderModule);
}

static void METAL_QueueDestroyComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuComputePipeline *computePipeline
) {
	NOT_IMPLEMENTED
}

static void METAL_QueueDestroyGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
    MetalGraphicsPipeline *metalGraphicsPipeline = (MetalGraphicsPipeline*) graphicsPipeline;
    metalGraphicsPipeline->handle = nil;
    SDL_free(metalGraphicsPipeline);
}

/* Render Pass */

static void METAL_BeginRenderPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
	Uint32 colorAttachmentCount,
	SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    SDL_GpuColorAttachmentInfo *attachmentInfo;
    
    for (Uint32 i = 0; i < colorAttachmentCount; i += 1)
    {
        attachmentInfo = &colorAttachmentInfos[i];

        passDescriptor.colorAttachments[i].texture = ((MetalTextureContainer*) attachmentInfo->textureSlice.texture)->activeTexture->handle;
        passDescriptor.colorAttachments[i].level = attachmentInfo->textureSlice.mipLevel;
        passDescriptor.colorAttachments[i].slice = attachmentInfo->textureSlice.layer;
        passDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
            attachmentInfo->clearColor.x,
            attachmentInfo->clearColor.y,
            attachmentInfo->clearColor.z,
            attachmentInfo->clearColor.w
        );
        passDescriptor.colorAttachments[i].loadAction = SDLToMetal_LoadOp[attachmentInfo->loadOp];
        passDescriptor.colorAttachments[i].storeAction = SDLToMetal_StoreOp(attachmentInfo->storeOp, 0);
        /* FIXME: Resolve texture! Also affects ^! */
    }
    
    /* FIXME: depth/stencil */
    
    metalCommandBuffer->renderEncoder = [metalCommandBuffer->handle renderCommandEncoderWithDescriptor:passDescriptor];
}

static void METAL_BindGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    MetalGraphicsPipeline *metalGraphicsPipeline = (MetalGraphicsPipeline*) graphicsPipeline;
    SDL_GpuRasterizerState *rast = &metalGraphicsPipeline->rasterizerState;
    
    [metalCommandBuffer->renderEncoder setRenderPipelineState:metalGraphicsPipeline->handle];
    
    /* Apply rasterizer state */
    [metalCommandBuffer->renderEncoder setTriangleFillMode: SDLToMetal_PolygonMode[metalGraphicsPipeline->rasterizerState.fillMode]];
    [metalCommandBuffer->renderEncoder setCullMode: SDLToMetal_CullMode[metalGraphicsPipeline->rasterizerState.cullMode]];
    [metalCommandBuffer->renderEncoder setFrontFacingWinding: SDLToMetal_FrontFace[metalGraphicsPipeline->rasterizerState.frontFace]];
    [metalCommandBuffer->renderEncoder
        setDepthBias: ((rast->depthBiasEnable) ? rast->depthBiasConstantFactor : 0)
        slopeScale: ((rast->depthBiasEnable) ? rast->depthBiasSlopeFactor : 0)
        clamp: ((rast->depthBiasEnable) ? rast->depthBiasClamp : 0)];
    
    /* Apply blend constants */
    [metalCommandBuffer->renderEncoder
        setBlendColorRed: metalGraphicsPipeline->blendConstants[0]
        green:metalGraphicsPipeline->blendConstants[1]
        blue:metalGraphicsPipeline->blendConstants[2]
        alpha:metalGraphicsPipeline->blendConstants[3]];
    
    /* Remember this for draw calls */
    metalCommandBuffer->primitiveType = metalGraphicsPipeline->primitiveType;
}

static void METAL_SetViewport(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuViewport *viewport
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    MTLViewport metalViewport;

    metalViewport.originX = viewport->x;
    metalViewport.originY = viewport->y;
    metalViewport.width = viewport->w;
    metalViewport.height = viewport->h;
    metalViewport.zfar = viewport->maxDepth;
    metalViewport.znear = viewport->minDepth;
    
    [metalCommandBuffer->renderEncoder setViewport:metalViewport];
}

static void METAL_SetScissor(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuRect *scissor
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    MTLScissorRect metalScissor;

    metalScissor.x = scissor->x;
    metalScissor.y = scissor->y;
    metalScissor.width = scissor->w;
    metalScissor.height = scissor->h;
    
    [metalCommandBuffer->renderEncoder setScissorRect:metalScissor];
}

static void METAL_BindVertexBuffers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 firstBinding,
	Uint32 bindingCount,
	SDL_GpuBufferBinding *pBindings
) {
	NOT_IMPLEMENTED
}

static void METAL_BindIndexBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBufferBinding *pBinding,
	SDL_GpuIndexElementSize indexElementSize
) {
	NOT_IMPLEMENTED
}

static void METAL_BindVertexSamplers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	NOT_IMPLEMENTED
}

static void METAL_BindFragmentSamplers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	NOT_IMPLEMENTED
}

static void METAL_PushVertexShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NOT_IMPLEMENTED
}

static void METAL_PushFragmentShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NOT_IMPLEMENTED
}

static void METAL_DrawInstancedPrimitives(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 baseVertex,
	Uint32 startIndex,
	Uint32 primitiveCount,
	Uint32 instanceCount
) {
    NOT_IMPLEMENTED
}

static void METAL_DrawPrimitives(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 vertexStart,
	Uint32 primitiveCount
) {
    (void) driverData; /* Used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    
    [metalCommandBuffer->renderEncoder
        drawPrimitives:SDLToMetal_PrimitiveType[ metalCommandBuffer->primitiveType]
        vertexStart:vertexStart
        vertexCount:PrimitiveVerts(metalCommandBuffer->primitiveType, primitiveCount)];
}

static void METAL_DrawPrimitivesIndirect(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *gpuBuffer,
	Uint32 offsetInBytes,
	Uint32 drawCount,
	Uint32 stride
) {
    NOT_IMPLEMENTED
}

static void METAL_EndRenderPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    
    [metalCommandBuffer->renderEncoder endEncoding];
    metalCommandBuffer->renderEncoder = nil;
    
    /* FIXME: Anything else to do here? */
}

/* Compute Pass */

static void METAL_BeginComputePass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
    NOT_IMPLEMENTED
}

static void METAL_BindComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputePipeline *computePipeline
) {
	NOT_IMPLEMENTED
}

static void METAL_BindComputeBuffers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeBufferBinding *pBindings
) {
	NOT_IMPLEMENTED
}

static void METAL_BindComputeTextures(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeTextureBinding *pBindings
) {
	NOT_IMPLEMENTED
}

static void METAL_PushComputeShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NOT_IMPLEMENTED
}

static void METAL_DispatchCompute(
	SDL_GpuRenderer *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 groupCountX,
	Uint32 groupCountY,
	Uint32 groupCountZ
) {
	NOT_IMPLEMENTED
}

static void METAL_EndComputePass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
    NOT_IMPLEMENTED
}

/* TransferBuffer Set/Get */

static void METAL_SetTransferData(
	SDL_GpuRenderer *driverData,
	void* data,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	NOT_IMPLEMENTED
}

static void METAL_GetTransferData(
	SDL_GpuRenderer *driverData,
	SDL_GpuTransferBuffer *transferBuffer,
	void* data,
	SDL_GpuBufferCopy *copyParams
) {
	NOT_IMPLEMENTED
}

/* Copy Pass */

static void METAL_BeginCopyPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NOT_IMPLEMENTED
}

static void METAL_UploadToTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuTextureRegion *textureSlice,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTextureWriteOptions writeOption
) {
	NOT_IMPLEMENTED
}

static void METAL_UploadToBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	NOT_IMPLEMENTED
}

static void METAL_DownloadFromTexture(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureRegion *textureRegion,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_GpuBufferImageCopy *copyParams,
    SDL_GpuTransferOptions transferOption
) {
	NOT_IMPLEMENTED
}

static void METAL_DownloadFromBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *gpuBuffer,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_GpuBufferCopy *copyParams,
    SDL_GpuTransferOptions transferOption
) {
	NOT_IMPLEMENTED
}

static void METAL_CopyTextureToTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureRegion *source,
	SDL_GpuTextureRegion *destination,
	SDL_GpuTextureWriteOptions writeOption
) {
	NOT_IMPLEMENTED
}

static void METAL_CopyBufferToBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *source,
	SDL_GpuBuffer *destination,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	NOT_IMPLEMENTED
}

static void METAL_GenerateMipmaps(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTexture *texture
) {
	NOT_IMPLEMENTED
}

static void METAL_EndCopyPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NOT_IMPLEMENTED
}

/* Window and Swapchain Management */

static MetalWindowData* METAL_INTERNAL_FetchWindowData(SDL_Window *windowHandle)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(windowHandle);
    return (MetalWindowData*) SDL_GetProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static Uint8 METAL_INTERNAL_CreateSwapchain(
    MetalRenderer *renderer,
    MetalWindowData *windowData,
    SDL_GpuPresentMode presentMode
) {
    MetalTexture *swapchainTexture = &windowData->texture;
    CGSize drawableSize;

    windowData->view = SDL_Metal_CreateView(windowData->windowHandle);
    windowData->drawable = nil;
    
    windowData->layer = (__bridge CAMetalLayer *)(SDL_Metal_GetLayer(windowData->view));
    windowData->layer.device = renderer->device;
    windowData->layer.displaySyncEnabled = (presentMode != SDL_GPU_PRESENTMODE_IMMEDIATE);
    windowData->layer.framebufferOnly = FALSE; /* Allow sampling swapchain textures, at the expense of performance */

    /* Fill out the texture struct */
    swapchainTexture->handle = nil; /* This will be set in AcquireSwapchainTexture. */
    swapchainTexture->levelCount = 1;
    swapchainTexture->depth = 1;
    swapchainTexture->isCube = 0;
    swapchainTexture->isRenderTarget = 1;
    
    drawableSize = windowData->layer.drawableSize;
    swapchainTexture->width = (Uint32) drawableSize.width;
    swapchainTexture->height = (Uint32) drawableSize.height;
    
    /* Set up the texture container */
    SDL_zero(windowData->textureContainer);
    windowData->textureContainer.canBeCycled = 0;
    windowData->textureContainer.activeTexture = swapchainTexture;
    windowData->textureContainer.textureCapacity = 1;
    windowData->textureContainer.textureCount = 1;
    
    return 1;
}

static Uint8 METAL_ClaimWindow(
	SDL_GpuRenderer *driverData,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(windowHandle);

    if (windowData == NULL)
    {
        windowData = (MetalWindowData*) SDL_malloc(sizeof(MetalWindowData));
        windowData->windowHandle = windowHandle; /* FIXME: needed? */

        if (METAL_INTERNAL_CreateSwapchain(renderer, windowData, presentMode))
        {
            SDL_SetProperty(SDL_GetWindowProperties(windowHandle), WINDOW_PROPERTY_DATA, windowData);

            SDL_LockMutex(renderer->windowLock);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity)
            {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(MetalWindowData*)
                );
            }
            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            SDL_UnlockMutex(renderer->windowLock);

            return 1;
        }
        else
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create swapchain, failed to claim window!");
            SDL_free(windowData);
            return 0;
        }
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Window already claimed!");
        return 0;
    }
}

static void METAL_UnclaimWindow(
	SDL_GpuRenderer *driverData,
	void *windowHandle
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    MetalWindowData *windowData = METAL_INTERNAL_FetchWindowData(windowHandle);

    if (windowData == NULL)
    {
        return;
    }

    /* FIXME: METAL_Wait(driverData); */

    windowData->layer = nil;
    SDL_Metal_DestroyView(windowData->view);

    SDL_LockMutex(renderer->windowLock);
    for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1)
    {
        if (renderer->claimedWindows[i]->windowHandle == windowHandle)
        {
            renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
            renderer->claimedWindowCount -= 1;
            break;
        }
    }
    SDL_UnlockMutex(renderer->windowLock);

    SDL_free(windowData);

    SDL_ClearProperty(SDL_GetWindowProperties(windowHandle), WINDOW_PROPERTY_DATA);
}

static void METAL_SetSwapchainPresentMode(
	SDL_GpuRenderer *driverData,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
	NOT_IMPLEMENTED
}

static SDL_GpuTextureFormat METAL_GetSwapchainFormat(
	SDL_GpuRenderer *driverData,
	void *windowHandle
) {
    NOT_IMPLEMENTED
    return SDL_GPU_TEXTUREFORMAT_R8;
}

/* Submission/Presentation */

static void METAL_INTERNAL_AllocateCommandBuffers(
    MetalRenderer *renderer,
    Uint32 allocateCount
) {
    MetalCommandBuffer *commandBuffer;

    renderer->availableCommandBufferCapacity += allocateCount;

    renderer->availableCommandBuffers = SDL_realloc(
        renderer->availableCommandBuffers,
        sizeof(MetalCommandBuffer*) * renderer->availableCommandBufferCapacity
    );

    for (Uint32 i = 0; i < allocateCount; i += 1)
    {
        commandBuffer = SDL_malloc(sizeof(MetalCommandBuffer));

        /* The native Metal command buffer is created later */

        renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
        renderer->availableCommandBufferCount += 1;
    }
}

static MetalCommandBuffer* METAL_INTERNAL_GetInactiveCommandBufferFromPool(
    MetalRenderer *renderer
) {
    MetalCommandBuffer *commandBuffer;

    if (renderer->availableCommandBufferCount == 0)
    {
        METAL_INTERNAL_AllocateCommandBuffers(
            renderer,
            renderer->availableCommandBufferCapacity
        );
    }

    commandBuffer = renderer->availableCommandBuffers[renderer->availableCommandBufferCount - 1];
    renderer->availableCommandBufferCount -= 1;

    return commandBuffer;
}

static Uint8 METAL_INTERNAL_CreateFence(
    MetalRenderer *renderer
) {
    MetalFence* fence;

    fence = SDL_malloc(sizeof(MetalFence));

    /* Add it to the available pool */
    if (renderer->availableFenceCount >= renderer->availableFenceCapacity)
    {
        renderer->availableFenceCapacity *= 2;

        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            sizeof(MetalFence*) * renderer->availableFenceCapacity
        );
    }

    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    return 1;
}

static Uint8 METAL_INTERNAL_AcquireFence(
    MetalRenderer *renderer,
    MetalCommandBuffer *commandBuffer
) {
    MetalFence *fence;

    /* Acquire a fence from the pool */
    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == 0)
    {
        if (!METAL_INTERNAL_CreateFence(renderer))
        {
            SDL_UnlockMutex(renderer->fenceLock);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fence!");
            return 0;
        }
    }

    fence = renderer->availableFences[renderer->availableFenceCount - 1];
    renderer->availableFenceCount -= 1;

    SDL_UnlockMutex(renderer->fenceLock);

    /* Reset the fence */
    fence->complete = 0;
    
    /* Associate the fence with the command buffer */
    commandBuffer->fence = fence;

    return 1;
}

static SDL_GpuCommandBuffer* METAL_AcquireCommandBuffer(
	SDL_GpuRenderer *driverData
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    MetalCommandBuffer *commandBuffer;

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    commandBuffer = METAL_INTERNAL_GetInactiveCommandBufferFromPool(renderer);
    commandBuffer->windowData = NULL;
    commandBuffer->handle = [renderer->queue commandBuffer];

    METAL_INTERNAL_AcquireFence(renderer, commandBuffer);
    commandBuffer->autoReleaseFence = 1;

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    return (SDL_GpuCommandBuffer*) commandBuffer;
}

static void METAL_INTERNAL_ReleaseFenceToPool(
    MetalRenderer *renderer,
    MetalFence *fence
) {
    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == renderer->availableFenceCapacity)
    {
        renderer->availableFenceCapacity *= 2;
        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            renderer->availableFenceCapacity * sizeof(MetalFence*)
        );
    }
    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    SDL_UnlockMutex(renderer->fenceLock);
}

static void METAL_INTERNAL_CleanCommandBuffer(
    MetalRenderer *renderer,
    MetalCommandBuffer *commandBuffer
) {
    /* FIXME: Unbind uniforms */
    /* FIXME: Reference counting */
    
    /* The fence is now available (unless SubmitAndAcquireFence was called) */
    if (commandBuffer->autoReleaseFence)
    {
        METAL_INTERNAL_ReleaseFenceToPool(renderer, commandBuffer->fence);
    }

    /* Return command buffer to pool */
    SDL_LockMutex(renderer->acquireCommandBufferLock);
    if (renderer->availableCommandBufferCount == renderer->availableCommandBufferCapacity)
    {
        renderer->availableCommandBufferCapacity += 1;
        renderer->availableCommandBuffers = SDL_realloc(
            renderer->availableCommandBuffers,
            renderer->availableCommandBufferCapacity * sizeof(MetalCommandBuffer*)
        );
    }
    renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
    renderer->availableCommandBufferCount += 1;
    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    /* Remove this command buffer from the submitted list */
    for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1)
    {
        if (renderer->submittedCommandBuffers[i] == commandBuffer)
        {
            renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
            renderer->submittedCommandBufferCount -= 1;
        }
    }
}

static SDL_GpuTexture* METAL_AcquireSwapchainTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *windowHandle,
	Uint32 *pWidth,
	Uint32 *pHeight
) {
    (void) driverData; /* used by other backends */
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    MetalWindowData *windowData;
    CGSize drawableSize;

    windowData = METAL_INTERNAL_FetchWindowData(windowHandle);
    if (windowData == NULL)
    {
        *pWidth = 0;
        *pHeight = 0;
        return NULL;
    }
    
    /* FIXME: Handle minimization! */
    
    /* Get the drawable and its underlying texture */
    windowData->drawable = [windowData->layer nextDrawable];
    windowData->texture.handle = [windowData->drawable texture];
    
    /* Let the command buffer know it's associated with this swapchain. */
    metalCommandBuffer->windowData = windowData;
    
    /* Update the window size */
    drawableSize = windowData->layer.drawableSize;
    windowData->texture.width = (Uint32) drawableSize.width;
    windowData->texture.height = (Uint32) drawableSize.height;

    /* Send the dimensions to the out parameters. */
    *pWidth = windowData->texture.width;
    *pHeight = windowData->texture.height;

    /* Return the swapchain texture */
    return (SDL_GpuTexture*) &windowData->textureContainer;
}

static void METAL_Submit(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
    MetalRenderer *renderer = (MetalRenderer*) driverData;
    MetalCommandBuffer *metalCommandBuffer = (MetalCommandBuffer*) commandBuffer;
    
    SDL_LockMutex(renderer->submitLock);
    
    /* Enqueue a present request, if applicable */
    if (metalCommandBuffer->windowData)
    {
        [metalCommandBuffer->handle presentDrawable:metalCommandBuffer->windowData->drawable];
        metalCommandBuffer->windowData->drawable = nil;
        metalCommandBuffer->windowData->texture.handle = nil;
    }
    
    /* Notify the fence when the command buffer has completed */
    [metalCommandBuffer->handle addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        metalCommandBuffer->fence->complete = 1;
    }];
    
    /* Submit the command buffer */
    [metalCommandBuffer->handle commit];
    metalCommandBuffer->handle = nil;
    
    /* Mark the command buffer as submitted */
    if (renderer->submittedCommandBufferCount >= renderer->submittedCommandBufferCapacity)
    {
        renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

        renderer->submittedCommandBuffers = SDL_realloc(
            renderer->submittedCommandBuffers,
            sizeof(MetalCommandBuffer*) * renderer->submittedCommandBufferCapacity
        );
    }
    renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = metalCommandBuffer;
    renderer->submittedCommandBufferCount += 1;
    
    /* Check if we can perform any cleanups */
    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1)
    {
        if (renderer->submittedCommandBuffers[i]->fence->complete)
        {
            METAL_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i]
            );
        }
    }

    /* FIXME: METAL_INTERNAL_PerformPendingDestroys(renderer); */

    SDL_UnlockMutex(renderer->submitLock);
}

static SDL_GpuFence* METAL_SubmitAndAcquireFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NOT_IMPLEMENTED
	return NULL;
}

static void METAL_Wait(
	SDL_GpuRenderer *driverData
) {
	NOT_IMPLEMENTED
}

static void METAL_WaitForFences(
	SDL_GpuRenderer *driverData,
	Uint8 waitAll,
	Uint32 fenceCount,
	SDL_GpuFence **pFences
) {
	NOT_IMPLEMENTED
}

static int METAL_QueryFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuFence *fence
) {
	NOT_IMPLEMENTED
	return 0;
}

static void METAL_ReleaseFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuFence *fence
) {
	NOT_IMPLEMENTED
}

/* Device Creation */

static Uint8 METAL_PrepareDriver(
	Uint32 *flags
) {
	/* FIXME: Add a macOS / iOS version check! Maybe support >= 10.14? */
	*flags = SDL_WINDOW_METAL;
	return 1;
}

static SDL_GpuDevice* METAL_CreateDevice(
	Uint8 debugMode
) {
    MetalRenderer *renderer;
    
    /* Allocate and zero out the renderer */
    renderer = (MetalRenderer*) SDL_calloc(1, sizeof(MetalRenderer));

    /* Create the Metal device and command queue */
    renderer->device = MTLCreateSystemDefaultDevice();
    renderer->queue = [renderer->device newCommandQueue];
    
    /* Print driver info */
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL GPU Driver: Metal");
    /* FIXME: Can we log more here? */
    
    /* Create mutexes */
    renderer->submitLock = SDL_CreateMutex();
    renderer->acquireCommandBufferLock = SDL_CreateMutex();
    renderer->fenceLock = SDL_CreateMutex();
    renderer->windowLock = SDL_CreateMutex();
    
    /* Create command buffer pool */
    METAL_INTERNAL_AllocateCommandBuffers(renderer, 2);
    
    /* Create fence pool */
    renderer->availableFenceCapacity = 2;
    renderer->availableFences = SDL_malloc(
        sizeof(MetalFence*) * renderer->availableFenceCapacity
    );
    
    /* Create claimed window list */
    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindows = SDL_malloc(
        sizeof(MetalWindowData*) * renderer->claimedWindowCapacity
    );

	SDL_GpuDevice *result = SDL_malloc(sizeof(SDL_GpuDevice));
	ASSIGN_DRIVER(METAL)
	result->driverData = (SDL_GpuRenderer*) renderer;
	return result;
}

SDL_GpuDriver MetalDriver = {
	"Metal",
	METAL_PrepareDriver,
	METAL_CreateDevice
};

#endif /*SDL_GPU_METAL*/
