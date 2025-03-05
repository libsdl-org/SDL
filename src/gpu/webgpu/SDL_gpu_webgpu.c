/*
  // Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

// File: /webgpu/SDL_gpu_webgpu.c
// Author: Kyle Lukaszek
// Email: kylelukaszek [at] gmail [dot] com
// Description: WebGPU driver for SDL_gpu using the emscripten WebGPU implementation
// Note: Compiling SDL GPU programs using emscripten will require -sUSE_WEBGPU=1 -sASYNCIFY=1

#include "../SDL_sysgpu.h"
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_webgpu.h>
#include <math.h>
#include <regex.h>

// TODO: REMOVE
// Code compiles without these but my LSP freaks out without them
#include <../../../wgpu/include/webgpu/webgpu.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_video.h>
#include <string.h>

/*
NOTE: Some of the enum conversion functions are a nightmare to manage between the different flavours of webgpu.h

wgpu-native source code pulls it's header from here: https://github.com/webgpu-native/webgpu-headers/blob/main/webgpu.h
however the header provided in the release version of the library: https://github.com/gfx-rs/wgpu-native/releases
have some changes to the enum values that kind of screw everything up.

Gotta wait until the header is finalized to get this fixed.

webgpu.h from webgpu-headers SHOULD work for all backends, except for Emscripten and wgpu-native apparently!
So as I suspected, Dawn would have been the best pick to work with like I had originally done before swapping to wgpu-native out of convenience.
*/

#define SDL_GPU_SHADERSTAGE_COMPUTE 2
#define WINDOW_PROPERTY_DATA        "SDL_GPUWebGPUWindowPropertyData"
#define TRACK_RESOURCE(resource, type, array, count, capacity)   \
    do {                                                         \
        Uint32 i;                                                \
                                                                 \
        for (i = 0; i < commandBuffer->count; i += 1) {          \
            if (commandBuffer->array[i] == (resource)) {         \
                return;                                          \
            }                                                    \
        }                                                        \
                                                                 \
        if (commandBuffer->count == commandBuffer->capacity) {   \
            commandBuffer->capacity += 1;                        \
            commandBuffer->array = SDL_realloc(                  \
                commandBuffer->array,                            \
                commandBuffer->capacity * sizeof(type));         \
        }                                                        \
        commandBuffer->array[commandBuffer->count] = (resource); \
        commandBuffer->count += 1;                               \
        SDL_AtomicIncRef(&(resource)->refCount);                 \
    } while (0)

#define SET_ERROR_AND_RETURN(fmt, msg, ret) \
    do {                                    \
        if (renderer->debugMode) {          \
            SDL_Log(fmt, msg);              \
        }                                   \
        SDL_SetError(fmt, msg);             \
        return ret;                         \
    } while (0)

#define SET_STRING_ERROR_AND_RETURN(msg, ret) SET_ERROR_AND_RETURN("%s", msg, ret)

// ----------------------------------------------------------------------------

typedef struct WebGPURenderer WebGPURenderer;
typedef struct WebGPUComputePipeline WebGPUComputePipeline;
typedef struct WebGPUCommandBuffer WebGPUCommandBuffer;
typedef struct WebGPUWindowData WebGPUWindowData;

static SDL_GPUShader *WebGPU_CreateShader(SDL_GPURenderer *driverData, const SDL_GPUShaderCreateInfo *shaderCreateInfo);
static SDL_GPUSampler *WebGPU_CreateSampler(SDL_GPURenderer *driverData, const SDL_GPUSamplerCreateInfo *createinfo);
static void WebGPU_INTERNAL_CleanCommandBuffer(WebGPURenderer *renderer, WebGPUCommandBuffer *commandBuffer, bool cancel);
static bool WebGPU_INTERNAL_CreateSwapchain(WebGPURenderer *renderer, WebGPUWindowData *windowData, SDL_GPUSwapchainComposition composition, SDL_GPUPresentMode presentMode);
static void WebGPU_INTERNAL_DestroySwapchain(WebGPURenderer *renderer, WebGPUWindowData *windowData);
static void WebGPU_INTERNAL_RecreateSwapchain(WebGPURenderer *renderer, WebGPUWindowData *windowData);

typedef struct WebGPUFence
{
    SDL_AtomicInt complete;
    SDL_AtomicInt referenceCount;
} WebGPUFence;

typedef struct WebGPUTexture
{
    WGPUTexture handle;
    SDL_AtomicInt refCount;
} WebGPUTexture;

typedef struct WebGPUTextureContainer
{
    TextureCommonHeader header;

    WebGPUTexture *activeTexture;
    Uint8 canBeCycled;

    Uint32 textureCapacity;
    Uint32 textureCount;
    WebGPUTexture **textures;

    char *debugName;
} WebGPUTextureContainer;

typedef struct WebGPUBuffer
{
    WGPUBuffer handle;
    bool isMapped;
    void *mappedData;
    Uint32 size;
    SDL_AtomicInt refCount;
    char *debugName;
} WebGPUBuffer;

// Callback user data for buffer mapping
typedef struct WebGPUMapCallbackData
{
    WebGPUBuffer *buffer;
    WebGPUFence *fence;
    bool success;
} WebGPUMapCallbackData;

typedef struct WebGPUBufferContainer
{
    WebGPUBuffer *activeBuffer;
    Uint32 size;

    Uint32 bufferCapacity;
    Uint32 bufferCount;
    WebGPUBuffer **buffers;

    bool isPrivate;
    bool isWriteOnly;
    char *debugName;

    WebGPUFence *lastFence;
} WebGPUBufferContainer;

typedef struct WebGPUUniformBuffer
{
    WGPUBuffer buffer;
    Uint32 writeOffset;
    Uint32 drawOffset;
} WebGPUUniformBuffer;

typedef struct BindGroupLayoutEntryInfo
{
    // These have to be extracted from the shader or else we don't
    // have enought information to build our pipeline layouts.
    WGPUTextureViewDimension sampleDimensions[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTextureSampleType sampleTypes[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUSamplerBindingType sampleBindingType[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUStorageTextureAccess storageAccess[MAX_STORAGE_TEXTURES_PER_STAGE];
    WGPUTextureViewDimension storageDimensions[MAX_STORAGE_TEXTURES_PER_STAGE];
    WGPUTextureFormat storageFormats[MAX_STORAGE_TEXTURES_PER_STAGE];
} BindGroupLayoutEntryInfo;

typedef struct WebGPUShader
{
    WGPUShaderModule shaderModule;

    SDL_GPUShaderStage stage;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;

    BindGroupLayoutEntryInfo bgl;
} WebGPUShader;

typedef struct WebGPUGraphicsPipeline
{
    WGPURenderPipeline handle;

    Uint32 sample_mask;

    SDL_GPURasterizerState rasterizerState;
    SDL_GPUPrimitiveType primitiveType;

    // Probably not needed since WebGPU stores this directly in the assembled graphics pipeline
    WGPUDepthStencilState depth_stencil_state;

    WGPUBindGroup bindGroup;

    Uint32 vertexSamplerCount;
    Uint32 vertexUniformBufferCount;
    Uint32 vertexStorageBufferCount;
    Uint32 vertexStorageTextureCount;

    Uint32 fragmentSamplerCount;
    Uint32 fragmentUniformBufferCount;
    Uint32 fragmentStorageBufferCount;
    Uint32 fragmentStorageTextureCount;

    bool resourcesDirty;
} WebGPUGraphicsPipeline;

typedef struct WebGPUComputePipeline
{
    WGPUComputePipeline handle;
    Uint32 numSamplers;
    Uint32 numReadonlyStorageTextures;
    Uint32 numReadWriteStorageTextures;
    Uint32 numReadonlyStorageBuffers;
    Uint32 numReadWriteStorageBuffers;
    Uint32 numUniformBuffers;
    Uint32 threadcountX;
    Uint32 threadcountY;
    Uint32 threadcountZ;

    BindGroupLayoutEntryInfo bgl;
} WebGPUComputePipeline;

typedef struct WebGPUWindowData
{
    SDL_Window *window;
    WebGPURenderer *renderer;
    WGPUSurface surface;
    SDL_GPUPresentMode presentMode;
    SDL_GPUSwapchainComposition swapchainComposition;
    WebGPUTexture texture;
    WebGPUTextureContainer textureContainer;
    SDL_GPUFence *inFlightFences[MAX_FRAMES_IN_FLIGHT];
    Uint32 frameCounter;
    bool needsConfigure;
} WebGPUWindowData;

typedef struct
{
    WebGPUGraphicsPipeline *pipeline; // Missing in previous solution
    WGPUBindGroup bindGroups[4];      // Pipelines will either have 3 or 4 bind groups (compute or graphics)
    bool resourcesDirty;
    Uint64 lastFrameUsed;
} WebGPUPipelineBindGroupCache;

struct WebGPUCommandBuffer
{
    CommandBufferCommonHeader header;
    WebGPURenderer *renderer;

    WGPUCommandEncoder handle;
    WGPUCommandBuffer commandBuffer; // Once the command encoder is submitted, this is created

    WebGPUWindowData **windowDatas;
    Uint32 windowDataCount;
    Uint32 windowDataCapacity;

    // Render pass
    WGPURenderPassEncoder renderEncoder;
    WebGPUGraphicsPipeline *graphicsPipeline;
    WebGPUBuffer *indexBuffer;
    Uint32 indexBufferOffset;
    SDL_GPUIndexElementSize index_element_size;

    // Copy pass
    WGPUCommandEncoder copyEncoder;

    // Compute pass
    WGPUComputePassEncoder computeEncoder;
    WebGPUComputePipeline *computePipeline;

    // Resource slot state
    bool needVertexSamplerBind;
    bool needVertexStorageTextureBind;
    bool needVertexStorageBufferBind;
    bool needVertexUniformBind;

    bool needFragmentSamplerBind;
    bool needFragmentStorageTextureBind;
    bool needFragmentStorageBufferBind;
    bool needFragmentUniformBind;

    bool needComputeSamplerBind;
    bool needComputeTextureBind;
    bool needComputeBufferBind;
    bool needComputeUniformBind;

    WGPUSampler vertexSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTexture vertexTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTexture vertexStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    WGPUBuffer vertexStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    WGPUSampler fragmentSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTexture fragmentTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTexture fragmentStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    WGPUBuffer fragmentStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    WGPUTexture computeSamplerTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUSampler computeSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUTexture computeReadOnlyTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    WGPUBuffer computeReadOnlyBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];
    WGPUTexture computeReadWriteTextures[MAX_COMPUTE_WRITE_TEXTURES];
    WGPUBuffer computeReadWriteBuffers[MAX_COMPUTE_WRITE_BUFFERS];

    WebGPUUniformBuffer *vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    WebGPUUniformBuffer *fragmentUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    WebGPUUniformBuffer *computeUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];

    WebGPUUniformBuffer **usedUniformBuffers;
    Uint32 usedUniformBufferCount;
    Uint32 usedUniformBufferCapacity;

    WebGPUFence *fence;
    bool autoReleaseFence;

    WebGPUBuffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    WebGPUTexture **usedTextures;
    Uint32 usedTextureCount;
    Uint32 usedTextureCapacity;

    WebGPUPipelineBindGroupCache *currentPipelineCache;
};

typedef struct WebGPUSampler
{
    WGPUSampler handle;
} WebGPUSampler;

typedef struct BlitPipeline
{
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUTextureFormat format;
} BlitPipeline;

struct WebGPURenderer
{
    SDL_GPUDevice *sdlDevice;
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    WGPULimits deviceLimits;
    WGPUAdapterInfo adapterInfo;

    bool deviceError;

    bool debugMode;
    bool preferLowPower;

    Uint32 allowedFramesInFlight;

    WebGPUWindowData **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;

    WebGPUCommandBuffer **availableCommandBuffers;
    Uint32 availableCommandBufferCount;
    Uint32 availableCommandBufferCapacity;

    WebGPUCommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCount;
    Uint32 submittedCommandBufferCapacity;

    WebGPUFence **availableFences;
    Uint32 availableFenceCount;
    Uint32 availableFenceCapacity;

    WebGPUUniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    WebGPUBufferContainer **bufferContainersToDestroy;
    Uint32 bufferContainersToDestroyCount;
    Uint32 bufferContainersToDestroyCapacity;

    WebGPUTextureContainer **textureContainersToDestroy;
    Uint32 textureContainersToDestroyCount;
    Uint32 textureContainersToDestroyCapacity;

    // Blit
    SDL_GPUShader *blitVertexShader;
    SDL_GPUShader *blitFrom2DShader;
    SDL_GPUShader *blitFrom2DArrayShader;
    SDL_GPUShader *blitFrom3DShader;
    SDL_GPUShader *blitFromCubeShader;
    SDL_GPUShader *blitFromCubeArrayShader;

    SDL_GPUSampler *blitNearestSampler;
    SDL_GPUSampler *blitLinearSampler;

    BlitPipelineCacheEntry *blitPipelines;
    Uint32 blitPipelineCount;
    Uint32 blitPipelineCapacity;

    WebGPUPipelineBindGroupCache *pipelineBindGroupCache;
    Uint32 pipelineBindGroupCacheCount;
    Uint32 pipelineBindGroupCacheCapacity;

    // Mutexes
    SDL_Mutex *submitLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *acquireUniformBufferLock;
    SDL_Mutex *disposeLock;
    SDL_Mutex *fenceLock;
    SDL_Mutex *windowLock;
};

// Static arrays

static SDL_GPUTextureFormat SwapchainCompositionToFormat[] = {
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,      // SDR
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB, // SDR_LINEAR
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,  // HDR_EXTENDED_LINEAR
    SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,   // HDR10_ST2084
};

// Debugging
static void DebugFrameObjects(WebGPUCommandBuffer *cmdBuffer)
{
    static void *lastPipeline = NULL;
    static void *lastBindGroup = NULL;

    void *currentPipeline = cmdBuffer->graphicsPipeline;
    void *currentBindGroup = cmdBuffer->graphicsPipeline ? cmdBuffer->graphicsPipeline->bindGroup : NULL;

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Frame objects: Pipeline=%p (same=%d), BindGroup=%p (same=%d)",
                currentPipeline, currentPipeline == lastPipeline,
                currentBindGroup, currentBindGroup == lastBindGroup);

    lastPipeline = currentPipeline;
    lastBindGroup = currentBindGroup;
}

// Resource Tracking:
// ---------------------------------------------------
static void WebGPU_INTERNAL_TrackTexture(
    WebGPUCommandBuffer *commandBuffer,
    WebGPUTexture *texture)
{
    TRACK_RESOURCE(
        texture,
        WebGPUTexture *,
        usedTextures,
        usedTextureCount,
        usedTextureCapacity);
}

static void WebGPU_INTERNAL_TrackBuffer(
    WebGPUCommandBuffer *commandBuffer,
    WebGPUBuffer *buffer)
{
    TRACK_RESOURCE(
        buffer,
        WebGPUBuffer *,
        usedBuffers,
        usedBufferCount,
        usedBufferCapacity);
}

static void WebGPU_INTERNAL_IncrementBufferRefCounts(WebGPUCommandBuffer *commandBuffer)
{
    for (Uint32 i = 0; i < commandBuffer->usedBufferCount; i++) {
        SDL_AtomicIncRef(&commandBuffer->usedBuffers[i]->refCount);
    }
}

static void WebGPU_INTERNAL_DecrementBufferRefCounts(WebGPUCommandBuffer *commandBuffer)
{
    for (Uint32 i = 0; i < commandBuffer->usedBufferCount; i++) {
        if (SDL_AtomicDecRef(&commandBuffer->usedBuffers[i]->refCount)) {
            // Buffer can be destroyed if refCount reaches 0, handled in dispose logic
        }
    }
}

static void WebGPU_INTERNAL_TrackUniformBuffer(
    WebGPUCommandBuffer *commandBuffer,
    WebGPUUniformBuffer *uniformBuffer)
{
    Uint32 i;
    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        if (commandBuffer->usedUniformBuffers[i] == uniformBuffer) {
            return;
        }
    }

    if (commandBuffer->usedUniformBufferCount == commandBuffer->usedUniformBufferCapacity) {
        commandBuffer->usedUniformBufferCapacity += 1;
        commandBuffer->usedUniformBuffers = SDL_realloc(
            commandBuffer->usedUniformBuffers,
            commandBuffer->usedUniformBufferCapacity * sizeof(WebGPUUniformBuffer *));
    }

    commandBuffer->usedUniformBuffers[commandBuffer->usedUniformBufferCount] = uniformBuffer;
    commandBuffer->usedUniformBufferCount += 1;
}

// Conversion Functions:
// ---------------------------------------------------

static WGPUBufferUsage SDLToWGPUBufferUsageFlags(SDL_GPUBufferUsageFlags usageFlags)
{
    WGPUBufferUsage wgpuFlags = WGPUBufferUsage_None;
    if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX) {
        wgpuFlags |= WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    }
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX) {
        wgpuFlags |= WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    }
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT) {
        wgpuFlags |= WGPUBufferUsage_Indirect;
    }
    return wgpuFlags;
}

static WGPULoadOp SDLToWGPULoadOp(SDL_GPULoadOp loadOp)
{
    switch (loadOp) {
    case SDL_GPU_LOADOP_LOAD:
        return WGPULoadOp_Load;
    case SDL_GPU_LOADOP_CLEAR:
        return WGPULoadOp_Clear;
    case SDL_GPU_LOADOP_DONT_CARE:
        return WGPULoadOp_Clear;
    default:
        return WGPULoadOp_Clear;
    }
}

static WGPUStoreOp SDLToWGPUStoreOp(SDL_GPUStoreOp storeOp)
{
    switch (storeOp) {
    case SDL_GPU_STOREOP_STORE:
        return WGPUStoreOp_Store;
    case SDL_GPU_STOREOP_DONT_CARE:
        return WGPUStoreOp_Discard;
    default:
        return WGPUStoreOp_Store;
    }
}

static WGPUAddressMode SDLToWGPUAddressMode(SDL_GPUSamplerAddressMode addressMode)
{
    switch (addressMode) {
    case SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE:
        return WGPUAddressMode_ClampToEdge;
    case SDL_GPU_SAMPLERADDRESSMODE_REPEAT:
        return WGPUAddressMode_Repeat;
    case SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT:
        return WGPUAddressMode_MirrorRepeat;
    default:
        return WGPUAddressMode_ClampToEdge;
    }
}

static WGPUFilterMode SDLToWGPUFilterMode(SDL_GPUFilter filterMode)
{
    switch (filterMode) {
    case SDL_GPU_FILTER_NEAREST:
        return WGPUFilterMode_Nearest;
    case SDL_GPU_FILTER_LINEAR:
        return WGPUFilterMode_Linear;
    default:
        return WGPUFilterMode_Undefined;
    }
}

static WGPUMipmapFilterMode SDLToWGPUSamplerMipmapMode(SDL_GPUSamplerMipmapMode mipmapMode)
{
    switch (mipmapMode) {
    case SDL_GPU_SAMPLERMIPMAPMODE_NEAREST:
        return WGPUMipmapFilterMode_Nearest;
    case SDL_GPU_SAMPLERMIPMAPMODE_LINEAR:
        return WGPUMipmapFilterMode_Linear;
    default:
        return WGPUMipmapFilterMode_Undefined;
    }
}

static WGPUPrimitiveTopology SDLToWGPUPrimitiveTopology(SDL_GPUPrimitiveType topology)
{
    switch (topology) {
    case SDL_GPU_PRIMITIVETYPE_POINTLIST:
        return WGPUPrimitiveTopology_PointList;
    case SDL_GPU_PRIMITIVETYPE_LINELIST:
        return WGPUPrimitiveTopology_LineList;
    case SDL_GPU_PRIMITIVETYPE_LINESTRIP:
        return WGPUPrimitiveTopology_LineStrip;
    case SDL_GPU_PRIMITIVETYPE_TRIANGLELIST:
        return WGPUPrimitiveTopology_TriangleList;
    case SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP:
        return WGPUPrimitiveTopology_TriangleStrip;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid primitive type %d", topology);
        return WGPUPrimitiveTopology_TriangleList;
    }
}

static WGPUFrontFace SDLToWGPUFrontFace(SDL_GPUFrontFace frontFace)
{
    switch (frontFace) {
    case SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE:
        return WGPUFrontFace_CCW;
    case SDL_GPU_FRONTFACE_CLOCKWISE:
        return WGPUFrontFace_CW;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid front face %d. Using CCW.", frontFace);
        return WGPUFrontFace_CCW;
    }
}

static WGPUCullMode SDLToWGPUCullMode(SDL_GPUCullMode cullMode)
{
    switch (cullMode) {
    case SDL_GPU_CULLMODE_NONE:
        return WGPUCullMode_None;
    case SDL_GPU_CULLMODE_FRONT:
        return WGPUCullMode_Front;
    case SDL_GPU_CULLMODE_BACK:
        return WGPUCullMode_Back;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid cull mode %d. Using None.", cullMode);
        return WGPUCullMode_None;
    }
}

static WGPUIndexFormat SDLToWGPUIndexFormat(SDL_GPUIndexElementSize indexType)
{
    switch (indexType) {
    case SDL_GPU_INDEXELEMENTSIZE_16BIT:
        return WGPUIndexFormat_Uint16;
    case SDL_GPU_INDEXELEMENTSIZE_32BIT:
        return WGPUIndexFormat_Uint32;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid index type %d. Using Uint16.", indexType);
        return WGPUIndexFormat_Uint16;
    }
}

static WGPUTextureFormat SDLToWGPUTextureFormat(SDL_GPUTextureFormat sdlFormat)
{
    switch (sdlFormat) {
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
        return WGPUTextureFormat_R8Unorm;
    case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
        return WGPUTextureFormat_RG8Unorm;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
        return WGPUTextureFormat_RGBA8Unorm;
    case SDL_GPU_TEXTUREFORMAT_R16_UNORM:
        return WGPUTextureFormat_R16Uint; // Note: WebGPU doesn't have R16Unorm
    case SDL_GPU_TEXTUREFORMAT_R16G16_UNORM:
        return WGPUTextureFormat_RG16Uint; // Note: WebGPU doesn't have RG16Unorm
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
        return WGPUTextureFormat_RGBA16Uint; // Note: WebGPU doesn't have RGBA16Unorm
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
        return WGPUTextureFormat_RGB10A2Unorm;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
        return WGPUTextureFormat_BGRA8Unorm;
    case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM:
        return WGPUTextureFormat_BC1RGBAUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM:
        return WGPUTextureFormat_BC2RGBAUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM:
        return WGPUTextureFormat_BC3RGBAUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM:
        return WGPUTextureFormat_BC4RUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM:
        return WGPUTextureFormat_BC5RGUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM:
        return WGPUTextureFormat_BC7RGBAUnorm;
    case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT:
        return WGPUTextureFormat_BC6HRGBFloat;
    case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT:
        return WGPUTextureFormat_BC6HRGBUfloat;
    case SDL_GPU_TEXTUREFORMAT_R8_SNORM:
        return WGPUTextureFormat_R8Snorm;
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
        return WGPUTextureFormat_RG8Snorm;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
        return WGPUTextureFormat_RGBA8Snorm;
    case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
        return WGPUTextureFormat_R16Float;
    case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
        return WGPUTextureFormat_RG16Float;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
        return WGPUTextureFormat_RGBA16Float;
    case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
        return WGPUTextureFormat_R32Float;
    case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
        return WGPUTextureFormat_RG32Float;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
        return WGPUTextureFormat_RGBA32Float;
    case SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT:
        return WGPUTextureFormat_RG11B10Ufloat;
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
        return WGPUTextureFormat_R8Uint;
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
        return WGPUTextureFormat_RG8Uint;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
        return WGPUTextureFormat_RGBA8Uint;
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
        return WGPUTextureFormat_R16Uint;
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
        return WGPUTextureFormat_RG16Uint;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
        return WGPUTextureFormat_RGBA16Uint;
    case SDL_GPU_TEXTUREFORMAT_R8_INT:
        return WGPUTextureFormat_R8Sint;
    case SDL_GPU_TEXTUREFORMAT_R8G8_INT:
        return WGPUTextureFormat_RG8Sint;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT:
        return WGPUTextureFormat_RGBA8Sint;
    case SDL_GPU_TEXTUREFORMAT_R16_INT:
        return WGPUTextureFormat_R16Sint;
    case SDL_GPU_TEXTUREFORMAT_R16G16_INT:
        return WGPUTextureFormat_RG16Sint;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT:
        return WGPUTextureFormat_RGBA16Sint;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
        return WGPUTextureFormat_RGBA8UnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        return WGPUTextureFormat_BGRA8UnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB:
        return WGPUTextureFormat_BC1RGBAUnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB:
        return WGPUTextureFormat_BC2RGBAUnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB:
        return WGPUTextureFormat_BC3RGBAUnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB:
        return WGPUTextureFormat_BC7RGBAUnormSrgb;
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
        return WGPUTextureFormat_Depth16Unorm;
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
        return WGPUTextureFormat_Depth24Plus;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
        return WGPUTextureFormat_Depth32Float;
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        return WGPUTextureFormat_Depth24PlusStencil8;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        return WGPUTextureFormat_Depth32FloatStencil8;
    default:
        return WGPUTextureFormat_Undefined;
    }
}

static WGPUTextureUsage SDLToWGPUTextureUsageFlags(SDL_GPUTextureUsageFlags usageFlags, SDL_GPUTextureFormat format, SDL_GPUTextureType type)
{
    WGPUTextureUsage wgpuFlags = WGPUTextureUsage_None;

    // Special handling for depth-stencil textures
    if (IsDepthFormat(format) || IsStencilFormat(format)) {
        // Depth-stencil textures must have RenderAttachment usage if 2D
        if (type != SDL_GPU_TEXTURETYPE_3D) {
            wgpuFlags |= WGPUTextureUsage_RenderAttachment;
        }

        // If it's used as a sampler, add TextureBinding
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
            wgpuFlags |= WGPUTextureUsage_TextureBinding;
        }

        // Always allow copying to/from depth-stencil textures
        wgpuFlags |= WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    } else {
        // Normal handling for color textures (your existing logic)
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
            wgpuFlags |= WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {
            // if (type != SDL_GPU_TEXTURETYPE_3D) {
            wgpuFlags |= WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
            // } else {
            // wgpuFlags |= WGPUTextureUsage_CopyDst;
            // }
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) {
            wgpuFlags |= WGPUTextureUsage_RenderAttachment;
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ) {
            wgpuFlags |= WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopyDst;
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ) {
            wgpuFlags |= WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopyDst;
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE) {
            wgpuFlags |= WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc;
        }
        if (usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE) {
            wgpuFlags |= WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
        }
    }

    return wgpuFlags;
}

static WGPUTextureDimension SDLToWGPUTextureDimension(SDL_GPUTextureType type)
{
    switch (type) {
    case SDL_GPU_TEXTURETYPE_2D:
    case SDL_GPU_TEXTURETYPE_2D_ARRAY:
    // Cubemaps in WebGPU are treated as 2D textures so we set the dimension to 2D
    case SDL_GPU_TEXTURETYPE_CUBE:
    case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
        return WGPUTextureDimension_2D;
    case SDL_GPU_TEXTURETYPE_3D:
        return WGPUTextureDimension_3D;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid texture type %d. Using 2D.", type);
        return WGPUTextureDimension_2D;
    }
}

static WGPUTextureViewDimension SDLToWGPUTextureViewDimension(SDL_GPUTextureType type)
{
    switch (type) {
    case SDL_GPU_TEXTURETYPE_2D:
        return WGPUTextureViewDimension_2D;
    case SDL_GPU_TEXTURETYPE_2D_ARRAY:
        return WGPUTextureViewDimension_2DArray;
    case SDL_GPU_TEXTURETYPE_CUBE:
        return WGPUTextureViewDimension_Cube;
    case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
        return WGPUTextureViewDimension_CubeArray;
    case SDL_GPU_TEXTURETYPE_3D:
        return WGPUTextureViewDimension_3D;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_GPU: Invalid texture type %d. Using 2D.", type);
        return WGPUTextureViewDimension_2D;
    }
}

static Uint32 SDLToWGPUSampleCount(SDL_GPUSampleCount samples)
{
    switch (samples) {
    // WGPU only supports 1, and 4x MSAA
    case SDL_GPU_SAMPLECOUNT_1:
        return 1;
    case SDL_GPU_SAMPLECOUNT_2:
    case SDL_GPU_SAMPLECOUNT_4:
    case SDL_GPU_SAMPLECOUNT_8:
        return 4;
    default:
        return 1;
    }
}

static WGPUBlendFactor SDLToWGPUBlendFactor(SDL_GPUBlendFactor sdlFactor)
{
    switch (sdlFactor) {
    case SDL_GPU_BLENDFACTOR_ZERO:
        return WGPUBlendFactor_Zero;
    case SDL_GPU_BLENDFACTOR_ONE:
        return WGPUBlendFactor_One;
    case SDL_GPU_BLENDFACTOR_SRC_COLOR:
        return WGPUBlendFactor_Src;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR:
        return WGPUBlendFactor_OneMinusSrc;
    case SDL_GPU_BLENDFACTOR_DST_COLOR:
        return WGPUBlendFactor_Dst;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR:
        return WGPUBlendFactor_OneMinusDst;
    case SDL_GPU_BLENDFACTOR_SRC_ALPHA:
        return WGPUBlendFactor_SrcAlpha;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:
        return WGPUBlendFactor_OneMinusSrcAlpha;
    case SDL_GPU_BLENDFACTOR_DST_ALPHA:
        return WGPUBlendFactor_DstAlpha;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA:
        return WGPUBlendFactor_OneMinusDstAlpha;
    case SDL_GPU_BLENDFACTOR_CONSTANT_COLOR:
        return WGPUBlendFactor_Constant;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR:
        return WGPUBlendFactor_OneMinusConstant;
    case SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE:
        return WGPUBlendFactor_SrcAlphaSaturated;
    default:
        return WGPUBlendFactor_Undefined;
    }
}

static WGPUBlendOperation SDLToWGPUBlendOperation(SDL_GPUBlendOp sdlOp)
{
    switch (sdlOp) {
    case SDL_GPU_BLENDOP_ADD:
        return WGPUBlendOperation_Add;
    case SDL_GPU_BLENDOP_SUBTRACT:
        return WGPUBlendOperation_Subtract;
    case SDL_GPU_BLENDOP_REVERSE_SUBTRACT:
        return WGPUBlendOperation_ReverseSubtract;
    case SDL_GPU_BLENDOP_MIN:
        return WGPUBlendOperation_Min;
    case SDL_GPU_BLENDOP_MAX:
        return WGPUBlendOperation_Max;
    default:
        return WGPUBlendOperation_Undefined;
    }
}

static WGPUStencilOperation SDLToWGPUStencilOperation(SDL_GPUStencilOp op)
{
    switch (op) {
    case SDL_GPU_STENCILOP_KEEP:
        return WGPUStencilOperation_Keep;
    case SDL_GPU_STENCILOP_ZERO:
        return WGPUStencilOperation_Zero;
    case SDL_GPU_STENCILOP_REPLACE:
        return WGPUStencilOperation_Replace;
    case SDL_GPU_STENCILOP_INVERT:
        return WGPUStencilOperation_Invert;
    case SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP:
        return WGPUStencilOperation_IncrementClamp;
    case SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP:
        return WGPUStencilOperation_DecrementClamp;
    case SDL_GPU_STENCILOP_INCREMENT_AND_WRAP:
        return WGPUStencilOperation_IncrementWrap;
    case SDL_GPU_STENCILOP_DECREMENT_AND_WRAP:
        return WGPUStencilOperation_DecrementWrap;
    default:
        return WGPUStencilOperation_Keep;
    }
}

static WGPUColorWriteMask SDLToWGPUColorWriteMask(SDL_GPUColorComponentFlags mask)
{
    WGPUColorWriteMask wgpuMask = WGPUColorWriteMask_None;
    if (mask & SDL_GPU_COLORCOMPONENT_R) {
        wgpuMask |= WGPUColorWriteMask_Green;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_G) {
        wgpuMask |= WGPUColorWriteMask_Blue;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_B) {
        wgpuMask |= WGPUColorWriteMask_Alpha;
    }
    if (mask & SDL_GPU_COLORCOMPONENT_A) {
        wgpuMask |= WGPUColorWriteMask_Red;
    }
    return wgpuMask;
}

static WGPUCompareFunction SDLToWGPUCompareFunction(SDL_GPUCompareOp compareOp)
{
    switch (compareOp) {
    case SDL_GPU_COMPAREOP_INVALID:
        return WGPUCompareFunction_Undefined;
    case SDL_GPU_COMPAREOP_NEVER:
        return WGPUCompareFunction_Never;
    case SDL_GPU_COMPAREOP_LESS:
        return WGPUCompareFunction_Less;
    case SDL_GPU_COMPAREOP_EQUAL:
        return WGPUCompareFunction_Equal;
    case SDL_GPU_COMPAREOP_LESS_OR_EQUAL:
        return WGPUCompareFunction_LessEqual;
    case SDL_GPU_COMPAREOP_GREATER:
        return WGPUCompareFunction_Greater;
    case SDL_GPU_COMPAREOP_NOT_EQUAL:
        return WGPUCompareFunction_NotEqual;
    case SDL_GPU_COMPAREOP_GREATER_OR_EQUAL:
        return WGPUCompareFunction_GreaterEqual;
    case SDL_GPU_COMPAREOP_ALWAYS:
        return WGPUCompareFunction_Always;
    default:
        return WGPUCompareFunction_Undefined;
    }
}

static WGPUPresentMode SDLToWGPUPresentMode(SDL_GPUPresentMode presentMode)
{
    switch (presentMode) {
    case SDL_GPU_PRESENTMODE_IMMEDIATE:
        /*SDL_Log("WebGPU: Immediate present mode.");*/
        return WGPUPresentMode_Immediate;
    case SDL_GPU_PRESENTMODE_MAILBOX:
        /*SDL_Log("WebGPU: Mailbox present mode.");*/
        return WGPUPresentMode_Mailbox;
    case SDL_GPU_PRESENTMODE_VSYNC:
        /*SDL_Log("WebGPU: VSYNC/FIFO present mode.");*/
        return WGPUPresentMode_Fifo;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "WebGPU: Defaulting to VSYNC/FIFO present mode.");
        return WGPUPresentMode_Fifo;
    }
}

// NOTE: This is one of the enums that is in limbo across WebGPU implementations.
// webgpu-headers says 0 should be reserved for Undefined, however wgpu-native
// believes that 0 should be for "buffer unused", 1 should be undefined, and ...
// When attempting to use the wgpu-native library from their releases page, I
// end up getting invalid vertex step mode errors when using vertex buffers. This
// issue does not occur when swapping back.
static WGPUVertexStepMode SDLToWGPUInputStepMode(SDL_GPUVertexInputRate inputRate)
{
    switch (inputRate) {
    case SDL_GPU_VERTEXINPUTRATE_VERTEX:
        return WGPUVertexStepMode_Vertex;
    case SDL_GPU_VERTEXINPUTRATE_INSTANCE:
        return WGPUVertexStepMode_Instance;
    default:
        return WGPUVertexStepMode_Undefined;
    }
}

static WGPUVertexFormat SDLToWGPUVertexFormat(SDL_GPUVertexElementFormat format)
{
    switch (format) {
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:
        return WGPUVertexFormat_Float32;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:
        return WGPUVertexFormat_Float32x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:
        return WGPUVertexFormat_Float32x3;
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:
        return WGPUVertexFormat_Float32x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT:
        return WGPUVertexFormat_Sint32;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT2:
        return WGPUVertexFormat_Sint32x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT3:
        return WGPUVertexFormat_Sint32x3;
    case SDL_GPU_VERTEXELEMENTFORMAT_INT4:
        return WGPUVertexFormat_Sint32x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT:
        return WGPUVertexFormat_Uint32;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:
        return WGPUVertexFormat_Uint32x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:
        return WGPUVertexFormat_Uint32x3;
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:
        return WGPUVertexFormat_Uint32x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
        return WGPUVertexFormat_Snorm8x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
        return WGPUVertexFormat_Snorm8x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:
        return WGPUVertexFormat_Unorm8x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:
        return WGPUVertexFormat_Unorm8x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:
        return WGPUVertexFormat_Sint16x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:
        return WGPUVertexFormat_Sint16x4;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:
        return WGPUVertexFormat_Uint16x2;
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:
        return WGPUVertexFormat_Uint16x4;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Incorrect Vertex Format Provided: %u", format);
        return 0;
    }
}

// Blit shaders for WebGPU
// ---------------------------------------------------

const char *blitVert = R"(
struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) tex: vec2<f32>
};

@vertex
fn main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var output: VertexOutput;
    let tex = vec2<f32>(
        f32((vertexIndex << 1u) & 2u),
        f32(vertexIndex & 2u)
    );
    output.tex = tex;
    output.pos = vec4<f32>(
        tex * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0),
        0.0,
        1.0
    );
    return output;
}
)";

const char *blit2DShader = R"(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}  
@group(2) @binding(0) var sourceTexture2D: texture_2d<f32>;
@group(2) @binding(1) var sourceSampler: sampler;
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;

@fragment
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;
    return textureSampleLevel(sourceTexture2D, sourceSampler, newCoord, sourceRegion.mipLevel);
}
)";

const char *blit2DArrayShader = R"(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}
@group(2) @binding(0) var sourceTexture2DArray: texture_2d_array<f32>;
@group(2) @binding(1) var sourceSampler: sampler;
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;

@fragment
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = vec2<f32>(
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex
    );
    return textureSampleLevel(sourceTexture2DArray, sourceSampler, newCoord, u32(sourceRegion.layerOrDepth), sourceRegion.mipLevel);
}
)";

const char *blit3DShader = R"(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}
@group(2) @binding(0) var sourceTexture3D: texture_3d<f32>;
@group(2) @binding(1) var sourceSampler: sampler;
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;

@fragment
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = vec3<f32>(
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex,
        sourceRegion.layerOrDepth
    );
    return textureSampleLevel(sourceTexture3D, sourceSampler, newCoord, sourceRegion.mipLevel);
}
)";

const char *blitCubeShader = R"(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}
@group(2) @binding(0) var sourceTextureCube: texture_cube<f32>;
@group(2) @binding(1) var sourceSampler: sampler;
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;

@fragment
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let scaledUV = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;
    let u = 2.0 * scaledUV.x - 1.0;
    let v = 2.0 * scaledUV.y - 1.0;
    var newCoord: vec3<f32>;

    switch(u32(sourceRegion.layerOrDepth)) {
        case 0u: { newCoord = vec3<f32>(1.0, -v, -u); }
        case 1u: { newCoord = vec3<f32>(-1.0, -v, u); }
        case 2u: { newCoord = vec3<f32>(u, 1.0, -v); }
        case 3u: { newCoord = vec3<f32>(u, -1.0, v); }
        case 4u: { newCoord = vec3<f32>(u, -v, 1.0); }
        case 5u: { newCoord = vec3<f32>(-u, -v, -1.0); }
        default: { newCoord = vec3<f32>(0.0, 0.0, 0.0); }
    }

    return textureSampleLevel(sourceTextureCube, sourceSampler, newCoord, sourceRegion.mipLevel);
}
)";

const char *blitCubeArrayShader = R""(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}
@group(2) @binding(0) var sourceTextureCubeArray: texture_cube_array<f32>;
@group(2) @binding(1) var sourceSampler: sampler;
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;

@fragment
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let scaledUV = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;
    let u = 2.0 * scaledUV.x - 1.0;
    let v = 2.0 * scaledUV.y - 1.0;
    let arrayIndex = u32(sourceRegion.layerOrDepth) / 6u;
    var newCoord: vec3<f32>;

    switch(u32(sourceRegion.layerOrDepth) % 6u) {
        case 0u: { newCoord = vec3<f32>(1.0, -v, -u); }
        case 1u: { newCoord = vec3<f32>(-1.0, -v, u); }
        case 2u: { newCoord = vec3<f32>(u, 1.0, -v); }
        case 3u: { newCoord = vec3<f32>(u, -1.0, v); }
        case 4u: { newCoord = vec3<f32>(u, -v, 1.0); }
        case 5u: { newCoord = vec3<f32>(-u, -v, -1.0); }
        default: { newCoord = vec3<f32>(0.0, 0.0, 0.0); }
    }

    return textureSampleLevel(sourceTextureCubeArray, sourceSampler, newCoord, arrayIndex, sourceRegion.mipLevel);
}
)"";

static void WebGPU_INTERNAL_InitBlitResources(
    WebGPURenderer *renderer)
{
    SDL_GPUShaderCreateInfo shaderCreateInfo;

    SDL_Log("Initializing WebGPU blit resources");

    renderer->blitPipelineCapacity = 2;
    renderer->blitPipelineCount = 0;
    renderer->blitPipelines = (BlitPipelineCacheEntry *)SDL_calloc(
        renderer->blitPipelineCapacity, sizeof(BlitPipelineCacheEntry));

    // Fullscreen vertex shader
    SDL_zero(shaderCreateInfo);
    shaderCreateInfo.code = (Uint8 *)blitVert;
    shaderCreateInfo.code_size = SDL_strlen(blitVert);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_WGSL;
    shaderCreateInfo.entrypoint = "main";

    renderer->blitVertexShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitVertexShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile vertex shader for blit!");
    }

    shaderCreateInfo.code = (Uint8 *)blit2DShader;
    shaderCreateInfo.code_size = SDL_strlen(blit2DShader);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.num_samplers = 1;
    shaderCreateInfo.num_uniform_buffers = 1;
    renderer->blitFrom2DShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom2DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2D pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blit2DArrayShader;
    shaderCreateInfo.code_size = SDL_strlen(blit2DArrayShader);
    shaderCreateInfo.entrypoint = "main";
    renderer->blitFrom2DArrayShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom2DArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2DArray pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blit3DShader;
    shaderCreateInfo.code_size = SDL_strlen(blit3DShader);
    renderer->blitFrom3DShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom3DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom3D pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blitCubeShader;
    shaderCreateInfo.code_size = SDL_strlen(blitCubeShader);
    renderer->blitFromCubeShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFromCubeShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFromCube pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blitCubeArrayShader;
    shaderCreateInfo.code_size = SDL_strlen(blitCubeArrayShader);
    renderer->blitFromCubeArrayShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFromCubeArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFromCubeArray pixel shader!");
    }

    // Create samplers
    SDL_GPUSamplerCreateInfo nearestCreateInfo = (SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    renderer->blitNearestSampler = WebGPU_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &nearestCreateInfo);

    if (renderer->blitNearestSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit nearest sampler!");
    }

    SDL_GPUSamplerCreateInfo linearCreateInfo = (SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    renderer->blitLinearSampler = WebGPU_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &linearCreateInfo);

    if (renderer->blitLinearSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit linear sampler!");
    }

    // WebGPUSampler *linearSampler = (WebGPUSampler *)renderer->blitLinearSampler;
    // WebGPUSampler *nearestSampler = (WebGPUSampler *)renderer->blitNearestSampler;

    // Set the labels of the samplers

    // const char *near_str = "Blit Nearest Sampler";
    // const char *lin_str = "Blit Linear Sampler";
    // wgpuSamplerSetLabel(nearestSampler->handle, (WGPUStringView){ near_str, SDL_strlen(near_str) });
    // wgpuSamplerSetLabel(linearSampler->handle, (WGPUStringView){ lin_str, SDL_strlen(lin_str) });
}

// Device Request Callback for when the device is requested from the adapter
static void WebGPU_RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2)
{
    WebGPURenderer *renderer = (WebGPURenderer *)userdata1;
    (void)userdata2;
    if (status == WGPURequestDeviceStatus_Success) {
        renderer->device = device;
        renderer->deviceError = false;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to request WebGPU device: %s", message.data);
        renderer->deviceError = true;
    }
}

// Callback for requesting an adapter from the WebGPU instance
// This will then request a device from the adapter once the adapter is successfully requested
static void WebGPU_RequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2)
{
    WebGPURenderer *renderer = (WebGPURenderer *)userdata1;

    switch (status) {
    case WGPURequestAdapterStatus_Success:
        renderer->adapter = adapter;
        // Request device from adapter
        // TODO: These should probably be props or something
        WGPUFeatureName requiredFeatures[1] = {
            WGPUFeatureName_Depth32FloatStencil8
        };
        WGPUDeviceDescriptor dev_desc = {
            .requiredFeatureCount = 1,
            .requiredFeatures = requiredFeatures,
        };

        WGPURequestDeviceCallbackInfo callback = {
            .callback = WebGPU_RequestDeviceCallback,
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .userdata1 = renderer
        };
        wgpuAdapterRequestDevice(renderer->adapter, &dev_desc, callback);
        break;
    // case WGPURequestAdapterStatus_Unknown:
    //     SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Adapter Status Unknown: %s", message.data);
    //     break;
    case WGPURequestAdapterStatus_Unavailable:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Adapter Status Unavailable: %s", message.data);
        break;
    case WGPURequestAdapterStatus_InstanceDropped:
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Adapter Status Instance Dropped: %s", message.data);
        break;
    case WGPURequestAdapterStatus_Error:
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Adapter Status Error: %s", message.data);
        break;
    default:
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Undefined Adapter Status ???: %s", message.data);
        break;
    }
}

static bool WebGPU_SupportsTextureFormat(SDL_GPURenderer *driverData,
                                         SDL_GPUTextureFormat format,
                                         SDL_GPUTextureType type,
                                         SDL_GPUTextureUsageFlags usage)
{
    (void)driverData;
    WGPUTextureFormat wgpuFormat = SDLToWGPUTextureFormat(format);
    WGPUTextureUsage wgpuUsage = SDLToWGPUTextureUsageFlags(usage, format, type);
    WGPUTextureDimension dimension = WGPUTextureDimension_Undefined;
    if (type == SDL_GPU_TEXTURETYPE_2D || type == SDL_GPU_TEXTURETYPE_2D_ARRAY) {
        dimension = WGPUTextureDimension_2D;
    } else if (type == SDL_GPU_TEXTURETYPE_3D || type == SDL_GPU_TEXTURETYPE_CUBE_ARRAY || type == SDL_GPU_TEXTURETYPE_CUBE) {
        dimension = WGPUTextureDimension_3D;
    }

    // Verify that the format, usage, and dimension are considered valid
    if (wgpuFormat == WGPUTextureFormat_Undefined) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Texture Format Undefined");
        return false;
    }
    if (wgpuUsage == WGPUTextureUsage_None) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Texture Usage NONE");
        return false;
    }
    if (dimension == WGPUTextureDimension_Undefined) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Undefined Texture Dimension!");
        return false;
    }

    // Texture format is valid.
    return true;
}

static bool WebGPU_SupportsSampleCount(SDL_GPURenderer *driverData,
                                       SDL_GPUTextureFormat format,
                                       SDL_GPUSampleCount desiredSampleCount)
{
    (void)driverData;
    WGPUTextureFormat wgpuFormat = SDLToWGPUTextureFormat(format);
    // Verify that the format and sample count are considered valid
    if (wgpuFormat == WGPUTextureFormat_Undefined) {
        return false;
    }

    // WebGPU only supports 1 and 4.
    if (desiredSampleCount != SDL_GPU_SAMPLECOUNT_1 && desiredSampleCount != SDL_GPU_SAMPLECOUNT_4) {
        return false;
    }

    /*Uint32 wgpuSampleCount = SDLToWGPUSampleCount(desiredSampleCount);*/

    // Sample count is valid.
    return true;
}

static bool WebGPU_SupportsPresentMode(SDL_GPURenderer *driverData,
                                       SDL_Window *window,
                                       SDL_GPUPresentMode presentMode)
{
    (void)driverData;
    (void)window;
    WGPUPresentMode wgpuPresentMode = SDLToWGPUPresentMode(presentMode);

    // WebGPU only supports these present modes
    if (wgpuPresentMode != WGPUPresentMode_Fifo &&
        wgpuPresentMode != WGPUPresentMode_Mailbox &&
        wgpuPresentMode != WGPUPresentMode_Immediate) {
        return false;
    }

    // Present mode is valid.
    return true;
}

static bool WebGPU_SupportsSwapchainComposition(SDL_GPURenderer *driverData,
                                                SDL_Window *window,
                                                SDL_GPUSwapchainComposition swapchainComposition)
{
    (void)driverData;
    (void)window;

    // We *should* only support SDR for now, but HDR support has been added through canvas tonemapping
    // see: https://developer.chrome.com/blog/new-in-webgpu-129
    if (swapchainComposition != SDL_GPU_SWAPCHAINCOMPOSITION_SDR && swapchainComposition != SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR) {
        return false;
    }

    // Swapchain composition is valid.
    return true;
}

static bool WebGPU_INTERNAL_CreateFence(
    WebGPURenderer *renderer)
{
    WebGPUFence *fence = SDL_calloc(1, sizeof(WebGPUFence));
    SDL_SetAtomicInt(&fence->complete, 0);
    SDL_AtomicIncRef(&fence->referenceCount);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->availableFences,
        WebGPUFence *,
        renderer->availableFenceCount + 1,
        renderer->availableFenceCapacity,
        renderer->availableFenceCapacity * 2);

    renderer->availableFences[renderer->availableFenceCount++] = fence;

    return true;
}

static bool WebGPU_INTERNAL_AcquireFence(
    WebGPURenderer *renderer,
    WebGPUCommandBuffer *commandBuffer)
{
    WebGPUFence *fence;

    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == 0) {
        if (!WebGPU_INTERNAL_CreateFence(renderer)) {
            SDL_UnlockMutex(renderer->fenceLock);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create fence!");
            return false;
        }
    }

    fence = renderer->availableFences[renderer->availableFenceCount - 1];
    renderer->availableFenceCount -= 1;

    SDL_UnlockMutex(renderer->fenceLock);

    // Associate the fence with the command buffer
    commandBuffer->fence = fence;
    SDL_SetAtomicInt(&fence->complete, 0); // Reset the fence
    (void)SDL_AtomicIncRef(&fence->referenceCount);

    return true;
}

static void WebGPU_INTERNAL_FrameCallback(WGPUQueueWorkDoneStatus status, void *userdata1, void *userdata2)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)userdata1;
    SDL_SetAtomicInt(&webgpuCommandBuffer->fence->complete, 1);

    // Frame work is done! Present.
    for (Uint32 i = 0; i < webgpuCommandBuffer->windowDataCount; i++) {
        WebGPUWindowData *window = webgpuCommandBuffer->windowDatas[i];
        if (window == NULL)
            break;

        // Present the surface to the window!
        wgpuSurfacePresent(window->surface);
    }
}

static void WebGPU_INTERNAL_ParseBGL(
    BindGroupLayoutEntryInfo *bgl,
    const char *wgsl)
{

    // NOTE:
    // We have a critical problem where we have to create our pipeline layout
    // for a pipeline before any bindings are provided. If a texture is used,
    // the pipeline layout expects us to provide an associated TextureViewDimension.
    //
    // Unfortunately I can only think to reflect here without changing the shader init
    // process.
    //
    // We only look for the dimension associated with each texture*<*> provided in a shader.
    // Unfortunately this also makes SPIRV support near impossible.

    Uint32 countSampleType = 0;
    Uint32 countSampleViewDimension = 0;
    Uint32 countSamplerBindingType = 0;
    Uint32 countStorageTextureAccess = 0;
    Uint32 countStorageTextureViewDimension = 0;
    bool found;

    char *token;
    char *copy = SDL_stack_alloc(char, SDL_strlen(wgsl));
    SDL_strlcpy(copy, wgsl, SDL_strlen(wgsl));

    // Naive approach, strtok and then check tokens.
    while ((token = SDL_strtok_r(copy, "\n", &copy))) {
        found = false;

        // 1D textures are not supported in SDL3 GPU so they are ignored.
        if (SDL_strstr(token, "texture_2d<")) {
            SDL_Log("texture_2d: %s", token);
            bgl->sampleDimensions[countSampleViewDimension++] = WGPUTextureViewDimension_2D;
            found = true;
        } else if (SDL_strstr(token, " texture_2d_array<")) {
            SDL_Log("texture_2d_array: %s", token);
            bgl->sampleDimensions[countSampleViewDimension++] = WGPUTextureViewDimension_2DArray;
            found = true;
        } else if (SDL_strstr(token, " texture_3d<")) {
            SDL_Log("texture_3d: %s", token);
            bgl->sampleDimensions[countSampleViewDimension++] = WGPUTextureViewDimension_3D;
            found = true;
        } else if (SDL_strstr(token, " texture_cube<")) {
            SDL_Log("texture_cube: %s", token);
            bgl->sampleDimensions[countSampleViewDimension++] = WGPUTextureViewDimension_Cube;
            found = true;
        } else if (SDL_strstr(token, " texture_cube_array<")) {
            SDL_Log("texture_cube_array: %s", token);
            bgl->sampleDimensions[countSampleViewDimension++] = WGPUTextureViewDimension_CubeArray;
            found = true;
        } else if (SDL_strstr(token, " sampler;")) {
            SDL_Log("sampler: %s", token);
            bgl->sampleBindingType[countSamplerBindingType++] = WGPUSamplerBindingType_Filtering;
        } else if (SDL_strstr(token, " sampler_comparison;")) {
            SDL_Log("comparison sampler: %s", token);
            bgl->sampleBindingType[countSamplerBindingType++] = WGPUSamplerBindingType_Comparison;
        } else if (SDL_strstr(token, " texture_storage")) {
            SDL_Log("%s", token);
        }

        // If a texture-sampler pair is found, we need to take note of the type.
        if (found) {
            if (SDL_strstr(token, "<f32>")) {
                bgl->sampleTypes[countSampleType++] = WGPUTextureSampleType_Float;
            } else if (SDL_strstr(token, "<i32>")) {
                bgl->sampleTypes[countSampleType++] = WGPUTextureSampleType_Sint;
            } else if (SDL_strstr(token, "<u32>")) {
                bgl->sampleTypes[countSampleType++] = WGPUTextureSampleType_Uint;
            }
        }
    }

    SDL_stack_free(copy);
}

static WebGPUShader *WebGPU_INTERNAL_CompileShader(
    WebGPURenderer *renderer,
    SDL_GPUShaderFormat format,
    const void *code,
    size_t codeSize,
    const char *entrypoint)
{
    WGPUShaderModuleDescriptor shader_desc;
    WebGPUShader *shader = SDL_calloc(1, sizeof(WebGPUShader));

    if (format == SDL_GPU_SHADERFORMAT_WGSL) {
        const char *wgsl = (const char *)code;
        WGPUShaderSourceWGSL wgsl_desc = {
            .chain = {
                .sType = WGPUSType_ShaderSourceWGSL,
                .next = NULL,
            },
            .code = (WGPUStringView){
                .data = wgsl,
                .length = SDL_strlen(wgsl),
            },
        };

        // Set Shader BGLs by naively parsing the
        // wgsl shader. We have to do this to build
        // bind groups layout entries.
        WebGPU_INTERNAL_ParseBGL(&shader->bgl, wgsl);

        const char *label = "SDL_GPU WGSL Shader";
        shader_desc = (WGPUShaderModuleDescriptor){
            .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
            .label = { label, SDL_strlen(label) },
        };
    }
    // else if (format == SDL_GPU_SHADERFORMAT_SPIRV) {
    //     WGPUShaderSourceSPIRV spirv_desc = {
    //         .chain = {
    //             .sType = WGPUSType_ShaderSourceSPIRV,
    //             .next = NULL,
    //         },
    //         .code = (const Uint32 *)code,
    //         .codeSize = (Uint32)codeSize / 4,
    //     };

    //     const char *label = "SDL_GPU WebGPU SPIRV Shader";
    //     shader_desc = (WGPUShaderModuleDescriptor){
    //         .nextInChain = (WGPUChainedStruct *)&spirv_desc,
    //         .label = { label, SDL_strlen(label) },
    //     };
    // } else {
    //     SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU: Unsupported shader format %d", format);
    //     return NULL;
    // }

    shader->shaderModule = wgpuDeviceCreateShaderModule(renderer->device, &shader_desc);

    return shader;
}

static SDL_GPUShader *WebGPU_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *shaderCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a shader");
    SDL_assert(shaderCreateInfo && "Shader create info must not be NULL when creating a shader");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUShader *shader = WebGPU_INTERNAL_CompileShader(
        renderer,
        shaderCreateInfo->format,
        shaderCreateInfo->code,
        shaderCreateInfo->code_size,
        shaderCreateInfo->entrypoint);

    // Assign all necessary shader information
    shader->stage = shaderCreateInfo->stage;
    shader->samplerCount = shaderCreateInfo->num_samplers;
    shader->storageBufferCount = shaderCreateInfo->num_storage_buffers;
    shader->uniformBufferCount = shaderCreateInfo->num_uniform_buffers;
    shader->storageTextureCount = shaderCreateInfo->num_storage_textures;

    return (SDL_GPUShader *)shader;
}

static void WebGPU_ReleaseShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a shader");
    SDL_assert(shader && "Shader must not be NULL when destroying a shader");

    WebGPUShader *wgpuShader = (WebGPUShader *)shader;

    // Release the shader module
    wgpuShaderModuleRelease(wgpuShader->shaderModule);

    SDL_free(shader);
}

static void WebGPU_INTERNAL_DestroyTextureContainer(
    WebGPUTextureContainer *container)
{
    for (Uint32 i = 0; i < container->textureCount; i += 1) {
        if (container->textures[i]->handle) {
            SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Releasing texture");
            wgpuTextureRelease(container->textures[i]->handle);
        }
        container->textures[i]->handle = NULL;
        SDL_free(container->textures[i]);
    }
    if (container->debugName != NULL) {
        SDL_free(container->debugName);
    }
    SDL_free(container->textures);
    SDL_free(container);
}

static void WebGPU_INTERNAL_DestroyBufferContainer(
    WebGPUBufferContainer *container)
{
    for (Uint32 i = 0; i < container->bufferCount; i += 1) {
        container->buffers[i]->handle = NULL;
        SDL_free(container->buffers[i]);
    }
    if (container->debugName != NULL) {
        SDL_free(container->debugName);
    }
    SDL_free(container->buffers);
    SDL_free(container);
}

static void WebGPU_INTERNAL_PerformPendingDestroys(
    WebGPURenderer *renderer)
{
    Sint32 referenceCount = 0;
    Sint32 i;
    Uint32 j;

    for (i = renderer->bufferContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->bufferContainersToDestroy[i]->bufferCount; j += 1) {
            referenceCount += SDL_GetAtomicInt(&renderer->bufferContainersToDestroy[i]->buffers[j]->refCount);
        }

        if (referenceCount == 0) {
            WebGPU_INTERNAL_DestroyBufferContainer(
                renderer->bufferContainersToDestroy[i]);

            renderer->bufferContainersToDestroy[i] = renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount - 1];
            renderer->bufferContainersToDestroyCount -= 1;
        }
    }

    for (i = renderer->textureContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->textureContainersToDestroy[i]->textureCount; j += 1) {
            referenceCount += SDL_GetAtomicInt(&renderer->textureContainersToDestroy[i]->textures[j]->refCount);
        }

        if (referenceCount == 0) {
            WebGPU_INTERNAL_DestroyTextureContainer(
                renderer->textureContainersToDestroy[i]);

            renderer->textureContainersToDestroy[i] = renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount - 1];
            renderer->textureContainersToDestroyCount -= 1;
        }
    }
}

static void WebGPU_INTERNAL_ReleaseFenceToPool(
    WebGPURenderer *renderer,
    WebGPUFence *fence)
{
    SDL_LockMutex(renderer->fenceLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->availableFences,
        WebGPUFence *,
        renderer->availableFenceCount + 1,
        renderer->availableFenceCapacity,
        renderer->availableFenceCapacity * 2);

    renderer->availableFences[renderer->availableFenceCount++] = fence;

    SDL_UnlockMutex(renderer->fenceLock);
}

static void WebGPU_ReleaseFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    WebGPUFence *webgpuFence = (WebGPUFence *)fence;
    if (SDL_AtomicDecRef(&webgpuFence->referenceCount)) {
        WebGPU_INTERNAL_ReleaseFenceToPool(
            (WebGPURenderer *)driverData,
            webgpuFence);
    }
}

static void WebGPU_ReleaseTexture(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUTextureContainer *container = (WebGPUTextureContainer *)texture;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->textureContainersToDestroy,
        WebGPUTextureContainer *,
        renderer->textureContainersToDestroyCount + 1,
        renderer->textureContainersToDestroyCapacity,
        renderer->textureContainersToDestroyCapacity + 1);

    renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount++] = container;

    SDL_UnlockMutex(renderer->disposeLock);
}

static WebGPUBuffer *WebGPU_INTERNAL_CreateBuffer(
    WebGPURenderer *renderer,
    Uint32 size,
    WGPUBufferUsage usage,
    bool mappedAtCreation,
    const char *debugName)
{
    WebGPUBuffer *buffer = SDL_calloc(1, sizeof(WebGPUBuffer));
    if (!buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate WebGPUBuffer");
        return NULL;
    }

    const char *debugLabel = debugName ? debugName : "SDL_GPU WebGPU Buffer";
    WGPUStringView debug = {
        .data = debugLabel,
        .length = SDL_strlen(debugLabel)
    };

    WGPUBufferDescriptor desc = {
        .size = size,
        .usage = usage,
        .mappedAtCreation = mappedAtCreation,
        .label = debug,
    };

    buffer->handle = wgpuDeviceCreateBuffer(renderer->device, &desc);
    if (!buffer->handle) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create WGPU buffer");
        SDL_free(buffer);
        return NULL;
    }

    buffer->size = size;
    buffer->isMapped = mappedAtCreation;
    if (mappedAtCreation) {
        buffer->mappedData = wgpuBufferGetMappedRange(buffer->handle, 0, size);
        if (!buffer->mappedData) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to get mapped range for buffer");
            wgpuBufferDestroy(buffer->handle);
            SDL_free(buffer);
            return NULL;
        }
    }
    SDL_SetAtomicInt(&buffer->refCount, 0);
    buffer->debugName = debugName ? SDL_strdup(debugName) : NULL;

    return buffer;
}

// Prepare buffer for use, cycling if needed
static WebGPUBuffer *WebGPU_INTERNAL_PrepareBufferForUse(
    WebGPURenderer *renderer,
    WebGPUBufferContainer *container,
    bool cycle,
    WGPUBufferUsage usage)
{
    if (!cycle || SDL_GetAtomicInt(&container->activeBuffer->refCount) == 0) {
        return container->activeBuffer;
    }

    // Find an unused buffer
    for (Uint32 i = 0; i < container->bufferCount; i++) {
        if (SDL_GetAtomicInt(&container->buffers[i]->refCount) == 0) {
            container->activeBuffer = container->buffers[i];
            return container->activeBuffer;
        }
    }

    // Create a new buffer if all are in use
    EXPAND_ARRAY_IF_NEEDED(
        container->buffers,
        WebGPUBuffer *,
        container->bufferCount + 1,
        container->bufferCapacity,
        container->bufferCapacity + 1);

    container->buffers[container->bufferCount] = WebGPU_INTERNAL_CreateBuffer(
        renderer, container->size, usage, false, container->debugName);
    if (!container->buffers[container->bufferCount]) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create new buffer for cycling");
        return NULL;
    }
    container->bufferCount += 1;
    container->activeBuffer = container->buffers[container->bufferCount - 1];
    return container->activeBuffer;
}

static void WebGPU_SetBufferName(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer,
    const char *text)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUBufferContainer *container = (WebGPUBufferContainer *)buffer;

    if (renderer->debugMode && text != NULL) {
        if (container->debugName != NULL) {
            SDL_free(container->debugName);
        }

        container->debugName = SDL_strdup(text);
        for (Uint32 i = 0; i < container->bufferCount; i += 1) {
            // wgpuBufferSetLabel(
            //     container->buffers[i]->handle,
            //     (WGPUStringView){ text, SDL_strlen(text) }
            // );
        }
    }
}

static SDL_GPUBuffer *WebGPU_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usage,
    Uint32 size,
    const char *debugName)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUBufferContainer *container = SDL_calloc(1, sizeof(WebGPUBufferContainer));
    if (!container) {
        SDL_Log("Failed to allocate WebGPUBufferContainer");
        return NULL;
    }

    WGPUBufferUsage wgpuUsage = WGPUBufferUsage_CopyDst;
    wgpuUsage = SDLToWGPUBufferUsageFlags(usage);

    container->size = size;
    container->bufferCapacity = 1;
    container->bufferCount = 1;
    container->buffers = SDL_calloc(1, sizeof(WebGPUBuffer *));
    container->isPrivate = true;
    container->isWriteOnly = false;
    container->debugName = debugName ? SDL_strdup(debugName) : NULL;

    container->buffers[0] = WebGPU_INTERNAL_CreateBuffer(renderer, size, wgpuUsage, false, debugName);
    if (!container->buffers[0]) {
        SDL_free(container->buffers);
        SDL_free(container->debugName);
        SDL_free(container);
        return NULL;
    }
    container->activeBuffer = container->buffers[0];

    return (SDL_GPUBuffer *)container;
}

static void WebGPU_ReleaseBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUBufferContainer *container = (WebGPUBufferContainer *)buffer;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->bufferContainersToDestroy,
        WebGPUBufferContainer *,
        renderer->bufferContainersToDestroyCount + 1,
        renderer->bufferContainersToDestroyCapacity,
        renderer->bufferContainersToDestroyCapacity + 1);

    renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount++] = container;

    SDL_UnlockMutex(renderer->disposeLock);
}

static SDL_GPUTransferBuffer *WebGPU_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage,
    Uint32 size,
    const char *debugName)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUBufferContainer *container = SDL_calloc(1, sizeof(WebGPUBufferContainer));
    if (!container) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate WebGPUBufferContainer");
        return NULL;
    }

    WGPUBufferUsage wgpuUsage;
    bool isWriteOnly = (usage == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
    if (isWriteOnly) {
        wgpuUsage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    } else {
        wgpuUsage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    }

    container->size = size;
    container->bufferCapacity = 1;
    container->bufferCount = 1;
    container->buffers = SDL_calloc(1, sizeof(WebGPUBuffer *));
    container->isPrivate = false;
    container->isWriteOnly = isWriteOnly;
    container->debugName = debugName ? SDL_strdup(debugName) : NULL;

    container->buffers[0] = WebGPU_INTERNAL_CreateBuffer(renderer, size, wgpuUsage, false, debugName);
    if (!container->buffers[0]) {
        SDL_free(container->buffers);
        SDL_free(container->debugName);
        SDL_free(container);
        return NULL;
    }
    container->activeBuffer = container->buffers[0];

    return (SDL_GPUTransferBuffer *)container;
}

static void WebGPU_ReleaseTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    WebGPU_ReleaseBuffer(driverData, (SDL_GPUBuffer *)transferBuffer);
}

static void *WebGPU_MapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer,
    bool cycle)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUBufferContainer *container = (WebGPUBufferContainer *)transferBuffer;

    if (container->isPrivate) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Cannot map GPU-only buffer");
        return NULL;
    }

    WGPUBufferUsage usage = container->isWriteOnly ? (WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc) : (WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst);

    // For upload buffers in initialization, assume mappedAtCreation
    if (container->isWriteOnly && !container->activeBuffer->isMapped) {
        // Recreate buffer if not mapped
        WebGPUBuffer *newBuffer = WebGPU_INTERNAL_CreateBuffer(
            renderer, container->size, usage, true, container->debugName);
        if (!newBuffer) {
            return NULL;
        }
        if (cycle && SDL_GetAtomicInt(&container->activeBuffer->refCount) > 0) {
            EXPAND_ARRAY_IF_NEEDED(
                container->buffers,
                WebGPUBuffer *,
                container->bufferCount + 1,
                container->bufferCapacity,
                container->bufferCapacity + 1);
            container->buffers[container->bufferCount++] = newBuffer;
        } else {
            wgpuBufferDestroy(container->activeBuffer->handle);
            SDL_free(container->activeBuffer);
            container->buffers[0] = newBuffer;
        }
        container->activeBuffer = newBuffer;
    }

    WebGPUBuffer *buffer = container->activeBuffer;
    if (!buffer->isMapped) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Download buffers not yet supported in this context");
        return NULL; // For downloads, we need async mapping, not yet fixed
    }

    return buffer->mappedData;
}

static void WebGPU_UnmapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    WebGPUBufferContainer *container = (WebGPUBufferContainer *)transferBuffer;
    WebGPUBuffer *buffer = container->activeBuffer;

    if (!buffer->isMapped) {
        return;
    }

    wgpuBufferUnmap(buffer->handle);
    buffer->isMapped = false;
}

static void WebGPU_BeginCopyPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    if (webgpuCommandBuffer->copyEncoder) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Copy pass already active, resetting");
        wgpuCommandEncoderRelease(webgpuCommandBuffer->copyEncoder);
    }
    webgpuCommandBuffer->copyEncoder = wgpuDeviceCreateCommandEncoder(webgpuCommandBuffer->renderer->device, NULL);
    webgpuCommandBuffer->commandBuffer = NULL; // Reset to allow new submission
}

static void WebGPU_EndCopyPass(SDL_GPUCommandBuffer *commandBuffer)
{
    (void)commandBuffer;
    // No need to do anything here, everything is handled in Submit for WGPU
}

static void WebGPU_UploadToBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUTransferBufferLocation *source,
    const SDL_GPUBufferRegion *destination,
    bool cycle)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBufferContainer *srcContainer = (WebGPUBufferContainer *)source->transfer_buffer;
    WebGPUBufferContainer *dstContainer = (WebGPUBufferContainer *)destination->buffer;

    if (!webgpuCommandBuffer->copyEncoder) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "No active copy pass");
        return;
    }

    WebGPUBuffer *srcBuffer = WebGPU_INTERNAL_PrepareBufferForUse(
        webgpuCommandBuffer->renderer, srcContainer, cycle,
        WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc);
    WebGPUBuffer *dstBuffer = dstContainer->activeBuffer;

    if (srcBuffer->isMapped) {
        WebGPU_UnmapTransferBuffer((SDL_GPURenderer *)webgpuCommandBuffer->renderer,
                                   (SDL_GPUTransferBuffer *)srcContainer);
    }

    wgpuCommandEncoderCopyBufferToBuffer(
        webgpuCommandBuffer->copyEncoder,
        srcBuffer->handle, source->offset,
        dstBuffer->handle, destination->offset,
        destination->size);

    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, srcBuffer);
    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, dstBuffer);
}

static void WebGPU_CopyBufferToBuffer(SDL_GPUCommandBuffer *commandBuffer,
                                      const SDL_GPUBufferLocation *source,
                                      const SDL_GPUBufferLocation *destination,
                                      Uint32 size,
                                      bool cycle)
{
}

static void WebGPU_DownloadFromBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUBufferRegion *source,
    const SDL_GPUTransferBufferLocation *destination)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBufferContainer *srcContainer = (WebGPUBufferContainer *)source->buffer;
    WebGPUBufferContainer *dstContainer = (WebGPUBufferContainer *)destination->transfer_buffer;

    if (!webgpuCommandBuffer->copyEncoder) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "No active copy pass");
        return;
    }

    WebGPUBuffer *srcBuffer = srcContainer->activeBuffer;
    WebGPUBuffer *dstBuffer = dstContainer->activeBuffer;

    if (dstBuffer->isMapped) {
        WebGPU_UnmapTransferBuffer((SDL_GPURenderer *)webgpuCommandBuffer->renderer,
                                   (SDL_GPUTransferBuffer *)dstContainer);
    }

    wgpuCommandEncoderCopyBufferToBuffer(
        webgpuCommandBuffer->copyEncoder,
        srcBuffer->handle, source->offset,
        dstBuffer->handle, destination->offset,
        source->size);

    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, srcBuffer);
    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, dstBuffer);

    // Update lastFence for download synchronization
    if (dstContainer->lastFence) {
        SDL_AtomicDecRef(&dstContainer->lastFence->referenceCount);
    }
    dstContainer->lastFence = webgpuCommandBuffer->fence;
    SDL_AtomicIncRef(&dstContainer->lastFence->referenceCount);
}

static void WebGPU_BindVertexBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    const SDL_GPUBufferBinding *bindings,
    Uint32 numBindings)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    if (!webgpuCommandBuffer->renderEncoder) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "No active render encoder for binding vertex buffers");
        return;
    }

    if (numBindings == 0) {
        return;
    }

    if (firstSlot + numBindings > MAX_VERTEX_BUFFERS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vertex buffer binding exceeds max slots: %u + %u > %u",
                    firstSlot, numBindings, MAX_VERTEX_BUFFERS);
        return;
    }

    for (Uint32 i = 0; i < numBindings; i++) {
        WebGPUBuffer *buffer = ((WebGPUBufferContainer *)bindings[i].buffer)->activeBuffer;
        if (!buffer || !buffer->handle) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Invalid buffer at binding slot %u", firstSlot + i);
            continue;
        }

        wgpuRenderPassEncoderSetVertexBuffer(
            webgpuCommandBuffer->renderEncoder,
            firstSlot + i,                                         // slot
            buffer->handle,                                        // buffer
            bindings[i].offset,                                    // offset
            wgpuBufferGetSize(buffer->handle) - bindings[i].offset // size (remaining)
        );

        WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, buffer);
    }
}

static void WebGPU_BindIndexBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUBufferBinding *binding,
    SDL_GPUIndexElementSize indexElementSize)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    webgpuCommandBuffer->indexBuffer = ((WebGPUBufferContainer *)binding->buffer)->activeBuffer;
    webgpuCommandBuffer->indexBufferOffset = binding->offset;
    webgpuCommandBuffer->index_element_size = indexElementSize;
    WGPUIndexFormat indexFormat = SDLToWGPUIndexFormat(indexElementSize);

    wgpuRenderPassEncoderSetIndexBuffer(
        webgpuCommandBuffer->renderEncoder,
        webgpuCommandBuffer->indexBuffer->handle,
        indexFormat,
        (Uint64)webgpuCommandBuffer->indexBufferOffset,
        (Uint64)webgpuCommandBuffer->indexBuffer->size);

    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, webgpuCommandBuffer->indexBuffer);
}

static void WebGPU_BindFragmentSamplers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    const SDL_GPUTextureSamplerBinding *textureSamplerBindings,
    Uint32 numBindings)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUTextureContainer *textureContainer;
    WebGPUSampler *sampler;

    for (Uint32 i = 0; i < numBindings; i += 1) {
        textureContainer = (WebGPUTextureContainer *)textureSamplerBindings[i].texture;
        sampler = (WebGPUSampler *)textureSamplerBindings[i].sampler;

        if (webgpuCommandBuffer->fragmentSamplers[firstSlot + i] != sampler->handle) {
            webgpuCommandBuffer->fragmentSamplers[firstSlot + i] = sampler->handle;
            webgpuCommandBuffer->needFragmentSamplerBind = true;
        }

        if (webgpuCommandBuffer->fragmentTextures[firstSlot + i] != textureContainer->activeTexture->handle) {
            WebGPU_INTERNAL_TrackTexture(
                webgpuCommandBuffer,
                textureContainer->activeTexture);

            webgpuCommandBuffer->fragmentTextures[firstSlot + i] =
                textureContainer->activeTexture->handle;

            webgpuCommandBuffer->needFragmentSamplerBind = true;
        }
    }
}

static WebGPUUniformBuffer *WebGPU_INTERNAL_CreateUniformBuffer(WebGPURenderer *renderer,
                                                                Uint32 size)
{
    WebGPUUniformBuffer *uniformBuffer = SDL_calloc(1, sizeof(WebGPUUniformBuffer));
    WGPUBufferUsage usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    const char *labelData = "SDL_GPU WebGPU Uniform Buffer";
    WGPUStringView label = {
        .data = labelData,
        .length = SDL_strlen(labelData),
    };

    uniformBuffer->buffer = wgpuDeviceCreateBuffer(renderer->device, &(WGPUBufferDescriptor){
                                                                         .size = size,
                                                                         .usage = usage,
                                                                         .mappedAtCreation = false,
                                                                         .label = label,
                                                                     });
    uniformBuffer->drawOffset = 0;
    uniformBuffer->writeOffset = 0;
    return uniformBuffer;
}

static void WebGPU_SetTextureName(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *buffer,
    const char *text)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUTextureContainer *container = (WebGPUTextureContainer *)buffer;

    if (renderer->debugMode && text != NULL) {
        if (container->debugName != NULL) {
            SDL_free(container->debugName);
        }

        container->debugName = SDL_strdup(text);
        for (Uint32 i = 0; i < container->textureCount; i += 1) {
            // wgpuTextureSetLabel(
            //     container->textures[i]->handle,
            //     (WGPUStringView){ text, SDL_strlen(text) });
        }
    }
}

static WebGPUTexture *WebGPU_INTERNAL_CreateTexture(
    WebGPURenderer *renderer,
    const SDL_GPUTextureCreateInfo *createInfo)
{
    WGPUTextureDescriptor desc;
    WGPUTexture texture;
    WebGPUTexture *webgpuTexture;

    // We don't worry about 2D MSAA since this is different from Metal
    desc.dimension = SDLToWGPUTextureDimension(createInfo->type);
    desc.format = SDLToWGPUTextureFormat(createInfo->format);
    if (createInfo->format == SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM) {
        SET_STRING_ERROR_AND_RETURN("SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM is not supported", NULL);
    }
    desc.viewFormatCount = 0;
    desc.viewFormats = NULL;
    desc.nextInChain = NULL;
    desc.size.width = createInfo->width;
    desc.size.height = createInfo->height;
    const char *str = "SDL_GPU Texture";
    desc.label = (WGPUStringView){
        .data = str,
        .length = SDL_strlen(str)
    };

    // Fix depth/array handling for different texture types
    if (createInfo->type == SDL_GPU_TEXTURETYPE_3D) {
        desc.size.depthOrArrayLayers = createInfo->layer_count_or_depth;
    } else if (createInfo->type == SDL_GPU_TEXTURETYPE_2D_ARRAY ||
               createInfo->type == SDL_GPU_TEXTURETYPE_CUBE ||
               createInfo->type == SDL_GPU_TEXTURETYPE_CUBE_ARRAY) {
        desc.size.depthOrArrayLayers = createInfo->layer_count_or_depth;
    } else {
        desc.size.depthOrArrayLayers = 1;
    }

    desc.mipLevelCount = createInfo->num_levels;

    // Handle sample count - special case for depth/stencil formats
    if (IsDepthFormat(createInfo->format) || IsStencilFormat(createInfo->format)) {
        desc.sampleCount = 1; // Force non-multisampled for depth-stencil
        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Creating depth/stencil texture: format=%d, width=%d, height=%d, usage=%d",
                    createInfo->format, createInfo->width, createInfo->height, createInfo->usage);
    } else {
        desc.sampleCount = SDLToWGPUSampleCount(createInfo->sample_count);
    }

    // Set up usage flags
    desc.usage = 0;
    desc.usage = SDLToWGPUTextureUsageFlags(createInfo->usage, createInfo->format, createInfo->type);

    // Debug log texture creation params
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Creating texture: format=%d, dimension=%d, width=%d, height=%d, depth/layers=%d, usage=0x%lx",
                desc.format, desc.dimension, desc.size.width, desc.size.height,
                desc.size.depthOrArrayLayers, desc.usage);

    texture = wgpuDeviceCreateTexture(renderer->device, &desc);
    if (!texture) {
        SET_STRING_ERROR_AND_RETURN("Failed to create texture", NULL);
    }

    webgpuTexture = SDL_calloc(1, sizeof(WebGPUTexture));
    webgpuTexture->handle = texture;
    SDL_SetAtomicInt(&webgpuTexture->refCount, 0);

    return webgpuTexture;
}

static SDL_GPUTexture *WebGPU_CreateTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *createInfo)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUTextureContainer *container;
    WebGPUTexture *texture;

    texture = WebGPU_INTERNAL_CreateTexture(renderer, createInfo);
    if (!texture) {
        SET_STRING_ERROR_AND_RETURN("Failed to create texture", NULL);
    }

    container = SDL_calloc(1, sizeof(WebGPUTextureContainer));
    container->canBeCycled = 1;

    // Copy properties so we don't lose information when the client destroys them
    container->header.info = *createInfo;
    container->header.info.props = SDL_CreateProperties();
    SDL_CopyProperties(createInfo->props, container->header.info.props);

    container->activeTexture = texture;
    container->textureCapacity = 1;
    container->textureCount = 1;

    container->textures = SDL_calloc(1, sizeof(WebGPUTexture *));
    container->textures[0] = texture;
    container->debugName = NULL;

    if (SDL_HasProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING)) {
        container->debugName = SDL_strdup(SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, NULL));
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Created texture");

    return (SDL_GPUTexture *)container;
}

static WebGPUTexture *WebGPU_INTERNAL_PrepareTextureForWrite(
    WebGPURenderer *renderer,
    WebGPUTextureContainer *container,
    bool cycle)
{
    Uint32 i;
    WebGPUWindowData *windowData = renderer->claimedWindows[0];

    if (!windowData) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "windowData is NULL in PrepareTextureForWrite");
        return NULL;
    }

    if (!container) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Texture container is NULL (frame %u)", windowData->frameCounter);
        return NULL;
    }

    if (!container->textures || !container->activeTexture) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Texture container has null textures or activeTexture (frame %u)", windowData->frameCounter);
        return NULL;
    }

    if (!container->activeTexture->handle) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Active texture handle is NULL (frame %u), attempting recovery", windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        if (!container->activeTexture->handle) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to recover texture handle (frame %u)", windowData->frameCounter);
            return NULL;
        }
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "PrepareTextureForWrite - windowData: %p, frameCounter: %u, Container: %p, textures: %p, activeTexture: %p, handle: %p, refCount: %d",
                 windowData, windowData->frameCounter, container, container->textures, container->activeTexture,
                 container->activeTexture->handle, SDL_GetAtomicInt(&container->activeTexture->refCount));
    if (cycle && container->canBeCycled) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Cycling texture");

        for (i = 0; i < container->textureCount; i += 1) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Checking texture %d", i);

            if (!container->textures[i]) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Texture at index %d is NULL", i);
                continue;
            }

            if (SDL_GetAtomicInt(&container->textures[i]->refCount) == 0) {
                SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Found texture %d", i);
                container->activeTexture = container->textures[i];
                return container->activeTexture;
            }
        }

        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "No free textures found, creating a new one");

        EXPAND_ARRAY_IF_NEEDED(
            container->textures,
            WebGPUTexture *,
            container->textureCount + 1,
            container->textureCapacity,
            container->textureCapacity * 2);

        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Expanded array");

        container->textures[container->textureCount] = WebGPU_INTERNAL_CreateTexture(
            renderer,
            &container->header.info);
        if (!container->textures[container->textureCount]) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create new texture");
            return container->activeTexture; // Return current texture instead of crashing
        }
        container->textureCount += 1;

        container->activeTexture = container->textures[container->textureCount - 1];
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Created new active texture %p", container->activeTexture);
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Returning activeTexture: %p", container->activeTexture);
    return container->activeTexture;
}

static void WebGPU_UploadToTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUTextureTransferInfo *source,
    const SDL_GPUTextureRegion *destination,
    bool cycle)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;
    WebGPUBufferContainer *bufferContainer = (WebGPUBufferContainer *)source->transfer_buffer;
    WebGPUTextureContainer *textureContainer = (WebGPUTextureContainer *)destination->texture;

    WebGPUTexture *webgpuTexture = WebGPU_INTERNAL_PrepareTextureForWrite(renderer, textureContainer, cycle);

    SDL_GPUTextureFormat format = textureContainer->header.info.format;
    Uint32 blockHeight = SDL_max(Texture_GetBlockHeight(format), 1);
    Uint32 blocksPerColumn = (destination->h + blockHeight - 1) / blockHeight;
    Uint32 bytesPerRow = BytesPerRow(destination->w, textureContainer->header.info.format);

    WGPUTexelCopyBufferLayout layout = {
        .offset = source->offset,
        .bytesPerRow = bytesPerRow,
        .rowsPerImage = blocksPerColumn,
    };

    WGPUTexelCopyTextureInfo info = {
        .texture = textureContainer->activeTexture->handle,
        .mipLevel = destination->mip_level,
        .aspect = WGPUTextureAspect_All,
        .origin = {
            .x = destination->x,
            .y = destination->y,
            .z = destination->z,
        }
    };

    WGPUExtent3D extent = {
        .width = destination->w,
        .height = destination->h,
        .depthOrArrayLayers = destination->d,
    };

    if (bytesPerRow >= 256 && bytesPerRow % 256 == 0) {
        wgpuCommandEncoderCopyBufferToTexture(
            webgpuCommandBuffer->copyEncoder,
            &(const WGPUTexelCopyBufferInfo){
                .buffer = bufferContainer->activeBuffer->handle,
                .layout = layout,
            },
            &info, &extent);
    } else {
        SDL_Log("HERE");
        wgpuQueueWriteTexture(
            renderer->queue,
            &info,
            bufferContainer->activeBuffer->mappedData,
            bufferContainer->size,
            &layout,
            &extent);
    }

    WebGPU_INTERNAL_TrackTexture(webgpuCommandBuffer, webgpuTexture);
    WebGPU_INTERNAL_TrackBuffer(webgpuCommandBuffer, bufferContainer->activeBuffer);
}

static SDL_GPUSampler *WebGPU_CreateSampler(
    SDL_GPURenderer *driverData,
    const SDL_GPUSamplerCreateInfo *createinfo)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUSampler *sampler = SDL_calloc(1, sizeof(WebGPUSampler));
    if (!sampler) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate GPU Sampler");
    }

    const char *label = "SDL_GPU Sampler";
    WGPUSamplerDescriptor samplerDesc = {
        .label = { label, SDL_strlen(label) },
        .addressModeU = SDLToWGPUAddressMode(createinfo->address_mode_u),
        .addressModeV = SDLToWGPUAddressMode(createinfo->address_mode_v),
        .addressModeW = SDLToWGPUAddressMode(createinfo->address_mode_w),
        .magFilter = SDLToWGPUFilterMode(createinfo->mag_filter),
        .minFilter = SDLToWGPUFilterMode(createinfo->min_filter),
        .mipmapFilter = SDLToWGPUSamplerMipmapMode(createinfo->mipmap_mode),
        .lodMinClamp = createinfo->min_lod,
        .lodMaxClamp = createinfo->max_lod,
        .compare = SDLToWGPUCompareFunction(createinfo->compare_op),
        .maxAnisotropy = 1,
    };

    sampler->handle = wgpuDeviceCreateSampler(renderer->device, &samplerDesc);
    if (sampler->handle == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create sampler");
        SDL_free(sampler);
        SDL_OutOfMemory();
        return NULL;
    }

    return (SDL_GPUSampler *)sampler;
}

static void WebGPU_ReleaseSampler(
    SDL_GPURenderer *driverData,
    SDL_GPUSampler *sampler)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a sampler");
    SDL_assert(sampler && "Sampler must not be NULL when destroying a sampler");

    WebGPUSampler *webgpuSampler = (WebGPUSampler *)sampler;

    wgpuSamplerRelease(webgpuSampler->handle);
    SDL_free(webgpuSampler);
}

static WebGPUUniformBuffer *WebGPU_INTERNAL_AcquireUniformBufferFromPool(
    WebGPUCommandBuffer *commandBuffer)
{
    WebGPURenderer *renderer = commandBuffer->renderer;
    WebGPUUniformBuffer *uniformBuffer;

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    if (renderer->uniformBufferPoolCount > 0) {
        uniformBuffer = renderer->uniformBufferPool[renderer->uniformBufferPoolCount - 1];
        renderer->uniformBufferPoolCount -= 1;
    } else {
        uniformBuffer = WebGPU_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    WebGPU_INTERNAL_TrackUniformBuffer(commandBuffer, uniformBuffer);

    return uniformBuffer;
}

static void WebGPU_INTERNAL_ReturnUniformBufferToPool(
    WebGPURenderer *renderer,
    WebGPUUniformBuffer *uniformBuffer)
{
    if (renderer->uniformBufferPoolCount >= renderer->uniformBufferPoolCapacity) {
        renderer->uniformBufferPoolCapacity *= 2;
        renderer->uniformBufferPool = SDL_realloc(
            renderer->uniformBufferPool,
            renderer->uniformBufferPoolCapacity * sizeof(WebGPUUniformBuffer *));
    }

    renderer->uniformBufferPool[renderer->uniformBufferPoolCount] = uniformBuffer;
    renderer->uniformBufferPoolCount += 1;

    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;
}

// When building a graphics pipeline, we need to create the VertexState which is comprised of a shader module, an entry,
// and vertex buffer layouts. Using the existing SDL_GPUVertexInputState, we can create the vertex buffer layouts and
// pass them to the WGPUVertexState.
static WGPUVertexBufferLayout *WebGPU_INTERNAL_CreateVertexBufferLayouts(const SDL_GPUVertexInputState *vertexInputState)
{
    if (vertexInputState == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vertex input state must not be NULL when creating vertex buffer layouts");
        return NULL;
    }

    // Allocate memory for the vertex buffer layouts if needed.
    // Otherwise, early return NULL if there are no vertex buffers to create layouts for.
    WGPUVertexBufferLayout *vertexBufferLayouts;
    if (vertexInputState->num_vertex_buffers != 0) {
        vertexBufferLayouts = SDL_calloc(vertexInputState->num_vertex_buffers, sizeof(WGPUVertexBufferLayout));
        if (vertexBufferLayouts == NULL) {
            SDL_OutOfMemory();
            return NULL;
        }
    } else {
        return NULL;
    }

    // Iterate through the vertex attributes and build the WGPUVertexAttribute array.
    // We also determine where each attribute belongs. This is used to build the vertex buffer layouts.
    WGPUVertexAttribute *attributes = SDL_calloc(vertexInputState->num_vertex_attributes, sizeof(WGPUVertexAttribute));
    Uint32 attribute_buffer_indices[vertexInputState->num_vertex_attributes];
    for (Uint32 i = 0; i < vertexInputState->num_vertex_attributes; i += 1) {
        const SDL_GPUVertexAttribute *vertexAttribute = &vertexInputState->vertex_attributes[i];
        attributes[i] = (WGPUVertexAttribute){
            .format = SDLToWGPUVertexFormat(vertexAttribute->format),
            .offset = vertexAttribute->offset,
            .shaderLocation = vertexAttribute->location,
        };
        attribute_buffer_indices[i] = vertexAttribute->buffer_slot;
    }

    // Iterate through the vertex buffers and build the WGPUVertexBufferLayouts using our attributes array.
    for (Uint32 i = 0; i < vertexInputState->num_vertex_buffers; i += 1) {
        Uint32 numAttributes = 0;
        // Not incredibly efficient but for now this will build the attributes for each vertex buffer
        for (Uint32 j = 0; j < vertexInputState->num_vertex_attributes; j += 1) {
            if (attribute_buffer_indices[j] == i) {
                numAttributes += 1;
            }
        }

        // Build the vertex buffer layout for the current vertex buffer using the attributes list (can be NULL)
        // This is then passed to the vertex state for the render pipeline
        const SDL_GPUVertexBufferDescription *vertexBuffer = &vertexInputState->vertex_buffer_descriptions[i];
        vertexBufferLayouts[i] = (WGPUVertexBufferLayout){
            .arrayStride = vertexBuffer->pitch,
            .stepMode = SDLToWGPUInputStepMode(vertexBuffer->input_rate),
            .attributeCount = numAttributes,
            .attributes = attributes,
        };
    }

    // Return a pointer to the head of the vertex buffer layouts
    return vertexBufferLayouts;
}

static SDL_GPUGraphicsPipeline *WebGPU_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUGraphicsPipelineCreateInfo *createinfo)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUShader *vertexShader = (WebGPUShader *)createinfo->vertex_shader;
    WebGPUShader *fragmentShader = (WebGPUShader *)createinfo->fragment_shader;
    WebGPUGraphicsPipeline *result = NULL;

    // Step 1: Create Pipeline Layout
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "WebGPU: Creating pipeline layout");

    // Get our bind group layout entry props from the shader info
    // This sucks and breaks if the bindings are not in order.
    //
    // TODO: Update to check against group and binding number instead
    // of making assumption on order binding order.
    BindGroupLayoutEntryInfo *vertexBGL = &vertexShader->bgl;

    // Bind Group 0: Vertex - Sampled Textures (TEXTURE then SAMPLER), Storage Textures, Storage Buffers
    Uint32 bindGroup0Total = (vertexShader->samplerCount * 2) + vertexShader->storageTextureCount + vertexShader->storageBufferCount;
    WGPUBindGroupLayoutEntry *bg0Entries = SDL_calloc(bindGroup0Total, sizeof(WGPUBindGroupLayoutEntry));
    Uint32 bindingIndex = 0;

    // Texture-Sampler Pairs
    for (Uint32 i = 0; i < vertexShader->samplerCount; i++) {
        // TEXTURE
        bg0Entries[bindingIndex].binding = bindingIndex; // e.g., 0, 2, 4...
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Vertex;
        bg0Entries[bindingIndex].texture.sampleType = vertexBGL->sampleTypes[i];
        bg0Entries[bindingIndex].texture.viewDimension = vertexBGL->sampleDimensions[i];
        bindingIndex++;

        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Texture Sample Type: %u, Dim: %u, Sampler Binding Type: %u", vertexBGL->sampleTypes[i], vertexBGL->sampleDimensions[i], vertexBGL->sampleBindingType[i]);

        // SAMPLER
        bg0Entries[bindingIndex].binding = bindingIndex; // e.g., 1, 3, 5...
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Vertex;
        bg0Entries[bindingIndex].sampler.type = vertexBGL->sampleBindingType[i];
        bindingIndex++;
    }
    // Storage Textures
    for (Uint32 i = 0; i < vertexShader->storageTextureCount; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Vertex;
        bg0Entries[bindingIndex].storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
        bg0Entries[bindingIndex].storageTexture.format = WGPUTextureFormat_RGBA8Unorm; // Adjust as needed
        bg0Entries[bindingIndex].storageTexture.viewDimension = WGPUTextureViewDimension_2D;
        bindingIndex++;
    }
    // Storage Buffers
    for (Uint32 i = 0; i < vertexShader->storageBufferCount; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Vertex;
        bg0Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Storage;
        bindingIndex++;
    }

    // Bind Group 1: Vertex - Uniform Buffers
    Uint32 bindGroup1Total = vertexShader->uniformBufferCount;
    WGPUBindGroupLayoutEntry *bg1Entries = SDL_calloc(bindGroup1Total, sizeof(WGPUBindGroupLayoutEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < vertexShader->uniformBufferCount; i++) {
        bg1Entries[bindingIndex].binding = bindingIndex;
        bg1Entries[bindingIndex].visibility = WGPUShaderStage_Vertex;
        bg1Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Uniform;
        bg1Entries[bindingIndex].buffer.hasDynamicOffset = false;
        bindingIndex++;
    }

    // Bind Group 2: Fragment - Similar to Bind Group 0 but with fragment visibility
    Uint32 bindGroup2Total = (fragmentShader->samplerCount * 2) + fragmentShader->storageTextureCount + fragmentShader->storageBufferCount;
    WGPUBindGroupLayoutEntry *bg2Entries = SDL_calloc(bindGroup2Total, sizeof(WGPUBindGroupLayoutEntry));
    BindGroupLayoutEntryInfo *fragBGL = &fragmentShader->bgl;
    bindingIndex = 0;
    for (Uint32 i = 0; i < fragmentShader->samplerCount; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].visibility = WGPUShaderStage_Fragment;
        bg2Entries[bindingIndex].texture.sampleType = fragBGL->sampleTypes[i];
        bg2Entries[bindingIndex].texture.viewDimension = fragBGL->sampleDimensions[i];
        bindingIndex++;

        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Texture Sample Type: %u, Dim: %u, Sampler Binding Type: %u", fragBGL->sampleTypes[i], fragBGL->sampleDimensions[i], fragBGL->sampleBindingType[i]);
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].visibility = WGPUShaderStage_Fragment;
        bg2Entries[bindingIndex].sampler.type = fragBGL->sampleBindingType[i];
        bindingIndex++;
    }
    // Storage Textures
    for (Uint32 i = 0; i < fragmentShader->storageTextureCount; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].visibility = WGPUShaderStage_Fragment;
        bg2Entries[bindingIndex].storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
        bg2Entries[bindingIndex].storageTexture.format = WGPUTextureFormat_RGBA8Unorm; // Adjust as needed
        bg2Entries[bindingIndex].storageTexture.viewDimension = WGPUTextureViewDimension_2D;
        bindingIndex++;
    }
    // Storage Buffers
    for (Uint32 i = 0; i < fragmentShader->storageBufferCount; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].visibility = WGPUShaderStage_Fragment;
        bg2Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Storage;
        bindingIndex++;
    }

    // Bind Group 3: Fragment - Uniform Buffers
    Uint32 bindGroup3Total = fragmentShader->uniformBufferCount;
    WGPUBindGroupLayoutEntry *bg3Entries = SDL_calloc(bindGroup3Total, sizeof(WGPUBindGroupLayoutEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < fragmentShader->uniformBufferCount; i++) {
        bg3Entries[bindingIndex].binding = bindingIndex;
        bg3Entries[bindingIndex].visibility = WGPUShaderStage_Fragment;
        bg3Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Uniform;
        bg3Entries[bindingIndex].buffer.hasDynamicOffset = false;
        bindingIndex++;
    }

    WGPUDevice device = renderer->device;
    const char *labels[4] = {
        "BG 0 Vertex: Sampled Textures, Storage Textures, and Storage Buffers",
        "BG 1 Vertex: Uniform Buffers",
        "BG 2 Frag: Sampled Textures, Storage Textures, and Storage Buffers",
        "BG 3 Frag: Uniform Buffers",
    };

    WGPUBindGroupLayout layouts[4] = {
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg0Entries, .entryCount = bindGroup0Total, .label = { labels[0], SDL_strlen(labels[0]) } }),
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg1Entries, .entryCount = bindGroup1Total, .label = { labels[1], SDL_strlen(labels[1]) } }),
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg2Entries, .entryCount = bindGroup2Total, .label = { labels[2], SDL_strlen(labels[2]) } }),
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg3Entries, .entryCount = bindGroup3Total, .label = { labels[3], SDL_strlen(labels[3]) } })
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {
        .bindGroupLayoutCount = 4,
        .bindGroupLayouts = layouts,
    };

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(renderer->device, &pipelineLayoutDesc);
    if (!pipelineLayout) {
        wgpuBindGroupLayoutRelease(layouts[0]);
        wgpuBindGroupLayoutRelease(layouts[1]);
        wgpuBindGroupLayoutRelease(layouts[2]);
        wgpuBindGroupLayoutRelease(layouts[3]);
        SDL_free(bg0Entries);
        SDL_free(bg1Entries);
        SDL_free(bg2Entries);
        SDL_free(bg3Entries);
        SDL_assert_release(!"Failed to create pipeline layout");
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "WebGPU: Created pipeline layout");

    // // Release bind group layouts (pipeline layout holds references)
    wgpuBindGroupLayoutRelease(layouts[0]);
    wgpuBindGroupLayoutRelease(layouts[1]);
    wgpuBindGroupLayoutRelease(layouts[2]);
    wgpuBindGroupLayoutRelease(layouts[3]);
    SDL_free(bg0Entries);
    SDL_free(bg1Entries);
    SDL_free(bg2Entries);
    SDL_free(bg3Entries);

    // Step 2: Configure Vertex State
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "WebGPU: Creating vertex buffer layouts");
    WGPUVertexBufferLayout *vertexBufferLayouts = WebGPU_INTERNAL_CreateVertexBufferLayouts(&createinfo->vertex_input_state);
    WGPUVertexState vertexState = {
        .module = vertexShader->shaderModule,
        .entryPoint = { "main", 4 },
        .bufferCount = createinfo->vertex_input_state.num_vertex_buffers,
        .buffers = vertexBufferLayouts
    };

    // Step 3: Configure Render Pipeline Descriptor
    WGPUColorTargetState *colorTargets = SDL_calloc(createinfo->target_info.num_color_targets, sizeof(WGPUColorTargetState));
    if (!colorTargets && createinfo->target_info.num_color_targets > 0) {
        SDL_free(vertexBufferLayouts);
        wgpuPipelineLayoutRelease(pipelineLayout);
        SDL_assert_release(!"Failed to allocate memory for color targets");
    }

    for (Uint32 i = 0; i < createinfo->target_info.num_color_targets; i++) {
        const SDL_GPUColorTargetBlendState *blendState = &createinfo->target_info.color_target_descriptions[i].blend_state;
        SDL_GPUColorComponentFlags colorWriteMask = blendState->enable_color_write_mask ? blendState->color_write_mask : 0xF;

        colorTargets[i].format = SDLToWGPUTextureFormat(createinfo->target_info.color_target_descriptions[i].format);
        colorTargets[i].writeMask = blendState->enable_blend ? SDLToWGPUColorWriteMask(colorWriteMask) : WGPUColorWriteMask_All;
        if (blendState->enable_blend) {
            colorTargets[i].blend = &(WGPUBlendState){
                .color.operation = SDLToWGPUBlendOperation(blendState->color_blend_op),
                .color.srcFactor = SDLToWGPUBlendFactor(blendState->src_color_blendfactor),
                .color.dstFactor = SDLToWGPUBlendFactor(blendState->dst_color_blendfactor),
                .alpha.operation = SDLToWGPUBlendOperation(blendState->alpha_blend_op),
                .alpha.srcFactor = SDLToWGPUBlendFactor(blendState->src_alpha_blendfactor),
                .alpha.dstFactor = SDLToWGPUBlendFactor(blendState->dst_alpha_blendfactor)
            };
        }
    }

    WGPUMultisampleState multisampleState = {
        .count = SDLToWGPUSampleCount(createinfo->multisample_state.sample_count),
        .mask = createinfo->multisample_state.enable_mask ? createinfo->multisample_state.sample_mask : 0xFFFFFFFF,
        .alphaToCoverageEnabled = false
    };

    WGPUDepthStencilState *depthStencilState = NULL;
    WGPUDepthStencilState depthStencilDesc = { 0 };
    if (createinfo->target_info.has_depth_stencil_target) {
        depthStencilDesc.format = SDLToWGPUTextureFormat(createinfo->target_info.depth_stencil_format);
        depthStencilDesc.depthWriteEnabled = createinfo->depth_stencil_state.enable_depth_write && createinfo->depth_stencil_state.enable_depth_test;
        depthStencilDesc.depthCompare = createinfo->depth_stencil_state.enable_depth_test ? SDLToWGPUCompareFunction(createinfo->depth_stencil_state.compare_op) : WGPUCompareFunction_Always;

        if (createinfo->depth_stencil_state.enable_stencil_test) {
            depthStencilDesc.stencilReadMask = createinfo->depth_stencil_state.compare_mask;
            depthStencilDesc.stencilWriteMask = createinfo->depth_stencil_state.write_mask;
            depthStencilDesc.stencilFront = (WGPUStencilFaceState){
                .compare = SDLToWGPUCompareFunction(createinfo->depth_stencil_state.front_stencil_state.compare_op),
                .failOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.front_stencil_state.fail_op),
                .depthFailOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.front_stencil_state.depth_fail_op),
                .passOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.front_stencil_state.pass_op)
            };
            depthStencilDesc.stencilBack = (WGPUStencilFaceState){
                .compare = SDLToWGPUCompareFunction(createinfo->depth_stencil_state.back_stencil_state.compare_op),
                .failOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.back_stencil_state.fail_op),
                .depthFailOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.back_stencil_state.depth_fail_op),
                .passOp = SDLToWGPUStencilOperation(createinfo->depth_stencil_state.back_stencil_state.pass_op)
            };
        }
        depthStencilState = &depthStencilDesc;
    }

    WGPUFragmentState fragmentState = {
        .module = fragmentShader->shaderModule,
        .entryPoint = { "main", 4 },
        .targetCount = createinfo->target_info.num_color_targets,
        .targets = colorTargets
    };

    WGPURenderPipelineDescriptor pipelineDesc = {
        .layout = pipelineLayout,
        .vertex = vertexState,
        .primitive = {
            .topology = SDLToWGPUPrimitiveTopology(createinfo->primitive_type),
            .frontFace = SDLToWGPUFrontFace(createinfo->rasterizer_state.front_face),
            .cullMode = SDLToWGPUCullMode(createinfo->rasterizer_state.cull_mode),
            .stripIndexFormat = WGPUIndexFormat_Undefined // Adjust if strip primitives are used
        },
        .depthStencil = depthStencilState,
        .multisample = multisampleState,
        .fragment = &fragmentState
    };

    // Step 4: Create WebGPU render pipeline
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(renderer->device, &pipelineDesc);
    if (!pipeline) {
        SDL_free(colorTargets);
        SDL_free(vertexBufferLayouts);
        wgpuPipelineLayoutRelease(pipelineLayout);
        SDL_assert_release(!"Failed to create render pipeline");
    }

    // Step 5: Create our abstraction
    result = SDL_calloc(1, sizeof(WebGPUGraphicsPipeline));
    if (!result) {
        wgpuRenderPipelineRelease(pipeline);
        SDL_free(colorTargets);
        SDL_free(vertexBufferLayouts);
        wgpuPipelineLayoutRelease(pipelineLayout);
        SDL_assert_release(!"Failed to allocate memory for graphics pipeline");
    }

    result->handle = pipeline; // Assuming WebGPUGraphicsPipeline has a WGPURenderPipeline field
    result->resourcesDirty = true;
    result->sample_mask = multisampleState.mask;
    result->rasterizerState = createinfo->rasterizer_state;
    result->primitiveType = createinfo->primitive_type;
    result->vertexSamplerCount = vertexShader->samplerCount;
    result->vertexUniformBufferCount = vertexShader->uniformBufferCount;
    result->vertexStorageBufferCount = vertexShader->storageBufferCount;
    result->vertexStorageTextureCount = vertexShader->storageTextureCount;
    result->fragmentSamplerCount = fragmentShader->samplerCount;
    result->fragmentUniformBufferCount = fragmentShader->uniformBufferCount;
    result->fragmentStorageBufferCount = fragmentShader->storageBufferCount;
    result->fragmentStorageTextureCount = fragmentShader->storageTextureCount;

    // Cleanup
    SDL_free(colorTargets);

    for (Uint32 i = 0; i < createinfo->vertex_input_state.num_vertex_buffers; i++) {
        if (vertexBufferLayouts[i].attributes != NULL) {
            SDL_free((void *)vertexBufferLayouts[i].attributes);
        }
    }
    SDL_free(vertexBufferLayouts);

    wgpuPipelineLayoutRelease(pipelineLayout);
    SDL_Log("Returning pipeline");
    return (SDL_GPUGraphicsPipeline *)result;
}

static void WebGPU_ReleaseGraphicsPipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUGraphicsPipeline *webgpuGraphicsPipeline = (WebGPUGraphicsPipeline *)graphicsPipeline;
    if (webgpuGraphicsPipeline->handle) {
        wgpuRenderPipelineRelease(webgpuGraphicsPipeline->handle);
    }
    if (webgpuGraphicsPipeline->bindGroup) {
        wgpuBindGroupRelease(webgpuGraphicsPipeline->bindGroup);
    }

    // Iterate through our cache of pipeline bindings and drop it if it exists.
    for (Uint32 i = 0; i < renderer->pipelineBindGroupCacheCount; i++) {
        WebGPUPipelineBindGroupCache *cache = &renderer->pipelineBindGroupCache[i];
        cache->pipeline = NULL;
    }

    SDL_free(webgpuGraphicsPipeline);
}

static SDL_GPUComputePipeline *WebGPU_CreateComputePipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUComputePipelineCreateInfo *createInfo)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUComputePipeline *pipeline;
    WGPUShaderModuleDescriptor shader_desc;
    WGPUShaderModule module;

    if (createInfo->format == SDL_GPU_SHADERFORMAT_WGSL) {
        const char *wgsl = (const char *)createInfo->code;
        WGPUShaderSourceWGSL wgsl_desc = {
            .chain = {
                .sType = WGPUSType_ShaderSourceWGSL,
                .next = NULL,
            },
            .code = (WGPUStringView){
                .data = wgsl,
                .length = SDL_strlen(wgsl),
            },
        };

        char *token;
        char *copy = SDL_stack_alloc(char, SDL_strlen(wgsl));
        SDL_strlcpy(copy, wgsl, SDL_strlen(wgsl));

        // Create our compute pipeline
        pipeline = SDL_calloc(1, sizeof(WebGPUComputePipeline));

        pipeline->numSamplers = createInfo->num_samplers;
        pipeline->numReadonlyStorageTextures = createInfo->num_readonly_storage_textures;
        pipeline->numReadWriteStorageTextures = createInfo->num_readwrite_storage_textures;
        pipeline->numReadonlyStorageBuffers = createInfo->num_readonly_storage_buffers;
        pipeline->numReadWriteStorageBuffers = createInfo->num_readwrite_storage_buffers;
        pipeline->numUniformBuffers = createInfo->num_uniform_buffers;
        pipeline->threadcountX = createInfo->threadcount_x;
        pipeline->threadcountY = createInfo->threadcount_y;
        pipeline->threadcountZ = createInfo->threadcount_z;

        WebGPU_INTERNAL_ParseBGL(&pipeline->bgl, wgsl);

        const char *label = "SDL_GPU WGSL Comp Shader";
        shader_desc = (WGPUShaderModuleDescriptor){
            .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
            .label = { label, SDL_strlen(label) },
        };
        module = wgpuDeviceCreateShaderModule(renderer->device, &shader_desc);
    }

    // Get the information for our bind group layouts.
    // Necessary to have our ComputePipeline validated against the shader and our bindings :/
    BindGroupLayoutEntryInfo *pipelineBGL = &pipeline->bgl;

    // Bind Group 0: Compute - Sampled Textures (TEXTURE then SAMPLER), Read Storage Textures, Read Storage Buffers
    Uint32 readOnlyTotal = pipeline->numReadonlyStorageTextures + pipeline->numReadonlyStorageBuffers;
    Uint32 bindGroup0Total = (pipeline->numSamplers * 2) + readOnlyTotal;
    WGPUBindGroupLayoutEntry *bg0Entries = SDL_calloc(bindGroup0Total, sizeof(WGPUBindGroupLayoutEntry));
    Uint32 bindingIndex = 0;
    Uint32 i;

    // Texture-Sampler Pairs
    for (i = 0; i < pipeline->numSamplers; i++) {
        // TEXTURE
        bg0Entries[bindingIndex].binding = bindingIndex; // e.g., 0, 2, 4...
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg0Entries[bindingIndex].texture.sampleType = pipelineBGL->sampleTypes[i];
        bg0Entries[bindingIndex].texture.viewDimension = pipelineBGL->sampleDimensions[i];
        bindingIndex++;

        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Texture Sample Type: %u, Dim: %u, Sampler Binding Type: %u", pipelineBGL->sampleTypes[i], pipelineBGL->sampleDimensions[i], pipelineBGL->sampleBindingType[i]);

        // SAMPLER
        bg0Entries[bindingIndex].binding = bindingIndex; // e.g., 1, 3, 5...
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg0Entries[bindingIndex].sampler.type = pipelineBGL->sampleBindingType[i];
        bindingIndex++;
    }

    // Read only storage textures
    for (i = 0; i < pipeline->numReadonlyStorageTextures; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg0Entries[bindingIndex].storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
        bg0Entries[bindingIndex].storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        bg0Entries[bindingIndex].storageTexture.viewDimension = pipelineBGL->storageDimensions[i];
    }

    for (i = 0; i < pipeline->numReadonlyStorageBuffers; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg0Entries[bindingIndex].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    }

    // Bind Group 1: Compute - Read-Write Storage Textures, Read-write Storage Buffers
    Uint32 bindGroup1Total = pipeline->numReadWriteStorageTextures + pipeline->numReadWriteStorageBuffers;
    WGPUBindGroupLayoutEntry *bg1Entries = SDL_calloc(bindGroup1Total, sizeof(WGPUBindGroupLayoutEntry));
    bindingIndex = 0;
    for (i = 0; i < pipeline->numReadWriteStorageTextures; i++) {
        bg1Entries[bindingIndex].binding = bindingIndex;
        bg1Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg1Entries[bindingIndex].storageTexture.access = WGPUStorageTextureAccess_ReadWrite;
        bg1Entries[bindingIndex].storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
        bg1Entries[bindingIndex].storageTexture.viewDimension = pipelineBGL->storageDimensions[i + readOnlyTotal];
    }
    for (i = 0; i < pipeline->numReadWriteStorageBuffers; i++) {
        bg1Entries[bindingIndex].binding = bindingIndex;
        bg1Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg1Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Storage;
    }

    // Bind Group 2: Compute - Uniform Buffers
    Uint32 bindGroup2Total = pipeline->numUniformBuffers;
    WGPUBindGroupLayoutEntry *bg2Entries = SDL_calloc(bindGroup2Total, sizeof(WGPUBindGroupLayoutEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < pipeline->numUniformBuffers; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].visibility = WGPUShaderStage_Compute;
        bg2Entries[bindingIndex].buffer.type = WGPUBufferBindingType_Uniform;
        bg2Entries[bindingIndex].buffer.hasDynamicOffset = false;
        bindingIndex++;
    }

    WGPUDevice device = renderer->device;
    const char *labels[3] = {
        "BG 0 Comp: Sampled Textures, READ Storage Textures, and READ Storage Buffers",
        "BG 1 Comp: RW storage textures, RW Storage Buffers",
        "BG 2 Comp: Uniform Buffers",
    };

    WGPUBindGroupLayout layouts[3] = {
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg0Entries, .entryCount = bindGroup0Total, .label = { labels[0], SDL_strlen(labels[0]) } }),
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg1Entries, .entryCount = bindGroup1Total, .label = { labels[1], SDL_strlen(labels[1]) } }),
        wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){ .entries = bg2Entries, .entryCount = bindGroup2Total, .label = { labels[2], SDL_strlen(labels[2]) } }),
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {
        .bindGroupLayoutCount = 3,
        .bindGroupLayouts = layouts,
    };

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(renderer->device, &pipelineLayoutDesc);
    if (!pipelineLayout) {
        wgpuBindGroupLayoutRelease(layouts[0]);
        wgpuBindGroupLayoutRelease(layouts[1]);
        wgpuBindGroupLayoutRelease(layouts[2]);
        wgpuBindGroupLayoutRelease(layouts[3]);
        SDL_free(bg0Entries);
        SDL_free(bg1Entries);
        SDL_free(bg2Entries);
        SDL_assert_release(!"Failed to create pipeline layout");
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "WebGPU: Created pipeline layout");

    // // Release bind group layouts (pipeline layout holds references)
    wgpuBindGroupLayoutRelease(layouts[0]);
    wgpuBindGroupLayoutRelease(layouts[1]);
    wgpuBindGroupLayoutRelease(layouts[2]);
    SDL_free(bg0Entries);
    SDL_free(bg1Entries);
    SDL_free(bg2Entries);

    WGPUComputePipelineDescriptor desc = {
        .layout = pipelineLayout,
        .compute = {
            .module = module,
            .entryPoint = { "main", 4 },
            .constants = NULL,
            .constantCount = 0 },
    };

    pipeline->handle = wgpuDeviceCreateComputePipeline(renderer->device, &desc);

    return (SDL_GPUComputePipeline *)pipeline;
}

// Helper to create or update the bind group
static void WebGPU_INTERNAL_CreateBindGroup(WebGPUCommandBuffer *commandBuffer, WGPUBindGroup *bindgroups)
{
    WebGPUGraphicsPipeline *pipeline = commandBuffer->graphicsPipeline;
    if (!pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No graphics pipeline active");
    }

    // Clean bindgroups if we reach this point
    for (Uint32 i = 0; i < 4; i++) {
        if (bindgroups[i] != NULL) {
            wgpuBindGroupRelease(bindgroups[i]);
        }
    }

    // Bind Group 0: Vertex - Sampled Textures (TEXTURE then SAMPLER), Storage Textures, Storage Buffers
    Uint32 bindGroup0Total = (pipeline->vertexSamplerCount * 2) + pipeline->vertexStorageTextureCount + pipeline->vertexStorageBufferCount;
    Uint32 bindingIndex = 0;

    // Texture-Sampler Pairs
    WGPUBindGroupEntry *bg0Entries = SDL_calloc(bindGroup0Total, sizeof(WGPUBindGroupEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < pipeline->vertexSamplerCount; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].textureView = wgpuTextureCreateView(commandBuffer->vertexTextures[i], NULL);
        bindingIndex++;

        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].sampler = commandBuffer->vertexSamplers[i];
        bindingIndex++;
    }
    // Storage Textures
    for (Uint32 i = 0; i < pipeline->vertexStorageTextureCount; i++) {
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].textureView = wgpuTextureCreateView(commandBuffer->vertexStorageTextures[i], NULL);
        bindingIndex++;
    }
    // Storage Buffers
    for (Uint32 i = 0; i < pipeline->vertexStorageBufferCount; i++) {
        WGPUBuffer buffer = commandBuffer->vertexStorageBuffers[i];
        bg0Entries[bindingIndex].binding = bindingIndex;
        bg0Entries[bindingIndex].buffer = buffer;
        bg0Entries[bindingIndex].size = wgpuBufferGetSize(buffer);
        bindingIndex++;
    }

    // Bind Group 1: Vertex - Uniform Buffers
    Uint32 bindGroup1Total = pipeline->vertexUniformBufferCount;
    WGPUBindGroupEntry *bg1Entries = SDL_calloc(bindGroup1Total, sizeof(WGPUBindGroupEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < pipeline->vertexUniformBufferCount; i++) {
        WebGPUUniformBuffer *buffer = commandBuffer->vertexUniformBuffers[i];
        bg1Entries[bindingIndex].binding = bindingIndex;
        bg1Entries[bindingIndex].buffer = buffer->buffer;
        bg1Entries[bindingIndex].size = 256,
        bg1Entries[bindingIndex].offset = buffer->drawOffset;
        bindingIndex++;
    }

    // Bind Group 2: Fragment - Similar to Bind Group 0 but with fragment visibility
    Uint32 bindGroup2Total = (pipeline->fragmentSamplerCount * 2) + pipeline->fragmentStorageTextureCount + pipeline->fragmentStorageBufferCount;
    WGPUBindGroupEntry *bg2Entries = SDL_calloc(bindGroup2Total, sizeof(WGPUBindGroupEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < pipeline->fragmentSamplerCount; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].textureView = wgpuTextureCreateView(commandBuffer->fragmentTextures[i], NULL);
        bindingIndex++;

        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].sampler = commandBuffer->fragmentSamplers[i];
        bindingIndex++;
    }
    // Storage Textures
    for (Uint32 i = 0; i < pipeline->fragmentStorageTextureCount; i++) {
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].textureView = wgpuTextureCreateView(commandBuffer->fragmentStorageTextures[i], NULL);
        bindingIndex++;
    }
    // Storage Buffers
    for (Uint32 i = 0; i < pipeline->fragmentStorageBufferCount; i++) {
        WGPUBuffer buffer = commandBuffer->fragmentStorageBuffers[i];
        bg2Entries[bindingIndex].binding = bindingIndex;
        bg2Entries[bindingIndex].buffer = buffer;
        bg2Entries[bindingIndex].size = wgpuBufferGetSize(buffer);
        bindingIndex++;
    }

    // Bind Group 3: Fragment - Uniform Buffers
    Uint32 bindGroup3Total = pipeline->fragmentUniformBufferCount;
    WGPUBindGroupEntry *bg3Entries = SDL_calloc(bindGroup3Total, sizeof(WGPUBindGroupEntry));
    bindingIndex = 0;
    for (Uint32 i = 0; i < pipeline->fragmentUniformBufferCount; i++) {
        WebGPUUniformBuffer *buffer = commandBuffer->fragmentUniformBuffers[i];
        bg3Entries[bindingIndex].binding = bindingIndex;
        bg3Entries[bindingIndex].buffer = buffer->buffer;
        bg3Entries[bindingIndex].size = 256;
        bg3Entries[bindingIndex].offset = buffer->drawOffset;
        bindingIndex++;
    }

    WGPUBindGroupDescriptor bindGroupDesc0 = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(commandBuffer->graphicsPipeline->handle, 0),
        .entryCount = bindGroup0Total,
        .entries = bg0Entries
    };

    WGPUBindGroupDescriptor bindGroupDesc1 = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(commandBuffer->graphicsPipeline->handle, 1),
        .entryCount = bindGroup1Total,
        .entries = bg1Entries
    };

    WGPUBindGroupDescriptor bindGroupDesc2 = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(commandBuffer->graphicsPipeline->handle, 2),
        .entryCount = bindGroup2Total,
        .entries = bg2Entries,
    };

    WGPUBindGroupDescriptor bindGroupDesc3 = {
        .layout = wgpuRenderPipelineGetBindGroupLayout(commandBuffer->graphicsPipeline->handle, 3),
        .entryCount = bindGroup3Total,
        .entries = bg3Entries
    };

    bindgroups[0] = wgpuDeviceCreateBindGroup(commandBuffer->renderer->device, &bindGroupDesc0);
    bindgroups[1] = wgpuDeviceCreateBindGroup(commandBuffer->renderer->device, &bindGroupDesc1);
    bindgroups[2] = wgpuDeviceCreateBindGroup(commandBuffer->renderer->device, &bindGroupDesc2);
    bindgroups[3] = wgpuDeviceCreateBindGroup(commandBuffer->renderer->device, &bindGroupDesc3);
    if (!bindgroups[0] || !bindgroups[1] || !bindgroups[2] || !bindgroups[3]) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create bind groups");
    }

    // Cleanup
    SDL_free(bg0Entries);
    SDL_free(bg1Entries);
    SDL_free(bg2Entries);
    SDL_free(bg3Entries);
}

static void WebGPU_INTERNAL_BindGraphicsResources(WebGPUCommandBuffer *commandBuffer)
{
    WebGPUGraphicsPipeline *graphicsPipeline = commandBuffer->graphicsPipeline;
    if (!graphicsPipeline || !commandBuffer->renderEncoder) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No graphics pipeline or render encoder active");
        return;
    }

    // Check if any resources need binding
    bool needBind = commandBuffer->needVertexSamplerBind ||
                    commandBuffer->needVertexStorageTextureBind ||
                    commandBuffer->needVertexStorageBufferBind ||
                    commandBuffer->needVertexUniformBind ||
                    commandBuffer->needFragmentSamplerBind ||
                    commandBuffer->needFragmentStorageTextureBind ||
                    commandBuffer->needFragmentStorageBufferBind ||
                    commandBuffer->needFragmentUniformBind;

    // Find or create cache entry for this pipeline
    WebGPURenderer *renderer = commandBuffer->renderer;
    WebGPUPipelineBindGroupCache *cache = NULL;

    for (Uint32 i = 0; i < renderer->pipelineBindGroupCacheCount; i++) {
        if (renderer->pipelineBindGroupCache[i].pipeline == graphicsPipeline) {
            cache = &renderer->pipelineBindGroupCache[i];
            break;
        }
    }

    if (!cache) {
        // Add new cache entry
        EXPAND_ARRAY_IF_NEEDED(
            renderer->pipelineBindGroupCache,
            WebGPUPipelineBindGroupCache,
            renderer->pipelineBindGroupCacheCount + 1,
            renderer->pipelineBindGroupCacheCapacity,
            renderer->pipelineBindGroupCacheCapacity * 2);

        cache = &renderer->pipelineBindGroupCache[renderer->pipelineBindGroupCacheCount++];
        cache->pipeline = graphicsPipeline;
        cache->resourcesDirty = true;
        cache->lastFrameUsed = 0;

        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Added new cache entry for pipeline %p", graphicsPipeline);
    }

    // Take note of which in flight frame was last associated with the cache
    cache->lastFrameUsed = renderer->claimedWindows[0]->frameCounter;

    // Check if we need to recreate the bind group
    if (needBind || cache->resourcesDirty) {
        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Creating/updating bind group for pipeline %p (dirty=%d, needBind=%d, existing=%p)",
                    graphicsPipeline, cache->resourcesDirty, needBind, cache->bindGroups);

        // Create new bind group
        WebGPU_INTERNAL_CreateBindGroup(commandBuffer, cache->bindGroups);
        cache->resourcesDirty = false;
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Reusing cached bind group %p for pipeline %p", cache->bindGroups, graphicsPipeline);
    }

    // Bind the bind groups with according offsets based on the bind group
    for (Uint32 i = 0; i < 4; i++) {
        wgpuRenderPassEncoderSetBindGroup(
            commandBuffer->renderEncoder,
            i,
            cache->bindGroups[i],
            0,
            NULL);
    }

    // Clear resource binding flags
    commandBuffer->needVertexSamplerBind = false;
    commandBuffer->needVertexStorageTextureBind = false;
    commandBuffer->needVertexStorageBufferBind = false;
    commandBuffer->needVertexUniformBind = false;
    commandBuffer->needFragmentSamplerBind = false;
    commandBuffer->needFragmentStorageTextureBind = false;
    commandBuffer->needFragmentStorageBufferBind = false;
    commandBuffer->needFragmentUniformBind = false;
}

static void WebGPU_BindGraphicsPipeline(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUGraphicsPipeline *webgpuGraphicsPipeline = (WebGPUGraphicsPipeline *)graphicsPipeline;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;

    // Find pipeline in the cache or add it
    WebGPUPipelineBindGroupCache *cache = NULL;
    for (Uint32 i = 0; i < renderer->pipelineBindGroupCacheCount; i++) {
        if (renderer->pipelineBindGroupCache[i].pipeline == webgpuGraphicsPipeline) {
            cache = &renderer->pipelineBindGroupCache[i];
            SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "PIPELINE CACHE: Found cached pipeline!");
            break;
        }
    }

    if (!cache) {
        // Add to cache if not found
        EXPAND_ARRAY_IF_NEEDED(
            renderer->pipelineBindGroupCache,
            WebGPUPipelineBindGroupCache,
            renderer->pipelineBindGroupCacheCount + 1,
            renderer->pipelineBindGroupCacheCapacity,
            renderer->pipelineBindGroupCacheCapacity + 4);

        cache = &renderer->pipelineBindGroupCache[renderer->pipelineBindGroupCacheCount++];
        cache->pipeline = webgpuGraphicsPipeline;
        cache->resourcesDirty = true;
        cache->lastFrameUsed = 0;

        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "PIPELINE CACHE: Created new pipeline cache entry for pipeline %p", webgpuGraphicsPipeline);
    }

    // Update the usage frame (this should be the in flight frame number)
    cache->lastFrameUsed = renderer->claimedWindows[0]->frameCounter;

    // Bind the pipeline
    webgpuCommandBuffer->graphicsPipeline = webgpuGraphicsPipeline;
    webgpuCommandBuffer->currentPipelineCache = cache;

    // All state stuff is handled by the pipeline, so we just need to bind it
    if (webgpuCommandBuffer->renderEncoder) {
        wgpuRenderPassEncoderSetPipeline(webgpuCommandBuffer->renderEncoder, webgpuGraphicsPipeline->handle);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "No active render pass encoder to bind pipeline");
        return;
    }

    // Only allocate uniform buffers if needed
    for (Uint32 i = 0; i < webgpuGraphicsPipeline->vertexUniformBufferCount; i += 1) {
        if (webgpuCommandBuffer->vertexUniformBuffers[i] == NULL) {
            webgpuCommandBuffer->vertexUniformBuffers[i] = WebGPU_INTERNAL_AcquireUniformBufferFromPool(webgpuCommandBuffer);
            webgpuCommandBuffer->needVertexUniformBind = true; // Set flag only when buffer changes
        }
    }

    for (Uint32 i = 0; i < webgpuGraphicsPipeline->fragmentUniformBufferCount; i += 1) {
        if (webgpuCommandBuffer->fragmentUniformBuffers[i] == NULL) {
            webgpuCommandBuffer->fragmentUniformBuffers[i] = WebGPU_INTERNAL_AcquireUniformBufferFromPool(webgpuCommandBuffer);
            webgpuCommandBuffer->needFragmentUniformBind = true; // Set flag only when buffer changes
        }
    }

    // Set command buffer flags based on cache state
    webgpuCommandBuffer->needVertexUniformBind = cache->resourcesDirty;
    webgpuCommandBuffer->needFragmentUniformBind = cache->resourcesDirty;

    SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Pipeline bound: %p, cache: %p, resourcesDirty: %d",
                 webgpuGraphicsPipeline, cache, cache->resourcesDirty);
}

static void WebGPU_DrawPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 numVertices,
    Uint32 numInstances,
    Uint32 firstVertex,
    Uint32 firstInstance)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;

    DebugFrameObjects(webgpuCommandBuffer);

    // Bind the graphics pipeline and resources
    WebGPU_INTERNAL_BindGraphicsResources(webgpuCommandBuffer);

    // Draw the primitives
    wgpuRenderPassEncoderDraw(
        webgpuCommandBuffer->renderEncoder,
        numVertices,
        numInstances,
        firstVertex,
        firstInstance);
}

static void WebGPU_DrawIndexedPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 numIndices,
    Uint32 numInstances,
    Uint32 firstIndex,
    Sint32 vertexOffset,
    Uint32 firstInstance)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    DebugFrameObjects(webgpuCommandBuffer);

    WebGPU_INTERNAL_BindGraphicsResources(webgpuCommandBuffer);

    wgpuRenderPassEncoderDrawIndexed(
        webgpuCommandBuffer->renderEncoder,
        numIndices,
        numInstances,
        firstIndex,
        vertexOffset,
        firstInstance);
}

static bool WebGPU_QueryFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    WebGPUFence *webgpuFence = (WebGPUFence *)fence;
    /*SDL_Log("WebGPU: Querying fence: %d", SDL_GetAtomicInt(&webgpuFence->complete));*/
    return SDL_GetAtomicInt(&webgpuFence->complete) == 1;
}

static bool WebGPU_WaitForFences(
    SDL_GPURenderer *driverData,
    bool waitAll,
    SDL_GPUFence *const *fences,
    Uint32 numFences)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    bool waiting;

    if (waitAll) {
        for (Uint32 i = 0; i < numFences; i += 1) {
            while (!SDL_GetAtomicInt(&((WebGPUFence *)fences[i])->complete)) {
                // Spin!
                SDL_Delay(1); // Hand control to the browser or OS (necessary for WebGPU)
            }
        }
    } else {
        waiting = 1;
        while (waiting) {
            for (Uint32 i = 0; i < numFences; i += 1) {
                if (SDL_GetAtomicInt(&((WebGPUFence *)fences[i])->complete) > 0) {
                    waiting = 0;
                    break;
                }
            }
            if (waiting) {
                SDL_Delay(1); // Hand control to the browser or OS (necessary for WebGPU)
            }
        }
    }

    WebGPU_INTERNAL_PerformPendingDestroys(renderer);

    return true;
}

// Fetch the necessary PropertiesID for the WebGPUWindow for a browser window
static WebGPUWindowData *WebGPU_INTERNAL_FetchWindowData(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    WebGPUWindowData *windowData = (WebGPUWindowData *)SDL_GetPointerProperty(properties, WINDOW_PROPERTY_DATA, NULL);
    return windowData;
}

static bool WebGPU_Wait(
    SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUCommandBuffer *commandBuffer;

    /*
     * Wait for all submitted command buffers to complete.
     * Sort of equivalent to vkDeviceWaitIdle.
     */
    for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        while (!SDL_GetAtomicInt(&renderer->submittedCommandBuffers[i]->fence->complete)) {
            // Spin!
            SDL_Delay(1); // Hand control to the browser or OS (necessary for WebGPU)
        }
    }

    SDL_LockMutex(renderer->submitLock);

    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        commandBuffer = renderer->submittedCommandBuffers[i];
        WebGPU_INTERNAL_CleanCommandBuffer(renderer, commandBuffer, false);
    }

    WebGPU_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->submitLock);

    return true;
}

// Callback for when the window is resized
static bool WebGPU_INTERNAL_OnWindowResize(void *userdata, SDL_Event *event)
{
    SDL_Window *window = (SDL_Window *)userdata;
    // Event watchers will pass any event, but we only care about window resize events
    if (event->type != SDL_EVENT_WINDOW_RESIZED) {
        return false;
    }
    WebGPUWindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData) {
        SDL_LockMutex(windowData->renderer->windowLock);

        windowData->window->w = event->window.data1;
        windowData->window->h = event->window.data2;

        // Set the window's needsConfigure flag to true
        windowData->needsConfigure = true;
        SDL_UnlockMutex(windowData->renderer->windowLock);
    }

    return true;
}

static void WebGPU_INTERNAL_DestroySwapchain(
    WebGPURenderer *renderer,
    WebGPUWindowData *windowData)
{
    SDL_LockMutex(renderer->windowLock);

    if (windowData->surface) {
        wgpuSurfaceRelease(windowData->surface);
        windowData->surface = NULL;
    }

    for (Uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        if (windowData->inFlightFences[i]) {
            WebGPU_ReleaseFence((SDL_GPURenderer *)renderer, windowData->inFlightFences[i]);
            windowData->inFlightFences[i] = NULL;
        }
    }

    SDL_UnlockMutex(renderer->windowLock);
}

static void WebGPU_INTERNAL_RecreateSwapchain(
    WebGPURenderer *renderer,
    WebGPUWindowData *windowData)
{
    WebGPU_INTERNAL_DestroySwapchain(renderer, windowData);
    if (WebGPU_INTERNAL_CreateSwapchain(renderer, windowData, windowData->swapchainComposition, windowData->presentMode)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "WebGPU: Recreated swapchain surface");
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain surface");
    }
    windowData->needsConfigure = false;
}

static bool WebGPU_INTERNAL_CreateSwapchain(
    WebGPURenderer *renderer,
    WebGPUWindowData *windowData,
    SDL_GPUSwapchainComposition composition,
    SDL_GPUPresentMode presentMode)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Creating Swapchain.");

    SDL_LockMutex(renderer->windowLock);

    // Create platform agnostic surface: Currently just supports X11
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_assert(_this && _this->WebGPU_CreateSurface);
    if (!_this->WebGPU_CreateSurface(
            _this,
            windowData->window,
            renderer->instance,
            &windowData->surface)) {
        return false;
    }

    SDL_assert(windowData->surface);

    windowData->texture.handle = NULL;
    windowData->textureContainer.activeTexture = &windowData->texture;

    for (Uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        if (windowData->inFlightFences[i]) {
            windowData->inFlightFences[i] = NULL;
        }
    }

    windowData->swapchainComposition = composition;
    windowData->presentMode = presentMode;
    windowData->frameCounter = 0;

    // Configure our swapchain surface before we acquire the texture
    wgpuSurfaceConfigure(
        windowData->surface,
        &(const WGPUSurfaceConfiguration){
            .usage = WGPUTextureUsage_RenderAttachment |
                     WGPUTextureUsage_CopySrc |
                     WGPUTextureUsage_CopyDst,
            .format = SDLToWGPUTextureFormat(SwapchainCompositionToFormat[composition]),
            .width = windowData->window->w,
            .height = windowData->window->h,
            .presentMode = SDLToWGPUPresentMode(windowData->presentMode),
            .alphaMode = WGPUCompositeAlphaMode_Opaque,
            .device = renderer->device,
        });

    // Precache blit pipelines for the swapchain format
    for (Uint32 i = 0; i < 4; i += 1) {
        SDL_GPU_FetchBlitPipeline(
            renderer->sdlDevice,
            (SDL_GPUTextureType)i,
            SwapchainCompositionToFormat[composition],
            renderer->blitVertexShader,
            renderer->blitFrom2DShader,
            renderer->blitFrom2DArrayShader,
            renderer->blitFrom3DShader,
            renderer->blitFromCubeShader,
            renderer->blitFromCubeArrayShader,
            &renderer->blitPipelines,
            &renderer->blitPipelineCount,
            &renderer->blitPipelineCapacity);
    }

    // Set up the texture container
    SDL_zero(windowData->textureContainer);
    windowData->textureContainer.textures = SDL_calloc(1, sizeof(WebGPUTexture *));
    windowData->textureContainer.textures[0] = &windowData->texture;
    windowData->textureContainer.canBeCycled = 0;
    windowData->textureContainer.activeTexture = &windowData->texture;
    windowData->textureContainer.textureCapacity = 1;
    windowData->textureContainer.textureCount = 1;
    windowData->textureContainer.header.info.format = SwapchainCompositionToFormat[composition];
    windowData->textureContainer.header.info.num_levels = 1;
    windowData->textureContainer.header.info.layer_count_or_depth = 1;
    windowData->textureContainer.header.info.type = SDL_GPU_TEXTURETYPE_2D;
    windowData->textureContainer.header.info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    windowData->textureContainer.header.info.width = (Uint32)windowData->window->w;
    windowData->textureContainer.header.info.height = (Uint32)windowData->window->h;

    SDL_UnlockMutex(renderer->windowLock);

    return windowData->surface != NULL;
}

static bool WebGPU_ClaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUWindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = SDL_calloc(sizeof(WebGPUWindowData), 1);
        windowData->window = window;
        windowData->renderer = renderer;
        windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        if (WebGPU_INTERNAL_CreateSwapchain(renderer, windowData, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);
            SDL_LockMutex(renderer->windowLock);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(WebGPUWindowData *));
            }

            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            SDL_UnlockMutex(renderer->windowLock);

            SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Swapchain created!");
            SDL_AddEventWatch(WebGPU_INTERNAL_OnWindowResize, window);
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

static void WebGPU_ReleaseWindow(SDL_GPURenderer *driverData, SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;

    SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "ReleaseWindow Called");
    if (renderer->claimedWindowCount == 0) {
        return;
    }

    WebGPUWindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    if (windowData == NULL) {
        return;
    }

    // Eliminate the window from the claimed windows
    for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1) {
        if (renderer->claimedWindows[i]->window == window) {
            renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
            renderer->claimedWindowCount -= 1;
            break;
        }
    }

    // Cleanup
    SDL_free(windowData);
    SDL_ClearProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA);
    SDL_RemoveEventWatch(WebGPU_INTERNAL_OnWindowResize, window);
}

static WGPUTexture WebGPU_INTERNAL_AcquireSurfaceTexture(
    WebGPURenderer *renderer,
    WebGPUWindowData *windowData)
{
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(windowData->surface, &surfaceTexture);

    SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Surface texture status: %d, texture: %p, frameCounter: %u",
                 surfaceTexture.status, surfaceTexture.texture, windowData->frameCounter);

    switch (surfaceTexture.status) {
    case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
    case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
        break;
    // case WGPUSurfaceGetCurrentTextureStatus_Lost:
    //     SDL_LogError(SDL_LOG_CATEGORY_GPU, "GPU DEVICE LOST (frame %u)", windowData->frameCounter);
    //     SDL_SetError("GPU DEVICE LOST");
    //     return NULL;
    // case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
    //     SDL_LogError(SDL_LOG_CATEGORY_GPU, "Out of memory acquiring surface texture (frame %u)", windowData->frameCounter);
    //     SDL_OutOfMemory();
    //     return NULL;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Surface texture acquisition timed out (frame %u)", windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        return NULL;
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Surface texture is outdated (frame %u)", windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        return NULL;
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Surface texture lost (frame %u)", windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        return NULL;
    default:
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Unknown surface texture status: %d (frame %u)",
                    surfaceTexture.status, windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        return NULL;
    }

    return surfaceTexture.texture;
}

static SDL_GPUTextureFormat WebGPU_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUWindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SET_STRING_ERROR_AND_RETURN("Cannot get swapchain format, window has not been claimed", SDL_GPU_TEXTUREFORMAT_INVALID);
    }

    return windowData->textureContainer.header.info.format;
}

static bool WebGPU_INTERNAL_AcquireSwapchainTexture(
    bool block,
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    SDL_GPUTexture **texture,
    Uint32 *swapchainTextureWidth,
    Uint32 *swapchainTextureHeight)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;

    SDL_LockMutex(renderer->windowLock);
    WebGPUWindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    SDL_AtomicIncRef(&windowData->texture.refCount);

    if (windowData->needsConfigure) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Recreating swapchain due to needsConfigure (frame %u)", windowData->frameCounter);
        WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
        if (windowData->surface == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain surface (frame %u)", windowData->frameCounter);
            SDL_AtomicDecRef(&windowData->texture.refCount);
            SDL_UnlockMutex(renderer->windowLock);
            return false;
        }
    }

    if (windowData->texture.handle) {
        wgpuTextureRelease(windowData->texture.handle);
    }

    Uint32 frameIndex = windowData->frameCounter % MAX_FRAMES_IN_FLIGHT;
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Frame %u, fence index %u, inFlightFences: %p, signaled: %d, texture handle: %p",
                windowData->frameCounter, frameIndex, windowData->inFlightFences[frameIndex],
                windowData->inFlightFences[frameIndex] ? WebGPU_QueryFence((SDL_GPURenderer *)renderer, windowData->inFlightFences[frameIndex]) : -1,
                windowData->texture.handle);

    if (windowData->inFlightFences[frameIndex]) {
        if (block) {
            if (!WebGPU_WaitForFences((SDL_GPURenderer *)renderer, true, &windowData->inFlightFences[frameIndex], 1)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to wait for fence (frame %u), retrying...", windowData->frameCounter);
                SDL_Delay(1); // Add a small delay to allow GPU time
                if (!WebGPU_WaitForFences((SDL_GPURenderer *)renderer, true, &windowData->inFlightFences[frameIndex], 1)) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Fence wait failed after delay (frame %u)", windowData->frameCounter);
                    SDL_AtomicDecRef(&windowData->texture.refCount);
                    SDL_UnlockMutex(renderer->windowLock);
                    return false;
                }
            }
        } else if (!WebGPU_QueryFence((SDL_GPURenderer *)renderer, windowData->inFlightFences[frameIndex])) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Skipping frame %u due to unsignaled fence, resetting texture handle",
                        windowData->frameCounter);
            // Reset or invalidate the texture handle to prevent use
            windowData->texture.handle = NULL;                 // Mark as invalid
            windowData->textureContainer.activeTexture = NULL; // Mark as invalid
            windowData->textureContainer.textures[0] = NULL;   // Mark as invalid
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "After reset - windowData: %p, frameCounter: %u", windowData, windowData->frameCounter);
            SDL_AtomicDecRef(&windowData->texture.refCount);
            SDL_UnlockMutex(renderer->windowLock);
            return true; // Skip frame without error
        }
        WebGPU_ReleaseFence((SDL_GPURenderer *)renderer, windowData->inFlightFences[frameIndex]);
        windowData->inFlightFences[frameIndex] = NULL;
    }

    WGPUTexture surfaceTexture = NULL;

    while (surfaceTexture == NULL) {
        surfaceTexture = WebGPU_INTERNAL_AcquireSurfaceTexture(renderer, windowData);
    }

    Uint32 width = wgpuTextureGetWidth(surfaceTexture);
    Uint32 height = wgpuTextureGetHeight(surfaceTexture);

    windowData->textureContainer.header.info.width = width;
    windowData->textureContainer.header.info.height = height;
    if (swapchainTextureWidth)
        *swapchainTextureWidth = width;
    if (swapchainTextureHeight)
        *swapchainTextureHeight = height;

    windowData->texture.handle = surfaceTexture;
    windowData->textureContainer.activeTexture = &windowData->texture;
    windowData->textureContainer.textures[0] = &windowData->texture;

    EXPAND_ARRAY_IF_NEEDED(webgpuCommandBuffer->windowDatas, WebGPUWindowData *,
                           webgpuCommandBuffer->windowDataCount + 1,
                           webgpuCommandBuffer->windowDataCapacity,
                           webgpuCommandBuffer->windowDataCapacity + 1);
    webgpuCommandBuffer->windowDatas[webgpuCommandBuffer->windowDataCount++] = windowData;

    SDL_AtomicDecRef(&windowData->texture.refCount);
    SDL_UnlockMutex(renderer->windowLock);

    *texture = (SDL_GPUTexture *)&windowData->textureContainer;
    return true;
}

static bool WebGPU_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_Window *window,
    SDL_GPUTexture **swapchain_texture,
    Uint32 *swapchain_texture_width,
    Uint32 *swapchain_texture_height)
{
    return WebGPU_INTERNAL_AcquireSwapchainTexture(
        false,
        command_buffer,
        window,
        swapchain_texture,
        swapchain_texture_width,
        swapchain_texture_height);
}

static bool WebGPU_WaitAndAcquireSwapchainTexture(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_Window *window,
    SDL_GPUTexture **swapchain_texture,
    Uint32 *swapchain_texture_width,
    Uint32 *swapchain_texture_height)
{
    return WebGPU_INTERNAL_AcquireSwapchainTexture(
        true,
        command_buffer,
        window,
        swapchain_texture,
        swapchain_texture_width,
        swapchain_texture_height);
}
static void WebGPU_INTERNAL_AllocateCommandBuffers(
    WebGPURenderer *renderer,
    Uint32 allocateCount)
{
    WebGPUCommandBuffer *commandBuffer;

    renderer->availableCommandBufferCapacity += allocateCount;

    renderer->availableCommandBuffers = SDL_realloc(
        renderer->availableCommandBuffers,
        renderer->availableCommandBufferCapacity * sizeof(WebGPUCommandBuffer *));

    for (Uint32 i = 0; i < allocateCount; i += 1) {
        commandBuffer = SDL_calloc(1, sizeof(WebGPUCommandBuffer));
        commandBuffer->renderer = renderer;

        // The native WebGPU command buffer is created om WebGPU_AcquireCommandBuffer
        // since command encoders are created per frame in WebGPU

        commandBuffer->windowDataCapacity = 1;
        commandBuffer->windowDataCount = 0;
        commandBuffer->windowDatas = SDL_calloc(
            commandBuffer->windowDataCapacity,
            sizeof(WebGPUWindowData *));

        commandBuffer->usedBufferCapacity = 4;
        commandBuffer->usedBufferCount = 0;
        commandBuffer->usedBuffers = SDL_calloc(
            commandBuffer->usedBufferCapacity,
            sizeof(WebGPUBuffer *));

        commandBuffer->usedTextureCount = 4;
        commandBuffer->usedTextureCount = 0;
        commandBuffer->usedTextures = SDL_calloc(
            commandBuffer->usedTextureCapacity,
            sizeof(WebGPUTexture *));

        renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
        renderer->availableCommandBufferCount += 1;
    }
}

static WebGPUCommandBuffer *WebGPU_INTERNAL_GetInactiveCommandBufferFromPool(
    WebGPURenderer *renderer)
{
    WebGPUCommandBuffer *commandBuffer;
    if (renderer->availableCommandBufferCount == 0) {
        WebGPU_INTERNAL_AllocateCommandBuffers(
            renderer,
            renderer->availableCommandBufferCapacity);
    }

    commandBuffer = renderer->availableCommandBuffers[renderer->availableCommandBufferCount - 1];
    renderer->availableCommandBufferCount -= 1;

    return commandBuffer;
}

static SDL_GPUCommandBuffer *WebGPU_AcquireCommandBuffer(SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUCommandBuffer *commandBuffer;

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    commandBuffer = WebGPU_INTERNAL_GetInactiveCommandBufferFromPool(renderer);
    const char *cmd_encoder_label = "SDL_GPU Command Encoder";
    commandBuffer->handle = wgpuDeviceCreateCommandEncoder(
        renderer->device,
        &(const WGPUCommandEncoderDescriptor){
            .label = { cmd_encoder_label, SDL_strlen(cmd_encoder_label) },
            .nextInChain = NULL,
        });
    commandBuffer->graphicsPipeline = NULL;
    commandBuffer->computePipeline = NULL;
    for (Uint32 i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
        commandBuffer->vertexUniformBuffers[i] = NULL;
        commandBuffer->fragmentUniformBuffers[i] = NULL;
        commandBuffer->computeUniformBuffers[i] = NULL;
    }

    // // FIXME: Do we actually need to set this?
    // commandBuffer->needVertexSamplerBind = false;
    // commandBuffer->needVertexStorageTextureBind = false;
    // commandBuffer->needVertexStorageBufferBind = false;
    // commandBuffer->needVertexUniformBind = false;
    // commandBuffer->needFragmentSamplerBind = false;
    // commandBuffer->needFragmentStorageTextureBind = false;
    // commandBuffer->needFragmentStorageBufferBind = false;
    // commandBuffer->needFragmentUniformBind = false;
    // commandBuffer->needComputeSamplerBind = false;
    // commandBuffer->needComputeBufferBind = false;
    // commandBuffer->needComputeTextureBind = false;
    // commandBuffer->needComputeUniformBind = false;

    commandBuffer->autoReleaseFence = true;

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    return (SDL_GPUCommandBuffer *)commandBuffer;
}

static void WebGPU_INTERNAL_CleanCommandBuffer(
    WebGPURenderer *renderer,
    WebGPUCommandBuffer *commandBuffer,
    bool cancel)
{
    Uint32 i;

    // Uniform buffers are now available
    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        WebGPU_INTERNAL_ReturnUniformBufferToPool(
            renderer,
            commandBuffer->usedUniformBuffers[i]);
    }
    commandBuffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    // Reference Counting

    for (i = 0; i < commandBuffer->usedBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedBuffers[i]->refCount);
    }
    commandBuffer->usedBufferCount = 0;

    for (i = 0; i < commandBuffer->usedTextureCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedTextures[i]->refCount);
    }
    commandBuffer->usedTextureCount = 0;

    // Reset presentation
    commandBuffer->windowDataCount = 0;

    // Reset bindings
    commandBuffer->indexBuffer = NULL;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i += 1) {
        commandBuffer->vertexSamplers[i] = NULL;
        commandBuffer->vertexTextures[i] = NULL;
        commandBuffer->fragmentSamplers[i] = NULL;
        commandBuffer->fragmentTextures[i] = NULL;
        commandBuffer->computeSamplers[i] = NULL;
        commandBuffer->computeSamplerTextures[i] = NULL;
    }
    for (i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
        commandBuffer->vertexStorageTextures[i] = NULL;
        commandBuffer->fragmentStorageTextures[i] = NULL;
        commandBuffer->computeReadOnlyTextures[i] = NULL;
    }
    for (i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
        commandBuffer->vertexStorageBuffers[i] = NULL;
        commandBuffer->fragmentStorageBuffers[i] = NULL;
        commandBuffer->computeReadOnlyBuffers[i] = NULL;
    }
    for (i = 0; i < MAX_COMPUTE_WRITE_TEXTURES; i += 1) {
        commandBuffer->computeReadWriteTextures[i] = NULL;
    }
    for (i = 0; i < MAX_COMPUTE_WRITE_BUFFERS; i += 1) {
        commandBuffer->computeReadWriteBuffers[i] = NULL;
    }

    // The fence is now available (unless SubmitAndAcquireFence was called)
    if (commandBuffer->autoReleaseFence) {
        WebGPU_ReleaseFence(
            (SDL_GPURenderer *)renderer,
            (SDL_GPUFence *)commandBuffer->fence);
    }

    // Return command buffer to pool
    SDL_LockMutex(renderer->acquireCommandBufferLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->availableCommandBuffers,
        WebGPUCommandBuffer *,
        renderer->availableCommandBufferCount + 1,
        renderer->availableCommandBufferCapacity,
        renderer->availableCommandBufferCapacity + 1);

    renderer->availableCommandBuffers[renderer->availableCommandBufferCount++] = commandBuffer;
    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    // Remove this command buffer from the submitted list
    if (!cancel) {
        for (i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
            if (renderer->submittedCommandBuffers[i] == commandBuffer) {
                renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
                renderer->submittedCommandBufferCount -= 1;
            }
        }
    }
}

static bool WebGPU_INTERNAL_CreateWebGPUDevice(WebGPURenderer *renderer)
{
    // Initialize WebGPU instance so that we can request an adapter and then device
    renderer->instance = wgpuCreateInstance(NULL);
    if (!renderer->instance) {
        SDL_SetError("Failed to create WebGPU instance");
        return false;
    }

    WGPURequestAdapterOptions adapter_options = {
        .backendType = WGPUBackendType_Undefined,
    };

    WGPURequestAdapterCallbackInfo callback = {
        .callback = WebGPU_RequestAdapterCallback,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .userdata1 = renderer,
        .userdata2 = NULL,
    };

    // Request adapter using the instance and then the device using the adapter (this is done in the callback)
    wgpuInstanceRequestAdapter(renderer->instance, &adapter_options, callback);

    // This seems to be necessary to ensure that the device is created before continuing
    // This should probably be tested on all browsers to ensure that it works as expected
    // but Chrome's Dawn WebGPU implementation needs this to work
    // See: https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/input-geometry/playing-with-buffers.html
    //
    // This will not loop infinitely as the callback will set the device or deviceError
    while (!renderer->device && !renderer->deviceError) {
        SDL_Delay(1);
    }

    if (renderer->deviceError) {
        SDL_SetError("Failed to create WebGPU device");
        return false;
    }

    /*// Set our error callback for emscripten*/
    // wgpuDeviceSetUncapturedErrorCallback(renderer->device, WebGPU_ErrorCallback, renderer);

    // Acquire the queue from the device
    renderer->queue = wgpuDeviceGetQueue(renderer->device);

    // Get the adapter limits
    wgpuAdapterGetLimits(renderer->adapter, &renderer->deviceLimits);
    wgpuAdapterGetInfo(renderer->adapter, &renderer->adapterInfo);

    return true;
}

void WebGPU_SetViewport(SDL_GPUCommandBuffer *renderPass, const SDL_GPUViewport *viewport)
{
    if (renderPass == NULL) {
        return;
    }

    WebGPUCommandBuffer *commandBuffer = (WebGPUCommandBuffer *)renderPass;

    // Set the viewport
    wgpuRenderPassEncoderSetViewport(commandBuffer->renderEncoder, viewport->x, viewport->y, viewport->w, viewport->h, viewport->min_depth, viewport->max_depth);
}

void WebGPU_SetScissorRect(SDL_GPUCommandBuffer *renderPass, const SDL_Rect *scissorRect)
{
    if (renderPass == NULL) {
        return;
    }

    WebGPUCommandBuffer *commandBuffer = (WebGPUCommandBuffer *)renderPass;

    wgpuRenderPassEncoderSetScissorRect(commandBuffer->renderEncoder, scissorRect->x, scissorRect->y, scissorRect->w, scissorRect->h);
}

static void WebGPU_SetStencilReference(SDL_GPUCommandBuffer *commandBuffer,
                                       Uint8 reference)
{
    if (commandBuffer == NULL) {
        return;
    }

    wgpuRenderPassEncoderSetStencilReference(((WebGPUCommandBuffer *)commandBuffer)->renderEncoder, reference);
}

static void WebGPU_SetBlendConstants(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_FColor blendConstants)
{
    if (commandBuffer == NULL) {
        return;
    }

    wgpuRenderPassEncoderSetBlendConstant(((WebGPUCommandBuffer *)commandBuffer)->renderEncoder,
                                          &(WGPUColor){
                                              .r = blendConstants.r,
                                              .g = blendConstants.g,
                                              .b = blendConstants.b,
                                              .a = blendConstants.a,
                                          });
}

static WGPUTextureView WebGPU_INTERNAL_CreateLayerView(WebGPURenderer *renderer,
                                                       WebGPUTextureContainer *container,
                                                       uint32_t layer)
{
    SDL_GPUTextureCreateInfo *info = &container->header.info;
    const char *view_desc_label = "SDL_GPU Temporary Layer View";
    WGPUTextureViewDescriptor viewDesc = {
        .format = SDLToWGPUTextureFormat(info->format),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = info->num_levels,
        .baseArrayLayer = layer,
        .arrayLayerCount = 1,
        .label = { view_desc_label, SDL_strlen(view_desc_label) },
    };

    WGPUTextureView view;
    if (info->type == SDL_GPU_TEXTURETYPE_3D && info->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {

        // const char *label = "3D -> 2D Texture View";
        // WGPUTexture newTex = wgpuDeviceCreateTexture(
        //     renderer->device,
        //     &(WGPUTextureDescriptor) {
        //         .dimension = WGPUTextureDimension_2D,
        //         .format = SDLToWGPUTextureFormat(info->format),
        //         .mipLevelCount = info->num_levels,
        //         .sampleCount = SDLToWGPUSampleCount(info->sample_count),
        //         .usage = SDLToWGPUTextureUsageFlags(info->usage, info->format, info->type),
        //         .label = { label, SDL_strlen(label) }
        //     });
    }

    view = wgpuTextureCreateView(container->activeTexture->handle, &viewDesc);

    return view;
}

static void WebGPU_BeginRenderPass(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUColorTargetInfo *colorTargetInfos,
    Uint32 numColorTargets,
    const SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;
    WebGPUWindowData *windowData = renderer->claimedWindows[0];

    Uint32 vpWidth = SDL_MAX_UINT32;
    Uint32 vpHeight = SDL_MAX_UINT32;
    SDL_GPUViewport viewport;
    SDL_Rect scissorRect;
    SDL_FColor blendConstants;
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Beginning render pass, frame: %u", windowData ? windowData->frameCounter : 0);

    // Set color attachments
    WGPURenderPassColorAttachment colorAttachments[numColorTargets];
    for (Uint32 i = 0; i < numColorTargets; i++) {
        WebGPUTextureContainer *container = (WebGPUTextureContainer *)colorTargetInfos[i].texture;
        const SDL_GPUColorTargetInfo *colorInfo = &colorTargetInfos[i];

        WebGPUTexture *texture = WebGPU_INTERNAL_PrepareTextureForWrite(
            renderer,
            container,
            colorInfo->cycle);

        if (!texture || !texture->handle) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Invalid texture or handle in BeginRenderPass (frame %u): %p, handle: %p, attempting recovery",
                        windowData ? windowData->frameCounter : 0, texture, texture ? texture->handle : NULL);
            WebGPU_INTERNAL_RecreateSwapchain(renderer, windowData);
            texture = WebGPU_INTERNAL_PrepareTextureForWrite(renderer, container, colorInfo->cycle);
            if (!texture || !texture->handle) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to recover texture handle (frame %u), skipping render",
                            windowData ? windowData->frameCounter : 0);
                return; // Skip this render pass
            }
        }

        // Create WGPU texture view from our texture so we can display it
        WGPUTextureView view = WebGPU_INTERNAL_CreateLayerView(
            renderer,
            container,
            colorInfo->layer_or_depth_plane);

        // Create WGPU color attachment
        colorAttachments[i] = (WGPURenderPassColorAttachment){
            .view = view,
            .loadOp = SDLToWGPULoadOp(colorInfo->load_op),
            .storeOp = SDLToWGPUStoreOp(colorInfo->store_op),
            .depthSlice = container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? colorInfo->layer_or_depth_plane : ~0u,
            .clearValue = (WGPUColor){
                .r = colorInfo->clear_color.r,
                .g = colorInfo->clear_color.g,
                .b = colorInfo->clear_color.b,
                .a = colorInfo->clear_color.a,
            }
        };

        WebGPU_INTERNAL_TrackTexture(webgpuCommandBuffer, texture);

        // Create resolve texture and texture view if necessary.
        if (colorInfo->store_op == SDL_GPU_STOREOP_RESOLVE || colorInfo->store_op == SDL_GPU_STOREOP_RESOLVE_AND_STORE) {
            WebGPUTextureContainer *resolveContainer = (WebGPUTextureContainer *)colorInfo->resolve_texture;
            WebGPUTexture *resolveTexture = WebGPU_INTERNAL_PrepareTextureForWrite(
                renderer,
                resolveContainer,
                colorInfo->cycle_resolve_texture);

            colorAttachments[i].resolveTarget = WebGPU_INTERNAL_CreateLayerView(
                renderer,
                container,
                colorInfo->resolve_layer);

            WebGPU_INTERNAL_TrackTexture(webgpuCommandBuffer, resolveTexture);
        }
    }

    // Set depth stencil if necessary
    WGPURenderPassDepthStencilAttachment depthStencilAttachment = { 0 };
    if (depthStencilTargetInfo != NULL) {
        WebGPUTextureContainer *container = (WebGPUTextureContainer *)depthStencilTargetInfo->texture;
        WebGPUTexture *texture = WebGPU_INTERNAL_PrepareTextureForWrite(
            renderer,
            container,
            depthStencilTargetInfo->cycle);

        const char *depth_label = "SDL_GPU Temporary Depth Stencil View";
        depthStencilAttachment.view = wgpuTextureCreateView(
            texture->handle,
            &(WGPUTextureViewDescriptor){
                .format = SDLToWGPUTextureFormat(container->header.info.format),
                .dimension = SDLToWGPUTextureViewDimension(container->header.info.type),
                .baseMipLevel = 0,
                .mipLevelCount = 1,
                .baseArrayLayer = 0,
                .arrayLayerCount = 1,
                .label = { depth_label, SDL_strlen(depth_label) },
            });

        depthStencilAttachment.depthClearValue = depthStencilTargetInfo->clear_depth;
        depthStencilAttachment.stencilClearValue = depthStencilTargetInfo->clear_stencil;
        depthStencilAttachment.depthLoadOp = SDLToWGPULoadOp(depthStencilTargetInfo->load_op);
        depthStencilAttachment.depthStoreOp = SDLToWGPUStoreOp(depthStencilTargetInfo->store_op);
        depthStencilAttachment.stencilLoadOp = SDLToWGPULoadOp(depthStencilTargetInfo->stencil_load_op);
        depthStencilAttachment.stencilStoreOp = SDLToWGPUStoreOp(depthStencilTargetInfo->stencil_store_op);

        WebGPU_INTERNAL_TrackTexture(webgpuCommandBuffer, texture);
    }

    const char *renderpass_label = "SDL_GPU Render Pass";
    WGPURenderPassDescriptor passDescriptor = {
        .colorAttachmentCount = numColorTargets,
        .colorAttachments = colorAttachments,
        .depthStencilAttachment = depthStencilTargetInfo ? &depthStencilAttachment : NULL,
        .label = { renderpass_label, SDL_strlen(renderpass_label) },
    };

    // Create the render pass encoder
    webgpuCommandBuffer->renderEncoder = wgpuCommandEncoderBeginRenderPass(
        webgpuCommandBuffer->handle,
        &passDescriptor);

    // The viewport cannot be larger than the smallest target.
    for (Uint32 i = 0; i < numColorTargets; i += 1) {
        WebGPUTextureContainer *container = (WebGPUTextureContainer *)colorTargetInfos[i].texture;
        Uint32 w = container->header.info.width >> colorTargetInfos[i].mip_level;
        Uint32 h = container->header.info.height >> colorTargetInfos[i].mip_level;

        if (w < vpWidth) {
            vpWidth = w;
        }

        if (h < vpHeight) {
            vpHeight = h;
        }
    }

    if (depthStencilTargetInfo != NULL) {
        WebGPUTextureContainer *container = (WebGPUTextureContainer *)depthStencilTargetInfo->texture;
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
    viewport.min_depth = 0;
    viewport.max_depth = 1;
    WebGPU_SetViewport(commandBuffer, &viewport);

    scissorRect.x = 0;
    scissorRect.y = 0;
    scissorRect.w = vpWidth;
    scissorRect.h = vpHeight;
    WebGPU_SetScissorRect(commandBuffer, &scissorRect);

    blendConstants.r = 1.0f;
    blendConstants.g = 1.0f;
    blendConstants.b = 1.0f;
    blendConstants.a = 1.0f;
    WebGPU_SetBlendConstants(
        commandBuffer,
        blendConstants);

    WebGPU_SetStencilReference(
        commandBuffer,
        0);
}

static void WebGPU_EndRenderPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // Finish the render pass with all bind groups set
    wgpuRenderPassEncoderEnd(webgpuCommandBuffer->renderEncoder);
    wgpuRenderPassEncoderRelease(webgpuCommandBuffer->renderEncoder);
}

static inline Uint32 WebGPU_INTERNAL_NextHighestAlignment(
    Uint32 n,
    Uint32 align)
{
    return align * ((n + align - 1) / align);
}

static void WebGPU_INTERNAL_PushUniformData(
    WebGPUCommandBuffer *webgpuCommandBuffer,
    SDL_GPUShaderStage shaderStage,
    Uint32 slotIndex,
    const void *data,
    Uint32 length)
{
    WebGPUUniformBuffer *webgpuUniformBuffer;
    Uint32 alignedDataLength;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        if (webgpuCommandBuffer->vertexUniformBuffers[slotIndex] == NULL) {
            webgpuCommandBuffer->vertexUniformBuffers[slotIndex] = WebGPU_INTERNAL_AcquireUniformBufferFromPool(
                webgpuCommandBuffer);
        }
        webgpuUniformBuffer = webgpuCommandBuffer->vertexUniformBuffers[slotIndex];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        if (webgpuCommandBuffer->fragmentUniformBuffers[slotIndex] == NULL) {
            webgpuCommandBuffer->fragmentUniformBuffers[slotIndex] = WebGPU_INTERNAL_AcquireUniformBufferFromPool(
                webgpuCommandBuffer);
        }
        webgpuUniformBuffer = webgpuCommandBuffer->fragmentUniformBuffers[slotIndex];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        if (webgpuCommandBuffer->computeUniformBuffers[slotIndex] == NULL) {
            webgpuCommandBuffer->computeUniformBuffers[slotIndex] = WebGPU_INTERNAL_AcquireUniformBufferFromPool(
                webgpuCommandBuffer);
        }
        webgpuUniformBuffer = webgpuCommandBuffer->computeUniformBuffers[slotIndex];
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
        return;
    }

    alignedDataLength = WebGPU_INTERNAL_NextHighestAlignment(
        length,
        256);

    if (webgpuUniformBuffer->writeOffset + alignedDataLength >= UNIFORM_BUFFER_SIZE) {
        webgpuUniformBuffer = WebGPU_INTERNAL_AcquireUniformBufferFromPool(
            webgpuCommandBuffer);

        webgpuUniformBuffer->writeOffset = 0;
        webgpuUniformBuffer->drawOffset = 0;

        if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
            webgpuCommandBuffer->vertexUniformBuffers[slotIndex] = webgpuUniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
            webgpuCommandBuffer->fragmentUniformBuffers[slotIndex] = webgpuUniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
            webgpuCommandBuffer->computeUniformBuffers[slotIndex] = webgpuUniformBuffer;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
            return;
        }
    }

    webgpuUniformBuffer->drawOffset = webgpuUniformBuffer->writeOffset;
    wgpuQueueWriteBuffer(
        webgpuCommandBuffer->renderer->queue,
        webgpuUniformBuffer->buffer,
        webgpuUniformBuffer->writeOffset,
        data,
        length);

    webgpuUniformBuffer->writeOffset += alignedDataLength;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        webgpuCommandBuffer->needVertexUniformBind = true;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        webgpuCommandBuffer->needFragmentUniformBind = true;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        webgpuCommandBuffer->needComputeUniformBind = true;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
    }
}

static void WebGPU_PushVertexUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 length)
{
    WebGPU_INTERNAL_PushUniformData(
        (WebGPUCommandBuffer *)commandBuffer,
        SDL_GPU_SHADERSTAGE_VERTEX,
        slotIndex,
        data,
        length);
}

static void WebGPU_PushFragmentUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 length)
{
    WebGPU_INTERNAL_PushUniformData(
        (WebGPUCommandBuffer *)commandBuffer,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        slotIndex,
        data,
        length);
}

static void WebGPU_Blit(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUBlitInfo *info)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = (WebGPURenderer *)webgpuCommandBuffer->renderer;

    SDL_GPU_BlitCommon(
        commandBuffer,
        info,
        renderer->blitLinearSampler,
        renderer->blitNearestSampler,
        renderer->blitVertexShader,
        renderer->blitFrom2DShader,
        renderer->blitFrom2DArrayShader,
        renderer->blitFrom3DShader,
        renderer->blitFromCubeShader,
        renderer->blitFromCubeArrayShader,
        &renderer->blitPipelines,
        &renderer->blitPipelineCount,
        &renderer->blitPipelineCapacity);
}

static bool WebGPU_Submit(
    SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;

    SDL_LockMutex(renderer->submitLock);

    if (!WebGPU_INTERNAL_AcquireFence(renderer, webgpuCommandBuffer)) {
        SDL_UnlockMutex(renderer->submitLock);
        return false;
    }

    for (Uint32 i = 0; i < webgpuCommandBuffer->usedBufferCount; i++) {
        WebGPUBuffer *buffer = webgpuCommandBuffer->usedBuffers[i];
        if (buffer->isMapped) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Buffer %p still mapped during submit", buffer);
        }
    }

    if (webgpuCommandBuffer->copyEncoder) {
        webgpuCommandBuffer->commandBuffer = wgpuCommandEncoderFinish(webgpuCommandBuffer->copyEncoder, NULL);
        wgpuQueueSubmit(webgpuCommandBuffer->renderer->queue, 1, &webgpuCommandBuffer->commandBuffer);
        WebGPU_INTERNAL_IncrementBufferRefCounts(webgpuCommandBuffer);
        SDL_SetAtomicInt(&webgpuCommandBuffer->fence->complete, 1);
        WebGPU_INTERNAL_DecrementBufferRefCounts(webgpuCommandBuffer);
        webgpuCommandBuffer->copyEncoder = NULL;
    }

    // Enqueue present requests, if applicable
    for (Uint32 i = 0; i < webgpuCommandBuffer->windowDataCount; i += 1) {
        WebGPUWindowData *windowData = webgpuCommandBuffer->windowDatas[i];

        windowData->inFlightFences[windowData->frameCounter] = (SDL_GPUFence *)webgpuCommandBuffer->fence;

        (void)SDL_AtomicIncRef(&webgpuCommandBuffer->fence->referenceCount);

        windowData->frameCounter = (windowData->frameCounter + 1) % renderer->allowedFramesInFlight;
    }

    // Create our command buffer
    const char *cmd_buf_label = "SDL_GPU Command Buffer";
    webgpuCommandBuffer->commandBuffer = wgpuCommandEncoderFinish(
        webgpuCommandBuffer->handle,
        &(WGPUCommandBufferDescriptor){
            .label = { cmd_buf_label, SDL_strlen(cmd_buf_label) },
        });

    // Submit the command buffer with a callback to release the fence
    WGPUQueueWorkDoneCallbackInfo callback = {
        .callback = WebGPU_INTERNAL_FrameCallback,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .userdata1 = webgpuCommandBuffer,
        .userdata2 = NULL
    };
    wgpuQueueOnSubmittedWorkDone(renderer->queue, callback);
    wgpuQueueSubmit(renderer->queue, 1, &webgpuCommandBuffer->commandBuffer);

    // Release the command buffer and the command encoder
    wgpuCommandBufferRelease(webgpuCommandBuffer->commandBuffer);
    wgpuCommandEncoderRelease(webgpuCommandBuffer->handle);

    // Mark the command buffer as submitted
    if (renderer->submittedCommandBufferCount >= renderer->submittedCommandBufferCapacity) {
        renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

        renderer->submittedCommandBuffers = SDL_realloc(
            renderer->submittedCommandBuffers,
            sizeof(WebGPUCommandBuffer *) * renderer->submittedCommandBufferCapacity);
    }
    renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = webgpuCommandBuffer;
    renderer->submittedCommandBufferCount += 1;

    // Check if we can perform any cleanups
    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        if (SDL_GetAtomicInt(&renderer->submittedCommandBuffers[i]->fence->complete)) {
            WebGPU_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i],
                false);
        }
    }

    WebGPU_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->submitLock);

    return true;
}

static bool WebGPU_PrepareDriver(SDL_VideoDevice *_this)
{
    if (_this->WebGPU_CreateSurface == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU_CreateSurface == NULL!");
        return false;
    }

    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_INFO);

    // Realistically, we should check if the browser supports WebGPU here and return false if it doesn't
    // For now, we'll just return true because it'll simply crash if the browser doesn't support WebGPU anyways
    return true;
}

static void WebGPU_DestroyDevice(SDL_GPUDevice *device)
{
    WebGPURenderer *renderer = (WebGPURenderer *)device->driverData;

    WebGPU_INTERNAL_PerformPendingDestroys(renderer);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Performed pending destroys");

    // Destroy all claimed windows
    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        WebGPU_ReleaseWindow(device->driverData, renderer->claimedWindows[i]->window);
    }
    SDL_free(renderer->claimedWindows);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released all claimed windows");

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Releasing pipeline bindgroup cache");
    SDL_free(renderer->pipelineBindGroupCache);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released pipeline bindgroup cache");

    // TODO: Release Blit resources
    // WebGPU_INTERNAL_DestroyBlitResources(renderer);

    // Release uniform buffers
    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        wgpuBufferRelease(renderer->uniformBufferPool[i]->buffer);
    }
    SDL_free(renderer->uniformBufferPool);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released uniform buffers");

    SDL_free(renderer->bufferContainersToDestroy);
    SDL_free(renderer->textureContainersToDestroy);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released containers to destroy");

    // Release command buffer infrastructure
    for (Uint32 i = 0; i < renderer->availableCommandBufferCount; i += 1) {
        WebGPUCommandBuffer *commandBuffer = renderer->availableCommandBuffers[i];
        SDL_free(commandBuffer->usedBuffers);
        SDL_free(commandBuffer->usedTextures);
        SDL_free(commandBuffer->usedUniformBuffers);
        SDL_free(commandBuffer->windowDatas);
        SDL_free(commandBuffer);
    }
    SDL_free(renderer->availableCommandBuffers);
    SDL_free(renderer->submittedCommandBuffers);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released command buffer infra");

    // Release fence infrastructure
    for (Uint32 i = 0; i < renderer->availableFenceCount; i += 1) {
        SDL_free(renderer->availableFences[i]);
    }
    SDL_free(renderer->availableFences);
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Released fence infra");

    // Release the mutexes
    SDL_DestroyMutex(renderer->submitLock);
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->acquireUniformBufferLock);
    SDL_DestroyMutex(renderer->disposeLock);
    SDL_DestroyMutex(renderer->fenceLock);
    SDL_DestroyMutex(renderer->windowLock);

    // Destroy the queue
    wgpuQueueRelease(renderer->queue);

    // Destroy the device
    wgpuDeviceDestroy(renderer->device);

    // Destroy the adapter
    wgpuAdapterRelease(renderer->adapter);

    // Destroy the instance
    wgpuInstanceRelease(renderer->instance);

    // Free the primary structures
    SDL_free(renderer);
    SDL_free(device);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SHUTDOWN: Device destroyed successfully!");
}

static SDL_GPUDevice *WebGPU_CreateDevice(bool debug, bool preferLowPower, SDL_PropertiesID props)
{
    WebGPURenderer *renderer;
    SDL_GPUDevice *result = NULL;

    // Initialize the WebGPURenderer to be used as the driver data for the SDL_GPUDevice
    renderer = (WebGPURenderer *)SDL_calloc(sizeof(WebGPURenderer), 1);
    SDL_zero(*renderer);
    renderer->debugMode = debug;
    renderer->preferLowPower = preferLowPower;
    renderer->deviceError = false;

    // This function loops until the WGPUDevice is created
    if (!WebGPU_INTERNAL_CreateWebGPUDevice(renderer)) {
        SDL_free(renderer);
        SDL_SetError("Failed to create WebGPU device");
        return NULL;
    }

    renderer->allowedFramesInFlight = MAX_FRAMES_IN_FLIGHT;

    renderer->submitLock = SDL_CreateMutex();
    renderer->acquireCommandBufferLock = SDL_CreateMutex();
    renderer->acquireUniformBufferLock = SDL_CreateMutex();
    renderer->disposeLock = SDL_CreateMutex();
    renderer->fenceLock = SDL_CreateMutex();
    renderer->windowLock = SDL_CreateMutex();

    WebGPU_INTERNAL_AllocateCommandBuffers(renderer, 2);

    // Create fence pool
    renderer->availableFenceCapacity = 2;
    renderer->availableFences = SDL_calloc(
        renderer->availableFenceCapacity, sizeof(WebGPUFence *));

    // Create uniform buffer pool
    renderer->uniformBufferPoolCapacity = 32;
    renderer->uniformBufferPoolCount = 32;
    renderer->uniformBufferPool = SDL_calloc(
        renderer->uniformBufferPoolCapacity, sizeof(WebGPUUniformBuffer *));

    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i] = WebGPU_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    // Create deferred destroy arrays
    renderer->bufferContainersToDestroyCapacity = 2;
    renderer->bufferContainersToDestroyCount = 0;
    renderer->bufferContainersToDestroy = SDL_calloc(
        renderer->bufferContainersToDestroyCapacity, sizeof(WebGPUBufferContainer *));

    renderer->textureContainersToDestroyCapacity = 2;
    renderer->textureContainersToDestroyCount = 0;
    renderer->textureContainersToDestroy = SDL_calloc(
        renderer->textureContainersToDestroyCapacity, sizeof(WebGPUTextureContainer *));

    // Create claimed window list
    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindows = SDL_calloc(
        renderer->claimedWindowCapacity, sizeof(WebGPUWindowData *));

    // Create our bind group cache to avoid recreating bind groups each frame
    // 6 are initially reserved for blit pipelines
    renderer->pipelineBindGroupCacheCapacity = 16;
    renderer->pipelineBindGroupCacheCount = 0;
    renderer->pipelineBindGroupCache = SDL_calloc(
        renderer->pipelineBindGroupCacheCapacity, sizeof(WebGPUPipelineBindGroupCache));

    for (Uint32 i = 0; i < renderer->pipelineBindGroupCacheCapacity; i++) {
        renderer->pipelineBindGroupCache[i].pipeline = NULL;
    }

    // TODO: We should initialize the blit pipelines here and then cache them
    WebGPU_INTERNAL_InitBlitResources(renderer);

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SDL_GPU Driver: WebGPU");
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "WebGPU Device: %s (Only for debugging purposes, not feature detection)",
                renderer->adapterInfo.description.data);

    // Initialize our SDL_GPUDevice
    result = (SDL_GPUDevice *)SDL_malloc(sizeof(SDL_GPUDevice));

    /* Assign driver functions */
    // Eventaually we should just call the ASSIGN macro here instead of manually assigning
    result->driverData = (SDL_GPURenderer *)renderer;
    result->DestroyDevice = WebGPU_DestroyDevice;
    result->ClaimWindow = WebGPU_ClaimWindow;
    result->ReleaseWindow = WebGPU_ReleaseWindow;
    result->SupportsTextureFormat = WebGPU_SupportsTextureFormat;
    result->SupportsPresentMode = WebGPU_SupportsPresentMode;
    result->SupportsSampleCount = WebGPU_SupportsSampleCount;
    result->SupportsSwapchainComposition = WebGPU_SupportsSwapchainComposition;
    result->GetSwapchainTextureFormat = WebGPU_GetSwapchainTextureFormat;
    result->AcquireSwapchainTexture = WebGPU_AcquireSwapchainTexture;
    result->WaitAndAcquireSwapchainTexture = WebGPU_WaitAndAcquireSwapchainTexture;
    result->AcquireCommandBuffer = WebGPU_AcquireCommandBuffer;
    result->ReleaseFence = WebGPU_ReleaseFence;
    result->BeginRenderPass = WebGPU_BeginRenderPass;
    result->EndRenderPass = WebGPU_EndRenderPass;
    result->Submit = WebGPU_Submit;

    result->CreateBuffer = WebGPU_CreateBuffer;
    result->ReleaseBuffer = WebGPU_ReleaseBuffer;
    result->SetBufferName = WebGPU_SetBufferName;
    result->CreateTransferBuffer = WebGPU_CreateTransferBuffer;
    result->ReleaseTransferBuffer = WebGPU_ReleaseTransferBuffer;
    result->CreateTransferBuffer = WebGPU_CreateTransferBuffer;
    result->MapTransferBuffer = WebGPU_MapTransferBuffer;
    result->ReleaseTransferBuffer = WebGPU_ReleaseTransferBuffer;
    result->UnmapTransferBuffer = WebGPU_UnmapTransferBuffer;

    result->BeginCopyPass = WebGPU_BeginCopyPass;
    result->EndCopyPass = WebGPU_EndCopyPass;
    result->UploadToBuffer = WebGPU_UploadToBuffer;
    result->DownloadFromBuffer = WebGPU_DownloadFromBuffer;
    result->CopyBufferToBuffer = WebGPU_CopyBufferToBuffer;
    result->BindVertexBuffers = WebGPU_BindVertexBuffers;
    result->BindIndexBuffer = WebGPU_BindIndexBuffer;

    result->PushVertexUniformData = WebGPU_PushVertexUniformData;
    result->PushFragmentUniformData = WebGPU_PushFragmentUniformData;

    result->CreateSampler = WebGPU_CreateSampler;
    result->ReleaseSampler = WebGPU_ReleaseSampler;
    result->BindFragmentSamplers = WebGPU_BindFragmentSamplers;

    result->CreateTexture = WebGPU_CreateTexture;
    result->ReleaseTexture = WebGPU_ReleaseTexture;
    result->SetTextureName = WebGPU_SetTextureName;
    result->UploadToTexture = WebGPU_UploadToTexture;

    result->CreateShader = WebGPU_CreateShader;
    result->ReleaseShader = WebGPU_ReleaseShader;
    result->CreateComputePipeline = WebGPU_CreateComputePipeline;
    result->CreateGraphicsPipeline = WebGPU_CreateGraphicsPipeline;
    result->ReleaseGraphicsPipeline = WebGPU_ReleaseGraphicsPipeline;
    result->BindGraphicsPipeline = WebGPU_BindGraphicsPipeline;
    result->Blit = WebGPU_Blit;
    result->DrawPrimitives = WebGPU_DrawPrimitives;
    result->DrawIndexedPrimitives = WebGPU_DrawIndexedPrimitives;
    result->Wait = WebGPU_Wait;
    result->SetViewport = WebGPU_SetViewport;
    result->SetScissor = WebGPU_SetScissorRect;
    result->SetStencilReference = WebGPU_SetStencilReference;
    result->SetBlendConstants = WebGPU_SetBlendConstants;

    result->driverData = (SDL_GPURenderer *)renderer;
    renderer->sdlDevice = result;

    return result;
}

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    SDL_GPU_SHADERFORMAT_WGSL, // | SDL_GPU_SHADERFORMAT_SPIRV,
    WebGPU_PrepareDriver,
    WebGPU_CreateDevice,
};
