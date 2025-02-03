/*
  Simple DirectMedia Layer
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
// License: Zlib
// Description: WebGPU driver for SDL_gpu using the emscripten WebGPU implementation
// Note: Compiling SDL GPU programs using emscripten will require -sUSE_WEBGPU=1 -sASYNCIFY=1

#include "../SDL_sysgpu.h"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <regex.h>

// TODO: REMOVE
// Code compiles without these but my LSP freaks out without them
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_mutex.h>

// I currently have a copy of the webgpu.h file in the local include directory:
// - usr/local/include/webgpu/webgpu.h
// it can be downloaded from here: https://github.com/webgpu-native/webgpu-headers/blob/main/webgpu.h
//
// The code compiles without it as long as Emscripten is used and the -sUSE_WEBGPU flag is set
#include <webgpu/webgpu.h>

// TODO: Implement Command Bufffer pool for WebGPU. It is important to note that CommandEncoders are consumed by the CommandBuffer and are not reusable.

#define MAX_UBO_SECTION_SIZE          4096 // 4   KiB
#define DESCRIPTOR_POOL_STARTING_SIZE 128
#define WINDOW_PROPERTY_DATA          "SDL_GPUWebGPUWindowPropertyData"
#define MAX_BIND_GROUPS               8
#define MAX_BIND_GROUP_ENTRIES        32
#define MAX_PIPELINE_BINDINGS         32
#define MAX_ENTRYPOINT_LENGTH         64

#define TRACK_RESOURCE(resource, type, array, count, capacity) \
    Uint32 i;                                                  \
                                                               \
    for (i = 0; i < commandBuffer->count; i += 1) {            \
        if (commandBuffer->array[i] == resource) {             \
            return;                                            \
        }                                                      \
    }                                                          \
                                                               \
    if (commandBuffer->count == commandBuffer->capacity) {     \
        commandBuffer->capacity += 1;                          \
        commandBuffer->array = SDL_realloc(                    \
            commandBuffer->array,                              \
            commandBuffer->capacity * sizeof(type));           \
    }                                                          \
    commandBuffer->array[commandBuffer->count] = resource;     \
    commandBuffer->count += 1;                                 \
    SDL_AtomicIncRef(&resource->referenceCount);

#define INTERNAL_PRINT_32BITS(value)                  \
    do {                                              \
        char buffer[64];                              \
        int pos = 0;                                  \
        for (int i = 31; i >= 0; i--) {               \
            buffer[pos++] = ((value >> i) & 1) + '0'; \
            if (i % 8 == 0 && i > 0) {                \
                buffer[pos++] = ' ';                  \
            }                                         \
        }                                             \
        buffer[pos] = '\0';                           \
        SDL_Log("%s", buffer);                        \
    } while (0)

// Typedefs and Enums
// ---------------------------------------------------

typedef enum WebGPUBindingType
{
    WGPUBindingType_Undefined = 0x00000000,
    WGPUBindingType_Buffer = 0x00000001,
    WGPUBindingType_Sampler = 0x00000002,
    WGPUBindingType_Texture = 0x00000003,
    WGPUBindingType_StorageTexture = 0x00000004,
    /**/
    /*// Buffer sub-types*/
    WGPUBindingType_UniformBuffer = 0x00000011,
    /*WGPUBindingType_StorageBuffer = 0x00000012,*/
    /*WGPUBindingType_ReadOnlyStorageBuffer = 0x00000013,*/
    /**/
    /*// Sampler sub-types*/
    /*WGPUBindingType_FilteringSampler = 0x00000022,*/
    /*WGPUBindingType_NonFilteringSampler = 0x00000023,*/
    /*WGPUBindingType_ComparisonSampler = 0x00000024,*/
    /**/
    /*// Texture sub-types*/
    /*WGPUBindingType_FilteringTexture = 0x00000033,*/
    /*WGPUBindingType_NonFilteringTexture = 0x00000034,*/
    /**/
    /*// Storage Texture sub-types (access modes)*/
    /*WGPUBindingType_WriteOnlyStorageTexture = 0x00000044,*/
    /*WGPUBindingType_ReadOnlyStorageTexture = 0x00000045,*/
    /*WGPUBindingType_ReadWriteStorageTexture = 0x00000046,*/
} WebGPUBindingType;

static const char *WebGPU_GetBindingTypeString(WebGPUBindingType type)
{
    switch (type) {
    case WGPUBindingType_Buffer:
        return "Buffer";
    case WGPUBindingType_UniformBuffer:
        return "UniformBuffer";
    case WGPUBindingType_Sampler:
        return "Sampler";
    case WGPUBindingType_Texture:
        return "Texture";
    default:
        return "Undefined";
    }
}

// WebGPU Structures
// ---------------------------------------------------
typedef struct WebGPUBuffer WebGPUBuffer;
typedef struct WebGPUTexture WebGPUTexture;
typedef struct WebGPUSampler WebGPUSampler;

typedef struct WebGPUViewport
{
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
} WebGPUViewport;

typedef struct WebGPURect
{
    Uint32 x;
    Uint32 y;
    Uint32 width;
    Uint32 height;
} WebGPURect;

// Buffer structures
// ---------------------------------------------------
typedef enum WebGPUBufferType
{
    WEBGPU_BUFFER_TYPE_GPU,
    WEBGPU_BUFFER_TYPE_UNIFORM,
    WEBGPU_BUFFER_TYPE_TRANSFER
} WebGPUBufferType;

typedef struct WebGPUUniformBuffer
{
    WebGPUBuffer *buffer;
    Uint8 group;
    Uint8 binding;
    Uint32 drawOffset;
    Uint32 writeOffset;
} WebGPUUniformBuffer;

typedef struct WebGPUBuffer
{
    WGPUBuffer buffer;
    Uint32 size;
    WebGPUBufferType type;
    SDL_GPUBufferUsageFlags usageFlags;
    SDL_AtomicInt referenceCount;
    Uint8 markedForDestroy;
    bool isMapped;
    void *mappedData;
    SDL_AtomicInt mappingComplete;
    WebGPUUniformBuffer *uniformBufferForDefrag;
    char *debugName;
} WebGPUBuffer;

// Bind Group Layout definitions
// ---------------------------------------------------
typedef enum WebGPUShaderStage
{
    WEBGPU_SHADER_STAGE_NONE = 0x00000000,
    WEBGPU_SHADER_STAGE_VERTEX = 0x00000001,
    WEBGPU_SHADER_STAGE_FRAGMENT = 0x00000002,
    WEBGPU_SHADER_STAGE_COMPUTE = 0x00000003,
} WebGPUShaderStage;

typedef struct WebGPUBindingInfo
{
    unsigned int group;
    unsigned int binding;
    WebGPUBindingType type;
    WebGPUShaderStage stage;
    WGPUTextureViewDimension viewDimension;
} WebGPUBindingInfo;

typedef struct WebGPUBindGroupLayout
{
    WGPUBindGroupLayout layout;
    Uint8 group;
    WebGPUBindingInfo bindings[MAX_BIND_GROUP_ENTRIES];
    size_t bindingCount;
} WebGPUBindGroupLayout;

typedef struct WebGPUPipelineResourceLayout
{
    WGPUPipelineLayout pipelineLayout;
    WebGPUBindGroupLayout bindGroupLayouts[MAX_BIND_GROUPS];
    Uint32 bindGroupLayoutCount;
} WebGPUPipelineResourceLayout;
// ---------------------------------------------------

// Bind Group Functions For Binding
// ---------------------------------------------------
typedef struct WebGPUBindGroup
{
    WGPUBindGroup bindGroup;
    WGPUBindGroupEntry entries[MAX_BIND_GROUP_ENTRIES];
    size_t entryCount;
    bool cycleBindings;
} WebGPUBindGroup;

// ---------------------------------------------------

// Texture structures

struct WebGPUTexture
{
    TextureCommonHeader common;

    WGPUTexture texture;
    WGPUTextureView fullView;
    WGPUExtent3D dimensions;

    SDL_GPUTextureType type;
    Uint8 isMSAAColorTarget;

    Uint32 depth;
    Uint32 layerCount;
    Uint32 levelCount;
    SDL_GPUTextureFormat format;
    SDL_GPUTextureUsageFlags usage;

    Uint8 markedForDestroy;
    SDL_AtomicInt referenceCount;

    Uint8 canBeCycled;
    char *debugName;
};

typedef struct WebGPUSampler
{
    WGPUSampler sampler;
    SDL_AtomicInt referenceCount;
} WebGPUSampler;

// Swapchain structures
typedef struct SwapchainSupportDetails
{
    // should just call wgpuSurfaceGetPreferredFormat
    WGPUTextureFormat *formats;
    Uint32 formatsLength;
    WGPUPresentMode *presentModes;
    Uint32 presentModesLength;
} SwapchainSupportDetails;

typedef struct WebGPUSwapchainData
{
    WGPUSurface surface;
    WGPUSurfaceDescriptor surfaceDesc;
    WGPUTextureFormat format;
    WGPUPresentMode presentMode;

    WGPUTexture depthStencilTexture;
    WGPUTextureView depthStencilView;

    WGPUTexture msaaTexture;
    WGPUTextureView msaaView;

    Uint32 width;
    Uint32 height;
    Uint32 sampleCount;

    Uint32 frameCounter;
} WebGPUSwapchainData;

typedef struct WebGPUWindow
{
    SDL_Window *window;
    SDL_GPUSwapchainComposition swapchainComposition;
    SDL_GPUPresentMode presentMode;
    WebGPUSwapchainData swapchainData;
    bool needsSwapchainRecreate;
} WebGPUWindow;

typedef struct WebGPUShader
{
    WGPUShaderModule shaderModule;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;
    SDL_AtomicInt referenceCount;
    char *wgslSource;
    char entrypoint[MAX_ENTRYPOINT_LENGTH];
} WebGPUShader;

// Graphics Pipeline definitions
typedef struct WebGPUGraphicsPipeline
{
    WGPURenderPipeline pipeline;
    SDL_GPUPrimitiveType primitiveType;
    WebGPUPipelineResourceLayout *resourceLayout;
    WebGPUBindGroup bindGroups[MAX_BIND_GROUPS];
    Uint32 bindGroupCount;
    WebGPUShader *vertexShader;
    WebGPUShader *fragmentShader;
    WGPURenderPipelineDescriptor pipelineDesc;

    // If BindGPUFragmentSamplers is called, this will be the hash representing all bindings provided to the function
    // Whenever BindGPUFragmentSamplers is called, if the hash matches the current hash, we can skip creating a new bind group
    size_t bindSamplerHash;
    size_t bindXXXXHash; // Reserved for future use
    size_t bindYYYYHash; // Reserved for future use
    size_t bindZZZZHash; // Reserved for future use

    bool cycleBindGroups;

    WebGPUUniformBuffer vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    Uint8 vertexUniformBufferCount;

    WebGPUUniformBuffer fragUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    Uint8 fragUniformBufferCount;

    SDL_AtomicInt referenceCount;
} WebGPUGraphicsPipeline;

// Fence structure
typedef struct WebGPUFence
{
    SDL_AtomicInt fence;
    SDL_AtomicInt referenceCount;
} WebGPUFence;

typedef struct WebGPUFencePool
{
    SDL_Mutex *lock;

    WebGPUFence **availableFences;
    Uint32 availableFenceCount;
    Uint32 availableFenceCapacity;
} WebGPUFencePool;

// Renderer Structure
typedef struct WebGPURenderer WebGPURenderer;

typedef struct WebGPUCommandPool WebGPUCommandPool;

// Renderer's command buffer structure
typedef struct WebGPUCommandBuffer
{
    CommandBufferCommonHeader common;
    WebGPURenderer *renderer;

    WGPUCommandEncoder commandEncoder;
    WGPURenderPassEncoder renderPassEncoder;
    WGPUComputePassEncoder computePassEncoder;

    // WebGPUComputePipeline *currentComputePipeline;
    WebGPUGraphicsPipeline *currentGraphicsPipeline;

    WebGPUBindGroup bindGroups[MAX_BIND_GROUPS];
    Uint32 bindGroupCount;

    /*bool resourcesDirty;*/

    WebGPUViewport currentViewport;
    WebGPURect currentScissor;
    float blendConstants[4];
    Uint8 stencilReference;

    // Used to track layer views that need to be released when the command buffer is destroyed
    WGPUTextureView layerViews[32];
    Uint32 layerViewCount;

    WebGPUGraphicsPipeline **usedGraphicsPipelines;
    Uint32 usedGraphicsPipelineCount;
    Uint32 usedGraphicsPipelineCapacity;

    WebGPUSwapchainData **usedFramebuffers;
    Uint32 usedFramebufferCount;
    Uint32 usedFramebufferCapacity;

    WebGPUUniformBuffer **usedUniformBuffers;
    Uint32 usedUniformBufferCount;
    Uint32 usedUniformBufferCapacity;

    WebGPUCommandPool *commandPool;

    // ... (other fields as needed)

    WebGPUFence *inFlightFence;
    bool autoReleaseFence;

} WebGPUCommandBuffer;

typedef struct WebGPUCommandPool
{
    SDL_ThreadID threadID;

    WebGPUCommandBuffer **activeCommandBuffers;
    WebGPUCommandBuffer **inactiveCommandBuffers;
    Uint32 inactiveCommandBufferCapacity;
    Uint32 inactiveCommandBufferCount;
} WebGPUCommandPool;

typedef struct CommandPoolHashTableKey
{
    SDL_ThreadID threadID;
} CommandPoolHashTableKey;

typedef struct WebGPURenderer
{
    bool debug;
    bool preferLowPower;

    SDL_GPUDevice *sdlDevice;
    SDL_PixelFormat pixelFormat;

    WGPUAdapterInfo adapterInfo;
    WGPUSupportedLimits physicalDeviceLimits;

    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    bool deviceError;
    WGPUQueue queue;

    WebGPUWindow **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;

    WebGPUCommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCount;
    Uint32 submittedCommandBufferCapacity;

    WebGPUFencePool fencePool;

    SDL_HashTable *commandPoolHashTable;
    SDL_HashTable *renderPassHashTable;
    SDL_HashTable *framebufferHashTable;
    SDL_HashTable *graphicsPipelineResourceLayoutHashTable;
    SDL_HashTable *computePipelineResourceLayoutHashTable;
    SDL_HashTable *descriptorSetLayoutHashTable;

    /*DescriptorSetCache **descriptorSetCachePool;*/
    Uint32 descriptorSetCachePoolCount;
    Uint32 descriptorSetCachePoolCapacity;

    WebGPUUniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    // Blit
    SDL_GPUShader *blitVertexShader;
    SDL_GPUShader *blitFrom2DShader;
    SDL_GPUShader *blitFrom2DArrayShader;
    SDL_GPUShader *blitFrom3DShader;
    SDL_GPUShader *blitFromCubeShader;
    SDL_GPUShader *blitFromCubeArrayShader;

    SDL_GPUSampler *blitNearestSampler;
    SDL_GPUSampler *blitLinearSampler;

    // Thread safety
    SDL_Mutex *allocatorLock;
    SDL_Mutex *disposeLock;
    SDL_Mutex *submitLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *acquireUniformBufferLock;
    SDL_Mutex *framebufferFetchLock;
    SDL_Mutex *windowLock;

    BlitPipelineCacheEntry *blitPipelines;
    Uint32 blitPipelineCount;
    Uint32 blitPipelineCapacity;

    Uint32 minUBOAlignment;

} WebGPURenderer;

// ---------------------------------------------------

// Conversion Functions:
// ---------------------------------------------------

static WGPUBufferUsageFlags SDLToWGPUBufferUsageFlags(SDL_GPUBufferUsageFlags usageFlags)
{
    WGPUBufferUsageFlags wgpuFlags = WGPUBufferUsage_None;
    if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX) {
        wgpuFlags |= WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    }
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX) {
        wgpuFlags |= WGPUBufferUsage_Index;
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
        SDL_Log("SDL_GPU: Invalid primitive type %d", topology);
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
        SDL_Log("SDL_GPU: Invalid front face %d. Using CCW.", frontFace);
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
        SDL_Log("SDL_GPU: Invalid cull mode %d. Using None.", cullMode);
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
        SDL_Log("SDL_GPU: Invalid index type %d. Using Uint16.", indexType);
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

static WGPUTextureUsageFlags SDLToWGPUTextureUsageFlags(SDL_GPUTextureUsageFlags usageFlags)
{
    WGPUTextureUsageFlags wgpuFlags = WGPUTextureUsage_None;

    if (usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
        wgpuFlags |= WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    }
    if (usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {
        wgpuFlags |= WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
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
        SDL_Log("SDL_GPU: Invalid texture type %d. Using 2D.", type);
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
        SDL_Log("SDL_GPU: Invalid texture type %d. Using 2D.", type);
        return WGPUTextureViewDimension_2D;
    }
}

const char *WebGPU_GetTextureViewDimensionString(WGPUTextureViewDimension dim)
{
    switch (dim) {
    case WGPUTextureViewDimension_Undefined:
        return "Undefined";
    case WGPUTextureViewDimension_1D:
        return "1D";
    case WGPUTextureViewDimension_2D:
        return "2D";
    case WGPUTextureViewDimension_2DArray:
        return "2DArray";
    case WGPUTextureViewDimension_Cube:
        return "Cube";
    case WGPUTextureViewDimension_CubeArray:
        return "CubeArray";
    case WGPUTextureViewDimension_3D:
        return "3D";
    default:
        return "Unknown";
    }
}

static SDL_GPUTextureFormat WGPUToSDLTextureFormat(WGPUTextureFormat wgpuFormat)
{
    switch (wgpuFormat) {
    case WGPUTextureFormat_R8Unorm:
        return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case WGPUTextureFormat_RG8Unorm:
        return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
    case WGPUTextureFormat_RGBA8Unorm:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case WGPUTextureFormat_R16Uint:
        return SDL_GPU_TEXTUREFORMAT_R16_UINT; // Note: This is an approximation
    case WGPUTextureFormat_RG16Uint:
        return SDL_GPU_TEXTUREFORMAT_R16G16_UINT; // Note: This is an approximation
    case WGPUTextureFormat_RGBA16Uint:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT; // Note: This is an approximation
    case WGPUTextureFormat_RGB10A2Unorm:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    case WGPUTextureFormat_BGRA8Unorm:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case WGPUTextureFormat_BC1RGBAUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
    case WGPUTextureFormat_BC2RGBAUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
    case WGPUTextureFormat_BC3RGBAUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
    case WGPUTextureFormat_BC4RUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
    case WGPUTextureFormat_BC5RGUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
    case WGPUTextureFormat_BC7RGBAUnorm:
        return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
    case WGPUTextureFormat_BC6HRGBFloat:
        return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT;
    case WGPUTextureFormat_BC6HRGBUfloat:
        return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT;
    case WGPUTextureFormat_R8Snorm:
        return SDL_GPU_TEXTUREFORMAT_R8_SNORM;
    case WGPUTextureFormat_RG8Snorm:
        return SDL_GPU_TEXTUREFORMAT_R8G8_SNORM;
    case WGPUTextureFormat_RGBA8Snorm:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM;
    case WGPUTextureFormat_R16Float:
        return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
    case WGPUTextureFormat_RG16Float:
        return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case WGPUTextureFormat_RGBA16Float:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case WGPUTextureFormat_R32Float:
        return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    case WGPUTextureFormat_RG32Float:
        return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
    case WGPUTextureFormat_RGBA32Float:
        return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case WGPUTextureFormat_RG11B10Ufloat:
        return SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
    case WGPUTextureFormat_R8Uint:
        return SDL_GPU_TEXTUREFORMAT_R8_UINT;
    case WGPUTextureFormat_RG8Uint:
        return SDL_GPU_TEXTUREFORMAT_R8G8_UINT;
    case WGPUTextureFormat_RGBA8Uint:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT;
    case WGPUTextureFormat_R8Sint:
        return SDL_GPU_TEXTUREFORMAT_R8_INT;
    case WGPUTextureFormat_RG8Sint:
        return SDL_GPU_TEXTUREFORMAT_R8G8_INT;
    case WGPUTextureFormat_RGBA8Sint:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT;
    case WGPUTextureFormat_R16Sint:
        return SDL_GPU_TEXTUREFORMAT_R16_INT;
    case WGPUTextureFormat_RG16Sint:
        return SDL_GPU_TEXTUREFORMAT_R16G16_INT;
    case WGPUTextureFormat_RGBA16Sint:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT;
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB;
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB;
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB;
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
        return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB;
    case WGPUTextureFormat_Depth16Unorm:
        return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    case WGPUTextureFormat_Depth24Plus:
        return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    case WGPUTextureFormat_Depth32Float:
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    case WGPUTextureFormat_Depth24PlusStencil8:
        return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    case WGPUTextureFormat_Depth32FloatStencil8:
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    default:
        return SDL_GPU_TEXTUREFORMAT_INVALID;
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
        SDL_Log("WebGPU: Immediate present mode.");
        return WGPUPresentMode_Immediate;
    case SDL_GPU_PRESENTMODE_MAILBOX:
        SDL_Log("WebGPU: Mailbox present mode.");
        return WGPUPresentMode_Mailbox;
    case SDL_GPU_PRESENTMODE_VSYNC:
        SDL_Log("WebGPU: VSYNC/FIFO present mode.");
        return WGPUPresentMode_Fifo;
    default:
        SDL_Log("WebGPU: Defaulting to VSYNC/FIFO present mode.");
        return WGPUPresentMode_Fifo;
    }
}

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
        return WGPUVertexFormat_Undefined;
    }
}

// Shader Reflection Functions
// ---------------------------------------------------

static WebGPUBindingType DetectBindingType(const char *line, WGPUTextureViewDimension *viewDimension)
{
    if (SDL_strstr(line, "buffer") != NULL) {
        return WGPUBindingType_Buffer;
    } else if (SDL_strstr(line, "uniform") != NULL) {
        return WGPUBindingType_UniformBuffer;
    } else if (SDL_strstr(line, "sampler") != NULL) {
        return WGPUBindingType_Sampler;
    } else if (SDL_strstr(line, "texture") != NULL) {
        if (SDL_strstr(line, "2d")) {
            *viewDimension = WGPUTextureViewDimension_2D;
            if (SDL_strstr(line, "2d_array")) {
                *viewDimension = WGPUTextureViewDimension_2DArray;
            }
        } else if (SDL_strstr(line, "3d")) {
            *viewDimension = WGPUTextureViewDimension_3D;
        }
        if (SDL_strstr(line, "cube")) {
            *viewDimension = WGPUTextureViewDimension_Cube;
            if (SDL_strstr(line, "cube_array")) {
                *viewDimension = WGPUTextureViewDimension_CubeArray;
            }
        }
        return WGPUBindingType_Texture;
    } else {
        return WGPUBindingType_Undefined;
    }
}

static void WebGPU_INTERNAL_ExtractBindingsFromWGSL(WebGPUBindingInfo *bindings, const char *shaderCode, Uint32 *outBindingCount, WebGPUShaderStage stage)
{

    // Iterate through each line of the shader code and extract the group and binding numbers when the pattern is found
    // The pattern is "@group(<group number>) @binding(<binding number>)"
    const char *pattern = "@group\\((\\d+)\\)\\s*@binding\\((\\d+)\\)";
    regex_t regex;
    regmatch_t matches[3]; // 0 is the entire match, 1 is group, 2 is binding
    Uint32 count = 0;

    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to compile regex pattern: %s", pattern);
        return;
    }

    // Iterate through each line of the shader code
    char *line = strtok((char *)shaderCode, "\n");
    do {
        // Check if the line matches the pattern
        if (regexec(&regex, line, 3, matches, 0) == 0) {
            // Extract group and binding numbers
            char groupStr[16] = { 0 };
            char bindingStr[16] = { 0 };
            strncpy(groupStr, line + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
            strncpy(bindingStr, line + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);

            // Store the group, binding, and type
            bindings[count].group = atoi(groupStr);
            bindings[count].binding = atoi(bindingStr);
            bindings[count].viewDimension = WGPUTextureViewDimension_Undefined;
            bindings[count].type = DetectBindingType(line, &bindings[count].viewDimension);
            if (bindings[count].viewDimension != WGPUTextureViewDimension_Undefined) {
                SDL_Log("Binding %d: Group %d, Binding %d, Type %s, View Dimension %s", count, bindings[count].group, bindings[count].binding, WebGPU_GetBindingTypeString(bindings[count].type), WebGPU_GetTextureViewDimensionString(bindings[count].viewDimension));
            } else {
                SDL_Log("Binding %d: Group %d, Binding %d, Type %s", count, bindings[count].group, bindings[count].binding, WebGPU_GetBindingTypeString(bindings[count].type));
            }
            bindings[count].stage = stage;
            count++;
        }
    } while ((line = strtok(NULL, "\n")) != NULL);

    regfree(&regex);

    *outBindingCount = count;
    /*return bindings;*/
}

// ---------------------------------------------------

// WebGPU Functions:
// ---------------------------------------------------

// Simple Error Callback for WebGPU
static void WebGPU_ErrorCallback(WGPUErrorType type, const char *message, void *userdata)
{
    /*const char *errorTypeStr;*/
    /*switch (type) {*/
    /*case WGPUErrorType_Validation:*/
    /*    errorTypeStr = "Validation Error";*/
    /*    break;*/
    /*case WGPUErrorType_OutOfMemory:*/
    /*    errorTypeStr = "Out of Memory Error";*/
    /*    break;*/
    /*case WGPUErrorType_Unknown:*/
    /*    errorTypeStr = "Unknown Error";*/
    /*    break;*/
    /*case WGPUErrorType_DeviceLost:*/
    /*    errorTypeStr = "Device Lost Error";*/
    /*    break;*/
    /*default:*/
    /*    errorTypeStr = "Unhandled Error Type";*/
    /*    break;*/
    /*}*/
    /*// Output the error information to the console*/
    /*SDL_Log("[%s]: %s\n", errorTypeStr, message);*/
}

// Device Request Callback for when the device is requested from the adapter
static void WebGPU_RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, const char *message, void *userdata)
{
    WebGPURenderer *renderer = (WebGPURenderer *)userdata;
    if (status == WGPURequestDeviceStatus_Success) {
        renderer->device = device;
        renderer->deviceError = false;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to request WebGPU device: %s", message);
        renderer->deviceError = true;
    }
}

// Callback for requesting an adapter from the WebGPU instance
// This will then request a device from the adapter once the adapter is successfully requested
static void WebGPU_RequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *message, void *userdata)
{
    WebGPURenderer *renderer = (WebGPURenderer *)userdata;
    if (status != WGPURequestAdapterStatus_Success) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to request WebGPU adapter: %s", message);
    } else {
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

        wgpuAdapterRequestDevice(renderer->adapter, &dev_desc, WebGPU_RequestDeviceCallback, renderer);
    }
}

// Fetch the necessary PropertiesID for the WebGPUWindow for a browser window
static WebGPUWindow *WebGPU_INTERNAL_FetchWindowData(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (WebGPUWindow *)SDL_GetPointerProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

// Callback for when the window is resized
static bool WebGPU_INTERNAL_OnWindowResize(void *userdata, SDL_Event *event)
{
    SDL_Window *window = (SDL_Window *)userdata;
    // Event watchers will pass any event, but we only care about window resize events
    if (event->type != SDL_EVENT_WINDOW_RESIZED) {
        return false;
    }

    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    if (windowData) {
        windowData->needsSwapchainRecreate = true;
    }

    return true;
}

// Buffer Management Functions
// ---------------------------------------------------
static SDL_GPUBuffer *WebGPU_INTERNAL_CreateGPUBuffer(SDL_GPURenderer *driverData,
                                                      SDL_GPUBufferUsageFlags usageFlags,
                                                      Uint32 size, WebGPUBufferType type)
{
    WebGPUBuffer *buffer = SDL_calloc(1, sizeof(WebGPUBuffer));
    buffer->size = size;

    WGPUBufferUsageFlags wgpuUsage = 0;
    if (type == WEBGPU_BUFFER_TYPE_TRANSFER) {
        if (usageFlags == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD) {
            wgpuUsage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
        } else if (usageFlags == SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) {
            wgpuUsage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        }
    } else if (type == WEBGPU_BUFFER_TYPE_UNIFORM) {
        wgpuUsage |= WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    } else {
        wgpuUsage = SDLToWGPUBufferUsageFlags(usageFlags);
        wgpuUsage |= WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    }

    WGPUBufferDescriptor bufferDesc = {
        .usage = wgpuUsage,
        .size = size,
        .mappedAtCreation = false, // The programmer will map the buffer when they need to write to it
    };

    buffer->buffer = wgpuDeviceCreateBuffer(((WebGPURenderer *)driverData)->device, &bufferDesc);

    SDL_SetAtomicInt(&buffer->mappingComplete, 0);
    buffer->usageFlags = usageFlags;
    buffer->type = type;
    buffer->markedForDestroy = false;
    buffer->isMapped = false;
    buffer->uniformBufferForDefrag = NULL;
    buffer->debugName = NULL;

    return (SDL_GPUBuffer *)buffer;
}

static WebGPUUniformBuffer *WebGPU_INTERNAL_CreateUniformBuffer(WebGPURenderer *renderer,
                                                                Uint32 size)
{
    WebGPUUniformBuffer *uniformBuffer = SDL_calloc(1, sizeof(WebGPUUniformBuffer));
    uniformBuffer->buffer = (WebGPUBuffer *)WebGPU_INTERNAL_CreateGPUBuffer(
        (SDL_GPURenderer *)renderer,
        0,
        size,
        WEBGPU_BUFFER_TYPE_UNIFORM);
    uniformBuffer->drawOffset = 0;
    uniformBuffer->writeOffset = 0;
    uniformBuffer->buffer->uniformBufferForDefrag = uniformBuffer;

    return uniformBuffer;
}

static void WebGPU_SetBufferName(SDL_GPURenderer *driverData,
                                 SDL_GPUBuffer *buffer,
                                 const char *text)
{
    if (!buffer) {
        return;
    }

    if (strlen(text) > 128) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Buffer name is too long");
        return;
    }

    WebGPUBuffer *webgpuBuffer = (WebGPUBuffer *)buffer;
    if (webgpuBuffer->debugName) {
        SDL_free(webgpuBuffer->debugName);
    }

    webgpuBuffer->debugName = SDL_strdup(text);

    wgpuBufferSetLabel(webgpuBuffer->buffer, text);
}

static SDL_GPUBuffer *WebGPU_CreateGPUBuffer(SDL_GPURenderer *driverData,
                                             SDL_GPUBufferUsageFlags usageFlags,
                                             Uint32 size,
                                             const char *debugName)
{
    SDL_GPUBuffer *buffer = WebGPU_INTERNAL_CreateGPUBuffer(driverData, usageFlags, size, WEBGPU_BUFFER_TYPE_GPU);
    if (debugName) {
        WebGPU_SetBufferName(driverData, buffer, debugName);
    }

    return buffer;
}

static void WebGPU_ReleaseBuffer(SDL_GPURenderer *driverData, SDL_GPUBuffer *buffer)
{
    WebGPUBuffer *webgpuBuffer = (WebGPUBuffer *)buffer;

    // if reference count == 0, release the buffer
    if (SDL_GetAtomicInt(&webgpuBuffer->referenceCount) == 0) {
        wgpuBufferRelease(webgpuBuffer->buffer);
    }

    SDL_free(webgpuBuffer);
}

static SDL_GPUTransferBuffer *WebGPU_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage, // ignored on WebGPU
    Uint32 size,
    const char *debugName)
{
    SDL_GPUBuffer *buffer = WebGPU_INTERNAL_CreateGPUBuffer(driverData, usage, size, WEBGPU_BUFFER_TYPE_TRANSFER);
    if (debugName) {
        WebGPU_SetBufferName(driverData, buffer, debugName);
    } else {
        WebGPU_SetBufferName(driverData, buffer, "SDLGPU Transfer Buffer");
    }
    return (SDL_GPUTransferBuffer *)buffer;
}

static void WebGPU_ReleaseTransferBuffer(SDL_GPURenderer *driverData, SDL_GPUTransferBuffer *transferBuffer)
{
    WebGPUBuffer *webgpuBuffer = (WebGPUBuffer *)transferBuffer;
    if (webgpuBuffer->debugName) {
        SDL_free(webgpuBuffer->debugName);
    }

    if (webgpuBuffer->buffer) {
        wgpuBufferRelease(webgpuBuffer->buffer);

        if (webgpuBuffer->mappedData) {
            SDL_free(webgpuBuffer->mappedData);
        }
    }
    SDL_free(webgpuBuffer);
}

static void WebGPU_INTERNAL_MapDownloadTransferBuffer(WGPUBufferMapAsyncStatus status, void *userdata)
{
    WebGPUBuffer *buffer = (WebGPUBuffer *)userdata;

    if (status != WGPUBufferMapAsyncStatus_Success) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to map buffer: status %d", status);
        buffer->mappedData = NULL;
        buffer->isMapped = false;
    } else {
        buffer->isMapped = true;
    }

    // Signal that the mapping operation is complete
    SDL_SetAtomicInt(&buffer->mappingComplete, 1);
}

static void *WebGPU_MapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer,
    bool cycle)
{
    WebGPUBuffer *buffer = (WebGPUBuffer *)transferBuffer;

    (void)cycle;

    if (!buffer || !buffer->buffer) {
        SDL_SetError("Invalid buffer");
        return NULL;
    }

    if (buffer->type != WEBGPU_BUFFER_TYPE_TRANSFER) {
        SDL_SetError("Buffer is not a transfer buffer");
        return NULL;
    }

    if (buffer->usageFlags != SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD &&
        buffer->usageFlags != SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) {
        SDL_SetError("Invalid transfer buffer usage");
        return NULL;
    }

    // We can skip the entire async mapping process if we're just uploading data.
    // Once we unmap, we can just check if the transfer buffer is an upload buffer
    // and then copy the data over using wgpuQueueWriteBuffer().
    //
    // See: https://toji.dev/webgpu-best-practices/buffer-uploads.html
    if (buffer->usageFlags == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD) {
        // Upload case:
        if (!buffer->mappedData) {
            buffer->mappedData = SDL_malloc(buffer->size);
        } else {
            buffer->mappedData = SDL_realloc(buffer->mappedData, buffer->size);
        }

        SDL_SetAtomicInt(&buffer->mappingComplete, 1);
        buffer->isMapped = true;
    } else {
        // Read back case:
        // We need to do an async mapping operation to read data from the buffer.
        const Uint32 TIMEOUT = 1000;
        Uint32 startTime = SDL_GetTicks();

        // Reset mapped state
        buffer->isMapped = false;
        buffer->mappedData = NULL;
        SDL_SetAtomicInt(&buffer->mappingComplete, 0);

        WGPUMapMode mapMode = buffer->usageFlags == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD ? WGPUMapMode_Write : WGPUMapMode_Read;

        SDL_Log("Mapping buffer %p with usage flags %d", buffer->buffer, buffer->usageFlags);

        // Start async mapping for download buffers
        wgpuBufferMapAsync(buffer->buffer, mapMode, 0, buffer->size,
                           WebGPU_INTERNAL_MapDownloadTransferBuffer, buffer);

        // Poll for completion
        while (SDL_GetAtomicInt(&buffer->mappingComplete) != 1) {
            if (SDL_GetTicks() - startTime > TIMEOUT) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to map buffer: timeout");
                return NULL;
            }

            // Must be called to yield control to the browser so that it can tick the device for us.
            // Without this, the mapping operation will never complete.
            SDL_Delay(1);
        }

        if (!buffer->isMapped) {
            SDL_SetError("Failed to map buffer");
            return NULL;
        }

        // We only need GetConstMappedRange for reading
        if (mapMode == WGPUMapMode_Read) {
            buffer->mappedData = (void *)wgpuBufferGetConstMappedRange(buffer->buffer, 0, buffer->size);
            SDL_Log("Mapped buffer %p to %p", buffer->buffer, buffer->mappedData);
        }
    }

    return buffer->mappedData;
}

static void WebGPU_UnmapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    WebGPUBuffer *buffer = (WebGPUBuffer *)transferBuffer;

    if (buffer && buffer->buffer) {

        // Write the data to the buffer if it's an upload buffer
        if (buffer->usageFlags == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD) {
            wgpuQueueWriteBuffer(((WebGPURenderer *)driverData)->queue, buffer->buffer, 0, buffer->mappedData, buffer->size);
        } else {
            wgpuBufferUnmap(buffer->buffer);
        }

        // Unlock the buffer
        buffer->isMapped = false;
        SDL_SetAtomicInt(&buffer->mappingComplete, 0);
    }

    (void)driverData;
}

static void WebGPU_UploadToBuffer(SDL_GPUCommandBuffer *commandBuffer,
                                  const SDL_GPUTransferBufferLocation *source,
                                  const SDL_GPUBufferRegion *destination,
                                  bool cycle)
{
    if (!commandBuffer || !source || !destination) {
        SDL_SetError("Invalid parameters for buffer upload");
        return;
    }

    (void)cycle;

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBuffer *srcBuffer = (WebGPUBuffer *)source->transfer_buffer;
    WebGPUBuffer *dstBuffer = (WebGPUBuffer *)destination->buffer;

    if (!srcBuffer || !srcBuffer->buffer || !dstBuffer || !dstBuffer->buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer");
        return;
    }

    if ((uint64_t)source->offset + destination->size > srcBuffer->size ||
        (uint64_t)destination->offset + destination->size > dstBuffer->size) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer region");
        return;
    }

    wgpuCommandEncoderCopyBufferToBuffer(
        webgpuCmdBuffer->commandEncoder,
        srcBuffer->buffer,
        source->offset,
        dstBuffer->buffer,
        destination->offset,
        destination->size);

    SDL_Log("Uploaded %u bytes from buffer %p to buffer %p", destination->size, srcBuffer->buffer, dstBuffer->buffer);
}

static void WebGPU_DownloadFromBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    const SDL_GPUBufferRegion *source,
    const SDL_GPUTransferBufferLocation *destination)
{
    if (!commandBuffer || !source || !destination) {
        SDL_SetError("Invalid parameters for buffer download");
        return;
    }

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBuffer *srcBuffer = (WebGPUBuffer *)source->buffer;
    WebGPUBuffer *dstBuffer = (WebGPUBuffer *)destination->transfer_buffer;

    if (!srcBuffer || !srcBuffer->buffer || !dstBuffer || !dstBuffer->buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer");
        return;
    }

    if (source->offset + source->size > srcBuffer->size ||
        destination->offset + source->size > dstBuffer->size) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer region");
        return;
    }

    WGPUCommandEncoder encoder = webgpuCmdBuffer->commandEncoder;

    wgpuCommandEncoderCopyBufferToBuffer(
        encoder,
        srcBuffer->buffer,
        source->offset,
        dstBuffer->buffer,
        destination->offset,
        source->size);

    SDL_Log("Downloaded %u bytes from buffer %p to buffer %p", source->size, srcBuffer->buffer, dstBuffer->buffer);
}

static void WebGPU_CopyBufferToBuffer(SDL_GPUCommandBuffer *commandBuffer,
                                      const SDL_GPUBufferLocation *source,
                                      const SDL_GPUBufferLocation *destination,
                                      Uint32 size,
                                      bool cycle)
{

    if (!commandBuffer || !source || !destination) {
        SDL_SetError("Invalid parameters for buffer copy");
        return;
    }

    (void)cycle;

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBuffer *srcBuffer = (WebGPUBuffer *)source->buffer;
    WebGPUBuffer *dstBuffer = (WebGPUBuffer *)destination->buffer;

    if (!srcBuffer || !srcBuffer->buffer || !dstBuffer || !dstBuffer->buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer");
        return;
    }

    if (source->offset + size > srcBuffer->size || destination->offset + size > dstBuffer->size) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid buffer region");
        return;
    }

    wgpuCommandEncoderCopyBufferToBuffer(
        webgpuCmdBuffer->commandEncoder,
        srcBuffer->buffer,
        source->offset,
        dstBuffer->buffer,
        destination->offset,
        size);

    SDL_Log("Copied %u bytes from buffer %p to buffer %p", size, srcBuffer->buffer, dstBuffer->buffer);
}

static void WebGPU_BindVertexBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    const SDL_GPUBufferBinding *bindings,
    Uint32 numBindings)
{
    if (!commandBuffer || !bindings || numBindings == 0) {
        SDL_SetError("Invalid parameters for binding vertex buffers");
        return;
    }

    if (firstSlot + numBindings > MAX_VERTEX_BUFFERS) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "BindVertexBuffers(): Too many vertex buffers");
        return;
    }

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // Ensure we're inside a render pass
    if (!webgpuCmdBuffer->renderPassEncoder) {
        SDL_SetError("Cannot bind vertex buffers outside of a render pass");
        return;
    }

    // WebGPU requires us to set vertex buffers individually
    for (Uint32 i = 0; i < numBindings; i++) {
        const SDL_GPUBufferBinding *binding = &bindings[i];
        WebGPUBuffer *buffer = (WebGPUBuffer *)binding->buffer;

        if (!buffer || !buffer->buffer) {
            SDL_SetError("Invalid buffer at binding %u", i);
            continue;
        }

        wgpuRenderPassEncoderSetVertexBuffer(
            webgpuCmdBuffer->renderPassEncoder,
            firstSlot + i,                                     // slot
            buffer->buffer,                                    // buffer
            binding->offset,                                   // offset
            buffer->size == 0 ? WGPU_WHOLE_SIZE : buffer->size // size
        );
    }
}

static void WebGPU_INTERNAL_BindSamplers(SDL_GPUCommandBuffer *commandBuffer,
                                         Uint32 firstSlot,
                                         const SDL_GPUTextureSamplerBinding *textureSamplerBindings,
                                         Uint32 numBindings)
{
    if (commandBuffer == NULL) {
        return;
    }

    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    if (wgpu_cmd_buf->currentGraphicsPipeline == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No current graphics pipeline set");
        return;
    }
    WebGPUGraphicsPipeline *pipeline = wgpu_cmd_buf->currentGraphicsPipeline;
    if (pipeline->resourceLayout == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No resource layout set for current graphics pipeline");
        return;
    }

    void *pointers[128];
    for (Uint32 i = 0; i < numBindings; i += 2) {
        pointers[i] = textureSamplerBindings[i].sampler;
        pointers[i + 1] = textureSamplerBindings[i].texture;
    }

    size_t hash = 0;
    for (Uint32 i = 0; i < numBindings; i += 1) {
        hash ^= (size_t)((const void *const *)pointers[i]);
        hash *= 0x9e3779b9;
    }

    // If the hash is different, we need to cycle the bind groups
    if (pipeline->bindSamplerHash == 0) {
        pipeline->bindSamplerHash = hash;
    } else if (pipeline->bindSamplerHash != hash) {
        SDL_Log("Cycling bind groups due to change in samplers");
        pipeline->cycleBindGroups = true;
        pipeline->bindSamplerHash = hash;
    }

    // Get our bind group layout from the pipeline resource layout
    WebGPUPipelineResourceLayout *resourceLayout = pipeline->resourceLayout;
    WebGPUBindGroupLayout *bgLayouts = resourceLayout->bindGroupLayouts;
    Uint32 bgLayoutCount = resourceLayout->bindGroupLayoutCount;
    size_t currentSampler = 0;

    WebGPUTexture *texture = (WebGPUTexture *)textureSamplerBindings[currentSampler].texture;
    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Texture container is NULL");
        return;
    }

    // Iterate over the bind group layouts and identify which sampler we are supposedly binding.
    // SamplerBindings are always passed in order, so #1 in the array is always the first sampler in the bind group.
    // We arent building the bind group here, just assigning bind group entries for building once we perform a pass.
    for (Uint32 i = 0; i < bgLayoutCount; i += 1) {
        WebGPUBindGroupLayout *layout = &bgLayouts[i];

        // Find the associated entry in the bind group layout
        for (Uint32 j = 0; j < layout->bindingCount; j += 1) {
            WebGPUBindingInfo *layBinding = &layout->bindings[j];
            // If the binding is a sampler, we need to create a bind group entry for it
            // Save this entry in the current resources for later use when we build the bind group
            if (layBinding->type == WGPUBindingType_Sampler) {
                WebGPUSampler *sampler = (WebGPUSampler *)textureSamplerBindings[currentSampler].sampler;
                if (sampler == NULL || sampler->sampler == NULL) {
                    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Sampler is NULL");
                    return;
                }

                // Add the bind group entry to the current resources if the binding matches the layout slot
                if (layBinding->binding == j) {
                    wgpu_cmd_buf->bindGroups[i].entries[j] = (WGPUBindGroupEntry){
                        .binding = layBinding->binding,
                        .sampler = sampler->sampler,
                    };
                    currentSampler += 1;
                }
            } else if (layBinding->type == WGPUBindingType_Texture) {
                // If the binding is a texture, we need to create a bind group entry for it
                // Save this entry in the current resources for later use when we build the bind group
                // Add the bind group entry to the current resources if the binding matches the layout slot
                if (layBinding->binding == j) {
                    wgpu_cmd_buf->bindGroups[i].entries[j] = (WGPUBindGroupEntry){
                        .binding = layBinding->binding,
                        .textureView = texture->fullView,
                    };
                }
            }
        }
    }
}

static void WebGPU_BindVertexSamplers(SDL_GPUCommandBuffer *commandBuffer,
                                      Uint32 firstSlot,
                                      const SDL_GPUTextureSamplerBinding *textureSamplerBindings,
                                      Uint32 numBindings)
{
    // Bind samplers to bind group for our pipeline
    WebGPU_INTERNAL_BindSamplers(commandBuffer, firstSlot, textureSamplerBindings, numBindings);
}

static void WebGPU_PushVertexUniformData(SDL_GPUCommandBuffer *commandBuffer,
                                         Uint32 slotIndex,
                                         const void *data,
                                         Uint32 length)
{
    if (!commandBuffer || !data || length == 0) {
        SDL_SetError("Invalid parameters for pushing vertex uniform data");
        return;
    }

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // Ensure we're inside a render pass
    if (!webgpuCmdBuffer->renderPassEncoder) {
        SDL_SetError("Cannot push vertex uniform data outside of a render pass");
        return;
    }

    if (slotIndex >= MAX_UNIFORM_BUFFERS_PER_STAGE) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "PushVertexUniformData(): out of bounds slot index");
        return;
    }

    WebGPUUniformBuffer uniformBuffer = webgpuCmdBuffer->currentGraphicsPipeline->vertexUniformBuffers[slotIndex];

    Uint32 group = uniformBuffer.group;
    Uint32 binding = uniformBuffer.binding;

    // Vertex and Uniform buffers are a pain since we have to know where they would be in the bind group.
    if (!uniformBuffer.buffer) {
        SDL_GPUBufferUsageFlags usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        // We must create a new buffer for each push. Ensure the buffer is destroyed after the push.
        WebGPUBuffer *buffer = (WebGPUBuffer *)WebGPU_INTERNAL_CreateGPUBuffer(
            (SDL_GPURenderer *)webgpuCmdBuffer->renderer,
            usageFlags,
            length,
            WEBGPU_BUFFER_TYPE_UNIFORM);
        WebGPU_SetBufferName((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)buffer, "Vertex Uniform Buffer");

        uniformBuffer.buffer = buffer;

        webgpuCmdBuffer->currentGraphicsPipeline->vertexUniformBuffers[slotIndex] = uniformBuffer;

        /*SDL_Log("Created vertex uniform buffer %p of size %d", buffer->buffer, length);*/
    } else if (wgpuBufferGetSize(uniformBuffer.buffer->buffer) < length) {
        // If the buffer is too small, we need to recreate it
        WebGPU_ReleaseBuffer((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)uniformBuffer.buffer);
        SDL_GPUBufferUsageFlags usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        WebGPUBuffer *buffer = (WebGPUBuffer *)WebGPU_INTERNAL_CreateGPUBuffer(
            (SDL_GPURenderer *)webgpuCmdBuffer->renderer,
            usageFlags,
            length,
            WEBGPU_BUFFER_TYPE_UNIFORM);
        WebGPU_SetBufferName((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)buffer, "Vertex Uniform Buffer");

        uniformBuffer.buffer = buffer;

        webgpuCmdBuffer->currentGraphicsPipeline->vertexUniformBuffers[slotIndex] = uniformBuffer;

        SDL_Log("Recreated vertex uniform buffer %p", buffer->buffer);
    }

    /*SDL_Log("Pushing vertex uniform data to buffer %p", uniformBuffer.buffer->buffer);*/

    // Copy the data into the uniform buffer
    wgpuQueueWriteBuffer(
        webgpuCmdBuffer->renderer->queue,
        uniformBuffer.buffer->buffer,
        0,
        data,
        length);

    /*// Read the data back to verify it was written correctly*/
    /*WebGPUBuffer *transferBuffer = (WebGPUBuffer *)WebGPU_CreateTransferBuffer(*/
    /*    (SDL_GPURenderer *)webgpuCmdBuffer->renderer,*/
    /*    SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,*/
    /*    length);*/
    /**/
    /*// Copy from the uniform buffer to the transfer buffer*/
    /*WebGPU_CopyBufferToBuffer(*/
    /*    commandBuffer,*/
    /*    &(SDL_GPUBufferLocation){ (SDL_GPUBuffer *)uniformBuffer.buffer, 0 },*/
    /*    &(SDL_GPUBufferLocation){ (SDL_GPUBuffer *)transferBuffer, 0 },*/
    /*    length,*/
    /*    false);*/
    /**/
    /*void *mappedData = WebGPU_MapTransferBuffer(*/
    /*    (SDL_GPURenderer *)webgpuCmdBuffer->renderer,*/
    /*    (SDL_GPUTransferBuffer *)transferBuffer,*/
    /*    false);*/
    /**/
    /*// Read the data back from the raw pointer*/
    /*for (Uint32 i = 0; i < length; i += 1) {*/
    /*    SDL_Log("Vertex uniform data: %f", ((float *)mappedData)[i]);*/
    /*}*/
    /**/
    /*WebGPU_UnmapTransferBuffer(*/
    /*    (SDL_GPURenderer *)webgpuCmdBuffer->renderer,*/
    /*    (SDL_GPUTransferBuffer *)transferBuffer);*/

    /*WebGPU_ReleaseTransferBuffer((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUTransferBuffer *)transferBuffer);*/

    WebGPUBindGroup *bindGroup = &webgpuCmdBuffer->bindGroups[group];

    /*SDL_Log("Pushed %u bytes of vertex uniform data to buffer %p", length, uniformBuffer.buffer->buffer);*/

    // Update the entry information for the bind group.
    bindGroup->entries[binding] = (WGPUBindGroupEntry){
        .binding = binding,
        .buffer = uniformBuffer.buffer->buffer,
        .size = wgpuBufferGetSize(uniformBuffer.buffer->buffer),
    };

    /*SDL_Log("Updating bind group entry %d with buffer %p", binding, uniformBuffer.buffer->buffer);*/
}

static void WebGPU_PushFragmentUniformData(SDL_GPUCommandBuffer *commandBuffer,
                                           Uint32 slotIndex,
                                           const void *data,
                                           Uint32 length)
{
    if (!commandBuffer || !data || length == 0) {
        SDL_SetError("Invalid parameters for pushing fragment uniform data");
        return;
    }

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // Ensure we're inside a render pass
    if (!webgpuCmdBuffer->renderPassEncoder) {
        SDL_SetError("Cannot push fragment uniform data outside of a render pass");
        return;
    }

    if (slotIndex >= MAX_UNIFORM_BUFFERS_PER_STAGE) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "PushFragmentUniformData(): out of bounds slot index");
        return;
    }

    WebGPUUniformBuffer uniformBuffer = webgpuCmdBuffer->currentGraphicsPipeline->fragUniformBuffers[slotIndex];

    Uint32 group = uniformBuffer.group;
    Uint32 binding = uniformBuffer.binding;

    // Vertex and Uniform buffers are a pain since we have to know where they would be in the bind group.
    if (!uniformBuffer.buffer) {
        SDL_GPUBufferUsageFlags usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        // We must create a new buffer for each push. Ensure the buffer is destroyed after the push.
        WebGPUBuffer *buffer = (WebGPUBuffer *)WebGPU_INTERNAL_CreateGPUBuffer(
            (SDL_GPURenderer *)webgpuCmdBuffer->renderer,
            usageFlags,
            length,
            WEBGPU_BUFFER_TYPE_UNIFORM);
        WebGPU_SetBufferName((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)buffer, "Fragment Uniform Buffer");

        uniformBuffer.buffer = buffer;

        webgpuCmdBuffer->currentGraphicsPipeline->fragUniformBuffers[slotIndex] = uniformBuffer;

        SDL_Log("Created fragment uniform buffer %p of size %d", buffer->buffer, length);
    } else if (wgpuBufferGetSize(uniformBuffer.buffer->buffer) < length) {
        // If the buffer is too small, we need to recreate it
        WebGPU_ReleaseBuffer((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)uniformBuffer.buffer);
        SDL_GPUBufferUsageFlags usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        WebGPUBuffer *buffer = (WebGPUBuffer *)WebGPU_INTERNAL_CreateGPUBuffer(
            (SDL_GPURenderer *)webgpuCmdBuffer->renderer,
            usageFlags,
            length,
            WEBGPU_BUFFER_TYPE_UNIFORM);
        WebGPU_SetBufferName((SDL_GPURenderer *)webgpuCmdBuffer->renderer, (SDL_GPUBuffer *)buffer, "Fragment Uniform Buffer");

        uniformBuffer.buffer = buffer;

        webgpuCmdBuffer->currentGraphicsPipeline->fragUniformBuffers[slotIndex] = uniformBuffer;

        SDL_Log("Recreated fragment uniform buffer %p", buffer->buffer);
    }

    /*SDL_Log("Pushing fragment uniform data to buffer %p", uniformBuffer.buffer->buffer);*/

    // Copy the data into the uniform buffer
    wgpuQueueWriteBuffer(
        webgpuCmdBuffer->renderer->queue,
        uniformBuffer.buffer->buffer,
        0,
        data,
        length);

    WebGPUBindGroup *bindGroup = &webgpuCmdBuffer->bindGroups[group];

    /*SDL_Log("Pushed %u bytes of fragment uniform data to buffer %p", length, uniformBuffer.buffer->buffer);*/

    // Update the entry information for the bind group.
    bindGroup->entries[binding] = (WGPUBindGroupEntry){
        .binding = binding,
        .buffer = uniformBuffer.buffer->buffer,
        .size = wgpuBufferGetSize(uniformBuffer.buffer->buffer),
    };

    /*SDL_Log("Updating bind group entry %d with buffer %p", binding, uniformBuffer.buffer->buffer);*/
}

static void WebGPU_BindIndexBuffer(SDL_GPUCommandBuffer *commandBuffer,
                                   const SDL_GPUBufferBinding *binding,
                                   SDL_GPUIndexElementSize indexElementSize)
{
    if (!commandBuffer || !binding) {
        SDL_SetError("Invalid parameters for binding index buffer");
        return;
    }

    WebGPUCommandBuffer *webgpuCmdBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // Ensure we're inside a render pass
    if (!webgpuCmdBuffer->renderPassEncoder) {
        SDL_SetError("Cannot bind index buffer outside of a render pass");
        return;
    }

    WebGPUBuffer *buffer = (WebGPUBuffer *)binding->buffer;

    if (!buffer || !buffer->buffer) {
        SDL_SetError("Invalid buffer");
        return;
    }

    WGPUIndexFormat indexFormat = SDLToWGPUIndexFormat(indexElementSize);

    wgpuRenderPassEncoderSetIndexBuffer(
        webgpuCmdBuffer->renderPassEncoder,
        buffer->buffer,
        indexFormat,
        binding->offset,
        buffer->size == 0 ? WGPU_WHOLE_SIZE : buffer->size);
}

static WebGPUFence *WebGPU_INTERNAL_AcquireFenceFromPool(WebGPURenderer *renderer)
{
    WebGPUFence *handle;

    /*SDL_Log("Acquiring fence from pool");*/

    if (renderer->fencePool.availableFenceCount == 0) {
        /*SDL_Log("Creating new fence");*/
        handle = (WebGPUFence *)SDL_malloc(sizeof(WebGPUFence));
        /*SDL_Log("Allocated fence %p", handle);*/
        SDL_SetAtomicInt(&handle->referenceCount, 0);
        return handle;
    }

    /*SDL_Log("Reusing fence from pool");*/

    SDL_LockMutex(renderer->fencePool.lock);

    handle = renderer->fencePool.availableFences[renderer->fencePool.availableFenceCount - 1];
    renderer->fencePool.availableFenceCount -= 1;

    SDL_UnlockMutex(renderer->fencePool.lock);

    return handle;
}

static void WebGPU_INTERNAL_ReturnFenceToPool(
    WebGPURenderer *renderer,
    WebGPUFence *fenceHandle)
{
    SDL_LockMutex(renderer->fencePool.lock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->fencePool.availableFences,
        WebGPUFence *,
        renderer->fencePool.availableFenceCount + 1,
        renderer->fencePool.availableFenceCapacity,
        renderer->fencePool.availableFenceCapacity * 2);

    renderer->fencePool.availableFences[renderer->fencePool.availableFenceCount] = fenceHandle;
    renderer->fencePool.availableFenceCount += 1;

    SDL_UnlockMutex(renderer->fencePool.lock);
}

static void WebGPU_INTERNAL_FenceCallback(WGPUQueueWorkDoneStatus status, void *userdata)
{
    WebGPUFence *fence = (WebGPUFence *)userdata;
    if (status == WGPUQueueWorkDoneStatus_Success) {
        SDL_SetAtomicInt(&fence->fence, 1);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to wait for fence");
    }
}

static bool WebGPU_QueryFence(SDL_GPURenderer *driverData, SDL_GPUFence *fence)
{
    WebGPUFence *handle = (WebGPUFence *)fence;

    // If the fence value is 1, then it has been submitted and completed
    if (SDL_GetAtomicInt(&handle->fence) == 1) {
        return true;
    }
    return false;
}

static void WebGPU_ReleaseFence(SDL_GPURenderer *driverData, SDL_GPUFence *fence)
{
    WebGPUFence *handle = (WebGPUFence *)fence;
    if (SDL_AtomicDecRef(&handle->referenceCount)) {
        WebGPU_INTERNAL_ReturnFenceToPool((WebGPURenderer *)driverData, handle);
    }
}

static bool WebGPU_INTERNAL_AllocateCommandBuffer(
    WebGPURenderer *renderer,
    WebGPUCommandPool *webgpuCommandPool)
{
    WebGPUCommandBuffer *commandBuffer;

    // Check if expansion is needed before incrementing the count
    if (webgpuCommandPool->inactiveCommandBufferCount + 1 > webgpuCommandPool->inactiveCommandBufferCapacity) {
        Uint32 newCapacity = webgpuCommandPool->inactiveCommandBufferCapacity * 2;
        newCapacity = newCapacity > 0 ? newCapacity : 1; // Handle initial capacity of 0
        WebGPUCommandBuffer **newArray = (WebGPUCommandBuffer **)SDL_realloc(
            webgpuCommandPool->inactiveCommandBuffers,
            sizeof(WebGPUCommandBuffer *) * newCapacity);
        if (!newArray) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to expand command buffer array");
            return false;
        }
        webgpuCommandPool->inactiveCommandBuffers = newArray;
        webgpuCommandPool->inactiveCommandBufferCapacity = newCapacity;
    }

    // Allocate and initialize the new command buffer
    commandBuffer = (WebGPUCommandBuffer *)SDL_malloc(sizeof(WebGPUCommandBuffer));
    if (!commandBuffer) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate command buffer");
        return false;
    }

    SDL_Log("Allocating command buffer: %p", commandBuffer);

    SDL_zero(*commandBuffer);
    commandBuffer->renderer = renderer;
    commandBuffer->commandPool = webgpuCommandPool;
    commandBuffer->common.device = renderer->sdlDevice;
    commandBuffer->autoReleaseFence = true;
    commandBuffer->inFlightFence = NULL;
    commandBuffer->currentGraphicsPipeline = NULL;

    // Add to the array and update count
    webgpuCommandPool->inactiveCommandBuffers[webgpuCommandPool->inactiveCommandBufferCount] = commandBuffer;
    webgpuCommandPool->inactiveCommandBufferCount += 1;

    return true;
}

static void WebGPU_INTERNAL_CleanCommandBuffer(
    WebGPURenderer *renderer,
    WebGPUCommandBuffer *commandBuffer,
    bool cancel)
{
    if (commandBuffer->autoReleaseFence) {
        WebGPU_ReleaseFence(
            (SDL_GPURenderer *)renderer,
            (SDL_GPUFence *)commandBuffer->inFlightFence);

        commandBuffer->inFlightFence = NULL;
    }


    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (Sint32 i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        /*WebGPU_INTERNAL_ReturnUniformBufferToPool(*/
        /*    renderer,*/
        /*    commandBuffer->usedUniformBuffers[i]);*/
    }
    commandBuffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    /*SDL_Log("Command Buffer Pool %p", commandBuffer->commandPool);*/

    if (commandBuffer->commandPool->inactiveCommandBufferCount == commandBuffer->commandPool->inactiveCommandBufferCapacity) {
        commandBuffer->commandPool->inactiveCommandBufferCapacity += 1;
        commandBuffer->commandPool->inactiveCommandBuffers = SDL_realloc(
            commandBuffer->commandPool->inactiveCommandBuffers,
            commandBuffer->commandPool->inactiveCommandBufferCapacity * sizeof(WebGPUCommandBuffer*));
    }

    commandBuffer->commandPool->inactiveCommandBuffers[commandBuffer->commandPool->inactiveCommandBufferCount] = commandBuffer;
    commandBuffer->commandPool->inactiveCommandBufferCount += 1;

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);
}


static bool WebGPU_Submit(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = wgpu_cmd_buf->renderer;

    /*SDL_Log("Submitting command buffer %p", wgpu_cmd_buf);*/

    // Lock the renderer's submit lock
    SDL_LockMutex(renderer->submitLock);

    WGPUCommandBufferDescriptor commandBufferDesc = {
        .label = "SDL_GPU Command Buffer",
    };

    // Finish the command buffer and submit it to the queue
    WGPUCommandBuffer commandHandle = wgpuCommandEncoderFinish(
        wgpu_cmd_buf->commandEncoder,
        &commandBufferDesc);

    if (!commandHandle) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to finish command buffer");
        SDL_UnlockMutex(renderer->submitLock);
        return false;
    }

    /*SDL_Log("Finished command buffer %p", wgpu_cmd_buf);*/

    // Create a fence for the command buffer
    wgpu_cmd_buf->inFlightFence = WebGPU_INTERNAL_AcquireFenceFromPool(renderer);
    if (wgpu_cmd_buf->inFlightFence == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to acquire fence from pool");
        SDL_UnlockMutex(renderer->submitLock);
        return false;
    }

    /*SDL_Log("Acquired fence %p", wgpu_cmd_buf->inFlightFence);*/

    // Command buffer has a reference to the in-flight fence
    (void)SDL_AtomicIncRef(&wgpu_cmd_buf->inFlightFence->referenceCount);

    // Submit the command buffer to the queue
    wgpuQueueSubmit(renderer->queue, 1, &commandHandle);

    /*SDL_Log("Submitted command buffer %p", wgpu_cmd_buf);*/

    // Release the actual command buffer and command encoder
    wgpuCommandBufferRelease(commandHandle);
    wgpuCommandEncoderRelease(wgpu_cmd_buf->commandEncoder);

    // Release any layer views that were created
    for (Uint32 i = 0; i < wgpu_cmd_buf->layerViewCount; i += 1) {
        wgpuTextureViewRelease(wgpu_cmd_buf->layerViews[i]);
    }

    /*// Release the memory for the command buffer*/
    WebGPU_INTERNAL_CleanCommandBuffer(renderer, wgpu_cmd_buf, false);

    /*SDL_Log("Cleaned command buffer %p", commandBuffer);*/

    // Unlock the renderer's submit lock
    SDL_UnlockMutex(renderer->submitLock);

    return true;
}

static SDL_GPUFence *WebGPU_SubmitAndAcquireFence(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    wgpu_cmd_buf->autoReleaseFence = false;
    if (!WebGPU_Submit(commandBuffer)) {
        return NULL;
    }
    wgpuQueueOnSubmittedWorkDone(wgpu_cmd_buf->renderer->queue, WebGPU_INTERNAL_FenceCallback, wgpu_cmd_buf->inFlightFence);
    return (SDL_GPUFence *)(wgpu_cmd_buf->inFlightFence);
}

static bool WebGPU_Wait(SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUCommandBuffer *commandBuffer;
    Sint32 i;

    // We pass control to the browser so it can tick the device for us
    // The device ticking is necessary for the QueueOnSubmittedWorkDone callback to be called and set the atomic flag.
    // This will need to be changed for native WebGPU implementations.
    SDL_Delay(1);

    SDL_LockMutex(renderer->submitLock);

    for (i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        commandBuffer = renderer->submittedCommandBuffers[i];
        WebGPU_INTERNAL_CleanCommandBuffer(renderer, commandBuffer, false);
    }

    SDL_UnlockMutex(renderer->submitLock);

    // TODO: Implement this once pools are all implemented
    /*WebGPU_INTERNAL_PerformPendingDestroys(renderer);*/

    return true;
}

static bool WebGPU_WaitForFences(
    SDL_GPURenderer *driverData,
    bool waitAll,
    SDL_GPUFence *const *fences,
    Uint32 numFences)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUCommandBuffer *commandBuffer;

    // Wait for all fences to be signaled
    for (Uint32 i = 0; i < numFences; i += 1) {
        while (!WebGPU_QueryFence(driverData, fences[i])) {
            // Pass control to the browser so it can tick the device for us
            // The device ticking is necessary for the QueueOnSubmittedWorkDone callback to be called and set the atomic flag.
            SDL_Delay(1);
        }
    }

    SDL_LockMutex(renderer->submitLock);

    for (Uint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        commandBuffer = renderer->submittedCommandBuffers[i];
        WebGPU_INTERNAL_CleanCommandBuffer(renderer, commandBuffer, false);
    }

    // TODO: Implement this once pools are all implemented
    /*WebGPU_INTERNAL_PerformPendingDestroys(renderer);*/

    SDL_UnlockMutex(renderer->submitLock);

    return true;
}

static bool WebGPU_Cancel(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPURenderer *renderer;
    WebGPUCommandBuffer *wgpuCommandBuffer;

    wgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    renderer = wgpuCommandBuffer->renderer;

    wgpuCommandBuffer->autoReleaseFence = false;
    SDL_LockMutex(renderer->submitLock);
    WebGPU_INTERNAL_CleanCommandBuffer(renderer, wgpuCommandBuffer, true);
    SDL_UnlockMutex(renderer->submitLock);

    return true;
}

static void WebGPU_INTERNAL_DestroyCommandPool(
    WebGPURenderer *renderer,
    WebGPUCommandPool *webgpuCommandPool)
{
    Uint32 i;
    WebGPUCommandBuffer *commandBuffer;

    for (i = 0; i < webgpuCommandPool->inactiveCommandBufferCount; i += 1) {
        commandBuffer = webgpuCommandPool->inactiveCommandBuffers[i];

        // TODO: Implement proper freeing once command pools are all implemented

        SDL_free(commandBuffer);
    }

    SDL_free(webgpuCommandPool->inactiveCommandBuffers);
    SDL_free(webgpuCommandPool);
}

static WebGPUCommandPool *WebGPU_INTERNAL_FetchCommandPool(
    WebGPURenderer *renderer,
    SDL_ThreadID threadID)
{
    WebGPUCommandPool *webgpuCommandPool = NULL;
    CommandPoolHashTableKey key;
    key.threadID = threadID;

    bool result = SDL_FindInHashTable(
        renderer->commandPoolHashTable,
        (const void *)&key,
        (const void **)&webgpuCommandPool);

    if (result) {
        return webgpuCommandPool;
    }

    webgpuCommandPool = (WebGPUCommandPool *)SDL_malloc(sizeof(WebGPUCommandPool));
    webgpuCommandPool->threadID = threadID;

    webgpuCommandPool->inactiveCommandBufferCapacity = 0;
    webgpuCommandPool->inactiveCommandBufferCount = 0;
    webgpuCommandPool->inactiveCommandBuffers = NULL;

    if (!WebGPU_INTERNAL_AllocateCommandBuffer(
            renderer,
            webgpuCommandPool)) {
        WebGPU_INTERNAL_DestroyCommandPool(renderer, webgpuCommandPool);
        return NULL;
    }

    CommandPoolHashTableKey *allocedKey = SDL_malloc(sizeof(CommandPoolHashTableKey));
    allocedKey->threadID = threadID;

    SDL_InsertIntoHashTable(
        renderer->commandPoolHashTable,
        (const void *)allocedKey,
        (const void *)webgpuCommandPool);

    return webgpuCommandPool;
}

static WebGPUCommandBuffer *WebGPU_INTERNAL_GetInactiveCommandBufferFromPool(
    WebGPURenderer *renderer,
    SDL_ThreadID threadID)
{
    WebGPUCommandPool *commandPool =
        WebGPU_INTERNAL_FetchCommandPool(renderer, threadID);
    WebGPUCommandBuffer *commandBuffer;

    if (commandPool == NULL) {
        return NULL;
    }

    if (commandPool->inactiveCommandBufferCount == 0) {
        if (!WebGPU_INTERNAL_AllocateCommandBuffer(renderer, commandPool)) {
            return NULL;
        }
    }

    commandBuffer = commandPool->inactiveCommandBuffers[commandPool->inactiveCommandBufferCount - 1];
    commandPool->inactiveCommandBufferCount -= 1;

    return commandBuffer;
}

static SDL_GPUCommandBuffer *WebGPU_AcquireCommandBuffer(SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;

    SDL_ThreadID threadID = SDL_GetCurrentThreadID();

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    WebGPUCommandBuffer *commandBuffer =
        WebGPU_INTERNAL_GetInactiveCommandBufferFromPool(renderer, threadID);

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    if (commandBuffer == NULL) {
        return NULL;
    }

    /*SDL_Log("Acquired command buffer %p", commandBuffer);*/

    // Reset state of command buffer

    SDL_zero(*commandBuffer);
    commandBuffer->renderer = renderer;
    commandBuffer->common.device = renderer->sdlDevice;
    commandBuffer->commandPool = WebGPU_INTERNAL_FetchCommandPool(renderer, threadID);

    SDL_zero(commandBuffer->layerViews);
    commandBuffer->layerViewCount = 0;


    WGPUCommandEncoderDescriptor commandEncoderDesc = {
        .label = "SDL_GPU Command Encoder",
    };

    commandBuffer->autoReleaseFence = true;
    commandBuffer->commandEncoder = wgpuDeviceCreateCommandEncoder(renderer->device, &commandEncoderDesc);

    return (SDL_GPUCommandBuffer *)commandBuffer;
}

static Uint32 WebGPU_INTERNAL_CommandPoolHashFunction(const void *key, void *data)
{
    return (Uint32)((CommandPoolHashTableKey *)key)->threadID;
}

static bool WebGPU_INTERNAL_CommandPoolHashKeyMatch(const void *aKey, const void *bKey, void *data)
{
    CommandPoolHashTableKey *a = (CommandPoolHashTableKey *)aKey;
    CommandPoolHashTableKey *b = (CommandPoolHashTableKey *)bKey;
    return a->threadID == b->threadID;
}

static void WebGPU_INTERNAL_CommandPoolHashNuke(const void *key, const void *value, void *data)
{
    WebGPURenderer *renderer = (WebGPURenderer *)data;
    WebGPUCommandPool *pool = (WebGPUCommandPool *)value;
    WebGPU_INTERNAL_DestroyCommandPool(renderer, pool);
    SDL_free((void *)key);
}

void WebGPU_SetViewport(SDL_GPUCommandBuffer *renderPass, const SDL_GPUViewport *viewport)
{
    if (renderPass == NULL) {
        return;
    }

    WebGPUCommandBuffer *commandBuffer = (WebGPUCommandBuffer *)renderPass;

    Uint32 window_width = commandBuffer->renderer->claimedWindows[0]->swapchainData.width;
    Uint32 window_height = commandBuffer->renderer->claimedWindows[0]->swapchainData.height;
    WebGPUViewport *wgpuViewport = &commandBuffer->currentViewport;

    float max_viewport_width = (float)window_width - viewport->x;
    float max_viewport_height = (float)window_height - viewport->y;

    wgpuViewport = &(WebGPUViewport){
        .x = viewport->x,
        .y = viewport->y,
        .width = viewport->w > max_viewport_width ? max_viewport_width : viewport->w,
        .height = viewport->h > max_viewport_height ? max_viewport_height : viewport->h,
        .minDepth = viewport->min_depth > 0.0f ? viewport->min_depth : 0.0f,
        .maxDepth = viewport->max_depth > wgpuViewport->minDepth ? viewport->max_depth : wgpuViewport->minDepth,
    };

    // Set the viewport
    wgpuRenderPassEncoderSetViewport(commandBuffer->renderPassEncoder, wgpuViewport->x, wgpuViewport->y, wgpuViewport->width, wgpuViewport->height, wgpuViewport->minDepth, wgpuViewport->maxDepth);
}

void WebGPU_SetScissorRect(SDL_GPUCommandBuffer *renderPass, const SDL_Rect *scissorRect)
{
    if (renderPass == NULL) {
        return;
    }

    WebGPUCommandBuffer *commandBuffer = (WebGPUCommandBuffer *)renderPass;

    Uint32 window_width = commandBuffer->renderer->claimedWindows[0]->swapchainData.width;
    Uint32 window_height = commandBuffer->renderer->claimedWindows[0]->swapchainData.height;

    Uint32 max_scissor_width = window_width - scissorRect->x;
    Uint32 max_scissor_height = window_height - scissorRect->y;

    Uint32 clamped_width = (scissorRect->w > max_scissor_width) ? max_scissor_width : scissorRect->w;
    Uint32 clamped_height = (scissorRect->h > max_scissor_height) ? max_scissor_height : scissorRect->h;

    commandBuffer->currentScissor = (WebGPURect){
        .x = scissorRect->x,
        .y = scissorRect->y,
        .width = clamped_width,
        .height = clamped_height,
    };

    wgpuRenderPassEncoderSetScissorRect(commandBuffer->renderPassEncoder, scissorRect->x, scissorRect->y, clamped_width, clamped_height);
}

static void WebGPU_SetStencilReference(SDL_GPUCommandBuffer *commandBuffer,
                                       Uint8 reference)
{
    if (commandBuffer == NULL) {
        return;
    }

    wgpuRenderPassEncoderSetStencilReference(((WebGPUCommandBuffer *)commandBuffer)->renderPassEncoder, reference);
}

static void WebGPU_SetBlendConstants(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_FColor blendConstants)
{
    if (commandBuffer == NULL) {
        return;
    }

    wgpuRenderPassEncoderSetBlendConstant(((WebGPUCommandBuffer *)commandBuffer)->renderPassEncoder,
                                          &(WGPUColor){
                                              .r = blendConstants.r,
                                              .g = blendConstants.g,
                                              .b = blendConstants.b,
                                              .a = blendConstants.a,
                                          });
}

static WGPUTextureView WebGPU_INTERNAL_CreateLayerView(WGPUTexture texture,
                                                       WGPUTextureFormat format,
                                                       SDL_GPUTextureType type,
                                                       uint32_t layer)
{
    WGPUTextureViewDescriptor viewDesc = {
        .format = format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = layer,
        .arrayLayerCount = 1,
        .label = "SDL_GPU Temporary Layer View",
    };

    if (type == SDL_GPU_TEXTURETYPE_3D) {
        viewDesc.dimension = WGPUTextureViewDimension_3D;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
    }

    return wgpuTextureCreateView(texture, &viewDesc);
}

void WebGPU_BeginRenderPass(SDL_GPUCommandBuffer *commandBuffer,
                            const SDL_GPUColorTargetInfo *colorAttachmentInfos,
                            uint32_t colorAttachmentCount,
                            const SDL_GPUDepthStencilTargetInfo *depthStencilAttachmentInfo)
{
    Uint32 w, h;
    SDL_GPUViewport defaultViewport;
    SDL_Rect defaultScissor;
    SDL_FColor defaultBlendConstants;
    Uint32 framebufferWidth = SDL_MAX_UINT32;
    Uint32 framebufferHeight = SDL_MAX_UINT32;

    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    if (!wgpu_cmd_buf || colorAttachmentCount == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid parameters for render pass");
        return;
    }

    // Handle color attachments
    WGPURenderPassColorAttachment colorAttachments[colorAttachmentCount];
    for (uint32_t i = 0; i < colorAttachmentCount; i++) {

        const SDL_GPUColorTargetInfo *colorInfo = &colorAttachmentInfos[i];
        WebGPUTexture *texture = (WebGPUTexture *)colorInfo->texture;
        WGPUTextureView textureView = texture->fullView;

        w = texture->common.info.width >> colorInfo->mip_level;
        h = texture->common.info.height >> colorInfo->mip_level;

        // The framebuffer cannot be larger than the smallest attachment.

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }

        if (h < framebufferHeight) {
            framebufferHeight = h;
        }

        // If a layer is specified, create a layer view from the texture
        if (colorInfo->layer_or_depth_plane != ~0u && texture->layerCount > 1) {
            textureView = WebGPU_INTERNAL_CreateLayerView(texture->texture,
                                                          SDLToWGPUTextureFormat(texture->format),
                                                          texture->type,
                                                          colorInfo->layer_or_depth_plane);
            wgpu_cmd_buf->layerViews[wgpu_cmd_buf->layerViewCount++] = textureView;
        }

        colorAttachments[i] = (WGPURenderPassColorAttachment){
            .view = textureView,
            .depthSlice = texture->type == SDL_GPU_TEXTURETYPE_3D ? colorInfo->layer_or_depth_plane : ~0u,
            .loadOp = SDLToWGPULoadOp(colorInfo->load_op),
            .storeOp = SDLToWGPUStoreOp(colorInfo->store_op),
            .clearValue = (WGPUColor){
                .r = colorInfo->clear_color.r,
                .g = colorInfo->clear_color.g,
                .b = colorInfo->clear_color.b,
                .a = colorInfo->clear_color.a,
            }
        };

        if (texture->isMSAAColorTarget) {
            colorAttachments[i].resolveTarget = texture->fullView;
        }
    }

    // Handle depth-stencil attachment
    WGPURenderPassDepthStencilAttachment depthStencilAttachment = { 0 };
    if (depthStencilAttachmentInfo) {
        WebGPUTexture *depthTex = (WebGPUTexture *)depthStencilAttachmentInfo->texture;
        depthStencilAttachment = (WGPURenderPassDepthStencilAttachment){
            .view = depthTex->fullView,
            .depthLoadOp = SDLToWGPULoadOp(depthStencilAttachmentInfo->load_op),
            .depthStoreOp = SDLToWGPUStoreOp(depthStencilAttachmentInfo->store_op),
            .depthClearValue = depthStencilAttachmentInfo->clear_depth,
            .stencilLoadOp = SDLToWGPULoadOp(depthStencilAttachmentInfo->stencil_load_op),
            .stencilStoreOp = SDLToWGPUStoreOp(depthStencilAttachmentInfo->stencil_store_op),
            .stencilClearValue = depthStencilAttachmentInfo->clear_stencil
        };

        WebGPUTexture *tex = (WebGPUTexture *)depthStencilAttachmentInfo->texture;
        w = tex->common.info.width;
        h = tex->common.info.height;

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }
        if (h < framebufferHeight) {
            framebufferHeight = h;
        }
    }

    WGPURenderPassDescriptor renderPassDesc = {
        .label = "SDL_GPU Render Pass",
        .colorAttachmentCount = colorAttachmentCount,
        .colorAttachments = colorAttachments,
        .depthStencilAttachment = depthStencilAttachmentInfo ? &depthStencilAttachment : NULL
    };

    // Create the render pass encoder
    wgpu_cmd_buf->renderPassEncoder =
        wgpuCommandEncoderBeginRenderPass(wgpu_cmd_buf->commandEncoder, &renderPassDesc);

    // Set sensible deafult states for the viewport, scissor, and blend constants
    defaultViewport.x = 0;
    defaultViewport.y = 0;
    defaultViewport.w = (float)framebufferWidth;
    defaultViewport.h = (float)framebufferHeight;
    defaultViewport.min_depth = 0;
    defaultViewport.max_depth = 1;
    wgpu_cmd_buf->currentViewport = (WebGPUViewport){ defaultViewport.x, defaultViewport.y, defaultViewport.w, defaultViewport.h, defaultViewport.min_depth, defaultViewport.max_depth };

    WebGPU_SetViewport(commandBuffer, &defaultViewport);

    defaultScissor.x = 0;
    defaultScissor.y = 0;
    defaultScissor.w = (Sint32)framebufferWidth;
    defaultScissor.h = (Sint32)framebufferHeight;
    wgpu_cmd_buf->currentScissor = (WebGPURect){ defaultScissor.x, defaultScissor.y, defaultScissor.w, defaultScissor.h };

    WebGPU_SetScissorRect(commandBuffer, &defaultScissor);

    defaultBlendConstants = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };
    WebGPU_SetBlendConstants(commandBuffer, defaultBlendConstants);

    WebGPU_SetStencilReference(commandBuffer, 0);

    wgpu_cmd_buf->common.render_pass = (Pass){
        .command_buffer = commandBuffer,
        .in_progress = true,
    };
}

static void WebGPU_EndRenderPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;

    // Finish the render pass with all bind groups set
    wgpuRenderPassEncoderEnd(wgpu_cmd_buf->renderPassEncoder);
    wgpuRenderPassEncoderRelease(wgpu_cmd_buf->renderPassEncoder);
}

static void WebGPU_BeginCopyPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WGPUCommandEncoderDescriptor commandEncoderDesc = {
        .label = "SDL_GPU Copy Encoder",
    };
    wgpu_cmd_buf->commandEncoder = wgpuDeviceCreateCommandEncoder(wgpu_cmd_buf->renderer->device, &commandEncoderDesc);
}

static void WebGPU_EndCopyPass(SDL_GPUCommandBuffer *commandBuffer)
{
    (void)commandBuffer;
    // No need to do anything here, everything is handled in Submit for WGPU
}

// ---------------------------------------------------
// Swapchain & Window Related Functions
// ---------------------------------------------------

bool WebGPU_INTERNAL_CreateSurface(WebGPURenderer *renderer, WebGPUWindow *windowData)
{
    if (!renderer || !windowData) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid parameters for creating surface");
        return false;
    }

    WGPUSurfaceDescriptor surfaceDescriptor = {
        .label = "SDL_GPU Swapchain Surface",
    };

// Slightly altered, though with permission by Elie Michel:
// @ https://github.com/eliemichel/sdl3webgpu/blob/main/sdl3webgpu.c
// https://github.com/libsdl-org/SDL/issues/10768#issuecomment-2499532299
#if defined(SDL_PLATFORM_MACOS)
    {
        id metal_layer = NULL;
        NSWindow *ns_window = (__bridge NSWindow *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
        if (!ns_window)
            return NULL;
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
#else
        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
#endif
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = metal_layer;

        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;
    }
#elif defined(SDL_PLATFORM_IOS)
    {
        UIWindow *ui_window = (__bridge UIWindow *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
        if (!uiwindow)
            return NULL;

        UIView *ui_view = ui_window.rootViewController.view;
        CAMetalLayer *metal_layer = [CAMetalLayer new];
        metal_layer.opaque = true;
        metal_layer.frame = ui_view.frame;
        metal_layer.drawableSize = ui_view.frame.size;

        [ui_view.layer addSublayer:metal_layer];

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
#else
        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
#endif
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = metal_layer;

        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;
    }
#elif defined(SDL_PLATFORM_LINUX)
#if defined(SDL_VIDEO_DRIVER_X11)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        Display *x11_display = (Display *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Window x11_window = (Window)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (!x11_display || !x11_window)
            return NULL;

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceXlibWindow fromXlibWindow;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
#else
        WGPUSurfaceDescriptorFromXlibWindow fromXlibWindow;
        fromXlibWindow.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
#endif
        fromXlibWindow.chain.next = NULL;
        fromXlibWindow.display = x11_display;
        fromXlibWindow.window = x11_window;

        surfaceDescriptor.nextInChain = &fromXlibWindow.chain;
        surfaceDescriptor.label = NULL;
    }
#endif // defined(SDL_VIDEO_DRIVER_X11)
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        struct wl_display *wayland_display = (struct wl_display *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        struct wl_surface *wayland_surface = (struct wl_surface *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        if (!wayland_display || !wayland_surface)
            return NULL;

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
#else
        WGPUSurfaceDescriptorFromWaylandSurface fromWaylandSurface;
        fromWaylandSurface.chain.sType = WGPUSType_SurfaceDescriptorFromWaylandSurface;
#endif
        fromWaylandSurface.chain.next = NULL;
        fromWaylandSurface.display = wayland_display;
        fromWaylandSurface.surface = wayland_surface;

        surfaceDescriptor.nextInChain = &fromWaylandSurface.chain;
        surfaceDescriptor.label = NULL;
    }
#endif // defined(SDL_VIDEO_DRIVER_WAYLAND)
#elif defined(SDL_PLATFORM_WIN32)
    {
        HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (!hwnd)
            return NULL;
        HINSTANCE hinstance = GetModuleHandle(NULL);

#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceWindowsHWND fromWindowsHWND;
        fromWindowsHWND.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
#else
        WGPUSurfaceDescriptorFromWindowsHWND fromWindowsHWND;
        fromWindowsHWND.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
#endif
        fromWindowsHWND.chain.next = NULL;
        fromWindowsHWND.hinstance = hinstance;
        fromWindowsHWND.hwnd = hwnd;

        surfaceDescriptor.nextInChain = &fromWindowsHWND.chain;
        surfaceDescriptor.label = NULL;
    }
#elif defined(SDL_PLATFORM_EMSCRIPTEN)
    {
#ifdef WEBGPU_BACKEND_DAWN
        WGPUSurfaceSourceCanvasHTMLSelector_Emscripten fromCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceSourceCanvasHTMLSelector_Emscripten;
#else
        WGPUSurfaceDescriptorFromCanvasHTMLSelector fromCanvasHTMLSelector;
        fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
#endif
        fromCanvasHTMLSelector.chain.next = NULL;
        fromCanvasHTMLSelector.selector = "#canvas";
        SDL_Log("Creating surface from canvas selector %s", fromCanvasHTMLSelector.selector);

        surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
        surfaceDescriptor.label = NULL;
    }
#else
#error "Unsupported WGPU_TARGET"
#endif
    windowData->swapchainData.surface = wgpuInstanceCreateSurface(renderer->instance, &surfaceDescriptor);
    windowData->swapchainData.surfaceDesc = surfaceDescriptor;
    return windowData->swapchainData.surface != NULL;
}

static void WebGPU_CreateSwapchain(WebGPURenderer *renderer, WebGPUWindow *windowData)
{
    SDL_assert(WebGPU_INTERNAL_CreateSurface(renderer, windowData));
    SDL_assert(windowData->swapchainData.surface);

    WebGPUSwapchainData *swapchainData = &windowData->swapchainData;

    // This is done as a temporary workaround until I can figure out why the macros in WebGPU_INTERNAL_CreateSurface are not working.
    // Ideally we would just call WebGPU_INTERNAL_CreateSurface here and skip the lines between the dividers.
    // -----
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
        .label = "SDL_GPU Swapchain Surface",
    };
    swapchainData->surface = wgpuInstanceCreateSurface(renderer->instance, &surf_desc);
    // -----

    swapchainData->format = wgpuSurfaceGetPreferredFormat(swapchainData->surface, renderer->adapter);
    swapchainData->presentMode = SDLToWGPUPresentMode(windowData->presentMode);
    wgpuSurfaceConfigure(swapchainData->surface, &(WGPUSurfaceConfiguration){
                                                     .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
                                                     .format = swapchainData->format,
                                                     .width = windowData->window->w,
                                                     .height = windowData->window->h,
                                                     .presentMode = swapchainData->presentMode,
                                                     .alphaMode = WGPUCompositeAlphaMode_Opaque,
                                                     .device = renderer->device,
                                                 });

    // Swapchain should be the size of whatever SDL_Window it is attached to
    swapchainData->width = windowData->window->w;
    swapchainData->height = windowData->window->h;
    swapchainData->sampleCount = 1;
    swapchainData->msaaView = NULL;
    swapchainData->msaaTexture = NULL;
    swapchainData->depthStencilView = NULL;
    swapchainData->depthStencilTexture = NULL;

    // Depth/stencil texture for swapchain
    WGPUTextureDescriptor depthDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = swapchainData->width,
            .height = swapchainData->height,
            .depthOrArrayLayers = 1,
        },
        .format = WGPUTextureFormat_Depth24PlusStencil8,
        .mipLevelCount = 1,
        .sampleCount = swapchainData->sampleCount != 0 ? swapchainData->sampleCount : 1,
        .label = "CanvasDepth/Stencil",
    };
    swapchainData->depthStencilTexture = wgpuDeviceCreateTexture(renderer->device, &depthDesc);
    swapchainData->depthStencilView = wgpuTextureCreateView(swapchainData->depthStencilTexture,
                                                            &(WGPUTextureViewDescriptor){
                                                                .label = "CanvasDepth/StencilView",
                                                                .format = WGPUTextureFormat_Depth24PlusStencil8,
                                                                .dimension = WGPUTextureViewDimension_2D,
                                                                .mipLevelCount = 1,
                                                                .arrayLayerCount = 1,
                                                            });

    // MSAA texture for swapchain
    if (swapchainData->sampleCount > 1) {
        WGPUTextureDescriptor msaaDesc = {
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = swapchainData->width,
                .height = swapchainData->height,
                .depthOrArrayLayers = 1,
            },
            .format = swapchainData->format,
            .mipLevelCount = 1,
            .sampleCount = swapchainData->sampleCount,
            .label = "CanvasMSAA",
        };
        swapchainData->msaaTexture = wgpuDeviceCreateTexture(renderer->device, &msaaDesc);
        swapchainData->msaaView = wgpuTextureCreateView(swapchainData->msaaTexture, NULL);
    }

    SDL_Log("WebGPU: Created swapchain surface %p of size %dx%d", swapchainData->surface, swapchainData->width, swapchainData->height);
}

static SDL_GPUTextureFormat WebGPU_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    WebGPUSwapchainData *swapchainData = &windowData->swapchainData;

    return WGPUToSDLTextureFormat(swapchainData->format);
}

static void WebGPU_DestroySwapchain(WebGPUSwapchainData *swapchainData)
{
    if (swapchainData->msaaView) {
        wgpuTextureViewRelease(swapchainData->msaaView);
        swapchainData->msaaView = NULL;
    }
    if (swapchainData->msaaTexture) {
        wgpuTextureRelease(swapchainData->msaaTexture);
        swapchainData->msaaTexture = NULL;
    }
    if (swapchainData->depthStencilView) {
        wgpuTextureViewRelease(swapchainData->depthStencilView);
        swapchainData->depthStencilView = NULL;
    }
    if (swapchainData->depthStencilTexture) {
        wgpuTextureRelease(swapchainData->depthStencilTexture);
        swapchainData->depthStencilTexture = NULL;
    }
    if (swapchainData->surface) {
        wgpuSurfaceRelease(swapchainData->surface);
        swapchainData->surface = NULL;
    }
}

static void WebGPU_RecreateSwapchain(WebGPURenderer *renderer, WebGPUWindow *windowData)
{
    WebGPU_DestroySwapchain(&windowData->swapchainData);
    WebGPU_CreateSwapchain(renderer, windowData);
    windowData->needsSwapchainRecreate = false;
}

static WGPUTexture WebGPU_INTERNAL_AcquireSurfaceTexture(WebGPURenderer *renderer, WebGPUWindow *windowData)
{
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(windowData->swapchainData.surface, &surfaceTexture);

    switch (surfaceTexture.status) {
    case WGPUSurfaceGetCurrentTextureStatus_Success:
        break;
    case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "GPU DEVICE LOST");
        SDL_SetError("GPU DEVICE LOST");
        return NULL;
    case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        SDL_OutOfMemory();
        return NULL;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
    default:
        WebGPU_RecreateSwapchain(renderer, windowData);
        return NULL;
    }

    return surfaceTexture.texture;
}

static bool WebGPU_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    SDL_GPUTexture **ret_texture,
    Uint32 *ret_width,
    Uint32 *ret_height)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = wgpu_cmd_buf->renderer;
    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    WebGPUSwapchainData *swapchainData = &windowData->swapchainData;

    // Check if the swapchain needs to be recreated
    if (windowData->needsSwapchainRecreate) {
        WebGPU_RecreateSwapchain(renderer, windowData);
        swapchainData = &windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain");
            SDL_SetError("Failed to recreate swapchain");
            return false;
        }
    }

    // Get the current texture from the swapchain
    WGPUTexture currentTexture = WebGPU_INTERNAL_AcquireSurfaceTexture(renderer, windowData);
    if (currentTexture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to acquire texture from swapchain");
        SDL_SetError("Failed to acquire texture from swapchain");
        return false;
    }

    // Create a temporary WebGPUTexture to return
    WebGPUTexture *texture = SDL_calloc(1, sizeof(WebGPUTexture));
    if (!texture) {
        SDL_OutOfMemory();
        return false;
    }

    texture->texture = currentTexture;
    texture->fullView = wgpuTextureCreateView(currentTexture, NULL);
    texture->dimensions = (WGPUExtent3D){
        .width = swapchainData->width,
        .height = swapchainData->height,
        .depthOrArrayLayers = 1,
    };
    texture->type = SDL_GPU_TEXTURETYPE_2D;
    texture->isMSAAColorTarget = swapchainData->sampleCount > 1;
    texture->format = WGPUToSDLTextureFormat(swapchainData->format);
    texture->usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;
    texture->common = (TextureCommonHeader){
        .info = {
            .usage = texture->usage,
            .type = texture->type,
            .format = texture->format,
            .width = texture->dimensions.width,
            .height = texture->dimensions.height,
            .num_levels = 1,
            .sample_count = swapchainData->sampleCount,
            .layer_count_or_depth = 1,
        },
    };

    // For MSAA, we'll return the MSAA texture instead of the swapchain texture
    if (swapchainData->sampleCount > 1) {
        texture->texture = swapchainData->msaaTexture;
        texture->fullView = swapchainData->msaaView;
    }

    *ret_texture = (SDL_GPUTexture *)texture;

    if (ret_width) {
        *ret_width = swapchainData->width;
    }
    if (ret_height) {
        *ret_height = swapchainData->height;
    }

    // It is important to release these textures when they are no longer needed
    return true;
}

static bool WebGPU_SupportsTextureFormat(SDL_GPURenderer *driverData,
                                         SDL_GPUTextureFormat format,
                                         SDL_GPUTextureType type,
                                         SDL_GPUTextureUsageFlags usage)
{
    (void)driverData;
    WGPUTextureFormat wgpuFormat = SDLToWGPUTextureFormat(format);
    WGPUTextureUsageFlags wgpuUsage = SDLToWGPUTextureUsageFlags(usage);
    WGPUTextureDimension dimension = WGPUTextureDimension_Undefined;
    if (type == SDL_GPU_TEXTURETYPE_2D || type == SDL_GPU_TEXTURETYPE_2D_ARRAY) {
        dimension = WGPUTextureDimension_2D;
    } else if (type == SDL_GPU_TEXTURETYPE_3D || type == SDL_GPU_TEXTURETYPE_CUBE_ARRAY || type == SDL_GPU_TEXTURETYPE_CUBE) {
        dimension = WGPUTextureDimension_3D;
    }

    // Verify that the format, usage, and dimension are considered valid
    if (wgpuFormat == WGPUTextureFormat_Undefined) {
        SDL_Log("Hi from Undefined Format!");
        return false;
    }
    if (wgpuUsage == WGPUTextureUsage_None) {
        SDL_Log("Hi from None!");
        return false;
    }
    if (dimension == WGPUTextureDimension_Undefined) {
        SDL_Log("Hi from Undefined Dimension!");
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

    SDL_Log("Desired sample count %u", desiredSampleCount);

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

static bool WebGPU_SetSwapchainParameters(SDL_GPURenderer *driverData,
                                          SDL_Window *window,
                                          SDL_GPUSwapchainComposition swapchainComposition,
                                          SDL_GPUPresentMode presentMode)
{
    (void)driverData;
    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    if (WebGPU_SupportsPresentMode(driverData, window, presentMode) && WebGPU_SupportsSwapchainComposition(driverData, window, swapchainComposition)) {
        windowData->presentMode = presentMode;
        windowData->swapchainComposition = swapchainComposition;
        windowData->needsSwapchainRecreate = true;
    } else {
        return false;
    }

    return true;
}

static bool WebGPU_ClaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = SDL_malloc(sizeof(WebGPUWindow));
        windowData->window = window;
        windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

        WebGPU_CreateSwapchain(renderer, windowData);

        if (windowData->swapchainData.surface) {
            SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(WebGPUWindow *));
            }

            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            SDL_AddEventWatch(WebGPU_INTERNAL_OnWindowResize, window);
            return true;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not create swapchain, failed to claim window!");
            SDL_free(windowData);
            return false;
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Window already claimed!");
        return false;
    }
}

static void WebGPU_ReleaseWindow(SDL_GPURenderer *driverData, SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;

    if (renderer->claimedWindowCount == 0) {
        return;
    }

    WebGPUWindow *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    if (windowData == NULL) {
        return;
    }

    // Destroy the swapchain
    if (windowData->swapchainData.surface) {
        WebGPU_DestroySwapchain(&windowData->swapchainData);
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

// Shader Functions
// ---------------------------------------------------
static void WebGPU_SetShaderLabel(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader,
    const char *label)
{
    SDL_assert(driverData && "Driver data must not be NULL when setting a shader label");
    SDL_assert(shader && "Shader must not be NULL when setting a shader label");

    WebGPUShader *wgpuShader = (WebGPUShader *)shader;
    wgpuShaderModuleSetLabel(wgpuShader->shaderModule, label);
}

static SDL_GPUShader *WebGPU_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *shaderCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a shader");
    SDL_assert(shaderCreateInfo && "Shader create info must not be NULL when creating a shader");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUShader *shader = SDL_calloc(1, sizeof(WebGPUShader));

    const char *wgsl = (const char *)shaderCreateInfo->code;
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain = {
            .sType = WGPUSType_ShaderModuleWGSLDescriptor,
            .next = NULL,
        },
        .code = wgsl,
    };

    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&wgsl_desc,
        .label = "SDL_GPU WebGPU WGSL Shader",
    };

    // Create a WebGPUShader object to cast to SDL_GPUShader *
    Uint32 entryPointNameLength = SDL_strlen(shaderCreateInfo->entrypoint) + 1;
    if (entryPointNameLength > MAX_ENTRYPOINT_LENGTH) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Entry point name \"%s\" is too long", shaderCreateInfo->entrypoint);
        SDL_free(shader);
        return NULL;
    }

    // Assign all necessary shader information
    shader->wgslSource = SDL_malloc(SDL_strlen(wgsl) + 1);
    SDL_strlcpy(shader->wgslSource, wgsl, SDL_strlen(wgsl) + 1);
    SDL_zero(shader->entrypoint);
    SDL_strlcpy((char *)shader->entrypoint, shaderCreateInfo->entrypoint, entryPointNameLength);
    shader->samplerCount = shaderCreateInfo->num_samplers;
    shader->storageBufferCount = shaderCreateInfo->num_storage_buffers;
    shader->uniformBufferCount = shaderCreateInfo->num_uniform_buffers;
    shader->storageTextureCount = shaderCreateInfo->num_storage_textures;
    shader->shaderModule = wgpuDeviceCreateShaderModule(renderer->device, &shader_desc);

    // Print out shader information if it's not a blit shader
    if (SDL_strstr(shader->entrypoint, "blit") == NULL) {
        SDL_Log("Shader Created Successfully: %s", shader->entrypoint);
        SDL_Log("entry: %s\n", shader->entrypoint);
        SDL_Log("sampler count: %u\n", shader->samplerCount);
        SDL_Log("storageBufferCount: %u\n", shader->storageBufferCount);
        SDL_Log("uniformBufferCount: %u\n", shader->uniformBufferCount);
    }

    // Set our shader referenceCount to 0 at creation
    SDL_SetAtomicInt(&shader->referenceCount, 0);
    return (SDL_GPUShader *)shader;
}

static void WebGPU_ReleaseShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a shader");
    SDL_assert(shader && "Shader must not be NULL when destroying a shader");

    WebGPUShader *wgpuShader = (WebGPUShader *)shader;

    /*// Free entry function string*/
    /*SDL_free((void *)wgpuShader->entrypoint);*/

    // Release the shader module
    wgpuShaderModuleRelease(wgpuShader->shaderModule);

    SDL_free(shader);
}

/*static void WebGPU_INTERNAL_TrackGraphicsPipeline(WebGPUCommandBuffer *commandBuffer,*/
/*                                                  WebGPUGraphicsPipeline *graphicsPipeline)*/
/*{*/
/*    TRACK_RESOURCE(*/
/*        graphicsPipeline,*/
/*        WebGPUGraphicsPipeline *,*/
/*        usedGraphicsPipelines,*/
/*        usedGraphicsPipelineCount,*/
/*        usedGraphicsPipelineCapacity)*/
/*}*/

// When building a graphics pipeline, we need to create the VertexState which is comprised of a shader module, an entry,
// and vertex buffer layouts. Using the existing SDL_GPUVertexInputState, we can create the vertex buffer layouts and
// pass them to the WGPUVertexState.
static WGPUVertexBufferLayout *WebGPU_INTERNAL_CreateVertexBufferLayouts(WebGPURenderer *renderer, const SDL_GPUVertexInputState *vertexInputState)
{
    if (vertexInputState == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Vertex input state must not be NULL when creating vertex buffer layouts");
        return NULL;
    }

    // Allocate memory for the vertex buffer layouts if needed.
    // Otherwise, early return NULL if there are no vertex buffers to create layouts for.
    WGPUVertexBufferLayout *vertexBufferLayouts;
    if (vertexInputState->num_vertex_buffers != 0) {
        vertexBufferLayouts = SDL_malloc(sizeof(WGPUVertexBufferLayout) * vertexInputState->num_vertex_buffers);
        if (vertexBufferLayouts == NULL) {
            SDL_OutOfMemory();
            return NULL;
        }
    } else {
        return NULL;
    }

    // Iterate through the vertex attributes and build the WGPUVertexAttribute array.
    // We also determine where each attribute belongs. This is used to build the vertex buffer layouts.
    WGPUVertexAttribute attributes[vertexInputState->num_vertex_attributes];
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

        // Build the attributes for the current iteration's vertex buffer
        WGPUVertexAttribute *buffer_attributes;
        if (numAttributes == 0) {
            buffer_attributes = NULL;
            SDL_Log("No attributes found for vertex buffer %d", i);
        } else {
            // Currently requires a malloc for the buffer attributes to make it to pipeline creation
            // before being freed.
            // I HATE THIS AND WANT TO REFACTOR IT.
            buffer_attributes = SDL_malloc(sizeof(WGPUVertexAttribute) * numAttributes);
            if (buffer_attributes == NULL) {
                SDL_OutOfMemory();
                return NULL;
            }

            int count = 0;
            // Iterate through the vertex attributes and populate the attributes array
            for (Uint32 j = 0; j < vertexInputState->num_vertex_attributes; j += 1) {
                if (attribute_buffer_indices[j] == i) {
                    // We need to make an explicit copy of the attribute to avoid issues with the original being freed
                    SDL_memcpy(&buffer_attributes[count], &attributes[j], sizeof(WGPUVertexAttribute));
                    count += 1;
                }
            } // End attribute iteration
        }

        // Build the vertex buffer layout for the current vertex buffer using the attributes list (can be NULL)
        // This is then passed to the vertex state for the render pipeline
        const SDL_GPUVertexBufferDescription *vertexBuffer = &vertexInputState->vertex_buffer_descriptions[i];
        vertexBufferLayouts[i] = (WGPUVertexBufferLayout){
            .arrayStride = vertexBuffer->pitch,
            .stepMode = SDLToWGPUInputStepMode(vertexBuffer->input_rate),
            .attributeCount = numAttributes,
            .attributes = buffer_attributes,
        };
    }

    // Return a pointer to the head of the vertex buffer layouts
    return vertexBufferLayouts;
}

static void WebGPU_INTERNAL_GetGraphicsPipelineBindingInfo(WebGPUBindingInfo *dstBindings,
                                                           WebGPUBindingInfo *bindingsA,
                                                           Uint32 countA,
                                                           WebGPUBindingInfo *bindingsB,
                                                           Uint32 countB,
                                                           Uint32 *retCount)
{
    /*// Combine both arrays into a set (no duplicates)*/
    WebGPUBindingInfo *pipelineBindings = dstBindings;
    Uint32 combinedCount = 0;

    if (bindingsA == NULL && bindingsB == NULL) {
        *retCount = 0;
        return;
    }

    // Iterate through the first array and add to the combined array
    for (Uint32 i = 0; i < countA; i += 1) {
        WebGPUBindingInfo *binding = &bindingsA[i];
        SDL_memcpy(&pipelineBindings[combinedCount], binding, sizeof(WebGPUBindingInfo));
        combinedCount += 1;
    }

    // Iterate through the second array and add to the combined array
    for (Uint32 i = 0; i < countB; i += 1) {
        WebGPUBindingInfo *binding = &bindingsB[i];
        bool found = false;

        // Check if the binding is already in the combined array
        for (Uint32 j = 0; j < combinedCount; j += 1) {
            if (pipelineBindings[j].binding == binding->binding && pipelineBindings[j].group == binding->group) {
                found = true;
                // Combine stages if they match
                pipelineBindings[j].stage |= binding->stage;
                break;
            }
        }

        // If the binding is not found, add it to the combined array
        if (!found) {
            SDL_memcpy(&pipelineBindings[combinedCount], binding, sizeof(WebGPUBindingInfo));
            combinedCount += 1;
        }
    }

    *retCount = combinedCount;
    /*return pipelineBindings;*/
}

static WebGPUPipelineResourceLayout *WebGPU_INTERNAL_CreatePipelineResourceLayout(
    WebGPURenderer *renderer,
    const void *pipelineCreateInfo,
    bool isComputePipeline)
{
    // Create some structure to hold our BindGroupLayouts for our graphics pipeline
    WebGPUPipelineResourceLayout *resourceLayout = SDL_malloc(sizeof(WebGPUPipelineResourceLayout));
    if (resourceLayout == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    WebGPUBindingInfo pipelineBindings[MAX_PIPELINE_BINDINGS];
    Uint32 bindingCount = 0;

    if (isComputePipeline) {
        // Compute pipeline resource layout creation
        SDL_Log("Creating Compute Pipeline Resource Layout");
        const SDL_GPUComputePipelineCreateInfo *pipelineInfo = (const SDL_GPUComputePipelineCreateInfo *)pipelineCreateInfo;
        WebGPU_INTERNAL_ExtractBindingsFromWGSL((WebGPUBindingInfo *)pipelineBindings, (const char *)pipelineInfo->code, &bindingCount, WEBGPU_SHADER_STAGE_COMPUTE);
    } else {
        // Graphics pipeline resource layout creation
        SDL_Log("Creating Graphics Pipeline Resource Layout");

        const SDL_GPUGraphicsPipelineCreateInfo *pipelineInfo = (const SDL_GPUGraphicsPipelineCreateInfo *)pipelineCreateInfo;
        WebGPUShader *vertShader = (WebGPUShader *)pipelineInfo->vertex_shader;
        WebGPUShader *fragShader = (WebGPUShader *)pipelineInfo->fragment_shader;

        // Get binding information required for the pipeline from the shaders
        Uint32 vertBindingCount = 0;
        Uint32 fragBindingCount = 0;

        // This allocates more memory than needed because there can be duplicate bindings between the vertex and fragment shaders
        WebGPUBindingInfo vertBindingInfo[MAX_PIPELINE_BINDINGS];
        WebGPUBindingInfo fragBindingInfo[MAX_PIPELINE_BINDINGS];

        // Extract bindings from the shaders into two sets of bindings that will be vetted for duplicates and then combined.
        WebGPU_INTERNAL_ExtractBindingsFromWGSL((WebGPUBindingInfo *)vertBindingInfo, vertShader->wgslSource, &vertBindingCount, WEBGPU_SHADER_STAGE_VERTEX);
        WebGPU_INTERNAL_ExtractBindingsFromWGSL((WebGPUBindingInfo *)fragBindingInfo, fragShader->wgslSource, &fragBindingCount, WEBGPU_SHADER_STAGE_FRAGMENT);

        // Get the combined bindings for the graphics pipeline
        bindingCount = SDL_max(vertBindingCount, fragBindingCount);
        WebGPU_INTERNAL_GetGraphicsPipelineBindingInfo((WebGPUBindingInfo *)&pipelineBindings,
                                                       vertBindingInfo, vertBindingCount, fragBindingInfo, fragBindingCount, &bindingCount);

        // Since we now have the combined bindings, we must ensure that we do not exceed the maximum number of pipeline bindings
        if (bindingCount > MAX_PIPELINE_BINDINGS) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU Pipeline has too many bindings! Max is %d", MAX_PIPELINE_BINDINGS);
            SDL_free(resourceLayout);
            return NULL;
        }
    }

    // Get number of bind groups required for the pipeline
    Uint32 bindGroupCount = 0;
    for (Uint32 i = 0; i < bindingCount; i += 1) {
        bindGroupCount = SDL_max(bindGroupCount, pipelineBindings[i].group + 1);
    }
    resourceLayout->bindGroupLayoutCount = bindGroupCount;

    SDL_Log("Creating %d BindGroupLayouts for Pipeline Resource Layout", bindGroupCount);
    SDL_zero(resourceLayout->bindGroupLayouts);

    for (Uint32 i = 0; i < bindGroupCount; i += 1) {
        size_t bindingsInGroup = 0;
        // Iterate through the bindings and count the number of bindings in the group
        for (Uint32 j = 0; j < bindingCount; j += 1) {
            if (pipelineBindings[j].group == i) {
                // Here we want to make sure that when we create a BGL, our layout knows the group which it belongs to
                resourceLayout->bindGroupLayouts[i].group = pipelineBindings[j].group;
                bindingsInGroup += 1;
            }
        }

        SDL_zero(resourceLayout->bindGroupLayouts[i].bindings);
        resourceLayout->bindGroupLayouts[i].bindingCount = bindingsInGroup;
    }

    // Based on the array, assign the correct bind group layout to the resource layout
    // This is done by iterating through the bindings and assigning them to the correct bind group layout and binding
    for (size_t i = 0; i < bindingCount; i++) {
        WebGPUBindingInfo *binding = &pipelineBindings[i];
        WebGPUBindGroupLayout *layout = &resourceLayout->bindGroupLayouts[binding->group];

        // Add the binding to the bind group layout
        WebGPUBindingInfo *layoutBinding = &layout->bindings[binding->binding];

        // Set the group for the layout
        layout->group = binding->group;
        layoutBinding->group = binding->group;
        layoutBinding->binding = binding->binding;
        layoutBinding->type = binding->type;
        layoutBinding->stage = binding->stage;
        layoutBinding->viewDimension = binding->viewDimension;
    }

    // We need to iterate through our resource layout bind group layouts and create the WGPUBindGroupLayouts using LayoutEntries and BindGroupLayoutDescriptors
    WGPUBindGroupLayout layouts[bindGroupCount];
    for (Uint32 i = 0; i < bindGroupCount; i += 1) {
        WebGPUBindGroupLayout *layout = &resourceLayout->bindGroupLayouts[i];

        // Store layout entries for the bind group layout
        WGPUBindGroupLayoutEntry layoutEntries[layout->bindingCount];
        WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
            .label = "SDL_GPU WebGPU Bind Group Layout",
            .entryCount = layout->bindingCount,
            .entries = layoutEntries,
        };

        // TODO: Improve this so that it can handle all types of bindings.
        // Currently does not support storage variants of the bindings.
        for (Uint32 j = 0; j < layout->bindingCount; j += 1) {
            WebGPUBindingInfo *binding = &layout->bindings[j];
            WGPUShaderStage stage = WGPUShaderStage_None;
            if (binding->stage & WEBGPU_SHADER_STAGE_VERTEX) {
                stage |= WGPUShaderStage_Vertex;
            }
            if (binding->stage & WEBGPU_SHADER_STAGE_FRAGMENT) {
                stage |= WGPUShaderStage_Fragment;
            }
            if (binding->stage & WEBGPU_SHADER_STAGE_COMPUTE) {
                stage |= WGPUShaderStage_Compute;
            }

            layoutEntries[j] = (WGPUBindGroupLayoutEntry){
                .binding = binding->binding,
                .visibility = stage,
            };

            switch (binding->type) {
            case WGPUBindingType_Texture:
                SDL_Log("View Dimension: %s", WebGPU_GetTextureViewDimensionString(binding->viewDimension));
                layoutEntries[j].texture = (WGPUTextureBindingLayout){
                    .sampleType = WGPUTextureSampleType_Float,
                    .viewDimension = binding->viewDimension,
                    .multisampled = false,
                };
                break;
            case WGPUBindingType_Buffer:
                layoutEntries[j].buffer = (WGPUBufferBindingLayout){
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = 0,
                    .hasDynamicOffset = false,
                };
                break;
            case WGPUBindingType_Sampler:
                layoutEntries[j].sampler = (WGPUSamplerBindingLayout){
                    .type = WGPUSamplerBindingType_Filtering,
                };
                break;
            case WGPUBindingType_UniformBuffer:
                layoutEntries[j].buffer = (WGPUBufferBindingLayout){
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = 0,
                    .hasDynamicOffset = false,
                };
                break;
            default:
                // Handle unexpected types if necessary
                break;
            }
        }

        layout->layout = wgpuDeviceCreateBindGroupLayout(renderer->device, &bindGroupLayoutDesc);
        // Create the bind group layout from the descriptor
        layouts[i] = layout->layout;
    }

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor layoutDesc = {
        .label = "SDL_GPU WebGPU Pipeline Layout",
        .bindGroupLayoutCount = bindGroupCount,
        .bindGroupLayouts = layouts,
    };

    // Create the pipeline layout from the descriptor
    resourceLayout->pipelineLayout = wgpuDeviceCreatePipelineLayout(renderer->device, &layoutDesc);
    return resourceLayout;
}

static SDL_GPUGraphicsPipeline *WebGPU_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    const SDL_GPUGraphicsPipelineCreateInfo *pipelineCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a graphics pipeline");
    SDL_assert(pipelineCreateInfo && "Pipeline create info must not be NULL when creating a graphics pipeline");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUGraphicsPipeline *pipeline = SDL_calloc(1, sizeof(WebGPUGraphicsPipeline));
    if (!pipeline) {
        SDL_OutOfMemory();
        return NULL;
    }

    // Create the pipeline resource layout
    WebGPUPipelineResourceLayout *resourceLayout = WebGPU_INTERNAL_CreatePipelineResourceLayout(renderer, pipelineCreateInfo, false);
    if (!resourceLayout) {
        SDL_free(pipeline);
        return NULL;
    }
    pipeline->resourceLayout = resourceLayout;

    SDL_Log("Created Pipeline Resource Layout");

    // Preconstruct the bind groups for the pipeline.
    SDL_zero(pipeline->bindGroups);
    pipeline->bindGroupCount = resourceLayout->bindGroupLayoutCount;

    // Used to determine when to cycle the bind groups if any of the bindings have changed
    pipeline->bindSamplerHash = 0;
    pipeline->bindXXXXHash = 0;
    pipeline->bindYYYYHash = 0;
    pipeline->bindZZZZHash = 0;

    // set to true for the first frame to ensure bind groups are created
    pipeline->cycleBindGroups = true;

    WebGPUShader *vertShader = (WebGPUShader *)pipelineCreateInfo->vertex_shader;
    WebGPUShader *fragShader = (WebGPUShader *)pipelineCreateInfo->fragment_shader;

    const SDL_GPUVertexInputState *vertexInputState = &pipelineCreateInfo->vertex_input_state;

    // Get the vertex buffer layouts for the vertex state if they exist
    WGPUVertexBufferLayout *vertexBufferLayouts = WebGPU_INTERNAL_CreateVertexBufferLayouts(renderer, vertexInputState);

    // Create the vertex state for the render pipeline
    WGPUVertexState vertexState = {
        .module = vertShader->shaderModule,
        .entryPoint = vertShader->entrypoint,
        .bufferCount = pipelineCreateInfo->vertex_input_state.num_vertex_buffers,
        .buffers = vertexBufferLayouts,
        .constantCount = 0, // Leave as 0 as the WebGPU backend does not support push constants either
        .constants = NULL,  // Leave as NULL as the WebGPU backend does not support push constants either
    };

    // Build the color targets for the render pipeline'
    const SDL_GPUGraphicsPipelineTargetInfo *targetInfo = &pipelineCreateInfo->target_info;
    WGPUColorTargetState colorTargets[targetInfo->num_color_targets];
    for (Uint32 i = 0; i < targetInfo->num_color_targets; i += 1) {
        const SDL_GPUColorTargetDescription *colorAttachment = &targetInfo->color_target_descriptions[i];
        SDL_GPUColorTargetBlendState blendState = colorAttachment->blend_state;
        colorTargets[i] = (WGPUColorTargetState){
            .format = SDLToWGPUTextureFormat(colorAttachment->format),
            .blend = blendState.enable_blend == false
                         ? 0
                         : &(WGPUBlendState){
                               .color = {
                                   .srcFactor = SDLToWGPUBlendFactor(blendState.src_color_blendfactor),
                                   .dstFactor = SDLToWGPUBlendFactor(blendState.dst_color_blendfactor),
                                   .operation = SDLToWGPUBlendOperation(blendState.color_blend_op),
                               },
                               .alpha = {
                                   .srcFactor = SDLToWGPUBlendFactor(blendState.src_alpha_blendfactor),
                                   .dstFactor = SDLToWGPUBlendFactor(blendState.dst_alpha_blendfactor),
                                   .operation = SDLToWGPUBlendOperation(blendState.alpha_blend_op),
                               },
                           },
            .writeMask = blendState.enable_blend == true ? SDLToWGPUColorWriteMask(blendState.color_write_mask) : WGPUColorWriteMask_All
        };
    }

    // Create the fragment state for the render pipeline
    WGPUFragmentState fragmentState = {
        .module = fragShader->shaderModule,
        .entryPoint = fragShader->entrypoint,
        .constantCount = 0,
        .constants = NULL,
        .targetCount = targetInfo->num_color_targets,
        .targets = colorTargets,
    };

    WGPUDepthStencilState depthStencil;
    if (pipelineCreateInfo->target_info.has_depth_stencil_target) {
        const SDL_GPUDepthStencilState *state = &pipelineCreateInfo->depth_stencil_state;

        depthStencil.format = SDLToWGPUTextureFormat(pipelineCreateInfo->target_info.depth_stencil_format);
        depthStencil.depthWriteEnabled = state->enable_depth_write;
        depthStencil.depthCompare = SDLToWGPUCompareFunction(state->compare_op);

        depthStencil.stencilReadMask = state->compare_mask != 0 ? state->compare_mask : 0xFF;
        depthStencil.stencilWriteMask = state->write_mask;

        // If the stencil test is enabled, we need to set up the stencil state for the front and back faces
        if (state->enable_stencil_test) {
            depthStencil.stencilFront = (WGPUStencilFaceState){
                .compare = SDLToWGPUCompareFunction(state->front_stencil_state.compare_op),
                .failOp = SDLToWGPUStencilOperation(state->front_stencil_state.fail_op),
                .depthFailOp = SDLToWGPUStencilOperation(state->front_stencil_state.depth_fail_op),
                .passOp = SDLToWGPUStencilOperation(state->front_stencil_state.pass_op),
            };

            depthStencil.stencilBack = (WGPUStencilFaceState){
                .compare = SDLToWGPUCompareFunction(state->back_stencil_state.compare_op),
                .failOp = SDLToWGPUStencilOperation(state->back_stencil_state.fail_op),
                .depthFailOp = SDLToWGPUStencilOperation(state->back_stencil_state.depth_fail_op),
                .passOp = SDLToWGPUStencilOperation(state->back_stencil_state.pass_op),
            };
        }
    }

    if (pipelineCreateInfo->rasterizer_state.fill_mode == SDL_GPU_FILLMODE_LINE) {
        SDL_Log("Line fill mode not supported in WebGPU. Defaulting to fill mode.");
        SDL_Log("TODO: Implement specific pipeline setup to emulate line fill mode.");
    }

    Uint32 sampleCount = pipelineCreateInfo->multisample_state.sample_count == 0 ? 1 : pipelineCreateInfo->multisample_state.sample_count;
    Uint32 sampleMask = pipelineCreateInfo->multisample_state.sample_mask == 0 ? 0xFFFF : pipelineCreateInfo->multisample_state.sample_mask;
    if (sampleCount != SDL_GPU_SAMPLECOUNT_1 && sampleCount != SDL_GPU_SAMPLECOUNT_4) {
        SDL_Log("Sample count not supported in WebGPU. Defaulting to 1.");
        sampleCount = SDL_GPU_SAMPLECOUNT_1;
    }

    // Create the render pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc = {
        .nextInChain = NULL,
        .label = "SDL_GPU WebGPU Render Pipeline",
        .layout = resourceLayout->pipelineLayout,
        .vertex = vertexState,
        .primitive = {
            .topology = SDLToWGPUPrimitiveTopology(pipelineCreateInfo->primitive_type),
            .stripIndexFormat = WGPUIndexFormat_Undefined, // TODO: Support strip index format Uint16 or Uint32
            .frontFace = SDLToWGPUFrontFace(pipelineCreateInfo->rasterizer_state.front_face),
            .cullMode = SDLToWGPUCullMode(pipelineCreateInfo->rasterizer_state.cull_mode),
        },
        // Needs to be set up
        .depthStencil = pipelineCreateInfo->target_info.has_depth_stencil_target ? &depthStencil : NULL,
        .multisample = {
            .count = SDLToWGPUSampleCount(sampleCount),
            .mask = sampleMask,
            .alphaToCoverageEnabled = false,
        },
        .fragment = &fragmentState,
    };

    // Create the WebGPU render pipeline from the descriptor
    WGPURenderPipeline wgpuPipeline = wgpuDeviceCreateRenderPipeline(renderer->device, &pipelineDesc);

    pipeline->pipeline = wgpuPipeline;
    pipeline->pipelineDesc = pipelineDesc;
    pipeline->vertexShader = vertShader;
    pipeline->fragmentShader = fragShader;
    pipeline->primitiveType = pipelineCreateInfo->primitive_type;

    // Set the reference count to 0
    SDL_SetAtomicInt(&pipeline->referenceCount, 0);

    // Iterate through the VertexBufferLayouts and free the attributes, then free the layout.
    // This can be done since everything has already been copied to the final render pipeline.
    for (Uint32 i = 0; i < vertexInputState->num_vertex_buffers; i += 1) {
        WGPUVertexBufferLayout *bufferLayout = &vertexBufferLayouts[i];
        if (bufferLayout->attributes != NULL) {
            SDL_free((void *)bufferLayout->attributes);
        }
        SDL_free(bufferLayout);
    }

    SDL_Log("Graphics Pipeline Created Successfully");
    return (SDL_GPUGraphicsPipeline *)pipeline;
}

static void WebGPU_ReleaseGraphicsPipeline(SDL_GPURenderer *driverData,
                                           SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPUGraphicsPipeline *pipeline = (WebGPUGraphicsPipeline *)graphicsPipeline;

    if (SDL_GetAtomicInt(&pipeline->referenceCount) > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Releasing a graphics pipeline with active references!");
    }

    for (Uint32 i = 0; i < pipeline->vertexUniformBufferCount; i += 1) {
        WebGPU_ReleaseBuffer(driverData, (SDL_GPUBuffer *)pipeline->vertexUniformBuffers[i].buffer);
    }

    for (Uint32 i = 0; i < pipeline->fragUniformBufferCount; i += 1) {
        WebGPU_ReleaseBuffer(driverData, (SDL_GPUBuffer *)pipeline->fragUniformBuffers[i].buffer);
    }

    if (pipeline->pipeline) {
        wgpuPipelineLayoutRelease(pipeline->resourceLayout->pipelineLayout);
        wgpuRenderPipelineRelease(pipeline->pipeline);
    }
}

// Texture Functions
// ---------------------------------------------------

static void WebGPU_SetTextureName(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture,
    const char *name)
{
    SDL_assert(driverData && "Driver data must not be NULL when setting a texture name");
    SDL_assert(texture && "Texture must not be NULL when setting a texture name");

    WebGPUTexture *webgpuTexture = (WebGPUTexture *)texture;

    // Set the texture name
    if (webgpuTexture->debugName) {
        SDL_free((void *)webgpuTexture->debugName);
    }

    webgpuTexture->debugName = SDL_strdup(name);

    // Set the texture view name
    wgpuTextureSetLabel(webgpuTexture->texture, name);
    wgpuTextureViewSetLabel(webgpuTexture->fullView, name);
}

static SDL_GPUTexture *WebGPU_CreateTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *textureCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a texture");
    SDL_assert(textureCreateInfo && "Texture create info must not be NULL when creating a texture");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUTexture *texture = (WebGPUTexture *)SDL_calloc(1, sizeof(WebGPUTexture));
    if (!texture) {
        SDL_OutOfMemory();
        return NULL;
    }

    // Ensure that the layer count is at least 1
    Uint32 layerCount = SDL_max(textureCreateInfo->layer_count_or_depth, 1);

    WGPUTextureDescriptor textureDesc = {
        .label = "New SDL_GPU WebGPU Texture",
        .size = (WGPUExtent3D){
            .width = textureCreateInfo->width,
            .height = textureCreateInfo->height,
            .depthOrArrayLayers = layerCount,
        },
        .mipLevelCount = textureCreateInfo->num_levels,
        .sampleCount = SDLToWGPUSampleCount(textureCreateInfo->sample_count),
        .dimension = SDLToWGPUTextureDimension(textureCreateInfo->type),
        .format = SDLToWGPUTextureFormat(textureCreateInfo->format),
        .usage = SDLToWGPUTextureUsageFlags(textureCreateInfo->usage) | WGPUTextureUsage_CopySrc,
    };

    WGPUTexture wgpuTexture = wgpuDeviceCreateTexture(renderer->device, &textureDesc);
    if (wgpuTexture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture");
        SDL_free(texture);
        SDL_OutOfMemory();
        return NULL;
    }

    // Create the WebGPUTexture object
    texture->texture = wgpuTexture;
    texture->usage = textureCreateInfo->usage;
    texture->format = textureCreateInfo->format;
    texture->dimensions = textureDesc.size;
    texture->layerCount = layerCount;
    texture->type = textureCreateInfo->type;
    texture->levelCount = textureCreateInfo->num_levels;
    SDL_memcpy(&texture->common.info, textureCreateInfo, sizeof(SDL_GPUTextureCreateInfo));

    /*INTERNAL_PRINT_32BITS(texture->common.info.usage);*/

    char tex_label[64];
    char view_label[64];
    SDL_snprintf(tex_label, sizeof(tex_label), "SDL_GPU WebGPU Texture %p", wgpuTexture);
    wgpuTextureSetLabel(wgpuTexture, tex_label);

    SDL_snprintf(view_label, sizeof(view_label), "SDL_GPU WebGPU Texture %p's View", wgpuTexture);

    WGPUTextureViewDimension dimension = SDLToWGPUTextureViewDimension(textureCreateInfo->type);
    SDL_Log("Texture Dimension: %s", WebGPU_GetTextureViewDimensionString(dimension));

    // Create Texture View for the texture
    WGPUTextureViewDescriptor viewDesc = {
        .label = view_label,
        .format = textureDesc.format,
        .dimension = dimension,
        .baseMipLevel = 0,
        .mipLevelCount = textureCreateInfo->num_levels,
        .baseArrayLayer = 0,
        .arrayLayerCount = dimension == WGPUTextureViewDimension_3D ? 1 : layerCount,
    };

    // Create the texture view
    texture->fullView = wgpuTextureCreateView(texture->texture, &viewDesc);
    if (texture->fullView == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture view");
        SDL_free(texture);
        SDL_OutOfMemory();
        return NULL;
    }

    texture->debugName = NULL;

    SDL_Log("Created texture %p", texture->texture);
    SDL_Log("Created texture view %p, for texture %p", texture->fullView, texture->texture);
    SDL_Log("Created texture's depth/arraylayers: %u", wgpuTextureGetDepthOrArrayLayers(texture->texture));

    return (SDL_GPUTexture *)texture;
}

static void WebGPU_ReleaseTexture(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a texture");
    SDL_assert(texture && "Texture must not be NULL when destroying a texture");

    WebGPUTexture *webgpuTexture = (WebGPUTexture *)texture;

    wgpuTextureDestroy(webgpuTexture->texture);

    SDL_Log("Destroyed texture %p", webgpuTexture->texture);

    // Release the texture view
    wgpuTextureViewRelease(webgpuTexture->fullView);

    SDL_Log("Released texture view %p", webgpuTexture->fullView);

    // Free the texture
    SDL_free(webgpuTexture);
}

// TODO: For buffers that meet the alignment requirements, we can use a wgpuQueueCopyBufferToTexture() instead.
// For this to work, the row pitch must be a multiple of 256 bytes.
// Otherwise we will just call wgpuQueueWriteTexture() as we do now.
static void WebGPU_UploadToTexture(SDL_GPUCommandBuffer *commandBuffer,
                                   const SDL_GPUTextureTransferInfo *source,
                                   const SDL_GPUTextureRegion *destination,
                                   bool cycle)
{
    (void)cycle;
    if (!commandBuffer || !source || !destination) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid parameters for uploading to texture");
        return;
    }

    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = (WebGPURenderer *)wgpu_cmd_buf->renderer;
    WebGPUTexture *webgpuTexture = (WebGPUTexture *)destination->texture;
    WebGPUBuffer *transfer_buffer = (WebGPUBuffer *)source->transfer_buffer;

    // TODO: Set up some HDR compatibility checks here
    // For now we'll just take the PixelFormat that SDL gives us;
    // Ideally I implement a function that takes WebGPUTextureFormat and returns Uint8 bytes per pixel
    if (renderer->pixelFormat == SDL_PIXELFORMAT_UNKNOWN) {
        renderer->pixelFormat = SDL_GetWindowPixelFormat(renderer->claimedWindows[0]->window);
    }

    WGPUTextureDataLayout dataLayout = {
        .offset = source->offset,
        .bytesPerRow = destination->w * SDL_GetPixelFormatDetails(wgpu_cmd_buf->renderer->pixelFormat)->bytes_per_pixel,
        .rowsPerImage = destination->h,
    };

    // Add these fields explicitly
    WGPUImageCopyTexture copyTexture = {
        .texture = webgpuTexture->texture,
        .mipLevel = destination->mip_level,
        .origin = (WGPUOrigin3D){
            .x = destination->x,
            .y = destination->y,
            .z = destination->layer, // layer index we are looking to transfer to
        },
        .aspect = WGPUTextureAspect_All,
    };

    WGPUExtent3D extent = {
        .width = destination->w,
        .height = destination->h,
        .depthOrArrayLayers = destination->d, // total "depth" of texture
    };

    wgpuQueueWriteTexture(
        renderer->queue,
        &copyTexture,
        transfer_buffer->mappedData,
        transfer_buffer->size, // Make sure this matches exactly what we need
        &dataLayout,
        &extent);
}

static void WebGPU_CopyTextureToTexture(SDL_GPUCommandBuffer *commandBuffer,
                                        const SDL_GPUTextureLocation *source,
                                        const SDL_GPUTextureLocation *destination,
                                        Uint32 w,
                                        Uint32 h,
                                        Uint32 d,
                                        bool cycle)
{
    (void)cycle;
    if (!commandBuffer || !source || !destination) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid parameters for copying texture to texture");
        return;
    }

    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUTexture *src_texture = (WebGPUTexture *)source->texture;
    WebGPUTexture *dst_texture = (WebGPUTexture *)destination->texture;

    WGPUImageCopyTexture srcCopyTexture = {
        .texture = src_texture->texture,
        .mipLevel = source->mip_level,
        .origin = (WGPUOrigin3D){
            .x = source->x,
            .y = source->y,
            .z = source->z,
        },
        .aspect = WGPUTextureAspect_All,
    };

    WGPUImageCopyTexture dstCopyTexture = {
        .texture = dst_texture->texture,
        .mipLevel = destination->mip_level,
        .origin = (WGPUOrigin3D){
            .x = destination->x,
            .y = destination->y,
            .z = destination->z,
        },
        .aspect = WGPUTextureAspect_All,
    };

    WGPUExtent3D extent = {
        .width = w,
        .height = h,
        .depthOrArrayLayers = d,
    };

    wgpuCommandEncoderCopyTextureToTexture(
        wgpu_cmd_buf->commandEncoder,
        &srcCopyTexture,
        &dstCopyTexture,
        &extent);
}

static void WebGPU_DownloadFromTexture(SDL_GPUCommandBuffer *commandBuffer,
                                       const SDL_GPUTextureRegion *source,
                                       const SDL_GPUTextureTransferInfo *destination)
{
    if (!commandBuffer || !source || !destination) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid parameters for downloading from texture");
        return;
    }

    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = (WebGPURenderer *)wgpu_cmd_buf->renderer;
    WebGPUTexture *webgpuTexture = (WebGPUTexture *)source->texture;
    WebGPUBuffer *transfer_buffer = (WebGPUBuffer *)destination->transfer_buffer;

    // Determine pixel format and bytes per pixel
    if (renderer->pixelFormat == SDL_PIXELFORMAT_UNKNOWN) {
        renderer->pixelFormat = SDL_GetWindowPixelFormat(renderer->claimedWindows[0]->window);
    }
    Uint32 bytesPerPixel = SDL_GetPixelFormatDetails(renderer->pixelFormat)->bytes_per_pixel;

    // Compute row pitch and alignment
    Uint32 rowPitch = source->w * bytesPerPixel;
    Uint32 alignedRowPitch = (rowPitch + 255) & ~255; // Align to 256 bytes
    Uint32 requiredSize = alignedRowPitch * source->h * source->d;

    if (requiredSize > transfer_buffer->size) {
        SDL_Log("Need to reallocate transfer buffer to size %u", requiredSize);

        // We don't use the WebGPU_CreateTransferBuffer function because we want to keep the same transfer buffer pointer
        wgpuBufferDestroy(transfer_buffer->buffer);
        wgpuDeviceCreateBuffer(renderer->device, &(WGPUBufferDescriptor){
                                                     .size = requiredSize,
                                                     .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
                                                     .mappedAtCreation = false,
                                                 });

        transfer_buffer->size = requiredSize;
        transfer_buffer->usageFlags = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    }

    // Log debug information
    SDL_Log("Downloading from texture %p to buffer %p", webgpuTexture->texture, transfer_buffer->buffer);
    SDL_Log("Texture Size: %u x %u x %u = %u", source->w, source->h, source->d, source->w * source->h * source->d);
    SDL_Log("Aligned Row Pitch: %u, Buffer Size: %u", alignedRowPitch, transfer_buffer->size);

    // Set rowsPerImage to 0 if data is tightly packed
    Uint32 rowsPerImage = (rowPitch % 256 == 0) ? source->h : 0;

    // Define data layout for the buffer
    WGPUTextureDataLayout dataLayout = {
        .offset = destination->offset,
        .bytesPerRow = alignedRowPitch,
        .rowsPerImage = rowsPerImage,
    };

    // Define the texture copy source
    WGPUImageCopyTexture copyTexture = {
        .texture = webgpuTexture->texture,
        .mipLevel = source->mip_level,
        .origin = (WGPUOrigin3D){
            .x = source->x,
            .y = source->y,
            .z = source->z,
        },
        .aspect = WGPUTextureAspect_All,
    };

    // Define the copy extent
    WGPUExtent3D extent = {
        .width = source->w,
        .height = source->h,
        .depthOrArrayLayers = source->d,
    };

    // Define the buffer copy destination
    WGPUImageCopyBuffer copyBuffer = {
        .buffer = transfer_buffer->buffer,
        .layout = dataLayout,
    };

    // Perform the copy operation
    wgpuCommandEncoderCopyTextureToBuffer(
        wgpu_cmd_buf->commandEncoder,
        &copyTexture,
        &copyBuffer,
        &extent);

    SDL_Log("Copy operation submitted successfully");
}

static SDL_GPUSampler *WebGPU_CreateSampler(
    SDL_GPURenderer *driverData,
    const SDL_GPUSamplerCreateInfo *createinfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a sampler");
    SDL_assert(createinfo && "Sampler create info must not be NULL when creating a sampler");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUSampler *sampler = SDL_malloc(sizeof(WebGPUSampler));
    if (!sampler) {
        SDL_OutOfMemory();
        return NULL;
    }

    WGPUSamplerDescriptor samplerDesc = {
        .label = "SDL_GPU WebGPU Sampler",
        .addressModeU = SDLToWGPUAddressMode(createinfo->address_mode_u),
        .addressModeV = SDLToWGPUAddressMode(createinfo->address_mode_v),
        .addressModeW = SDLToWGPUAddressMode(createinfo->address_mode_w),
        .magFilter = SDLToWGPUFilterMode(createinfo->mag_filter),
        .minFilter = SDLToWGPUFilterMode(createinfo->min_filter),
        .mipmapFilter = SDLToWGPUSamplerMipmapMode(createinfo->mipmap_mode),
        .lodMinClamp = createinfo->min_lod,
        .lodMaxClamp = createinfo->max_lod,
        .compare = SDLToWGPUCompareFunction(createinfo->compare_op),
        .maxAnisotropy = (Uint16)createinfo->max_anisotropy,
    };

    WGPUSampler wgpuSampler = wgpuDeviceCreateSampler(renderer->device, &samplerDesc);
    if (wgpuSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create sampler");
        SDL_free(sampler);
        SDL_OutOfMemory();
        return NULL;
    }

    sampler->sampler = wgpuSampler;
    return (SDL_GPUSampler *)sampler;
}

static void WebGPU_ReleaseSampler(
    SDL_GPURenderer *driverData,
    SDL_GPUSampler *sampler)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a sampler");
    SDL_assert(sampler && "Sampler must not be NULL when destroying a sampler");

    WebGPUSampler *webgpuSampler = (WebGPUSampler *)sampler;

    wgpuSamplerRelease(webgpuSampler->sampler);
    SDL_free(webgpuSampler);
}

static void WebGPU_BindFragmentSamplers(SDL_GPUCommandBuffer *commandBuffer,
                                        Uint32 firstSlot,
                                        const SDL_GPUTextureSamplerBinding *textureSamplerBindings,
                                        Uint32 numBindings)
{
    // Pipeline fragment and vertex samplers are bound in the same way because of bind groups
    WebGPU_INTERNAL_BindSamplers(commandBuffer, firstSlot, textureSamplerBindings, numBindings);
}

static void WebGPU_BindGraphicsPipeline(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUGraphicsPipeline *pipeline = (WebGPUGraphicsPipeline *)graphicsPipeline;

    // Set the current pipeline
    SDL_zero(wgpu_cmd_buf->bindGroups);
    wgpu_cmd_buf->currentGraphicsPipeline = pipeline;
    Uint32 bindGroupCount = pipeline->resourceLayout->bindGroupLayoutCount;

    // Copy the pre-constructed bind groups from the pipeline to the command buffer
    SDL_memcpy(wgpu_cmd_buf->bindGroups, pipeline->bindGroups, sizeof(WebGPUBindGroup) * bindGroupCount);
    wgpu_cmd_buf->bindGroupCount = bindGroupCount;

    // Analyze pipeline bindings and find any number of uniform buffers that need to be set.
    // Assign them in order to wgpu_cmd_buf->bindGroups[i].entries[j] where i is the bind group index
    // and j is the binding index.

    Uint32 fragUniformBufferCount = 0;
    Uint32 vertexUniformBufferCount = 0;

    // Count uniform buffers for vertex and fragment shaders
    for (int i = 0; i < bindGroupCount; i += 1) {
        WebGPUBindGroupLayout *layout = &pipeline->resourceLayout->bindGroupLayouts[i];
        WebGPUBindGroup *bindGroup = &wgpu_cmd_buf->bindGroups[i];
        SDL_zero(bindGroup->entries);
        for (int j = 0; j < layout->bindingCount; j += 1) {
            WebGPUBindingInfo *binding = &layout->bindings[j];
            if (binding->type == WGPUBindingType_UniformBuffer) {
                if (binding->stage == WEBGPU_SHADER_STAGE_FRAGMENT) {
                    WebGPUUniformBuffer *fragUniformBuffer = &wgpu_cmd_buf->currentGraphicsPipeline->fragUniformBuffers[fragUniformBufferCount];
                    fragUniformBuffer->group = binding->group;
                    fragUniformBuffer->binding = binding->binding;
                    fragUniformBufferCount += 1;
                } else if (binding->stage == WEBGPU_SHADER_STAGE_VERTEX) {
                    WebGPUUniformBuffer *vertexUniformBuffer = &wgpu_cmd_buf->currentGraphicsPipeline->vertexUniformBuffers[vertexUniformBufferCount];
                    vertexUniformBuffer->group = binding->group;
                    vertexUniformBuffer->binding = binding->binding;
                    vertexUniformBufferCount += 1;
                }
            }
        }
    }

    // Set the uniform buffer counts
    wgpu_cmd_buf->currentGraphicsPipeline->vertexUniformBufferCount = vertexUniformBufferCount;
    wgpu_cmd_buf->currentGraphicsPipeline->fragUniformBufferCount = fragUniformBufferCount;

    // Bind the pipeline
    wgpuRenderPassEncoderSetPipeline(wgpu_cmd_buf->renderPassEncoder, pipeline->pipeline);

    // Mark resources as clean
    /*wgpu_cmd_buf->resourcesDirty = false;*/
}

static void WebGPU_INTERNAL_SetBindGroups(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUGraphicsPipeline *pipeline = wgpu_cmd_buf->currentGraphicsPipeline;

    // If we have a current graphics pipeline, we need to check if we have bind groups to set
    // If so, we need to set them before we begin the render pass

    // If we have uniform buffers loaded but no entries loaded, we need to find the uniform buffer entries and set them
    if (pipeline != NULL) {
        Uint32 numBindGroups = wgpu_cmd_buf->bindGroupCount;
        WebGPUBindGroup *cmdBindGroups = wgpu_cmd_buf->bindGroups;
        WebGPUPipelineResourceLayout *resourceLayout = wgpu_cmd_buf->currentGraphicsPipeline->resourceLayout;

        // Check if we need to cycle the bind groups (if any of the bindings have changed or first load)
        if (numBindGroups != 0 && pipeline->cycleBindGroups == true) {

            // If the WGPUBindGroup does not exist, we need to create it so that we can set it on the render pass
            for (Uint32 i = 0; i < numBindGroups; i += 1) {

                SDL_zero(pipeline->bindGroups[i].entries);

                // Copy the bind group over to the pipeline so we don't have to recreate it every frame
                SDL_memcpy(&pipeline->bindGroups[i], &cmdBindGroups[i], sizeof(WebGPUBindGroup));

                WGPUBindGroupDescriptor bindGroupDesc = {
                    .layout = resourceLayout->bindGroupLayouts[i].layout,
                    .entryCount = resourceLayout->bindGroupLayouts[i].bindingCount,
                    .entries = pipeline->bindGroups[i].entries,
                };

                pipeline->bindGroups[i].entryCount = bindGroupDesc.entryCount;
                // Assign the bind group to the pipeline so that we can reuse it without recreating it.
                pipeline->bindGroups[i].bindGroup = wgpuDeviceCreateBindGroup(wgpu_cmd_buf->renderer->device, &bindGroupDesc);

                /*SDL_Log("Created bind group %u, expecting %zu entries", i, pipeline->bindGroups[i].entryCount);*/
            }

            // Reset the cycle flag
            pipeline->cycleBindGroups = false;
        }

        // Iterate over the pipeline bind groups and set them for the render pass
        for (Uint32 i = 0; i < numBindGroups; i += 1) {
            Uint32 group = resourceLayout->bindGroupLayouts[i].group;
            wgpuRenderPassEncoderSetBindGroup(wgpu_cmd_buf->renderPassEncoder, group, pipeline->bindGroups[i].bindGroup, 0, 0);
        }
    }
}

static void WebGPU_DrawPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 vertexCount,
    Uint32 instanceCount,
    Uint32 firstVertex,
    Uint32 firstInstance)
{
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPU_INTERNAL_SetBindGroups(commandBuffer);
    wgpuRenderPassEncoderDraw(wgpu_cmd_buf->renderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
}

static void WebGPU_DrawIndexedPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 numIndices,
    Uint32 numInstances,
    Uint32 firstIndex,
    Sint32 vertexOffset,
    Uint32 firstInstance)
{
    if (commandBuffer == NULL) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPU_INTERNAL_SetBindGroups(commandBuffer);
    wgpuRenderPassEncoderDrawIndexed(wgpu_cmd_buf->renderPassEncoder, numIndices, numInstances, firstIndex, vertexOffset, firstInstance);
}

static void WebGPU_DrawPrimitivesIndirect(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 drawCount)
{
    if (commandBuffer == NULL) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBuffer *wgpuBuffer = (WebGPUBuffer *)buffer;
    Uint32 pitch = sizeof(SDL_GPUIndirectDrawCommand);
    WebGPU_INTERNAL_SetBindGroups(commandBuffer);
    for (Uint32 i = 0; i < drawCount; i += 1) {
        wgpuRenderPassEncoderDrawIndirect(wgpu_cmd_buf->renderPassEncoder, wgpuBuffer->buffer, offset + (i * pitch));
    }
}

static void WebGPU_DrawIndexedIndirect(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 drawCount)
{
    if (commandBuffer == NULL) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUBuffer *wgpuBuffer = (WebGPUBuffer *)buffer;
    Uint32 pitch = sizeof(SDL_GPUIndexedIndirectDrawCommand);
    WebGPU_INTERNAL_SetBindGroups(commandBuffer);
    for (Uint32 i = 0; i < drawCount; i += 1) {
        wgpuRenderPassEncoderDrawIndexedIndirect(wgpu_cmd_buf->renderPassEncoder, wgpuBuffer->buffer, offset + (i * pitch));
    }
}

// Blit Functionality Workaround
// ---------------------------------------------------

const char *blitVert = R"(
struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) tex: vec2<f32>
};

@vertex
fn blitVert(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
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

const char *commonCode = R"(
struct SourceRegionBuffer {
    uvLeftTop: vec2<f32>,
    uvDimensions: vec2<f32>,
    mipLevel: f32,
    layerOrDepth: f32
}

@group(0) @binding(0) var sourceSampler: sampler;
@group(1) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;
)";

const char *blit2DShader = R"(
@group(0) @binding(1) var sourceTexture2D: texture_2d<f32>;

@fragment
fn blitFrom2D(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;
    return textureSampleLevel(sourceTexture2D, sourceSampler, newCoord, sourceRegion.mipLevel);
}
)";

const char *blit2DArrayShader = R"(
@group(0) @binding(1) var sourceTexture2DArray: texture_2d_array<f32>;

@fragment
fn blitFrom2DArray(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = vec2<f32>(
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex
    );
    return textureSampleLevel(sourceTexture2DArray, sourceSampler, newCoord, u32(sourceRegion.layerOrDepth), sourceRegion.mipLevel);
}
)";

const char *blit3DShader = R"(
@group(0) @binding(1) var sourceTexture3D: texture_3d<f32>;

@fragment
fn blitFrom3D(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
    let newCoord = vec3<f32>(
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex,
        sourceRegion.layerOrDepth
    );
    return textureSampleLevel(sourceTexture3D, sourceSampler, newCoord, sourceRegion.mipLevel);
}
)";

const char *blitCubeShader = R"(
@group(0) @binding(1) var sourceTextureCube: texture_cube<f32>;

@fragment
fn blitFromCube(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
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

const char *blitCubeArrayShader = R"(
@group(0) @binding(1) var sourceTextureCubeArray: texture_cube_array<f32>;

@fragment
fn blitFromCubeArray(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {
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
)";

static void WebGPU_INTERNAL_InitBlitResources(
    WebGPURenderer *renderer)
{
    SDL_GPUShaderCreateInfo shaderCreateInfo;

    SDL_Log("Initializing WebGPU blit resources");

    renderer->blitPipelineCapacity = 6;
    renderer->blitPipelineCount = 0;
    renderer->blitPipelines = (BlitPipelineCacheEntry *)SDL_malloc(
        renderer->blitPipelineCapacity * sizeof(BlitPipelineCacheEntry));

    // Fullscreen vertex shader
    SDL_zero(shaderCreateInfo);
    shaderCreateInfo.code = (Uint8 *)blitVert;
    shaderCreateInfo.code_size = SDL_strlen(blitVert);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_WGSL;
    shaderCreateInfo.entrypoint = "blitVert";
    renderer->blitVertexShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitVertexShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile vertex shader for blit!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitVertexShader, "BlitVertex");

    char *WebGPUBlit2D = SDL_stack_alloc(char, SDL_strlen(commonCode) + SDL_strlen(blit2DShader) + 1);
    if (WebGPUBlit2D == NULL) {
        SDL_OutOfMemory();
        return;
    }
    SDL_strlcpy(WebGPUBlit2D, commonCode, SDL_strlen(commonCode) + 1);
    SDL_strlcat(WebGPUBlit2D, blit2DShader, SDL_strlen(commonCode) + SDL_strlen(blit2DShader) + 1);
    shaderCreateInfo.code = (Uint8 *)WebGPUBlit2D;
    shaderCreateInfo.code_size = SDL_strlen(WebGPUBlit2D);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.num_samplers = 1;
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_WGSL;
    shaderCreateInfo.num_uniform_buffers = 1;
    shaderCreateInfo.entrypoint = "blitFrom2D";
    renderer->blitFrom2DShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom2DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2D pixel shader!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitFrom2DShader, "BlitFrom2D");

    char *WebGPUBlit2DArray = SDL_stack_alloc(char, SDL_strlen(commonCode) + SDL_strlen(blit2DArrayShader) + 1);
    if (WebGPUBlit2DArray == NULL) {
        SDL_OutOfMemory();
        return;
    }
    SDL_strlcpy(WebGPUBlit2DArray, commonCode, SDL_strlen(commonCode) + 1);
    SDL_strlcat(WebGPUBlit2DArray, blit2DArrayShader, SDL_strlen(commonCode) + SDL_strlen(blit2DArrayShader) + 1);
    shaderCreateInfo.code = (Uint8 *)WebGPUBlit2DArray;
    shaderCreateInfo.code_size = SDL_strlen(WebGPUBlit2DArray);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.entrypoint = "blitFrom2DArray";
    renderer->blitFrom2DArrayShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom2DArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom2DArray pixel shader!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitFrom2DArrayShader, "BlitFrom2DArray");

    char *WebGPUBlit3D = SDL_stack_alloc(char, SDL_strlen(commonCode) + SDL_strlen(blit3DShader) + 1);
    if (WebGPUBlit3D == NULL) {
        SDL_OutOfMemory();
        return;
    }
    SDL_strlcpy(WebGPUBlit3D, commonCode, SDL_strlen(commonCode) + 1);
    SDL_strlcat(WebGPUBlit3D, blit3DShader, SDL_strlen(commonCode) + SDL_strlen(blit3DShader) + 1);
    shaderCreateInfo.code = (Uint8 *)WebGPUBlit3D;
    shaderCreateInfo.code_size = SDL_strlen(WebGPUBlit3D);
    shaderCreateInfo.entrypoint = "blitFrom3D";
    renderer->blitFrom3DShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFrom3DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFrom3D pixel shader!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitFrom3DShader, "BlitFrom3D");

    char *WebGPUBlitCube = SDL_stack_alloc(char, SDL_strlen(commonCode) + SDL_strlen(blitCubeShader) + 1);
    if (WebGPUBlitCube == NULL) {
        SDL_OutOfMemory();
        return;
    }
    SDL_strlcpy(WebGPUBlitCube, commonCode, SDL_strlen(commonCode) + 1);
    SDL_strlcat(WebGPUBlitCube, blitCubeShader, SDL_strlen(commonCode) + SDL_strlen(blitCubeShader) + 1);
    shaderCreateInfo.code = (Uint8 *)WebGPUBlitCube;
    shaderCreateInfo.code_size = SDL_strlen(WebGPUBlitCube);
    shaderCreateInfo.entrypoint = "blitFromCube";
    renderer->blitFromCubeShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFromCubeShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFromCube pixel shader!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitFromCubeShader, "BlitFromCube");

    char *WebGPUBlitCubeArray = SDL_stack_alloc(char, SDL_strlen(commonCode) + SDL_strlen(blitCubeArrayShader) + 1);
    if (WebGPUBlitCubeArray == NULL) {
        SDL_OutOfMemory();
        return;
    }
    SDL_strlcpy(WebGPUBlitCubeArray, commonCode, SDL_strlen(commonCode) + 1);
    SDL_strlcat(WebGPUBlitCubeArray, blitCubeArrayShader, SDL_strlen(commonCode) + SDL_strlen(blitCubeArrayShader) + 1);
    shaderCreateInfo.code = (Uint8 *)WebGPUBlitCubeArray;
    shaderCreateInfo.code_size = SDL_strlen(WebGPUBlitCubeArray);
    shaderCreateInfo.entrypoint = "blitFromCubeArray";
    renderer->blitFromCubeArrayShader = WebGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitFromCubeArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitFromCubeArray pixel shader!");
    }
    WebGPU_SetShaderLabel((SDL_GPURenderer *)renderer, renderer->blitFromCubeArrayShader, "BlitFromCubeArray");

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

    WebGPUSampler *linearSampler = (WebGPUSampler *)renderer->blitLinearSampler;
    WebGPUSampler *nearestSampler = (WebGPUSampler *)renderer->blitNearestSampler;

    // Set the labels of the samplers
    wgpuSamplerSetLabel(nearestSampler->sampler, "Blit Nearest Sampler");
    wgpuSamplerSetLabel(linearSampler->sampler, "Blit Linear Sampler");

    // We don't need the shader strings
    SDL_stack_free(WebGPUBlit2D);
    SDL_stack_free(WebGPUBlit2DArray);
    SDL_stack_free(WebGPUBlit3D);
    SDL_stack_free(WebGPUBlitCube);
    SDL_stack_free(WebGPUBlitCubeArray);
}

static void WebGPU_INTERNAL_ReleaseBlitPipelines(SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPU_ReleaseSampler(driverData, renderer->blitLinearSampler);
    WebGPU_ReleaseSampler(driverData, renderer->blitNearestSampler);
    WebGPU_ReleaseShader(driverData, renderer->blitVertexShader);
    WebGPU_ReleaseShader(driverData, renderer->blitFrom2DShader);
    WebGPU_ReleaseShader(driverData, renderer->blitFrom2DArrayShader);
    WebGPU_ReleaseShader(driverData, renderer->blitFrom3DShader);
    WebGPU_ReleaseShader(driverData, renderer->blitFromCubeShader);
    WebGPU_ReleaseShader(driverData, renderer->blitFromCubeArrayShader);

    for (Uint32 i = 0; i < renderer->blitPipelineCount; i += 1) {
        WebGPU_ReleaseGraphicsPipeline(driverData, renderer->blitPipelines[i].pipeline);
    }
    SDL_free(renderer->blitPipelines);
}

static void WebGPU_Blit(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUBlitInfo *info)
{
    if (!commandBuffer || !info) {
        return;
    }

    // get renderer from command buffer
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = wgpu_cmd_buf->renderer;

    /*// Create a new command buffer for this blit command*/
    SDL_GPUCommandBuffer *new_cmd_buf = WebGPU_AcquireCommandBuffer((SDL_GPURenderer *)renderer);

    SDL_GPU_BlitCommon(
        new_cmd_buf,
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

    // Submit the command buffer
    WebGPU_Submit(new_cmd_buf);
}

// ---------------------------------------------------

void WebGPU_GenerateMipmaps(SDL_GPUCommandBuffer *commandBuffer,
                            SDL_GPUTexture *texture)
{
    // We will have to implement our own mipmapping pipeline here.
    // I suggest Elie Michel's guide on mipmapping: https://eliemichel.github.io/LearnWebGPU/basic-compute/image-processing/mipmap-generation.html
    SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU mipmapping is not yet implemented");
}

// ---------------------------------------------------

// Debugging Functionality

void WebGPU_InsertDebugLabel(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *text)
{
    if (!commandBuffer || !text) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    wgpuCommandEncoderInsertDebugMarker(wgpu_cmd_buf->commandEncoder, text);
}

void WebGPU_PushDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *text)
{
    if (!commandBuffer || !text) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    wgpuCommandEncoderPushDebugGroup(wgpu_cmd_buf->commandEncoder, text);
}

void WebGPU_PopDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer)
{
    if (!commandBuffer) {
        return;
    }
    WebGPUCommandBuffer *wgpu_cmd_buf = (WebGPUCommandBuffer *)commandBuffer;
    wgpuCommandEncoderPopDebugGroup(wgpu_cmd_buf->commandEncoder);
}

// ---------------------------------------------------

static bool WebGPU_PrepareDriver(SDL_VideoDevice *_this)
{
    // Realistically, we should check if the browser supports WebGPU here and return false if it doesn't
    // For now, we'll just return true because it'll simply crash if the browser doesn't support WebGPU anyways
    return true;
}

static void WebGPU_DestroyDevice(SDL_GPUDevice *device)
{
    WebGPURenderer *renderer = (WebGPURenderer *)device->driverData;

    WebGPU_Wait(device->driverData);

    // Release all blit pipelines
    // This will eventually be removed when proper resource pool caching is implemented
    WebGPU_INTERNAL_ReleaseBlitPipelines((SDL_GPURenderer *)renderer);

    WebGPU_Wait(device->driverData);

    // Destroy all claimed windows
    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        WebGPU_ReleaseWindow(device->driverData, renderer->claimedWindows[i]->window);
    }

    SDL_free(renderer->claimedWindows);

    WebGPU_Wait(device->driverData);

    SDL_free(renderer->submittedCommandBuffers);

    // Destroy uniform buffer pool
    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        WebGPU_ReleaseBuffer(device->driverData, (SDL_GPUBuffer *)renderer->uniformBufferPool[i]->buffer);
        SDL_free(renderer->uniformBufferPool[i]);
    }
    SDL_free(renderer->uniformBufferPool);

    // Destroy the fence pool
    for (Uint32 i = 0; i < renderer->fencePool.availableFenceCount; i += 1) {
        SDL_free(renderer->fencePool.availableFences[i]);
    }
    SDL_free(renderer->fencePool.availableFences);
    SDL_DestroyMutex(renderer->fencePool.lock);

    SDL_DestroyHashTable(renderer->commandPoolHashTable);

    // Destroy all renderer associated locks
    SDL_DestroyMutex(renderer->allocatorLock);
    SDL_DestroyMutex(renderer->disposeLock);
    SDL_DestroyMutex(renderer->submitLock);
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->acquireUniformBufferLock);
    SDL_DestroyMutex(renderer->framebufferFetchLock);
    SDL_DestroyMutex(renderer->windowLock);

    /*// Destroy the device*/
    wgpuDeviceDestroy(renderer->device);
    wgpuInstanceRelease(renderer->instance);
    wgpuAdapterRelease(renderer->adapter);

    // Free the renderer
    SDL_free(renderer);
    SDL_free(device);
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
        .backendType = WGPUBackendType_WebGPU
    };

    // Request adapter using the instance and then the device using the adapter (this is done in the callback)
    wgpuInstanceRequestAdapter(renderer->instance, &adapter_options, WebGPU_RequestAdapterCallback, renderer);

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
    wgpuDeviceSetUncapturedErrorCallback(renderer->device, WebGPU_ErrorCallback, renderer);

    // Acquire the queue from the device
    renderer->queue = wgpuDeviceGetQueue(renderer->device);

    // Get the adapter limits
    wgpuAdapterGetLimits(renderer->adapter, &renderer->physicalDeviceLimits);
    wgpuAdapterGetInfo(renderer->adapter, &renderer->adapterInfo);

    return true;
}

static SDL_GPUDevice *WebGPU_CreateDevice(bool debug, bool preferLowPower, SDL_PropertiesID props)
{
    WebGPURenderer *renderer;
    SDL_GPUDevice *result = NULL;
    Uint32 i;

    // Initialize the WebGPURenderer to be used as the driver data for the SDL_GPUDevice
    renderer = (WebGPURenderer *)SDL_malloc(sizeof(WebGPURenderer));
    SDL_zero(*renderer);
    renderer->debug = debug;
    renderer->preferLowPower = preferLowPower;
    renderer->deviceError = false;

    // This function loops until the device is created
    if (!WebGPU_INTERNAL_CreateWebGPUDevice(renderer)) {
        SDL_free(renderer);
        SDL_SetError("Failed to create WebGPU device");
        return NULL;
    }

    SDL_Log("SDL_GPU Driver: WebGPU");
    SDL_Log("WebGPU Device: %s",
            renderer->adapterInfo.description);

    // Keep track of the minimum uniform buffer alignment
    renderer->minUBOAlignment = renderer->physicalDeviceLimits.limits.minUniformBufferOffsetAlignment;

    // Initialize our SDL_GPUDevice
    result = (SDL_GPUDevice *)SDL_malloc(sizeof(SDL_GPUDevice));

    /*
    TODO: Ensure that all function signatures for the driver are correct so that the following line compiles
          This will attach all of the driver's functions to the SDL_GPUDevice struct

          i.e. result->CreateTexture = WebGPU_CreateTexture;
               result->DestroyDevice = WebGPU_DestroyDevice;
               ... etc.
    */
    /*ASSIGN_DRIVER(WebGPU)*/
    result->driverData = (SDL_GPURenderer *)renderer;
    result->DestroyDevice = WebGPU_DestroyDevice;
    result->ClaimWindow = WebGPU_ClaimWindow;
    result->ReleaseWindow = WebGPU_ReleaseWindow;

    result->AcquireCommandBuffer = WebGPU_AcquireCommandBuffer;
    result->AcquireSwapchainTexture = WebGPU_AcquireSwapchainTexture;
    result->GetSwapchainTextureFormat = WebGPU_GetSwapchainTextureFormat;
    result->SupportsTextureFormat = WebGPU_SupportsTextureFormat;
    result->SupportsSampleCount = WebGPU_SupportsSampleCount;
    result->SupportsPresentMode = WebGPU_SupportsPresentMode;
    result->SupportsSwapchainComposition = WebGPU_SupportsSwapchainComposition;
    result->SetSwapchainParameters = WebGPU_SetSwapchainParameters;

    result->CreateBuffer = WebGPU_CreateGPUBuffer;
    result->ReleaseBuffer = WebGPU_ReleaseBuffer;
    result->SetBufferName = WebGPU_SetBufferName;
    result->CreateTransferBuffer = WebGPU_CreateTransferBuffer;
    result->ReleaseTransferBuffer = WebGPU_ReleaseTransferBuffer;
    result->MapTransferBuffer = WebGPU_MapTransferBuffer;
    result->UnmapTransferBuffer = WebGPU_UnmapTransferBuffer;
    result->UploadToBuffer = WebGPU_UploadToBuffer;
    result->DownloadFromBuffer = WebGPU_DownloadFromBuffer;
    result->CopyBufferToBuffer = WebGPU_CopyBufferToBuffer;

    result->CreateTexture = WebGPU_CreateTexture;
    result->ReleaseTexture = WebGPU_ReleaseTexture;
    result->SetTextureName = WebGPU_SetTextureName;
    result->UploadToTexture = WebGPU_UploadToTexture;
    result->CopyTextureToTexture = WebGPU_CopyTextureToTexture;
    result->DownloadFromTexture = WebGPU_DownloadFromTexture;
    result->Blit = WebGPU_Blit;

    result->CreateSampler = WebGPU_CreateSampler;
    result->ReleaseSampler = WebGPU_ReleaseSampler;
    result->BindFragmentSamplers = WebGPU_BindFragmentSamplers;
    result->PushFragmentUniformData = WebGPU_PushFragmentUniformData;

    result->BindVertexBuffers = WebGPU_BindVertexBuffers;
    result->BindVertexSamplers = WebGPU_BindVertexSamplers;
    result->PushVertexUniformData = WebGPU_PushVertexUniformData;
    result->BindIndexBuffer = WebGPU_BindIndexBuffer;

    result->CreateShader = WebGPU_CreateShader;
    result->ReleaseShader = WebGPU_ReleaseShader;

    result->CreateGraphicsPipeline = WebGPU_CreateGraphicsPipeline;
    result->BindGraphicsPipeline = WebGPU_BindGraphicsPipeline;
    result->ReleaseGraphicsPipeline = WebGPU_ReleaseGraphicsPipeline;
    result->DrawPrimitives = WebGPU_DrawPrimitives;
    result->DrawPrimitivesIndirect = WebGPU_DrawPrimitivesIndirect;
    result->DrawIndexedPrimitives = WebGPU_DrawIndexedPrimitives;
    result->DrawIndexedPrimitivesIndirect = WebGPU_DrawIndexedIndirect;

    result->SetScissor = WebGPU_SetScissorRect;
    result->SetViewport = WebGPU_SetViewport;
    result->SetStencilReference = WebGPU_SetStencilReference;
    result->SetBlendConstants = WebGPU_SetBlendConstants;

    result->GenerateMipmaps = WebGPU_GenerateMipmaps;

    result->Submit = WebGPU_Submit;
    result->SubmitAndAcquireFence = WebGPU_SubmitAndAcquireFence;
    result->Wait = WebGPU_Wait;
    result->WaitForFences = WebGPU_WaitForFences;
    result->Cancel = WebGPU_Cancel;
    result->QueryFence = WebGPU_QueryFence;
    result->ReleaseFence = WebGPU_ReleaseFence;
    result->BeginRenderPass = WebGPU_BeginRenderPass;
    result->EndRenderPass = WebGPU_EndRenderPass;
    result->BeginCopyPass = WebGPU_BeginCopyPass;
    result->EndCopyPass = WebGPU_EndCopyPass;

    result->PushDebugGroup = WebGPU_PushDebugGroup;
    result->PopDebugGroup = WebGPU_PopDebugGroup;
    result->InsertDebugLabel = WebGPU_InsertDebugLabel;

    result->driverData = (SDL_GPURenderer *)renderer;
    renderer->sdlDevice = result;

    // Initialize all of the nessary resources for enabling blitting
    WebGPU_INTERNAL_InitBlitResources(renderer);

    /*
     * Create initial swapchain array
     */

    SDL_Log("Creating initial swapchain array");

    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindowCount = 0;
    renderer->claimedWindows = (WebGPUWindow **)SDL_malloc(renderer->claimedWindowCapacity * sizeof(WebGPUWindow *));

    // Threading
    renderer->allocatorLock = SDL_CreateMutex();
    renderer->disposeLock = SDL_CreateMutex();
    renderer->submitLock = SDL_CreateMutex();
    renderer->acquireCommandBufferLock = SDL_CreateMutex();
    renderer->acquireUniformBufferLock = SDL_CreateMutex();
    renderer->framebufferFetchLock = SDL_CreateMutex();
    renderer->windowLock = SDL_CreateMutex();

    // manually synchronized due to submission timing
    renderer->commandPoolHashTable = SDL_CreateHashTable(
        (void *)renderer,
        64,
        WebGPU_INTERNAL_CommandPoolHashFunction,
        WebGPU_INTERNAL_CommandPoolHashKeyMatch,
        WebGPU_INTERNAL_CommandPoolHashNuke,
        false, false);

    // Initialize our fence pool
    renderer->fencePool.lock = SDL_CreateMutex();
    renderer->fencePool.availableFenceCapacity = 4;
    renderer->fencePool.availableFenceCount = 0;
    renderer->fencePool.availableFences = SDL_malloc(renderer->fencePool.availableFenceCapacity * sizeof(WebGPUFence *));

    // Create submitted command buffer array
    renderer->submittedCommandBufferCapacity = 16;
    renderer->submittedCommandBufferCount = 0;
    renderer->submittedCommandBuffers = SDL_malloc(renderer->submittedCommandBufferCapacity * sizeof(WebGPUCommandBuffer *));

    // Create uniform buffer pool
    renderer->uniformBufferPoolCount = 32;
    renderer->uniformBufferPoolCapacity = 32;
    renderer->uniformBufferPool = SDL_malloc(renderer->uniformBufferPoolCapacity * sizeof(WebGPUUniformBuffer *));

    for (i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i] = WebGPU_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    SDL_Log("WebGPU device created");

    return result;
}

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    SDL_GPU_SHADERFORMAT_WGSL,
    WebGPU_PrepareDriver,
    WebGPU_CreateDevice,
};
