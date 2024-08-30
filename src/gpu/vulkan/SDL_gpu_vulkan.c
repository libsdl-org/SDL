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

#if SDL_GPU_VULKAN

// Needed for VK_KHR_portability_subset
#define VK_ENABLE_BETA_EXTENSIONS

#define VK_NO_PROTOTYPES
#include "../../video/khronos/vulkan/vulkan.h"

#include "SDL_hashtable.h"
#include <SDL3/SDL_vulkan.h>

#include "../SDL_sysgpu.h"

#define VULKAN_INTERNAL_clamp(val, min, max) SDL_max(min, SDL_min(val, max))

// Global Vulkan Loader Entry Points

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;

#define VULKAN_GLOBAL_FUNCTION(name) \
    static PFN_##name name = NULL;
#include "SDL_gpu_vulkan_vkfuncs.h"

typedef struct VulkanExtensions
{
    // These extensions are required!

    // Globally supported
    Uint8 KHR_swapchain;
    // Core since 1.1, needed for negative VkViewport::height
    Uint8 KHR_maintenance1;

    // These extensions are optional!

    // Core since 1.2, but requires annoying paperwork to implement
    Uint8 KHR_driver_properties;
    // EXT, probably not going to be Core
    Uint8 EXT_vertex_attribute_divisor;
    // Only required for special implementations (i.e. MoltenVK)
    Uint8 KHR_portability_subset;
} VulkanExtensions;

// Defines

#define SMALL_ALLOCATION_THRESHOLD    2097152  // 2   MiB
#define SMALL_ALLOCATION_SIZE         16777216 // 16  MiB
#define LARGE_ALLOCATION_INCREMENT    67108864 // 64  MiB
#define MAX_UBO_SECTION_SIZE          4096     // 4   KiB
#define DESCRIPTOR_POOL_STARTING_SIZE 128
#define WINDOW_PROPERTY_DATA          "SDL_GPUVulkanWindowPropertyData"

#define IDENTITY_SWIZZLE               \
    {                                  \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY  \
    }

#define NULL_DESC_LAYOUT     (VkDescriptorSetLayout)0
#define NULL_PIPELINE_LAYOUT (VkPipelineLayout)0
#define NULL_RENDER_PASS     (SDL_GPURenderPass *)0

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

// Conversions

static const Uint8 DEVICE_PRIORITY_HIGHPERFORMANCE[] = {
    0, // VK_PHYSICAL_DEVICE_TYPE_OTHER
    3, // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
    4, // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    2, // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
    1  // VK_PHYSICAL_DEVICE_TYPE_CPU
};

static const Uint8 DEVICE_PRIORITY_LOWPOWER[] = {
    0, // VK_PHYSICAL_DEVICE_TYPE_OTHER
    4, // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
    3, // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    2, // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
    1  // VK_PHYSICAL_DEVICE_TYPE_CPU
};

static VkPresentModeKHR SDLToVK_PresentMode[] = {
    VK_PRESENT_MODE_FIFO_KHR,
    VK_PRESENT_MODE_IMMEDIATE_KHR,
    VK_PRESENT_MODE_MAILBOX_KHR
};

static VkFormat SDLToVK_SurfaceFormat[] = {
    VK_FORMAT_R8G8B8A8_UNORM,           // R8G8B8A8_UNORM
    VK_FORMAT_B8G8R8A8_UNORM,           // B8G8R8A8_UNORM
    VK_FORMAT_R5G6B5_UNORM_PACK16,      // B5G6R5_UNORM
    VK_FORMAT_A1R5G5B5_UNORM_PACK16,    // B5G5R5A1_UNORM
    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    // B4G4R4A4_UNORM
    VK_FORMAT_A2B10G10R10_UNORM_PACK32, // R10G10B10A2_UNORM
    VK_FORMAT_R16G16_UNORM,             // R16G16_UNORM
    VK_FORMAT_R16G16B16A16_UNORM,       // R16G16B16A16_UNORM
    VK_FORMAT_R8_UNORM,                 // R8_UNORM
    VK_FORMAT_R8_UNORM,                 // A8_UNORM
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK,     // BC1_UNORM
    VK_FORMAT_BC2_UNORM_BLOCK,          // BC2_UNORM
    VK_FORMAT_BC3_UNORM_BLOCK,          // BC3_UNORM
    VK_FORMAT_BC7_UNORM_BLOCK,          // BC7_UNORM
    VK_FORMAT_R8G8_SNORM,               // R8G8_SNORM
    VK_FORMAT_R8G8B8A8_SNORM,           // R8G8B8A8_SNORM
    VK_FORMAT_R16_SFLOAT,               // R16_FLOAT
    VK_FORMAT_R16G16_SFLOAT,            // R16G16_FLOAT
    VK_FORMAT_R16G16B16A16_SFLOAT,      // R16G16B16A16_FLOAT
    VK_FORMAT_R32_SFLOAT,               // R32_FLOAT
    VK_FORMAT_R32G32_SFLOAT,            // R32G32_FLOAT
    VK_FORMAT_R32G32B32A32_SFLOAT,      // R32G32B32A32_FLOAT
    VK_FORMAT_R8_UINT,                  // R8_UINT
    VK_FORMAT_R8G8_UINT,                // R8G8_UINT
    VK_FORMAT_R8G8B8A8_UINT,            // R8G8B8A8_UINT
    VK_FORMAT_R16_UINT,                 // R16_UINT
    VK_FORMAT_R16G16_UINT,              // R16G16_UINT
    VK_FORMAT_R16G16B16A16_UINT,        // R16G16B16A16_UINT
    VK_FORMAT_R8G8B8A8_SRGB,            // R8G8B8A8_UNORM_SRGB
    VK_FORMAT_B8G8R8A8_SRGB,            // B8G8R8A8_UNORM_SRGB
    VK_FORMAT_BC3_SRGB_BLOCK,           // BC3_UNORM_SRGB
    VK_FORMAT_BC7_SRGB_BLOCK,           // BC7_UNORM_SRGB
    VK_FORMAT_D16_UNORM,                // D16_UNORM
    VK_FORMAT_X8_D24_UNORM_PACK32,      // D24_UNORM
    VK_FORMAT_D32_SFLOAT,               // D32_FLOAT
    VK_FORMAT_D24_UNORM_S8_UINT,        // D24_UNORM_S8_UINT
    VK_FORMAT_D32_SFLOAT_S8_UINT,       // D32_FLOAT_S8_UINT
};
SDL_COMPILE_TIME_ASSERT(SDLToVK_SurfaceFormat, SDL_arraysize(SDLToVK_SurfaceFormat) == SDL_GPU_TEXTUREFORMAT_MAX);

static VkComponentMapping SDLToVK_SurfaceSwizzle[] = {
    IDENTITY_SWIZZLE, // R8G8B8A8
    IDENTITY_SWIZZLE, // B8G8R8A8
    {
        // B5G6R5
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_ONE,
    },
    {
        // B5G5R5A1
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_A,
    },
    IDENTITY_SWIZZLE, // B4G4R4A4
    {
        // R10G10B10A2
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    },
    IDENTITY_SWIZZLE, // R16G16
    IDENTITY_SWIZZLE, // R16G16B16A16
    IDENTITY_SWIZZLE, // R8
    {
        // A8
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_R,
    },
    IDENTITY_SWIZZLE, // BC1
    IDENTITY_SWIZZLE, // BC2
    IDENTITY_SWIZZLE, // BC3
    IDENTITY_SWIZZLE, // BC7
    IDENTITY_SWIZZLE, // R8G8_SNORM
    IDENTITY_SWIZZLE, // R8G8B8A8_SNORM
    IDENTITY_SWIZZLE, // R16_SFLOAT
    IDENTITY_SWIZZLE, // R16G16_SFLOAT
    IDENTITY_SWIZZLE, // R16G16B16A16_SFLOAT
    IDENTITY_SWIZZLE, // R32_SFLOAT
    IDENTITY_SWIZZLE, // R32G32_SFLOAT
    IDENTITY_SWIZZLE, // R32G32B32A32_SFLOAT
    IDENTITY_SWIZZLE, // R8_UINT
    IDENTITY_SWIZZLE, // R8G8_UINT
    IDENTITY_SWIZZLE, // R8G8B8A8_UINT
    IDENTITY_SWIZZLE, // R16_UINT
    IDENTITY_SWIZZLE, // R16G16_UINT
    IDENTITY_SWIZZLE, // R16G16B16A16_UINT
    IDENTITY_SWIZZLE, // R8G8B8A8_SRGB
    IDENTITY_SWIZZLE, // B8G8R8A8_SRGB
    IDENTITY_SWIZZLE, // BC3_SRGB
    IDENTITY_SWIZZLE, // BC7_SRGB
    IDENTITY_SWIZZLE, // D16_UNORM
    IDENTITY_SWIZZLE, // D24_UNORM
    IDENTITY_SWIZZLE, // D32_SFLOAT
    IDENTITY_SWIZZLE, // D24_UNORM_S8_UINT
    IDENTITY_SWIZZLE, // D32_SFLOAT_S8_UINT
};

static VkFormat SwapchainCompositionToFormat[] = {
    VK_FORMAT_B8G8R8A8_UNORM,          // SDR
    VK_FORMAT_B8G8R8A8_SRGB,           // SDR_LINEAR
    VK_FORMAT_R16G16B16A16_SFLOAT,     // HDR_EXTENDED_LINEAR
    VK_FORMAT_A2B10G10R10_UNORM_PACK32 // HDR10_ST2048
};

static VkFormat SwapchainCompositionToFallbackFormat[] = {
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_UNDEFINED, // no fallback
    VK_FORMAT_UNDEFINED  // no fallback
};

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

static VkColorSpaceKHR SwapchainCompositionToColorSpace[] = {
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
    VK_COLOR_SPACE_HDR10_ST2084_EXT
};

static VkComponentMapping SwapchainCompositionSwizzle[] = {
    IDENTITY_SWIZZLE, // SDR
    IDENTITY_SWIZZLE, // SDR_SRGB
    IDENTITY_SWIZZLE, // HDR
    {
        // HDR_ADVANCED
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    }
};

static VkFormat SDLToVK_VertexFormat[] = {
    VK_FORMAT_R32_SINT,            // INT
    VK_FORMAT_R32G32_SINT,         // INT2
    VK_FORMAT_R32G32B32_SINT,      // INT3
    VK_FORMAT_R32G32B32A32_SINT,   // INT4
    VK_FORMAT_R32_UINT,            // UINT
    VK_FORMAT_R32G32_UINT,         // UINT2
    VK_FORMAT_R32G32B32_UINT,      // UINT3
    VK_FORMAT_R32G32B32A32_UINT,   // UINT4
    VK_FORMAT_R32_SFLOAT,          // FLOAT
    VK_FORMAT_R32G32_SFLOAT,       // FLOAT2
    VK_FORMAT_R32G32B32_SFLOAT,    // FLOAT3
    VK_FORMAT_R32G32B32A32_SFLOAT, // FLOAT4
    VK_FORMAT_R8G8_SINT,           // BYTE2
    VK_FORMAT_R8G8B8A8_SINT,       // BYTE4
    VK_FORMAT_R8G8_UINT,           // UBYTE2
    VK_FORMAT_R8G8B8A8_UINT,       // UBYTE4
    VK_FORMAT_R8G8_SNORM,          // BYTE2_NORM
    VK_FORMAT_R8G8B8A8_SNORM,      // BYTE4_NORM
    VK_FORMAT_R8G8_UNORM,          // UBYTE2_NORM
    VK_FORMAT_R8G8B8A8_UNORM,      // UBYTE4_NORM
    VK_FORMAT_R16G16_SINT,         // SHORT2
    VK_FORMAT_R16G16B16A16_SINT,   // SHORT4
    VK_FORMAT_R16G16_UINT,         // USHORT2
    VK_FORMAT_R16G16B16A16_UINT,   // USHORT4
    VK_FORMAT_R16G16_SNORM,        // SHORT2_NORM
    VK_FORMAT_R16G16B16A16_SNORM,  // SHORT4_NORM
    VK_FORMAT_R16G16_UNORM,        // USHORT2_NORM
    VK_FORMAT_R16G16B16A16_UNORM,  // USHORT4_NORM
    VK_FORMAT_R16G16_SFLOAT,       // HALF2
    VK_FORMAT_R16G16B16A16_SFLOAT  // HALF4
};

static VkIndexType SDLToVK_IndexType[] = {
    VK_INDEX_TYPE_UINT16,
    VK_INDEX_TYPE_UINT32
};

static VkPrimitiveTopology SDLToVK_PrimitiveType[] = {
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
};

static VkCullModeFlags SDLToVK_CullMode[] = {
    VK_CULL_MODE_NONE,
    VK_CULL_MODE_FRONT_BIT,
    VK_CULL_MODE_BACK_BIT,
    VK_CULL_MODE_FRONT_AND_BACK
};

static VkFrontFace SDLToVK_FrontFace[] = {
    VK_FRONT_FACE_COUNTER_CLOCKWISE,
    VK_FRONT_FACE_CLOCKWISE
};

static VkBlendFactor SDLToVK_BlendFactor[] = {
    VK_BLEND_FACTOR_ZERO,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_FACTOR_SRC_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    VK_BLEND_FACTOR_DST_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    VK_BLEND_FACTOR_SRC_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    VK_BLEND_FACTOR_DST_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    VK_BLEND_FACTOR_CONSTANT_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    VK_BLEND_FACTOR_CONSTANT_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
};

static VkBlendOp SDLToVK_BlendOp[] = {
    VK_BLEND_OP_ADD,
    VK_BLEND_OP_SUBTRACT,
    VK_BLEND_OP_REVERSE_SUBTRACT,
    VK_BLEND_OP_MIN,
    VK_BLEND_OP_MAX
};

static VkCompareOp SDLToVK_CompareOp[] = {
    VK_COMPARE_OP_NEVER,
    VK_COMPARE_OP_LESS,
    VK_COMPARE_OP_EQUAL,
    VK_COMPARE_OP_LESS_OR_EQUAL,
    VK_COMPARE_OP_GREATER,
    VK_COMPARE_OP_NOT_EQUAL,
    VK_COMPARE_OP_GREATER_OR_EQUAL,
    VK_COMPARE_OP_ALWAYS
};

static VkStencilOp SDLToVK_StencilOp[] = {
    VK_STENCIL_OP_KEEP,
    VK_STENCIL_OP_ZERO,
    VK_STENCIL_OP_REPLACE,
    VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    VK_STENCIL_OP_INVERT,
    VK_STENCIL_OP_INCREMENT_AND_WRAP,
    VK_STENCIL_OP_DECREMENT_AND_WRAP
};

static VkAttachmentLoadOp SDLToVK_LoadOp[] = {
    VK_ATTACHMENT_LOAD_OP_LOAD,
    VK_ATTACHMENT_LOAD_OP_CLEAR,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE
};

static VkAttachmentStoreOp SDLToVK_StoreOp[] = {
    VK_ATTACHMENT_STORE_OP_STORE,
    VK_ATTACHMENT_STORE_OP_DONT_CARE
};

static VkSampleCountFlagBits SDLToVK_SampleCount[] = {
    VK_SAMPLE_COUNT_1_BIT,
    VK_SAMPLE_COUNT_2_BIT,
    VK_SAMPLE_COUNT_4_BIT,
    VK_SAMPLE_COUNT_8_BIT
};

static VkVertexInputRate SDLToVK_VertexInputRate[] = {
    VK_VERTEX_INPUT_RATE_VERTEX,
    VK_VERTEX_INPUT_RATE_INSTANCE
};

static VkFilter SDLToVK_Filter[] = {
    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR
};

static VkSamplerMipmapMode SDLToVK_SamplerMipmapMode[] = {
    VK_SAMPLER_MIPMAP_MODE_NEAREST,
    VK_SAMPLER_MIPMAP_MODE_LINEAR
};

static VkSamplerAddressMode SDLToVK_SamplerAddressMode[] = {
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
};

// Structures

typedef struct VulkanMemoryAllocation VulkanMemoryAllocation;
typedef struct VulkanBuffer VulkanBuffer;
typedef struct VulkanBufferContainer VulkanBufferContainer;
typedef struct VulkanTexture VulkanTexture;
typedef struct VulkanTextureContainer VulkanTextureContainer;

typedef struct VulkanFenceHandle
{
    VkFence fence;
    SDL_AtomicInt referenceCount;
} VulkanFenceHandle;

// Memory Allocation

typedef struct VulkanMemoryFreeRegion
{
    VulkanMemoryAllocation *allocation;
    VkDeviceSize offset;
    VkDeviceSize size;
    Uint32 allocationIndex;
    Uint32 sortedIndex;
} VulkanMemoryFreeRegion;

typedef struct VulkanMemoryUsedRegion
{
    VulkanMemoryAllocation *allocation;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkDeviceSize resourceOffset; // differs from offset based on alignment
    VkDeviceSize resourceSize;   // differs from size based on alignment
    VkDeviceSize alignment;
    Uint8 isBuffer;
    union
    {
        VulkanBuffer *vulkanBuffer;
        VulkanTexture *vulkanTexture;
    };
} VulkanMemoryUsedRegion;

typedef struct VulkanMemorySubAllocator
{
    Uint32 memoryTypeIndex;
    VulkanMemoryAllocation **allocations;
    Uint32 allocationCount;
    VulkanMemoryFreeRegion **sortedFreeRegions;
    Uint32 sortedFreeRegionCount;
    Uint32 sortedFreeRegionCapacity;
} VulkanMemorySubAllocator;

struct VulkanMemoryAllocation
{
    VulkanMemorySubAllocator *allocator;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VulkanMemoryUsedRegion **usedRegions;
    Uint32 usedRegionCount;
    Uint32 usedRegionCapacity;
    VulkanMemoryFreeRegion **freeRegions;
    Uint32 freeRegionCount;
    Uint32 freeRegionCapacity;
    Uint8 availableForAllocation;
    VkDeviceSize freeSpace;
    VkDeviceSize usedSpace;
    Uint8 *mapPointer;
    SDL_Mutex *memoryLock;
};

typedef struct VulkanMemoryAllocator
{
    VulkanMemorySubAllocator subAllocators[VK_MAX_MEMORY_TYPES];
} VulkanMemoryAllocator;

// Memory structures

/* We use pointer indirection so that defrag can occur without objects
 * needing to be aware of the backing buffers changing.
 */
typedef struct VulkanBufferHandle
{
    VulkanBuffer *vulkanBuffer;
    VulkanBufferContainer *container;
} VulkanBufferHandle;

typedef enum VulkanBufferType
{
    VULKAN_BUFFER_TYPE_GPU,
    VULKAN_BUFFER_TYPE_UNIFORM,
    VULKAN_BUFFER_TYPE_TRANSFER
} VulkanBufferType;

struct VulkanBuffer
{
    VkBuffer buffer;
    VkDeviceSize size;
    VulkanMemoryUsedRegion *usedRegion;

    VulkanBufferType type;
    SDL_GPUBufferUsageFlags usageFlags;

    SDL_AtomicInt referenceCount; // Tracks command buffer usage

    VulkanBufferHandle *handle;

    bool transitioned;
    Uint8 markedForDestroy; // so that defrag doesn't double-free
};

/* Buffer resources consist of multiple backing buffer handles so that data transfers
 * can occur without blocking or the client having to manage extra resources.
 *
 * Cast from SDL_GPUBuffer or SDL_GPUTransferBuffer.
 */
struct VulkanBufferContainer
{
    VulkanBufferHandle *activeBufferHandle;

    /* These are all the buffer handles that have been used by this container.
     * If the resource is bound and then updated with a cycle parameter, a new resource
     * will be added to this list.
     * These can be reused after they are submitted and command processing is complete.
     */
    Uint32 bufferCapacity;
    Uint32 bufferCount;
    VulkanBufferHandle **bufferHandles;

    char *debugName;
};

// Renderer Structure

typedef struct QueueFamilyIndices
{
    Uint32 graphicsFamily;
    Uint32 presentFamily;
    Uint32 computeFamily;
    Uint32 transferFamily;
} QueueFamilyIndices;

typedef struct VulkanSampler
{
    VkSampler sampler;
    SDL_AtomicInt referenceCount;
} VulkanSampler;

typedef struct VulkanShader
{
    VkShaderModule shaderModule;
    const char *entryPointName;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;
    SDL_AtomicInt referenceCount;
} VulkanShader;

typedef struct VulkanTextureHandle
{
    VulkanTexture *vulkanTexture;
    VulkanTextureContainer *container;
} VulkanTextureHandle;

/* Textures are made up of individual subresources.
 * This helps us barrier the resource efficiently.
 */
typedef struct VulkanTextureSubresource
{
    VulkanTexture *parent;
    Uint32 layer;
    Uint32 level;

    VkImageView *renderTargetViews; // One render target view per depth slice
    VkImageView computeWriteView;
    VkImageView depthStencilView;

    VulkanTextureHandle *msaaTexHandle; // NULL if parent sample count is 1 or is depth target

    bool transitioned; // used for layout tracking
} VulkanTextureSubresource;

struct VulkanTexture
{
    VulkanMemoryUsedRegion *usedRegion;

    VkImage image;
    VkImageView fullView; // used for samplers and storage reads
    VkExtent2D dimensions;

    SDL_GPUTextureType type;
    Uint8 isMSAAColorTarget;

    Uint32 depth;
    Uint32 layerCount;
    Uint32 levelCount;
    VkSampleCountFlagBits sampleCount; // NOTE: This refers to the sample count of a render target pass using this texture, not the actual sample count of the texture
    VkFormat format;
    VkComponentMapping swizzle;
    SDL_GPUTextureUsageFlags usageFlags;
    VkImageAspectFlags aspectFlags;

    Uint32 subresourceCount;
    VulkanTextureSubresource *subresources;

    VulkanTextureHandle *handle;

    Uint8 markedForDestroy; // so that defrag doesn't double-free
    SDL_AtomicInt referenceCount;
};

/* Texture resources consist of multiple backing texture handles so that data transfers
 * can occur without blocking or the client having to manage extra resources.
 *
 * Cast from SDL_GPUTexture.
 */
struct VulkanTextureContainer
{
    TextureCommonHeader header; // FIXME: Use this instead of passing so many args to CreateTexture

    VulkanTextureHandle *activeTextureHandle;

    /* These are all the texture handles that have been used by this container.
     * If the resource is bound and then updated with CYCLE, a new resource
     * will be added to this list.
     * These can be reused after they are submitted and command processing is complete.
     */
    Uint32 textureCapacity;
    Uint32 textureCount;
    VulkanTextureHandle **textureHandles;

    // Swapchain images cannot be cycled
    Uint8 canBeCycled;

    char *debugName;
};

typedef enum VulkanBufferUsageMode
{
    VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
    VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION,
    VULKAN_BUFFER_USAGE_MODE_VERTEX_READ,
    VULKAN_BUFFER_USAGE_MODE_INDEX_READ,
    VULKAN_BUFFER_USAGE_MODE_INDIRECT,
    VULKAN_BUFFER_USAGE_MODE_GRAPHICS_STORAGE_READ,
    VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ,
    VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
} VulkanBufferUsageMode;

typedef enum VulkanTextureUsageMode
{
    VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
    VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
    VULKAN_TEXTURE_USAGE_MODE_SAMPLER,
    VULKAN_TEXTURE_USAGE_MODE_GRAPHICS_STORAGE_READ,
    VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
    VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
    VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT,
    VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT,
    VULKAN_TEXTURE_USAGE_MODE_PRESENT
} VulkanTextureUsageMode;

typedef enum VulkanUniformBufferStage
{
    VULKAN_UNIFORM_BUFFER_STAGE_VERTEX,
    VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT,
    VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE
} VulkanUniformBufferStage;

typedef struct VulkanFramebuffer
{
    VkFramebuffer framebuffer;
    SDL_AtomicInt referenceCount;
} VulkanFramebuffer;

typedef struct VulkanSwapchainData
{
    // Window surface
    VkSurfaceKHR surface;

    // Swapchain for window surface
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkColorSpaceKHR colorSpace;
    VkComponentMapping swapchainSwizzle;
    VkPresentModeKHR presentMode;
    bool usingFallbackFormat;

    // Swapchain images
    VulkanTextureContainer *textureContainers; // use containers so that swapchain textures can use the same API as other textures
    Uint32 imageCount;

    // Synchronization primitives
    VkSemaphore imageAvailableSemaphore[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphore[MAX_FRAMES_IN_FLIGHT];
    VulkanFenceHandle *inFlightFences[MAX_FRAMES_IN_FLIGHT];

    Uint32 frameCounter;
} VulkanSwapchainData;

typedef struct WindowData
{
    SDL_Window *window;
    SDL_GPUSwapchainComposition swapchainComposition;
    SDL_GPUPresentMode presentMode;
    VulkanSwapchainData *swapchainData;
    bool needsSwapchainRecreate;
} WindowData;

typedef struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    Uint32 formatsLength;
    VkPresentModeKHR *presentModes;
    Uint32 presentModesLength;
} SwapchainSupportDetails;

typedef struct VulkanPresentData
{
    WindowData *windowData;
    Uint32 swapchainImageIndex;
} VulkanPresentData;

typedef struct VulkanUniformBuffer
{
    VulkanBufferHandle *bufferHandle;
    Uint32 drawOffset;
    Uint32 writeOffset;
} VulkanUniformBuffer;

typedef struct VulkanDescriptorInfo
{
    VkDescriptorType descriptorType;
    VkShaderStageFlagBits stageFlag;
} VulkanDescriptorInfo;

typedef struct DescriptorSetPool
{
    SDL_Mutex *lock;

    VkDescriptorSetLayout descriptorSetLayout;

    VulkanDescriptorInfo *descriptorInfos;
    Uint32 descriptorInfoCount;

    // This is actually a descriptor set and descriptor pool simultaneously
    VkDescriptorPool *descriptorPools;
    Uint32 descriptorPoolCount;
    Uint32 nextPoolSize;

    // We just manage a pool ourselves instead of freeing the sets
    VkDescriptorSet *inactiveDescriptorSets;
    Uint32 inactiveDescriptorSetCount;
    Uint32 inactiveDescriptorSetCapacity;
} DescriptorSetPool;

typedef struct VulkanGraphicsPipelineResourceLayout
{
    VkPipelineLayout pipelineLayout;

    /*
     * Descriptor set layout is as follows:
     * 0: vertex resources
     * 1: vertex uniform buffers
     * 2: fragment resources
     * 3: fragment uniform buffers
     */
    DescriptorSetPool descriptorSetPools[4];

    Uint32 vertexSamplerCount;
    Uint32 vertexStorageBufferCount;
    Uint32 vertexStorageTextureCount;
    Uint32 vertexUniformBufferCount;

    Uint32 fragmentSamplerCount;
    Uint32 fragmentStorageBufferCount;
    Uint32 fragmentStorageTextureCount;
    Uint32 fragmentUniformBufferCount;
} VulkanGraphicsPipelineResourceLayout;

typedef struct VulkanGraphicsPipeline
{
    VkPipeline pipeline;
    SDL_GPUPrimitiveType primitiveType;

    VulkanGraphicsPipelineResourceLayout resourceLayout;

    VulkanShader *vertexShader;
    VulkanShader *fragmentShader;

    SDL_AtomicInt referenceCount;
} VulkanGraphicsPipeline;

typedef struct VulkanComputePipelineResourceLayout
{
    VkPipelineLayout pipelineLayout;

    /*
     * Descriptor set layout is as follows:
     * 0: read-only textures, then read-only buffers
     * 1: write-only textures, then write-only buffers
     * 2: uniform buffers
     */
    DescriptorSetPool descriptorSetPools[3];

    Uint32 readOnlyStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 writeOnlyStorageTextureCount;
    Uint32 writeOnlyStorageBufferCount;
    Uint32 uniformBufferCount;
} VulkanComputePipelineResourceLayout;

typedef struct VulkanComputePipeline
{
    VkShaderModule shaderModule;
    VkPipeline pipeline;
    VulkanComputePipelineResourceLayout resourceLayout;
    SDL_AtomicInt referenceCount;
} VulkanComputePipeline;

typedef struct RenderPassColorTargetDescription
{
    VkFormat format;
    SDL_GPULoadOp loadOp;
    SDL_GPUStoreOp storeOp;
} RenderPassColorTargetDescription;

typedef struct RenderPassDepthStencilTargetDescription
{
    VkFormat format;
    SDL_GPULoadOp loadOp;
    SDL_GPUStoreOp storeOp;
    SDL_GPULoadOp stencilLoadOp;
    SDL_GPUStoreOp stencilStoreOp;
} RenderPassDepthStencilTargetDescription;

typedef struct CommandPoolHashTableKey
{
    SDL_ThreadID threadID;
} CommandPoolHashTableKey;

typedef struct RenderPassHashTableKey
{
    RenderPassColorTargetDescription colorTargetDescriptions[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentCount;
    RenderPassDepthStencilTargetDescription depthStencilTargetDescription;
    VkSampleCountFlagBits colorAttachmentSampleCount;
} RenderPassHashTableKey;

typedef struct VulkanRenderPassHashTableValue
{
    VkRenderPass handle;
} VulkanRenderPassHashTableValue;

typedef struct FramebufferHashTableKey
{
    VkImageView colorAttachmentViews[MAX_COLOR_TARGET_BINDINGS];
    VkImageView colorMultiSampleAttachmentViews[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentCount;
    VkImageView depthStencilAttachmentView;
    Uint32 width;
    Uint32 height;
} FramebufferHashTableKey;

// Command structures

typedef struct DescriptorSetData
{
    DescriptorSetPool *descriptorSetPool;
    VkDescriptorSet descriptorSet;
} DescriptorSetData;

typedef struct VulkanFencePool
{
    SDL_Mutex *lock;

    VulkanFenceHandle **availableFences;
    Uint32 availableFenceCount;
    Uint32 availableFenceCapacity;
} VulkanFencePool;

typedef struct VulkanCommandPool VulkanCommandPool;

typedef struct VulkanRenderer VulkanRenderer;

typedef struct VulkanCommandBuffer
{
    CommandBufferCommonHeader common;
    VulkanRenderer *renderer;

    VkCommandBuffer commandBuffer;
    VulkanCommandPool *commandPool;

    VulkanPresentData *presentDatas;
    Uint32 presentDataCount;
    Uint32 presentDataCapacity;

    VkSemaphore *waitSemaphores;
    Uint32 waitSemaphoreCount;
    Uint32 waitSemaphoreCapacity;

    VkSemaphore *signalSemaphores;
    Uint32 signalSemaphoreCount;
    Uint32 signalSemaphoreCapacity;

    VulkanComputePipeline *currentComputePipeline;
    VulkanGraphicsPipeline *currentGraphicsPipeline;

    // Keep track of resources transitioned away from their default state to barrier them on pass end

    VulkanTextureSubresource *colorAttachmentSubresources[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentSubresourceCount;

    VulkanTextureSubresource *depthStencilAttachmentSubresource; // may be NULL

    // Viewport/scissor state

    VkViewport currentViewport;
    VkRect2D currentScissor;

    // Resource bind state

    bool needNewVertexResourceDescriptorSet;
    bool needNewVertexUniformDescriptorSet;
    bool needNewVertexUniformOffsets;
    bool needNewFragmentResourceDescriptorSet;
    bool needNewFragmentUniformDescriptorSet;
    bool needNewFragmentUniformOffsets;

    bool needNewComputeReadOnlyDescriptorSet;
    bool needNewComputeWriteOnlyDescriptorSet;
    bool needNewComputeUniformDescriptorSet;
    bool needNewComputeUniformOffsets;

    VkDescriptorSet vertexResourceDescriptorSet;
    VkDescriptorSet vertexUniformDescriptorSet;
    VkDescriptorSet fragmentResourceDescriptorSet;
    VkDescriptorSet fragmentUniformDescriptorSet;

    VkDescriptorSet computeReadOnlyDescriptorSet;
    VkDescriptorSet computeWriteOnlyDescriptorSet;
    VkDescriptorSet computeUniformDescriptorSet;

    DescriptorSetData *boundDescriptorSetDatas;
    Uint32 boundDescriptorSetDataCount;
    Uint32 boundDescriptorSetDataCapacity;

    VulkanTexture *vertexSamplerTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanSampler *vertexSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanTexture *vertexStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *vertexStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    VulkanTexture *fragmentSamplerTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanSampler *fragmentSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanTexture *fragmentStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *fragmentStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    VulkanTextureSubresource *writeOnlyComputeStorageTextureSubresources[MAX_COMPUTE_WRITE_TEXTURES];
    Uint32 writeOnlyComputeStorageTextureSubresourceCount;
    VulkanBuffer *writeOnlyComputeStorageBuffers[MAX_COMPUTE_WRITE_BUFFERS];

    VulkanTexture *readOnlyComputeStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *readOnlyComputeStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    // Uniform buffers

    VulkanUniformBuffer *vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    VulkanUniformBuffer *fragmentUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    VulkanUniformBuffer *computeUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];

    // Track used resources

    VulkanBuffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    VulkanTexture **usedTextures;
    Uint32 usedTextureCount;
    Uint32 usedTextureCapacity;

    VulkanSampler **usedSamplers;
    Uint32 usedSamplerCount;
    Uint32 usedSamplerCapacity;

    VulkanGraphicsPipeline **usedGraphicsPipelines;
    Uint32 usedGraphicsPipelineCount;
    Uint32 usedGraphicsPipelineCapacity;

    VulkanComputePipeline **usedComputePipelines;
    Uint32 usedComputePipelineCount;
    Uint32 usedComputePipelineCapacity;

    VulkanFramebuffer **usedFramebuffers;
    Uint32 usedFramebufferCount;
    Uint32 usedFramebufferCapacity;

    VulkanUniformBuffer **usedUniformBuffers;
    Uint32 usedUniformBufferCount;
    Uint32 usedUniformBufferCapacity;

    VulkanFenceHandle *inFlightFence;
    Uint8 autoReleaseFence;

    Uint8 isDefrag; // Whether this CB was created for defragging
} VulkanCommandBuffer;

struct VulkanCommandPool
{
    SDL_ThreadID threadID;
    VkCommandPool commandPool;

    VulkanCommandBuffer **inactiveCommandBuffers;
    Uint32 inactiveCommandBufferCapacity;
    Uint32 inactiveCommandBufferCount;
};

// Context

struct VulkanRenderer
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties2KHR physicalDeviceProperties;
    VkPhysicalDeviceDriverPropertiesKHR physicalDeviceDriverProperties;
    VkDevice logicalDevice;
    Uint8 integratedMemoryNotification;
    Uint8 outOfDeviceLocalMemoryWarning;
    Uint8 outofBARMemoryWarning;
    Uint8 fillModeOnlyWarning;

    bool debugMode;
    bool preferLowPower;
    VulkanExtensions supports;
    bool supportsDebugUtils;
    bool supportsColorspace;
    bool supportsFillModeNonSolid;
    bool supportsMultiDrawIndirect;

    VulkanMemoryAllocator *memoryAllocator;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    WindowData **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;

    Uint32 queueFamilyIndex;
    VkQueue unifiedQueue;

    VulkanCommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCount;
    Uint32 submittedCommandBufferCapacity;

    VulkanFencePool fencePool;

    SDL_HashTable *commandPoolHashTable;
    SDL_HashTable *renderPassHashTable;
    SDL_HashTable *framebufferHashTable;

    VulkanUniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    Uint32 minUBOAlignment;

    // Some drivers don't support D16 for some reason. Fun!
    VkFormat D16Format;
    VkFormat D16S8Format;

    // Deferred resource destruction

    VulkanTexture **texturesToDestroy;
    Uint32 texturesToDestroyCount;
    Uint32 texturesToDestroyCapacity;

    VulkanBuffer **buffersToDestroy;
    Uint32 buffersToDestroyCount;
    Uint32 buffersToDestroyCapacity;

    VulkanSampler **samplersToDestroy;
    Uint32 samplersToDestroyCount;
    Uint32 samplersToDestroyCapacity;

    VulkanGraphicsPipeline **graphicsPipelinesToDestroy;
    Uint32 graphicsPipelinesToDestroyCount;
    Uint32 graphicsPipelinesToDestroyCapacity;

    VulkanComputePipeline **computePipelinesToDestroy;
    Uint32 computePipelinesToDestroyCount;
    Uint32 computePipelinesToDestroyCapacity;

    VulkanShader **shadersToDestroy;
    Uint32 shadersToDestroyCount;
    Uint32 shadersToDestroyCapacity;

    VulkanFramebuffer **framebuffersToDestroy;
    Uint32 framebuffersToDestroyCount;
    Uint32 framebuffersToDestroyCapacity;

    SDL_Mutex *allocatorLock;
    SDL_Mutex *disposeLock;
    SDL_Mutex *submitLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *acquireUniformBufferLock;
    SDL_Mutex *renderPassFetchLock;
    SDL_Mutex *framebufferFetchLock;

    Uint8 defragInProgress;

    VulkanMemoryAllocation **allocationsToDefrag;
    Uint32 allocationsToDefragCount;
    Uint32 allocationsToDefragCapacity;

#define VULKAN_INSTANCE_FUNCTION(func) \
    PFN_##func func;
#define VULKAN_DEVICE_FUNCTION(func) \
    PFN_##func func;
#include "SDL_gpu_vulkan_vkfuncs.h"
};

// Forward declarations

static Uint8 VULKAN_INTERNAL_DefragmentMemory(VulkanRenderer *renderer);
static void VULKAN_INTERNAL_BeginCommandBuffer(VulkanRenderer *renderer, VulkanCommandBuffer *commandBuffer);
static void VULKAN_UnclaimWindow(SDL_GPURenderer *driverData, SDL_Window *window);
static void VULKAN_Wait(SDL_GPURenderer *driverData);
static void VULKAN_WaitForFences(SDL_GPURenderer *driverData, bool waitAll, SDL_GPUFence **pFences, Uint32 fenceCount);
static void VULKAN_Submit(SDL_GPUCommandBuffer *commandBuffer);
static VulkanTexture *VULKAN_INTERNAL_CreateTexture(
    VulkanRenderer *renderer,
    Uint32 width,
    Uint32 height,
    Uint32 depth,
    SDL_GPUTextureType type,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GPUTextureUsageFlags textureUsageFlags,
    bool isMSAAColorTarget);

// Error Handling

static inline const char *VkErrorMessages(VkResult code)
{
#define ERR_TO_STR(e) \
    case e:           \
        return #e;
    switch (code) {
        ERR_TO_STR(VK_ERROR_OUT_OF_HOST_MEMORY)
        ERR_TO_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY)
        ERR_TO_STR(VK_ERROR_FRAGMENTED_POOL)
        ERR_TO_STR(VK_ERROR_OUT_OF_POOL_MEMORY)
        ERR_TO_STR(VK_ERROR_INITIALIZATION_FAILED)
        ERR_TO_STR(VK_ERROR_LAYER_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_EXTENSION_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_FEATURE_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_TOO_MANY_OBJECTS)
        ERR_TO_STR(VK_ERROR_DEVICE_LOST)
        ERR_TO_STR(VK_ERROR_INCOMPATIBLE_DRIVER)
        ERR_TO_STR(VK_ERROR_OUT_OF_DATE_KHR)
        ERR_TO_STR(VK_ERROR_SURFACE_LOST_KHR)
        ERR_TO_STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        ERR_TO_STR(VK_SUBOPTIMAL_KHR)
    default:
        return "Unhandled VkResult!";
    }
#undef ERR_TO_STR
}

static inline void LogVulkanResultAsError(
    const char *vulkanFunctionName,
    VkResult result)
{
    if (result != VK_SUCCESS) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "%s: %s",
            vulkanFunctionName,
            VkErrorMessages(result));
    }
}

#define VULKAN_ERROR_CHECK(res, fn, ret)                                        \
    if (res != VK_SUCCESS) {                                                    \
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "%s %s", #fn, VkErrorMessages(res)); \
        return ret;                                                             \
    }

// Utility

static inline bool VULKAN_INTERNAL_IsVulkanDepthFormat(VkFormat format)
{
    // FIXME: Can we refactor and use the regular IsDepthFormat for this?
    return (
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D16_UNORM] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D24_UNORM] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D32_FLOAT] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT]);
}

static inline VkSampleCountFlagBits VULKAN_INTERNAL_GetMaxMultiSampleCount(
    VulkanRenderer *renderer,
    VkSampleCountFlagBits multiSampleCount)
{
    VkSampleCountFlags flags = renderer->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts;
    VkSampleCountFlagBits maxSupported = VK_SAMPLE_COUNT_1_BIT;

    if (flags & VK_SAMPLE_COUNT_8_BIT) {
        maxSupported = VK_SAMPLE_COUNT_8_BIT;
    } else if (flags & VK_SAMPLE_COUNT_4_BIT) {
        maxSupported = VK_SAMPLE_COUNT_4_BIT;
    } else if (flags & VK_SAMPLE_COUNT_2_BIT) {
        maxSupported = VK_SAMPLE_COUNT_2_BIT;
    }

    return SDL_min(multiSampleCount, maxSupported);
}

static inline VkPolygonMode SDLToVK_PolygonMode(
    VulkanRenderer *renderer,
    SDL_GPUFillMode mode)
{
    if (mode == SDL_GPU_FILLMODE_FILL) {
        return VK_POLYGON_MODE_FILL; // always available!
    }

    if (renderer->supportsFillModeNonSolid && mode == SDL_GPU_FILLMODE_LINE) {
        return VK_POLYGON_MODE_LINE;
    }

    if (!renderer->fillModeOnlyWarning) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Unsupported fill mode requested, using FILL!");
        renderer->fillModeOnlyWarning = 1;
    }
    return VK_POLYGON_MODE_FILL;
}

// Memory Management

// Vulkan: Memory Allocation

static inline VkDeviceSize VULKAN_INTERNAL_NextHighestAlignment(
    VkDeviceSize n,
    VkDeviceSize align)
{
    return align * ((n + align - 1) / align);
}

static inline Uint32 VULKAN_INTERNAL_NextHighestAlignment32(
    Uint32 n,
    Uint32 align)
{
    return align * ((n + align - 1) / align);
}

static void VULKAN_INTERNAL_MakeMemoryUnavailable(
    VulkanRenderer *renderer,
    VulkanMemoryAllocation *allocation)
{
    Uint32 i, j;
    VulkanMemoryFreeRegion *freeRegion;

    allocation->availableForAllocation = 0;

    for (i = 0; i < allocation->freeRegionCount; i += 1) {
        freeRegion = allocation->freeRegions[i];

        // close the gap in the sorted list
        if (allocation->allocator->sortedFreeRegionCount > 1) {
            for (j = freeRegion->sortedIndex; j < allocation->allocator->sortedFreeRegionCount - 1; j += 1) {
                allocation->allocator->sortedFreeRegions[j] =
                    allocation->allocator->sortedFreeRegions[j + 1];

                allocation->allocator->sortedFreeRegions[j]->sortedIndex = j;
            }
        }

        allocation->allocator->sortedFreeRegionCount -= 1;
    }
}

static void VULKAN_INTERNAL_MarkAllocationsForDefrag(
    VulkanRenderer *renderer)
{
    Uint32 memoryType, allocationIndex;
    VulkanMemorySubAllocator *currentAllocator;

    for (memoryType = 0; memoryType < VK_MAX_MEMORY_TYPES; memoryType += 1) {
        currentAllocator = &renderer->memoryAllocator->subAllocators[memoryType];

        for (allocationIndex = 0; allocationIndex < currentAllocator->allocationCount; allocationIndex += 1) {
            if (currentAllocator->allocations[allocationIndex]->availableForAllocation == 1) {
                if (currentAllocator->allocations[allocationIndex]->freeRegionCount > 1) {
                    EXPAND_ARRAY_IF_NEEDED(
                        renderer->allocationsToDefrag,
                        VulkanMemoryAllocation *,
                        renderer->allocationsToDefragCount + 1,
                        renderer->allocationsToDefragCapacity,
                        renderer->allocationsToDefragCapacity * 2);

                    renderer->allocationsToDefrag[renderer->allocationsToDefragCount] =
                        currentAllocator->allocations[allocationIndex];

                    renderer->allocationsToDefragCount += 1;

                    VULKAN_INTERNAL_MakeMemoryUnavailable(
                        renderer,
                        currentAllocator->allocations[allocationIndex]);
                }
            }
        }
    }
}

static void VULKAN_INTERNAL_RemoveMemoryFreeRegion(
    VulkanRenderer *renderer,
    VulkanMemoryFreeRegion *freeRegion)
{
    Uint32 i;

    SDL_LockMutex(renderer->allocatorLock);

    if (freeRegion->allocation->availableForAllocation) {
        // close the gap in the sorted list
        if (freeRegion->allocation->allocator->sortedFreeRegionCount > 1) {
            for (i = freeRegion->sortedIndex; i < freeRegion->allocation->allocator->sortedFreeRegionCount - 1; i += 1) {
                freeRegion->allocation->allocator->sortedFreeRegions[i] =
                    freeRegion->allocation->allocator->sortedFreeRegions[i + 1];

                freeRegion->allocation->allocator->sortedFreeRegions[i]->sortedIndex = i;
            }
        }

        freeRegion->allocation->allocator->sortedFreeRegionCount -= 1;
    }

    // close the gap in the buffer list
    if (freeRegion->allocation->freeRegionCount > 1 && freeRegion->allocationIndex != freeRegion->allocation->freeRegionCount - 1) {
        freeRegion->allocation->freeRegions[freeRegion->allocationIndex] =
            freeRegion->allocation->freeRegions[freeRegion->allocation->freeRegionCount - 1];

        freeRegion->allocation->freeRegions[freeRegion->allocationIndex]->allocationIndex =
            freeRegion->allocationIndex;
    }

    freeRegion->allocation->freeRegionCount -= 1;

    freeRegion->allocation->freeSpace -= freeRegion->size;

    SDL_free(freeRegion);

    SDL_UnlockMutex(renderer->allocatorLock);
}

static void VULKAN_INTERNAL_NewMemoryFreeRegion(
    VulkanRenderer *renderer,
    VulkanMemoryAllocation *allocation,
    VkDeviceSize offset,
    VkDeviceSize size)
{
    VulkanMemoryFreeRegion *newFreeRegion;
    VkDeviceSize newOffset, newSize;
    Sint32 insertionIndex = 0;

    SDL_LockMutex(renderer->allocatorLock);

    // look for an adjacent region to merge
    for (Sint32 i = allocation->freeRegionCount - 1; i >= 0; i -= 1) {
        // check left side
        if (allocation->freeRegions[i]->offset + allocation->freeRegions[i]->size == offset) {
            newOffset = allocation->freeRegions[i]->offset;
            newSize = allocation->freeRegions[i]->size + size;

            VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, allocation->freeRegions[i]);
            VULKAN_INTERNAL_NewMemoryFreeRegion(renderer, allocation, newOffset, newSize);

            SDL_UnlockMutex(renderer->allocatorLock);
            return;
        }

        // check right side
        if (allocation->freeRegions[i]->offset == offset + size) {
            newOffset = offset;
            newSize = allocation->freeRegions[i]->size + size;

            VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, allocation->freeRegions[i]);
            VULKAN_INTERNAL_NewMemoryFreeRegion(renderer, allocation, newOffset, newSize);

            SDL_UnlockMutex(renderer->allocatorLock);
            return;
        }
    }

    // region is not contiguous with another free region, make a new one
    allocation->freeRegionCount += 1;
    if (allocation->freeRegionCount > allocation->freeRegionCapacity) {
        allocation->freeRegionCapacity *= 2;
        allocation->freeRegions = SDL_realloc(
            allocation->freeRegions,
            sizeof(VulkanMemoryFreeRegion *) * allocation->freeRegionCapacity);
    }

    newFreeRegion = SDL_malloc(sizeof(VulkanMemoryFreeRegion));
    newFreeRegion->offset = offset;
    newFreeRegion->size = size;
    newFreeRegion->allocation = allocation;

    allocation->freeSpace += size;

    allocation->freeRegions[allocation->freeRegionCount - 1] = newFreeRegion;
    newFreeRegion->allocationIndex = allocation->freeRegionCount - 1;

    if (allocation->availableForAllocation) {
        for (Uint32 i = 0; i < allocation->allocator->sortedFreeRegionCount; i += 1) {
            if (allocation->allocator->sortedFreeRegions[i]->size < size) {
                // this is where the new region should go
                break;
            }

            insertionIndex += 1;
        }

        if (allocation->allocator->sortedFreeRegionCount + 1 > allocation->allocator->sortedFreeRegionCapacity) {
            allocation->allocator->sortedFreeRegionCapacity *= 2;
            allocation->allocator->sortedFreeRegions = SDL_realloc(
                allocation->allocator->sortedFreeRegions,
                sizeof(VulkanMemoryFreeRegion *) * allocation->allocator->sortedFreeRegionCapacity);
        }

        // perform insertion sort
        if (allocation->allocator->sortedFreeRegionCount > 0 && insertionIndex != allocation->allocator->sortedFreeRegionCount) {
            for (Sint32 i = allocation->allocator->sortedFreeRegionCount; i > insertionIndex && i > 0; i -= 1) {
                allocation->allocator->sortedFreeRegions[i] = allocation->allocator->sortedFreeRegions[i - 1];
                allocation->allocator->sortedFreeRegions[i]->sortedIndex = i;
            }
        }

        allocation->allocator->sortedFreeRegionCount += 1;
        allocation->allocator->sortedFreeRegions[insertionIndex] = newFreeRegion;
        newFreeRegion->sortedIndex = insertionIndex;
    }

    SDL_UnlockMutex(renderer->allocatorLock);
}

static VulkanMemoryUsedRegion *VULKAN_INTERNAL_NewMemoryUsedRegion(
    VulkanRenderer *renderer,
    VulkanMemoryAllocation *allocation,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkDeviceSize resourceOffset,
    VkDeviceSize resourceSize,
    VkDeviceSize alignment)
{
    VulkanMemoryUsedRegion *memoryUsedRegion;

    SDL_LockMutex(renderer->allocatorLock);

    if (allocation->usedRegionCount == allocation->usedRegionCapacity) {
        allocation->usedRegionCapacity *= 2;
        allocation->usedRegions = SDL_realloc(
            allocation->usedRegions,
            allocation->usedRegionCapacity * sizeof(VulkanMemoryUsedRegion *));
    }

    memoryUsedRegion = SDL_malloc(sizeof(VulkanMemoryUsedRegion));
    memoryUsedRegion->allocation = allocation;
    memoryUsedRegion->offset = offset;
    memoryUsedRegion->size = size;
    memoryUsedRegion->resourceOffset = resourceOffset;
    memoryUsedRegion->resourceSize = resourceSize;
    memoryUsedRegion->alignment = alignment;

    allocation->usedSpace += size;

    allocation->usedRegions[allocation->usedRegionCount] = memoryUsedRegion;
    allocation->usedRegionCount += 1;

    SDL_UnlockMutex(renderer->allocatorLock);

    return memoryUsedRegion;
}

static void VULKAN_INTERNAL_RemoveMemoryUsedRegion(
    VulkanRenderer *renderer,
    VulkanMemoryUsedRegion *usedRegion)
{
    Uint32 i;

    SDL_LockMutex(renderer->allocatorLock);

    for (i = 0; i < usedRegion->allocation->usedRegionCount; i += 1) {
        if (usedRegion->allocation->usedRegions[i] == usedRegion) {
            // plug the hole
            if (i != usedRegion->allocation->usedRegionCount - 1) {
                usedRegion->allocation->usedRegions[i] = usedRegion->allocation->usedRegions[usedRegion->allocation->usedRegionCount - 1];
            }

            break;
        }
    }

    usedRegion->allocation->usedSpace -= usedRegion->size;

    usedRegion->allocation->usedRegionCount -= 1;

    VULKAN_INTERNAL_NewMemoryFreeRegion(
        renderer,
        usedRegion->allocation,
        usedRegion->offset,
        usedRegion->size);

    SDL_free(usedRegion);

    SDL_UnlockMutex(renderer->allocatorLock);
}

static bool VULKAN_INTERNAL_CheckMemoryTypeArrayUnique(
    Uint32 memoryTypeIndex,
    Uint32 *memoryTypeIndexArray,
    Uint32 count)
{
    Uint32 i = 0;

    for (i = 0; i < count; i += 1) {
        if (memoryTypeIndexArray[i] == memoryTypeIndex) {
            return false;
        }
    }

    return true;
}

/* Returns an array of memory type indices in order of preference.
 * Memory types are requested with the following three guidelines:
 *
 * Required: Absolutely necessary
 * Preferred: Nice to have, but not necessary
 * Tolerable: Can be allowed if there are no other options
 *
 * We return memory types in this order:
 * 1. Required and preferred. This is the best category.
 * 2. Required only.
 * 3. Required, preferred, and tolerable.
 * 4. Required and tolerable. This is the worst category.
 */
static Uint32 *VULKAN_INTERNAL_FindBestMemoryTypes(
    VulkanRenderer *renderer,
    Uint32 typeFilter,
    VkMemoryPropertyFlags requiredProperties,
    VkMemoryPropertyFlags preferredProperties,
    VkMemoryPropertyFlags tolerableProperties,
    Uint32 *pCount)
{
    Uint32 i;
    Uint32 index = 0;
    Uint32 *result = SDL_malloc(sizeof(Uint32) * renderer->memoryProperties.memoryTypeCount);

    // required + preferred + !tolerable
    for (i = 0; i < renderer->memoryProperties.memoryTypeCount; i += 1) {
        if ((typeFilter & (1 << i)) &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & preferredProperties) == preferredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & tolerableProperties) == 0) {
            if (VULKAN_INTERNAL_CheckMemoryTypeArrayUnique(
                    i,
                    result,
                    index)) {
                result[index] = i;
                index += 1;
            }
        }
    }

    // required + !preferred + !tolerable
    for (i = 0; i < renderer->memoryProperties.memoryTypeCount; i += 1) {
        if ((typeFilter & (1 << i)) &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & preferredProperties) == 0 &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & tolerableProperties) == 0) {
            if (VULKAN_INTERNAL_CheckMemoryTypeArrayUnique(
                    i,
                    result,
                    index)) {
                result[index] = i;
                index += 1;
            }
        }
    }

    // required + preferred + tolerable
    for (i = 0; i < renderer->memoryProperties.memoryTypeCount; i += 1) {
        if ((typeFilter & (1 << i)) &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & preferredProperties) == preferredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & tolerableProperties) == tolerableProperties) {
            if (VULKAN_INTERNAL_CheckMemoryTypeArrayUnique(
                    i,
                    result,
                    index)) {
                result[index] = i;
                index += 1;
            }
        }
    }

    // required + !preferred + tolerable
    for (i = 0; i < renderer->memoryProperties.memoryTypeCount; i += 1) {
        if ((typeFilter & (1 << i)) &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & preferredProperties) == 0 &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & tolerableProperties) == tolerableProperties) {
            if (VULKAN_INTERNAL_CheckMemoryTypeArrayUnique(
                    i,
                    result,
                    index)) {
                result[index] = i;
                index += 1;
            }
        }
    }

    *pCount = index;
    return result;
}

static Uint32 *VULKAN_INTERNAL_FindBestBufferMemoryTypes(
    VulkanRenderer *renderer,
    VkBuffer buffer,
    VkMemoryPropertyFlags requiredMemoryProperties,
    VkMemoryPropertyFlags preferredMemoryProperties,
    VkMemoryPropertyFlags tolerableMemoryProperties,
    VkMemoryRequirements *pMemoryRequirements,
    Uint32 *pCount)
{
    renderer->vkGetBufferMemoryRequirements(
        renderer->logicalDevice,
        buffer,
        pMemoryRequirements);

    return VULKAN_INTERNAL_FindBestMemoryTypes(
        renderer,
        pMemoryRequirements->memoryTypeBits,
        requiredMemoryProperties,
        preferredMemoryProperties,
        tolerableMemoryProperties,
        pCount);
}

static Uint32 *VULKAN_INTERNAL_FindBestImageMemoryTypes(
    VulkanRenderer *renderer,
    VkImage image,
    VkMemoryPropertyFlags preferredMemoryPropertyFlags,
    VkMemoryRequirements *pMemoryRequirements,
    Uint32 *pCount)
{
    renderer->vkGetImageMemoryRequirements(
        renderer->logicalDevice,
        image,
        pMemoryRequirements);

    return VULKAN_INTERNAL_FindBestMemoryTypes(
        renderer,
        pMemoryRequirements->memoryTypeBits,
        0,
        preferredMemoryPropertyFlags,
        0,
        pCount);
}

static void VULKAN_INTERNAL_DeallocateMemory(
    VulkanRenderer *renderer,
    VulkanMemorySubAllocator *allocator,
    Uint32 allocationIndex)
{
    Uint32 i;

    VulkanMemoryAllocation *allocation = allocator->allocations[allocationIndex];

    SDL_LockMutex(renderer->allocatorLock);

    // If this allocation was marked for defrag, cancel that
    for (i = 0; i < renderer->allocationsToDefragCount; i += 1) {
        if (allocation == renderer->allocationsToDefrag[i]) {
            renderer->allocationsToDefrag[i] = renderer->allocationsToDefrag[renderer->allocationsToDefragCount - 1];
            renderer->allocationsToDefragCount -= 1;

            break;
        }
    }

    for (i = 0; i < allocation->freeRegionCount; i += 1) {
        VULKAN_INTERNAL_RemoveMemoryFreeRegion(
            renderer,
            allocation->freeRegions[i]);
    }
    SDL_free(allocation->freeRegions);

    /* no need to iterate used regions because deallocate
     * only happens when there are 0 used regions
     */
    SDL_free(allocation->usedRegions);

    renderer->vkFreeMemory(
        renderer->logicalDevice,
        allocation->memory,
        NULL);

    SDL_DestroyMutex(allocation->memoryLock);
    SDL_free(allocation);

    if (allocationIndex != allocator->allocationCount - 1) {
        allocator->allocations[allocationIndex] = allocator->allocations[allocator->allocationCount - 1];
    }

    allocator->allocationCount -= 1;

    SDL_UnlockMutex(renderer->allocatorLock);
}

static Uint8 VULKAN_INTERNAL_AllocateMemory(
    VulkanRenderer *renderer,
    VkBuffer buffer,
    VkImage image,
    Uint32 memoryTypeIndex,
    VkDeviceSize allocationSize,
    Uint8 isHostVisible,
    VulkanMemoryAllocation **pMemoryAllocation)
{
    VulkanMemoryAllocation *allocation;
    VulkanMemorySubAllocator *allocator = &renderer->memoryAllocator->subAllocators[memoryTypeIndex];
    VkMemoryAllocateInfo allocInfo;
    VkResult result;

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = NULL;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.allocationSize = allocationSize;

    allocation = SDL_malloc(sizeof(VulkanMemoryAllocation));
    allocation->size = allocationSize;
    allocation->freeSpace = 0; // added by FreeRegions
    allocation->usedSpace = 0; // added by UsedRegions
    allocation->memoryLock = SDL_CreateMutex();

    allocator->allocationCount += 1;
    allocator->allocations = SDL_realloc(
        allocator->allocations,
        sizeof(VulkanMemoryAllocation *) * allocator->allocationCount);

    allocator->allocations[allocator->allocationCount - 1] = allocation;

    allocInfo.pNext = NULL;
    allocation->availableForAllocation = 1;

    allocation->usedRegions = SDL_malloc(sizeof(VulkanMemoryUsedRegion *));
    allocation->usedRegionCount = 0;
    allocation->usedRegionCapacity = 1;

    allocation->freeRegions = SDL_malloc(sizeof(VulkanMemoryFreeRegion *));
    allocation->freeRegionCount = 0;
    allocation->freeRegionCapacity = 1;

    allocation->allocator = allocator;

    result = renderer->vkAllocateMemory(
        renderer->logicalDevice,
        &allocInfo,
        NULL,
        &allocation->memory);

    if (result != VK_SUCCESS) {
        // Uh oh, we couldn't allocate, time to clean up
        SDL_free(allocation->freeRegions);

        allocator->allocationCount -= 1;
        allocator->allocations = SDL_realloc(
            allocator->allocations,
            sizeof(VulkanMemoryAllocation *) * allocator->allocationCount);

        SDL_free(allocation);

        return 0;
    }

    // Persistent mapping for host-visible memory
    if (isHostVisible) {
        result = renderer->vkMapMemory(
            renderer->logicalDevice,
            allocation->memory,
            0,
            VK_WHOLE_SIZE,
            0,
            (void **)&allocation->mapPointer);
        VULKAN_ERROR_CHECK(result, vkMapMemory, 0)
    } else {
        allocation->mapPointer = NULL;
    }

    VULKAN_INTERNAL_NewMemoryFreeRegion(
        renderer,
        allocation,
        0,
        allocation->size);

    *pMemoryAllocation = allocation;
    return 1;
}

static Uint8 VULKAN_INTERNAL_BindBufferMemory(
    VulkanRenderer *renderer,
    VulkanMemoryUsedRegion *usedRegion,
    VkDeviceSize alignedOffset,
    VkBuffer buffer)
{
    VkResult vulkanResult;

    SDL_LockMutex(usedRegion->allocation->memoryLock);

    vulkanResult = renderer->vkBindBufferMemory(
        renderer->logicalDevice,
        buffer,
        usedRegion->allocation->memory,
        alignedOffset);

    SDL_UnlockMutex(usedRegion->allocation->memoryLock);

    VULKAN_ERROR_CHECK(vulkanResult, vkBindBufferMemory, 0)

    return 1;
}

static Uint8 VULKAN_INTERNAL_BindImageMemory(
    VulkanRenderer *renderer,
    VulkanMemoryUsedRegion *usedRegion,
    VkDeviceSize alignedOffset,
    VkImage image)
{
    VkResult vulkanResult;

    SDL_LockMutex(usedRegion->allocation->memoryLock);

    vulkanResult = renderer->vkBindImageMemory(
        renderer->logicalDevice,
        image,
        usedRegion->allocation->memory,
        alignedOffset);

    SDL_UnlockMutex(usedRegion->allocation->memoryLock);

    VULKAN_ERROR_CHECK(vulkanResult, vkBindBufferMemory, 0)

    return 1;
}

static Uint8 VULKAN_INTERNAL_BindResourceMemory(
    VulkanRenderer *renderer,
    Uint32 memoryTypeIndex,
    VkMemoryRequirements *memoryRequirements,
    VkDeviceSize resourceSize, // may be different from requirements size!
    VkBuffer buffer,           // may be VK_NULL_HANDLE
    VkImage image,             // may be VK_NULL_HANDLE
    VulkanMemoryUsedRegion **pMemoryUsedRegion)
{
    VulkanMemoryAllocation *allocation;
    VulkanMemorySubAllocator *allocator;
    VulkanMemoryFreeRegion *region;
    VulkanMemoryFreeRegion *selectedRegion;
    VulkanMemoryUsedRegion *usedRegion;

    VkDeviceSize requiredSize, allocationSize;
    VkDeviceSize alignedOffset;
    VkDeviceSize newRegionSize, newRegionOffset;
    Uint8 isHostVisible, smallAllocation, allocationResult;
    Sint32 i;

    isHostVisible =
        (renderer->memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

    allocator = &renderer->memoryAllocator->subAllocators[memoryTypeIndex];
    requiredSize = memoryRequirements->size;
    smallAllocation = requiredSize <= SMALL_ALLOCATION_THRESHOLD;

    if ((buffer == VK_NULL_HANDLE && image == VK_NULL_HANDLE) ||
        (buffer != VK_NULL_HANDLE && image != VK_NULL_HANDLE)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "BindResourceMemory must be given either a VulkanBuffer or a VulkanTexture");
        return 0;
    }

    SDL_LockMutex(renderer->allocatorLock);

    selectedRegion = NULL;

    for (i = allocator->sortedFreeRegionCount - 1; i >= 0; i -= 1) {
        region = allocator->sortedFreeRegions[i];

        if (smallAllocation && region->allocation->size != SMALL_ALLOCATION_SIZE) {
            // region is not in a small allocation
            continue;
        }

        if (!smallAllocation && region->allocation->size == SMALL_ALLOCATION_SIZE) {
            // allocation is not small and current region is in a small allocation
            continue;
        }

        alignedOffset = VULKAN_INTERNAL_NextHighestAlignment(
            region->offset,
            memoryRequirements->alignment);

        if (alignedOffset + requiredSize <= region->offset + region->size) {
            selectedRegion = region;
            break;
        }
    }

    if (selectedRegion != NULL) {
        region = selectedRegion;
        allocation = region->allocation;

        usedRegion = VULKAN_INTERNAL_NewMemoryUsedRegion(
            renderer,
            allocation,
            region->offset,
            requiredSize + (alignedOffset - region->offset),
            alignedOffset,
            resourceSize,
            memoryRequirements->alignment);

        usedRegion->isBuffer = buffer != VK_NULL_HANDLE;

        newRegionSize = region->size - ((alignedOffset - region->offset) + requiredSize);
        newRegionOffset = alignedOffset + requiredSize;

        // remove and add modified region to re-sort
        VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, region);

        // if size is 0, no need to re-insert
        if (newRegionSize != 0) {
            VULKAN_INTERNAL_NewMemoryFreeRegion(
                renderer,
                allocation,
                newRegionOffset,
                newRegionSize);
        }

        SDL_UnlockMutex(renderer->allocatorLock);

        if (buffer != VK_NULL_HANDLE) {
            if (!VULKAN_INTERNAL_BindBufferMemory(
                    renderer,
                    usedRegion,
                    alignedOffset,
                    buffer)) {
                VULKAN_INTERNAL_RemoveMemoryUsedRegion(
                    renderer,
                    usedRegion);

                return 0;
            }
        } else if (image != VK_NULL_HANDLE) {
            if (!VULKAN_INTERNAL_BindImageMemory(
                    renderer,
                    usedRegion,
                    alignedOffset,
                    image)) {
                VULKAN_INTERNAL_RemoveMemoryUsedRegion(
                    renderer,
                    usedRegion);

                return 0;
            }
        }

        *pMemoryUsedRegion = usedRegion;
        return 1;
    }

    // No suitable free regions exist, allocate a new memory region
    if (
        renderer->allocationsToDefragCount == 0 &&
        !renderer->defragInProgress) {
        // Mark currently fragmented allocations for defrag
        VULKAN_INTERNAL_MarkAllocationsForDefrag(renderer);
    }

    if (requiredSize > SMALL_ALLOCATION_THRESHOLD) {
        // allocate a page of required size aligned to LARGE_ALLOCATION_INCREMENT increments
        allocationSize =
            VULKAN_INTERNAL_NextHighestAlignment(requiredSize, LARGE_ALLOCATION_INCREMENT);
    } else {
        allocationSize = SMALL_ALLOCATION_SIZE;
    }

    allocationResult = VULKAN_INTERNAL_AllocateMemory(
        renderer,
        buffer,
        image,
        memoryTypeIndex,
        allocationSize,
        isHostVisible,
        &allocation);

    // Uh oh, we're out of memory
    if (allocationResult == 0) {
        SDL_UnlockMutex(renderer->allocatorLock);

        // Responsibility of the caller to handle being out of memory
        return 2;
    }

    usedRegion = VULKAN_INTERNAL_NewMemoryUsedRegion(
        renderer,
        allocation,
        0,
        requiredSize,
        0,
        resourceSize,
        memoryRequirements->alignment);

    usedRegion->isBuffer = buffer != VK_NULL_HANDLE;

    region = allocation->freeRegions[0];

    newRegionOffset = region->offset + requiredSize;
    newRegionSize = region->size - requiredSize;

    VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, region);

    if (newRegionSize != 0) {
        VULKAN_INTERNAL_NewMemoryFreeRegion(
            renderer,
            allocation,
            newRegionOffset,
            newRegionSize);
    }

    SDL_UnlockMutex(renderer->allocatorLock);

    if (buffer != VK_NULL_HANDLE) {
        if (!VULKAN_INTERNAL_BindBufferMemory(
                renderer,
                usedRegion,
                0,
                buffer)) {
            VULKAN_INTERNAL_RemoveMemoryUsedRegion(
                renderer,
                usedRegion);

            return 0;
        }
    } else if (image != VK_NULL_HANDLE) {
        if (!VULKAN_INTERNAL_BindImageMemory(
                renderer,
                usedRegion,
                0,
                image)) {
            VULKAN_INTERNAL_RemoveMemoryUsedRegion(
                renderer,
                usedRegion);

            return 0;
        }
    }

    *pMemoryUsedRegion = usedRegion;
    return 1;
}

static Uint8 VULKAN_INTERNAL_BindMemoryForImage(
    VulkanRenderer *renderer,
    VkImage image,
    VulkanMemoryUsedRegion **usedRegion)
{
    Uint8 bindResult = 0;
    Uint32 memoryTypeCount = 0;
    Uint32 *memoryTypesToTry = NULL;
    Uint32 selectedMemoryTypeIndex = 0;
    Uint32 i;
    VkMemoryPropertyFlags preferredMemoryPropertyFlags;
    VkMemoryRequirements memoryRequirements;

    /* Vulkan memory types have several memory properties.
     *
     * Unlike buffers, images are always optimally stored device-local,
     * so that is the only property we prefer here.
     *
     * If memory is constrained, it is fine for the texture to not
     * be device-local.
     */
    preferredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    memoryTypesToTry = VULKAN_INTERNAL_FindBestImageMemoryTypes(
        renderer,
        image,
        preferredMemoryPropertyFlags,
        &memoryRequirements,
        &memoryTypeCount);

    for (i = 0; i < memoryTypeCount; i += 1) {
        bindResult = VULKAN_INTERNAL_BindResourceMemory(
            renderer,
            memoryTypesToTry[i],
            &memoryRequirements,
            memoryRequirements.size,
            VK_NULL_HANDLE,
            image,
            usedRegion);

        if (bindResult == 1) {
            selectedMemoryTypeIndex = memoryTypesToTry[i];
            break;
        }
    }

    SDL_free(memoryTypesToTry);

    // Check for warnings on success
    if (bindResult == 1) {
        if (!renderer->outOfDeviceLocalMemoryWarning) {
            if ((renderer->memoryProperties.memoryTypes[selectedMemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Out of device-local memory, allocating textures on host-local memory!");
                renderer->outOfDeviceLocalMemoryWarning = 1;
            }
        }
    }

    return bindResult;
}

static Uint8 VULKAN_INTERNAL_BindMemoryForBuffer(
    VulkanRenderer *renderer,
    VkBuffer buffer,
    VkDeviceSize size,
    VulkanBufferType type,
    VulkanMemoryUsedRegion **usedRegion)
{
    Uint8 bindResult = 0;
    Uint32 memoryTypeCount = 0;
    Uint32 *memoryTypesToTry = NULL;
    Uint32 selectedMemoryTypeIndex = 0;
    Uint32 i;
    VkMemoryPropertyFlags requiredMemoryPropertyFlags = 0;
    VkMemoryPropertyFlags preferredMemoryPropertyFlags = 0;
    VkMemoryPropertyFlags tolerableMemoryPropertyFlags = 0;
    VkMemoryRequirements memoryRequirements;

    /* Buffers need to be optimally bound to a memory type
     * based on their use case and the architecture of the system.
     *
     * It is important to understand the distinction between device and host.
     *
     * On a traditional high-performance desktop computer,
     * the "device" would be the GPU, and the "host" would be the CPU.
     * Memory being copied between these two must cross the PCI bus.
     * On these systems we have to be concerned about bandwidth limitations
     * and causing memory stalls, so we have taken a great deal of care
     * to structure this API to guide the client towards optimal usage.
     *
     * Other kinds of devices do not necessarily have this distinction.
     * On an iPhone or Nintendo Switch, all memory is accessible both to the
     * GPU and the CPU at all times. These kinds of systems are known as
     * UMA, or Unified Memory Architecture. A desktop computer using the
     * CPU's integrated graphics can also be thought of as UMA.
     *
     * Vulkan memory types have several memory properties.
     * The relevant memory properties are as follows:
     *
     * DEVICE_LOCAL:
     *   This memory is on-device and most efficient for device access.
     *   On UMA systems all memory is device-local.
     *   If memory is not device-local, then it is host-local.
     *
     * HOST_VISIBLE:
     *   This memory can be mapped for host access, meaning we can obtain
     *   a pointer to directly access the memory.
     *
     * HOST_COHERENT:
     *   Host-coherent memory does not require cache management operations
     *   when mapped, so we always set this alongside HOST_VISIBLE
     *   to avoid extra record keeping.
     *
     * HOST_CACHED:
     *   Host-cached memory is faster to access than uncached memory
     *   but memory of this type might not always be available.
     *
     * GPU buffers, like vertex buffers, indirect buffers, etc
     * are optimally stored in device-local memory.
     * However, if device-local memory is low, these buffers
     * can be accessed from host-local memory with a performance penalty.
     *
     * Uniform buffers must be host-visible and coherent because
     * the client uses them to quickly push small amounts of data.
     * We prefer uniform buffers to also be device-local because
     * they are accessed by shaders, but the amount of memory
     * that is both device-local and host-visible
     * is often constrained, particularly on low-end devices.
     *
     * Transfer buffers must be host-visible and coherent because
     * the client uses them to stage data to be transferred
     * to device-local memory, or to read back data transferred
     * from the device. We prefer the cache bit for performance
     * but it isn't strictly necessary. We tolerate device-local
     * memory in this situation because, as mentioned above,
     * on certain devices all memory is device-local, and even
     * though the transfer isn't strictly necessary it is still
     * useful for correctly timelining data.
     */
    if (type == VULKAN_BUFFER_TYPE_GPU) {
        preferredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else if (type == VULKAN_BUFFER_TYPE_UNIFORM) {
        requiredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        preferredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else if (type == VULKAN_BUFFER_TYPE_TRANSFER) {
        requiredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        preferredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        tolerableMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized buffer type!");
        return 0;
    }

    memoryTypesToTry = VULKAN_INTERNAL_FindBestBufferMemoryTypes(
        renderer,
        buffer,
        requiredMemoryPropertyFlags,
        preferredMemoryPropertyFlags,
        tolerableMemoryPropertyFlags,
        &memoryRequirements,
        &memoryTypeCount);

    for (i = 0; i < memoryTypeCount; i += 1) {
        bindResult = VULKAN_INTERNAL_BindResourceMemory(
            renderer,
            memoryTypesToTry[i],
            &memoryRequirements,
            size,
            buffer,
            VK_NULL_HANDLE,
            usedRegion);

        if (bindResult == 1) {
            selectedMemoryTypeIndex = memoryTypesToTry[i];
            break;
        }
    }

    SDL_free(memoryTypesToTry);

    // Check for warnings on success
    if (bindResult == 1) {
        if (type == VULKAN_BUFFER_TYPE_GPU) {
            if (!renderer->outOfDeviceLocalMemoryWarning) {
                if ((renderer->memoryProperties.memoryTypes[selectedMemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Out of device-local memory, allocating buffers on host-local memory, expect degraded performance!");
                    renderer->outOfDeviceLocalMemoryWarning = 1;
                }
            }
        } else if (type == VULKAN_BUFFER_TYPE_UNIFORM) {
            if (!renderer->outofBARMemoryWarning) {
                if ((renderer->memoryProperties.memoryTypes[selectedMemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Out of BAR memory, allocating uniform buffers on host-local memory, expect degraded performance!");
                    renderer->outofBARMemoryWarning = 1;
                }
            }
        } else if (type == VULKAN_BUFFER_TYPE_TRANSFER) {
            if (!renderer->integratedMemoryNotification) {
                if ((renderer->memoryProperties.memoryTypes[selectedMemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Integrated memory detected, allocating TransferBuffers on device-local memory!");
                    renderer->integratedMemoryNotification = 1;
                }
            }
        }
    }

    return bindResult;
}

// Resource tracking

#define ADD_TO_ARRAY_UNIQUE(resource, type, array, count, capacity) \
    Uint32 i;                                                       \
                                                                    \
    for (i = 0; i < commandBuffer->count; i += 1) {                 \
        if (commandBuffer->array[i] == resource) {                  \
            return;                                                 \
        }                                                           \
    }                                                               \
                                                                    \
    if (commandBuffer->count == commandBuffer->capacity) {          \
        commandBuffer->capacity += 1;                               \
        commandBuffer->array = SDL_realloc(                         \
            commandBuffer->array,                                   \
            commandBuffer->capacity * sizeof(type));                \
    }                                                               \
    commandBuffer->array[commandBuffer->count] = resource;          \
    commandBuffer->count += 1;

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

static void VULKAN_INTERNAL_TrackBuffer(
    VulkanCommandBuffer *commandBuffer,
    VulkanBuffer *buffer)
{
    TRACK_RESOURCE(
        buffer,
        VulkanBuffer *,
        usedBuffers,
        usedBufferCount,
        usedBufferCapacity)
}

static void VULKAN_INTERNAL_TrackTexture(
    VulkanCommandBuffer *commandBuffer,
    VulkanTexture *texture)
{
    TRACK_RESOURCE(
        texture,
        VulkanTexture *,
        usedTextures,
        usedTextureCount,
        usedTextureCapacity)
}

static void VULKAN_INTERNAL_TrackSampler(
    VulkanCommandBuffer *commandBuffer,
    VulkanSampler *sampler)
{
    TRACK_RESOURCE(
        sampler,
        VulkanSampler *,
        usedSamplers,
        usedSamplerCount,
        usedSamplerCapacity)
}

static void VULKAN_INTERNAL_TrackGraphicsPipeline(
    VulkanCommandBuffer *commandBuffer,
    VulkanGraphicsPipeline *graphicsPipeline)
{
    TRACK_RESOURCE(
        graphicsPipeline,
        VulkanGraphicsPipeline *,
        usedGraphicsPipelines,
        usedGraphicsPipelineCount,
        usedGraphicsPipelineCapacity)
}

static void VULKAN_INTERNAL_TrackComputePipeline(
    VulkanCommandBuffer *commandBuffer,
    VulkanComputePipeline *computePipeline)
{
    TRACK_RESOURCE(
        computePipeline,
        VulkanComputePipeline *,
        usedComputePipelines,
        usedComputePipelineCount,
        usedComputePipelineCapacity)
}

static void VULKAN_INTERNAL_TrackFramebuffer(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanFramebuffer *framebuffer)
{
    TRACK_RESOURCE(
        framebuffer,
        VulkanFramebuffer *,
        usedFramebuffers,
        usedFramebufferCount,
        usedFramebufferCapacity);
}

static void VULKAN_INTERNAL_TrackUniformBuffer(
    VulkanCommandBuffer *commandBuffer,
    VulkanUniformBuffer *uniformBuffer)
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
            commandBuffer->usedUniformBufferCapacity * sizeof(VulkanUniformBuffer *));
    }
    commandBuffer->usedUniformBuffers[commandBuffer->usedUniformBufferCount] = uniformBuffer;
    commandBuffer->usedUniformBufferCount += 1;

    VULKAN_INTERNAL_TrackBuffer(
        commandBuffer,
        uniformBuffer->bufferHandle->vulkanBuffer);
}

#undef TRACK_RESOURCE

// Memory Barriers

/*
 * In Vulkan, we must manually synchronize operations that write to resources on the GPU
 * so that read-after-write, write-after-read, and write-after-write hazards do not occur.
 * Additionally, textures are required to be in specific layouts for specific use cases.
 * Both of these tasks are accomplished with vkCmdPipelineBarrier.
 *
 * To insert the correct barriers, we keep track of "usage modes" for buffers and textures.
 * These indicate the current usage of that resource on the command buffer.
 * The transition from one usage mode to another indicates how the barrier should be constructed.
 *
 * Pipeline barriers cannot be inserted during a render pass, but they can be inserted
 * during a compute or copy pass.
 *
 * This means that the "default" usage mode of any given resource should be that it should be
 * ready for a graphics-read operation, because we cannot barrier during a render pass.
 * In the case where a resource is only used in compute, its default usage mode can be compute-read.
 * This strategy allows us to avoid expensive record keeping of command buffer/resource usage mode pairs,
 * and it fully covers synchronization between all combinations of stages.
 *
 * In Upload and Copy functions, we transition the resource immediately before and after the copy command.
 *
 * When binding a resource for compute, we transition when the Bind functions are called.
 * If a bind slot containing a resource is overwritten, we transition the resource in that slot back to its default.
 * When EndComputePass is called we transition all bound resources back to their default state.
 *
 * When binding a texture as a render pass attachment, we transition the resource on BeginRenderPass
 * and transition it back to its default on EndRenderPass.
 *
 * This strategy imposes certain limitations on resource usage flags.
 * For example, a texture cannot have both the SAMPLER and GRAPHICS_STORAGE usage flags,
 * because then it is imposible for the backend to infer which default usage mode the texture should use.
 *
 * Sync hazards can be detected by setting VK_KHRONOS_VALIDATION_VALIDATE_SYNC=1 when using validation layers.
 */

static void VULKAN_INTERNAL_BufferMemoryBarrier(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanBufferUsageMode sourceUsageMode,
    VulkanBufferUsageMode destinationUsageMode,
    VulkanBuffer *buffer)
{
    VkPipelineStageFlags srcStages = 0;
    VkPipelineStageFlags dstStages = 0;
    VkBufferMemoryBarrier memoryBarrier;

    memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    memoryBarrier.pNext = NULL;
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = 0;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.buffer = buffer->buffer;
    memoryBarrier.offset = 0;
    memoryBarrier.size = buffer->size;

    if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE) {
        srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION) {
        srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_VERTEX_READ) {
        srcStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_INDEX_READ) {
        srcStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_INDEX_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_INDIRECT) {
        srcStages = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_GRAPHICS_STORAGE_READ) {
        srcStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ) {
        srcStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else if (sourceUsageMode == VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE) {
        srcStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized buffer source barrier type!");
        return;
    }

    if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE) {
        dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION) {
        dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_VERTEX_READ) {
        dstStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_INDEX_READ) {
        dstStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_INDIRECT) {
        dstStages = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_GRAPHICS_STORAGE_READ) {
        dstStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ) {
        dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else if (destinationUsageMode == VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE) {
        dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized buffer destination barrier type!");
        return;
    }

    renderer->vkCmdPipelineBarrier(
        commandBuffer->commandBuffer,
        srcStages,
        dstStages,
        0,
        0,
        NULL,
        1,
        &memoryBarrier,
        0,
        NULL);

    buffer->transitioned = true;
}

static void VULKAN_INTERNAL_TextureSubresourceMemoryBarrier(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode sourceUsageMode,
    VulkanTextureUsageMode destinationUsageMode,
    VulkanTextureSubresource *textureSubresource)
{
    VkPipelineStageFlags srcStages = 0;
    VkPipelineStageFlags dstStages = 0;
    VkImageMemoryBarrier memoryBarrier;

    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.pNext = NULL;
    memoryBarrier.srcAccessMask = 0;
    memoryBarrier.dstAccessMask = 0;
    memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.image = textureSubresource->parent->image;
    memoryBarrier.subresourceRange.aspectMask = textureSubresource->parent->aspectFlags;
    memoryBarrier.subresourceRange.baseArrayLayer = textureSubresource->layer;
    memoryBarrier.subresourceRange.layerCount = 1;
    memoryBarrier.subresourceRange.baseMipLevel = textureSubresource->level;
    memoryBarrier.subresourceRange.levelCount = 1;

    if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE) {
        srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION) {
        srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_SAMPLER) {
        srcStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_GRAPHICS_STORAGE_READ) {
        srcStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ) {
        srcStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE) {
        srcStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT) {
        srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else if (sourceUsageMode == VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT) {
        srcStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized texture source barrier type!");
        return;
    }

    if (!textureSubresource->transitioned) {
        memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE) {
        dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION) {
        dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_SAMPLER) {
        dstStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_GRAPHICS_STORAGE_READ) {
        dstStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ) {
        dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE) {
        dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT) {
        dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT) {
        dstStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else if (destinationUsageMode == VULKAN_TEXTURE_USAGE_MODE_PRESENT) {
        dstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        memoryBarrier.dstAccessMask = 0;
        memoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized texture destination barrier type!");
        return;
    }

    renderer->vkCmdPipelineBarrier(
        commandBuffer->commandBuffer,
        srcStages,
        dstStages,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &memoryBarrier);

    textureSubresource->transitioned = true;
}

static VulkanBufferUsageMode VULKAN_INTERNAL_DefaultBufferUsageMode(
    VulkanBuffer *buffer)
{
    // NOTE: order matters here!

    if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_VERTEX_READ;
    } else if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_INDEX_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_INDEX_READ;
    } else if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_INDIRECT;
    } else if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_GRAPHICS_STORAGE_READ;
    } else if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ;
    } else if (buffer->usageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT) {
        return VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Buffer has no default usage mode!");
        return VULKAN_BUFFER_USAGE_MODE_VERTEX_READ;
    }
}

static VulkanTextureUsageMode VULKAN_INTERNAL_DefaultTextureUsageMode(
    VulkanTexture *texture)
{
    // NOTE: order matters here!
    // NOTE: graphics storage bits and sampler bit are mutually exclusive!

    if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_SAMPLER;
    } else if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_GRAPHICS_STORAGE_READ;
    } else if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT;
    } else if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT;
    } else if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ;
    } else if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT) {
        return VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Texture has no default usage mode!");
        return VULKAN_TEXTURE_USAGE_MODE_SAMPLER;
    }
}

static void VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanBufferUsageMode destinationUsageMode,
    VulkanBuffer *buffer)
{
    VULKAN_INTERNAL_BufferMemoryBarrier(
        renderer,
        commandBuffer,
        VULKAN_INTERNAL_DefaultBufferUsageMode(buffer),
        destinationUsageMode,
        buffer);
}

static void VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanBufferUsageMode sourceUsageMode,
    VulkanBuffer *buffer)
{
    VULKAN_INTERNAL_BufferMemoryBarrier(
        renderer,
        commandBuffer,
        sourceUsageMode,
        VULKAN_INTERNAL_DefaultBufferUsageMode(buffer),
        buffer);
}

static void VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode destinationUsageMode,
    VulkanTextureSubresource *textureSubresource)
{
    VULKAN_INTERNAL_TextureSubresourceMemoryBarrier(
        renderer,
        commandBuffer,
        VULKAN_INTERNAL_DefaultTextureUsageMode(textureSubresource->parent),
        destinationUsageMode,
        textureSubresource);
}

static void VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode destinationUsageMode,
    VulkanTexture *texture)
{
    for (Uint32 i = 0; i < texture->subresourceCount; i += 1) {
        VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
            renderer,
            commandBuffer,
            destinationUsageMode,
            &texture->subresources[i]);
    }
}

static void VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode sourceUsageMode,
    VulkanTextureSubresource *textureSubresource)
{
    VULKAN_INTERNAL_TextureSubresourceMemoryBarrier(
        renderer,
        commandBuffer,
        sourceUsageMode,
        VULKAN_INTERNAL_DefaultTextureUsageMode(textureSubresource->parent),
        textureSubresource);
}

static void VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode sourceUsageMode,
    VulkanTexture *texture)
{
    // FIXME: could optimize this barrier
    for (Uint32 i = 0; i < texture->subresourceCount; i += 1) {
        VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
            renderer,
            commandBuffer,
            sourceUsageMode,
            &texture->subresources[i]);
    }
}

// Resource Disposal

static void VULKAN_INTERNAL_ReleaseFramebuffer(
    VulkanRenderer *renderer,
    VulkanFramebuffer *framebuffer)
{
    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->framebuffersToDestroy,
        VulkanFramebuffer *,
        renderer->framebuffersToDestroyCount + 1,
        renderer->framebuffersToDestroyCapacity,
        renderer->framebuffersToDestroyCapacity * 2)

    renderer->framebuffersToDestroy[renderer->framebuffersToDestroyCount] = framebuffer;
    renderer->framebuffersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_INTERNAL_DestroyFramebuffer(
    VulkanRenderer *renderer,
    VulkanFramebuffer *framebuffer)
{
    renderer->vkDestroyFramebuffer(
        renderer->logicalDevice,
        framebuffer->framebuffer,
        NULL);

    SDL_free(framebuffer);
}

static void VULKAN_INTERNAL_RemoveFramebuffersContainingView(
    VulkanRenderer *renderer,
    VkImageView view)
{
    FramebufferHashTableKey *key;
    VulkanFramebuffer *value;
    void *iter = NULL;

    // Can't remove while iterating!
    Uint32 keysToRemoveCapacity = 8;
    Uint32 keysToRemoveCount = 0;
    FramebufferHashTableKey **keysToRemove = SDL_malloc(keysToRemoveCapacity * sizeof(FramebufferHashTableKey *));

    SDL_LockMutex(renderer->framebufferFetchLock);

    while (SDL_IterateHashTable(renderer->framebufferHashTable, (const void **)&key, (const void **)&value, &iter)) {
        bool remove = false;
        for (Uint32 i = 0; i < key->colorAttachmentCount; i += 1) {
            if (key->colorAttachmentViews[i] == view) {
                remove = true;
            }
        }
        if (key->depthStencilAttachmentView == view) {
            remove = true;
        }

        if (remove) {
            if (keysToRemoveCount == keysToRemoveCapacity) {
                keysToRemoveCapacity *= 2;
                keysToRemove = SDL_realloc(
                    keysToRemove,
                    keysToRemoveCapacity * sizeof(FramebufferHashTableKey *));
            }

            keysToRemove[keysToRemoveCount] = key;
            keysToRemoveCount += 1;
        }
    }

    for (Uint32 i = 0; i < keysToRemoveCount; i += 1) {
        SDL_RemoveFromHashTable(renderer->framebufferHashTable, (void *)keysToRemove[i]);
    }

    SDL_UnlockMutex(renderer->framebufferFetchLock);

    SDL_free(keysToRemove);
}

static void VULKAN_INTERNAL_DestroyTexture(
    VulkanRenderer *renderer,
    VulkanTexture *texture)
{
    // Clean up subresources
    for (Uint32 subresourceIndex = 0; subresourceIndex < texture->subresourceCount; subresourceIndex += 1) {
        if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) {
            for (Uint32 depthIndex = 0; depthIndex < texture->depth; depthIndex += 1) {
                VULKAN_INTERNAL_RemoveFramebuffersContainingView(
                    renderer,
                    texture->subresources[subresourceIndex].renderTargetViews[depthIndex]);
            }

            if (texture->subresources[subresourceIndex].msaaTexHandle != NULL) {
                VULKAN_INTERNAL_DestroyTexture(
                    renderer,
                    texture->subresources[subresourceIndex].msaaTexHandle->vulkanTexture);
                SDL_free(texture->subresources[subresourceIndex].msaaTexHandle);
            }

            for (Uint32 depthIndex = 0; depthIndex < texture->depth; depthIndex += 1) {
                renderer->vkDestroyImageView(
                    renderer->logicalDevice,
                    texture->subresources[subresourceIndex].renderTargetViews[depthIndex],
                    NULL);
            }
            SDL_free(texture->subresources[subresourceIndex].renderTargetViews);
        }

        if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT) {
            renderer->vkDestroyImageView(
                renderer->logicalDevice,
                texture->subresources[subresourceIndex].computeWriteView,
                NULL);
        }

        if (texture->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
            renderer->vkDestroyImageView(
                renderer->logicalDevice,
                texture->subresources[subresourceIndex].depthStencilView,
                NULL);
        }
    }

    SDL_free(texture->subresources);

    renderer->vkDestroyImageView(
        renderer->logicalDevice,
        texture->fullView,
        NULL);

    renderer->vkDestroyImage(
        renderer->logicalDevice,
        texture->image,
        NULL);

    VULKAN_INTERNAL_RemoveMemoryUsedRegion(
        renderer,
        texture->usedRegion);

    SDL_free(texture);
}

static void VULKAN_INTERNAL_DestroyBuffer(
    VulkanRenderer *renderer,
    VulkanBuffer *buffer)
{
    renderer->vkDestroyBuffer(
        renderer->logicalDevice,
        buffer->buffer,
        NULL);

    VULKAN_INTERNAL_RemoveMemoryUsedRegion(
        renderer,
        buffer->usedRegion);

    SDL_free(buffer);
}

static void VULKAN_INTERNAL_DestroyCommandPool(
    VulkanRenderer *renderer,
    VulkanCommandPool *commandPool)
{
    Uint32 i;
    VulkanCommandBuffer *commandBuffer;

    renderer->vkDestroyCommandPool(
        renderer->logicalDevice,
        commandPool->commandPool,
        NULL);

    for (i = 0; i < commandPool->inactiveCommandBufferCount; i += 1) {
        commandBuffer = commandPool->inactiveCommandBuffers[i];

        SDL_free(commandBuffer->presentDatas);
        SDL_free(commandBuffer->waitSemaphores);
        SDL_free(commandBuffer->signalSemaphores);
        SDL_free(commandBuffer->boundDescriptorSetDatas);
        SDL_free(commandBuffer->usedBuffers);
        SDL_free(commandBuffer->usedTextures);
        SDL_free(commandBuffer->usedSamplers);
        SDL_free(commandBuffer->usedGraphicsPipelines);
        SDL_free(commandBuffer->usedComputePipelines);
        SDL_free(commandBuffer->usedFramebuffers);
        SDL_free(commandBuffer->usedUniformBuffers);

        SDL_free(commandBuffer);
    }

    SDL_free(commandPool->inactiveCommandBuffers);
    SDL_free(commandPool);
}

static void VULKAN_INTERNAL_DestroyDescriptorSetPool(
    VulkanRenderer *renderer,
    DescriptorSetPool *pool)
{
    Uint32 i;

    if (pool == NULL) {
        return;
    }

    for (i = 0; i < pool->descriptorPoolCount; i += 1) {
        renderer->vkDestroyDescriptorPool(
            renderer->logicalDevice,
            pool->descriptorPools[i],
            NULL);
    }

    renderer->vkDestroyDescriptorSetLayout(
        renderer->logicalDevice,
        pool->descriptorSetLayout,
        NULL);

    SDL_free(pool->descriptorInfos);
    SDL_free(pool->descriptorPools);
    SDL_free(pool->inactiveDescriptorSets);
    SDL_DestroyMutex(pool->lock);
}

static void VULKAN_INTERNAL_DestroyGraphicsPipeline(
    VulkanRenderer *renderer,
    VulkanGraphicsPipeline *graphicsPipeline)
{
    Uint32 i;

    renderer->vkDestroyPipeline(
        renderer->logicalDevice,
        graphicsPipeline->pipeline,
        NULL);

    renderer->vkDestroyPipelineLayout(
        renderer->logicalDevice,
        graphicsPipeline->resourceLayout.pipelineLayout,
        NULL);

    for (i = 0; i < 4; i += 1) {
        VULKAN_INTERNAL_DestroyDescriptorSetPool(
            renderer,
            &graphicsPipeline->resourceLayout.descriptorSetPools[i]);
    }

    (void)SDL_AtomicDecRef(&graphicsPipeline->vertexShader->referenceCount);
    (void)SDL_AtomicDecRef(&graphicsPipeline->fragmentShader->referenceCount);

    SDL_free(graphicsPipeline);
}

static void VULKAN_INTERNAL_DestroyComputePipeline(
    VulkanRenderer *renderer,
    VulkanComputePipeline *computePipeline)
{
    Uint32 i;

    renderer->vkDestroyPipeline(
        renderer->logicalDevice,
        computePipeline->pipeline,
        NULL);

    renderer->vkDestroyPipelineLayout(
        renderer->logicalDevice,
        computePipeline->resourceLayout.pipelineLayout,
        NULL);

    for (i = 0; i < 3; i += 1) {
        VULKAN_INTERNAL_DestroyDescriptorSetPool(
            renderer,
            &computePipeline->resourceLayout.descriptorSetPools[i]);
    }

    renderer->vkDestroyShaderModule(
        renderer->logicalDevice,
        computePipeline->shaderModule,
        NULL);

    SDL_free(computePipeline);
}

static void VULKAN_INTERNAL_DestroyShader(
    VulkanRenderer *renderer,
    VulkanShader *vulkanShader)
{
    renderer->vkDestroyShaderModule(
        renderer->logicalDevice,
        vulkanShader->shaderModule,
        NULL);

    SDL_free((void *)vulkanShader->entryPointName);
    SDL_free(vulkanShader);
}

static void VULKAN_INTERNAL_DestroySampler(
    VulkanRenderer *renderer,
    VulkanSampler *vulkanSampler)
{
    renderer->vkDestroySampler(
        renderer->logicalDevice,
        vulkanSampler->sampler,
        NULL);

    SDL_free(vulkanSampler);
}

static void VULKAN_INTERNAL_DestroySwapchain(
    VulkanRenderer *renderer,
    WindowData *windowData)
{
    Uint32 i;
    VulkanSwapchainData *swapchainData;

    if (windowData == NULL) {
        return;
    }

    swapchainData = windowData->swapchainData;

    if (swapchainData == NULL) {
        return;
    }

    for (i = 0; i < swapchainData->imageCount; i += 1) {
        VULKAN_INTERNAL_RemoveFramebuffersContainingView(
            renderer,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].renderTargetViews[0]);
        renderer->vkDestroyImageView(
            renderer->logicalDevice,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].renderTargetViews[0],
            NULL);
        SDL_free(swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].renderTargetViews);
        SDL_free(swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources);

        SDL_free(swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture);
        SDL_free(swapchainData->textureContainers[i].activeTextureHandle);
    }

    SDL_free(swapchainData->textureContainers);

    renderer->vkDestroySwapchainKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        NULL);

    renderer->vkDestroySurfaceKHR(
        renderer->instance,
        swapchainData->surface,
        NULL);

    for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        renderer->vkDestroySemaphore(
            renderer->logicalDevice,
            swapchainData->imageAvailableSemaphore[i],
            NULL);

        renderer->vkDestroySemaphore(
            renderer->logicalDevice,
            swapchainData->renderFinishedSemaphore[i],
            NULL);
    }

    windowData->swapchainData = NULL;
    SDL_free(swapchainData);
}

// Hashtable functions

static Uint32 VULKAN_INTERNAL_CommandPoolHashFunction(const void *key, void *data)
{
    return (Uint32)((CommandPoolHashTableKey *)key)->threadID;
}

static bool VULKAN_INTERNAL_CommandPoolHashKeyMatch(const void *aKey, const void *bKey, void *data)
{
    CommandPoolHashTableKey *a = (CommandPoolHashTableKey *)aKey;
    CommandPoolHashTableKey *b = (CommandPoolHashTableKey *)bKey;
    return a->threadID == b->threadID;
}

static void VULKAN_INTERNAL_CommandPoolHashNuke(const void *key, const void *value, void *data)
{
    VulkanRenderer *renderer = (VulkanRenderer *)data;
    VulkanCommandPool *pool = (VulkanCommandPool *)value;
    VULKAN_INTERNAL_DestroyCommandPool(renderer, pool);
    SDL_free((void *)key);
}

static Uint32 VULKAN_INTERNAL_RenderPassHashFunction(
    const void *key,
    void *data)
{
    RenderPassHashTableKey *hashTableKey = (RenderPassHashTableKey *)key;

    /* The algorithm for this hashing function
     * is taken from Josh Bloch's "Effective Java".
     * (https://stackoverflow.com/a/113600/12492383)
     */
    const Uint32 HASH_FACTOR = 31;
    Uint32 result = 1;

    for (Uint32 i = 0; i < hashTableKey->colorAttachmentCount; i += 1) {
        result = result * HASH_FACTOR + hashTableKey->colorTargetDescriptions[i].loadOp;
        result = result * HASH_FACTOR + hashTableKey->colorTargetDescriptions[i].storeOp;
        result = result * HASH_FACTOR + hashTableKey->colorTargetDescriptions[i].format;
    }

    result = result * HASH_FACTOR + hashTableKey->depthStencilTargetDescription.loadOp;
    result = result * HASH_FACTOR + hashTableKey->depthStencilTargetDescription.storeOp;
    result = result * HASH_FACTOR + hashTableKey->depthStencilTargetDescription.stencilLoadOp;
    result = result * HASH_FACTOR + hashTableKey->depthStencilTargetDescription.stencilStoreOp;
    result = result * HASH_FACTOR + hashTableKey->depthStencilTargetDescription.format;

    result = result * HASH_FACTOR + hashTableKey->colorAttachmentSampleCount;

    return result;
}

static bool VULKAN_INTERNAL_RenderPassHashKeyMatch(
    const void *aKey,
    const void *bKey,
    void *data)
{
    RenderPassHashTableKey *a = (RenderPassHashTableKey *)aKey;
    RenderPassHashTableKey *b = (RenderPassHashTableKey *)bKey;

    if (a->colorAttachmentCount != b->colorAttachmentCount) {
        return 0;
    }

    if (a->colorAttachmentSampleCount != b->colorAttachmentSampleCount) {
        return 0;
    }

    for (Uint32 i = 0; i < a->colorAttachmentCount; i += 1) {
        if (a->colorTargetDescriptions[i].format != b->colorTargetDescriptions[i].format) {
            return 0;
        }

        if (a->colorTargetDescriptions[i].loadOp != b->colorTargetDescriptions[i].loadOp) {
            return 0;
        }

        if (a->colorTargetDescriptions[i].storeOp != b->colorTargetDescriptions[i].storeOp) {
            return 0;
        }
    }

    if (a->depthStencilTargetDescription.format != b->depthStencilTargetDescription.format) {
        return 0;
    }

    if (a->depthStencilTargetDescription.loadOp != b->depthStencilTargetDescription.loadOp) {
        return 0;
    }

    if (a->depthStencilTargetDescription.storeOp != b->depthStencilTargetDescription.storeOp) {
        return 0;
    }

    if (a->depthStencilTargetDescription.stencilLoadOp != b->depthStencilTargetDescription.stencilLoadOp) {
        return 0;
    }

    if (a->depthStencilTargetDescription.stencilStoreOp != b->depthStencilTargetDescription.stencilStoreOp) {
        return 0;
    }

    return 1;
}

static void VULKAN_INTERNAL_RenderPassHashNuke(const void *key, const void *value, void *data)
{
    VulkanRenderer *renderer = (VulkanRenderer *)data;
    VulkanRenderPassHashTableValue *renderPassWrapper = (VulkanRenderPassHashTableValue *)value;
    renderer->vkDestroyRenderPass(
        renderer->logicalDevice,
        renderPassWrapper->handle,
        NULL);
    SDL_free(renderPassWrapper);
    SDL_free((void *)key);
}

static Uint32 VULKAN_INTERNAL_FramebufferHashFunction(
    const void *key,
    void *data)
{
    FramebufferHashTableKey *hashTableKey = (FramebufferHashTableKey *)key;

    /* The algorithm for this hashing function
     * is taken from Josh Bloch's "Effective Java".
     * (https://stackoverflow.com/a/113600/12492383)
     */
    const Uint32 HASH_FACTOR = 31;
    Uint32 result = 1;

    for (Uint32 i = 0; i < hashTableKey->colorAttachmentCount; i += 1) {
        result = result * HASH_FACTOR + (Uint32)(uintptr_t)hashTableKey->colorAttachmentViews[i];
        result = result * HASH_FACTOR + (Uint32)(uintptr_t)hashTableKey->colorMultiSampleAttachmentViews[i];
    }

    result = result * HASH_FACTOR + (Uint32)(uintptr_t)hashTableKey->depthStencilAttachmentView;
    result = result * HASH_FACTOR + hashTableKey->width;
    result = result * HASH_FACTOR + hashTableKey->height;

    return result;
}

static bool VULKAN_INTERNAL_FramebufferHashKeyMatch(
    const void *aKey,
    const void *bKey,
    void *data)
{
    FramebufferHashTableKey *a = (FramebufferHashTableKey *)aKey;
    FramebufferHashTableKey *b = (FramebufferHashTableKey *)bKey;

    if (a->colorAttachmentCount != b->colorAttachmentCount) {
        return 0;
    }

    for (Uint32 i = 0; i < a->colorAttachmentCount; i += 1) {
        if (a->colorAttachmentViews[i] != b->colorAttachmentViews[i]) {
            return 0;
        }

        if (a->colorMultiSampleAttachmentViews[i] != b->colorMultiSampleAttachmentViews[i]) {
            return 0;
        }
    }

    if (a->depthStencilAttachmentView != b->depthStencilAttachmentView) {
        return 0;
    }

    if (a->width != b->width) {
        return 0;
    }

    if (a->height != b->height) {
        return 0;
    }

    return 1;
}

static void VULKAN_INTERNAL_FramebufferHashNuke(const void *key, const void *value, void *data)
{
    VulkanRenderer *renderer = (VulkanRenderer *)data;
    VulkanFramebuffer *framebuffer = (VulkanFramebuffer *)value;
    VULKAN_INTERNAL_ReleaseFramebuffer(renderer, framebuffer);
    SDL_free((void *)key);
}

// Descriptor pool stuff

static bool VULKAN_INTERNAL_CreateDescriptorPool(
    VulkanRenderer *renderer,
    VulkanDescriptorInfo *descriptorInfos,
    Uint32 descriptorInfoCount,
    Uint32 descriptorSetPoolSize,
    VkDescriptorPool *pDescriptorPool)
{
    VkDescriptorPoolSize *descriptorPoolSizes;
    VkDescriptorPoolCreateInfo descriptorPoolInfo;
    VkResult vulkanResult;
    Uint32 i;

    descriptorPoolSizes = NULL;

    if (descriptorInfoCount > 0) {
        descriptorPoolSizes = SDL_stack_alloc(VkDescriptorPoolSize, descriptorInfoCount);

        for (i = 0; i < descriptorInfoCount; i += 1) {
            descriptorPoolSizes[i].type = descriptorInfos[i].descriptorType;
            descriptorPoolSizes[i].descriptorCount = descriptorSetPoolSize;
        }
    }

    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = NULL;
    descriptorPoolInfo.flags = 0;
    descriptorPoolInfo.maxSets = descriptorSetPoolSize;
    descriptorPoolInfo.poolSizeCount = descriptorInfoCount;
    descriptorPoolInfo.pPoolSizes = descriptorPoolSizes;

    vulkanResult = renderer->vkCreateDescriptorPool(
        renderer->logicalDevice,
        &descriptorPoolInfo,
        NULL,
        pDescriptorPool);

    SDL_stack_free(descriptorPoolSizes);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorPool", vulkanResult);
        return false;
    }

    return true;
}

static bool VULKAN_INTERNAL_AllocateDescriptorSets(
    VulkanRenderer *renderer,
    VkDescriptorPool descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    Uint32 descriptorSetCount,
    VkDescriptorSet *descriptorSetArray)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    VkDescriptorSetLayout *descriptorSetLayouts = SDL_stack_alloc(VkDescriptorSetLayout, descriptorSetCount);
    VkResult vulkanResult;
    Uint32 i;

    for (i = 0; i < descriptorSetCount; i += 1) {
        descriptorSetLayouts[i] = descriptorSetLayout;
    }

    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = NULL;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts;

    vulkanResult = renderer->vkAllocateDescriptorSets(
        renderer->logicalDevice,
        &descriptorSetAllocateInfo,
        descriptorSetArray);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkAllocateDescriptorSets", vulkanResult);
        SDL_stack_free(descriptorSetLayouts);
        return false;
    }

    SDL_stack_free(descriptorSetLayouts);
    return true;
}

static void VULKAN_INTERNAL_InitializeDescriptorSetPool(
    VulkanRenderer *renderer,
    DescriptorSetPool *descriptorSetPool)
{
    descriptorSetPool->lock = SDL_CreateMutex();

    // Descriptor set layout and descriptor infos are already set when this function is called

    descriptorSetPool->descriptorPoolCount = 1;
    descriptorSetPool->descriptorPools = SDL_malloc(sizeof(VkDescriptorPool));
    descriptorSetPool->nextPoolSize = DESCRIPTOR_POOL_STARTING_SIZE * 2;

    VULKAN_INTERNAL_CreateDescriptorPool(
        renderer,
        descriptorSetPool->descriptorInfos,
        descriptorSetPool->descriptorInfoCount,
        DESCRIPTOR_POOL_STARTING_SIZE,
        &descriptorSetPool->descriptorPools[0]);

    descriptorSetPool->inactiveDescriptorSetCapacity = DESCRIPTOR_POOL_STARTING_SIZE;
    descriptorSetPool->inactiveDescriptorSetCount = DESCRIPTOR_POOL_STARTING_SIZE;
    descriptorSetPool->inactiveDescriptorSets = SDL_malloc(
        sizeof(VkDescriptorSet) * DESCRIPTOR_POOL_STARTING_SIZE);

    VULKAN_INTERNAL_AllocateDescriptorSets(
        renderer,
        descriptorSetPool->descriptorPools[0],
        descriptorSetPool->descriptorSetLayout,
        DESCRIPTOR_POOL_STARTING_SIZE,
        descriptorSetPool->inactiveDescriptorSets);
}

static bool VULKAN_INTERNAL_InitializeGraphicsPipelineResourceLayout(
    VulkanRenderer *renderer,
    VulkanShader *vertexShader,
    VulkanShader *fragmentShader,
    VulkanGraphicsPipelineResourceLayout *pipelineResourceLayout)
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[MAX_TEXTURE_SAMPLERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE + MAX_STORAGE_BUFFERS_PER_STAGE];
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    VkDescriptorSetLayout descriptorSetLayouts[4];
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    DescriptorSetPool *descriptorSetPool;
    VkResult vulkanResult;
    Uint32 i;

    pipelineResourceLayout->vertexSamplerCount = vertexShader->samplerCount;
    pipelineResourceLayout->vertexStorageTextureCount = vertexShader->storageTextureCount;
    pipelineResourceLayout->vertexStorageBufferCount = vertexShader->storageBufferCount;
    pipelineResourceLayout->vertexUniformBufferCount = vertexShader->uniformBufferCount;

    pipelineResourceLayout->fragmentSamplerCount = fragmentShader->samplerCount;
    pipelineResourceLayout->fragmentStorageTextureCount = fragmentShader->storageTextureCount;
    pipelineResourceLayout->fragmentStorageBufferCount = fragmentShader->storageBufferCount;
    pipelineResourceLayout->fragmentUniformBufferCount = fragmentShader->uniformBufferCount;

    // Vertex Resources

    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = NULL;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.pBindings = NULL;
    descriptorSetLayoutCreateInfo.bindingCount =
        vertexShader->samplerCount +
        vertexShader->storageTextureCount +
        vertexShader->storageBufferCount;

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[0];

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < vertexShader->samplerCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_VERTEX_BIT;
        }

        for (i = vertexShader->samplerCount; i < vertexShader->samplerCount + vertexShader->storageTextureCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_VERTEX_BIT;
        }

        for (i = vertexShader->samplerCount + vertexShader->storageTextureCount; i < descriptorSetLayoutCreateInfo.bindingCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_VERTEX_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[0] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Vertex UBOs

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[1];

    descriptorSetLayoutCreateInfo.bindingCount = pipelineResourceLayout->vertexUniformBufferCount;
    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < vertexShader->uniformBufferCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_VERTEX_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[1] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Fragment resources

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[2];

    descriptorSetLayoutCreateInfo.bindingCount =
        fragmentShader->samplerCount +
        fragmentShader->storageTextureCount +
        fragmentShader->storageBufferCount;

    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < fragmentShader->samplerCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        for (i = fragmentShader->samplerCount; i < fragmentShader->samplerCount + fragmentShader->storageTextureCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        for (i = fragmentShader->samplerCount + fragmentShader->storageTextureCount; i < descriptorSetLayoutCreateInfo.bindingCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[2] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Fragment UBOs

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[3];

    descriptorSetLayoutCreateInfo.bindingCount =
        pipelineResourceLayout->fragmentUniformBufferCount;

    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < fragmentShader->uniformBufferCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[3] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Create the pipeline layout

    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 4;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    vulkanResult = renderer->vkCreatePipelineLayout(
        renderer->logicalDevice,
        &pipelineLayoutCreateInfo,
        NULL,
        &pipelineResourceLayout->pipelineLayout);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreatePipelineLayout", vulkanResult);
        return false;
    }

    for (i = 0; i < 4; i += 1) {
        VULKAN_INTERNAL_InitializeDescriptorSetPool(
            renderer,
            &pipelineResourceLayout->descriptorSetPools[i]);
    }

    return true;
}

static bool VULKAN_INTERNAL_InitializeComputePipelineResourceLayout(
    VulkanRenderer *renderer,
    SDL_GPUComputePipelineCreateInfo *pipelineCreateInfo,
    VulkanComputePipelineResourceLayout *pipelineResourceLayout)
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[MAX_UNIFORM_BUFFERS_PER_STAGE];
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    VkDescriptorSetLayout descriptorSetLayouts[3];
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    DescriptorSetPool *descriptorSetPool;
    VkResult vulkanResult;
    Uint32 i;

    pipelineResourceLayout->readOnlyStorageTextureCount = pipelineCreateInfo->readOnlyStorageTextureCount;
    pipelineResourceLayout->readOnlyStorageBufferCount = pipelineCreateInfo->readOnlyStorageBufferCount;
    pipelineResourceLayout->writeOnlyStorageTextureCount = pipelineCreateInfo->writeOnlyStorageTextureCount;
    pipelineResourceLayout->writeOnlyStorageBufferCount = pipelineCreateInfo->writeOnlyStorageBufferCount;
    pipelineResourceLayout->uniformBufferCount = pipelineCreateInfo->uniformBufferCount;

    // Read-only resources

    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = NULL;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.pBindings = NULL;
    descriptorSetLayoutCreateInfo.bindingCount =
        pipelineCreateInfo->readOnlyStorageTextureCount +
        pipelineCreateInfo->readOnlyStorageBufferCount;

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[0];

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < pipelineCreateInfo->readOnlyStorageTextureCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        for (i = pipelineCreateInfo->readOnlyStorageTextureCount; i < descriptorSetLayoutCreateInfo.bindingCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[0] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Write-only resources

    descriptorSetLayoutCreateInfo.bindingCount =
        pipelineCreateInfo->writeOnlyStorageTextureCount +
        pipelineCreateInfo->writeOnlyStorageBufferCount;

    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[1];

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < pipelineCreateInfo->writeOnlyStorageTextureCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        for (i = pipelineCreateInfo->writeOnlyStorageTextureCount; i < descriptorSetLayoutCreateInfo.bindingCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[1] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Uniform buffers

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[2];

    descriptorSetLayoutCreateInfo.bindingCount = pipelineCreateInfo->uniformBufferCount;
    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < pipelineCreateInfo->uniformBufferCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    }

    vulkanResult = renderer->vkCreateDescriptorSetLayout(
        renderer->logicalDevice,
        &descriptorSetLayoutCreateInfo,
        NULL,
        &descriptorSetPool->descriptorSetLayout);

    descriptorSetLayouts[2] = descriptorSetPool->descriptorSetLayout;

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateDescriptorSetLayout", vulkanResult);
        return false;
    }

    // Create the pipeline layout

    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 3;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    vulkanResult = renderer->vkCreatePipelineLayout(
        renderer->logicalDevice,
        &pipelineLayoutCreateInfo,
        NULL,
        &pipelineResourceLayout->pipelineLayout);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreatePipelineLayout", vulkanResult);
        return false;
    }

    for (i = 0; i < 3; i += 1) {
        VULKAN_INTERNAL_InitializeDescriptorSetPool(
            renderer,
            &pipelineResourceLayout->descriptorSetPools[i]);
    }

    return true;
}

// Data Buffer

static VulkanBuffer *VULKAN_INTERNAL_CreateBuffer(
    VulkanRenderer *renderer,
    VkDeviceSize size,
    SDL_GPUBufferUsageFlags usageFlags,
    VulkanBufferType type)
{
    VulkanBuffer *buffer;
    VkResult vulkanResult;
    VkBufferCreateInfo bufferCreateInfo;
    VkBufferUsageFlags vulkanUsageFlags = 0;
    Uint8 bindResult;

    if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX_BIT) {
        vulkanUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX_BIT) {
        vulkanUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (usageFlags & (SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
        vulkanUsageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT) {
        vulkanUsageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    if (type == VULKAN_BUFFER_TYPE_UNIFORM) {
        vulkanUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    } else {
        // GPU buffers need transfer bits for defrag, transfer buffers need them for transfers
        vulkanUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    buffer = SDL_malloc(sizeof(VulkanBuffer));

    buffer->size = size;
    buffer->usageFlags = usageFlags;
    buffer->type = type;
    buffer->markedForDestroy = 0;
    buffer->transitioned = false;

    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = NULL;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = vulkanUsageFlags;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &renderer->queueFamilyIndex;

    // Set transfer bits so we can defrag
    bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vulkanResult = renderer->vkCreateBuffer(
        renderer->logicalDevice,
        &bufferCreateInfo,
        NULL,
        &buffer->buffer);
    VULKAN_ERROR_CHECK(vulkanResult, vkCreateBuffer, 0)

    bindResult = VULKAN_INTERNAL_BindMemoryForBuffer(
        renderer,
        buffer->buffer,
        buffer->size,
        buffer->type,
        &buffer->usedRegion);

    if (bindResult != 1) {
        renderer->vkDestroyBuffer(
            renderer->logicalDevice,
            buffer->buffer,
            NULL);

        return NULL;
    }

    buffer->usedRegion->vulkanBuffer = buffer; // lol
    buffer->handle = NULL;

    SDL_AtomicSet(&buffer->referenceCount, 0);

    return buffer;
}

// Indirection so we can cleanly defrag buffers
static VulkanBufferHandle *VULKAN_INTERNAL_CreateBufferHandle(
    VulkanRenderer *renderer,
    VkDeviceSize sizeInBytes,
    SDL_GPUBufferUsageFlags usageFlags,
    VulkanBufferType type)
{
    VulkanBufferHandle *bufferHandle;
    VulkanBuffer *buffer;

    buffer = VULKAN_INTERNAL_CreateBuffer(
        renderer,
        sizeInBytes,
        usageFlags,
        type);

    if (buffer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create buffer!");
        return NULL;
    }

    bufferHandle = SDL_malloc(sizeof(VulkanBufferHandle));
    bufferHandle->vulkanBuffer = buffer;
    bufferHandle->container = NULL;

    buffer->handle = bufferHandle;

    return bufferHandle;
}

static VulkanBufferContainer *VULKAN_INTERNAL_CreateBufferContainer(
    VulkanRenderer *renderer,
    VkDeviceSize sizeInBytes,
    SDL_GPUBufferUsageFlags usageFlags,
    VulkanBufferType type)
{
    VulkanBufferContainer *bufferContainer;
    VulkanBufferHandle *bufferHandle;

    bufferHandle = VULKAN_INTERNAL_CreateBufferHandle(
        renderer,
        sizeInBytes,
        usageFlags,
        type);

    if (bufferHandle == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create buffer container!");
        return NULL;
    }

    bufferContainer = SDL_malloc(sizeof(VulkanBufferContainer));

    bufferContainer->activeBufferHandle = bufferHandle;
    bufferHandle->container = bufferContainer;

    bufferContainer->bufferCapacity = 1;
    bufferContainer->bufferCount = 1;
    bufferContainer->bufferHandles = SDL_malloc(
        bufferContainer->bufferCapacity * sizeof(VulkanBufferHandle *));
    bufferContainer->bufferHandles[0] = bufferContainer->activeBufferHandle;
    bufferContainer->debugName = NULL;

    return bufferContainer;
}

// Texture Subresource Utilities

static Uint32 VULKAN_INTERNAL_GetTextureSubresourceIndex(
    Uint32 mipLevel,
    Uint32 layer,
    Uint32 numLevels)
{
    return mipLevel + (layer * numLevels);
}

static VulkanTextureSubresource *VULKAN_INTERNAL_FetchTextureSubresource(
    VulkanTextureContainer *textureContainer,
    Uint32 layer,
    Uint32 level)
{
    Uint32 index = VULKAN_INTERNAL_GetTextureSubresourceIndex(
        level,
        layer,
        textureContainer->header.info.levelCount);

    return &textureContainer->activeTextureHandle->vulkanTexture->subresources[index];
}

static void VULKAN_INTERNAL_CreateRenderTargetView(
    VulkanRenderer *renderer,
    VulkanTexture *texture,
    Uint32 layerOrDepth,
    Uint32 level,
    VkComponentMapping swizzle,
    VkImageView *pView)
{
    VkResult vulkanResult;
    VkImageViewCreateInfo imageViewCreateInfo;

    // create framebuffer compatible views for RenderTarget
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = texture->image;
    imageViewCreateInfo.format = texture->format;
    imageViewCreateInfo.components = swizzle;
    imageViewCreateInfo.subresourceRange.aspectMask = texture->aspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = level;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = layerOrDepth;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    vulkanResult = renderer->vkCreateImageView(
        renderer->logicalDevice,
        &imageViewCreateInfo,
        NULL,
        pView);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError(
            "vkCreateImageView",
            vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create color attachment image view");
        *pView = (VkImageView)VK_NULL_HANDLE;
        return;
    }
}

static void VULKAN_INTERNAL_CreateSubresourceView(
    VulkanRenderer *renderer,
    VulkanTexture *texture,
    Uint32 layer,
    Uint32 level,
    VkComponentMapping swizzle,
    VkImageView *pView)
{
    VkResult vulkanResult;
    VkImageViewCreateInfo imageViewCreateInfo;

    // create framebuffer compatible views for RenderTarget
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = texture->image;
    imageViewCreateInfo.format = texture->format;
    imageViewCreateInfo.components = swizzle;
    imageViewCreateInfo.subresourceRange.aspectMask = texture->aspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = level;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = layer;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = texture->depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;

    vulkanResult = renderer->vkCreateImageView(
        renderer->logicalDevice,
        &imageViewCreateInfo,
        NULL,
        pView);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError(
            "vkCreateImageView",
            vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create color attachment image view");
        *pView = (VkImageView)VK_NULL_HANDLE;
        return;
    }
}

// Swapchain

static Uint8 VULKAN_INTERNAL_QuerySwapchainSupport(
    VulkanRenderer *renderer,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    SwapchainSupportDetails *outputDetails)
{
    VkResult result;
    VkBool32 supportsPresent;

    renderer->vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice,
        renderer->queueFamilyIndex,
        surface,
        &supportsPresent);

    // Initialize these in case anything fails
    outputDetails->formatsLength = 0;
    outputDetails->presentModesLength = 0;

    if (!supportsPresent) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "This surface does not support presenting!");
        return 0;
    }

    // Run the device surface queries
    result = renderer->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice,
        surface,
        &outputDetails->capabilities);
    VULKAN_ERROR_CHECK(result, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, 0)

    if (!(outputDetails->capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Opaque presentation unsupported! Expect weird transparency bugs!");
    }

    result = renderer->vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice,
        surface,
        &outputDetails->formatsLength,
        NULL);
    VULKAN_ERROR_CHECK(result, vkGetPhysicalDeviceSurfaceFormatsKHR, 0)
    result = renderer->vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice,
        surface,
        &outputDetails->presentModesLength,
        NULL);
    VULKAN_ERROR_CHECK(result, vkGetPhysicalDeviceSurfacePresentModesKHR, 0)

    // Generate the arrays, if applicable

    outputDetails->formats = NULL;
    if (outputDetails->formatsLength != 0) {
        outputDetails->formats = (VkSurfaceFormatKHR *)SDL_malloc(
            sizeof(VkSurfaceFormatKHR) * outputDetails->formatsLength);

        if (!outputDetails->formats) {
            return 0;
        }

        result = renderer->vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &outputDetails->formatsLength,
            outputDetails->formats);
        if (result != VK_SUCCESS) {
            SDL_LogError(
                SDL_LOG_CATEGORY_GPU,
                "vkGetPhysicalDeviceSurfaceFormatsKHR: %s",
                VkErrorMessages(result));

            SDL_free(outputDetails->formats);
            return 0;
        }
    }

    outputDetails->presentModes = NULL;
    if (outputDetails->presentModesLength != 0) {
        outputDetails->presentModes = (VkPresentModeKHR *)SDL_malloc(
            sizeof(VkPresentModeKHR) * outputDetails->presentModesLength);

        if (!outputDetails->presentModes) {
            SDL_free(outputDetails->formats);
            return 0;
        }

        result = renderer->vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &outputDetails->presentModesLength,
            outputDetails->presentModes);
        if (result != VK_SUCCESS) {
            SDL_LogError(
                SDL_LOG_CATEGORY_GPU,
                "vkGetPhysicalDeviceSurfacePresentModesKHR: %s",
                VkErrorMessages(result));

            SDL_free(outputDetails->formats);
            SDL_free(outputDetails->presentModes);
            return 0;
        }
    }

    /* If we made it here, all the queries were successful. This does NOT
     * necessarily mean there are any supported formats or present modes!
     */
    return 1;
}

static bool VULKAN_INTERNAL_VerifySwapSurfaceFormat(
    VkFormat desiredFormat,
    VkColorSpaceKHR desiredColorSpace,
    VkSurfaceFormatKHR *availableFormats,
    Uint32 availableFormatsLength)
{
    Uint32 i;
    for (i = 0; i < availableFormatsLength; i += 1) {
        if (availableFormats[i].format == desiredFormat &&
            availableFormats[i].colorSpace == desiredColorSpace) {
            return true;
        }
    }
    return false;
}

static bool VULKAN_INTERNAL_VerifySwapPresentMode(
    VkPresentModeKHR presentMode,
    VkPresentModeKHR *availablePresentModes,
    Uint32 availablePresentModesLength)
{
    Uint32 i;
    for (i = 0; i < availablePresentModesLength; i += 1) {
        if (availablePresentModes[i] == presentMode) {
            return true;
        }
    }
    return false;
}

static bool VULKAN_INTERNAL_CreateSwapchain(
    VulkanRenderer *renderer,
    WindowData *windowData)
{
    VkResult vulkanResult;
    VulkanSwapchainData *swapchainData;
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    VkImage *swapchainImages;
    VkSemaphoreCreateInfo semaphoreCreateInfo;
    SwapchainSupportDetails swapchainSupportDetails;
    bool hasValidSwapchainComposition, hasValidPresentMode;
    Sint32 drawableWidth, drawableHeight;
    Uint32 i;
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    SDL_assert(_this && _this->Vulkan_CreateSurface);

    swapchainData = SDL_malloc(sizeof(VulkanSwapchainData));
    swapchainData->frameCounter = 0;

    // Each swapchain must have its own surface.

    if (!_this->Vulkan_CreateSurface(
            _this,
            windowData->window,
            renderer->instance,
            NULL, // FIXME: VAllocationCallbacks
            &swapchainData->surface)) {
        SDL_free(swapchainData);
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "Vulkan_CreateSurface failed: %s",
            SDL_GetError());
        return false;
    }

    if (!VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            swapchainData->surface,
            &swapchainSupportDetails)) {
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);
        if (swapchainSupportDetails.formatsLength > 0) {
            SDL_free(swapchainSupportDetails.formats);
        }
        if (swapchainSupportDetails.presentModesLength > 0) {
            SDL_free(swapchainSupportDetails.presentModes);
        }
        SDL_free(swapchainData);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Device does not support swap chain creation");
        return false;
    }

    if (swapchainSupportDetails.capabilities.currentExtent.width == 0 ||
        swapchainSupportDetails.capabilities.currentExtent.height == 0) {
        // Not an error, just minimize behavior!
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);
        if (swapchainSupportDetails.formatsLength > 0) {
            SDL_free(swapchainSupportDetails.formats);
        }
        if (swapchainSupportDetails.presentModesLength > 0) {
            SDL_free(swapchainSupportDetails.presentModes);
        }
        SDL_free(swapchainData);
        return false;
    }

    // Verify that we can use the requested composition and present mode

    swapchainData->format = SwapchainCompositionToFormat[windowData->swapchainComposition];
    swapchainData->colorSpace = SwapchainCompositionToColorSpace[windowData->swapchainComposition];
    swapchainData->swapchainSwizzle = SwapchainCompositionSwizzle[windowData->swapchainComposition];
    swapchainData->usingFallbackFormat = false;

    hasValidSwapchainComposition = VULKAN_INTERNAL_VerifySwapSurfaceFormat(
        swapchainData->format,
        swapchainData->colorSpace,
        swapchainSupportDetails.formats,
        swapchainSupportDetails.formatsLength);

    if (!hasValidSwapchainComposition) {
        // Let's try again with the fallback format...
        swapchainData->format = SwapchainCompositionToFallbackFormat[windowData->swapchainComposition];
        swapchainData->usingFallbackFormat = true;
        hasValidSwapchainComposition = VULKAN_INTERNAL_VerifySwapSurfaceFormat(
            swapchainData->format,
            swapchainData->colorSpace,
            swapchainSupportDetails.formats,
            swapchainSupportDetails.formatsLength);
    }

    swapchainData->presentMode = SDLToVK_PresentMode[windowData->presentMode];
    hasValidPresentMode = VULKAN_INTERNAL_VerifySwapPresentMode(
        swapchainData->presentMode,
        swapchainSupportDetails.presentModes,
        swapchainSupportDetails.presentModesLength);

    if (!hasValidSwapchainComposition || !hasValidPresentMode) {
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);

        if (swapchainSupportDetails.formatsLength > 0) {
            SDL_free(swapchainSupportDetails.formats);
        }

        if (swapchainSupportDetails.presentModesLength > 0) {
            SDL_free(swapchainSupportDetails.presentModes);
        }

        SDL_free(swapchainData);

        if (!hasValidSwapchainComposition) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Device does not support requested swapchain composition!");
        }
        if (!hasValidPresentMode) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Device does not support requested presentMode!");
        }
        return false;
    }

    // Sync now to be sure that our swapchain size is correct
    SDL_SyncWindow(windowData->window);
    SDL_GetWindowSizeInPixels(
        windowData->window,
        &drawableWidth,
        &drawableHeight);

    if (drawableWidth < (Sint32)swapchainSupportDetails.capabilities.minImageExtent.width ||
        drawableWidth > (Sint32)swapchainSupportDetails.capabilities.maxImageExtent.width ||
        drawableHeight < (Sint32)swapchainSupportDetails.capabilities.minImageExtent.height ||
        drawableHeight > (Sint32)swapchainSupportDetails.capabilities.maxImageExtent.height) {
        if (swapchainSupportDetails.capabilities.currentExtent.width != UINT32_MAX) {
            drawableWidth = VULKAN_INTERNAL_clamp(
                drawableWidth,
                (Sint32)swapchainSupportDetails.capabilities.minImageExtent.width,
                (Sint32)swapchainSupportDetails.capabilities.maxImageExtent.width);
            drawableHeight = VULKAN_INTERNAL_clamp(
                drawableHeight,
                (Sint32)swapchainSupportDetails.capabilities.minImageExtent.height,
                (Sint32)swapchainSupportDetails.capabilities.maxImageExtent.height);
        } else {
            renderer->vkDestroySurfaceKHR(
                renderer->instance,
                swapchainData->surface,
                NULL);
            if (swapchainSupportDetails.formatsLength > 0) {
                SDL_free(swapchainSupportDetails.formats);
            }
            if (swapchainSupportDetails.presentModesLength > 0) {
                SDL_free(swapchainSupportDetails.presentModes);
            }
            SDL_free(swapchainData);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "No fallback swapchain size available!");
            return false;
        }
    }

    swapchainData->imageCount = MAX_FRAMES_IN_FLIGHT;

    if (swapchainSupportDetails.capabilities.maxImageCount > 0 &&
        swapchainData->imageCount > swapchainSupportDetails.capabilities.maxImageCount) {
        swapchainData->imageCount = swapchainSupportDetails.capabilities.maxImageCount;
    }

    if (swapchainData->presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        /* Required for proper triple-buffering.
         * Note that this is below the above maxImageCount check!
         * If the driver advertises MAILBOX but does not support 3 swap
         * images, it's not real mailbox support, so let it fail hard.
         * -flibit
         */
        swapchainData->imageCount = SDL_max(swapchainData->imageCount, 3);
    }

    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = swapchainData->surface;
    swapchainCreateInfo.minImageCount = swapchainData->imageCount;
    swapchainCreateInfo.imageFormat = swapchainData->format;
    swapchainCreateInfo.imageColorSpace = swapchainData->colorSpace;
    swapchainCreateInfo.imageExtent.width = drawableWidth;
    swapchainCreateInfo.imageExtent.height = drawableHeight;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = NULL;
    swapchainCreateInfo.preTransform = swapchainSupportDetails.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = swapchainData->presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    vulkanResult = renderer->vkCreateSwapchainKHR(
        renderer->logicalDevice,
        &swapchainCreateInfo,
        NULL,
        &swapchainData->swapchain);

    if (swapchainSupportDetails.formatsLength > 0) {
        SDL_free(swapchainSupportDetails.formats);
    }
    if (swapchainSupportDetails.presentModesLength > 0) {
        SDL_free(swapchainSupportDetails.presentModes);
    }

    if (vulkanResult != VK_SUCCESS) {
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);
        SDL_free(swapchainData);
        LogVulkanResultAsError("vkCreateSwapchainKHR", vulkanResult);
        return false;
    }

    renderer->vkGetSwapchainImagesKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        &swapchainData->imageCount,
        NULL);

    swapchainData->textureContainers = SDL_malloc(
        sizeof(VulkanTextureContainer) * swapchainData->imageCount);

    if (!swapchainData->textureContainers) {
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);
        SDL_free(swapchainData);
        return false;
    }

    swapchainImages = SDL_stack_alloc(VkImage, swapchainData->imageCount);

    renderer->vkGetSwapchainImagesKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        &swapchainData->imageCount,
        swapchainImages);

    for (i = 0; i < swapchainData->imageCount; i += 1) {

        // Initialize dummy container
        SDL_zero(swapchainData->textureContainers[i]);
        swapchainData->textureContainers[i].canBeCycled = false;
        swapchainData->textureContainers[i].header.info.width = drawableWidth;
        swapchainData->textureContainers[i].header.info.height = drawableHeight;
        swapchainData->textureContainers[i].header.info.layerCountOrDepth = 1;
        swapchainData->textureContainers[i].header.info.format = SwapchainCompositionToSDLFormat(
            windowData->swapchainComposition,
            swapchainData->usingFallbackFormat);
        swapchainData->textureContainers[i].header.info.type = SDL_GPU_TEXTURETYPE_2D;
        swapchainData->textureContainers[i].header.info.levelCount = 1;
        swapchainData->textureContainers[i].header.info.sampleCount = SDL_GPU_SAMPLECOUNT_1;
        swapchainData->textureContainers[i].header.info.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;

        swapchainData->textureContainers[i].activeTextureHandle = SDL_malloc(sizeof(VulkanTextureHandle));

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture = SDL_malloc(sizeof(VulkanTexture));

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->image = swapchainImages[i];

        // Swapchain memory is managed by the driver
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->usedRegion = NULL;

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->dimensions.width = drawableWidth;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->dimensions.height = drawableHeight;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->format = swapchainData->format;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->swizzle = swapchainData->swapchainSwizzle;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->type = SDL_GPU_TEXTURETYPE_2D;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->depth = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->layerCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->levelCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->sampleCount = VK_SAMPLE_COUNT_1_BIT;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->usageFlags =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        SDL_AtomicSet(&swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->referenceCount, 0);

        swapchainData->textureContainers[i].activeTextureHandle->container = NULL;

        // Create slice
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresourceCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources = SDL_malloc(sizeof(VulkanTextureSubresource));
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].parent = swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].layer = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].level = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].transitioned = true;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].msaaTexHandle = NULL;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].renderTargetViews = SDL_malloc(sizeof(VkImageView));
        VULKAN_INTERNAL_CreateRenderTargetView(
            renderer,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture,
            0,
            0,
            swapchainData->swapchainSwizzle,
            &swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->subresources[0].renderTargetViews[0]);
    }

    SDL_stack_free(swapchainImages);

    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;

    for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        renderer->vkCreateSemaphore(
            renderer->logicalDevice,
            &semaphoreCreateInfo,
            NULL,
            &swapchainData->imageAvailableSemaphore[i]);

        renderer->vkCreateSemaphore(
            renderer->logicalDevice,
            &semaphoreCreateInfo,
            NULL,
            &swapchainData->renderFinishedSemaphore[i]);

        swapchainData->inFlightFences[i] = NULL;
    }

    windowData->swapchainData = swapchainData;
    windowData->needsSwapchainRecreate = false;
    return true;
}

// Command Buffers

static void VULKAN_INTERNAL_BeginCommandBuffer(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo;
    VkResult result;

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = NULL;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = renderer->vkBeginCommandBuffer(
        commandBuffer->commandBuffer,
        &beginInfo);

    if (result != VK_SUCCESS) {
        LogVulkanResultAsError("vkBeginCommandBuffer", result);
    }
}

static void VULKAN_INTERNAL_EndCommandBuffer(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer)
{
    VkResult result;

    result = renderer->vkEndCommandBuffer(
        commandBuffer->commandBuffer);

    if (result != VK_SUCCESS) {
        LogVulkanResultAsError("vkEndCommandBuffer", result);
    }
}

static void VULKAN_DestroyDevice(
    SDL_GPUDevice *device)
{
    VulkanRenderer *renderer = (VulkanRenderer *)device->driverData;
    VulkanMemorySubAllocator *allocator;

    VULKAN_Wait(device->driverData);

    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        VULKAN_UnclaimWindow(device->driverData, renderer->claimedWindows[i]->window);
    }

    SDL_free(renderer->claimedWindows);

    VULKAN_Wait(device->driverData);

    SDL_free(renderer->submittedCommandBuffers);

    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        VULKAN_INTERNAL_DestroyBuffer(
            renderer,
            renderer->uniformBufferPool[i]->bufferHandle->vulkanBuffer);
        SDL_free(renderer->uniformBufferPool[i]->bufferHandle);
        SDL_free(renderer->uniformBufferPool[i]);
    }
    SDL_free(renderer->uniformBufferPool);

    for (Uint32 i = 0; i < renderer->fencePool.availableFenceCount; i += 1) {
        renderer->vkDestroyFence(
            renderer->logicalDevice,
            renderer->fencePool.availableFences[i]->fence,
            NULL);

        SDL_free(renderer->fencePool.availableFences[i]);
    }

    SDL_free(renderer->fencePool.availableFences);
    SDL_DestroyMutex(renderer->fencePool.lock);

    SDL_DestroyHashTable(renderer->commandPoolHashTable);
    SDL_DestroyHashTable(renderer->renderPassHashTable);
    SDL_DestroyHashTable(renderer->framebufferHashTable);

    for (Uint32 i = 0; i < VK_MAX_MEMORY_TYPES; i += 1) {
        allocator = &renderer->memoryAllocator->subAllocators[i];

        for (Sint32 j = allocator->allocationCount - 1; j >= 0; j -= 1) {
            for (Sint32 k = allocator->allocations[j]->usedRegionCount - 1; k >= 0; k -= 1) {
                VULKAN_INTERNAL_RemoveMemoryUsedRegion(
                    renderer,
                    allocator->allocations[j]->usedRegions[k]);
            }

            VULKAN_INTERNAL_DeallocateMemory(
                renderer,
                allocator,
                j);
        }

        if (renderer->memoryAllocator->subAllocators[i].allocations != NULL) {
            SDL_free(renderer->memoryAllocator->subAllocators[i].allocations);
        }

        SDL_free(renderer->memoryAllocator->subAllocators[i].sortedFreeRegions);
    }

    SDL_free(renderer->memoryAllocator);

    SDL_free(renderer->texturesToDestroy);
    SDL_free(renderer->buffersToDestroy);
    SDL_free(renderer->graphicsPipelinesToDestroy);
    SDL_free(renderer->computePipelinesToDestroy);
    SDL_free(renderer->shadersToDestroy);
    SDL_free(renderer->samplersToDestroy);
    SDL_free(renderer->framebuffersToDestroy);
    SDL_free(renderer->allocationsToDefrag);

    SDL_DestroyMutex(renderer->allocatorLock);
    SDL_DestroyMutex(renderer->disposeLock);
    SDL_DestroyMutex(renderer->submitLock);
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->acquireUniformBufferLock);
    SDL_DestroyMutex(renderer->renderPassFetchLock);
    SDL_DestroyMutex(renderer->framebufferFetchLock);

    renderer->vkDestroyDevice(renderer->logicalDevice, NULL);
    renderer->vkDestroyInstance(renderer->instance, NULL);

    SDL_free(renderer);
    SDL_free(device);
    SDL_Vulkan_UnloadLibrary();
}

static VkDescriptorSet VULKAN_INTERNAL_FetchDescriptorSet(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *vulkanCommandBuffer,
    DescriptorSetPool *descriptorSetPool)
{
    VkDescriptorSet descriptorSet;

    SDL_LockMutex(descriptorSetPool->lock);

    // If no inactive descriptor sets remain, create a new pool and allocate new inactive sets

    if (descriptorSetPool->inactiveDescriptorSetCount == 0) {
        descriptorSetPool->descriptorPoolCount += 1;
        descriptorSetPool->descriptorPools = SDL_realloc(
            descriptorSetPool->descriptorPools,
            sizeof(VkDescriptorPool) * descriptorSetPool->descriptorPoolCount);

        if (!VULKAN_INTERNAL_CreateDescriptorPool(
                renderer,
                descriptorSetPool->descriptorInfos,
                descriptorSetPool->descriptorInfoCount,
                descriptorSetPool->nextPoolSize,
                &descriptorSetPool->descriptorPools[descriptorSetPool->descriptorPoolCount - 1])) {
            SDL_UnlockMutex(descriptorSetPool->lock);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create descriptor pool!");
            return VK_NULL_HANDLE;
        }

        descriptorSetPool->inactiveDescriptorSetCapacity += descriptorSetPool->nextPoolSize;

        descriptorSetPool->inactiveDescriptorSets = SDL_realloc(
            descriptorSetPool->inactiveDescriptorSets,
            sizeof(VkDescriptorSet) * descriptorSetPool->inactiveDescriptorSetCapacity);

        if (!VULKAN_INTERNAL_AllocateDescriptorSets(
                renderer,
                descriptorSetPool->descriptorPools[descriptorSetPool->descriptorPoolCount - 1],
                descriptorSetPool->descriptorSetLayout,
                descriptorSetPool->nextPoolSize,
                descriptorSetPool->inactiveDescriptorSets)) {
            SDL_UnlockMutex(descriptorSetPool->lock);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate descriptor sets!");
            return VK_NULL_HANDLE;
        }

        descriptorSetPool->inactiveDescriptorSetCount = descriptorSetPool->nextPoolSize;

        descriptorSetPool->nextPoolSize *= 2;
    }

    descriptorSet = descriptorSetPool->inactiveDescriptorSets[descriptorSetPool->inactiveDescriptorSetCount - 1];
    descriptorSetPool->inactiveDescriptorSetCount -= 1;

    SDL_UnlockMutex(descriptorSetPool->lock);

    if (vulkanCommandBuffer->boundDescriptorSetDataCount == vulkanCommandBuffer->boundDescriptorSetDataCapacity) {
        vulkanCommandBuffer->boundDescriptorSetDataCapacity *= 2;
        vulkanCommandBuffer->boundDescriptorSetDatas = SDL_realloc(
            vulkanCommandBuffer->boundDescriptorSetDatas,
            vulkanCommandBuffer->boundDescriptorSetDataCapacity * sizeof(DescriptorSetData));
    }

    vulkanCommandBuffer->boundDescriptorSetDatas[vulkanCommandBuffer->boundDescriptorSetDataCount].descriptorSet = descriptorSet;
    vulkanCommandBuffer->boundDescriptorSetDatas[vulkanCommandBuffer->boundDescriptorSetDataCount].descriptorSetPool = descriptorSetPool;
    vulkanCommandBuffer->boundDescriptorSetDataCount += 1;

    return descriptorSet;
}

static void VULKAN_INTERNAL_BindGraphicsDescriptorSets(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer)
{
    VulkanGraphicsPipelineResourceLayout *resourceLayout;
    VkWriteDescriptorSet *writeDescriptorSets;
    VkWriteDescriptorSet *currentWriteDescriptorSet;
    DescriptorSetPool *descriptorSetPool;
    VkDescriptorBufferInfo bufferInfos[MAX_STORAGE_BUFFERS_PER_STAGE];
    VkDescriptorImageInfo imageInfos[MAX_TEXTURE_SAMPLERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE];
    Uint32 dynamicOffsets[MAX_UNIFORM_BUFFERS_PER_STAGE];
    Uint32 bufferInfoCount = 0;
    Uint32 imageInfoCount = 0;
    Uint32 i;

    resourceLayout = &commandBuffer->currentGraphicsPipeline->resourceLayout;

    if (commandBuffer->needNewVertexResourceDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[0];

        commandBuffer->vertexResourceDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->vertexSamplerCount +
                resourceLayout->vertexStorageTextureCount +
                resourceLayout->vertexStorageBufferCount);

        for (i = 0; i < resourceLayout->vertexSamplerCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];
            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->vertexResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = commandBuffer->vertexSamplers[i]->sampler;
            imageInfos[imageInfoCount].imageView = commandBuffer->vertexSamplerTextures[i]->fullView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->vertexStorageTextureCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->vertexSamplerCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->vertexSamplerCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->vertexResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = VK_NULL_HANDLE;
            imageInfos[imageInfoCount].imageView = commandBuffer->vertexStorageTextures[i]->fullView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->vertexStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->vertexSamplerCount + resourceLayout->vertexStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->vertexSamplerCount + resourceLayout->vertexStorageTextureCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->vertexResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->vertexStorageBuffers[i]->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = VK_WHOLE_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->vertexSamplerCount + resourceLayout->vertexStorageTextureCount + resourceLayout->vertexStorageBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            resourceLayout->pipelineLayout,
            0,
            1,
            &commandBuffer->vertexResourceDescriptorSet,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewVertexResourceDescriptorSet = false;
    }

    if (commandBuffer->needNewVertexUniformDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[1];

        commandBuffer->vertexUniformDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->vertexUniformBufferCount);

        for (i = 0; i < resourceLayout->vertexUniformBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->vertexUniformDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->vertexUniformBuffers[i]->bufferHandle->vulkanBuffer->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = MAX_UBO_SECTION_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->vertexUniformBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewVertexUniformDescriptorSet = false;
        commandBuffer->needNewVertexUniformOffsets = true;
    }

    if (commandBuffer->needNewVertexUniformOffsets) {
        for (i = 0; i < resourceLayout->vertexUniformBufferCount; i += 1) {
            dynamicOffsets[i] = commandBuffer->vertexUniformBuffers[i]->drawOffset;
        }

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            resourceLayout->pipelineLayout,
            1,
            1,
            &commandBuffer->vertexUniformDescriptorSet,
            resourceLayout->vertexUniformBufferCount,
            dynamicOffsets);

        commandBuffer->needNewVertexUniformOffsets = false;
    }

    if (commandBuffer->needNewFragmentResourceDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[2];

        commandBuffer->fragmentResourceDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->fragmentSamplerCount +
                resourceLayout->fragmentStorageTextureCount +
                resourceLayout->fragmentStorageBufferCount);

        for (i = 0; i < resourceLayout->fragmentSamplerCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];
            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->fragmentResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = commandBuffer->fragmentSamplers[i]->sampler;
            imageInfos[imageInfoCount].imageView = commandBuffer->fragmentSamplerTextures[i]->fullView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->fragmentStorageTextureCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->fragmentSamplerCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->fragmentSamplerCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->fragmentResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = VK_NULL_HANDLE;
            imageInfos[imageInfoCount].imageView = commandBuffer->fragmentStorageTextures[i]->fullView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->fragmentStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->fragmentSamplerCount + resourceLayout->fragmentStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->fragmentSamplerCount + resourceLayout->fragmentStorageTextureCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->fragmentResourceDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->fragmentStorageBuffers[i]->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = VK_WHOLE_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->fragmentSamplerCount + resourceLayout->fragmentStorageTextureCount + resourceLayout->fragmentStorageBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            resourceLayout->pipelineLayout,
            2,
            1,
            &commandBuffer->fragmentResourceDescriptorSet,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewFragmentResourceDescriptorSet = false;
    }

    if (commandBuffer->needNewFragmentUniformDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[3];

        commandBuffer->fragmentUniformDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->fragmentUniformBufferCount);

        for (i = 0; i < resourceLayout->fragmentUniformBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->fragmentUniformDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->fragmentUniformBuffers[i]->bufferHandle->vulkanBuffer->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = MAX_UBO_SECTION_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->fragmentUniformBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewFragmentUniformDescriptorSet = false;
        commandBuffer->needNewFragmentUniformOffsets = true;
    }

    if (commandBuffer->needNewFragmentUniformOffsets) {
        for (i = 0; i < resourceLayout->fragmentUniformBufferCount; i += 1) {
            dynamicOffsets[i] = commandBuffer->fragmentUniformBuffers[i]->drawOffset;
        }

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            resourceLayout->pipelineLayout,
            3,
            1,
            &commandBuffer->fragmentUniformDescriptorSet,
            resourceLayout->fragmentUniformBufferCount,
            dynamicOffsets);

        commandBuffer->needNewFragmentUniformOffsets = false;
    }
}

static void VULKAN_DrawIndexedPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 indexCount,
    Uint32 instanceCount,
    Uint32 firstIndex,
    Sint32 vertexOffset,
    Uint32 firstInstance)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDrawIndexed(
        vulkanCommandBuffer->commandBuffer,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);
}

static void VULKAN_DrawPrimitives(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 vertexCount,
    Uint32 instanceCount,
    Uint32 firstVertex,
    Uint32 firstInstance)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDraw(
        vulkanCommandBuffer->commandBuffer,
        vertexCount,
        instanceCount,
        firstVertex,
        firstInstance);
}

static void VULKAN_DrawPrimitivesIndirect(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)buffer)->activeBufferHandle->vulkanBuffer;
    Uint32 i;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    if (renderer->supportsMultiDrawIndirect) {
        // Real multi-draw!
        renderer->vkCmdDrawIndirect(
            vulkanCommandBuffer->commandBuffer,
            vulkanBuffer->buffer,
            offsetInBytes,
            drawCount,
            stride);
    } else {
        // Fake multi-draw...
        for (i = 0; i < drawCount; i += 1) {
            renderer->vkCmdDrawIndirect(
                vulkanCommandBuffer->commandBuffer,
                vulkanBuffer->buffer,
                offsetInBytes + (stride * i),
                1,
                stride);
        }
    }

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

static void VULKAN_DrawIndexedPrimitivesIndirect(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)buffer)->activeBufferHandle->vulkanBuffer;
    Uint32 i;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    if (renderer->supportsMultiDrawIndirect) {
        // Real multi-draw!
        renderer->vkCmdDrawIndexedIndirect(
            vulkanCommandBuffer->commandBuffer,
            vulkanBuffer->buffer,
            offsetInBytes,
            drawCount,
            stride);
    } else {
        // Fake multi-draw...
        for (i = 0; i < drawCount; i += 1) {
            renderer->vkCmdDrawIndexedIndirect(
                vulkanCommandBuffer->commandBuffer,
                vulkanBuffer->buffer,
                offsetInBytes + (stride * i),
                1,
                stride);
        }
    }

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

// Debug Naming

static void VULKAN_INTERNAL_SetBufferName(
    VulkanRenderer *renderer,
    VulkanBuffer *buffer,
    const char *text)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo;

    if (renderer->debugMode && renderer->supportsDebugUtils) {
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.pNext = NULL;
        nameInfo.pObjectName = text;
        nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
        nameInfo.objectHandle = (uint64_t)buffer->buffer;

        renderer->vkSetDebugUtilsObjectNameEXT(
            renderer->logicalDevice,
            &nameInfo);
    }
}

static void VULKAN_SetBufferName(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer,
    const char *text)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *container = (VulkanBufferContainer *)buffer;
    size_t textLength = SDL_strlen(text) + 1;

    if (renderer->debugMode && renderer->supportsDebugUtils) {
        container->debugName = SDL_realloc(
            container->debugName,
            textLength);

        SDL_utf8strlcpy(
            container->debugName,
            text,
            textLength);

        for (Uint32 i = 0; i < container->bufferCount; i += 1) {
            VULKAN_INTERNAL_SetBufferName(
                renderer,
                container->bufferHandles[i]->vulkanBuffer,
                text);
        }
    }
}

static void VULKAN_INTERNAL_SetTextureName(
    VulkanRenderer *renderer,
    VulkanTexture *texture,
    const char *text)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo;

    if (renderer->debugMode && renderer->supportsDebugUtils) {
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.pNext = NULL;
        nameInfo.pObjectName = text;
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
        nameInfo.objectHandle = (uint64_t)texture->image;

        renderer->vkSetDebugUtilsObjectNameEXT(
            renderer->logicalDevice,
            &nameInfo);
    }
}

static void VULKAN_SetTextureName(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture,
    const char *text)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanTextureContainer *container = (VulkanTextureContainer *)texture;
    size_t textLength = SDL_strlen(text) + 1;

    if (renderer->debugMode && renderer->supportsDebugUtils) {
        container->debugName = SDL_realloc(
            container->debugName,
            textLength);

        SDL_utf8strlcpy(
            container->debugName,
            text,
            textLength);

        for (Uint32 i = 0; i < container->textureCount; i += 1) {
            VULKAN_INTERNAL_SetTextureName(
                renderer,
                container->textureHandles[i]->vulkanTexture,
                text);
        }
    }
}

static void VULKAN_InsertDebugLabel(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *text)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkDebugUtilsLabelEXT labelInfo;

    if (renderer->supportsDebugUtils) {
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pNext = NULL;
        labelInfo.pLabelName = text;

        renderer->vkCmdInsertDebugUtilsLabelEXT(
            vulkanCommandBuffer->commandBuffer,
            &labelInfo);
    }
}

static void VULKAN_PushDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *name)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkDebugUtilsLabelEXT labelInfo;

    if (renderer->supportsDebugUtils) {
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pNext = NULL;
        labelInfo.pLabelName = name;

        renderer->vkCmdBeginDebugUtilsLabelEXT(
            vulkanCommandBuffer->commandBuffer,
            &labelInfo);
    }
}

static void VULKAN_PopDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    if (renderer->supportsDebugUtils) {
        renderer->vkCmdEndDebugUtilsLabelEXT(vulkanCommandBuffer->commandBuffer);
    }
}

static VulkanTextureHandle *VULKAN_INTERNAL_CreateTextureHandle(
    VulkanRenderer *renderer,
    Uint32 width,
    Uint32 height,
    Uint32 depth,
    SDL_GPUTextureType type,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GPUTextureUsageFlags textureUsageFlags,
    bool isMSAAColorTarget)
{
    VulkanTextureHandle *textureHandle;
    VulkanTexture *texture;

    texture = VULKAN_INTERNAL_CreateTexture(
        renderer,
        width,
        height,
        depth,
        type,
        layerCount,
        levelCount,
        sampleCount,
        format,
        swizzle,
        aspectMask,
        textureUsageFlags,
        isMSAAColorTarget);

    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture!");
        return NULL;
    }

    textureHandle = SDL_malloc(sizeof(VulkanTextureHandle));
    textureHandle->vulkanTexture = texture;
    textureHandle->container = NULL;

    texture->handle = textureHandle;

    return textureHandle;
}

static VulkanTexture *VULKAN_INTERNAL_CreateTexture(
    VulkanRenderer *renderer,
    Uint32 width,
    Uint32 height,
    Uint32 depth,
    SDL_GPUTextureType type,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GPUTextureUsageFlags textureUsageFlags,
    bool isMSAAColorTarget)
{
    VkResult vulkanResult;
    VkImageCreateInfo imageCreateInfo;
    VkImageCreateFlags imageCreateFlags = 0;
    VkImageViewCreateInfo imageViewCreateInfo;
    Uint8 bindResult;
    Uint8 isRenderTarget =
        ((textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) != 0) ||
        ((textureUsageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) != 0);
    VkImageUsageFlags vkUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VulkanTexture *texture = SDL_malloc(sizeof(VulkanTexture));

    texture->type = type;
    texture->isMSAAColorTarget = isMSAAColorTarget;
    texture->markedForDestroy = 0;

    if (type == SDL_GPU_TEXTURETYPE_CUBE) {
        imageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    } else if (type == SDL_GPU_TEXTURETYPE_3D) {
        imageCreateFlags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) {
        vkUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) {
        vkUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
        vkUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (textureUsageFlags & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT |
                             SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT |
                             SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
        vkUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.flags = imageCreateFlags;
    imageCreateInfo.imageType = type == SDL_GPU_TEXTURETYPE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = depth;
    imageCreateInfo.mipLevels = levelCount;
    imageCreateInfo.arrayLayers = layerCount;
    imageCreateInfo.samples = isMSAAColorTarget || VULKAN_INTERNAL_IsVulkanDepthFormat(format) ? sampleCount : VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = vkUsageFlags;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = NULL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vulkanResult = renderer->vkCreateImage(
        renderer->logicalDevice,
        &imageCreateInfo,
        NULL,
        &texture->image);
    VULKAN_ERROR_CHECK(vulkanResult, vkCreateImage, 0)

    bindResult = VULKAN_INTERNAL_BindMemoryForImage(
        renderer,
        texture->image,
        &texture->usedRegion);

    if (bindResult != 1) {
        renderer->vkDestroyImage(
            renderer->logicalDevice,
            texture->image,
            NULL);

        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unable to bind memory for texture!");
        return NULL;
    }

    texture->usedRegion->vulkanTexture = texture; // lol

    texture->fullView = VK_NULL_HANDLE;

    if (
        (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) ||
        (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT) ||
        (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT)) {

        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = NULL;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = texture->image;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.components = swizzle;
        imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = levelCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = layerCount;

        if (type == SDL_GPU_TEXTURETYPE_CUBE) {
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        } else if (type == SDL_GPU_TEXTURETYPE_3D) {
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        } else if (type == SDL_GPU_TEXTURETYPE_2D_ARRAY) {
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        } else {
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        }

        vulkanResult = renderer->vkCreateImageView(
            renderer->logicalDevice,
            &imageViewCreateInfo,
            NULL,
            &texture->fullView);

        if (vulkanResult != VK_SUCCESS) {
            LogVulkanResultAsError("vkCreateImageView", vulkanResult);
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture image view");
            return NULL;
        }
    }

    texture->dimensions.width = width;
    texture->dimensions.height = height;
    texture->depth = depth;
    texture->format = format;
    texture->swizzle = swizzle;
    texture->levelCount = levelCount;
    texture->layerCount = layerCount;
    texture->sampleCount = sampleCount;
    texture->usageFlags = textureUsageFlags;
    texture->aspectFlags = aspectMask;
    SDL_AtomicSet(&texture->referenceCount, 0);

    // Define slices
    texture->subresourceCount =
        texture->layerCount *
        texture->levelCount;

    texture->subresources = SDL_malloc(
        texture->subresourceCount * sizeof(VulkanTextureSubresource));

    for (Uint32 i = 0; i < texture->layerCount; i += 1) {
        for (Uint32 j = 0; j < texture->levelCount; j += 1) {
            Uint32 subresourceIndex = VULKAN_INTERNAL_GetTextureSubresourceIndex(
                j,
                i,
                texture->levelCount);

            texture->subresources[subresourceIndex].renderTargetViews = NULL;
            texture->subresources[subresourceIndex].computeWriteView = VK_NULL_HANDLE;
            texture->subresources[subresourceIndex].depthStencilView = VK_NULL_HANDLE;

            if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) {
                texture->subresources[subresourceIndex].renderTargetViews = SDL_malloc(
                    texture->depth * sizeof(VkImageView));

                if (texture->depth > 1) {
                    for (Uint32 k = 0; k < texture->depth; k += 1) {
                        VULKAN_INTERNAL_CreateRenderTargetView(
                            renderer,
                            texture,
                            k,
                            j,
                            swizzle,
                            &texture->subresources[subresourceIndex].renderTargetViews[k]);
                    }
                } else {
                    VULKAN_INTERNAL_CreateRenderTargetView(
                        renderer,
                        texture,
                        i,
                        j,
                        swizzle,
                        &texture->subresources[subresourceIndex].renderTargetViews[0]);
                }
            }

            if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT) {
                VULKAN_INTERNAL_CreateSubresourceView(
                    renderer,
                    texture,
                    i,
                    j,
                    swizzle,
                    &texture->subresources[subresourceIndex].computeWriteView);
            }

            if (textureUsageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
                VULKAN_INTERNAL_CreateSubresourceView(
                    renderer,
                    texture,
                    i,
                    j,
                    swizzle,
                    &texture->subresources[subresourceIndex].depthStencilView);
            }

            texture->subresources[subresourceIndex].parent = texture;
            texture->subresources[subresourceIndex].layer = i;
            texture->subresources[subresourceIndex].level = j;
            texture->subresources[subresourceIndex].msaaTexHandle = NULL;
            texture->subresources[subresourceIndex].transitioned = false;

            if (
                sampleCount > VK_SAMPLE_COUNT_1_BIT &&
                isRenderTarget &&
                !isMSAAColorTarget &&
                !VULKAN_INTERNAL_IsVulkanDepthFormat(texture->format)) {
                texture->subresources[subresourceIndex].msaaTexHandle = VULKAN_INTERNAL_CreateTextureHandle(
                    renderer,
                    texture->dimensions.width >> j,
                    texture->dimensions.height >> j,
                    1,
                    0,
                    1,
                    1,
                    sampleCount,
                    texture->format,
                    texture->swizzle,
                    aspectMask,
                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT,
                    true);
            }
        }
    }

    return texture;
}

static void VULKAN_INTERNAL_CycleActiveBuffer(
    VulkanRenderer *renderer,
    VulkanBufferContainer *bufferContainer)
{
    VulkanBufferHandle *bufferHandle;
    Uint32 i;

    // If a previously-cycled buffer is available, we can use that.
    for (i = 0; i < bufferContainer->bufferCount; i += 1) {
        bufferHandle = bufferContainer->bufferHandles[i];
        if (SDL_AtomicGet(&bufferHandle->vulkanBuffer->referenceCount) == 0) {
            bufferContainer->activeBufferHandle = bufferHandle;
            return;
        }
    }

    // No buffer handle is available, generate a new one.
    bufferContainer->activeBufferHandle = VULKAN_INTERNAL_CreateBufferHandle(
        renderer,
        bufferContainer->activeBufferHandle->vulkanBuffer->size,
        bufferContainer->activeBufferHandle->vulkanBuffer->usageFlags,
        bufferContainer->activeBufferHandle->vulkanBuffer->type);

    bufferContainer->activeBufferHandle->container = bufferContainer;

    EXPAND_ARRAY_IF_NEEDED(
        bufferContainer->bufferHandles,
        VulkanBufferHandle *,
        bufferContainer->bufferCount + 1,
        bufferContainer->bufferCapacity,
        bufferContainer->bufferCapacity * 2);

    bufferContainer->bufferHandles[bufferContainer->bufferCount] = bufferContainer->activeBufferHandle;
    bufferContainer->bufferCount += 1;

    if (
        renderer->debugMode &&
        renderer->supportsDebugUtils &&
        bufferContainer->debugName != NULL) {
        VULKAN_INTERNAL_SetBufferName(
            renderer,
            bufferContainer->activeBufferHandle->vulkanBuffer,
            bufferContainer->debugName);
    }
}

static void VULKAN_INTERNAL_CycleActiveTexture(
    VulkanRenderer *renderer,
    VulkanTextureContainer *textureContainer)
{
    // If a previously-cycled texture is available, we can use that.
    for (Uint32 i = 0; i < textureContainer->textureCount; i += 1) {
        VulkanTextureHandle *textureHandle = textureContainer->textureHandles[i];

        if (SDL_AtomicGet(&textureHandle->vulkanTexture->referenceCount) == 0) {
            textureContainer->activeTextureHandle = textureHandle;
            return;
        }
    }

    // No texture handle is available, generate a new one.
    textureContainer->activeTextureHandle = VULKAN_INTERNAL_CreateTextureHandle(
        renderer,
        textureContainer->activeTextureHandle->vulkanTexture->dimensions.width,
        textureContainer->activeTextureHandle->vulkanTexture->dimensions.height,
        textureContainer->activeTextureHandle->vulkanTexture->depth,
        textureContainer->activeTextureHandle->vulkanTexture->type,
        textureContainer->activeTextureHandle->vulkanTexture->layerCount,
        textureContainer->activeTextureHandle->vulkanTexture->levelCount,
        textureContainer->activeTextureHandle->vulkanTexture->sampleCount,
        textureContainer->activeTextureHandle->vulkanTexture->format,
        textureContainer->activeTextureHandle->vulkanTexture->swizzle,
        textureContainer->activeTextureHandle->vulkanTexture->aspectFlags,
        textureContainer->activeTextureHandle->vulkanTexture->usageFlags,
        false);

    textureContainer->activeTextureHandle->container = textureContainer;

    EXPAND_ARRAY_IF_NEEDED(
        textureContainer->textureHandles,
        VulkanTextureHandle *,
        textureContainer->textureCount + 1,
        textureContainer->textureCapacity,
        textureContainer->textureCapacity * 2);

    textureContainer->textureHandles[textureContainer->textureCount] = textureContainer->activeTextureHandle;
    textureContainer->textureCount += 1;

    if (
        renderer->debugMode &&
        renderer->supportsDebugUtils &&
        textureContainer->debugName != NULL) {
        VULKAN_INTERNAL_SetTextureName(
            renderer,
            textureContainer->activeTextureHandle->vulkanTexture,
            textureContainer->debugName);
    }
}

static VulkanBuffer *VULKAN_INTERNAL_PrepareBufferForWrite(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanBufferContainer *bufferContainer,
    bool cycle,
    VulkanBufferUsageMode destinationUsageMode)
{
    if (
        cycle &&
        SDL_AtomicGet(&bufferContainer->activeBufferHandle->vulkanBuffer->referenceCount) > 0) {
        VULKAN_INTERNAL_CycleActiveBuffer(
            renderer,
            bufferContainer);
    }

    VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
        renderer,
        commandBuffer,
        destinationUsageMode,
        bufferContainer->activeBufferHandle->vulkanBuffer);

    return bufferContainer->activeBufferHandle->vulkanBuffer;
}

static VulkanTextureSubresource *VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureContainer *textureContainer,
    Uint32 layer,
    Uint32 level,
    bool cycle,
    VulkanTextureUsageMode destinationUsageMode)
{
    VulkanTextureSubresource *textureSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
        textureContainer,
        layer,
        level);

    if (
        cycle &&
        textureContainer->canBeCycled &&
        SDL_AtomicGet(&textureContainer->activeTextureHandle->vulkanTexture->referenceCount) > 0) {
        VULKAN_INTERNAL_CycleActiveTexture(
            renderer,
            textureContainer);

        textureSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
            textureContainer,
            layer,
            level);
    }

    // always do barrier because of layout transitions
    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
        renderer,
        commandBuffer,
        destinationUsageMode,
        textureSubresource);

    return textureSubresource;
}

static VkRenderPass VULKAN_INTERNAL_CreateRenderPass(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    VkResult vulkanResult;
    VkAttachmentDescription attachmentDescriptions[2 * MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference colorAttachmentReferences[MAX_COLOR_TARGET_BINDINGS];
    VkAttachmentReference resolveReferences[MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference depthStencilAttachmentReference;
    VkRenderPassCreateInfo renderPassCreateInfo;
    VkSubpassDescription subpass;
    VkRenderPass renderPass;
    Uint32 i;

    Uint32 attachmentDescriptionCount = 0;
    Uint32 colorAttachmentReferenceCount = 0;
    Uint32 resolveReferenceCount = 0;

    VulkanTexture *texture = NULL;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        texture = ((VulkanTextureContainer *)colorAttachmentInfos[i].texture)->activeTextureHandle->vulkanTexture;

        if (texture->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            // Resolve attachment and multisample attachment

            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = texture->format;
            attachmentDescriptions[attachmentDescriptionCount].samples =
                VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[attachmentDescriptionCount].loadOp = SDLToVK_LoadOp[colorAttachmentInfos[i].loadOp];
            attachmentDescriptions[attachmentDescriptionCount].storeOp =
                VK_ATTACHMENT_STORE_OP_STORE; // Always store the resolve texture
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolveReferences[resolveReferenceCount].attachment =
                attachmentDescriptionCount;
            resolveReferences[resolveReferenceCount].layout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            resolveReferenceCount += 1;

            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = texture->format;
            attachmentDescriptions[attachmentDescriptionCount].samples = texture->sampleCount;
            attachmentDescriptions[attachmentDescriptionCount].loadOp = SDLToVK_LoadOp[colorAttachmentInfos[i].loadOp];
            attachmentDescriptions[attachmentDescriptionCount].storeOp = SDLToVK_StoreOp[colorAttachmentInfos[i].storeOp];
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorAttachmentReferences[colorAttachmentReferenceCount].attachment =
                attachmentDescriptionCount;
            colorAttachmentReferences[colorAttachmentReferenceCount].layout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            colorAttachmentReferenceCount += 1;
        } else {
            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = texture->format;
            attachmentDescriptions[attachmentDescriptionCount].samples =
                VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[attachmentDescriptionCount].loadOp = SDLToVK_LoadOp[colorAttachmentInfos[i].loadOp];
            attachmentDescriptions[attachmentDescriptionCount].storeOp =
                VK_ATTACHMENT_STORE_OP_STORE; // Always store non-MSAA textures
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorAttachmentReferences[colorAttachmentReferenceCount].attachment = attachmentDescriptionCount;
            colorAttachmentReferences[colorAttachmentReferenceCount].layout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            colorAttachmentReferenceCount += 1;
        }
    }

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = colorAttachmentCount;
    subpass.pColorAttachments = colorAttachmentReferences;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    if (depthStencilAttachmentInfo == NULL) {
        subpass.pDepthStencilAttachment = NULL;
    } else {
        texture = ((VulkanTextureContainer *)depthStencilAttachmentInfo->texture)->activeTextureHandle->vulkanTexture;

        attachmentDescriptions[attachmentDescriptionCount].flags = 0;
        attachmentDescriptions[attachmentDescriptionCount].format = texture->format;
        attachmentDescriptions[attachmentDescriptionCount].samples = texture->sampleCount;

        attachmentDescriptions[attachmentDescriptionCount].loadOp = SDLToVK_LoadOp[depthStencilAttachmentInfo->loadOp];
        attachmentDescriptions[attachmentDescriptionCount].storeOp = SDLToVK_StoreOp[depthStencilAttachmentInfo->storeOp];
        attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp = SDLToVK_LoadOp[depthStencilAttachmentInfo->stencilLoadOp];
        attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp = SDLToVK_StoreOp[depthStencilAttachmentInfo->stencilStoreOp];
        attachmentDescriptions[attachmentDescriptionCount].initialLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentDescriptions[attachmentDescriptionCount].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilAttachmentReference.attachment =
            attachmentDescriptionCount;
        depthStencilAttachmentReference.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment =
            &depthStencilAttachmentReference;

        attachmentDescriptionCount += 1;
    }

    if (texture != NULL && texture->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
        subpass.pResolveAttachments = resolveReferences;
    } else {
        subpass.pResolveAttachments = NULL;
    }

    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.pAttachments = attachmentDescriptions;
    renderPassCreateInfo.attachmentCount = attachmentDescriptionCount;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;

    vulkanResult = renderer->vkCreateRenderPass(
        renderer->logicalDevice,
        &renderPassCreateInfo,
        NULL,
        &renderPass);

    if (vulkanResult != VK_SUCCESS) {
        renderPass = VK_NULL_HANDLE;
        LogVulkanResultAsError("vkCreateRenderPass", vulkanResult);
    }

    return renderPass;
}

static VkRenderPass VULKAN_INTERNAL_CreateTransientRenderPass(
    VulkanRenderer *renderer,
    SDL_GPUGraphicsPipelineAttachmentInfo attachmentInfo,
    VkSampleCountFlagBits sampleCount)
{
    VkAttachmentDescription attachmentDescriptions[2 * MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference colorAttachmentReferences[MAX_COLOR_TARGET_BINDINGS];
    VkAttachmentReference resolveReferences[MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference depthStencilAttachmentReference;
    SDL_GPUColorAttachmentDescription attachmentDescription;
    VkSubpassDescription subpass;
    VkRenderPassCreateInfo renderPassCreateInfo;
    VkRenderPass renderPass;
    VkResult result;

    Uint32 multisampling = 0;
    Uint32 attachmentDescriptionCount = 0;
    Uint32 colorAttachmentReferenceCount = 0;
    Uint32 resolveReferenceCount = 0;
    Uint32 i;

    for (i = 0; i < attachmentInfo.colorAttachmentCount; i += 1) {
        attachmentDescription = attachmentInfo.colorAttachmentDescriptions[i];

        if (sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            multisampling = 1;

            // Resolve attachment and multisample attachment

            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = SDLToVK_SurfaceFormat[attachmentDescription.format];
            attachmentDescriptions[attachmentDescriptionCount].samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[attachmentDescriptionCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolveReferences[resolveReferenceCount].attachment = attachmentDescriptionCount;
            resolveReferences[resolveReferenceCount].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            resolveReferenceCount += 1;

            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = SDLToVK_SurfaceFormat[attachmentDescription.format];
            attachmentDescriptions[attachmentDescriptionCount].samples = sampleCount;

            attachmentDescriptions[attachmentDescriptionCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorAttachmentReferences[colorAttachmentReferenceCount].attachment =
                attachmentDescriptionCount;
            colorAttachmentReferences[colorAttachmentReferenceCount].layout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            colorAttachmentReferenceCount += 1;
        } else {
            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = SDLToVK_SurfaceFormat[attachmentDescription.format];
            attachmentDescriptions[attachmentDescriptionCount].samples =
                VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[attachmentDescriptionCount].loadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].storeOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[attachmentDescriptionCount].initialLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescriptionCount].finalLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorAttachmentReferences[colorAttachmentReferenceCount].attachment = attachmentDescriptionCount;
            colorAttachmentReferences[colorAttachmentReferenceCount].layout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptionCount += 1;
            colorAttachmentReferenceCount += 1;
        }
    }

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = attachmentInfo.colorAttachmentCount;
    subpass.pColorAttachments = colorAttachmentReferences;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    if (attachmentInfo.hasDepthStencilAttachment) {
        attachmentDescriptions[attachmentDescriptionCount].flags = 0;
        attachmentDescriptions[attachmentDescriptionCount].format =
            SDLToVK_SurfaceFormat[attachmentInfo.depthStencilFormat];
        attachmentDescriptions[attachmentDescriptionCount].samples = sampleCount;

        attachmentDescriptions[attachmentDescriptionCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[attachmentDescriptionCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[attachmentDescriptionCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescriptions[attachmentDescriptionCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescriptions[attachmentDescriptionCount].initialLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentDescriptions[attachmentDescriptionCount].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilAttachmentReference.attachment =
            attachmentDescriptionCount;
        depthStencilAttachmentReference.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment =
            &depthStencilAttachmentReference;

        attachmentDescriptionCount += 1;
    } else {
        subpass.pDepthStencilAttachment = NULL;
    }

    if (multisampling) {
        subpass.pResolveAttachments = resolveReferences;
    } else {
        subpass.pResolveAttachments = NULL;
    }

    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.pAttachments = attachmentDescriptions;
    renderPassCreateInfo.attachmentCount = attachmentDescriptionCount;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;

    result = renderer->vkCreateRenderPass(
        renderer->logicalDevice,
        &renderPassCreateInfo,
        NULL,
        &renderPass);

    if (result != VK_SUCCESS) {
        renderPass = VK_NULL_HANDLE;
        LogVulkanResultAsError("vkCreateRenderPass", result);
    }

    return renderPass;
}

static SDL_GPUGraphicsPipeline *VULKAN_CreateGraphicsPipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUGraphicsPipelineCreateInfo *pipelineCreateInfo)
{
    VkResult vulkanResult;
    Uint32 i;
    VkSampleCountFlagBits actualSampleCount;

    VulkanGraphicsPipeline *graphicsPipeline = (VulkanGraphicsPipeline *)SDL_malloc(sizeof(VulkanGraphicsPipeline));
    VkGraphicsPipelineCreateInfo vkPipelineCreateInfo;

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2];

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorStateCreateInfo;
    VkVertexInputBindingDescription *vertexInputBindingDescriptions = SDL_stack_alloc(VkVertexInputBindingDescription, pipelineCreateInfo->vertexInputState.vertexBindingCount);
    VkVertexInputAttributeDescription *vertexInputAttributeDescriptions = SDL_stack_alloc(VkVertexInputAttributeDescription, pipelineCreateInfo->vertexInputState.vertexAttributeCount);
    VkVertexInputBindingDivisorDescriptionEXT *divisorDescriptions = SDL_stack_alloc(VkVertexInputBindingDivisorDescriptionEXT, pipelineCreateInfo->vertexInputState.vertexBindingCount);
    Uint32 divisorDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
    VkStencilOpState frontStencilState;
    VkStencilOpState backStencilState;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    VkPipelineColorBlendAttachmentState *colorBlendAttachmentStates = SDL_stack_alloc(
        VkPipelineColorBlendAttachmentState,
        pipelineCreateInfo->attachmentInfo.colorAttachmentCount);

    static const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;

    VulkanRenderer *renderer = (VulkanRenderer *)driverData;

    // Find a compatible sample count to use

    actualSampleCount = VULKAN_INTERNAL_GetMaxMultiSampleCount(
        renderer,
        SDLToVK_SampleCount[pipelineCreateInfo->multisampleState.sampleCount]);

    // Create a "compatible" render pass

    VkRenderPass transientRenderPass = VULKAN_INTERNAL_CreateTransientRenderPass(
        renderer,
        pipelineCreateInfo->attachmentInfo,
        actualSampleCount);

    // Dynamic state

    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext = NULL;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = SDL_arraysize(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    // Shader stages

    graphicsPipeline->vertexShader = (VulkanShader *)pipelineCreateInfo->vertexShader;
    SDL_AtomicIncRef(&graphicsPipeline->vertexShader->referenceCount);

    shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfos[0].pNext = NULL;
    shaderStageCreateInfos[0].flags = 0;
    shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageCreateInfos[0].module = graphicsPipeline->vertexShader->shaderModule;
    shaderStageCreateInfos[0].pName = graphicsPipeline->vertexShader->entryPointName;
    shaderStageCreateInfos[0].pSpecializationInfo = NULL;

    graphicsPipeline->fragmentShader = (VulkanShader *)pipelineCreateInfo->fragmentShader;
    SDL_AtomicIncRef(&graphicsPipeline->fragmentShader->referenceCount);

    shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfos[1].pNext = NULL;
    shaderStageCreateInfos[1].flags = 0;
    shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageCreateInfos[1].module = graphicsPipeline->fragmentShader->shaderModule;
    shaderStageCreateInfos[1].pName = graphicsPipeline->fragmentShader->entryPointName;
    shaderStageCreateInfos[1].pSpecializationInfo = NULL;

    // Vertex input

    for (i = 0; i < pipelineCreateInfo->vertexInputState.vertexBindingCount; i += 1) {
        vertexInputBindingDescriptions[i].binding = pipelineCreateInfo->vertexInputState.vertexBindings[i].binding;
        vertexInputBindingDescriptions[i].inputRate = SDLToVK_VertexInputRate[pipelineCreateInfo->vertexInputState.vertexBindings[i].inputRate];
        vertexInputBindingDescriptions[i].stride = pipelineCreateInfo->vertexInputState.vertexBindings[i].stride;

        if (pipelineCreateInfo->vertexInputState.vertexBindings[i].inputRate == SDL_GPU_VERTEXINPUTRATE_INSTANCE) {
            divisorDescriptionCount += 1;
        }
    }

    for (i = 0; i < pipelineCreateInfo->vertexInputState.vertexAttributeCount; i += 1) {
        vertexInputAttributeDescriptions[i].binding = pipelineCreateInfo->vertexInputState.vertexAttributes[i].binding;
        vertexInputAttributeDescriptions[i].format = SDLToVK_VertexFormat[pipelineCreateInfo->vertexInputState.vertexAttributes[i].format];
        vertexInputAttributeDescriptions[i].location = pipelineCreateInfo->vertexInputState.vertexAttributes[i].location;
        vertexInputAttributeDescriptions[i].offset = pipelineCreateInfo->vertexInputState.vertexAttributes[i].offset;
    }

    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = NULL;
    vertexInputStateCreateInfo.flags = 0;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = pipelineCreateInfo->vertexInputState.vertexBindingCount;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDescriptions;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = pipelineCreateInfo->vertexInputState.vertexAttributeCount;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions;

    if (divisorDescriptionCount > 0) {
        divisorDescriptionCount = 0;

        for (i = 0; i < pipelineCreateInfo->vertexInputState.vertexBindingCount; i += 1) {
            if (pipelineCreateInfo->vertexInputState.vertexBindings[i].inputRate == SDL_GPU_VERTEXINPUTRATE_INSTANCE) {
                divisorDescriptions[divisorDescriptionCount].binding = pipelineCreateInfo->vertexInputState.vertexBindings[i].binding;
                divisorDescriptions[divisorDescriptionCount].divisor = pipelineCreateInfo->vertexInputState.vertexBindings[i].instanceStepRate;

                divisorDescriptionCount += 1;
            }
        }

        divisorStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
        divisorStateCreateInfo.pNext = NULL;
        divisorStateCreateInfo.vertexBindingDivisorCount = divisorDescriptionCount;
        divisorStateCreateInfo.pVertexBindingDivisors = divisorDescriptions;

        vertexInputStateCreateInfo.pNext = &divisorStateCreateInfo;
    }

    // Topology

    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = NULL;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyStateCreateInfo.topology = SDLToVK_PrimitiveType[pipelineCreateInfo->primitiveType];

    graphicsPipeline->primitiveType = pipelineCreateInfo->primitiveType;

    // Viewport

    // NOTE: viewport and scissor are dynamic, and must be set using the command buffer

    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = NULL;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = NULL;

    // Rasterization

    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = SDLToVK_PolygonMode(
        renderer,
        pipelineCreateInfo->rasterizerState.fillMode);
    rasterizationStateCreateInfo.cullMode = SDLToVK_CullMode[pipelineCreateInfo->rasterizerState.cullMode];
    rasterizationStateCreateInfo.frontFace = SDLToVK_FrontFace[pipelineCreateInfo->rasterizerState.frontFace];
    rasterizationStateCreateInfo.depthBiasEnable =
        pipelineCreateInfo->rasterizerState.depthBiasEnable;
    rasterizationStateCreateInfo.depthBiasConstantFactor =
        pipelineCreateInfo->rasterizerState.depthBiasConstantFactor;
    rasterizationStateCreateInfo.depthBiasClamp =
        pipelineCreateInfo->rasterizerState.depthBiasClamp;
    rasterizationStateCreateInfo.depthBiasSlopeFactor =
        pipelineCreateInfo->rasterizerState.depthBiasSlopeFactor;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    // Multisample

    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples = actualSampleCount;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask =
        &pipelineCreateInfo->multisampleState.sampleMask;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    // Depth Stencil State

    frontStencilState.failOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.frontStencilState.failOp];
    frontStencilState.passOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.frontStencilState.passOp];
    frontStencilState.depthFailOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.frontStencilState.depthFailOp];
    frontStencilState.compareOp = SDLToVK_CompareOp[pipelineCreateInfo->depthStencilState.frontStencilState.compareOp];
    frontStencilState.compareMask =
        pipelineCreateInfo->depthStencilState.compareMask;
    frontStencilState.writeMask =
        pipelineCreateInfo->depthStencilState.writeMask;
    frontStencilState.reference =
        pipelineCreateInfo->depthStencilState.reference;

    backStencilState.failOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.backStencilState.failOp];
    backStencilState.passOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.backStencilState.passOp];
    backStencilState.depthFailOp = SDLToVK_StencilOp[pipelineCreateInfo->depthStencilState.backStencilState.depthFailOp];
    backStencilState.compareOp = SDLToVK_CompareOp[pipelineCreateInfo->depthStencilState.backStencilState.compareOp];
    backStencilState.compareMask =
        pipelineCreateInfo->depthStencilState.compareMask;
    backStencilState.writeMask =
        pipelineCreateInfo->depthStencilState.writeMask;
    backStencilState.reference =
        pipelineCreateInfo->depthStencilState.reference;

    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.pNext = NULL;
    depthStencilStateCreateInfo.flags = 0;
    depthStencilStateCreateInfo.depthTestEnable =
        pipelineCreateInfo->depthStencilState.depthTestEnable;
    depthStencilStateCreateInfo.depthWriteEnable =
        pipelineCreateInfo->depthStencilState.depthWriteEnable;
    depthStencilStateCreateInfo.depthCompareOp = SDLToVK_CompareOp[pipelineCreateInfo->depthStencilState.compareOp];
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable =
        pipelineCreateInfo->depthStencilState.stencilTestEnable;
    depthStencilStateCreateInfo.front = frontStencilState;
    depthStencilStateCreateInfo.back = backStencilState;
    depthStencilStateCreateInfo.minDepthBounds = 0; // unused
    depthStencilStateCreateInfo.maxDepthBounds = 0; // unused

    // Color Blend

    for (i = 0; i < pipelineCreateInfo->attachmentInfo.colorAttachmentCount; i += 1) {
        SDL_GPUColorAttachmentBlendState blendState = pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].blendState;

        colorBlendAttachmentStates[i].blendEnable =
            blendState.blendEnable;
        colorBlendAttachmentStates[i].srcColorBlendFactor = SDLToVK_BlendFactor[blendState.srcColorBlendFactor];
        colorBlendAttachmentStates[i].dstColorBlendFactor = SDLToVK_BlendFactor[blendState.dstColorBlendFactor];
        colorBlendAttachmentStates[i].colorBlendOp = SDLToVK_BlendOp[blendState.colorBlendOp];
        colorBlendAttachmentStates[i].srcAlphaBlendFactor = SDLToVK_BlendFactor[blendState.srcAlphaBlendFactor];
        colorBlendAttachmentStates[i].dstAlphaBlendFactor = SDLToVK_BlendFactor[blendState.dstAlphaBlendFactor];
        colorBlendAttachmentStates[i].alphaBlendOp = SDLToVK_BlendOp[blendState.alphaBlendOp];
        colorBlendAttachmentStates[i].colorWriteMask =
            blendState.colorWriteMask;
    }

    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.attachmentCount =
        pipelineCreateInfo->attachmentInfo.colorAttachmentCount;
    colorBlendStateCreateInfo.pAttachments =
        colorBlendAttachmentStates;
    colorBlendStateCreateInfo.blendConstants[0] =
        pipelineCreateInfo->blendConstants[0];
    colorBlendStateCreateInfo.blendConstants[1] =
        pipelineCreateInfo->blendConstants[1];
    colorBlendStateCreateInfo.blendConstants[2] =
        pipelineCreateInfo->blendConstants[2];
    colorBlendStateCreateInfo.blendConstants[3] =
        pipelineCreateInfo->blendConstants[3];

    // We don't support LogicOp, so this is easy.
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = 0;

    // Pipeline Layout

    if (!VULKAN_INTERNAL_InitializeGraphicsPipelineResourceLayout(
            renderer,
            graphicsPipeline->vertexShader,
            graphicsPipeline->fragmentShader,
            &graphicsPipeline->resourceLayout)) {
        SDL_stack_free(vertexInputBindingDescriptions);
        SDL_stack_free(vertexInputAttributeDescriptions);
        SDL_stack_free(colorBlendAttachmentStates);
        SDL_stack_free(divisorDescriptions);
        SDL_free(graphicsPipeline);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to initialize pipeline resource layout!");
        return NULL;
    }

    // Pipeline

    vkPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vkPipelineCreateInfo.pNext = NULL;
    vkPipelineCreateInfo.flags = 0;
    vkPipelineCreateInfo.stageCount = 2;
    vkPipelineCreateInfo.pStages = shaderStageCreateInfos;
    vkPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    vkPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    vkPipelineCreateInfo.pTessellationState = VK_NULL_HANDLE;
    vkPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    vkPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    vkPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    vkPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    vkPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    vkPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    vkPipelineCreateInfo.layout = graphicsPipeline->resourceLayout.pipelineLayout;
    vkPipelineCreateInfo.renderPass = transientRenderPass;
    vkPipelineCreateInfo.subpass = 0;
    vkPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    vkPipelineCreateInfo.basePipelineIndex = 0;

    // TODO: enable pipeline caching
    vulkanResult = renderer->vkCreateGraphicsPipelines(
        renderer->logicalDevice,
        VK_NULL_HANDLE,
        1,
        &vkPipelineCreateInfo,
        NULL,
        &graphicsPipeline->pipeline);

    SDL_stack_free(vertexInputBindingDescriptions);
    SDL_stack_free(vertexInputAttributeDescriptions);
    SDL_stack_free(colorBlendAttachmentStates);
    SDL_stack_free(divisorDescriptions);

    renderer->vkDestroyRenderPass(
        renderer->logicalDevice,
        transientRenderPass,
        NULL);

    if (vulkanResult != VK_SUCCESS) {
        SDL_free(graphicsPipeline);
        LogVulkanResultAsError("vkCreateGraphicsPipelines", vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create graphics pipeline!");
        return NULL;
    }

    SDL_AtomicSet(&graphicsPipeline->referenceCount, 0);

    return (SDL_GPUGraphicsPipeline *)graphicsPipeline;
}

static SDL_GPUComputePipeline *VULKAN_CreateComputePipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUComputePipelineCreateInfo *pipelineCreateInfo)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    VkComputePipelineCreateInfo computePipelineCreateInfo;
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo;
    VkResult vulkanResult;
    Uint32 i;
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanComputePipeline *vulkanComputePipeline;

    if (pipelineCreateInfo->format != SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Incompatible shader format for Vulkan!");
        return NULL;
    }

    vulkanComputePipeline = SDL_malloc(sizeof(VulkanComputePipeline));
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = NULL;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = pipelineCreateInfo->codeSize;
    shaderModuleCreateInfo.pCode = (Uint32 *)pipelineCreateInfo->code;

    vulkanResult = renderer->vkCreateShaderModule(
        renderer->logicalDevice,
        &shaderModuleCreateInfo,
        NULL,
        &vulkanComputePipeline->shaderModule);

    if (vulkanResult != VK_SUCCESS) {
        SDL_free(vulkanComputePipeline);
        LogVulkanResultAsError("vkCreateShaderModule", vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create compute pipeline!");
        return NULL;
    }

    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.pNext = NULL;
    pipelineShaderStageCreateInfo.flags = 0;
    pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineShaderStageCreateInfo.module = vulkanComputePipeline->shaderModule;
    pipelineShaderStageCreateInfo.pName = pipelineCreateInfo->entryPointName;
    pipelineShaderStageCreateInfo.pSpecializationInfo = NULL;

    if (!VULKAN_INTERNAL_InitializeComputePipelineResourceLayout(
            renderer,
            pipelineCreateInfo,
            &vulkanComputePipeline->resourceLayout)) {
        renderer->vkDestroyShaderModule(
            renderer->logicalDevice,
            vulkanComputePipeline->shaderModule,
            NULL);
        SDL_free(vulkanComputePipeline);
        return NULL;
    }

    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = NULL;
    computePipelineCreateInfo.flags = 0;
    computePipelineCreateInfo.stage = pipelineShaderStageCreateInfo;
    computePipelineCreateInfo.layout =
        vulkanComputePipeline->resourceLayout.pipelineLayout;
    computePipelineCreateInfo.basePipelineHandle = (VkPipeline)VK_NULL_HANDLE;
    computePipelineCreateInfo.basePipelineIndex = 0;

    vulkanResult = renderer->vkCreateComputePipelines(
        renderer->logicalDevice,
        (VkPipelineCache)VK_NULL_HANDLE,
        1,
        &computePipelineCreateInfo,
        NULL,
        &vulkanComputePipeline->pipeline);

    if (vulkanResult != VK_SUCCESS) {
        renderer->vkDestroyPipelineLayout(
            renderer->logicalDevice,
            vulkanComputePipeline->resourceLayout.pipelineLayout,
            NULL);

        for (i = 0; i < 3; i += 1) {
            VULKAN_INTERNAL_DestroyDescriptorSetPool(
                renderer,
                &vulkanComputePipeline->resourceLayout.descriptorSetPools[i]);
        }

        renderer->vkDestroyShaderModule(
            renderer->logicalDevice,
            vulkanComputePipeline->shaderModule,
            NULL);

        LogVulkanResultAsError("vkCreateComputePipeline", vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create compute pipeline!");
        return NULL;
    }

    SDL_AtomicSet(&vulkanComputePipeline->referenceCount, 0);

    return (SDL_GPUComputePipeline *)vulkanComputePipeline;
}

static SDL_GPUSampler *VULKAN_CreateSampler(
    SDL_GPURenderer *driverData,
    SDL_GPUSamplerCreateInfo *samplerCreateInfo)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanSampler *vulkanSampler = SDL_malloc(sizeof(VulkanSampler));
    VkResult vulkanResult;

    VkSamplerCreateInfo vkSamplerCreateInfo;
    vkSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    vkSamplerCreateInfo.pNext = NULL;
    vkSamplerCreateInfo.flags = 0;
    vkSamplerCreateInfo.magFilter = SDLToVK_Filter[samplerCreateInfo->magFilter];
    vkSamplerCreateInfo.minFilter = SDLToVK_Filter[samplerCreateInfo->minFilter];
    vkSamplerCreateInfo.mipmapMode = SDLToVK_SamplerMipmapMode[samplerCreateInfo->mipmapMode];
    vkSamplerCreateInfo.addressModeU = SDLToVK_SamplerAddressMode[samplerCreateInfo->addressModeU];
    vkSamplerCreateInfo.addressModeV = SDLToVK_SamplerAddressMode[samplerCreateInfo->addressModeV];
    vkSamplerCreateInfo.addressModeW = SDLToVK_SamplerAddressMode[samplerCreateInfo->addressModeW];
    vkSamplerCreateInfo.mipLodBias = samplerCreateInfo->mipLodBias;
    vkSamplerCreateInfo.anisotropyEnable = samplerCreateInfo->anisotropyEnable;
    vkSamplerCreateInfo.maxAnisotropy = samplerCreateInfo->maxAnisotropy;
    vkSamplerCreateInfo.compareEnable = samplerCreateInfo->compareEnable;
    vkSamplerCreateInfo.compareOp = SDLToVK_CompareOp[samplerCreateInfo->compareOp];
    vkSamplerCreateInfo.minLod = samplerCreateInfo->minLod;
    vkSamplerCreateInfo.maxLod = samplerCreateInfo->maxLod;
    vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; // arbitrary, unused
    vkSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    vulkanResult = renderer->vkCreateSampler(
        renderer->logicalDevice,
        &vkSamplerCreateInfo,
        NULL,
        &vulkanSampler->sampler);

    if (vulkanResult != VK_SUCCESS) {
        SDL_free(vulkanSampler);
        LogVulkanResultAsError("vkCreateSampler", vulkanResult);
        return NULL;
    }

    SDL_AtomicSet(&vulkanSampler->referenceCount, 0);

    return (SDL_GPUSampler *)vulkanSampler;
}

static SDL_GPUShader *VULKAN_CreateShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShaderCreateInfo *shaderCreateInfo)
{
    VulkanShader *vulkanShader;
    VkResult vulkanResult;
    VkShaderModuleCreateInfo vkShaderModuleCreateInfo;
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    size_t entryPointNameLength;

    vulkanShader = SDL_malloc(sizeof(VulkanShader));
    vkShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkShaderModuleCreateInfo.pNext = NULL;
    vkShaderModuleCreateInfo.flags = 0;
    vkShaderModuleCreateInfo.codeSize = shaderCreateInfo->codeSize;
    vkShaderModuleCreateInfo.pCode = (Uint32 *)shaderCreateInfo->code;

    vulkanResult = renderer->vkCreateShaderModule(
        renderer->logicalDevice,
        &vkShaderModuleCreateInfo,
        NULL,
        &vulkanShader->shaderModule);

    if (vulkanResult != VK_SUCCESS) {
        SDL_free(vulkanShader);
        LogVulkanResultAsError("vkCreateShaderModule", vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create shader module!");
        return NULL;
    }

    entryPointNameLength = SDL_strlen(shaderCreateInfo->entryPointName) + 1;
    vulkanShader->entryPointName = SDL_malloc(entryPointNameLength);
    SDL_utf8strlcpy((char *)vulkanShader->entryPointName, shaderCreateInfo->entryPointName, entryPointNameLength);

    vulkanShader->samplerCount = shaderCreateInfo->samplerCount;
    vulkanShader->storageTextureCount = shaderCreateInfo->storageTextureCount;
    vulkanShader->storageBufferCount = shaderCreateInfo->storageBufferCount;
    vulkanShader->uniformBufferCount = shaderCreateInfo->uniformBufferCount;

    SDL_AtomicSet(&vulkanShader->referenceCount, 0);

    return (SDL_GPUShader *)vulkanShader;
}

static bool VULKAN_SupportsSampleCount(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUSampleCount sampleCount)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkSampleCountFlags bits = IsDepthFormat(format) ? renderer->physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts : renderer->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts;
    VkSampleCountFlagBits vkSampleCount = SDLToVK_SampleCount[sampleCount];
    return !!(bits & vkSampleCount);
}

static SDL_GPUTexture *VULKAN_CreateTexture(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureCreateInfo *textureCreateInfo)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkImageAspectFlags imageAspectFlags;
    Uint8 isDepthFormat = IsDepthFormat(textureCreateInfo->format);
    VkFormat format;
    VkComponentMapping swizzle;
    VulkanTextureContainer *container;
    VulkanTextureHandle *textureHandle;

    format = SDLToVK_SurfaceFormat[textureCreateInfo->format];
    swizzle = SDLToVK_SurfaceSwizzle[textureCreateInfo->format];

    if (isDepthFormat) {
        imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (IsStencilFormat(textureCreateInfo->format)) {
            imageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    textureHandle = VULKAN_INTERNAL_CreateTextureHandle(
        renderer,
        textureCreateInfo->width,
        textureCreateInfo->height,
        textureCreateInfo->type == SDL_GPU_TEXTURETYPE_3D ? textureCreateInfo->layerCountOrDepth : 1,
        textureCreateInfo->type,
        textureCreateInfo->type == SDL_GPU_TEXTURETYPE_3D ? 1 : textureCreateInfo->layerCountOrDepth,
        textureCreateInfo->levelCount,
        SDLToVK_SampleCount[textureCreateInfo->sampleCount],
        format,
        swizzle,
        imageAspectFlags,
        textureCreateInfo->usageFlags,
        false);

    if (textureHandle == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create texture container!");
        return NULL;
    }

    container = SDL_malloc(sizeof(VulkanTextureContainer));
    container->header.info = *textureCreateInfo;
    container->canBeCycled = 1;
    container->activeTextureHandle = textureHandle;
    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textureHandles = SDL_malloc(
        container->textureCapacity * sizeof(VulkanTextureHandle *));
    container->textureHandles[0] = container->activeTextureHandle;
    container->debugName = NULL;

    textureHandle->container = container;

    return (SDL_GPUTexture *)container;
}

static SDL_GPUBuffer *VULKAN_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usageFlags,
    Uint32 sizeInBytes)
{
    return (SDL_GPUBuffer *)VULKAN_INTERNAL_CreateBufferContainer(
        (VulkanRenderer *)driverData,
        (VkDeviceSize)sizeInBytes,
        usageFlags,
        VULKAN_BUFFER_TYPE_GPU);
}

static VulkanUniformBuffer *VULKAN_INTERNAL_CreateUniformBuffer(
    VulkanRenderer *renderer,
    Uint32 sizeInBytes)
{
    VulkanUniformBuffer *uniformBuffer = SDL_malloc(sizeof(VulkanUniformBuffer));

    uniformBuffer->bufferHandle = VULKAN_INTERNAL_CreateBufferHandle(
        renderer,
        (VkDeviceSize)sizeInBytes,
        0,
        VULKAN_BUFFER_TYPE_UNIFORM);

    uniformBuffer->drawOffset = 0;
    uniformBuffer->writeOffset = 0;

    return uniformBuffer;
}

static SDL_GPUTransferBuffer *VULKAN_CreateTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBufferUsage usage, // ignored on Vulkan
    Uint32 sizeInBytes)
{
    return (SDL_GPUTransferBuffer *)VULKAN_INTERNAL_CreateBufferContainer(
        (VulkanRenderer *)driverData,
        (VkDeviceSize)sizeInBytes,
        0,
        VULKAN_BUFFER_TYPE_TRANSFER);
}

static void VULKAN_INTERNAL_ReleaseTexture(
    VulkanRenderer *renderer,
    VulkanTexture *vulkanTexture)
{
    if (vulkanTexture->markedForDestroy) {
        return;
    }

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->texturesToDestroy,
        VulkanTexture *,
        renderer->texturesToDestroyCount + 1,
        renderer->texturesToDestroyCapacity,
        renderer->texturesToDestroyCapacity * 2)

    renderer->texturesToDestroy[renderer->texturesToDestroyCount] = vulkanTexture;
    renderer->texturesToDestroyCount += 1;

    vulkanTexture->markedForDestroy = 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseTexture(
    SDL_GPURenderer *driverData,
    SDL_GPUTexture *texture)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanTextureContainer *vulkanTextureContainer = (VulkanTextureContainer *)texture;
    Uint32 i;

    SDL_LockMutex(renderer->disposeLock);

    for (i = 0; i < vulkanTextureContainer->textureCount; i += 1) {
        VULKAN_INTERNAL_ReleaseTexture(renderer, vulkanTextureContainer->textureHandles[i]->vulkanTexture);
        SDL_free(vulkanTextureContainer->textureHandles[i]);
    }

    // Containers are just client handles, so we can destroy immediately
    if (vulkanTextureContainer->debugName != NULL) {
        SDL_free(vulkanTextureContainer->debugName);
    }
    SDL_free(vulkanTextureContainer->textureHandles);
    SDL_free(vulkanTextureContainer);

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseSampler(
    SDL_GPURenderer *driverData,
    SDL_GPUSampler *sampler)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanSampler *vulkanSampler = (VulkanSampler *)sampler;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->samplersToDestroy,
        VulkanSampler *,
        renderer->samplersToDestroyCount + 1,
        renderer->samplersToDestroyCapacity,
        renderer->samplersToDestroyCapacity * 2)

    renderer->samplersToDestroy[renderer->samplersToDestroyCount] = vulkanSampler;
    renderer->samplersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_INTERNAL_ReleaseBuffer(
    VulkanRenderer *renderer,
    VulkanBuffer *vulkanBuffer)
{
    if (vulkanBuffer->markedForDestroy) {
        return;
    }

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->buffersToDestroy,
        VulkanBuffer *,
        renderer->buffersToDestroyCount + 1,
        renderer->buffersToDestroyCapacity,
        renderer->buffersToDestroyCapacity * 2)

    renderer->buffersToDestroy[renderer->buffersToDestroyCount] = vulkanBuffer;
    renderer->buffersToDestroyCount += 1;

    vulkanBuffer->markedForDestroy = 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_INTERNAL_ReleaseBufferContainer(
    VulkanRenderer *renderer,
    VulkanBufferContainer *bufferContainer)
{
    Uint32 i;

    SDL_LockMutex(renderer->disposeLock);

    for (i = 0; i < bufferContainer->bufferCount; i += 1) {
        VULKAN_INTERNAL_ReleaseBuffer(renderer, bufferContainer->bufferHandles[i]->vulkanBuffer);
        SDL_free(bufferContainer->bufferHandles[i]);
    }

    // Containers are just client handles, so we can free immediately
    if (bufferContainer->debugName != NULL) {
        SDL_free(bufferContainer->debugName);
    }
    SDL_free(bufferContainer->bufferHandles);
    SDL_free(bufferContainer);

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBuffer *buffer)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *vulkanBufferContainer = (VulkanBufferContainer *)buffer;

    VULKAN_INTERNAL_ReleaseBufferContainer(
        renderer,
        vulkanBufferContainer);
}

static void VULKAN_ReleaseTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)transferBuffer;

    VULKAN_INTERNAL_ReleaseBufferContainer(
        renderer,
        transferBufferContainer);
}

static void VULKAN_ReleaseShader(
    SDL_GPURenderer *driverData,
    SDL_GPUShader *shader)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanShader *vulkanShader = (VulkanShader *)shader;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->shadersToDestroy,
        VulkanShader *,
        renderer->shadersToDestroyCount + 1,
        renderer->shadersToDestroyCapacity,
        renderer->shadersToDestroyCapacity * 2)

    renderer->shadersToDestroy[renderer->shadersToDestroyCount] = vulkanShader;
    renderer->shadersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseComputePipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUComputePipeline *computePipeline)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanComputePipeline *vulkanComputePipeline = (VulkanComputePipeline *)computePipeline;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->computePipelinesToDestroy,
        VulkanComputePipeline *,
        renderer->computePipelinesToDestroyCount + 1,
        renderer->computePipelinesToDestroyCapacity,
        renderer->computePipelinesToDestroyCapacity * 2)

    renderer->computePipelinesToDestroy[renderer->computePipelinesToDestroyCount] = vulkanComputePipeline;
    renderer->computePipelinesToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseGraphicsPipeline(
    SDL_GPURenderer *driverData,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanGraphicsPipeline *vulkanGraphicsPipeline = (VulkanGraphicsPipeline *)graphicsPipeline;

    SDL_LockMutex(renderer->disposeLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->graphicsPipelinesToDestroy,
        VulkanGraphicsPipeline *,
        renderer->graphicsPipelinesToDestroyCount + 1,
        renderer->graphicsPipelinesToDestroyCapacity,
        renderer->graphicsPipelinesToDestroyCapacity * 2)

    renderer->graphicsPipelinesToDestroy[renderer->graphicsPipelinesToDestroyCount] = vulkanGraphicsPipeline;
    renderer->graphicsPipelinesToDestroyCount += 1;

    SDL_UnlockMutex(renderer->disposeLock);
}

// Command Buffer render state

static VkRenderPass VULKAN_INTERNAL_FetchRenderPass(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    VulkanRenderPassHashTableValue *renderPassWrapper = NULL;
    VkRenderPass renderPassHandle;
    RenderPassHashTableKey key;
    Uint32 i;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        key.colorTargetDescriptions[i].format = ((VulkanTextureContainer *)colorAttachmentInfos[i].texture)->activeTextureHandle->vulkanTexture->format;
        key.colorTargetDescriptions[i].loadOp = colorAttachmentInfos[i].loadOp;
        key.colorTargetDescriptions[i].storeOp = colorAttachmentInfos[i].storeOp;
    }

    key.colorAttachmentSampleCount = VK_SAMPLE_COUNT_1_BIT;
    if (colorAttachmentCount > 0) {
        key.colorAttachmentSampleCount = ((VulkanTextureContainer *)colorAttachmentInfos[0].texture)->activeTextureHandle->vulkanTexture->sampleCount;
    }

    key.colorAttachmentCount = colorAttachmentCount;

    if (depthStencilAttachmentInfo == NULL) {
        key.depthStencilTargetDescription.format = 0;
        key.depthStencilTargetDescription.loadOp = SDL_GPU_LOADOP_DONT_CARE;
        key.depthStencilTargetDescription.storeOp = SDL_GPU_STOREOP_DONT_CARE;
        key.depthStencilTargetDescription.stencilLoadOp = SDL_GPU_LOADOP_DONT_CARE;
        key.depthStencilTargetDescription.stencilStoreOp = SDL_GPU_STOREOP_DONT_CARE;
    } else {
        key.depthStencilTargetDescription.format = ((VulkanTextureContainer *)depthStencilAttachmentInfo->texture)->activeTextureHandle->vulkanTexture->format;
        key.depthStencilTargetDescription.loadOp = depthStencilAttachmentInfo->loadOp;
        key.depthStencilTargetDescription.storeOp = depthStencilAttachmentInfo->storeOp;
        key.depthStencilTargetDescription.stencilLoadOp = depthStencilAttachmentInfo->stencilLoadOp;
        key.depthStencilTargetDescription.stencilStoreOp = depthStencilAttachmentInfo->stencilStoreOp;
    }

    SDL_LockMutex(renderer->renderPassFetchLock);

    bool result = SDL_FindInHashTable(
        renderer->renderPassHashTable,
        (const void *)&key,
        (const void **)&renderPassWrapper);

    SDL_UnlockMutex(renderer->renderPassFetchLock);

    if (result) {
        return renderPassWrapper->handle;
    }

    renderPassHandle = VULKAN_INTERNAL_CreateRenderPass(
        renderer,
        commandBuffer,
        colorAttachmentInfos,
        colorAttachmentCount,
        depthStencilAttachmentInfo);

    if (renderPassHandle == VK_NULL_HANDLE) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create VkRenderPass!");
        return VK_NULL_HANDLE;
    }

    // Have to malloc the key to store it in the hashtable
    RenderPassHashTableKey *allocedKey = SDL_malloc(sizeof(RenderPassHashTableKey));
    SDL_memcpy(allocedKey, &key, sizeof(RenderPassHashTableKey));

    renderPassWrapper = SDL_malloc(sizeof(VulkanRenderPassHashTableValue));
    renderPassWrapper->handle = renderPassHandle;

    SDL_LockMutex(renderer->renderPassFetchLock);

    SDL_InsertIntoHashTable(
        renderer->renderPassHashTable,
        (const void *)allocedKey,
        (const void *)renderPassWrapper);

    SDL_UnlockMutex(renderer->renderPassFetchLock);
    return renderPassHandle;
}

static VulkanFramebuffer *VULKAN_INTERNAL_FetchFramebuffer(
    VulkanRenderer *renderer,
    VkRenderPass renderPass,
    SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo,
    Uint32 width,
    Uint32 height)
{
    VulkanFramebuffer *vulkanFramebuffer = NULL;
    VkFramebufferCreateInfo framebufferInfo;
    VkResult result;
    VkImageView imageViewAttachments[2 * MAX_COLOR_TARGET_BINDINGS + 1];
    FramebufferHashTableKey key;
    Uint32 attachmentCount = 0;
    Uint32 i;

    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        key.colorAttachmentViews[i] = VK_NULL_HANDLE;
        key.colorMultiSampleAttachmentViews[i] = VK_NULL_HANDLE;
    }

    key.colorAttachmentCount = colorAttachmentCount;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        VulkanTextureContainer *container = (VulkanTextureContainer *)colorAttachmentInfos[i].texture;
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_FetchTextureSubresource(
            container,
            container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : colorAttachmentInfos[i].layerOrDepthPlane,
            colorAttachmentInfos[i].mipLevel);

        Uint32 rtvIndex =
            container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? colorAttachmentInfos[i].layerOrDepthPlane : 0;
        key.colorAttachmentViews[i] = subresource->renderTargetViews[rtvIndex];

        if (subresource->msaaTexHandle != NULL) {
            key.colorMultiSampleAttachmentViews[i] = subresource->msaaTexHandle->vulkanTexture->subresources[0].renderTargetViews[0];
        }
    }

    if (depthStencilAttachmentInfo == NULL) {
        key.depthStencilAttachmentView = VK_NULL_HANDLE;
    } else {
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_FetchTextureSubresource(
            (VulkanTextureContainer *)depthStencilAttachmentInfo->texture,
            0,
            0);
        key.depthStencilAttachmentView = subresource->depthStencilView;
    }

    key.width = width;
    key.height = height;

    SDL_LockMutex(renderer->framebufferFetchLock);

    bool findResult = SDL_FindInHashTable(
        renderer->framebufferHashTable,
        (const void *)&key,
        (const void **)&vulkanFramebuffer);

    SDL_UnlockMutex(renderer->framebufferFetchLock);

    if (findResult) {
        return vulkanFramebuffer;
    }

    vulkanFramebuffer = SDL_malloc(sizeof(VulkanFramebuffer));

    SDL_AtomicSet(&vulkanFramebuffer->referenceCount, 0);

    // Create a new framebuffer

    for (i = 0; i < colorAttachmentCount; i += 1) {
        VulkanTextureContainer *container = (VulkanTextureContainer *)colorAttachmentInfos[i].texture;
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_FetchTextureSubresource(
            container,
            container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : colorAttachmentInfos[i].layerOrDepthPlane,
            colorAttachmentInfos[i].mipLevel);

        Uint32 rtvIndex =
            container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? colorAttachmentInfos[i].layerOrDepthPlane : 0;

        imageViewAttachments[attachmentCount] =
            subresource->renderTargetViews[rtvIndex];

        attachmentCount += 1;

        if (subresource->msaaTexHandle != NULL) {
            imageViewAttachments[attachmentCount] =
                subresource->msaaTexHandle->vulkanTexture->subresources[0].renderTargetViews[0];

            attachmentCount += 1;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_FetchTextureSubresource(
            (VulkanTextureContainer *)depthStencilAttachmentInfo->texture,
            0,
            0);
        imageViewAttachments[attachmentCount] = subresource->depthStencilView;

        attachmentCount += 1;
    }

    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = NULL;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = attachmentCount;
    framebufferInfo.pAttachments = imageViewAttachments;
    framebufferInfo.width = key.width;
    framebufferInfo.height = key.height;
    framebufferInfo.layers = 1;

    result = renderer->vkCreateFramebuffer(
        renderer->logicalDevice,
        &framebufferInfo,
        NULL,
        &vulkanFramebuffer->framebuffer);

    if (result == VK_SUCCESS) {
        // Have to malloc the key to store it in the hashtable
        FramebufferHashTableKey *allocedKey = SDL_malloc(sizeof(FramebufferHashTableKey));
        SDL_memcpy(allocedKey, &key, sizeof(FramebufferHashTableKey));

        SDL_LockMutex(renderer->framebufferFetchLock);

        SDL_InsertIntoHashTable(
            renderer->framebufferHashTable,
            (const void *)allocedKey,
            (const void *)vulkanFramebuffer);

        SDL_UnlockMutex(renderer->framebufferFetchLock);
    } else {
        LogVulkanResultAsError("vkCreateFramebuffer", result);
        SDL_free(vulkanFramebuffer);
        vulkanFramebuffer = NULL;
    }

    return vulkanFramebuffer;
}

static void VULKAN_INTERNAL_SetCurrentViewport(
    VulkanCommandBuffer *commandBuffer,
    SDL_GPUViewport *viewport)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    vulkanCommandBuffer->currentViewport.x = viewport->x;
    vulkanCommandBuffer->currentViewport.width = viewport->w;
    vulkanCommandBuffer->currentViewport.minDepth = viewport->minDepth;
    vulkanCommandBuffer->currentViewport.maxDepth = viewport->maxDepth;

    // Viewport flip for consistency with other backends
    // FIXME: need moltenVK hack
    vulkanCommandBuffer->currentViewport.y = viewport->y + viewport->h;
    vulkanCommandBuffer->currentViewport.height = -viewport->h;
}

static void VULKAN_SetViewport(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUViewport *viewport)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_SetCurrentViewport(
        vulkanCommandBuffer,
        viewport);

    renderer->vkCmdSetViewport(
        vulkanCommandBuffer->commandBuffer,
        0,
        1,
        &vulkanCommandBuffer->currentViewport);
}

static void VULKAN_INTERNAL_SetCurrentScissor(
    VulkanCommandBuffer *vulkanCommandBuffer,
    SDL_Rect *scissor)
{
    vulkanCommandBuffer->currentScissor.offset.x = scissor->x;
    vulkanCommandBuffer->currentScissor.offset.y = scissor->y;
    vulkanCommandBuffer->currentScissor.extent.width = scissor->w;
    vulkanCommandBuffer->currentScissor.extent.height = scissor->h;
}

static void VULKAN_SetScissor(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Rect *scissor)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_SetCurrentScissor(
        vulkanCommandBuffer,
        scissor);

    renderer->vkCmdSetScissor(
        vulkanCommandBuffer->commandBuffer,
        0,
        1,
        &vulkanCommandBuffer->currentScissor);
}

static void VULKAN_BindVertexSamplers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)textureSamplerBindings[i].texture;
        vulkanCommandBuffer->vertexSamplerTextures[firstSlot + i] = textureContainer->activeTextureHandle->vulkanTexture;
        vulkanCommandBuffer->vertexSamplers[firstSlot + i] = (VulkanSampler *)textureSamplerBindings[i].sampler;

        VULKAN_INTERNAL_TrackSampler(
            vulkanCommandBuffer,
            (VulkanSampler *)textureSamplerBindings[i].sampler);

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            textureContainer->activeTextureHandle->vulkanTexture);
    }

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = true;
}

static void VULKAN_BindVertexStorageTextures(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)storageTextures[i];

        vulkanCommandBuffer->vertexStorageTextures[firstSlot + i] = textureContainer->activeTextureHandle->vulkanTexture;

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            textureContainer->activeTextureHandle->vulkanTexture);
    }

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = true;
}

static void VULKAN_BindVertexStorageBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanBufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        bufferContainer = (VulkanBufferContainer *)storageBuffers[i];

        vulkanCommandBuffer->vertexStorageBuffers[firstSlot + i] = bufferContainer->activeBufferHandle->vulkanBuffer;

        VULKAN_INTERNAL_TrackBuffer(
            vulkanCommandBuffer,
            bufferContainer->activeBufferHandle->vulkanBuffer);
    }

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = true;
}

static void VULKAN_BindFragmentSamplers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)textureSamplerBindings[i].texture;
        vulkanCommandBuffer->fragmentSamplerTextures[firstSlot + i] = textureContainer->activeTextureHandle->vulkanTexture;
        vulkanCommandBuffer->fragmentSamplers[firstSlot + i] = (VulkanSampler *)textureSamplerBindings[i].sampler;

        VULKAN_INTERNAL_TrackSampler(
            vulkanCommandBuffer,
            (VulkanSampler *)textureSamplerBindings[i].sampler);

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            textureContainer->activeTextureHandle->vulkanTexture);
    }

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = true;
}

static void VULKAN_BindFragmentStorageTextures(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)storageTextures[i];

        vulkanCommandBuffer->fragmentStorageTextures[firstSlot + i] =
            textureContainer->activeTextureHandle->vulkanTexture;

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            textureContainer->activeTextureHandle->vulkanTexture);
    }

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = true;
}

static void VULKAN_BindFragmentStorageBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanBufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        bufferContainer = (VulkanBufferContainer *)storageBuffers[i];

        vulkanCommandBuffer->fragmentStorageBuffers[firstSlot + i] = bufferContainer->activeBufferHandle->vulkanBuffer;

        VULKAN_INTERNAL_TrackBuffer(
            vulkanCommandBuffer,
            bufferContainer->activeBufferHandle->vulkanBuffer);
    }

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = true;
}

static VulkanUniformBuffer *VULKAN_INTERNAL_AcquireUniformBufferFromPool(
    VulkanCommandBuffer *commandBuffer)
{
    VulkanRenderer *renderer = commandBuffer->renderer;
    VulkanUniformBuffer *uniformBuffer;

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    if (renderer->uniformBufferPoolCount > 0) {
        uniformBuffer = renderer->uniformBufferPool[renderer->uniformBufferPoolCount - 1];
        renderer->uniformBufferPoolCount -= 1;
    } else {
        uniformBuffer = VULKAN_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    VULKAN_INTERNAL_TrackUniformBuffer(commandBuffer, uniformBuffer);

    return uniformBuffer;
}

static void VULKAN_INTERNAL_ReturnUniformBufferToPool(
    VulkanRenderer *renderer,
    VulkanUniformBuffer *uniformBuffer)
{
    if (renderer->uniformBufferPoolCount >= renderer->uniformBufferPoolCapacity) {
        renderer->uniformBufferPoolCapacity *= 2;
        renderer->uniformBufferPool = SDL_realloc(
            renderer->uniformBufferPool,
            renderer->uniformBufferPoolCapacity * sizeof(VulkanUniformBuffer *));
    }

    renderer->uniformBufferPool[renderer->uniformBufferPoolCount] = uniformBuffer;
    renderer->uniformBufferPoolCount += 1;

    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;
}

static void VULKAN_INTERNAL_PushUniformData(
    VulkanCommandBuffer *commandBuffer,
    VulkanUniformBufferStage uniformBufferStage,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    Uint32 blockSize =
        VULKAN_INTERNAL_NextHighestAlignment32(
            dataLengthInBytes,
            commandBuffer->renderer->minUBOAlignment);

    VulkanUniformBuffer *uniformBuffer;

    if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_VERTEX) {
        if (commandBuffer->vertexUniformBuffers[slotIndex] == NULL) {
            commandBuffer->vertexUniformBuffers[slotIndex] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                commandBuffer);
        }
        uniformBuffer = commandBuffer->vertexUniformBuffers[slotIndex];
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
        if (commandBuffer->fragmentUniformBuffers[slotIndex] == NULL) {
            commandBuffer->fragmentUniformBuffers[slotIndex] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                commandBuffer);
        }
        uniformBuffer = commandBuffer->fragmentUniformBuffers[slotIndex];
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
        if (commandBuffer->computeUniformBuffers[slotIndex] == NULL) {
            commandBuffer->computeUniformBuffers[slotIndex] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                commandBuffer);
        }
        uniformBuffer = commandBuffer->computeUniformBuffers[slotIndex];
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
        return;
    }

    // If there is no more room, acquire a new uniform buffer
    if (uniformBuffer->writeOffset + blockSize + MAX_UBO_SECTION_SIZE >= uniformBuffer->bufferHandle->vulkanBuffer->size) {
        uniformBuffer = VULKAN_INTERNAL_AcquireUniformBufferFromPool(commandBuffer);

        uniformBuffer->drawOffset = 0;
        uniformBuffer->writeOffset = 0;

        if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_VERTEX) {
            commandBuffer->vertexUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewVertexUniformDescriptorSet = true;
        } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
            commandBuffer->fragmentUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewFragmentUniformDescriptorSet = true;
        } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
            commandBuffer->computeUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewComputeUniformDescriptorSet = true;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
            return;
        }
    }

    uniformBuffer->drawOffset = uniformBuffer->writeOffset;

    Uint8 *dst =
        uniformBuffer->bufferHandle->vulkanBuffer->usedRegion->allocation->mapPointer +
        uniformBuffer->bufferHandle->vulkanBuffer->usedRegion->resourceOffset +
        uniformBuffer->writeOffset;

    SDL_memcpy(
        dst,
        data,
        dataLengthInBytes);

    uniformBuffer->writeOffset += blockSize;

    if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_VERTEX) {
        commandBuffer->needNewVertexUniformOffsets = true;
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
        commandBuffer->needNewFragmentUniformOffsets = true;
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
        commandBuffer->needNewComputeUniformOffsets = true;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unrecognized shader stage!");
        return;
    }
}

static void VULKAN_BeginRenderPass(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkRenderPass renderPass;
    VulkanFramebuffer *framebuffer;

    Uint32 w, h;
    VkClearValue *clearValues;
    Uint32 clearCount = colorAttachmentCount;
    Uint32 multisampleAttachmentCount = 0;
    Uint32 totalColorAttachmentCount = 0;
    Uint32 i;
    SDL_GPUViewport defaultViewport;
    SDL_Rect defaultScissor;
    Uint32 framebufferWidth = UINT32_MAX;
    Uint32 framebufferHeight = UINT32_MAX;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)colorAttachmentInfos[i].texture;

        w = textureContainer->activeTextureHandle->vulkanTexture->dimensions.width >> colorAttachmentInfos[i].mipLevel;
        h = textureContainer->activeTextureHandle->vulkanTexture->dimensions.height >> colorAttachmentInfos[i].mipLevel;

        // The framebuffer cannot be larger than the smallest attachment.

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }

        if (h < framebufferHeight) {
            framebufferHeight = h;
        }

        // FIXME: validate this in gpu.c
        if (!(textureContainer->header.info.usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Color attachment texture was not designated as a target!");
            return;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)depthStencilAttachmentInfo->texture;

        w = textureContainer->activeTextureHandle->vulkanTexture->dimensions.width;
        h = textureContainer->activeTextureHandle->vulkanTexture->dimensions.height;

        // The framebuffer cannot be larger than the smallest attachment.

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }

        if (h < framebufferHeight) {
            framebufferHeight = h;
        }

        // FIXME: validate this in gpu.c
        if (!(textureContainer->header.info.usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Depth stencil attachment texture was not designated as a target!");
            return;
        }
    }

    for (i = 0; i < colorAttachmentCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)colorAttachmentInfos[i].texture;
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            textureContainer->header.info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : colorAttachmentInfos[i].layerOrDepthPlane,
            colorAttachmentInfos[i].mipLevel,
            colorAttachmentInfos[i].cycle,
            VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT);

        if (subresource->msaaTexHandle != NULL) {
            // Transition the multisample attachment
            VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT,
                &subresource->msaaTexHandle->vulkanTexture->subresources[0]);

            clearCount += 1;
            multisampleAttachmentCount += 1;
        }

        vulkanCommandBuffer->colorAttachmentSubresources[i] = subresource;

        VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, subresource->parent);
        // TODO: do we need to track the msaa texture? or is it implicitly only used when the regular texture is used?
    }

    vulkanCommandBuffer->colorAttachmentSubresourceCount = colorAttachmentCount;

    if (depthStencilAttachmentInfo != NULL) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)depthStencilAttachmentInfo->texture;
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            0,
            0,
            depthStencilAttachmentInfo->cycle,
            VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT);

        clearCount += 1;

        vulkanCommandBuffer->depthStencilAttachmentSubresource = subresource;

        VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, subresource->parent);
    }

    // Fetch required render objects

    renderPass = VULKAN_INTERNAL_FetchRenderPass(
        renderer,
        vulkanCommandBuffer,
        colorAttachmentInfos,
        colorAttachmentCount,
        depthStencilAttachmentInfo);

    framebuffer = VULKAN_INTERNAL_FetchFramebuffer(
        renderer,
        renderPass,
        colorAttachmentInfos,
        colorAttachmentCount,
        depthStencilAttachmentInfo,
        framebufferWidth,
        framebufferHeight);

    VULKAN_INTERNAL_TrackFramebuffer(renderer, vulkanCommandBuffer, framebuffer);

    // Set clear values

    clearValues = SDL_stack_alloc(VkClearValue, clearCount);

    totalColorAttachmentCount = colorAttachmentCount + multisampleAttachmentCount;

    for (i = 0; i < totalColorAttachmentCount; i += 1) {
        clearValues[i].color.float32[0] = colorAttachmentInfos[i].clearColor.r;
        clearValues[i].color.float32[1] = colorAttachmentInfos[i].clearColor.g;
        clearValues[i].color.float32[2] = colorAttachmentInfos[i].clearColor.b;
        clearValues[i].color.float32[3] = colorAttachmentInfos[i].clearColor.a;

        VulkanTextureContainer *container = (VulkanTextureContainer *)colorAttachmentInfos[i].texture;
        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_FetchTextureSubresource(
            container,
            container->header.info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : colorAttachmentInfos[i].layerOrDepthPlane,
            colorAttachmentInfos[i].mipLevel);

        if (subresource->parent->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            clearValues[i + 1].color.float32[0] = colorAttachmentInfos[i].clearColor.r;
            clearValues[i + 1].color.float32[1] = colorAttachmentInfos[i].clearColor.g;
            clearValues[i + 1].color.float32[2] = colorAttachmentInfos[i].clearColor.b;
            clearValues[i + 1].color.float32[3] = colorAttachmentInfos[i].clearColor.a;
            i += 1;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        clearValues[totalColorAttachmentCount].depthStencil.depth =
            depthStencilAttachmentInfo->depthStencilClearValue.depth;
        clearValues[totalColorAttachmentCount].depthStencil.stencil =
            depthStencilAttachmentInfo->depthStencilClearValue.stencil;
    }

    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffer->framebuffer;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.clearValueCount = clearCount;
    renderPassBeginInfo.renderArea.extent.width = framebufferWidth;
    renderPassBeginInfo.renderArea.extent.height = framebufferHeight;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;

    renderer->vkCmdBeginRenderPass(
        vulkanCommandBuffer->commandBuffer,
        &renderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    SDL_stack_free(clearValues);

    // Set sensible default viewport state

    defaultViewport.x = 0;
    defaultViewport.y = 0;
    defaultViewport.w = (float)framebufferWidth;
    defaultViewport.h = (float)framebufferHeight;
    defaultViewport.minDepth = 0;
    defaultViewport.maxDepth = 1;

    VULKAN_INTERNAL_SetCurrentViewport(
        vulkanCommandBuffer,
        &defaultViewport);

    defaultScissor.x = 0;
    defaultScissor.y = 0;
    defaultScissor.w = (Sint32)framebufferWidth;
    defaultScissor.h = (Sint32)framebufferHeight;

    VULKAN_INTERNAL_SetCurrentScissor(
        vulkanCommandBuffer,
        &defaultScissor);
}

static void VULKAN_BindGraphicsPipeline(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanGraphicsPipeline *pipeline = (VulkanGraphicsPipeline *)graphicsPipeline;

    renderer->vkCmdBindPipeline(
        vulkanCommandBuffer->commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->pipeline);

    vulkanCommandBuffer->currentGraphicsPipeline = pipeline;

    VULKAN_INTERNAL_TrackGraphicsPipeline(vulkanCommandBuffer, pipeline);

    renderer->vkCmdSetViewport(
        vulkanCommandBuffer->commandBuffer,
        0,
        1,
        &vulkanCommandBuffer->currentViewport);

    renderer->vkCmdSetScissor(
        vulkanCommandBuffer->commandBuffer,
        0,
        1,
        &vulkanCommandBuffer->currentScissor);

    // Acquire uniform buffers if necessary
    for (Uint32 i = 0; i < pipeline->resourceLayout.vertexUniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->vertexUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->vertexUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer);
        }
    }

    for (Uint32 i = 0; i < pipeline->resourceLayout.fragmentUniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->fragmentUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->fragmentUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer);
        }
    }

    // Mark bindings as needed
    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = true;
    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = true;
    vulkanCommandBuffer->needNewVertexUniformDescriptorSet = true;
    vulkanCommandBuffer->needNewFragmentUniformDescriptorSet = true;
    vulkanCommandBuffer->needNewVertexUniformOffsets = true;
    vulkanCommandBuffer->needNewFragmentUniformOffsets = true;
}

static void VULKAN_BindVertexBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstBinding,
    SDL_GPUBufferBinding *pBindings,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *currentVulkanBuffer;
    VkBuffer *buffers = SDL_stack_alloc(VkBuffer, bindingCount);
    VkDeviceSize *offsets = SDL_stack_alloc(VkDeviceSize, bindingCount);
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        currentVulkanBuffer = ((VulkanBufferContainer *)pBindings[i].buffer)->activeBufferHandle->vulkanBuffer;
        buffers[i] = currentVulkanBuffer->buffer;
        offsets[i] = (VkDeviceSize)pBindings[i].offset;
        VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, currentVulkanBuffer);
    }

    renderer->vkCmdBindVertexBuffers(
        vulkanCommandBuffer->commandBuffer,
        firstBinding,
        bindingCount,
        buffers,
        offsets);

    SDL_stack_free(buffers);
    SDL_stack_free(offsets);
}

static void VULKAN_BindIndexBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBufferBinding *pBinding,
    SDL_GPUIndexElementSize indexElementSize)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)pBinding->buffer)->activeBufferHandle->vulkanBuffer;

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);

    renderer->vkCmdBindIndexBuffer(
        vulkanCommandBuffer->commandBuffer,
        vulkanBuffer->buffer,
        (VkDeviceSize)pBinding->offset,
        SDLToVK_IndexType[indexElementSize]);
}

static void VULKAN_PushVertexUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    VULKAN_INTERNAL_PushUniformData(
        vulkanCommandBuffer,
        VULKAN_UNIFORM_BUFFER_STAGE_VERTEX,
        slotIndex,
        data,
        dataLengthInBytes);
}

static void VULKAN_PushFragmentUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    VULKAN_INTERNAL_PushUniformData(
        vulkanCommandBuffer,
        VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT,
        slotIndex,
        data,
        dataLengthInBytes);
}

static void VULKAN_EndRenderPass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    Uint32 i;

    renderer->vkCmdEndRenderPass(
        vulkanCommandBuffer->commandBuffer);

    for (i = 0; i < vulkanCommandBuffer->colorAttachmentSubresourceCount; i += 1) {
        VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT,
            vulkanCommandBuffer->colorAttachmentSubresources[i]);
    }
    vulkanCommandBuffer->colorAttachmentSubresourceCount = 0;

    if (vulkanCommandBuffer->depthStencilAttachmentSubresource != NULL) {
        VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT,
            vulkanCommandBuffer->depthStencilAttachmentSubresource);
        vulkanCommandBuffer->depthStencilAttachmentSubresource = NULL;
    }

    vulkanCommandBuffer->currentGraphicsPipeline = NULL;

    vulkanCommandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;

    // Reset bind state
    SDL_zeroa(vulkanCommandBuffer->colorAttachmentSubresources);
    vulkanCommandBuffer->depthStencilAttachmentSubresource = NULL;

    SDL_zeroa(vulkanCommandBuffer->vertexSamplers);
    SDL_zeroa(vulkanCommandBuffer->vertexSamplerTextures);
    SDL_zeroa(vulkanCommandBuffer->vertexStorageTextures);
    SDL_zeroa(vulkanCommandBuffer->vertexStorageBuffers);

    SDL_zeroa(vulkanCommandBuffer->fragmentSamplers);
    SDL_zeroa(vulkanCommandBuffer->fragmentSamplerTextures);
    SDL_zeroa(vulkanCommandBuffer->fragmentStorageTextures);
    SDL_zeroa(vulkanCommandBuffer->fragmentStorageBuffers);
}

static void VULKAN_BeginComputePass(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUStorageTextureWriteOnlyBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GPUStorageBufferWriteOnlyBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanBufferContainer *bufferContainer;
    VulkanBuffer *buffer;
    Uint32 i;

    vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresourceCount = storageTextureBindingCount;

    for (i = 0; i < storageTextureBindingCount; i += 1) {
        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)storageTextureBindings[i].texture;
        if (!(textureContainer->activeTextureHandle->vulkanTexture->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempted to bind read-only texture as compute write texture");
        }

        VulkanTextureSubresource *subresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            storageTextureBindings[i].layer,
            storageTextureBindings[i].mipLevel,
            storageTextureBindings[i].cycle,
            VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE);

        vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresources[i] = subresource;

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            subresource->parent);
    }

    for (i = 0; i < storageBufferBindingCount; i += 1) {
        bufferContainer = (VulkanBufferContainer *)storageBufferBindings[i].buffer;
        buffer = VULKAN_INTERNAL_PrepareBufferForWrite(
            renderer,
            vulkanCommandBuffer,
            bufferContainer,
            storageBufferBindings[i].cycle,
            VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ);

        vulkanCommandBuffer->writeOnlyComputeStorageBuffers[i] = buffer;

        VULKAN_INTERNAL_TrackBuffer(
            vulkanCommandBuffer,
            buffer);
    }
}

static void VULKAN_BindComputePipeline(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUComputePipeline *computePipeline)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanComputePipeline *vulkanComputePipeline = (VulkanComputePipeline *)computePipeline;

    renderer->vkCmdBindPipeline(
        vulkanCommandBuffer->commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        vulkanComputePipeline->pipeline);

    vulkanCommandBuffer->currentComputePipeline = vulkanComputePipeline;

    VULKAN_INTERNAL_TrackComputePipeline(vulkanCommandBuffer, vulkanComputePipeline);

    // Acquire uniform buffers if necessary
    for (Uint32 i = 0; i < vulkanComputePipeline->resourceLayout.uniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->computeUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->computeUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer);
        }
    }

    // Mark binding as needed
    vulkanCommandBuffer->needNewComputeWriteOnlyDescriptorSet = true;
    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = true;
    vulkanCommandBuffer->needNewComputeUniformDescriptorSet = true;
    vulkanCommandBuffer->needNewComputeUniformOffsets = true;
}

static void VULKAN_BindComputeStorageTextures(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageTextures[firstSlot + i] != NULL) {
            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageTextures[firstSlot + i]);
        }

        VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)storageTextures[i];

        vulkanCommandBuffer->readOnlyComputeStorageTextures[firstSlot + i] =
            textureContainer->activeTextureHandle->vulkanTexture;

        VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
            textureContainer->activeTextureHandle->vulkanTexture);

        VULKAN_INTERNAL_TrackTexture(
            vulkanCommandBuffer,
            textureContainer->activeTextureHandle->vulkanTexture);
    }

    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = true;
}

static void VULKAN_BindComputeStorageBuffers(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanBufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageBuffers[firstSlot + i] != NULL) {
            VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageBuffers[firstSlot + i]);
        }

        bufferContainer = (VulkanBufferContainer *)storageBuffers[i];

        vulkanCommandBuffer->readOnlyComputeStorageBuffers[firstSlot + i] = bufferContainer->activeBufferHandle->vulkanBuffer;

        VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ,
            bufferContainer->activeBufferHandle->vulkanBuffer);

        VULKAN_INTERNAL_TrackBuffer(
            vulkanCommandBuffer,
            bufferContainer->activeBufferHandle->vulkanBuffer);
    }

    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = true;
}

static void VULKAN_PushComputeUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    VULKAN_INTERNAL_PushUniformData(
        vulkanCommandBuffer,
        VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE,
        slotIndex,
        data,
        dataLengthInBytes);
}

static void VULKAN_INTERNAL_BindComputeDescriptorSets(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer)
{
    VulkanComputePipelineResourceLayout *resourceLayout;
    VkWriteDescriptorSet *writeDescriptorSets;
    VkWriteDescriptorSet *currentWriteDescriptorSet;
    DescriptorSetPool *descriptorSetPool;
    VkDescriptorBufferInfo bufferInfos[MAX_STORAGE_BUFFERS_PER_STAGE]; // 8 is max for both read and write
    VkDescriptorImageInfo imageInfos[MAX_STORAGE_TEXTURES_PER_STAGE];  // 8 is max for both read and write
    Uint32 dynamicOffsets[MAX_UNIFORM_BUFFERS_PER_STAGE];
    Uint32 bufferInfoCount = 0;
    Uint32 imageInfoCount = 0;
    Uint32 i;

    resourceLayout = &commandBuffer->currentComputePipeline->resourceLayout;

    if (commandBuffer->needNewComputeReadOnlyDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[0];

        commandBuffer->computeReadOnlyDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->readOnlyStorageTextureCount +
                resourceLayout->readOnlyStorageBufferCount);

        for (i = 0; i < resourceLayout->readOnlyStorageTextureCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];
            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeReadOnlyDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = VK_NULL_HANDLE;
            imageInfos[imageInfoCount].imageView = commandBuffer->readOnlyComputeStorageTextures[i]->fullView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->readOnlyStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->readOnlyStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->readOnlyStorageTextureCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeReadOnlyDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->readOnlyComputeStorageBuffers[i]->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = VK_WHOLE_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->readOnlyStorageTextureCount + resourceLayout->readOnlyStorageBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            resourceLayout->pipelineLayout,
            0,
            1,
            &commandBuffer->computeReadOnlyDescriptorSet,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewComputeReadOnlyDescriptorSet = false;
    }

    if (commandBuffer->needNewComputeWriteOnlyDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[1];

        commandBuffer->computeWriteOnlyDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->writeOnlyStorageTextureCount +
                resourceLayout->writeOnlyStorageBufferCount);

        for (i = 0; i < resourceLayout->writeOnlyStorageTextureCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeWriteOnlyDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = VK_NULL_HANDLE;
            imageInfos[imageInfoCount].imageView = commandBuffer->writeOnlyComputeStorageTextureSubresources[i]->computeWriteView;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->writeOnlyStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->writeOnlyStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->writeOnlyStorageTextureCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeWriteOnlyDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->writeOnlyComputeStorageBuffers[i]->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = VK_WHOLE_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->writeOnlyStorageTextureCount + resourceLayout->writeOnlyStorageBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            resourceLayout->pipelineLayout,
            1,
            1,
            &commandBuffer->computeWriteOnlyDescriptorSet,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewComputeWriteOnlyDescriptorSet = false;
    }

    if (commandBuffer->needNewComputeUniformDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[2];

        commandBuffer->computeUniformDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->uniformBufferCount);

        for (i = 0; i < resourceLayout->uniformBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeUniformDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->computeUniformBuffers[i]->bufferHandle->vulkanBuffer->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = MAX_UBO_SECTION_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->uniformBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewComputeUniformDescriptorSet = false;
        commandBuffer->needNewComputeUniformOffsets = true;
    }

    if (commandBuffer->needNewComputeUniformOffsets) {
        for (i = 0; i < resourceLayout->uniformBufferCount; i += 1) {
            dynamicOffsets[i] = commandBuffer->computeUniformBuffers[i]->drawOffset;
        }

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            resourceLayout->pipelineLayout,
            2,
            1,
            &commandBuffer->computeUniformDescriptorSet,
            resourceLayout->uniformBufferCount,
            dynamicOffsets);

        commandBuffer->needNewComputeUniformOffsets = false;
    }
}

static void VULKAN_DispatchCompute(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 groupCountX,
    Uint32 groupCountY,
    Uint32 groupCountZ)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_BindComputeDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDispatch(
        vulkanCommandBuffer->commandBuffer,
        groupCountX,
        groupCountY,
        groupCountZ);
}

static void VULKAN_DispatchComputeIndirect(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)buffer)->activeBufferHandle->vulkanBuffer;

    VULKAN_INTERNAL_BindComputeDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDispatchIndirect(
        vulkanCommandBuffer->commandBuffer,
        vulkanBuffer->buffer,
        offsetInBytes);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

static void VULKAN_EndComputePass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    Uint32 i;

    for (i = 0; i < vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresourceCount; i += 1) {
        VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
            vulkanCommandBuffer->renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
            vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresources[i]);
        vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresources[i] = NULL;
    }
    vulkanCommandBuffer->writeOnlyComputeStorageTextureSubresourceCount = 0;

    for (i = 0; i < MAX_COMPUTE_WRITE_BUFFERS; i += 1) {
        if (vulkanCommandBuffer->writeOnlyComputeStorageBuffers[i] != NULL) {
            VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
                vulkanCommandBuffer->writeOnlyComputeStorageBuffers[i]);

            vulkanCommandBuffer->writeOnlyComputeStorageBuffers[i] = NULL;
        }
    }

    for (i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageTextures[i] != NULL) {
            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageTextures[i]);

            vulkanCommandBuffer->readOnlyComputeStorageTextures[i] = NULL;
        }
    }

    for (i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageBuffers[i] != NULL) {
            VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageBuffers[i]);

            vulkanCommandBuffer->readOnlyComputeStorageBuffers[i] = NULL;
        }
    }

    vulkanCommandBuffer->currentComputePipeline = NULL;

    vulkanCommandBuffer->computeReadOnlyDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->computeWriteOnlyDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;
}

static void *VULKAN_MapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer,
    bool cycle)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)transferBuffer;

    if (
        cycle &&
        SDL_AtomicGet(&transferBufferContainer->activeBufferHandle->vulkanBuffer->referenceCount) > 0) {
        VULKAN_INTERNAL_CycleActiveBuffer(
            renderer,
            transferBufferContainer);
    }

    Uint8 *bufferPointer =
        transferBufferContainer->activeBufferHandle->vulkanBuffer->usedRegion->allocation->mapPointer +
        transferBufferContainer->activeBufferHandle->vulkanBuffer->usedRegion->resourceOffset;

    return bufferPointer;
}

static void VULKAN_UnmapTransferBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUTransferBuffer *transferBuffer)
{
    // no-op because transfer buffers are persistently mapped
    (void)driverData;
    (void)transferBuffer;
}

static void VULKAN_BeginCopyPass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    // no-op
    (void)commandBuffer;
}

static void VULKAN_UploadToTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTextureTransferInfo *source,
    SDL_GPUTextureRegion *destination,
    bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)source->transferBuffer;
    VulkanTextureContainer *vulkanTextureContainer = (VulkanTextureContainer *)destination->texture;
    VulkanTextureSubresource *vulkanTextureSubresource;
    VkBufferImageCopy imageCopy;

    // Note that the transfer buffer does not need a barrier, as it is synced by the client

    vulkanTextureSubresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
        renderer,
        vulkanCommandBuffer,
        vulkanTextureContainer,
        destination->layer,
        destination->mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    imageCopy.imageExtent.width = destination->w;
    imageCopy.imageExtent.height = destination->h;
    imageCopy.imageExtent.depth = destination->d;
    imageCopy.imageOffset.x = destination->x;
    imageCopy.imageOffset.y = destination->y;
    imageCopy.imageOffset.z = destination->z;
    imageCopy.imageSubresource.aspectMask = vulkanTextureSubresource->parent->aspectFlags;
    imageCopy.imageSubresource.baseArrayLayer = destination->layer;
    imageCopy.imageSubresource.layerCount = 1;
    imageCopy.imageSubresource.mipLevel = destination->mipLevel;
    imageCopy.bufferOffset = source->offset;
    imageCopy.bufferRowLength = source->imagePitch;
    imageCopy.bufferImageHeight = source->imageHeight;

    renderer->vkCmdCopyBufferToImage(
        vulkanCommandBuffer->commandBuffer,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        vulkanTextureSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        vulkanTextureSubresource);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, vulkanTextureSubresource->parent);
}

static void VULKAN_UploadToBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTransferBufferLocation *source,
    SDL_GPUBufferRegion *destination,
    bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)source->transferBuffer;
    VulkanBufferContainer *bufferContainer = (VulkanBufferContainer *)destination->buffer;
    VkBufferCopy bufferCopy;

    // Note that the transfer buffer does not need a barrier, as it is synced by the client

    VulkanBuffer *vulkanBuffer = VULKAN_INTERNAL_PrepareBufferForWrite(
        renderer,
        vulkanCommandBuffer,
        bufferContainer,
        cycle,
        VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION);

    bufferCopy.srcOffset = source->offset;
    bufferCopy.dstOffset = destination->offset;
    bufferCopy.size = destination->size;

    renderer->vkCmdCopyBuffer(
        vulkanCommandBuffer->commandBuffer,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        vulkanBuffer->buffer,
        1,
        &bufferCopy);

    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION,
        vulkanBuffer);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

// Readback

static void VULKAN_DownloadFromTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTextureRegion *source,
    SDL_GPUTextureTransferInfo *destination)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanTextureContainer *textureContainer = (VulkanTextureContainer *)source->texture;
    VulkanTextureSubresource *vulkanTextureSubresource;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)destination->transferBuffer;
    VkBufferImageCopy imageCopy;
    vulkanTextureSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
        textureContainer,
        source->layer,
        source->mipLevel);

    // Note that the transfer buffer does not need a barrier, as it is synced by the client

    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        vulkanTextureSubresource);

    imageCopy.imageExtent.width = source->w;
    imageCopy.imageExtent.height = source->h;
    imageCopy.imageExtent.depth = source->d;
    imageCopy.imageOffset.x = source->x;
    imageCopy.imageOffset.y = source->y;
    imageCopy.imageOffset.z = source->z;
    imageCopy.imageSubresource.aspectMask = vulkanTextureSubresource->parent->aspectFlags;
    imageCopy.imageSubresource.baseArrayLayer = source->layer;
    imageCopy.imageSubresource.layerCount = 1;
    imageCopy.imageSubresource.mipLevel = source->mipLevel;
    imageCopy.bufferOffset = destination->offset;
    imageCopy.bufferRowLength = destination->imagePitch;
    imageCopy.bufferImageHeight = destination->imageHeight;

    renderer->vkCmdCopyImageToBuffer(
        vulkanCommandBuffer->commandBuffer,
        vulkanTextureSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        vulkanTextureSubresource);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, vulkanTextureSubresource->parent);
}

static void VULKAN_DownloadFromBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBufferRegion *source,
    SDL_GPUTransferBufferLocation *destination)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanBufferContainer *bufferContainer = (VulkanBufferContainer *)source->buffer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)destination->transferBuffer;
    VkBufferCopy bufferCopy;

    // Note that transfer buffer does not need a barrier, as it is synced by the client

    VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
        bufferContainer->activeBufferHandle->vulkanBuffer);

    bufferCopy.srcOffset = source->offset;
    bufferCopy.dstOffset = destination->offset;
    bufferCopy.size = source->size;

    renderer->vkCmdCopyBuffer(
        vulkanCommandBuffer->commandBuffer,
        bufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        1,
        &bufferCopy);

    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
        bufferContainer->activeBufferHandle->vulkanBuffer);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, bufferContainer->activeBufferHandle->vulkanBuffer);
}

static void VULKAN_CopyTextureToTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTextureLocation *source,
    SDL_GPUTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanTextureSubresource *srcSubresource;
    VulkanTextureSubresource *dstSubresource;
    VkImageCopy imageCopy;

    srcSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
        (VulkanTextureContainer *)source->texture,
        source->layer,
        source->mipLevel);

    dstSubresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
        renderer,
        vulkanCommandBuffer,
        (VulkanTextureContainer *)destination->texture,
        destination->layer,
        destination->mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSubresource);

    imageCopy.srcOffset.x = source->x;
    imageCopy.srcOffset.y = source->y;
    imageCopy.srcOffset.z = source->z;
    imageCopy.srcSubresource.aspectMask = srcSubresource->parent->aspectFlags;
    imageCopy.srcSubresource.baseArrayLayer = source->layer;
    imageCopy.srcSubresource.layerCount = 1;
    imageCopy.srcSubresource.mipLevel = source->mipLevel;
    imageCopy.dstOffset.x = destination->x;
    imageCopy.dstOffset.y = destination->y;
    imageCopy.dstOffset.z = destination->z;
    imageCopy.dstSubresource.aspectMask = dstSubresource->parent->aspectFlags;
    imageCopy.dstSubresource.baseArrayLayer = destination->layer;
    imageCopy.dstSubresource.layerCount = 1;
    imageCopy.dstSubresource.mipLevel = destination->mipLevel;
    imageCopy.extent.width = w;
    imageCopy.extent.height = h;
    imageCopy.extent.depth = d;

    renderer->vkCmdCopyImage(
        vulkanCommandBuffer->commandBuffer,
        srcSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSubresource);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        dstSubresource);

    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, srcSubresource->parent);
    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, dstSubresource->parent);
}

static void VULKAN_CopyBufferToBuffer(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBufferLocation *source,
    SDL_GPUBufferLocation *destination,
    Uint32 size,
    bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBufferContainer *srcContainer = (VulkanBufferContainer *)source->buffer;
    VulkanBufferContainer *dstContainer = (VulkanBufferContainer *)destination->buffer;
    VkBufferCopy bufferCopy;

    VulkanBuffer *dstBuffer = VULKAN_INTERNAL_PrepareBufferForWrite(
        renderer,
        vulkanCommandBuffer,
        dstContainer,
        cycle,
        VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION);

    VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
        srcContainer->activeBufferHandle->vulkanBuffer);

    bufferCopy.srcOffset = source->offset;
    bufferCopy.dstOffset = destination->offset;
    bufferCopy.size = size;

    renderer->vkCmdCopyBuffer(
        vulkanCommandBuffer->commandBuffer,
        srcContainer->activeBufferHandle->vulkanBuffer->buffer,
        dstBuffer->buffer,
        1,
        &bufferCopy);

    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
        srcContainer->activeBufferHandle->vulkanBuffer);

    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION,
        dstBuffer);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, srcContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, dstBuffer);
}

static void VULKAN_GenerateMipmaps(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTexture *texture)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanTexture *vulkanTexture = ((VulkanTextureContainer *)texture)->activeTextureHandle->vulkanTexture;
    VulkanTextureSubresource *srcTextureSubresource;
    VulkanTextureSubresource *dstTextureSubresource;
    VkImageBlit blit;

    // Blit each slice sequentially. Barriers, barriers everywhere!
    for (Uint32 layerOrDepthIndex = 0; layerOrDepthIndex < vulkanTexture->layerCount; layerOrDepthIndex += 1)
        for (Uint32 level = 1; level < vulkanTexture->levelCount; level += 1) {
            Uint32 layer = vulkanTexture->type == SDL_GPU_TEXTURETYPE_3D ? 0 : layerOrDepthIndex;
            Uint32 depth = vulkanTexture->type == SDL_GPU_TEXTURETYPE_3D ? layerOrDepthIndex : 0;

            Uint32 srcSubresourceIndex = VULKAN_INTERNAL_GetTextureSubresourceIndex(
                level - 1,
                layer,
                vulkanTexture->levelCount);
            Uint32 dstSubresourceIndex = VULKAN_INTERNAL_GetTextureSubresourceIndex(
                level,
                layer,
                vulkanTexture->levelCount);

            srcTextureSubresource = &vulkanTexture->subresources[srcSubresourceIndex];
            dstTextureSubresource = &vulkanTexture->subresources[dstSubresourceIndex];

            VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                srcTextureSubresource);

            VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                dstTextureSubresource);

            blit.srcOffsets[0].x = 0;
            blit.srcOffsets[0].y = 0;
            blit.srcOffsets[0].z = depth;

            blit.srcOffsets[1].x = vulkanTexture->dimensions.width >> (level - 1);
            blit.srcOffsets[1].y = vulkanTexture->dimensions.height >> (level - 1);
            blit.srcOffsets[1].z = depth + 1;

            blit.dstOffsets[0].x = 0;
            blit.dstOffsets[0].y = 0;
            blit.dstOffsets[0].z = depth;

            blit.dstOffsets[1].x = vulkanTexture->dimensions.width >> level;
            blit.dstOffsets[1].y = vulkanTexture->dimensions.height >> level;
            blit.dstOffsets[1].z = depth + 1;

            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.baseArrayLayer = layer;
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.mipLevel = level - 1;

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.baseArrayLayer = layer;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.mipLevel = level;

            renderer->vkCmdBlitImage(
                vulkanCommandBuffer->commandBuffer,
                vulkanTexture->image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                vulkanTexture->image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR);

            VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                srcTextureSubresource);

            VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                dstTextureSubresource);

            VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, srcTextureSubresource->parent);
            VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, dstTextureSubresource->parent);
        }
}

static void VULKAN_EndCopyPass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    // no-op
    (void)commandBuffer;
}

static void VULKAN_Blit(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBlitRegion *source,
    SDL_GPUBlitRegion *destination,
    SDL_FlipMode flipMode,
    SDL_GPUFilter filterMode,
    bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    TextureCommonHeader *srcHeader = (TextureCommonHeader *)source->texture;
    TextureCommonHeader *dstHeader = (TextureCommonHeader *)destination->texture;
    VkImageBlit region;
    Uint32 srcLayer = srcHeader->info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : source->layerOrDepthPlane;
    Uint32 srcDepth = srcHeader->info.type == SDL_GPU_TEXTURETYPE_3D ? source->layerOrDepthPlane : 0;
    Uint32 dstLayer = dstHeader->info.type == SDL_GPU_TEXTURETYPE_3D ? 0 : destination->layerOrDepthPlane;
    Uint32 dstDepth = dstHeader->info.type == SDL_GPU_TEXTURETYPE_3D ? destination->layerOrDepthPlane : 0;
    int32_t swap;

    VulkanTextureSubresource *srcSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
        (VulkanTextureContainer *)source->texture,
        srcLayer,
        source->mipLevel);

    VulkanTextureSubresource *dstSubresource = VULKAN_INTERNAL_PrepareTextureSubresourceForWrite(
        renderer,
        vulkanCommandBuffer,
        (VulkanTextureContainer *)destination->texture,
        dstLayer,
        destination->mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSubresource);

    region.srcSubresource.aspectMask = srcSubresource->parent->aspectFlags;
    region.srcSubresource.baseArrayLayer = srcSubresource->layer;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.mipLevel = srcSubresource->level;
    region.srcOffsets[0].x = source->x;
    region.srcOffsets[0].y = source->y;
    region.srcOffsets[0].z = srcDepth;
    region.srcOffsets[1].x = source->x + source->w;
    region.srcOffsets[1].y = source->y + source->h;
    region.srcOffsets[1].z = srcDepth + 1;

    if (flipMode & SDL_FLIP_HORIZONTAL) {
        // flip the x positions
        swap = region.srcOffsets[0].x;
        region.srcOffsets[0].x = region.srcOffsets[1].x;
        region.srcOffsets[1].x = swap;
    }

    if (flipMode & SDL_FLIP_VERTICAL) {
        // flip the y positions
        swap = region.srcOffsets[0].y;
        region.srcOffsets[0].y = region.srcOffsets[1].y;
        region.srcOffsets[1].y = swap;
    }

    region.dstSubresource.aspectMask = dstSubresource->parent->aspectFlags;
    region.dstSubresource.baseArrayLayer = dstSubresource->layer;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.mipLevel = dstSubresource->level;
    region.dstOffsets[0].x = destination->x;
    region.dstOffsets[0].y = destination->y;
    region.dstOffsets[0].z = dstDepth;
    region.dstOffsets[1].x = destination->x + destination->w;
    region.dstOffsets[1].y = destination->y + destination->h;
    region.dstOffsets[1].z = dstDepth + 1;

    renderer->vkCmdBlitImage(
        vulkanCommandBuffer->commandBuffer,
        srcSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstSubresource->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region,
        SDLToVK_Filter[filterMode]);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSubresource);

    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        dstSubresource);

    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, srcSubresource->parent);
    VULKAN_INTERNAL_TrackTexture(vulkanCommandBuffer, dstSubresource->parent);
}

static void VULKAN_INTERNAL_AllocateCommandBuffers(
    VulkanRenderer *renderer,
    VulkanCommandPool *vulkanCommandPool,
    Uint32 allocateCount)
{
    VkCommandBufferAllocateInfo allocateInfo;
    VkResult vulkanResult;
    Uint32 i;
    VkCommandBuffer *commandBuffers = SDL_stack_alloc(VkCommandBuffer, allocateCount);
    VulkanCommandBuffer *commandBuffer;

    vulkanCommandPool->inactiveCommandBufferCapacity += allocateCount;

    vulkanCommandPool->inactiveCommandBuffers = SDL_realloc(
        vulkanCommandPool->inactiveCommandBuffers,
        sizeof(VulkanCommandBuffer *) *
            vulkanCommandPool->inactiveCommandBufferCapacity);

    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = NULL;
    allocateInfo.commandPool = vulkanCommandPool->commandPool;
    allocateInfo.commandBufferCount = allocateCount;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vulkanResult = renderer->vkAllocateCommandBuffers(
        renderer->logicalDevice,
        &allocateInfo,
        commandBuffers);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkAllocateCommandBuffers", vulkanResult);
        SDL_stack_free(commandBuffers);
        return;
    }

    for (i = 0; i < allocateCount; i += 1) {
        commandBuffer = SDL_malloc(sizeof(VulkanCommandBuffer));
        commandBuffer->renderer = renderer;
        commandBuffer->commandPool = vulkanCommandPool;
        commandBuffer->commandBuffer = commandBuffers[i];

        commandBuffer->inFlightFence = VK_NULL_HANDLE;

        // Presentation tracking

        commandBuffer->presentDataCapacity = 1;
        commandBuffer->presentDataCount = 0;
        commandBuffer->presentDatas = SDL_malloc(
            commandBuffer->presentDataCapacity * sizeof(VulkanPresentData));

        commandBuffer->waitSemaphoreCapacity = 1;
        commandBuffer->waitSemaphoreCount = 0;
        commandBuffer->waitSemaphores = SDL_malloc(
            commandBuffer->waitSemaphoreCapacity * sizeof(VkSemaphore));

        commandBuffer->signalSemaphoreCapacity = 1;
        commandBuffer->signalSemaphoreCount = 0;
        commandBuffer->signalSemaphores = SDL_malloc(
            commandBuffer->signalSemaphoreCapacity * sizeof(VkSemaphore));

        // Descriptor set tracking

        commandBuffer->boundDescriptorSetDataCapacity = 16;
        commandBuffer->boundDescriptorSetDataCount = 0;
        commandBuffer->boundDescriptorSetDatas = SDL_malloc(
            commandBuffer->boundDescriptorSetDataCapacity * sizeof(DescriptorSetData));

        // Resource bind tracking

        commandBuffer->needNewVertexResourceDescriptorSet = true;
        commandBuffer->needNewVertexUniformDescriptorSet = true;
        commandBuffer->needNewVertexUniformOffsets = true;
        commandBuffer->needNewFragmentResourceDescriptorSet = true;
        commandBuffer->needNewFragmentUniformDescriptorSet = true;
        commandBuffer->needNewFragmentUniformOffsets = true;

        commandBuffer->needNewComputeWriteOnlyDescriptorSet = true;
        commandBuffer->needNewComputeReadOnlyDescriptorSet = true;
        commandBuffer->needNewComputeUniformDescriptorSet = true;
        commandBuffer->needNewComputeUniformOffsets = true;

        commandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;

        commandBuffer->computeReadOnlyDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->computeWriteOnlyDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;

        // Resource tracking

        commandBuffer->usedBufferCapacity = 4;
        commandBuffer->usedBufferCount = 0;
        commandBuffer->usedBuffers = SDL_malloc(
            commandBuffer->usedBufferCapacity * sizeof(VulkanBuffer *));

        commandBuffer->usedTextureCapacity = 4;
        commandBuffer->usedTextureCount = 0;
        commandBuffer->usedTextures = SDL_malloc(
            commandBuffer->usedTextureCapacity * sizeof(VulkanTexture *));

        commandBuffer->usedSamplerCapacity = 4;
        commandBuffer->usedSamplerCount = 0;
        commandBuffer->usedSamplers = SDL_malloc(
            commandBuffer->usedSamplerCapacity * sizeof(VulkanSampler *));

        commandBuffer->usedGraphicsPipelineCapacity = 4;
        commandBuffer->usedGraphicsPipelineCount = 0;
        commandBuffer->usedGraphicsPipelines = SDL_malloc(
            commandBuffer->usedGraphicsPipelineCapacity * sizeof(VulkanGraphicsPipeline *));

        commandBuffer->usedComputePipelineCapacity = 4;
        commandBuffer->usedComputePipelineCount = 0;
        commandBuffer->usedComputePipelines = SDL_malloc(
            commandBuffer->usedComputePipelineCapacity * sizeof(VulkanComputePipeline *));

        commandBuffer->usedFramebufferCapacity = 4;
        commandBuffer->usedFramebufferCount = 0;
        commandBuffer->usedFramebuffers = SDL_malloc(
            commandBuffer->usedFramebufferCapacity * sizeof(VulkanFramebuffer *));

        commandBuffer->usedUniformBufferCapacity = 4;
        commandBuffer->usedUniformBufferCount = 0;
        commandBuffer->usedUniformBuffers = SDL_malloc(
            commandBuffer->usedUniformBufferCapacity * sizeof(VulkanUniformBuffer *));

        // Pool it!

        vulkanCommandPool->inactiveCommandBuffers[vulkanCommandPool->inactiveCommandBufferCount] = commandBuffer;
        vulkanCommandPool->inactiveCommandBufferCount += 1;
    }

    SDL_stack_free(commandBuffers);
}

static VulkanCommandPool *VULKAN_INTERNAL_FetchCommandPool(
    VulkanRenderer *renderer,
    SDL_ThreadID threadID)
{
    VulkanCommandPool *vulkanCommandPool = NULL;
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    VkResult vulkanResult;
    CommandPoolHashTableKey key;
    key.threadID = threadID;

    bool result = SDL_FindInHashTable(
        renderer->commandPoolHashTable,
        (const void *)&key,
        (const void **)&vulkanCommandPool);

    if (result) {
        return vulkanCommandPool;
    }

    vulkanCommandPool = (VulkanCommandPool *)SDL_malloc(sizeof(VulkanCommandPool));

    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = NULL;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = renderer->queueFamilyIndex;

    vulkanResult = renderer->vkCreateCommandPool(
        renderer->logicalDevice,
        &commandPoolCreateInfo,
        NULL,
        &vulkanCommandPool->commandPool);

    if (vulkanResult != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create command pool!");
        LogVulkanResultAsError("vkCreateCommandPool", vulkanResult);
        return NULL;
    }

    vulkanCommandPool->threadID = threadID;

    vulkanCommandPool->inactiveCommandBufferCapacity = 0;
    vulkanCommandPool->inactiveCommandBufferCount = 0;
    vulkanCommandPool->inactiveCommandBuffers = NULL;

    VULKAN_INTERNAL_AllocateCommandBuffers(
        renderer,
        vulkanCommandPool,
        2);

    CommandPoolHashTableKey *allocedKey = SDL_malloc(sizeof(CommandPoolHashTableKey));
    allocedKey->threadID = threadID;

    SDL_InsertIntoHashTable(
        renderer->commandPoolHashTable,
        (const void *)allocedKey,
        (const void *)vulkanCommandPool);

    return vulkanCommandPool;
}

static VulkanCommandBuffer *VULKAN_INTERNAL_GetInactiveCommandBufferFromPool(
    VulkanRenderer *renderer,
    SDL_ThreadID threadID)
{
    VulkanCommandPool *commandPool =
        VULKAN_INTERNAL_FetchCommandPool(renderer, threadID);
    VulkanCommandBuffer *commandBuffer;

    if (commandPool == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to fetch command pool!");
        return NULL;
    }

    if (commandPool->inactiveCommandBufferCount == 0) {
        VULKAN_INTERNAL_AllocateCommandBuffers(
            renderer,
            commandPool,
            commandPool->inactiveCommandBufferCapacity);
    }

    commandBuffer = commandPool->inactiveCommandBuffers[commandPool->inactiveCommandBufferCount - 1];
    commandPool->inactiveCommandBufferCount -= 1;

    return commandBuffer;
}

static SDL_GPUCommandBuffer *VULKAN_AcquireCommandBuffer(
    SDL_GPURenderer *driverData)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkResult result;
    Uint32 i;

    SDL_ThreadID threadID = SDL_GetCurrentThreadID();

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    VulkanCommandBuffer *commandBuffer =
        VULKAN_INTERNAL_GetInactiveCommandBufferFromPool(renderer, threadID);

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    if (commandBuffer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to acquire command buffer!");
        return NULL;
    }

    // Reset state

    commandBuffer->currentComputePipeline = NULL;
    commandBuffer->currentGraphicsPipeline = NULL;

    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        commandBuffer->colorAttachmentSubresources[i] = NULL;
    }

    for (i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
        commandBuffer->vertexUniformBuffers[i] = NULL;
        commandBuffer->fragmentUniformBuffers[i] = NULL;
        commandBuffer->computeUniformBuffers[i] = NULL;
    }

    commandBuffer->depthStencilAttachmentSubresource = NULL;

    commandBuffer->needNewVertexResourceDescriptorSet = true;
    commandBuffer->needNewVertexUniformDescriptorSet = true;
    commandBuffer->needNewVertexUniformOffsets = true;
    commandBuffer->needNewFragmentResourceDescriptorSet = true;
    commandBuffer->needNewFragmentUniformDescriptorSet = true;
    commandBuffer->needNewFragmentUniformOffsets = true;

    commandBuffer->needNewComputeReadOnlyDescriptorSet = true;
    commandBuffer->needNewComputeUniformDescriptorSet = true;
    commandBuffer->needNewComputeUniformOffsets = true;

    commandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;

    commandBuffer->computeReadOnlyDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->computeWriteOnlyDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;

    SDL_zeroa(commandBuffer->vertexSamplerTextures);
    SDL_zeroa(commandBuffer->vertexSamplers);
    SDL_zeroa(commandBuffer->vertexStorageTextures);
    SDL_zeroa(commandBuffer->vertexStorageBuffers);

    SDL_zeroa(commandBuffer->fragmentSamplerTextures);
    SDL_zeroa(commandBuffer->fragmentSamplers);
    SDL_zeroa(commandBuffer->fragmentStorageTextures);
    SDL_zeroa(commandBuffer->fragmentStorageBuffers);

    SDL_zeroa(commandBuffer->writeOnlyComputeStorageTextureSubresources);
    commandBuffer->writeOnlyComputeStorageTextureSubresourceCount = 0;
    SDL_zeroa(commandBuffer->writeOnlyComputeStorageBuffers);
    SDL_zeroa(commandBuffer->readOnlyComputeStorageTextures);
    SDL_zeroa(commandBuffer->readOnlyComputeStorageBuffers);

    commandBuffer->autoReleaseFence = 1;

    commandBuffer->isDefrag = 0;

    /* Reset the command buffer here to avoid resets being called
     * from a separate thread than where the command buffer was acquired
     */
    result = renderer->vkResetCommandBuffer(
        commandBuffer->commandBuffer,
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    if (result != VK_SUCCESS) {
        LogVulkanResultAsError("vkResetCommandBuffer", result);
    }

    VULKAN_INTERNAL_BeginCommandBuffer(renderer, commandBuffer);

    return (SDL_GPUCommandBuffer *)commandBuffer;
}

static bool VULKAN_QueryFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkResult result;

    result = renderer->vkGetFenceStatus(
        renderer->logicalDevice,
        ((VulkanFenceHandle *)fence)->fence);

    if (result == VK_SUCCESS) {
        return 1;
    } else if (result == VK_NOT_READY) {
        return 0;
    } else {
        LogVulkanResultAsError("vkGetFenceStatus", result);
        return 0;
    }
}

static void VULKAN_INTERNAL_ReturnFenceToPool(
    VulkanRenderer *renderer,
    VulkanFenceHandle *fenceHandle)
{
    SDL_LockMutex(renderer->fencePool.lock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->fencePool.availableFences,
        VulkanFenceHandle *,
        renderer->fencePool.availableFenceCount + 1,
        renderer->fencePool.availableFenceCapacity,
        renderer->fencePool.availableFenceCapacity * 2);

    renderer->fencePool.availableFences[renderer->fencePool.availableFenceCount] = fenceHandle;
    renderer->fencePool.availableFenceCount += 1;

    SDL_UnlockMutex(renderer->fencePool.lock);
}

static void VULKAN_ReleaseFence(
    SDL_GPURenderer *driverData,
    SDL_GPUFence *fence)
{
    VulkanFenceHandle *handle = (VulkanFenceHandle *)fence;

    if (SDL_AtomicDecRef(&handle->referenceCount)) {
        VULKAN_INTERNAL_ReturnFenceToPool((VulkanRenderer *)driverData, handle);
    }
}

static WindowData *VULKAN_INTERNAL_FetchWindowData(
    SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (WindowData *)SDL_GetPointerProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static SDL_bool VULKAN_INTERNAL_OnWindowResize(void *userdata, SDL_Event *e)
{
    SDL_Window *w = (SDL_Window *)userdata;
    WindowData *data;
    if (e->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        data = VULKAN_INTERNAL_FetchWindowData(w);
        data->needsSwapchainRecreate = true;
    }

    return true;
}

static bool VULKAN_SupportsSwapchainComposition(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);
    VkSurfaceKHR surface;
    SwapchainSupportDetails supportDetails;
    bool result = false;

    if (windowData == NULL || windowData->swapchainData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Must claim window before querying swapchain composition support!");
        return false;
    }

    surface = windowData->swapchainData->surface;

    if (VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            surface,
            &supportDetails)) {

        result = VULKAN_INTERNAL_VerifySwapSurfaceFormat(
            SwapchainCompositionToFormat[swapchainComposition],
            SwapchainCompositionToColorSpace[swapchainComposition],
            supportDetails.formats,
            supportDetails.formatsLength);

        if (!result) {
            // Let's try again with the fallback format...
            result = VULKAN_INTERNAL_VerifySwapSurfaceFormat(
                SwapchainCompositionToFallbackFormat[swapchainComposition],
                SwapchainCompositionToColorSpace[swapchainComposition],
                supportDetails.formats,
                supportDetails.formatsLength);
        }

        SDL_free(supportDetails.formats);
        SDL_free(supportDetails.presentModes);
    }

    return result;
}

static bool VULKAN_SupportsPresentMode(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUPresentMode presentMode)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);
    VkSurfaceKHR surface;
    SwapchainSupportDetails supportDetails;
    bool result = false;

    if (windowData == NULL || windowData->swapchainData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Must claim window before querying present mode support!");
        return false;
    }

    surface = windowData->swapchainData->surface;

    if (VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            surface,
            &supportDetails)) {

        result = VULKAN_INTERNAL_VerifySwapPresentMode(
            SDLToVK_PresentMode[presentMode],
            supportDetails.presentModes,
            supportDetails.presentModesLength);

        SDL_free(supportDetails.formats);
        SDL_free(supportDetails.presentModes);
    }

    return result;
}

static bool VULKAN_ClaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = SDL_malloc(sizeof(WindowData));
        windowData->window = window;
        windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

        if (VULKAN_INTERNAL_CreateSwapchain(renderer, windowData)) {
            SDL_SetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(WindowData *));
            }

            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            SDL_AddEventWatch(VULKAN_INTERNAL_OnWindowResize, window);

            return 1;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not create swapchain, failed to claim window!");
            SDL_free(windowData);
            return 0;
        }
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Window already claimed!");
        return 0;
    }
}

static void VULKAN_UnclaimWindow(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);
    Uint32 i;

    if (windowData == NULL) {
        return;
    }

    if (windowData->swapchainData != NULL) {
        VULKAN_Wait(driverData);

        for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
            if (windowData->swapchainData->inFlightFences[i] != NULL) {
                VULKAN_ReleaseFence(
                    driverData,
                    (SDL_GPUFence *)windowData->swapchainData->inFlightFences[i]);
            }
        }

        VULKAN_INTERNAL_DestroySwapchain(
            (VulkanRenderer *)driverData,
            windowData);
    }

    for (i = 0; i < renderer->claimedWindowCount; i += 1) {
        if (renderer->claimedWindows[i]->window == window) {
            renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
            renderer->claimedWindowCount -= 1;
            break;
        }
    }

    SDL_free(windowData);

    SDL_ClearProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA);
    SDL_DelEventWatch(VULKAN_INTERNAL_OnWindowResize, window);
}

static bool VULKAN_INTERNAL_RecreateSwapchain(
    VulkanRenderer *renderer,
    WindowData *windowData)
{
    Uint32 i;

    if (windowData->swapchainData != NULL) {
        VULKAN_Wait((SDL_GPURenderer *)renderer);

        for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
            if (windowData->swapchainData->inFlightFences[i] != NULL) {
                VULKAN_ReleaseFence(
                    (SDL_GPURenderer *)renderer,
                    (SDL_GPUFence *)windowData->swapchainData->inFlightFences[i]);
            }
        }
    }

    VULKAN_INTERNAL_DestroySwapchain(renderer, windowData);
    return VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);
}

static SDL_GPUTexture *VULKAN_AcquireSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    Uint32 swapchainImageIndex;
    WindowData *windowData;
    VulkanSwapchainData *swapchainData;
    VkResult acquireResult = VK_SUCCESS;
    VulkanTextureContainer *swapchainTextureContainer = NULL;
    VulkanPresentData *presentData;

    windowData = VULKAN_INTERNAL_FetchWindowData(window);
    if (windowData == NULL) {
        return NULL;
    }

    swapchainData = windowData->swapchainData;

    // Window is claimed but swapchain is invalid!
    if (swapchainData == NULL) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            // Window is minimized, don't bother
            return NULL;
        }

        // Let's try to recreate
        VULKAN_INTERNAL_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain!");
            return NULL;
        }
    }

    if (swapchainData->inFlightFences[swapchainData->frameCounter] != NULL) {
        if (swapchainData->presentMode == VK_PRESENT_MODE_FIFO_KHR) {
            // In VSYNC mode, block until the least recent presented frame is done
            VULKAN_WaitForFences(
                (SDL_GPURenderer *)renderer,
                true,
                (SDL_GPUFence **)&swapchainData->inFlightFences[swapchainData->frameCounter],
                1);
        } else {
            if (!VULKAN_QueryFence(
                    (SDL_GPURenderer *)renderer,
                    (SDL_GPUFence *)swapchainData->inFlightFences[swapchainData->frameCounter])) {
                /*
                 * In MAILBOX or IMMEDIATE mode, if the least recent fence is not signaled,
                 * return NULL to indicate that rendering should be skipped
                 */
                return NULL;
            }
        }

        VULKAN_ReleaseFence(
            (SDL_GPURenderer *)renderer,
            (SDL_GPUFence *)swapchainData->inFlightFences[swapchainData->frameCounter]);

        swapchainData->inFlightFences[swapchainData->frameCounter] = NULL;
    }

    // If window data marked as needing swapchain recreate, try to recreate
    if (windowData->needsSwapchainRecreate) {
        VULKAN_INTERNAL_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain!");
            return NULL;
        }
    }

    // Finally, try to acquire!
    acquireResult = renderer->vkAcquireNextImageKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        UINT64_MAX,
        swapchainData->imageAvailableSemaphore[swapchainData->frameCounter],
        VK_NULL_HANDLE,
        &swapchainImageIndex);

    // Acquisition is invalid, let's try to recreate
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        VULKAN_INTERNAL_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to recreate swapchain!");
            return NULL;
        }

        acquireResult = renderer->vkAcquireNextImageKHR(
            renderer->logicalDevice,
            swapchainData->swapchain,
            UINT64_MAX,
            swapchainData->imageAvailableSemaphore[swapchainData->frameCounter],
            VK_NULL_HANDLE,
            &swapchainImageIndex);

        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to acquire swapchain texture!");
            return NULL;
        }
    }

    swapchainTextureContainer = &swapchainData->textureContainers[swapchainImageIndex];

    // We need a special execution dependency with pWaitDstStageMask or image transition can start before acquire finishes

    VkImageMemoryBarrier imageBarrier;
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = NULL;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = swapchainTextureContainer->activeTextureHandle->vulkanTexture->image;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    renderer->vkCmdPipelineBarrier(
        vulkanCommandBuffer->commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &imageBarrier);

    // Set up present struct

    if (vulkanCommandBuffer->presentDataCount == vulkanCommandBuffer->presentDataCapacity) {
        vulkanCommandBuffer->presentDataCapacity += 1;
        vulkanCommandBuffer->presentDatas = SDL_realloc(
            vulkanCommandBuffer->presentDatas,
            vulkanCommandBuffer->presentDataCapacity * sizeof(VulkanPresentData));
    }

    presentData = &vulkanCommandBuffer->presentDatas[vulkanCommandBuffer->presentDataCount];
    vulkanCommandBuffer->presentDataCount += 1;

    presentData->windowData = windowData;
    presentData->swapchainImageIndex = swapchainImageIndex;

    // Set up present semaphores

    if (vulkanCommandBuffer->waitSemaphoreCount == vulkanCommandBuffer->waitSemaphoreCapacity) {
        vulkanCommandBuffer->waitSemaphoreCapacity += 1;
        vulkanCommandBuffer->waitSemaphores = SDL_realloc(
            vulkanCommandBuffer->waitSemaphores,
            vulkanCommandBuffer->waitSemaphoreCapacity * sizeof(VkSemaphore));
    }

    vulkanCommandBuffer->waitSemaphores[vulkanCommandBuffer->waitSemaphoreCount] =
        swapchainData->imageAvailableSemaphore[swapchainData->frameCounter];
    vulkanCommandBuffer->waitSemaphoreCount += 1;

    if (vulkanCommandBuffer->signalSemaphoreCount == vulkanCommandBuffer->signalSemaphoreCapacity) {
        vulkanCommandBuffer->signalSemaphoreCapacity += 1;
        vulkanCommandBuffer->signalSemaphores = SDL_realloc(
            vulkanCommandBuffer->signalSemaphores,
            vulkanCommandBuffer->signalSemaphoreCapacity * sizeof(VkSemaphore));
    }

    vulkanCommandBuffer->signalSemaphores[vulkanCommandBuffer->signalSemaphoreCount] =
        swapchainData->renderFinishedSemaphore[swapchainData->frameCounter];
    vulkanCommandBuffer->signalSemaphoreCount += 1;

    *pWidth = swapchainData->textureContainers[swapchainData->frameCounter].header.info.width;
    *pHeight = swapchainData->textureContainers[swapchainData->frameCounter].header.info.height;

    return (SDL_GPUTexture *)swapchainTextureContainer;
}

static SDL_GPUTextureFormat VULKAN_GetSwapchainTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_Window *window)
{
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Cannot get swapchain format, window has not been claimed!");
        return 0;
    }

    if (windowData->swapchainData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Cannot get swapchain format, swapchain is currently invalid!");
        return 0;
    }

    return SwapchainCompositionToSDLFormat(
        windowData->swapchainComposition,
        windowData->swapchainData->usingFallbackFormat);
}

static bool VULKAN_SetSwapchainParameters(
    SDL_GPURenderer *driverData,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition,
    SDL_GPUPresentMode presentMode)
{
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Cannot set swapchain parameters on unclaimed window!");
        return false;
    }

    if (!VULKAN_SupportsSwapchainComposition(driverData, window, swapchainComposition)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Swapchain composition not supported!");
        return false;
    }

    if (!VULKAN_SupportsPresentMode(driverData, window, presentMode)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Present mode not supported!");
        return false;
    }

    windowData->presentMode = presentMode;
    windowData->swapchainComposition = swapchainComposition;

    return VULKAN_INTERNAL_RecreateSwapchain(
        (VulkanRenderer *)driverData,
        windowData);
}

// Submission structure

static VulkanFenceHandle *VULKAN_INTERNAL_AcquireFenceFromPool(
    VulkanRenderer *renderer)
{
    VulkanFenceHandle *handle;
    VkFenceCreateInfo fenceCreateInfo;
    VkFence fence;
    VkResult vulkanResult;

    if (renderer->fencePool.availableFenceCount == 0) {
        // Create fence
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = NULL;
        fenceCreateInfo.flags = 0;

        vulkanResult = renderer->vkCreateFence(
            renderer->logicalDevice,
            &fenceCreateInfo,
            NULL,
            &fence);

        if (vulkanResult != VK_SUCCESS) {
            LogVulkanResultAsError("vkCreateFence", vulkanResult);
            return NULL;
        }

        handle = SDL_malloc(sizeof(VulkanFenceHandle));
        handle->fence = fence;
        SDL_AtomicSet(&handle->referenceCount, 0);
        return handle;
    }

    SDL_LockMutex(renderer->fencePool.lock);

    handle = renderer->fencePool.availableFences[renderer->fencePool.availableFenceCount - 1];
    renderer->fencePool.availableFenceCount -= 1;

    vulkanResult = renderer->vkResetFences(
        renderer->logicalDevice,
        1,
        &handle->fence);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkResetFences", vulkanResult);
    }

    SDL_UnlockMutex(renderer->fencePool.lock);

    return handle;
}

static void VULKAN_INTERNAL_PerformPendingDestroys(
    VulkanRenderer *renderer)
{
    SDL_LockMutex(renderer->disposeLock);

    for (Sint32 i = renderer->texturesToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->texturesToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyTexture(
                renderer,
                renderer->texturesToDestroy[i]);

            renderer->texturesToDestroy[i] = renderer->texturesToDestroy[renderer->texturesToDestroyCount - 1];
            renderer->texturesToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->buffersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->buffersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyBuffer(
                renderer,
                renderer->buffersToDestroy[i]);

            renderer->buffersToDestroy[i] = renderer->buffersToDestroy[renderer->buffersToDestroyCount - 1];
            renderer->buffersToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->graphicsPipelinesToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->graphicsPipelinesToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyGraphicsPipeline(
                renderer,
                renderer->graphicsPipelinesToDestroy[i]);

            renderer->graphicsPipelinesToDestroy[i] = renderer->graphicsPipelinesToDestroy[renderer->graphicsPipelinesToDestroyCount - 1];
            renderer->graphicsPipelinesToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->computePipelinesToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->computePipelinesToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyComputePipeline(
                renderer,
                renderer->computePipelinesToDestroy[i]);

            renderer->computePipelinesToDestroy[i] = renderer->computePipelinesToDestroy[renderer->computePipelinesToDestroyCount - 1];
            renderer->computePipelinesToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->shadersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->shadersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyShader(
                renderer,
                renderer->shadersToDestroy[i]);

            renderer->shadersToDestroy[i] = renderer->shadersToDestroy[renderer->shadersToDestroyCount - 1];
            renderer->shadersToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->samplersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->samplersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroySampler(
                renderer,
                renderer->samplersToDestroy[i]);

            renderer->samplersToDestroy[i] = renderer->samplersToDestroy[renderer->samplersToDestroyCount - 1];
            renderer->samplersToDestroyCount -= 1;
        }
    }

    for (Sint32 i = renderer->framebuffersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->framebuffersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyFramebuffer(
                renderer,
                renderer->framebuffersToDestroy[i]);

            renderer->framebuffersToDestroy[i] = renderer->framebuffersToDestroy[renderer->framebuffersToDestroyCount - 1];
            renderer->framebuffersToDestroyCount -= 1;
        }
    }

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_INTERNAL_CleanCommandBuffer(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer)
{
    Uint32 i;
    DescriptorSetData *descriptorSetData;

    if (commandBuffer->autoReleaseFence) {
        VULKAN_ReleaseFence(
            (SDL_GPURenderer *)renderer,
            (SDL_GPUFence *)commandBuffer->inFlightFence);

        commandBuffer->inFlightFence = NULL;
    }

    // Bound descriptor sets are now available

    for (i = 0; i < commandBuffer->boundDescriptorSetDataCount; i += 1) {
        descriptorSetData = &commandBuffer->boundDescriptorSetDatas[i];

        SDL_LockMutex(descriptorSetData->descriptorSetPool->lock);

        if (descriptorSetData->descriptorSetPool->inactiveDescriptorSetCount == descriptorSetData->descriptorSetPool->inactiveDescriptorSetCapacity) {
            descriptorSetData->descriptorSetPool->inactiveDescriptorSetCapacity *= 2;
            descriptorSetData->descriptorSetPool->inactiveDescriptorSets = SDL_realloc(
                descriptorSetData->descriptorSetPool->inactiveDescriptorSets,
                descriptorSetData->descriptorSetPool->inactiveDescriptorSetCapacity * sizeof(VkDescriptorSet));
        }

        descriptorSetData->descriptorSetPool->inactiveDescriptorSets[descriptorSetData->descriptorSetPool->inactiveDescriptorSetCount] = descriptorSetData->descriptorSet;
        descriptorSetData->descriptorSetPool->inactiveDescriptorSetCount += 1;

        SDL_UnlockMutex(descriptorSetData->descriptorSetPool->lock);
    }

    commandBuffer->boundDescriptorSetDataCount = 0;

    // Uniform buffers are now available

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        VULKAN_INTERNAL_ReturnUniformBufferToPool(
            renderer,
            commandBuffer->usedUniformBuffers[i]);
    }
    commandBuffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    // Decrement reference counts

    for (i = 0; i < commandBuffer->usedBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedBuffers[i]->referenceCount);
    }
    commandBuffer->usedBufferCount = 0;

    for (i = 0; i < commandBuffer->usedTextureCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedTextures[i]->referenceCount);
    }
    commandBuffer->usedTextureCount = 0;

    for (i = 0; i < commandBuffer->usedSamplerCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedSamplers[i]->referenceCount);
    }
    commandBuffer->usedSamplerCount = 0;

    for (i = 0; i < commandBuffer->usedGraphicsPipelineCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedGraphicsPipelines[i]->referenceCount);
    }
    commandBuffer->usedGraphicsPipelineCount = 0;

    for (i = 0; i < commandBuffer->usedComputePipelineCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedComputePipelines[i]->referenceCount);
    }
    commandBuffer->usedComputePipelineCount = 0;

    for (i = 0; i < commandBuffer->usedFramebufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedFramebuffers[i]->referenceCount);
    }
    commandBuffer->usedFramebufferCount = 0;

    // Reset presentation data

    commandBuffer->presentDataCount = 0;
    commandBuffer->waitSemaphoreCount = 0;
    commandBuffer->signalSemaphoreCount = 0;

    // Reset defrag state

    if (commandBuffer->isDefrag) {
        renderer->defragInProgress = 0;
    }

    // Return command buffer to pool

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    if (commandBuffer->commandPool->inactiveCommandBufferCount == commandBuffer->commandPool->inactiveCommandBufferCapacity) {
        commandBuffer->commandPool->inactiveCommandBufferCapacity += 1;
        commandBuffer->commandPool->inactiveCommandBuffers = SDL_realloc(
            commandBuffer->commandPool->inactiveCommandBuffers,
            commandBuffer->commandPool->inactiveCommandBufferCapacity * sizeof(VulkanCommandBuffer *));
    }

    commandBuffer->commandPool->inactiveCommandBuffers[commandBuffer->commandPool->inactiveCommandBufferCount] = commandBuffer;
    commandBuffer->commandPool->inactiveCommandBufferCount += 1;

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    // Remove this command buffer from the submitted list
    for (i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        if (renderer->submittedCommandBuffers[i] == commandBuffer) {
            renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
            renderer->submittedCommandBufferCount -= 1;
        }
    }
}

static void VULKAN_WaitForFences(
    SDL_GPURenderer *driverData,
    bool waitAll,
    SDL_GPUFence **pFences,
    Uint32 fenceCount)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkFence *fences = SDL_stack_alloc(VkFence, fenceCount);
    VkResult result;

    for (Uint32 i = 0; i < fenceCount; i += 1) {
        fences[i] = ((VulkanFenceHandle *)pFences[i])->fence;
    }

    result = renderer->vkWaitForFences(
        renderer->logicalDevice,
        fenceCount,
        fences,
        waitAll,
        UINT64_MAX);

    if (result != VK_SUCCESS) {
        LogVulkanResultAsError("vkWaitForFences", result);
    }

    SDL_stack_free(fences);

    SDL_LockMutex(renderer->submitLock);

    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        result = renderer->vkGetFenceStatus(
            renderer->logicalDevice,
            renderer->submittedCommandBuffers[i]->inFlightFence->fence);

        if (result == VK_SUCCESS) {
            VULKAN_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i]);
        }
    }

    VULKAN_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->submitLock);
}

static void VULKAN_Wait(
    SDL_GPURenderer *driverData)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanCommandBuffer *commandBuffer;
    VkResult result;
    Sint32 i;

    result = renderer->vkDeviceWaitIdle(renderer->logicalDevice);

    if (result != VK_SUCCESS) {
        LogVulkanResultAsError("vkDeviceWaitIdle", result);
        return;
    }

    SDL_LockMutex(renderer->submitLock);

    for (i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        commandBuffer = renderer->submittedCommandBuffers[i];
        VULKAN_INTERNAL_CleanCommandBuffer(renderer, commandBuffer);
    }

    VULKAN_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->submitLock);
}

static SDL_GPUFence *VULKAN_SubmitAndAcquireFence(
    SDL_GPUCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer;

    vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    vulkanCommandBuffer->autoReleaseFence = 0;

    VULKAN_Submit(commandBuffer);

    return (SDL_GPUFence *)vulkanCommandBuffer->inFlightFence;
}

static void VULKAN_Submit(
    SDL_GPUCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkSubmitInfo submitInfo;
    VkPresentInfoKHR presentInfo;
    VulkanPresentData *presentData;
    VkResult vulkanResult, presentResult = VK_SUCCESS;
    VkPipelineStageFlags waitStages[MAX_PRESENT_COUNT];
    Uint32 swapchainImageIndex;
    VulkanTextureSubresource *swapchainTextureSubresource;
    Uint8 commandBufferCleaned = 0;
    VulkanMemorySubAllocator *allocator;
    bool presenting = false;

    SDL_LockMutex(renderer->submitLock);

    // FIXME: Can this just be permanent?
    for (Uint32 i = 0; i < MAX_PRESENT_COUNT; i += 1) {
        waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    for (Uint32 j = 0; j < vulkanCommandBuffer->presentDataCount; j += 1) {
        swapchainImageIndex = vulkanCommandBuffer->presentDatas[j].swapchainImageIndex;
        swapchainTextureSubresource = VULKAN_INTERNAL_FetchTextureSubresource(
            &vulkanCommandBuffer->presentDatas[j].windowData->swapchainData->textureContainers[swapchainImageIndex],
            0,
            0);

        VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_PRESENT,
            swapchainTextureSubresource);
    }

    VULKAN_INTERNAL_EndCommandBuffer(renderer, vulkanCommandBuffer);

    vulkanCommandBuffer->inFlightFence = VULKAN_INTERNAL_AcquireFenceFromPool(renderer);

    // Command buffer has a reference to the in-flight fence
    (void)SDL_AtomicIncRef(&vulkanCommandBuffer->inFlightFence->referenceCount);

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vulkanCommandBuffer->commandBuffer;

    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.pWaitSemaphores = vulkanCommandBuffer->waitSemaphores;
    submitInfo.waitSemaphoreCount = vulkanCommandBuffer->waitSemaphoreCount;
    submitInfo.pSignalSemaphores = vulkanCommandBuffer->signalSemaphores;
    submitInfo.signalSemaphoreCount = vulkanCommandBuffer->signalSemaphoreCount;

    vulkanResult = renderer->vkQueueSubmit(
        renderer->unifiedQueue,
        1,
        &submitInfo,
        vulkanCommandBuffer->inFlightFence->fence);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkQueueSubmit", vulkanResult);
    }

    // Mark command buffers as submitted

    if (renderer->submittedCommandBufferCount + 1 >= renderer->submittedCommandBufferCapacity) {
        renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

        renderer->submittedCommandBuffers = SDL_realloc(
            renderer->submittedCommandBuffers,
            sizeof(VulkanCommandBuffer *) * renderer->submittedCommandBufferCapacity);
    }

    renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = vulkanCommandBuffer;
    renderer->submittedCommandBufferCount += 1;

    // Present, if applicable

    for (Uint32 j = 0; j < vulkanCommandBuffer->presentDataCount; j += 1) {
        presenting = true;

        presentData = &vulkanCommandBuffer->presentDatas[j];

        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.pWaitSemaphores =
            &presentData->windowData->swapchainData->renderFinishedSemaphore[presentData->windowData->swapchainData->frameCounter];
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pSwapchains = &presentData->windowData->swapchainData->swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &presentData->swapchainImageIndex;
        presentInfo.pResults = NULL;

        presentResult = renderer->vkQueuePresentKHR(
            renderer->unifiedQueue,
            &presentInfo);

        presentData->windowData->swapchainData->frameCounter =
            (presentData->windowData->swapchainData->frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;

        if (presentResult != VK_SUCCESS) {
            VULKAN_INTERNAL_RecreateSwapchain(
                renderer,
                presentData->windowData);
        } else {
            // If presenting, the swapchain is using the in-flight fence
            presentData->windowData->swapchainData->inFlightFences[presentData->windowData->swapchainData->frameCounter] = vulkanCommandBuffer->inFlightFence;

            (void)SDL_AtomicIncRef(&vulkanCommandBuffer->inFlightFence->referenceCount);
        }
    }

    // Check if we can perform any cleanups

    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        vulkanResult = renderer->vkGetFenceStatus(
            renderer->logicalDevice,
            renderer->submittedCommandBuffers[i]->inFlightFence->fence);

        if (vulkanResult == VK_SUCCESS) {
            VULKAN_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i]);

            commandBufferCleaned = 1;
        }
    }

    if (commandBufferCleaned) {
        SDL_LockMutex(renderer->allocatorLock);

        for (Uint32 i = 0; i < VK_MAX_MEMORY_TYPES; i += 1) {
            allocator = &renderer->memoryAllocator->subAllocators[i];

            for (Sint32 j = allocator->allocationCount - 1; j >= 0; j -= 1) {
                if (allocator->allocations[j]->usedRegionCount == 0) {
                    VULKAN_INTERNAL_DeallocateMemory(
                        renderer,
                        allocator,
                        j);
                }
            }
        }

        SDL_UnlockMutex(renderer->allocatorLock);
    }

    // Check pending destroys
    VULKAN_INTERNAL_PerformPendingDestroys(renderer);

    // Defrag!
    if (
        presenting &&
        renderer->allocationsToDefragCount > 0 &&
        !renderer->defragInProgress) {
        VULKAN_INTERNAL_DefragmentMemory(renderer);
    }

    SDL_UnlockMutex(renderer->submitLock);
}

static Uint8 VULKAN_INTERNAL_DefragmentMemory(
    VulkanRenderer *renderer)
{
    VulkanMemoryAllocation *allocation;
    VulkanMemoryUsedRegion *currentRegion;
    VulkanBuffer *newBuffer;
    VulkanTexture *newTexture;
    VkBufferCopy bufferCopy;
    VkImageCopy imageCopy;
    VulkanCommandBuffer *commandBuffer;
    VulkanTextureSubresource *srcSubresource;
    VulkanTextureSubresource *dstSubresource;
    Uint32 i, subresourceIndex;

    SDL_LockMutex(renderer->allocatorLock);

    renderer->defragInProgress = 1;

    commandBuffer = (VulkanCommandBuffer *)VULKAN_AcquireCommandBuffer((SDL_GPURenderer *)renderer);
    if (commandBuffer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create defrag command buffer!");
        return 0;
    }
    commandBuffer->isDefrag = 1;

    allocation = renderer->allocationsToDefrag[renderer->allocationsToDefragCount - 1];
    renderer->allocationsToDefragCount -= 1;

    /* For each used region in the allocation
     * create a new resource, copy the data
     * and re-point the resource containers
     */
    for (i = 0; i < allocation->usedRegionCount; i += 1) {
        currentRegion = allocation->usedRegions[i];

        if (currentRegion->isBuffer && !currentRegion->vulkanBuffer->markedForDestroy) {
            currentRegion->vulkanBuffer->usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            newBuffer = VULKAN_INTERNAL_CreateBuffer(
                renderer,
                currentRegion->vulkanBuffer->size,
                currentRegion->vulkanBuffer->usageFlags,
                currentRegion->vulkanBuffer->type);

            if (newBuffer == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create defrag buffer!");
                return 0;
            }

            if (
                renderer->debugMode &&
                renderer->supportsDebugUtils &&
                currentRegion->vulkanBuffer->handle != NULL &&
                currentRegion->vulkanBuffer->handle->container != NULL &&
                currentRegion->vulkanBuffer->handle->container->debugName != NULL) {
                VULKAN_INTERNAL_SetBufferName(
                    renderer,
                    newBuffer,
                    currentRegion->vulkanBuffer->handle->container->debugName);
            }

            // Copy buffer contents if necessary
            if (
                currentRegion->vulkanBuffer->type == VULKAN_BUFFER_TYPE_GPU && currentRegion->vulkanBuffer->transitioned) {
                VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
                    renderer,
                    commandBuffer,
                    VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE,
                    currentRegion->vulkanBuffer);

                VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(
                    renderer,
                    commandBuffer,
                    VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION,
                    newBuffer);

                bufferCopy.srcOffset = 0;
                bufferCopy.dstOffset = 0;
                bufferCopy.size = currentRegion->resourceSize;

                renderer->vkCmdCopyBuffer(
                    commandBuffer->commandBuffer,
                    currentRegion->vulkanBuffer->buffer,
                    newBuffer->buffer,
                    1,
                    &bufferCopy);

                VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
                    renderer,
                    commandBuffer,
                    VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION,
                    newBuffer);

                VULKAN_INTERNAL_TrackBuffer(commandBuffer, currentRegion->vulkanBuffer);
                VULKAN_INTERNAL_TrackBuffer(commandBuffer, newBuffer);
            }

            // re-point original container to new buffer
            if (currentRegion->vulkanBuffer->handle != NULL) {
                newBuffer->handle = currentRegion->vulkanBuffer->handle;
                newBuffer->handle->vulkanBuffer = newBuffer;
                currentRegion->vulkanBuffer->handle = NULL;
            }

            VULKAN_INTERNAL_ReleaseBuffer(renderer, currentRegion->vulkanBuffer);
        } else if (!currentRegion->isBuffer && !currentRegion->vulkanTexture->markedForDestroy) {
            newTexture = VULKAN_INTERNAL_CreateTexture(
                renderer,
                currentRegion->vulkanTexture->dimensions.width,
                currentRegion->vulkanTexture->dimensions.height,
                currentRegion->vulkanTexture->depth,
                currentRegion->vulkanTexture->type,
                currentRegion->vulkanTexture->layerCount,
                currentRegion->vulkanTexture->levelCount,
                currentRegion->vulkanTexture->sampleCount,
                currentRegion->vulkanTexture->format,
                currentRegion->vulkanTexture->swizzle,
                currentRegion->vulkanTexture->aspectFlags,
                currentRegion->vulkanTexture->usageFlags,
                currentRegion->vulkanTexture->isMSAAColorTarget);

            if (newTexture == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create defrag texture!");
                return 0;
            }

            for (subresourceIndex = 0; subresourceIndex < currentRegion->vulkanTexture->subresourceCount; subresourceIndex += 1) {
                // copy subresource if necessary
                srcSubresource = &currentRegion->vulkanTexture->subresources[subresourceIndex];
                dstSubresource = &newTexture->subresources[subresourceIndex];

                // Set debug name if it exists
                if (
                    renderer->debugMode &&
                    renderer->supportsDebugUtils &&
                    srcSubresource->parent->handle != NULL &&
                    srcSubresource->parent->handle->container != NULL &&
                    srcSubresource->parent->handle->container->debugName != NULL) {
                    VULKAN_INTERNAL_SetTextureName(
                        renderer,
                        currentRegion->vulkanTexture,
                        srcSubresource->parent->handle->container->debugName);
                }

                if (srcSubresource->transitioned) {
                    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                        srcSubresource);

                    VULKAN_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                        dstSubresource);

                    imageCopy.srcOffset.x = 0;
                    imageCopy.srcOffset.y = 0;
                    imageCopy.srcOffset.z = 0;
                    imageCopy.srcSubresource.aspectMask = srcSubresource->parent->aspectFlags;
                    imageCopy.srcSubresource.baseArrayLayer = srcSubresource->layer;
                    imageCopy.srcSubresource.layerCount = 1;
                    imageCopy.srcSubresource.mipLevel = srcSubresource->level;
                    imageCopy.extent.width = SDL_max(1, srcSubresource->parent->dimensions.width >> srcSubresource->level);
                    imageCopy.extent.height = SDL_max(1, srcSubresource->parent->dimensions.height >> srcSubresource->level);
                    imageCopy.extent.depth = srcSubresource->parent->depth;
                    imageCopy.dstOffset.x = 0;
                    imageCopy.dstOffset.y = 0;
                    imageCopy.dstOffset.z = 0;
                    imageCopy.dstSubresource.aspectMask = dstSubresource->parent->aspectFlags;
                    imageCopy.dstSubresource.baseArrayLayer = dstSubresource->layer;
                    imageCopy.dstSubresource.layerCount = 1;
                    imageCopy.dstSubresource.mipLevel = dstSubresource->level;

                    renderer->vkCmdCopyImage(
                        commandBuffer->commandBuffer,
                        currentRegion->vulkanTexture->image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        newTexture->image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &imageCopy);

                    VULKAN_INTERNAL_TextureSubresourceTransitionToDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                        dstSubresource);

                    VULKAN_INTERNAL_TrackTexture(commandBuffer, srcSubresource->parent);
                    VULKAN_INTERNAL_TrackTexture(commandBuffer, dstSubresource->parent);
                }
            }

            // re-point original container to new texture
            newTexture->handle = currentRegion->vulkanTexture->handle;
            newTexture->handle->vulkanTexture = newTexture;
            currentRegion->vulkanTexture->handle = NULL;

            VULKAN_INTERNAL_ReleaseTexture(renderer, currentRegion->vulkanTexture);
        }
    }

    SDL_UnlockMutex(renderer->allocatorLock);

    VULKAN_Submit(
        (SDL_GPUCommandBuffer *)commandBuffer);

    return 1;
}

// Format Info

static bool VULKAN_SupportsTextureFormat(
    SDL_GPURenderer *driverData,
    SDL_GPUTextureFormat format,
    SDL_GPUTextureType type,
    SDL_GPUTextureUsageFlags usage)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkFormat vulkanFormat = SDLToVK_SurfaceFormat[format];
    VkImageUsageFlags vulkanUsage = 0;
    VkImageCreateFlags createFlags = 0;
    VkImageFormatProperties properties;
    VkResult vulkanResult;

    if (usage & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) {
        vulkanUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) {
        vulkanUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
        vulkanUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (usage & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT |
                 SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT |
                 SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
        vulkanUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (type == SDL_GPU_TEXTURETYPE_CUBE) {
        createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    vulkanResult = renderer->vkGetPhysicalDeviceImageFormatProperties(
        renderer->physicalDevice,
        vulkanFormat,
        (type == SDL_GPU_TEXTURETYPE_3D) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        vulkanUsage,
        createFlags,
        &properties);

    return vulkanResult == VK_SUCCESS;
}

// Device instantiation

static inline Uint8 CheckDeviceExtensions(
    VkExtensionProperties *extensions,
    Uint32 numExtensions,
    VulkanExtensions *supports)
{
    Uint32 i;

    SDL_memset(supports, '\0', sizeof(VulkanExtensions));
    for (i = 0; i < numExtensions; i += 1) {
        const char *name = extensions[i].extensionName;
#define CHECK(ext)                           \
    if (SDL_strcmp(name, "VK_" #ext) == 0) { \
        supports->ext = 1;                   \
    }
        CHECK(KHR_swapchain)
        else CHECK(KHR_maintenance1) else CHECK(KHR_driver_properties) else CHECK(EXT_vertex_attribute_divisor) else CHECK(KHR_portability_subset)
#undef CHECK
    }

    return (supports->KHR_swapchain &&
            supports->KHR_maintenance1);
}

static inline Uint32 GetDeviceExtensionCount(VulkanExtensions *supports)
{
    return (
        supports->KHR_swapchain +
        supports->KHR_maintenance1 +
        supports->KHR_driver_properties +
        supports->EXT_vertex_attribute_divisor +
        supports->KHR_portability_subset);
}

static inline void CreateDeviceExtensionArray(
    VulkanExtensions *supports,
    const char **extensions)
{
    Uint8 cur = 0;
#define CHECK(ext)                      \
    if (supports->ext) {                \
        extensions[cur++] = "VK_" #ext; \
    }
    CHECK(KHR_swapchain)
    CHECK(KHR_maintenance1)
    CHECK(KHR_driver_properties)
    CHECK(EXT_vertex_attribute_divisor)
    CHECK(KHR_portability_subset)
#undef CHECK
}

static inline Uint8 SupportsInstanceExtension(
    const char *ext,
    VkExtensionProperties *availableExtensions,
    Uint32 numAvailableExtensions)
{
    Uint32 i;
    for (i = 0; i < numAvailableExtensions; i += 1) {
        if (SDL_strcmp(ext, availableExtensions[i].extensionName) == 0) {
            return 1;
        }
    }
    return 0;
}

static Uint8 VULKAN_INTERNAL_CheckInstanceExtensions(
    const char **requiredExtensions,
    Uint32 requiredExtensionsLength,
    bool *supportsDebugUtils,
    bool *supportsColorspace)
{
    Uint32 extensionCount, i;
    VkExtensionProperties *availableExtensions;
    Uint8 allExtensionsSupported = 1;

    vkEnumerateInstanceExtensionProperties(
        NULL,
        &extensionCount,
        NULL);
    availableExtensions = SDL_malloc(
        extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(
        NULL,
        &extensionCount,
        availableExtensions);

    for (i = 0; i < requiredExtensionsLength; i += 1) {
        if (!SupportsInstanceExtension(
                requiredExtensions[i],
                availableExtensions,
                extensionCount)) {
            allExtensionsSupported = 0;
            break;
        }
    }

    // This is optional, but nice to have!
    *supportsDebugUtils = SupportsInstanceExtension(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        availableExtensions,
        extensionCount);

    // Also optional and nice to have!
    *supportsColorspace = SupportsInstanceExtension(
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
        availableExtensions,
        extensionCount);

    SDL_free(availableExtensions);
    return allExtensionsSupported;
}

static Uint8 VULKAN_INTERNAL_CheckDeviceExtensions(
    VulkanRenderer *renderer,
    VkPhysicalDevice physicalDevice,
    VulkanExtensions *physicalDeviceExtensions)
{
    Uint32 extensionCount;
    VkExtensionProperties *availableExtensions;
    Uint8 allExtensionsSupported;

    renderer->vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        NULL,
        &extensionCount,
        NULL);
    availableExtensions = (VkExtensionProperties *)SDL_malloc(
        extensionCount * sizeof(VkExtensionProperties));
    renderer->vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        NULL,
        &extensionCount,
        availableExtensions);

    allExtensionsSupported = CheckDeviceExtensions(
        availableExtensions,
        extensionCount,
        physicalDeviceExtensions);

    SDL_free(availableExtensions);
    return allExtensionsSupported;
}

static Uint8 VULKAN_INTERNAL_CheckValidationLayers(
    const char **validationLayers,
    Uint32 validationLayersLength)
{
    Uint32 layerCount;
    VkLayerProperties *availableLayers;
    Uint32 i, j;
    Uint8 layerFound = 0;

    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    availableLayers = (VkLayerProperties *)SDL_malloc(
        layerCount * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    for (i = 0; i < validationLayersLength; i += 1) {
        layerFound = 0;

        for (j = 0; j < layerCount; j += 1) {
            if (SDL_strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
                layerFound = 1;
                break;
            }
        }

        if (!layerFound) {
            break;
        }
    }

    SDL_free(availableLayers);
    return layerFound;
}

static Uint8 VULKAN_INTERNAL_CreateInstance(VulkanRenderer *renderer)
{
    VkResult vulkanResult;
    VkApplicationInfo appInfo;
    VkInstanceCreateFlags createFlags;
    const char *const *originalInstanceExtensionNames;
    const char **instanceExtensionNames;
    Uint32 instanceExtensionCount;
    VkInstanceCreateInfo createInfo;
    static const char *layerNames[] = { "VK_LAYER_KHRONOS_validation" };

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = NULL;
    appInfo.applicationVersion = 0;
    appInfo.pEngineName = "SDLGPU";
    appInfo.engineVersion = SDL_VERSION;
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    createFlags = 0;

    originalInstanceExtensionNames = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);
    if (!originalInstanceExtensionNames) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "SDL_Vulkan_GetInstanceExtensions(): getExtensionCount: %s",
            SDL_GetError());

        return 0;
    }

    /* Extra space for the following extensions:
     * VK_KHR_get_physical_device_properties2
     * VK_EXT_swapchain_colorspace
     * VK_EXT_debug_utils
     * VK_KHR_portability_enumeration
     */
    instanceExtensionNames = SDL_stack_alloc(
        const char *,
        instanceExtensionCount + 4);
    SDL_memcpy(instanceExtensionNames, originalInstanceExtensionNames, instanceExtensionCount * sizeof(const char *));

    // Core since 1.1
    instanceExtensionNames[instanceExtensionCount++] =
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

#ifdef SDL_PLATFORM_APPLE
    instanceExtensionNames[instanceExtensionCount++] =
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    createFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (!VULKAN_INTERNAL_CheckInstanceExtensions(
            instanceExtensionNames,
            instanceExtensionCount,
            &renderer->supportsDebugUtils,
            &renderer->supportsColorspace)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "Required Vulkan instance extensions not supported");

        SDL_stack_free((char *)instanceExtensionNames);
        return 0;
    }

    if (renderer->supportsDebugUtils) {
        // Append the debug extension
        instanceExtensionNames[instanceExtensionCount++] =
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    } else {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "%s is not supported!",
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (renderer->supportsColorspace) {
        // Append colorspace extension
        instanceExtensionNames[instanceExtensionCount++] =
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
    }

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = createFlags;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledLayerNames = layerNames;
    createInfo.enabledExtensionCount = instanceExtensionCount;
    createInfo.ppEnabledExtensionNames = instanceExtensionNames;
    if (renderer->debugMode) {
        createInfo.enabledLayerCount = SDL_arraysize(layerNames);
        if (!VULKAN_INTERNAL_CheckValidationLayers(
                layerNames,
                createInfo.enabledLayerCount)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Validation layers not found, continuing without validation");
            createInfo.enabledLayerCount = 0;
        } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Validation layers enabled, expect debug level performance!");
        }
    } else {
        createInfo.enabledLayerCount = 0;
    }

    vulkanResult = vkCreateInstance(&createInfo, NULL, &renderer->instance);
    if (vulkanResult != VK_SUCCESS) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "vkCreateInstance failed: %s",
            VkErrorMessages(vulkanResult));

        SDL_stack_free((char *)instanceExtensionNames);
        return 0;
    }

    SDL_stack_free((char *)instanceExtensionNames);
    return 1;
}

static Uint8 VULKAN_INTERNAL_IsDeviceSuitable(
    VulkanRenderer *renderer,
    VkPhysicalDevice physicalDevice,
    VulkanExtensions *physicalDeviceExtensions,
    Uint32 *queueFamilyIndex,
    Uint8 *deviceRank)
{
    Uint32 queueFamilyCount, queueFamilyRank, queueFamilyBest;
    VkQueueFamilyProperties *queueProps;
    bool supportsPresent;
    VkPhysicalDeviceProperties deviceProperties;
    Uint32 i;

    const Uint8 *devicePriority = renderer->preferLowPower ? DEVICE_PRIORITY_LOWPOWER : DEVICE_PRIORITY_HIGHPERFORMANCE;

    /* Get the device rank before doing any checks, in case one fails.
     * Note: If no dedicated device exists, one that supports our features
     * would be fine
     */
    renderer->vkGetPhysicalDeviceProperties(
        physicalDevice,
        &deviceProperties);
    if (*deviceRank < devicePriority[deviceProperties.deviceType]) {
        /* This device outranks the best device we've found so far!
         * This includes a dedicated GPU that has less features than an
         * integrated GPU, because this is a freak case that is almost
         * never intentionally desired by the end user
         */
        *deviceRank = devicePriority[deviceProperties.deviceType];
    } else if (*deviceRank > devicePriority[deviceProperties.deviceType]) {
        /* Device is outranked by a previous device, don't even try to
         * run a query and reset the rank to avoid overwrites
         */
        *deviceRank = 0;
        return 0;
    }

    if (!VULKAN_INTERNAL_CheckDeviceExtensions(
            renderer,
            physicalDevice,
            physicalDeviceExtensions)) {
        return 0;
    }

    renderer->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        NULL);

    queueProps = (VkQueueFamilyProperties *)SDL_stack_alloc(
        VkQueueFamilyProperties,
        queueFamilyCount);
    renderer->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueProps);

    queueFamilyBest = 0;
    *queueFamilyIndex = UINT32_MAX;
    for (i = 0; i < queueFamilyCount; i += 1) {
        supportsPresent = SDL_Vulkan_GetPresentationSupport(
            renderer->instance,
            physicalDevice,
            i);
        if (!supportsPresent ||
            !(queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            // Not a graphics family, ignore.
            continue;
        }

        /* The queue family bitflags are kind of annoying.
         *
         * We of course need a graphics family, but we ideally want the
         * _primary_ graphics family. The spec states that at least one
         * graphics family must also be a compute family, so generally
         * drivers make that the first one. But hey, maybe something
         * genuinely can't do compute or something, and FNA doesn't
         * need it, so we'll be open to a non-compute queue family.
         *
         * Additionally, it's common to see the primary queue family
         * have the transfer bit set, which is great! But this is
         * actually optional; it's impossible to NOT have transfers in
         * graphics/compute but it _is_ possible for a graphics/compute
         * family, even the primary one, to just decide not to set the
         * bitflag. Admittedly, a driver may want to isolate transfer
         * queues to a dedicated family so that queues made solely for
         * transfers can have an optimized DMA queue.
         *
         * That, or the driver author got lazy and decided not to set
         * the bit. Looking at you, Android.
         *
         * -flibit
         */
        if (queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (queueProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                // Has all attribs!
                queueFamilyRank = 3;
            } else {
                // Probably has a DMA transfer queue family
                queueFamilyRank = 2;
            }
        } else {
            // Just a graphics family, probably has something better
            queueFamilyRank = 1;
        }
        if (queueFamilyRank > queueFamilyBest) {
            *queueFamilyIndex = i;
            queueFamilyBest = queueFamilyRank;
        }
    }

    SDL_stack_free(queueProps);

    if (*queueFamilyIndex == UINT32_MAX) {
        // Somehow no graphics queues existed. Compute-only device?
        return 0;
    }

    // FIXME: Need better structure for checking vs storing swapchain support details
    return 1;
}

static Uint8 VULKAN_INTERNAL_DeterminePhysicalDevice(VulkanRenderer *renderer)
{
    VkResult vulkanResult;
    VkPhysicalDevice *physicalDevices;
    VulkanExtensions *physicalDeviceExtensions;
    Uint32 physicalDeviceCount, i, suitableIndex;
    Uint32 queueFamilyIndex, suitableQueueFamilyIndex;
    Uint8 deviceRank, highestRank;

    vulkanResult = renderer->vkEnumeratePhysicalDevices(
        renderer->instance,
        &physicalDeviceCount,
        NULL);
    VULKAN_ERROR_CHECK(vulkanResult, vkEnumeratePhysicalDevices, 0)

    if (physicalDeviceCount == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Failed to find any GPUs with Vulkan support");
        return 0;
    }

    physicalDevices = SDL_stack_alloc(VkPhysicalDevice, physicalDeviceCount);
    physicalDeviceExtensions = SDL_stack_alloc(VulkanExtensions, physicalDeviceCount);

    vulkanResult = renderer->vkEnumeratePhysicalDevices(
        renderer->instance,
        &physicalDeviceCount,
        physicalDevices);

    /* This should be impossible to hit, but from what I can tell this can
     * be triggered not because the array is too small, but because there
     * were drivers that turned out to be bogus, so this is the loader's way
     * of telling us that the list is now smaller than expected :shrug:
     */
    if (vulkanResult == VK_INCOMPLETE) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "vkEnumeratePhysicalDevices returned VK_INCOMPLETE, will keep trying anyway...");
        vulkanResult = VK_SUCCESS;
    }

    if (vulkanResult != VK_SUCCESS) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "vkEnumeratePhysicalDevices failed: %s",
            VkErrorMessages(vulkanResult));
        SDL_stack_free(physicalDevices);
        SDL_stack_free(physicalDeviceExtensions);
        return 0;
    }

    // Any suitable device will do, but we'd like the best
    suitableIndex = -1;
    suitableQueueFamilyIndex = 0;
    highestRank = 0;
    for (i = 0; i < physicalDeviceCount; i += 1) {
        deviceRank = highestRank;
        if (VULKAN_INTERNAL_IsDeviceSuitable(
                renderer,
                physicalDevices[i],
                &physicalDeviceExtensions[i],
                &queueFamilyIndex,
                &deviceRank)) {
            /* Use this for rendering.
             * Note that this may override a previous device that
             * supports rendering, but shares the same device rank.
             */
            suitableIndex = i;
            suitableQueueFamilyIndex = queueFamilyIndex;
            highestRank = deviceRank;
        } else if (deviceRank > highestRank) {
            /* In this case, we found a... "realer?" GPU,
             * but it doesn't actually support our Vulkan.
             * We should disqualify all devices below as a
             * result, because if we don't we end up
             * ignoring real hardware and risk using
             * something like LLVMpipe instead!
             * -flibit
             */
            suitableIndex = -1;
            highestRank = deviceRank;
        }
    }

    if (suitableIndex != -1) {
        renderer->supports = physicalDeviceExtensions[suitableIndex];
        renderer->physicalDevice = physicalDevices[suitableIndex];
        renderer->queueFamilyIndex = suitableQueueFamilyIndex;
    } else {
        SDL_stack_free(physicalDevices);
        SDL_stack_free(physicalDeviceExtensions);
        return 0;
    }

    renderer->physicalDeviceProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    if (renderer->supports.KHR_driver_properties) {
        renderer->physicalDeviceDriverProperties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
        renderer->physicalDeviceDriverProperties.pNext = NULL;

        renderer->physicalDeviceProperties.pNext =
            &renderer->physicalDeviceDriverProperties;

        renderer->vkGetPhysicalDeviceProperties2KHR(
            renderer->physicalDevice,
            &renderer->physicalDeviceProperties);
    } else {
        renderer->physicalDeviceProperties.pNext = NULL;

        renderer->vkGetPhysicalDeviceProperties(
            renderer->physicalDevice,
            &renderer->physicalDeviceProperties.properties);
    }

    renderer->vkGetPhysicalDeviceMemoryProperties(
        renderer->physicalDevice,
        &renderer->memoryProperties);

    SDL_stack_free(physicalDevices);
    SDL_stack_free(physicalDeviceExtensions);
    return 1;
}

static Uint8 VULKAN_INTERNAL_CreateLogicalDevice(
    VulkanRenderer *renderer)
{
    VkResult vulkanResult;
    VkDeviceCreateInfo deviceCreateInfo;
    VkPhysicalDeviceFeatures desiredDeviceFeatures;
    VkPhysicalDeviceFeatures haveDeviceFeatures;
    VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures;
    const char **deviceExtensions;

    VkDeviceQueueCreateInfo queueCreateInfo;
    float queuePriority = 1.0f;

    queueCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = NULL;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = renderer->queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // check feature support

    renderer->vkGetPhysicalDeviceFeatures(
        renderer->physicalDevice,
        &haveDeviceFeatures);

    // specifying used device features

    SDL_zero(desiredDeviceFeatures);
    desiredDeviceFeatures.independentBlend = VK_TRUE;
    desiredDeviceFeatures.samplerAnisotropy = VK_TRUE;

    if (haveDeviceFeatures.fillModeNonSolid) {
        desiredDeviceFeatures.fillModeNonSolid = VK_TRUE;
        renderer->supportsFillModeNonSolid = true;
    }

    if (haveDeviceFeatures.multiDrawIndirect) {
        desiredDeviceFeatures.multiDrawIndirect = VK_TRUE;
        renderer->supportsMultiDrawIndirect = true;
    }

    // creating the logical device

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    if (renderer->supports.KHR_portability_subset) {
        portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
        portabilityFeatures.pNext = NULL;
        portabilityFeatures.constantAlphaColorBlendFactors = VK_FALSE;
        portabilityFeatures.events = VK_FALSE;
        portabilityFeatures.imageViewFormatReinterpretation = VK_FALSE;
        portabilityFeatures.imageViewFormatSwizzle = VK_TRUE;
        portabilityFeatures.imageView2DOn3DImage = VK_FALSE;
        portabilityFeatures.multisampleArrayImage = VK_FALSE;
        portabilityFeatures.mutableComparisonSamplers = VK_FALSE;
        portabilityFeatures.pointPolygons = VK_FALSE;
        portabilityFeatures.samplerMipLodBias = VK_FALSE; // Technically should be true, but eh
        portabilityFeatures.separateStencilMaskRef = VK_FALSE;
        portabilityFeatures.shaderSampleRateInterpolationFunctions = VK_FALSE;
        portabilityFeatures.tessellationIsolines = VK_FALSE;
        portabilityFeatures.tessellationPointMode = VK_FALSE;
        portabilityFeatures.triangleFans = VK_FALSE;
        portabilityFeatures.vertexAttributeAccessBeyondStride = VK_FALSE;
        deviceCreateInfo.pNext = &portabilityFeatures;
    } else {
        deviceCreateInfo.pNext = NULL;
    }
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = NULL;
    deviceCreateInfo.enabledExtensionCount = GetDeviceExtensionCount(
        &renderer->supports);
    deviceExtensions = SDL_stack_alloc(
        const char *,
        deviceCreateInfo.enabledExtensionCount);
    CreateDeviceExtensionArray(&renderer->supports, deviceExtensions);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceCreateInfo.pEnabledFeatures = &desiredDeviceFeatures;

    vulkanResult = renderer->vkCreateDevice(
        renderer->physicalDevice,
        &deviceCreateInfo,
        NULL,
        &renderer->logicalDevice);
    SDL_stack_free(deviceExtensions);
    VULKAN_ERROR_CHECK(vulkanResult, vkCreateDevice, 0)

    // Load vkDevice entry points

#define VULKAN_DEVICE_FUNCTION(func)                    \
    renderer->func = (PFN_##func)                       \
                         renderer->vkGetDeviceProcAddr( \
                             renderer->logicalDevice,   \
                             #func);
#include "SDL_gpu_vulkan_vkfuncs.h"

    renderer->vkGetDeviceQueue(
        renderer->logicalDevice,
        renderer->queueFamilyIndex,
        0,
        &renderer->unifiedQueue);

    return 1;
}

static void VULKAN_INTERNAL_LoadEntryPoints(void)
{
    // Required for MoltenVK support
    SDL_setenv("MVK_CONFIG_FULL_IMAGE_VIEW_SWIZZLE", "1", 1);

    // Load Vulkan entry points
    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vulkan: SDL_Vulkan_LoadLibrary failed!");
        return;
    }

#ifdef HAVE_GCC_DIAGNOSTIC_PRAGMA
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
#ifdef HAVE_GCC_DIAGNOSTIC_PRAGMA
#pragma GCC diagnostic pop
#endif
    if (vkGetInstanceProcAddr == NULL) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "SDL_Vulkan_GetVkGetInstanceProcAddr(): %s",
            SDL_GetError());
        return;
    }

#define VULKAN_GLOBAL_FUNCTION(name)                                                                      \
    name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);                                      \
    if (name == NULL) {                                                                                   \
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "vkGetInstanceProcAddr(VK_NULL_HANDLE, \"" #name "\") failed"); \
        return;                                                                                           \
    }
#include "SDL_gpu_vulkan_vkfuncs.h"
}

static bool VULKAN_INTERNAL_PrepareVulkan(
    VulkanRenderer *renderer)
{
    VULKAN_INTERNAL_LoadEntryPoints();

    if (!VULKAN_INTERNAL_CreateInstance(renderer)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vulkan: Could not create Vulkan instance");
        return false;
    }

#define VULKAN_INSTANCE_FUNCTION(func) \
    renderer->func = (PFN_##func)vkGetInstanceProcAddr(renderer->instance, #func);
#include "SDL_gpu_vulkan_vkfuncs.h"

    if (!VULKAN_INTERNAL_DeterminePhysicalDevice(renderer)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vulkan: Failed to determine a suitable physical device");
        return false;
    }
    return true;
}

static bool VULKAN_PrepareDriver(SDL_VideoDevice *_this)
{
    // Set up dummy VulkanRenderer
    VulkanRenderer *renderer;
    Uint8 result;

    if (_this->Vulkan_CreateSurface == NULL) {
        return false;
    }

    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        return false;
    }

    renderer = (VulkanRenderer *)SDL_malloc(sizeof(VulkanRenderer));
    SDL_memset(renderer, '\0', sizeof(VulkanRenderer));

    result = VULKAN_INTERNAL_PrepareVulkan(renderer);

    if (result) {
        renderer->vkDestroyInstance(renderer->instance, NULL);
    }
    SDL_free(renderer);
    SDL_Vulkan_UnloadLibrary();
    return result;
}

static SDL_GPUDevice *VULKAN_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)
{
    VulkanRenderer *renderer;

    SDL_GPUDevice *result;
    VkResult vulkanResult;
    Uint32 i;

    // Variables: Image Format Detection
    VkImageFormatProperties imageFormatProperties;

    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        SDL_assert(!"This should have failed in PrepareDevice first!");
        return NULL;
    }

    renderer = (VulkanRenderer *)SDL_malloc(sizeof(VulkanRenderer));
    SDL_memset(renderer, '\0', sizeof(VulkanRenderer));
    renderer->debugMode = debugMode;
    renderer->preferLowPower = preferLowPower;

    if (!VULKAN_INTERNAL_PrepareVulkan(renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to initialize Vulkan!");
        SDL_free(renderer);
        SDL_Vulkan_UnloadLibrary();
        return NULL;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "SDL_GPU Driver: Vulkan");
    SDL_LogInfo(
        SDL_LOG_CATEGORY_GPU,
        "Vulkan Device: %s",
        renderer->physicalDeviceProperties.properties.deviceName);
    if (renderer->supports.KHR_driver_properties) {
        SDL_LogInfo(
            SDL_LOG_CATEGORY_GPU,
            "Vulkan Driver: %s %s",
            renderer->physicalDeviceDriverProperties.driverName,
            renderer->physicalDeviceDriverProperties.driverInfo);
        SDL_LogInfo(
            SDL_LOG_CATEGORY_GPU,
            "Vulkan Conformance: %u.%u.%u",
            renderer->physicalDeviceDriverProperties.conformanceVersion.major,
            renderer->physicalDeviceDriverProperties.conformanceVersion.minor,
            renderer->physicalDeviceDriverProperties.conformanceVersion.patch);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "KHR_driver_properties unsupported! Bother your vendor about this!");
    }

    if (!VULKAN_INTERNAL_CreateLogicalDevice(
            renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create logical device");
        SDL_free(renderer);
        SDL_Vulkan_UnloadLibrary();
        return NULL;
    }

    // FIXME: just move this into this function
    result = (SDL_GPUDevice *)SDL_malloc(sizeof(SDL_GPUDevice));
    ASSIGN_DRIVER(VULKAN)

    result->driverData = (SDL_GPURenderer *)renderer;

    /*
     * Create initial swapchain array
     */

    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindowCount = 0;
    renderer->claimedWindows = SDL_malloc(
        renderer->claimedWindowCapacity * sizeof(WindowData *));

    // Threading

    renderer->allocatorLock = SDL_CreateMutex();
    renderer->disposeLock = SDL_CreateMutex();
    renderer->submitLock = SDL_CreateMutex();
    renderer->acquireCommandBufferLock = SDL_CreateMutex();
    renderer->acquireUniformBufferLock = SDL_CreateMutex();
    renderer->renderPassFetchLock = SDL_CreateMutex();
    renderer->framebufferFetchLock = SDL_CreateMutex();

    /*
     * Create submitted command buffer list
     */

    renderer->submittedCommandBufferCapacity = 16;
    renderer->submittedCommandBufferCount = 0;
    renderer->submittedCommandBuffers = SDL_malloc(sizeof(VulkanCommandBuffer *) * renderer->submittedCommandBufferCapacity);

    // Memory Allocator

    renderer->memoryAllocator = (VulkanMemoryAllocator *)SDL_malloc(
        sizeof(VulkanMemoryAllocator));

    for (i = 0; i < VK_MAX_MEMORY_TYPES; i += 1) {
        renderer->memoryAllocator->subAllocators[i].memoryTypeIndex = i;
        renderer->memoryAllocator->subAllocators[i].allocations = NULL;
        renderer->memoryAllocator->subAllocators[i].allocationCount = 0;
        renderer->memoryAllocator->subAllocators[i].sortedFreeRegions = SDL_malloc(
            sizeof(VulkanMemoryFreeRegion *) * 4);
        renderer->memoryAllocator->subAllocators[i].sortedFreeRegionCount = 0;
        renderer->memoryAllocator->subAllocators[i].sortedFreeRegionCapacity = 4;
    }

    // Create uniform buffer pool

    renderer->uniformBufferPoolCount = 32;
    renderer->uniformBufferPoolCapacity = 32;
    renderer->uniformBufferPool = SDL_malloc(
        renderer->uniformBufferPoolCapacity * sizeof(VulkanUniformBuffer *));

    for (i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i] = VULKAN_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    // Device limits

    renderer->minUBOAlignment = (Uint32)renderer->physicalDeviceProperties.properties.limits.minUniformBufferOffsetAlignment;

    // Initialize caches

    renderer->commandPoolHashTable = SDL_CreateHashTable(
        (void *)renderer,
        64,
        VULKAN_INTERNAL_CommandPoolHashFunction,
        VULKAN_INTERNAL_CommandPoolHashKeyMatch,
        VULKAN_INTERNAL_CommandPoolHashNuke,
        false);

    renderer->renderPassHashTable = SDL_CreateHashTable(
        (void *)renderer,
        64,
        VULKAN_INTERNAL_RenderPassHashFunction,
        VULKAN_INTERNAL_RenderPassHashKeyMatch,
        VULKAN_INTERNAL_RenderPassHashNuke,
        false);

    renderer->framebufferHashTable = SDL_CreateHashTable(
        (void *)renderer,
        64,
        VULKAN_INTERNAL_FramebufferHashFunction,
        VULKAN_INTERNAL_FramebufferHashKeyMatch,
        VULKAN_INTERNAL_FramebufferHashNuke,
        false);

    // Initialize fence pool

    renderer->fencePool.lock = SDL_CreateMutex();

    renderer->fencePool.availableFenceCapacity = 4;
    renderer->fencePool.availableFenceCount = 0;
    renderer->fencePool.availableFences = SDL_malloc(
        renderer->fencePool.availableFenceCapacity * sizeof(VulkanFenceHandle *));

    // Some drivers don't support D16, so we have to fall back to D32.

    vulkanResult = renderer->vkGetPhysicalDeviceImageFormatProperties(
        renderer->physicalDevice,
        VK_FORMAT_D16_UNORM,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        0,
        &imageFormatProperties);

    if (vulkanResult == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        renderer->D16Format = VK_FORMAT_D32_SFLOAT;
    } else {
        renderer->D16Format = VK_FORMAT_D16_UNORM;
    }

    vulkanResult = renderer->vkGetPhysicalDeviceImageFormatProperties(
        renderer->physicalDevice,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        0,
        &imageFormatProperties);

    if (vulkanResult == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        renderer->D16S8Format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    } else {
        renderer->D16S8Format = VK_FORMAT_D16_UNORM_S8_UINT;
    }

    // Deferred destroy storage

    renderer->texturesToDestroyCapacity = 16;
    renderer->texturesToDestroyCount = 0;

    renderer->texturesToDestroy = (VulkanTexture **)SDL_malloc(
        sizeof(VulkanTexture *) *
        renderer->texturesToDestroyCapacity);

    renderer->buffersToDestroyCapacity = 16;
    renderer->buffersToDestroyCount = 0;

    renderer->buffersToDestroy = SDL_malloc(
        sizeof(VulkanBuffer *) *
        renderer->buffersToDestroyCapacity);

    renderer->samplersToDestroyCapacity = 16;
    renderer->samplersToDestroyCount = 0;

    renderer->samplersToDestroy = SDL_malloc(
        sizeof(VulkanSampler *) *
        renderer->samplersToDestroyCapacity);

    renderer->graphicsPipelinesToDestroyCapacity = 16;
    renderer->graphicsPipelinesToDestroyCount = 0;

    renderer->graphicsPipelinesToDestroy = SDL_malloc(
        sizeof(VulkanGraphicsPipeline *) *
        renderer->graphicsPipelinesToDestroyCapacity);

    renderer->computePipelinesToDestroyCapacity = 16;
    renderer->computePipelinesToDestroyCount = 0;

    renderer->computePipelinesToDestroy = SDL_malloc(
        sizeof(VulkanComputePipeline *) *
        renderer->computePipelinesToDestroyCapacity);

    renderer->shadersToDestroyCapacity = 16;
    renderer->shadersToDestroyCount = 0;

    renderer->shadersToDestroy = SDL_malloc(
        sizeof(VulkanShader *) *
        renderer->shadersToDestroyCapacity);

    renderer->framebuffersToDestroyCapacity = 16;
    renderer->framebuffersToDestroyCount = 0;
    renderer->framebuffersToDestroy = SDL_malloc(
        sizeof(VulkanFramebuffer *) *
        renderer->framebuffersToDestroyCapacity);

    // Defrag state

    renderer->defragInProgress = 0;

    renderer->allocationsToDefragCount = 0;
    renderer->allocationsToDefragCapacity = 4;
    renderer->allocationsToDefrag = SDL_malloc(
        renderer->allocationsToDefragCapacity * sizeof(VulkanMemoryAllocation *));

    return result;
}

SDL_GPUBootstrap VulkanDriver = {
    "Vulkan",
    SDL_GPU_DRIVER_VULKAN,
    SDL_GPU_SHADERFORMAT_SPIRV,
    VULKAN_PrepareDriver,
    VULKAN_CreateDevice
};

#endif // SDL_GPU_VULKAN
