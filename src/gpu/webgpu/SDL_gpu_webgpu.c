// File: /webgpu/SDL_gpu_webgpu.c
// Author: Kyle Lukaszek
// Email: kylelukaszek [at] gmail [dot] com
// License: Zlib
// Description: WebGPU driver for SDL_gpu using the emscripten WebGPU implementation
// Note: Compiling SDL GPU programs using emscripten will require -sUSE_WEBGPU=1 -sASYNCIFY=1

#include "../SDL_sysgpu.h"
#include "SDL_internal.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <spirv_cross_c.h>
#include <stdint.h>
#include <webgpu/webgpu.h>

/* Tint WASM exported functions START */

void tint_initialize(void);
const char *tint_spv_to_wgsl(const void *shader_data, const size_t shader_size);

/* Tint WASM exported functions END */

#define MAX_UBO_SECTION_SIZE          4096 // 4   KiB
#define DESCRIPTOR_POOL_STARTING_SIZE 128
#define WINDOW_PROPERTY_DATA          "SDL_GPUWebGPUWindowPropertyData"

#define EXPAND_ELEMENTS_IF_NEEDED(arr, initialValue, type) \
    if (arr->count == arr->capacity) {                     \
        if (arr->capacity == 0) {                          \
            arr->capacity = initialValue;                  \
        } else {                                           \
            arr->capacity *= 2;                            \
        }                                                  \
        arr->elements = (type *)SDL_realloc(               \
            arr->elements,                                 \
            arr->capacity * sizeof(type));                 \
    }

#define EXPAND_ARRAY_IF_NEEDED(arr, elementType, newCount, capacity, newCapacity) \
    if (newCount >= capacity) {                                                   \
        capacity = newCapacity;                                                   \
        arr = (elementType *)SDL_realloc(                                         \
            arr,                                                                  \
            sizeof(elementType) * capacity);                                      \
    }

#define MOVE_ARRAY_CONTENTS_AND_RESET(i, dstArr, dstCount, srcArr, srcCount) \
    for (i = 0; i < srcCount; i += 1) {                                      \
        dstArr[i] = srcArr[i];                                               \
    }                                                                        \
    dstCount = srcCount;                                                     \
    srcCount = 0;

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

// Enums
typedef enum WebGPUBindingType
{
    WGPUBindingType_Undefined = 0x00000000,
    WGPUBindingType_Buffer = 0x00000001,
    WGPUBindingType_Sampler = 0x00000002,
    WGPUBindingType_Texture = 0x00000003,
    WGPUBindingType_StorageTexture = 0x00000004,

    // Buffer sub-types
    WGPUBindingType_UniformBuffer = 0x00000011,
    WGPUBindingType_StorageBuffer = 0x00000012,
    WGPUBindingType_ReadOnlyStorageBuffer = 0x00000013,

    // Sampler sub-types
    WGPUBindingType_FilteringSampler = 0x00000022,
    WGPUBindingType_NonFilteringSampler = 0x00000023,
    WGPUBindingType_ComparisonSampler = 0x00000024,

    // Texture sub-types
    WGPUBindingType_FilteringTexture = 0x00000033,
    WGPUBindingType_NonFilteringTexture = 0x00000034,

    // Storage Texture sub-types (access modes)
    WGPUBindingType_WriteOnlyStorageTexture = 0x00000044,
    WGPUBindingType_ReadOnlyStorageTexture = 0x00000045,
    WGPUBindingType_ReadWriteStorageTexture = 0x00000046,
} WebGPUBindingType;

static WebGPUBindingType SPIRVToWebGPUBindingType(spvc_resource_type type)
{
    switch (type) {
    case SPVC_RESOURCE_TYPE_UNIFORM_BUFFER:
        return WGPUBindingType_UniformBuffer;
    case SPVC_RESOURCE_TYPE_STORAGE_BUFFER:
        return WGPUBindingType_StorageBuffer;
    case SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS:
        return WGPUBindingType_Sampler;
    case SPVC_RESOURCE_TYPE_SAMPLED_IMAGE:
        return WGPUBindingType_Texture;
    case SPVC_RESOURCE_TYPE_STORAGE_IMAGE:
        return WGPUBindingType_StorageTexture;
    default:
        return WGPUBindingType_Undefined;
    }
}

typedef enum WebGPUBufferType
{
    WEBGPU_BUFFER_TYPE_GPU,
    WEBGPU_BUFFER_TYPE_UNIFORM,
    WEBGPU_BUFFER_TYPE_TRANSFER
} WebGPUBufferType;

// SPIRV-Reflection Definitions:
// ---------------------------------------------------
typedef struct ReflectedBinding
{
    uint32_t set;
    uint32_t binding;
    WebGPUBindingType type;
    WGPUShaderStageFlags stages;
} ReflectedBinding;

typedef struct ReflectedBindGroup
{
    ReflectedBinding *bindings;
    uint32_t bindingCount;
} ReflectedBindGroup;

// WebGPU Structures
// ---------------------------------------------------
typedef struct WebGPUBuffer WebGPUBuffer;
typedef struct WebGPUBufferContainer WebGPUBufferContainer;
typedef struct WebGPUTexture WebGPUTexture;
typedef struct WebGPUTextureContainer WebGPUTextureContainer;

typedef struct WebGPUShader
{
    uint32_t *spirv;
    size_t spirvSize;
    WGPUShaderModule shaderModule;
    const char *entryPointName;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;
    SDL_AtomicInt referenceCount;
} WebGPUShader;

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
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} WebGPURect;

// Buffer structures

#define MAX_BIND_GROUPS 4

typedef struct WebGPUBufferHandle
{
    WebGPUBuffer *webgpuBuffer;
    WebGPUBufferContainer *container;
} WebGPUBufferHandle;

typedef struct WebGPUBuffer
{
    WGPUBuffer buffer;
    uint64_t size;
    WebGPUBufferType type;
    SDL_GPUBufferUsageFlags usageFlags;
    SDL_AtomicInt referenceCount;
    WebGPUBufferHandle *handle;
    bool transitioned;
    Uint8 markedForDestroy;
} WebGPUBuffer;

typedef struct WebGPUBufferContainer
{
    WebGPUBufferHandle *activeBufferHandle;
    Uint32 bufferCapacity;
    Uint32 bufferCount;
    WebGPUBufferHandle **bufferHandles;
    char *debugName;
} WebGPUBufferContainer;

// Bind Group definitions

typedef struct WebGPUBindGroupEntry
{
    WebGPUBindingType type;
    union
    {
        struct
        {
            WebGPUBufferHandle *bufferHandle;
            uint32_t offset;
            uint32_t size;
        } buffer;
        WGPUTextureView textureView;
        WGPUSampler sampler;
    };
} WebGPUBindGroupEntry;

typedef struct WebGPUBindGroup
{
    WGPUBindGroup bindGroup;
    WebGPUBindGroupEntry *entries;
    uint32_t entryCount;
} WebGPUBindGroup;

typedef struct WebGPUBindGroupLayout
{
    WGPUBindGroupLayout layout;
    WGPUShaderStageFlags *stageFlags;
    WebGPUBindingType *types;
    uint32_t entryCount;
} WebGPUBindGroupLayout;

// Graphics Pipeline definitions
typedef struct WebGPUPipelineResourceLayout
{
    WGPUPipelineLayout pipelineLayout;
    WebGPUBindGroupLayout bindGroupLayouts[MAX_BIND_GROUPS];
    uint32_t bindGroupLayoutCount;
} WebGPUPipelineResourceLayout;

typedef struct WebGPUGraphicsPipeline
{
    WGPURenderPipeline pipeline;
    SDL_GPUPrimitiveType primitiveType;
    WebGPUPipelineResourceLayout resourceLayout;
    WebGPUShader *vertexShader;
    WebGPUShader *fragmentShader;
    WGPURenderPipelineDescriptor pipelineDesc;
    SDL_AtomicInt referenceCount;
} WebGPUGraphicsPipeline;

typedef struct WebGPUPipelineResources
{
    WebGPUBindGroup bindGroups[MAX_BIND_GROUPS];
    uint32_t bindGroupCount;
} WebGPUPipelineResources;

// Texture structures

typedef struct WebGPUTextureHandle
{
    WebGPUTexture *webgpuTexture;
    WebGPUTextureContainer *container;
} WebGPUTextureHandle;

typedef struct WebGPUTextureSubresource
{
    WebGPUTexture *parent;
    Uint32 layer;
    Uint32 level;

    WGPUTextureView *renderTargetViews;
    WGPUTextureView computeWriteView;
    WGPUTextureView depthStencilView;

    WebGPUTextureHandle *msaaTexHandle;

    bool transitioned;
} WebGPUTextureSubresource;

struct WebGPUTexture
{
    WGPUTexture texture;
    WGPUTextureView fullView;
    WGPUExtent3D dimensions;

    SDL_GPUTextureType type;
    Uint8 isMSAAColorTarget;

    Uint32 depth;
    Uint32 layerCount;
    Uint32 levelCount;
    WGPUTextureFormat format;
    SDL_GPUTextureUsageFlags usageFlags;

    Uint32 subresourceCount;
    WebGPUTextureSubresource *subresources;

    WebGPUTextureHandle *handle;

    Uint8 markedForDestroy;
    SDL_AtomicInt referenceCount;
};

struct WebGPUTextureContainer
{
    TextureCommonHeader header;

    WebGPUTextureHandle *activeTextureHandle;

    Uint32 textureCapacity;
    Uint32 textureCount;
    WebGPUTextureHandle **textureHandles;

    Uint8 canBeCycled;

    char *debugName;
};

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
    WGPUSwapChain swapchain;
    WGPUTextureFormat format;
    WGPUPresentMode presentMode;

    WGPUTexture depthStencilTexture;
    WGPUTextureView depthStencilView;

    WGPUTexture msaaTexture;
    WGPUTextureView msaaView;

    uint32_t width;
    uint32_t height;
    uint32_t sampleCount;

    uint32_t frameCounter;
} WebGPUSwapchainData;

typedef struct WindowData
{
    SDL_Window *window;
    SDL_GPUSwapchainComposition swapchainComposition;
    SDL_GPUPresentMode presentMode;
    WebGPUSwapchainData *swapchainData;
    bool needsSwapchainRecreate;
} WindowData;

// Renderer Structure
typedef struct WebGPURenderer WebGPURenderer;

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

    WebGPUPipelineResources currentResources;
    bool resourcesDirty;

    WebGPUViewport currentViewport;
    WebGPURect currentScissor;

    WebGPUGraphicsPipeline **usedGraphicsPipelines;
    Uint32 usedGraphicsPipelineCount;
    Uint32 usedGraphicsPipelineCapacity;

    // ... (other fields as needed)

} WebGPUCommandBuffer;

typedef struct WebGPURenderer
{
    bool debugMode;
    bool preferLowPower;

    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    WindowData **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;
} WebGPURenderer;

// ---------------------------------------------------

// Conversion Functions:
// ---------------------------------------------------

static WGPULoadOp SDLToWGPULoadOp(SDL_GPULoadOp loadOp)
{
    switch (loadOp) {
    case SDL_GPU_LOADOP_LOAD:
        return WGPULoadOp_Load;
    case SDL_GPU_LOADOP_CLEAR:
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
        return WGPUCompareFunction_Never;
    }
}

static SDL_GPUTextureFormat SwapchainCompositionToSDLFormat(
    SDL_GPUSwapchainComposition composition,
    bool usingFallback)
{
    switch (composition) {
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    default:
        return SDL_GPU_TEXTUREFORMAT_INVALID;
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

// ---------------------------------------------------

// SPIRV-Reflection Functions:
// ---------------------------------------------------
static void ReflectSPIRVShaderBindings_INTERNAL(spvc_compiler *compiler,
                                                spvc_resources *resources,
                                                spvc_resource_type type,
                                                const spvc_reflected_resource *reflectedResources,
                                                ReflectedBindGroup *bindGroups,
                                                uint32_t *maxSet,
                                                WGPUShaderStageFlags stageFlags)
{
    if (compiler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid SPIRV compiler.");
        return;
    }

    if (resources == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid SPIRV resources.");
        return;
    }

    if (maxSet == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid max set pointer.");
        return;
    }

    size_t reflectedCount;

    // Get the resources for the specified type
    spvc_resources_get_resource_list_for_type(*resources, type, &reflectedResources, &reflectedCount);

    if (reflectedCount == 0) {
        return;
    }

    SDL_Log("Reflected %zu resource(s) of type %d", reflectedCount, type);

    for (size_t i = 0; i < reflectedCount; i += 1) {
        // Get the descriptor set and binding
        uint32_t set = spvc_compiler_get_decoration(*compiler, reflectedResources[i].id, SpvDecorationDescriptorSet);
        uint32_t binding = spvc_compiler_get_decoration(*compiler, reflectedResources[i].id, SpvDecorationBinding);

        // Update the max set value within the pointer
        if (set > *maxSet) {
            *maxSet = set;
        }

        // Calloc enough memory for the bindgroups if it hasn't been allocated yet
        // The individual bindgroups are resized as needed
        if (!bindGroups) {
            bindGroups = SDL_calloc((size_t)set + 1, sizeof(ReflectedBindGroup));
            bindGroups[0].bindingCount = 0;
        }

        ReflectedBinding *newBinding = SDL_realloc(bindGroups[set].bindings,
                                                   (bindGroups[set].bindingCount + 1) * sizeof(ReflectedBinding));
        if (!newBinding) {
            SDL_free(bindGroups);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate memory for reflected bindings.");
            return;
        }

        bindGroups[set].bindings = newBinding;
        bindGroups[set].bindings[bindGroups[set].bindingCount] = (ReflectedBinding){
            .set = set,
            .binding = binding,
            .type = SPIRVToWebGPUBindingType(type),
            .stages = stageFlags,
        };
        bindGroups[set].bindingCount += 1;
    }

    SDL_free((void *)reflectedResources);
}

static ReflectedBindGroup *ReflectSPIRVShaderBindings(
    const uint32_t *spirv,
    size_t spirvSize,
    WGPUShaderStageFlags stageFlags,
    uint32_t *outBindGroupCount)
{
    spvc_context context;
    spvc_parsed_ir ir;
    spvc_compiler compiler;
    spvc_resources resources;

    const spvc_reflected_resource *reflectedResources = NULL;

    // Initialize SPIRV-Cross
    spvc_context_create(&context);
    spvc_context_parse_spirv(context, spirv, spirvSize / sizeof(uint32_t), &ir);
    spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

    // Create spirv shader resources
    spvc_compiler_create_shader_resources(compiler, &resources);
    if (!resources) {
        spvc_context_destroy(context);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create shader resources.");
        return NULL;
    }

    // Reflected bind groups
    ReflectedBindGroup *bindGroups = NULL;
    uint32_t maxSet = 0;

    // Reflect gl variables
    SDL_Log("Reflecting gl uniforms...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_GL_PLAIN_UNIFORM,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect Vertex attributes
    SDL_Log("Reflecting vertex attributes...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_STAGE_INPUT,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect uniform buffers
    SDL_Log("Reflecting uniform buffers...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect storage buffers
    SDL_Log("Reflecting storage buffers...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_STORAGE_BUFFER,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Store textures
    SDL_Log("Reflecting textures...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect storage images
    SDL_Log("Reflecting storage textures...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_STORAGE_IMAGE,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect sampled images
    SDL_Log("Reflecting sampled texture...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    // Reflect samplers
    SDL_Log("Reflecting reflecting samplers...");
    ReflectSPIRVShaderBindings_INTERNAL(&compiler, &resources,
                                        SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,
                                        reflectedResources, bindGroups, &maxSet, stageFlags);

    SDL_Log("Max set: %d", maxSet);

    // Free resources
    spvc_context_destroy(context);
    SDL_free((void *)reflectedResources);

    // Set the output bind group count
    *outBindGroupCount = maxSet;
    return bindGroups;
}

static void WebGPU_CreateBindingLayout(WGPUBindGroupLayoutEntry *entry, ReflectedBinding *binding)
{
    switch (binding->type) {
    case WGPUBindingType_Buffer:
    case WGPUBindingType_UniformBuffer:
        entry->buffer = (WGPUBufferBindingLayout){
            .type = WGPUBufferBindingType_Uniform,
            .hasDynamicOffset = false,
            .minBindingSize = 0,
        };
        break;
    case WGPUBindingType_StorageBuffer:
        entry->buffer = (WGPUBufferBindingLayout){
            .type = WGPUBufferBindingType_Storage,
            .hasDynamicOffset = false,
            .minBindingSize = 0,
        };
        break;
    case WGPUBindingType_ReadOnlyStorageBuffer:
        entry->buffer = (WGPUBufferBindingLayout){
            .type = WGPUBufferBindingType_ReadOnlyStorage,
            .hasDynamicOffset = false,
            .minBindingSize = 0,
        };
        break;
    case WGPUBindingType_Texture:
    case WGPUBindingType_FilteringTexture:
    case WGPUBindingType_NonFilteringTexture:
        entry->texture = (WGPUTextureBindingLayout){
            .sampleType = WGPUTextureSampleType_Float,
            .viewDimension = WGPUTextureViewDimension_2D,
            .multisampled = false,
        };
        break;
    case WGPUBindingType_StorageTexture:
    case WGPUBindingType_WriteOnlyStorageTexture:
        entry->storageTexture = (WGPUStorageTextureBindingLayout){
            .access = WGPUStorageTextureAccess_WriteOnly,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .viewDimension = WGPUTextureViewDimension_2D,
        };
        break;
    case WGPUBindingType_ReadOnlyStorageTexture:
        entry->storageTexture = (WGPUStorageTextureBindingLayout){
            .access = WGPUStorageTextureAccess_ReadOnly,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .viewDimension = WGPUTextureViewDimension_2D,
        };
        break;
    case WGPUBindingType_ReadWriteStorageTexture:
        entry->storageTexture = (WGPUStorageTextureBindingLayout){
            .access = WGPUStorageTextureAccess_ReadWrite,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .viewDimension = WGPUTextureViewDimension_2D,
        };
        break;
    case WGPUBindingType_Sampler:
    case WGPUBindingType_FilteringSampler:
        entry->sampler = (WGPUSamplerBindingLayout){
            .type = WGPUSamplerBindingType_Filtering,
        };
        break;
    case WGPUBindingType_NonFilteringSampler:
        entry->sampler = (WGPUSamplerBindingLayout){
            .type = WGPUSamplerBindingType_NonFiltering,
        };
        break;
    case WGPUBindingType_ComparisonSampler:
        entry->sampler = (WGPUSamplerBindingLayout){
            .type = WGPUSamplerBindingType_Comparison,
        };
        break;
    default:
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Invalid binding type %d", binding->type);
        break;
    };
}

static WebGPUPipelineResourceLayout *CreateResourceLayoutFromReflection(
    WebGPURenderer *renderer,
    ReflectedBindGroup *vertexBindGroups,
    uint32_t vertexBindGroupCount,
    ReflectedBindGroup *fragmentBindGroups,
    uint32_t fragmentBindGroupCount)
{
    WebGPUPipelineResourceLayout *layout = SDL_malloc(sizeof(WebGPUPipelineResourceLayout));
    layout->bindGroupLayoutCount = SDL_max(vertexBindGroupCount, fragmentBindGroupCount);

    WGPUBindGroupLayoutEntry *entries = NULL;

    SDL_Log("Vertex bind group count: %d", vertexBindGroupCount);
    SDL_Log("Fragment bind group count: %d", fragmentBindGroupCount);

    for (uint32_t i = 0; i < layout->bindGroupLayoutCount; i++) {
        SDL_Log("Creating bind group layout %d", i);
        uint32_t entryCount = 0;
        // Combine vertex and fragment bindings
        if (i < vertexBindGroupCount) {
            SDL_Log("Processing vertex bindgroup %d, with %u bindings", i, vertexBindGroups[i].bindingCount);
            for (uint32_t j = 0; j < vertexBindGroups[i].bindingCount; j++) {
                ReflectedBindGroup *bindGroup = &vertexBindGroups[i];
                SDL_Log("Retrieving binding %d", j);
                ReflectedBinding *binding = &bindGroup->bindings[j];
                SDL_Log("Binding %d, set %d, binding %d, type %d, stages %d", j, binding->set, binding->binding, binding->type, binding->stages);
                entries = SDL_realloc(entries, (entryCount + 1) * sizeof(WGPUBindGroupLayoutEntry));
                entries[entryCount] = (WGPUBindGroupLayoutEntry){
                    .binding = binding->binding,
                    .visibility = binding->stages,
                };

                // Assign the correct binding layout type to the BindingLayoutEntry
                // This can be a buffer, texture, sampler, etc.
                WebGPU_CreateBindingLayout(&entries[entryCount], binding);

                entryCount++;
            }
        }

        if (i < fragmentBindGroupCount) {
            for (uint32_t j = 0; j < fragmentBindGroups[i].bindingCount; j++) {
                ReflectedBinding *binding = &fragmentBindGroups[i].bindings[j];
                // Check if this binding already exists from vertex stage
                bool found = false;
                for (uint32_t k = 0; k < entryCount; k++) {
                    if (entries[k].binding == binding->binding) {
                        entries[k].visibility |= binding->stages;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    entries = SDL_realloc(entries, (entryCount + 1) * sizeof(WGPUBindGroupLayoutEntry));
                    entries[entryCount] = (WGPUBindGroupLayoutEntry){
                        .binding = binding->binding,
                        .visibility = binding->stages,
                    };

                    // Assign the correct binding layout type to the BindingLayoutEntry
                    // This can be a buffer, texture, sampler, etc.
                    WebGPU_CreateBindingLayout(&entries[entryCount], binding);

                    entryCount++;
                }
            }
        }

        WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
            .entryCount = entryCount,
            .entries = entries
        };

        layout->bindGroupLayouts[i].layout = wgpuDeviceCreateBindGroupLayout(renderer->device, &bindGroupLayoutDesc);
        layout->bindGroupLayouts[i].entryCount = entryCount;
        /*layout->bindGroupLayouts[i].types = SDL_malloc(entryCount * sizeof(WebGPUBindingType));*/
        layout->bindGroupLayouts[i].stageFlags = SDL_malloc(entryCount * sizeof(WGPUShaderStageFlags));

        for (uint32_t j = 0; j < entryCount; j++) {
            /*layout->bindGroupLayouts[i].types[j] = entries[j].type;*/
            layout->bindGroupLayouts[i].stageFlags[j] = entries[j].visibility;
        }

        SDL_free(entries);
    }

    return layout;
}

// WebGPU Functions:
// ---------------------------------------------------

// Simple Error Callback for WebGPU
static void WebGPU_ErrorCallback(WGPUErrorType type, const char *message, void *userdata)
{
    SDL_SetError("WebGPU error: %s", message);
}

// Device Request Callback for when the device is requested from the adapter
static void WebGPU_RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, const char *message, void *userdata)
{
    WebGPURenderer *renderer = (WebGPURenderer *)userdata;
    if (status == WGPURequestDeviceStatus_Success) {
        renderer->device = device;
        SDL_Log("WebGPU device requested successfully");
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to request WebGPU device: %s", message);
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
        SDL_Log("WebGPU adapter requested successfully");

        // Request device from adapter
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

// Fetch the necessary PropertiesID for the WindowData for a browser window
static WindowData *WebGPU_INTERNAL_FetchWindowData(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (WindowData *)SDL_GetPointerProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

// Callback for when the window is resized
static SDL_bool WebGPU_INTERNAL_OnWindowResize(void *userdata, SDL_Event *event)
{
    SDL_Window *window = (SDL_Window *)userdata;
    // Event watchers will pass any event, but we only care about window resize events
    if (event->type != SDL_EVENT_WINDOW_RESIZED) {
        return false;
    }

    WindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    if (windowData) {
        windowData->needsSwapchainRecreate = true;
    }

    SDL_Log("Window resized, recreating swapchain");

    return true;
}

SDL_GPUCommandBuffer *WebGPU_AcquireCommandBuffer(SDL_GPURenderer *driverData)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUCommandBuffer *commandBuffer = SDL_malloc(sizeof(WebGPUCommandBuffer));
    memset(commandBuffer, '\0', sizeof(WebGPUCommandBuffer));
    commandBuffer->renderer = renderer;

    WGPUCommandEncoderDescriptor commandEncoderDesc = {
        .label = "SDL_GPU Command Encoder",
    };

    commandBuffer->commandEncoder = wgpuDeviceCreateCommandEncoder(renderer->device, &commandEncoderDesc);

    return (SDL_GPUCommandBuffer *)commandBuffer;
}

void WebGPU_Submit(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;

    WGPUCommandBufferDescriptor commandBufferDesc = {
        .label = "SDL_GPU Command Buffer",
    };

    WGPUCommandBuffer commandHandle = wgpuCommandEncoderFinish(webgpuCommandBuffer->commandEncoder, &commandBufferDesc);
    if (!commandHandle) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to finish command buffer");
        return;
    }
    wgpuQueueSubmit(renderer->queue, 1, &commandHandle);

    // Release the actual command buffer followed by the SDL command buffer
    wgpuCommandBufferRelease(commandHandle);
    wgpuCommandEncoderRelease(webgpuCommandBuffer->commandEncoder);
    SDL_free(webgpuCommandBuffer);
}

void WebGPU_BeginRenderPass(SDL_GPUCommandBuffer *commandBuffer,
                            SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
                            Uint32 colorAttachmentCount,
                            SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUTexture *texture = NULL;

    if (colorAttachmentCount == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No color attachments provided for render pass");
        return;
    }

    // Build WGPU color attachments from the SDL color attachment structs
    WGPURenderPassColorAttachment *colorAttachments = SDL_malloc(sizeof(WGPURenderPassColorAttachment) * colorAttachmentCount);
    for (Uint32 i = 0; i < colorAttachmentCount; i += 1) {
        texture = (WebGPUTexture *)colorAttachmentInfos[i].texture;
        colorAttachments[i] = (WGPURenderPassColorAttachment){
            .view = texture->fullView,
            .depthSlice = ~0u,
            .loadOp = SDLToWGPULoadOp(colorAttachmentInfos[i].loadOp),
            .storeOp = SDLToWGPUStoreOp(colorAttachmentInfos[i].storeOp),
            .clearValue = (WGPUColor){
                .r = colorAttachmentInfos[i].clearColor.r,
                .g = colorAttachmentInfos[i].clearColor.g,
                .b = colorAttachmentInfos[i].clearColor.b,
                .a = colorAttachmentInfos[i].clearColor.a,
            },
        };

        // If we have an MSAA texture, we need to make sure the resolve target is not NULL
        if (texture->isMSAAColorTarget) {
            colorAttachments[i].resolveTarget = texture->fullView;
        }
    }

    // Set color attachments for the render pass
    WGPURenderPassDescriptor renderPassDesc = {
        .colorAttachmentCount = colorAttachmentCount,
        .colorAttachments = colorAttachments,
    };

    // Set depth stencil attachment if provided
    if (depthStencilAttachmentInfo != NULL) {
        // Get depth texture as WebGPUTexture
        texture = (WebGPUTexture *)depthStencilAttachmentInfo->texture;
        WGPURenderPassDepthStencilAttachment depthStencilAttachment = {
            .view = texture->fullView,
            .depthLoadOp = SDLToWGPULoadOp(depthStencilAttachmentInfo->stencilLoadOp),
            .depthStoreOp = SDLToWGPUStoreOp(depthStencilAttachmentInfo->stencilStoreOp),
            .depthClearValue = depthStencilAttachmentInfo->depthStencilClearValue.depth,
            .stencilLoadOp = SDLToWGPULoadOp(depthStencilAttachmentInfo->stencilLoadOp),
            .stencilStoreOp = SDLToWGPUStoreOp(depthStencilAttachmentInfo->stencilStoreOp),
            .stencilClearValue = depthStencilAttachmentInfo->depthStencilClearValue.stencil,
        };

        renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    }

    // Begin the render pass
    webgpuCommandBuffer->renderPassEncoder = wgpuCommandEncoderBeginRenderPass(webgpuCommandBuffer->commandEncoder, &renderPassDesc);
}

void WebGPU_EndRenderPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    wgpuRenderPassEncoderEnd(webgpuCommandBuffer->renderPassEncoder);
    wgpuRenderPassEncoderRelease(webgpuCommandBuffer->renderPassEncoder);
}

bool WebGPU_INTERNAL_CreateSurface(WebGPURenderer *renderer, WindowData *windowData)
{
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
    };
    windowData->swapchainData->surface = wgpuInstanceCreateSurface(renderer->instance, &surf_desc);
    return windowData->swapchainData->surface != NULL;
}

static void WebGPU_CreateSwapchain(WebGPURenderer *renderer, WindowData *windowData)
{
    windowData->swapchainData = SDL_calloc(1, sizeof(WebGPUSwapchainData));

    SDL_assert(WebGPU_INTERNAL_CreateSurface(renderer, windowData));

    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {
        .chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
        .selector = "#canvas",
    };
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
    };
    windowData->swapchainData->surface = wgpuInstanceCreateSurface(renderer->instance, &surf_desc);

    WebGPUSwapchainData *swapchainData = windowData->swapchainData;
    swapchainData->format = wgpuSurfaceGetPreferredFormat(swapchainData->surface, renderer->adapter);
    swapchainData->presentMode = SDLToWGPUPresentMode(windowData->presentMode);

    // Swapchain should be the size of whatever SDL_Window it is attached to
    swapchainData->width = windowData->window->w;
    swapchainData->height = windowData->window->h;

    // Emscripten WebGPU swapchain
    WGPUSwapChainDescriptor swapchainDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapchainData->format,
        .width = swapchainData->width,
        .height = swapchainData->height,
        .presentMode = swapchainData->presentMode,
    };

    swapchainData->swapchain = wgpuDeviceCreateSwapChain(renderer->device, swapchainData->surface, &swapchainDesc);

    // Depth/stencil texture for swapchain
    WGPUTextureDescriptor depthDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = swapchainData->width,
            .height = swapchainData->height,
            .depthOrArrayLayers = 1,
        },
        .format = WGPUTextureFormat_Depth32FloatStencil8,
        .mipLevelCount = 1,
        .sampleCount = swapchainData->sampleCount,
    };
    swapchainData->depthStencilTexture = wgpuDeviceCreateTexture(renderer->device, &depthDesc);
    swapchainData->depthStencilView = wgpuTextureCreateView(swapchainData->depthStencilTexture, NULL);

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
        };
        swapchainData->msaaTexture = wgpuDeviceCreateTexture(renderer->device, &msaaDesc);
        swapchainData->msaaView = wgpuTextureCreateView(swapchainData->msaaTexture, NULL);
    }

    SDL_Log("WebGPU: Created swapchain %p of size %dx%d", swapchainData->swapchain, swapchainData->width, swapchainData->height);
}

static SDL_GPUTextureFormat WebGPU_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    WebGPUSwapchainData *swapchainData = windowData->swapchainData;

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
    if (swapchainData->swapchain) {
        wgpuSwapChainRelease(swapchainData->swapchain);
        swapchainData->swapchain = NULL;
    }
    if (swapchainData->surface) {
        wgpuSurfaceRelease(swapchainData->surface);
        swapchainData->surface = NULL;
    }
}

static void WebGPU_RecreateSwapchain(WebGPURenderer *renderer, WindowData *windowData)
{
    WebGPU_DestroySwapchain(windowData->swapchainData);
    WebGPU_CreateSwapchain(renderer, windowData);
    windowData->needsSwapchainRecreate = false;
}

static SDL_GPUTexture *WebGPU_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = webgpuCommandBuffer->renderer;
    WindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);
    WebGPUSwapchainData *swapchainData = windowData->swapchainData;

    // Check if the swapchain needs to be recreated
    if (windowData->needsSwapchainRecreate) {
        WebGPU_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_SetError("Failed to recreate swapchain");
            return NULL;
        }
    }

    // Get the current texture view from the swapchain
    WGPUTextureView currentView = wgpuSwapChainGetCurrentTextureView(swapchainData->swapchain);
    if (currentView == NULL) {
        SDL_SetError("Failed to acquire texture view from swapchain");
        return NULL;
    }

    // Create a temporary WebGPUTexture to return
    WebGPUTexture *texture = SDL_calloc(1, sizeof(WebGPUTexture));
    if (!texture) {
        SDL_OutOfMemory();
        return NULL;
    }

    texture->texture = wgpuSwapChainGetCurrentTexture(swapchainData->swapchain);
    texture->fullView = currentView;
    texture->dimensions = (WGPUExtent3D){
        .width = swapchainData->width,
        .height = swapchainData->height,
        .depthOrArrayLayers = 1,
    };
    texture->type = SDL_GPU_TEXTURETYPE_2D;
    texture->isMSAAColorTarget = swapchainData->sampleCount > 1;
    texture->format = swapchainData->format;
    texture->usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    // For MSAA, we'll return the MSAA texture instead of the swapchain texture
    if (swapchainData->sampleCount > 1) {
        texture->texture = swapchainData->msaaTexture;
        texture->fullView = swapchainData->msaaView;
    }

    // Set the width and height if provided
    if (pWidth) {
        *pWidth = swapchainData->width;
    }
    if (pHeight) {
        *pHeight = swapchainData->height;
    }

    // It is important to release these textures when they are no longer needed
    return (SDL_GPUTexture *)texture;
}

static bool WebGPU_ClaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = SDL_malloc(sizeof(WindowData));
        windowData->window = window;
        windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

        WebGPU_CreateSwapchain(renderer, windowData);

        if (windowData->swapchainData != NULL) {
            SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(WindowData *));
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

    WindowData *windowData = WebGPU_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        return;
    }

    // Destroy the swapchain
    if (windowData->swapchainData) {
        WebGPU_DestroySwapchain(windowData->swapchainData);
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

static SDL_GPUShader *WebGPU_CreateShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShaderCreateInfo *shaderCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a shader");
    SDL_assert(shaderCreateInfo && "Shader create info must not be NULL when creating a shader");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUShader *shader = SDL_calloc(1, sizeof(WebGPUShader));

    const char *wgsl = tint_spv_to_wgsl(shaderCreateInfo->code, shaderCreateInfo->codeSize);

    printf("WGSL: %s", wgsl);

    SDL_free((void *)wgsl);

    WGPUShaderModuleSPIRVDescriptor spirv_desc = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
        },
        .code = (uint32_t *)shaderCreateInfo->code,
        .codeSize = shaderCreateInfo->codeSize,
    };

    WGPUShaderModuleDescriptor shader_desc = {
        .nextInChain = (WGPUChainedStruct *)&spirv_desc,
        .label = "SDL_GPU WebGPU SPIRV Shader",
    };

    // Create a WebGPUShader object to cast to SDL_GPUShader *
    uint32_t entryPointNameLength = SDL_strlen(shaderCreateInfo->entryPointName) + 1;
    shader->spirv = SDL_malloc(shaderCreateInfo->codeSize);
    SDL_memcpy(shader->spirv, shaderCreateInfo->code, shaderCreateInfo->codeSize);
    shader->spirvSize = shaderCreateInfo->codeSize;
    shader->entryPointName = SDL_malloc(entryPointNameLength);
    SDL_utf8strlcpy((char *)shader->entryPointName, shaderCreateInfo->entryPointName, entryPointNameLength);
    shader->samplerCount = shaderCreateInfo->samplerCount;
    shader->storageBufferCount = shaderCreateInfo->storageBufferCount;
    shader->uniformBufferCount = shaderCreateInfo->uniformBufferCount;
    shader->shaderModule = wgpuDeviceCreateShaderModule(renderer->device, &shader_desc);

    SDL_Log("Shader Created Successfully:");
    SDL_Log("entry: %s\n", shader->entryPointName);
    SDL_Log("sampler count: %u\n", shader->samplerCount);
    SDL_Log("storageBufferCount: %u\n", shader->storageBufferCount);
    SDL_Log("uniformBufferCount: %u\n", shader->uniformBufferCount);

    // Set our shader referenceCount to 0 at creation
    SDL_AtomicSet(&shader->referenceCount, 0);

    return (SDL_GPUShader *)shader;
}

static void WebGPU_ReleaseShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader)
{
    SDL_assert(driverData && "Driver data must not be NULL when destroying a shader");
    SDL_assert(shader && "Shader must not be NULL when destroying a shader");

    WebGPUShader *wgpuShader = (WebGPUShader *)shader;

    // Free entry function string
    SDL_free((void *)wgpuShader->entryPointName);
    SDL_free((void *)wgpuShader->spirv);

    // Release the shader module
    wgpuShaderModuleRelease(wgpuShader->shaderModule);

    SDL_free(shader);
}

static void WebGPU_INTERNAL_TrackGraphicsPipeline(WebGPUCommandBuffer *commandBuffer,
                                                  WebGPUGraphicsPipeline *graphicsPipeline)
{
    TRACK_RESOURCE(
        graphicsPipeline,
        WebGPUGraphicsPipeline *,
        usedGraphicsPipelines,
        usedGraphicsPipelineCount,
        usedGraphicsPipelineCapacity)
}

static SDL_GPUGraphicsPipeline *WebGPU_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUGraphicsPipelineCreateInfo *pipelineCreateInfo)
{
    SDL_assert(driverData && "Driver data must not be NULL when creating a graphics pipeline");
    SDL_assert(pipelineCreateInfo && "Pipeline create info must not be NULL when creating a graphics pipeline");

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUGraphicsPipeline *pipeline = SDL_calloc(1, sizeof(WebGPUGraphicsPipeline));
    if (!pipeline) {
        SDL_OutOfMemory();
        return NULL;
    }

    WebGPUShader *vertShader = (WebGPUShader *)pipelineCreateInfo->vertexShader;
    WebGPUShader *fragShader = (WebGPUShader *)pipelineCreateInfo->fragmentShader;

    // Get the bind groups for the vertex and fragment shaders using SPIRV's parser and compiler.
    // We do this because WebGPU requires us to create bind group layouts for the pipeline layout even
    // though bind groups aren't a thing in SPIRV, they use sets and bindings instead from my understanding.
    uint32_t vertexBindGroupCount = 0;
    uint32_t fragmentBindGroupCount = 0;
    ReflectedBindGroup *vertexBindGroups = ReflectSPIRVShaderBindings(vertShader->spirv,
                                                                      vertShader->spirvSize,
                                                                      WGPUShaderStage_Vertex, &vertexBindGroupCount);
    ReflectedBindGroup *fragmentBindGroups = ReflectSPIRVShaderBindings(fragShader->spirv,
                                                                        fragShader->spirvSize,
                                                                        WGPUShaderStage_Fragment, &fragmentBindGroupCount);

    WebGPUPipelineResourceLayout *resourceLayout = CreateResourceLayoutFromReflection(renderer,
                                                                                      vertexBindGroups,
                                                                                      vertexBindGroupCount,
                                                                                      fragmentBindGroups,
                                                                                      fragmentBindGroupCount);

    /*wgpuDeviceSetUncapturedErrorCallback(renderer->device, WebGPU_ErrorCallback, renderer);*/

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor layoutDesc = {
        .label = "SDL_GPU WebGPU Pipeline Layout",
        .bindGroupLayoutCount = resourceLayout->bindGroupLayoutCount,
        .bindGroupLayouts = SDL_malloc(sizeof(WGPUBindGroupLayout) * resourceLayout->bindGroupLayoutCount),
    };

    // Assign the bind group layouts to the pipeline layout
    for (uint32_t i = 0; i < resourceLayout->bindGroupLayoutCount; i++) {
        SDL_memcpy(layoutDesc.bindGroupLayouts[i], &resourceLayout->bindGroupLayouts[i].layout, sizeof(WGPUBindGroupLayout));
    }

    // Create the pipeline layout from the descriptor
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(renderer->device, &layoutDesc);

    resourceLayout->pipelineLayout = pipelineLayout;

    // Create the vertex state for the render pipeline
    WGPUVertexState vertexState = {
        .module = wgpuDeviceCreateShaderModule(renderer->device, &(WGPUShaderModuleDescriptor){
                                                                     .label = "SDL_GPU WebGPU Vertex Shader",
                                                                     .nextInChain = (void *)&(WGPUShaderModuleSPIRVDescriptor){
                                                                         .chain = {
                                                                             .next = NULL,
                                                                             .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
                                                                         },
                                                                         .code = vertShader->spirv,
                                                                         .codeSize = vertShader->spirvSize,
                                                                     },
                                                                 }),
        .entryPoint = "main",
        .bufferCount = pipelineCreateInfo->vertexInputState.vertexBindingCount,
        .buffers = NULL, // TODO: Create an array of WGPUVertexBufferLayout for each vertex binding.
        .constantCount = pipelineCreateInfo->vertexInputState.vertexAttributeCount,
        .constants = NULL, // TODO: Create an array of WGPUConstantEntries for each vertex attribute.
    };

    // Build the color targets for the render pipeline
    WGPUColorTargetState *colorTargets = SDL_malloc(sizeof(WGPUColorTargetState) * pipelineCreateInfo->attachmentInfo.colorAttachmentCount);
    for (Uint32 i = 0; i < pipelineCreateInfo->attachmentInfo.colorAttachmentCount; i += 1) {
        SDL_GPUColorAttachmentDescription *colorAttachment = &pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i];
        SDL_GPUColorAttachmentBlendState blendState = colorAttachment->blendState;
        colorTargets[i] = (WGPUColorTargetState){
            .format = SDLToWGPUTextureFormat(colorAttachment->format),
            .blend = &(WGPUBlendState){
                .color = {
                    .srcFactor = SDLToWGPUBlendFactor(blendState.srcColorBlendFactor),
                    .dstFactor = SDLToWGPUBlendFactor(blendState.dstColorBlendFactor),
                    .operation = SDLToWGPUBlendOperation(blendState.colorBlendOp),
                },
                .alpha = {
                    .srcFactor = SDLToWGPUBlendFactor(blendState.srcAlphaBlendFactor),
                    .dstFactor = SDLToWGPUBlendFactor(blendState.dstAlphaBlendFactor),
                    .operation = SDLToWGPUBlendOperation(blendState.alphaBlendOp),
                },
            },
            .writeMask = SDLToWGPUColorWriteMask(blendState.colorWriteMask),
        };
    }

    // Create the fragment state for the render pipeline
    WGPUFragmentState fragmentState = {
        .module = fragShader->shaderModule,
        .entryPoint = "main",
        .constantCount = 0,
        .constants = NULL,
        .targetCount = pipelineCreateInfo->attachmentInfo.colorAttachmentCount,
        .targets = colorTargets,
    };

    WGPUDepthStencilState depthStencil;
    if (pipelineCreateInfo->attachmentInfo.hasDepthStencilAttachment) {
        SDL_GPUDepthStencilState *state = &pipelineCreateInfo->depthStencilState;

        depthStencil.format = SDLToWGPUTextureFormat(pipelineCreateInfo->attachmentInfo.depthStencilFormat);
        depthStencil.depthWriteEnabled = state->depthWriteEnable;
        depthStencil.depthCompare = SDLToWGPUCompareFunction(state->compareOp);

        // No read mask is provided with SDL_GPU and this isn't too big of an issue
        // since we can just set it to 0xFF and move on with our lives
        depthStencil.stencilReadMask = 0xFF;
        depthStencil.stencilWriteMask = state->writeMask;
    }

    // Create the render pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc = {
        .label = "SDL_GPU WebGPU Render Pipeline",
        .layout = pipelineLayout,
        .vertex = vertexState,
        .primitive = {
            .topology = SDLToWGPUPrimitiveTopology(pipelineCreateInfo->primitiveType),
            .stripIndexFormat = SDL_GPU_INDEXELEMENTSIZE_32BIT ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16,
            .frontFace = SDLToWGPUFrontFace(pipelineCreateInfo->rasterizerState.frontFace),
            .cullMode = SDLToWGPUCullMode(pipelineCreateInfo->rasterizerState.cullMode),
        },
        // Needs to be set up
        .depthStencil = pipelineCreateInfo->attachmentInfo.hasDepthStencilAttachment ? &depthStencil : NULL,
        .multisample = {
            .count = pipelineCreateInfo->multisampleState.sampleCount,
            .mask = pipelineCreateInfo->multisampleState.sampleMask,
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
    pipeline->primitiveType = pipelineCreateInfo->primitiveType;

    SDL_AtomicSet(&pipeline->referenceCount, 1);

    // Clean up
    SDL_free(colorTargets);
    SDL_free(((void *)layoutDesc.bindGroupLayouts));
    wgpuPipelineLayoutRelease(pipelineLayout);
    SDL_free(resourceLayout);

    // Free reflected bind groups
    for (uint32_t i = 0; i < vertexBindGroupCount; i++) {
        SDL_free(vertexBindGroups[i].bindings);
    }
    SDL_free(vertexBindGroups);
    for (uint32_t i = 0; i < fragmentBindGroupCount; i++) {
        SDL_free(fragmentBindGroups[i].bindings);
    }
    SDL_free(fragmentBindGroups);

    wgpuDevicePopErrorScope(renderer->device, WebGPU_ErrorCallback, renderer);

    return (SDL_GPUGraphicsPipeline *)pipeline;
}

// Helper function to create or update a bind group
static void WebGPU_INTERNAL_CreateOrUpdateBindGroup(
    WebGPUCommandBuffer *commandBuffer,
    WebGPUGraphicsPipeline *pipeline,
    uint32_t bindGroupIndex)
{
    WebGPUBindGroup *bindGroup = &commandBuffer->currentResources.bindGroups[bindGroupIndex];
    WebGPUBindGroupLayout *layout = &pipeline->resourceLayout.bindGroupLayouts[bindGroupIndex];

    WGPUBindGroupEntry *entries = SDL_malloc(sizeof(WGPUBindGroupEntry) * layout->entryCount);

    for (uint32_t i = 0; i < layout->entryCount; i++) {
        entries[i].binding = i;
        entries[i].sampler = NULL;
        entries[i].textureView = NULL;
        entries[i].buffer = NULL;
        entries[i].offset = 0;
        entries[i].size = 0;

        switch (layout->types[i]) {
        case WGPUBindingType_Buffer:
        case WGPUBindingType_UniformBuffer:
        case WGPUBindingType_StorageBuffer:
        case WGPUBindingType_ReadOnlyStorageBuffer:
            entries[i].buffer = bindGroup->entries[i].buffer.bufferHandle->webgpuBuffer->buffer;
            entries[i].offset = bindGroup->entries[i].buffer.offset;
            entries[i].size = bindGroup->entries[i].buffer.size;
            break;
        case WGPUBindingType_Sampler:
        case WGPUBindingType_FilteringSampler:
        case WGPUBindingType_NonFilteringSampler:
        case WGPUBindingType_ComparisonSampler:
            entries[i].sampler = bindGroup->entries[i].sampler;
            break;
        case WGPUBindingType_FilteringTexture:
        case WGPUBindingType_NonFilteringTexture:
            entries[i].textureView = bindGroup->entries[i].textureView;
            break;
            // Add cases for other binding types as needed
        case WGPUBindingType_Undefined:
        default:
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Undefined binding type in bind group layout");
            break;
        }
    }

    WGPUBindGroupDescriptor bindGroupDescriptor = {
        .layout = layout->layout,
        .entryCount = layout->entryCount,
        .entries = entries
    };

    if (bindGroup->bindGroup != NULL) {
        wgpuBindGroupRelease(bindGroup->bindGroup);
    }

    bindGroup->bindGroup = wgpuDeviceCreateBindGroup(commandBuffer->renderer->device, &bindGroupDescriptor);

    SDL_free(entries);
}

static void WebGPU_BindGraphicsPipeline(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPUCommandBuffer *webgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;
    WebGPURenderer *renderer = (WebGPURenderer *)webgpuCommandBuffer->renderer;
    WebGPUGraphicsPipeline *pipeline = (WebGPUGraphicsPipeline *)graphicsPipeline;

    // Bind the pipeline
    wgpuRenderPassEncoderSetPipeline(webgpuCommandBuffer->renderPassEncoder, pipeline->pipeline);

    // Set the current pipeline
    webgpuCommandBuffer->currentGraphicsPipeline = pipeline;

    // Track the pipeline (you may need to implement this function)
    WebGPU_INTERNAL_TrackGraphicsPipeline(webgpuCommandBuffer, pipeline);

    // Set viewport and scissor
    WebGPUViewport viewport = {
        .x = webgpuCommandBuffer->currentViewport.x,
        .y = webgpuCommandBuffer->currentViewport.y,
        .width = webgpuCommandBuffer->currentViewport.width,
        .height = webgpuCommandBuffer->currentViewport.height,
        .minDepth = webgpuCommandBuffer->currentViewport.minDepth,
        .maxDepth = webgpuCommandBuffer->currentViewport.maxDepth
    };
    wgpuRenderPassEncoderSetViewport(webgpuCommandBuffer->renderPassEncoder,
                                     viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);

    WebGPURect scissorRect = {
        .x = webgpuCommandBuffer->currentScissor.x,
        .y = webgpuCommandBuffer->currentScissor.y,
        .width = webgpuCommandBuffer->currentScissor.width,
        .height = webgpuCommandBuffer->currentScissor.height,
    };
    wgpuRenderPassEncoderSetScissorRect(webgpuCommandBuffer->renderPassEncoder,
                                        scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);

    // Bind resources based on the pipeline's resource layout
    for (uint32_t i = 0; i < pipeline->resourceLayout.bindGroupLayoutCount; i++) {
        WebGPUBindGroup *bindGroup = &webgpuCommandBuffer->currentResources.bindGroups[i];

        // Check if we need to create or update the bind group
        if (bindGroup->bindGroup == NULL || webgpuCommandBuffer->resourcesDirty) {
            WebGPU_INTERNAL_CreateOrUpdateBindGroup(webgpuCommandBuffer, pipeline, i);
        }

        // Set the bind group
        wgpuRenderPassEncoderSetBindGroup(webgpuCommandBuffer->renderPassEncoder, i, bindGroup->bindGroup, 0, NULL);
    }

    // Mark resources as clean
    webgpuCommandBuffer->resourcesDirty = false;
}

static void WebGPU_DrawPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 vertexCount,
    Uint32 instanceCount,
    Uint32 firstVertex,
    Uint32 firstInstance)
{
    WebGPUCommandBuffer *wgpuCommandBuffer = (WebGPUCommandBuffer *)commandBuffer;

    // TODO: I need to implement some kind of descriptor set system to ensure that we can bind all necessary data before proceeding with the RenderPass. Too tired to do this on a plane.

    wgpuRenderPassEncoderDraw(wgpuCommandBuffer->renderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);

    wgpuDevicePopErrorScope(wgpuCommandBuffer->renderer->device, WebGPU_ErrorCallback, wgpuCommandBuffer);
}

static bool WebGPU_PrepareDriver(SDL_VideoDevice *_this)
{
    // Realistically, we should check if the browser supports WebGPU here and return false if it doesn't
    // For now, we'll just return true because it'll simply crash if the browser doesn't support WebGPU anyways
    return true;
}

static void WebGPU_DestroyDevice(SDL_GPUDevice *device)
{
    WebGPURenderer *renderer = (WebGPURenderer *)device->driverData;

    // Destroy all claimed windows
    for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1) {
        WebGPU_ReleaseWindow((SDL_GPURenderer *)renderer, renderer->claimedWindows[i]->window);
    }

    /*// Destroy the device*/
    /*wgpuDeviceRelease(renderer->device);*/
    /*wgpuAdapterRelease(renderer->adapter);*/
    /*wgpuInstanceRelease(renderer->instance);*/

    // Free the renderer
    SDL_free(renderer);
}

static SDL_GPUDevice *WebGPU_CreateDevice(SDL_bool debug, bool preferLowPower, SDL_PropertiesID props)
{
    WebGPURenderer *renderer;
    SDL_GPUDevice *result = NULL;

    // Allocate memory for the renderer and device
    renderer = (WebGPURenderer *)SDL_malloc(sizeof(WebGPURenderer));
    memset(renderer, '\0', sizeof(WebGPURenderer));
    renderer->debugMode = debug;
    renderer->preferLowPower = preferLowPower;

    // Initialize WebGPU instance so that we can request an adapter and then device
    renderer->instance = wgpuCreateInstance(NULL);
    if (!renderer->instance) {
        SDL_SetError("Failed to create WebGPU instance");
        SDL_free(renderer);
        return NULL;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SDL_GPU Driver: WebGPU");

    WGPURequestAdapterOptions adapter_options = {
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .backendType = WGPUBackendType_Vulkan,
    };

    // Request adapter using the instance and then the device using the adapter (this is done in the callback)
    wgpuInstanceRequestAdapter(renderer->instance, &adapter_options, WebGPU_RequestAdapterCallback, renderer);

    // This seems to be necessary to ensure that the device is created before continuing
    // This should probably be tested on all browsers to ensure that it works as expected
    // but Chrome's Dawn WebGPU implementation needs this to work
    while (!renderer->device) {
        emscripten_sleep(1);
    }

    /*// Set our error callback for emscripten*/
    wgpuDeviceSetUncapturedErrorCallback(renderer->device, WebGPU_ErrorCallback, renderer);
    wgpuDevicePushErrorScope(renderer->device, WGPUErrorFilter_Validation);

    /*emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, emsc_fullscreen_callback);*/

    // Acquire the queue from the device
    renderer->queue = wgpuDeviceGetQueue(renderer->device);

    result = (SDL_GPUDevice *)SDL_malloc(sizeof(SDL_GPUDevice));


    // Initialize Tint for SPIRV to WGSL conversion
    tint_initialize();

    /*
    TODO: Ensure that all function signatures for the driver are correct so that the following line compiles
          This will attach all of the driver's functions to the SDL_GPUDevice struct

          i.e. result->CreateTexture = WebGPU_CreateTexture;
               result->DestroyDevice = WebGPU_DestroyDevice;
               ... etc.
    */
    /*ASSIGN_DRIVER(WebGPU)*/
    result->DestroyDevice = WebGPU_DestroyDevice;
    result->ClaimWindow = WebGPU_ClaimWindow;
    result->ReleaseWindow = WebGPU_ReleaseWindow;
    result->driverData = (SDL_GPURenderer *)renderer;
    result->AcquireCommandBuffer = WebGPU_AcquireCommandBuffer;
    result->AcquireSwapchainTexture = WebGPU_AcquireSwapchainTexture;
    result->GetSwapchainTextureFormat = WebGPU_GetSwapchainTextureFormat;
    result->CreateShader = WebGPU_CreateShader;
    result->ReleaseShader = WebGPU_ReleaseShader;

    // TODO START (finish the implementation of these functions)

    result->CreateGraphicsPipeline = WebGPU_CreateGraphicsPipeline;
    result->BindGraphicsPipeline = WebGPU_BindGraphicsPipeline;
    result->DrawPrimitives = WebGPU_DrawPrimitives;

    // TODO END

    result->Submit = WebGPU_Submit;
    result->BeginRenderPass = WebGPU_BeginRenderPass;
    result->EndRenderPass = WebGPU_EndRenderPass;

    return result;
}

// TODO: Implement other necessary functions like WebGPU_DestroyDevice, WebGPU_CreateTexture, etc.

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    SDL_GPU_DRIVER_WEBGPU,
    SDL_GPU_SHADERFORMAT_SPIRV,
    WebGPU_PrepareDriver,
    WebGPU_CreateDevice,
};
