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

/* Needed for VK_KHR_portability_subset */
#define VK_ENABLE_BETA_EXTENSIONS

#define VK_NO_PROTOTYPES
#include "../../video/khronos/vulkan/vulkan.h"

#include "SDL_hashtable.h"
#include <SDL3/SDL_vulkan.h>

#include "../SDL_gpu_driver.h"

#define VULKAN_INTERNAL_clamp(val, min, max) SDL_max(min, SDL_min(val, max))

/* Global Vulkan Loader Entry Points */

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;

#define VULKAN_GLOBAL_FUNCTION(name) \
    static PFN_##name name = NULL;
#include "SDL_gpu_vulkan_vkfuncs.h"

/* vkInstance/vkDevice function typedefs */

#define VULKAN_INSTANCE_FUNCTION(ext, ret, func, params) \
    typedef ret(VKAPI_CALL *vkfntype_##func) params;
#define VULKAN_DEVICE_FUNCTION(ext, ret, func, params) \
    typedef ret(VKAPI_CALL *vkfntype_##func) params;
#include "SDL_gpu_vulkan_vkfuncs.h"

typedef struct VulkanExtensions
{
    /* Globally supported */
    Uint8 KHR_swapchain;
    /* Core since 1.1 */
    Uint8 KHR_maintenance1;
    Uint8 KHR_get_memory_requirements2;

    /* Core since 1.2 */
    Uint8 KHR_driver_properties;
    /* EXT, probably not going to be Core */
    Uint8 EXT_vertex_attribute_divisor;
    /* Only required for special implementations (i.e. MoltenVK) */
    Uint8 KHR_portability_subset;
} VulkanExtensions;

/* Defines */

#define SMALL_ALLOCATION_THRESHOLD    2097152  /* 2   MiB */
#define SMALL_ALLOCATION_SIZE         16777216 /* 16  MiB */
#define LARGE_ALLOCATION_INCREMENT    67108864 /* 64  MiB */
#define MAX_UBO_SECTION_SIZE          4096     /* 4   KiB */
#define DESCRIPTOR_POOL_STARTING_SIZE 128
#define WINDOW_PROPERTY_DATA          "SDL_GpuVulkanWindowPropertyData"

#define IDENTITY_SWIZZLE               \
    {                                  \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY, \
        VK_COMPONENT_SWIZZLE_IDENTITY  \
    }

#define NULL_DESC_LAYOUT     (VkDescriptorSetLayout)0
#define NULL_PIPELINE_LAYOUT (VkPipelineLayout)0
#define NULL_RENDER_PASS     (SDL_GpuRenderPass *)0

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

/* Conversions */

static const Uint8 DEVICE_PRIORITY[] = {
    0, /* VK_PHYSICAL_DEVICE_TYPE_OTHER */
    3, /* VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU */
    4, /* VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU */
    2, /* VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU */
    1  /* VK_PHYSICAL_DEVICE_TYPE_CPU */
};

static VkPresentModeKHR SDLToVK_PresentMode[] = {
    VK_PRESENT_MODE_FIFO_KHR,
    VK_PRESENT_MODE_IMMEDIATE_KHR,
    VK_PRESENT_MODE_MAILBOX_KHR
};

static VkFormat SDLToVK_SurfaceFormat[] = {
    VK_FORMAT_R8G8B8A8_UNORM,           /* R8G8B8A8 */
    VK_FORMAT_B8G8R8A8_UNORM,           /* B8G8R8A8 */
    VK_FORMAT_R5G6B5_UNORM_PACK16,      /* B5G6R5 */
    VK_FORMAT_A1R5G5B5_UNORM_PACK16,    /* B5G5R5A1 */
    VK_FORMAT_B4G4R4A4_UNORM_PACK16,    /* B4G4R4A4 */
    VK_FORMAT_A2B10G10R10_UNORM_PACK32, /* R10G10B10A2 */
    VK_FORMAT_R16G16_UNORM,             /* R16G16 */
    VK_FORMAT_R16G16B16A16_UNORM,       /* R16G16B16A16 */
    VK_FORMAT_R8_UNORM,                 /* R8 */
    VK_FORMAT_R8_UNORM,                 /* A8 */
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK,     /* BC1 */
    VK_FORMAT_BC2_UNORM_BLOCK,          /* BC2 */
    VK_FORMAT_BC3_UNORM_BLOCK,          /* BC3 */
    VK_FORMAT_BC7_UNORM_BLOCK,          /* BC7 */
    VK_FORMAT_R8G8_SNORM,               /* R8G8_SNORM */
    VK_FORMAT_R8G8B8A8_SNORM,           /* R8G8B8A8_SNORM */
    VK_FORMAT_R16_SFLOAT,               /* R16_SFLOAT */
    VK_FORMAT_R16G16_SFLOAT,            /* R16G16_SFLOAT */
    VK_FORMAT_R16G16B16A16_SFLOAT,      /* R16G16B16A16_SFLOAT */
    VK_FORMAT_R32_SFLOAT,               /* R32_SFLOAT */
    VK_FORMAT_R32G32_SFLOAT,            /* R32G32_SFLOAT */
    VK_FORMAT_R32G32B32A32_SFLOAT,      /* R32G32B32A32_SFLOAT */
    VK_FORMAT_R8_UINT,                  /* R8_UINT */
    VK_FORMAT_R8G8_UINT,                /* R8G8_UINT */
    VK_FORMAT_R8G8B8A8_UINT,            /* R8G8B8A8_UINT */
    VK_FORMAT_R16_UINT,                 /* R16_UINT */
    VK_FORMAT_R16G16_UINT,              /* R16G16_UINT */
    VK_FORMAT_R16G16B16A16_UINT,        /* R16G16B16A16_UINT */
    VK_FORMAT_R8G8B8A8_SRGB,            /* R8G8B8A8_SRGB */
    VK_FORMAT_B8G8R8A8_SRGB,            /* B8G8R8A8_SRGB */
    VK_FORMAT_BC3_SRGB_BLOCK,           /* BC3_SRGB */
    VK_FORMAT_BC7_SRGB_BLOCK,           /* BC7_SRGB */
    VK_FORMAT_D16_UNORM,                /* D16_UNORM */
    VK_FORMAT_X8_D24_UNORM_PACK32,      /* D24_UNORM */
    VK_FORMAT_D32_SFLOAT,               /* D32_SFLOAT */
    VK_FORMAT_D24_UNORM_S8_UINT,        /* D24_UNORM_S8_UINT */
    VK_FORMAT_D32_SFLOAT_S8_UINT,       /* D32_SFLOAT_S8_UINT */
};

static VkComponentMapping SDLToVK_SurfaceSwizzle[] = {
    IDENTITY_SWIZZLE, /* R8G8B8A8 */
    IDENTITY_SWIZZLE, /* B8G8R8A8 */
    {                 /* B5G6R5 */
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_ONE,
    },
    {                 /* B5G5R5A1 */
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_A,
    },
    IDENTITY_SWIZZLE, /* B4G4R4A4 */
    {                 /* R10G10B10A2 */
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    },
    IDENTITY_SWIZZLE, /* R16G16 */
    IDENTITY_SWIZZLE, /* R16G16B16A16 */
    IDENTITY_SWIZZLE, /* R8 */
    {                 /* A8 */
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_R,
    },
    IDENTITY_SWIZZLE, /* BC1 */
    IDENTITY_SWIZZLE, /* BC2 */
    IDENTITY_SWIZZLE, /* BC3 */
    IDENTITY_SWIZZLE, /* BC7 */
    IDENTITY_SWIZZLE, /* R8G8_SNORM */
    IDENTITY_SWIZZLE, /* R8G8B8A8_SNORM */
    IDENTITY_SWIZZLE, /* R16_SFLOAT */
    IDENTITY_SWIZZLE, /* R16G16_SFLOAT */
    IDENTITY_SWIZZLE, /* R16G16B16A16_SFLOAT */
    IDENTITY_SWIZZLE, /* R32_SFLOAT */
    IDENTITY_SWIZZLE, /* R32G32_SFLOAT */
    IDENTITY_SWIZZLE, /* R32G32B32A32_SFLOAT */
    IDENTITY_SWIZZLE, /* R8_UINT */
    IDENTITY_SWIZZLE, /* R8G8_UINT */
    IDENTITY_SWIZZLE, /* R8G8B8A8_UINT */
    IDENTITY_SWIZZLE, /* R16_UINT */
    IDENTITY_SWIZZLE, /* R16G16_UINT */
    IDENTITY_SWIZZLE, /* R16G16B16A16_UINT */
    IDENTITY_SWIZZLE, /* R8G8B8A8_SRGB */
    IDENTITY_SWIZZLE, /* B8G8R8A8_SRGB */
    IDENTITY_SWIZZLE, /* BC3_SRGB */
    IDENTITY_SWIZZLE, /* BC7_SRGB */
    IDENTITY_SWIZZLE, /* D16_UNORM */
    IDENTITY_SWIZZLE, /* D24_UNORM */
    IDENTITY_SWIZZLE, /* D32_SFLOAT */
    IDENTITY_SWIZZLE, /* D24_UNORM_S8_UINT */
    IDENTITY_SWIZZLE, /* D32_SFLOAT_S8_UINT */
};

/* from SWAPCHAINCOMPOSITION */
static VkFormat SwapchainCompositionToFormat[] = {
    VK_FORMAT_B8G8R8A8_UNORM,          /* SDR */
    VK_FORMAT_B8G8R8A8_SRGB,           /* SDR_SRGB */
    VK_FORMAT_R16G16B16A16_SFLOAT,     /* HDR */
    VK_FORMAT_A2B10G10R10_UNORM_PACK32 /* HDR_ADVANCED */
};

/* from SWAPCHAINCOMPOSITION */
static VkColorSpaceKHR SwapchainCompositionToColorSpace[] = {
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
    VK_COLOR_SPACE_HDR10_ST2084_EXT
};

static VkComponentMapping SwapchainCompositionSwizzle[] = {
    IDENTITY_SWIZZLE, /* SDR */
    IDENTITY_SWIZZLE, /* SDR_SRGB */
    IDENTITY_SWIZZLE, /* HDR */
    {                 /* HDR_ADVANCED */
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    }
};

static VkFormat SDLToVK_VertexFormat[] = {
    VK_FORMAT_R32_UINT,             /* UINT */
    VK_FORMAT_R32_SFLOAT,           /* FLOAT */
    VK_FORMAT_R32G32_SFLOAT,        /* VECTOR2 */
    VK_FORMAT_R32G32B32_SFLOAT,     /* VECTOR3 */
    VK_FORMAT_R32G32B32A32_SFLOAT,  /* VECTOR4 */
    VK_FORMAT_R8G8B8A8_UNORM,       /* COLOR */
    VK_FORMAT_R8G8B8A8_USCALED,     /* BYTE4 */
    VK_FORMAT_R16G16_SSCALED,       /* SHORT2 */
    VK_FORMAT_R16G16B16A16_SSCALED, /* SHORT4 */
    VK_FORMAT_R16G16_SNORM,         /* NORMALIZEDSHORT2 */
    VK_FORMAT_R16G16B16A16_SNORM,   /* NORMALIZEDSHORT4 */
    VK_FORMAT_R16G16_SFLOAT,        /* HALFVECTOR2 */
    VK_FORMAT_R16G16B16A16_SFLOAT   /* HALFVECTOR4 */
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

static VkPolygonMode SDLToVK_PolygonMode[] = {
    VK_POLYGON_MODE_FILL,
    VK_POLYGON_MODE_LINE,
    VK_POLYGON_MODE_POINT
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
    VK_SAMPLE_COUNT_8_BIT,
    VK_SAMPLE_COUNT_16_BIT,
    VK_SAMPLE_COUNT_32_BIT,
    VK_SAMPLE_COUNT_64_BIT
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

/* Structures */

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

/* Memory Allocation */

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
    VkDeviceSize resourceOffset; /* differs from offset based on alignment*/
    VkDeviceSize resourceSize;   /* differs from size based on alignment */
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

/* Memory structures */

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
    SDL_GpuBufferUsageFlags usageFlags;

    SDL_AtomicInt referenceCount; /* Tracks command buffer usage */

    VulkanBufferHandle *handle;

    SDL_bool transitioned;
    Uint8 markedForDestroy; /* so that defrag doesn't double-free */
};

/* Buffer resources consist of multiple backing buffer handles so that data transfers
 * can occur without blocking or the client having to manage extra resources.
 *
 * Cast from SDL_GpuBuffer or SDL_GpuTransferBuffer.
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

/* Renderer Structure */

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

/* Textures are made up of individual slices.
 * This helps us barrier the resource efficiently.
 */
typedef struct VulkanTextureSlice
{
    VulkanTexture *parent;
    Uint32 layer;
    Uint32 level;

    SDL_AtomicInt referenceCount;

    VkImageView view;
    VulkanTextureHandle *msaaTexHandle; /* NULL if parent sample count is 1 or is depth target */

    SDL_bool transitioned; /* used for layout tracking */
} VulkanTextureSlice;

struct VulkanTexture
{
    VulkanMemoryUsedRegion *usedRegion;

    VkImage image;
    VkImageView view;
    VkExtent2D dimensions;

    Uint8 is3D;
    Uint8 isCube;
    Uint8 isRenderTarget;
    Uint8 isMSAAColorTarget;

    Uint32 depth;
    Uint32 layerCount;
    Uint32 levelCount;
    VkSampleCountFlagBits sampleCount; /* NOTE: This refers to the sample count of a render target pass using this texture, not the actual sample count of the texture */
    VkFormat format;
    VkComponentMapping swizzle;
    SDL_GpuTextureUsageFlags usageFlags;
    VkImageAspectFlags aspectFlags;

    Uint32 sliceCount;
    VulkanTextureSlice *slices;

    VulkanTextureHandle *handle;

    Uint8 markedForDestroy; /* so that defrag doesn't double-free */
};

/* Texture resources consist of multiple backing texture handles so that data transfers
 * can occur without blocking or the client having to manage extra resources.
 *
 * Cast from SDL_GpuTexture.
 */
struct VulkanTextureContainer
{
    VulkanTextureHandle *activeTextureHandle;

    /* These are all the texture handles that have been used by this container.
     * If the resource is bound and then updated with CYCLE, a new resource
     * will be added to this list.
     * These can be reused after they are submitted and command processing is complete.
     */
    Uint32 textureCapacity;
    Uint32 textureCount;
    VulkanTextureHandle **textureHandles;

    /* Swapchain images cannot be cycled */
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
    /* Window surface */
    VkSurfaceKHR surface;

    /* Swapchain for window surface */
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkColorSpaceKHR colorSpace;
    VkComponentMapping swapchainSwizzle;
    VkPresentModeKHR presentMode;

    /* Swapchain images */
    VkExtent2D extent;
    VulkanTextureContainer *textureContainers; /* use containers so that swapchain textures can use the same API as other textures */
    Uint32 imageCount;

    /* Synchronization primitives */
    VkSemaphore imageAvailableSemaphore[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphore[MAX_FRAMES_IN_FLIGHT];
    VulkanFenceHandle *inFlightFences[MAX_FRAMES_IN_FLIGHT];

    Uint32 frameCounter;
} VulkanSwapchainData;

typedef struct WindowData
{
    SDL_Window *window;
    SDL_GpuSwapchainComposition swapchainComposition;
    SDL_GpuPresentMode presentMode;
    VulkanSwapchainData *swapchainData;
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

    /* This is actually a descriptor set and descriptor pool simultaneously */
    VkDescriptorPool *descriptorPools;
    Uint32 descriptorPoolCount;
    Uint32 nextPoolSize;

    /* We just manage a pool ourselves instead of freeing the sets */
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
    SDL_GpuPrimitiveType primitiveType;

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
     * 1: read-write textures, then read-write buffers
     * 2: uniform buffers
     */
    DescriptorSetPool descriptorSetPools[3];

    Uint32 readOnlyStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 readWriteStorageTextureCount;
    Uint32 readWriteStorageBufferCount;
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
    SDL_GpuColor clearColor;
    SDL_GpuLoadOp loadOp;
    SDL_GpuStoreOp storeOp;
} RenderPassColorTargetDescription;

typedef struct RenderPassDepthStencilTargetDescription
{
    VkFormat format;
    SDL_GpuLoadOp loadOp;
    SDL_GpuStoreOp storeOp;
    SDL_GpuLoadOp stencilLoadOp;
    SDL_GpuStoreOp stencilStoreOp;
} RenderPassDepthStencilTargetDescription;

typedef struct RenderPassHash
{
    RenderPassColorTargetDescription colorTargetDescriptions[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentCount;
    RenderPassDepthStencilTargetDescription depthStencilTargetDescription;
    VkSampleCountFlagBits colorAttachmentSampleCount;
} RenderPassHash;

typedef struct RenderPassHashMap
{
    RenderPassHash key;
    VkRenderPass value;
} RenderPassHashMap;

typedef struct RenderPassHashArray
{
    RenderPassHashMap *elements;
    Sint32 count;
    Sint32 capacity;
} RenderPassHashArray;

static inline Uint8 RenderPassHash_Compare(
    RenderPassHash *a,
    RenderPassHash *b)
{
    Uint32 i;

    if (a->colorAttachmentCount != b->colorAttachmentCount) {
        return 0;
    }

    if (a->colorAttachmentSampleCount != b->colorAttachmentSampleCount) {
        return 0;
    }

    for (i = 0; i < a->colorAttachmentCount; i += 1) {
        if (a->colorTargetDescriptions[i].format != b->colorTargetDescriptions[i].format) {
            return 0;
        }

        if (a->colorTargetDescriptions[i].clearColor.r != b->colorTargetDescriptions[i].clearColor.r ||
            a->colorTargetDescriptions[i].clearColor.g != b->colorTargetDescriptions[i].clearColor.g ||
            a->colorTargetDescriptions[i].clearColor.b != b->colorTargetDescriptions[i].clearColor.b ||
            a->colorTargetDescriptions[i].clearColor.a != b->colorTargetDescriptions[i].clearColor.a) {
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

static inline VkRenderPass RenderPassHashArray_Fetch(
    RenderPassHashArray *arr,
    RenderPassHash *key)
{
    Sint32 i;

    for (i = 0; i < arr->count; i += 1) {
        RenderPassHash *e = &arr->elements[i].key;

        if (RenderPassHash_Compare(e, key)) {
            return arr->elements[i].value;
        }
    }

    return VK_NULL_HANDLE;
}

static inline void RenderPassHashArray_Insert(
    RenderPassHashArray *arr,
    RenderPassHash key,
    VkRenderPass value)
{
    RenderPassHashMap map;

    map.key = key;
    map.value = value;

    EXPAND_ELEMENTS_IF_NEEDED(arr, 4, RenderPassHashMap)

    arr->elements[arr->count] = map;
    arr->count += 1;
}

typedef struct FramebufferHash
{
    VkImageView colorAttachmentViews[MAX_COLOR_TARGET_BINDINGS];
    VkImageView colorMultiSampleAttachmentViews[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentCount;
    VkImageView depthStencilAttachmentView;
    Uint32 width;
    Uint32 height;
} FramebufferHash;

typedef struct FramebufferHashMap
{
    FramebufferHash key;
    VulkanFramebuffer *value;
} FramebufferHashMap;

typedef struct FramebufferHashArray
{
    FramebufferHashMap *elements;
    Sint32 count;
    Sint32 capacity;
} FramebufferHashArray;

static inline Uint8 FramebufferHash_Compare(
    FramebufferHash *a,
    FramebufferHash *b)
{
    Uint32 i;

    if (a->colorAttachmentCount != b->colorAttachmentCount) {
        return 0;
    }

    for (i = 0; i < a->colorAttachmentCount; i += 1) {
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

static inline VulkanFramebuffer *FramebufferHashArray_Fetch(
    FramebufferHashArray *arr,
    FramebufferHash *key)
{
    Sint32 i;

    for (i = 0; i < arr->count; i += 1) {
        FramebufferHash *e = &arr->elements[i].key;
        if (FramebufferHash_Compare(e, key)) {
            return arr->elements[i].value;
        }
    }

    return VK_NULL_HANDLE;
}

static inline void FramebufferHashArray_Insert(
    FramebufferHashArray *arr,
    FramebufferHash key,
    VulkanFramebuffer *value)
{
    FramebufferHashMap map;
    map.key = key;
    map.value = value;

    EXPAND_ELEMENTS_IF_NEEDED(arr, 4, FramebufferHashMap)

    arr->elements[arr->count] = map;
    arr->count += 1;
}

static inline void FramebufferHashArray_Remove(
    FramebufferHashArray *arr,
    Uint32 index)
{
    if (index != arr->count - 1) {
        arr->elements[index] = arr->elements[arr->count - 1];
    }

    arr->count -= 1;
}

/* Command structures */

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

    /* Keep track of resources transitioned away from their default state to barrier them on pass end */

    VulkanTextureSlice *colorAttachmentSlices[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorAttachmentSliceCount;

    VulkanTextureSlice *depthStencilAttachmentSlice; /* may be NULL */

    /* Viewport/scissor state */

    VkViewport currentViewport;
    VkRect2D currentScissor;

    /* Resource bind state */

    SDL_bool needNewVertexResourceDescriptorSet;
    SDL_bool needNewVertexUniformDescriptorSet;
    SDL_bool needNewVertexUniformOffsets;
    SDL_bool needNewFragmentResourceDescriptorSet;
    SDL_bool needNewFragmentUniformDescriptorSet;
    SDL_bool needNewFragmentUniformOffsets;

    SDL_bool needNewComputeReadOnlyDescriptorSet;
    SDL_bool needNewComputeReadWriteDescriptorSet;
    SDL_bool needNewComputeUniformDescriptorSet;
    SDL_bool needNewComputeUniformOffsets;

    VkDescriptorSet vertexResourceDescriptorSet;
    VkDescriptorSet vertexUniformDescriptorSet;
    VkDescriptorSet fragmentResourceDescriptorSet;
    VkDescriptorSet fragmentUniformDescriptorSet;

    VkDescriptorSet computeReadOnlyDescriptorSet;
    VkDescriptorSet computeReadWriteDescriptorSet;
    VkDescriptorSet computeUniformDescriptorSet;

    DescriptorSetData *boundDescriptorSetDatas;
    Uint32 boundDescriptorSetDataCount;
    Uint32 boundDescriptorSetDataCapacity;

    VulkanTexture *vertexSamplerTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanSampler *vertexSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanTextureSlice *vertexStorageTextureSlices[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *vertexStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    VulkanTexture *fragmentSamplerTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanSampler *fragmentSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    VulkanTextureSlice *fragmentStorageTextureSlices[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *fragmentStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    VulkanTextureSlice *readWriteComputeStorageTextureSlices[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *readWriteComputeStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    VulkanTextureSlice *readOnlyComputeStorageTextureSlices[MAX_STORAGE_TEXTURES_PER_STAGE];
    VulkanBuffer *readOnlyComputeStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

    /* Uniform buffers */

    VulkanUniformBuffer *vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    VulkanUniformBuffer *fragmentUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    VulkanUniformBuffer *computeUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];

    /* Track used resources */

    VulkanBuffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    VulkanTextureSlice **usedTextureSlices;
    Uint32 usedTextureSliceCount;
    Uint32 usedTextureSliceCapacity;

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

    Uint8 isDefrag; /* Whether this CB was created for defragging */
} VulkanCommandBuffer;

struct VulkanCommandPool
{
    SDL_ThreadID threadID;
    VkCommandPool commandPool;

    VulkanCommandBuffer **inactiveCommandBuffers;
    Uint32 inactiveCommandBufferCapacity;
    Uint32 inactiveCommandBufferCount;
};

#define NUM_COMMAND_POOL_BUCKETS 1031

typedef struct CommandPoolHash
{
    SDL_ThreadID threadID;
} CommandPoolHash;

typedef struct CommandPoolHashMap
{
    CommandPoolHash key;
    VulkanCommandPool *value;
} CommandPoolHashMap;

typedef struct CommandPoolHashArray
{
    CommandPoolHashMap *elements;
    Uint32 count;
    Uint32 capacity;
} CommandPoolHashArray;

typedef struct CommandPoolHashTable
{
    CommandPoolHashArray buckets[NUM_COMMAND_POOL_BUCKETS];
} CommandPoolHashTable;

static inline uint64_t CommandPoolHashTable_GetHashCode(CommandPoolHash key)
{
    const uint64_t HASH_FACTOR = 97;
    uint64_t result = 1;
    result = result * HASH_FACTOR + (uint64_t)key.threadID;
    return result;
}

static inline VulkanCommandPool *CommandPoolHashTable_Fetch(
    CommandPoolHashTable *table,
    CommandPoolHash key)
{
    Uint32 i;
    uint64_t hashcode = CommandPoolHashTable_GetHashCode(key);
    CommandPoolHashArray *arr = &table->buckets[hashcode % NUM_COMMAND_POOL_BUCKETS];

    for (i = 0; i < arr->count; i += 1) {
        const CommandPoolHash *e = &arr->elements[i].key;
        if (key.threadID == e->threadID) {
            return arr->elements[i].value;
        }
    }

    return NULL;
}

static inline void CommandPoolHashTable_Insert(
    CommandPoolHashTable *table,
    CommandPoolHash key,
    VulkanCommandPool *value)
{
    uint64_t hashcode = CommandPoolHashTable_GetHashCode(key);
    CommandPoolHashArray *arr = &table->buckets[hashcode % NUM_COMMAND_POOL_BUCKETS];

    CommandPoolHashMap map;
    map.key = key;
    map.value = value;

    EXPAND_ELEMENTS_IF_NEEDED(arr, 4, CommandPoolHashMap)

    arr->elements[arr->count] = map;
    arr->count += 1;
}

/* Context */

struct VulkanRenderer
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    VkPhysicalDeviceDriverPropertiesKHR physicalDeviceDriverProperties;
    VkDevice logicalDevice;
    Uint8 integratedMemoryNotification;
    Uint8 outOfDeviceLocalMemoryWarning;
    Uint8 outofBARMemoryWarning;

    SDL_bool debugMode;
    VulkanExtensions supports;
    SDL_bool supportsDebugUtils;
    SDL_bool supportsColorspace;

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

    CommandPoolHashTable commandPoolHashTable;
    RenderPassHashArray renderPassHashArray;
    FramebufferHashArray framebufferHashArray;

    VulkanUniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    Uint32 minUBOAlignment;

    /* Some drivers don't support D16 for some reason. Fun! */
    VkFormat D16Format;
    VkFormat D16S8Format;

    /* Deferred resource destruction */

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

#define VULKAN_INSTANCE_FUNCTION(ext, ret, func, params) \
    vkfntype_##func func;
#define VULKAN_DEVICE_FUNCTION(ext, ret, func, params) \
    vkfntype_##func func;
#include "SDL_gpu_vulkan_vkfuncs.h"
};

/* Forward declarations */

static Uint8 VULKAN_INTERNAL_DefragmentMemory(VulkanRenderer *renderer);
static void VULKAN_INTERNAL_BeginCommandBuffer(VulkanRenderer *renderer, VulkanCommandBuffer *commandBuffer);
static void VULKAN_UnclaimWindow(SDL_GpuRenderer *driverData, SDL_Window *window);
static void VULKAN_Wait(SDL_GpuRenderer *driverData);
static void VULKAN_WaitForFences(SDL_GpuRenderer *driverData, SDL_bool waitAll, SDL_GpuFence **pFences, Uint32 fenceCount);
static void VULKAN_Submit(SDL_GpuCommandBuffer *commandBuffer);
static VulkanTextureSlice *VULKAN_INTERNAL_FetchTextureSlice(VulkanTexture *texture, Uint32 layer, Uint32 level);
static VulkanTexture *VULKAN_INTERNAL_CreateTexture(
    VulkanRenderer *renderer,
    Uint32 width,
    Uint32 height,
    Uint32 depth,
    Uint32 isCube,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GpuTextureUsageFlags textureUsageFlags,
    SDL_bool isMSAAColorTarget);

/* Error Handling */

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
            SDL_LOG_CATEGORY_APPLICATION,
            "%s: %s",
            vulkanFunctionName,
            VkErrorMessages(result));
    }
}

#define VULKAN_ERROR_CHECK(res, fn, ret)                                                \
    if (res != VK_SUCCESS) {                                                            \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s %s", #fn, VkErrorMessages(res)); \
        return ret;                                                                     \
    }

/* Utility */

static inline SDL_bool VULKAN_INTERNAL_IsVulkanDepthFormat(VkFormat format)
{
    /* FIXME: Can we refactor and use the regular IsDepthFormat for this? */
    return (
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D16_UNORM] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D24_UNORM] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D32_SFLOAT] ||
        format == SDLToVK_SurfaceFormat[SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT]);
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

/* Memory Management */

/* Vulkan: Memory Allocation */

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

        /* close the gap in the sorted list */
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
        /* close the gap in the sorted list */
        if (freeRegion->allocation->allocator->sortedFreeRegionCount > 1) {
            for (i = freeRegion->sortedIndex; i < freeRegion->allocation->allocator->sortedFreeRegionCount - 1; i += 1) {
                freeRegion->allocation->allocator->sortedFreeRegions[i] =
                    freeRegion->allocation->allocator->sortedFreeRegions[i + 1];

                freeRegion->allocation->allocator->sortedFreeRegions[i]->sortedIndex = i;
            }
        }

        freeRegion->allocation->allocator->sortedFreeRegionCount -= 1;
    }

    /* close the gap in the buffer list */
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
    Sint32 i;

    SDL_LockMutex(renderer->allocatorLock);

    /* look for an adjacent region to merge */
    for (i = allocation->freeRegionCount - 1; i >= 0; i -= 1) {
        /* check left side */
        if (allocation->freeRegions[i]->offset + allocation->freeRegions[i]->size == offset) {
            newOffset = allocation->freeRegions[i]->offset;
            newSize = allocation->freeRegions[i]->size + size;

            VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, allocation->freeRegions[i]);
            VULKAN_INTERNAL_NewMemoryFreeRegion(renderer, allocation, newOffset, newSize);

            SDL_UnlockMutex(renderer->allocatorLock);
            return;
        }

        /* check right side */
        if (allocation->freeRegions[i]->offset == offset + size) {
            newOffset = offset;
            newSize = allocation->freeRegions[i]->size + size;

            VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, allocation->freeRegions[i]);
            VULKAN_INTERNAL_NewMemoryFreeRegion(renderer, allocation, newOffset, newSize);

            SDL_UnlockMutex(renderer->allocatorLock);
            return;
        }
    }

    /* region is not contiguous with another free region, make a new one */
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
        for (i = 0; i < allocation->allocator->sortedFreeRegionCount; i += 1) {
            if (allocation->allocator->sortedFreeRegions[i]->size < size) {
                /* this is where the new region should go */
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

        /* perform insertion sort */
        if (allocation->allocator->sortedFreeRegionCount > 0 && insertionIndex != allocation->allocator->sortedFreeRegionCount) {
            for (i = allocation->allocator->sortedFreeRegionCount; i > insertionIndex && i > 0; i -= 1) {
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
            /* plug the hole */
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

static Uint8 VULKAN_INTERNAL_FindMemoryType(
    VulkanRenderer *renderer,
    Uint32 typeFilter,
    VkMemoryPropertyFlags requiredProperties,
    VkMemoryPropertyFlags ignoredProperties,
    Uint32 *memoryTypeIndex)
{
    Uint32 i;

    for (i = *memoryTypeIndex; i < renderer->memoryProperties.memoryTypeCount; i += 1) {
        if ((typeFilter & (1 << i)) &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties &&
            (renderer->memoryProperties.memoryTypes[i].propertyFlags & ignoredProperties) == 0) {
            *memoryTypeIndex = i;
            return 1;
        }
    }

    return 0;
}

static Uint8 VULKAN_INTERNAL_FindBufferMemoryRequirements(
    VulkanRenderer *renderer,
    VkBuffer buffer,
    VkMemoryPropertyFlags requiredMemoryProperties,
    VkMemoryPropertyFlags ignoredMemoryProperties,
    VkMemoryRequirements2KHR *pMemoryRequirements,
    Uint32 *pMemoryTypeIndex)
{
    VkBufferMemoryRequirementsInfo2KHR bufferRequirementsInfo;
    bufferRequirementsInfo.sType =
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR;
    bufferRequirementsInfo.pNext = NULL;
    bufferRequirementsInfo.buffer = buffer;

    renderer->vkGetBufferMemoryRequirements2KHR(
        renderer->logicalDevice,
        &bufferRequirementsInfo,
        pMemoryRequirements);

    return VULKAN_INTERNAL_FindMemoryType(
        renderer,
        pMemoryRequirements->memoryRequirements.memoryTypeBits,
        requiredMemoryProperties,
        ignoredMemoryProperties,
        pMemoryTypeIndex);
}

static Uint8 VULKAN_INTERNAL_FindImageMemoryRequirements(
    VulkanRenderer *renderer,
    VkImage image,
    VkMemoryPropertyFlags requiredMemoryPropertyFlags,
    VkMemoryRequirements2KHR *pMemoryRequirements,
    Uint32 *pMemoryTypeIndex)
{
    VkImageMemoryRequirementsInfo2KHR imageRequirementsInfo;
    imageRequirementsInfo.sType =
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
    imageRequirementsInfo.pNext = NULL;
    imageRequirementsInfo.image = image;

    renderer->vkGetImageMemoryRequirements2KHR(
        renderer->logicalDevice,
        &imageRequirementsInfo,
        pMemoryRequirements);

    return VULKAN_INTERNAL_FindMemoryType(
        renderer,
        pMemoryRequirements->memoryRequirements.memoryTypeBits,
        requiredMemoryPropertyFlags,
        0,
        pMemoryTypeIndex);
}

static void VULKAN_INTERNAL_DeallocateMemory(
    VulkanRenderer *renderer,
    VulkanMemorySubAllocator *allocator,
    Uint32 allocationIndex)
{
    Uint32 i;

    VulkanMemoryAllocation *allocation = allocator->allocations[allocationIndex];

    SDL_LockMutex(renderer->allocatorLock);

    /* If this allocation was marked for defrag, cancel that */
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
    allocation->freeSpace = 0; /* added by FreeRegions */
    allocation->usedSpace = 0; /* added by UsedRegions */
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
        /* Uh oh, we couldn't allocate, time to clean up */
        SDL_free(allocation->freeRegions);

        allocator->allocationCount -= 1;
        allocator->allocations = SDL_realloc(
            allocator->allocations,
            sizeof(VulkanMemoryAllocation *) * allocator->allocationCount);

        SDL_free(allocation);

        return 0;
    }

    /* Persistent mapping for host-visible memory */
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
    VkMemoryRequirements2KHR *memoryRequirements,
    VkDeviceSize resourceSize, /* may be different from requirements size! */
    VkBuffer buffer,           /* may be VK_NULL_HANDLE */
    VkImage image,             /* may be VK_NULL_HANDLE */
    VulkanMemoryUsedRegion **pMemoryUsedRegion)
{
    VulkanMemoryAllocation *allocation;
    VulkanMemorySubAllocator *allocator;
    VulkanMemoryFreeRegion *region;
    VulkanMemoryFreeRegion *selectedRegion;
    VulkanMemoryUsedRegion *usedRegion;

    VkDeviceSize requiredSize, allocationSize;
    VkDeviceSize alignedOffset;
    Uint32 newRegionSize, newRegionOffset;
    Uint8 isHostVisible, smallAllocation, allocationResult;
    Sint32 i;

    isHostVisible =
        (renderer->memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

    allocator = &renderer->memoryAllocator->subAllocators[memoryTypeIndex];
    requiredSize = memoryRequirements->memoryRequirements.size;
    smallAllocation = requiredSize <= SMALL_ALLOCATION_THRESHOLD;

    if ((buffer == VK_NULL_HANDLE && image == VK_NULL_HANDLE) ||
        (buffer != VK_NULL_HANDLE && image != VK_NULL_HANDLE)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "BindResourceMemory must be given either a VulkanBuffer or a VulkanTexture");
        return 0;
    }

    SDL_LockMutex(renderer->allocatorLock);

    selectedRegion = NULL;

    for (i = allocator->sortedFreeRegionCount - 1; i >= 0; i -= 1) {
        region = allocator->sortedFreeRegions[i];

        if (smallAllocation && region->allocation->size != SMALL_ALLOCATION_SIZE) {
            /* region is not in a small allocation */
            continue;
        }

        if (!smallAllocation && region->allocation->size == SMALL_ALLOCATION_SIZE) {
            /* allocation is not small and current region is in a small allocation */
            continue;
        }

        alignedOffset = VULKAN_INTERNAL_NextHighestAlignment(
            region->offset,
            memoryRequirements->memoryRequirements.alignment);

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
            memoryRequirements->memoryRequirements.alignment);

        usedRegion->isBuffer = buffer != VK_NULL_HANDLE;

        newRegionSize = region->size - ((alignedOffset - region->offset) + requiredSize);
        newRegionOffset = alignedOffset + requiredSize;

        /* remove and add modified region to re-sort */
        VULKAN_INTERNAL_RemoveMemoryFreeRegion(renderer, region);

        /* if size is 0, no need to re-insert */
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

    /* No suitable free regions exist, allocate a new memory region */
    if (
        renderer->allocationsToDefragCount == 0 &&
        !renderer->defragInProgress) {
        /* Mark currently fragmented allocations for defrag */
        VULKAN_INTERNAL_MarkAllocationsForDefrag(renderer);
    }

    if (requiredSize > SMALL_ALLOCATION_THRESHOLD) {
        /* allocate a page of required size aligned to LARGE_ALLOCATION_INCREMENT increments */
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

    /* Uh oh, we're out of memory */
    if (allocationResult == 0) {
        SDL_UnlockMutex(renderer->allocatorLock);

        /* Responsibility of the caller to handle being out of memory */
        return 2;
    }

    usedRegion = VULKAN_INTERNAL_NewMemoryUsedRegion(
        renderer,
        allocation,
        0,
        requiredSize,
        0,
        resourceSize,
        memoryRequirements->memoryRequirements.alignment);

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
    Uint32 memoryTypeIndex = 0;
    VkMemoryPropertyFlags requiredMemoryPropertyFlags;
    VkMemoryRequirements2KHR memoryRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
        NULL
    };

    /* Prefer GPU allocation for textures */
    requiredMemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    while (VULKAN_INTERNAL_FindImageMemoryRequirements(
        renderer,
        image,
        requiredMemoryPropertyFlags,
        &memoryRequirements,
        &memoryTypeIndex)) {
        bindResult = VULKAN_INTERNAL_BindResourceMemory(
            renderer,
            memoryTypeIndex,
            &memoryRequirements,
            memoryRequirements.memoryRequirements.size,
            VK_NULL_HANDLE,
            image,
            usedRegion);

        if (bindResult == 1) {
            break;
        } else /* Bind failed, try the next device-local heap */
        {
            memoryTypeIndex += 1;
        }
    }

    /* Bind _still_ failed, try again without device local */
    if (bindResult != 1) {
        memoryTypeIndex = 0;
        requiredMemoryPropertyFlags = 0;

        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Out of device-local memory, allocating textures on host-local memory!");

        while (VULKAN_INTERNAL_FindImageMemoryRequirements(
            renderer,
            image,
            requiredMemoryPropertyFlags,
            &memoryRequirements,
            &memoryTypeIndex)) {
            bindResult = VULKAN_INTERNAL_BindResourceMemory(
                renderer,
                memoryTypeIndex,
                &memoryRequirements,
                memoryRequirements.memoryRequirements.size,
                VK_NULL_HANDLE,
                image,
                usedRegion);

            if (bindResult == 1) {
                break;
            } else /* Bind failed, try the next heap */
            {
                memoryTypeIndex += 1;
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
    Uint32 memoryTypeIndex = 0;
    VkMemoryPropertyFlags requiredMemoryPropertyFlags = 0;
    VkMemoryPropertyFlags ignoredMemoryPropertyFlags = 0;
    VkMemoryRequirements2KHR memoryRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
        NULL
    };

    if (type == VULKAN_BUFFER_TYPE_GPU) {
        requiredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else if (type == VULKAN_BUFFER_TYPE_UNIFORM) {
        requiredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else if (type == VULKAN_BUFFER_TYPE_TRANSFER) {
        requiredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        ignoredMemoryPropertyFlags |=
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized buffer type!");
        return 0;
    }

    while (VULKAN_INTERNAL_FindBufferMemoryRequirements(
        renderer,
        buffer,
        requiredMemoryPropertyFlags,
        ignoredMemoryPropertyFlags,
        &memoryRequirements,
        &memoryTypeIndex)) {
        bindResult = VULKAN_INTERNAL_BindResourceMemory(
            renderer,
            memoryTypeIndex,
            &memoryRequirements,
            size,
            buffer,
            VK_NULL_HANDLE,
            usedRegion);

        if (bindResult == 1) {
            break;
        } else /* Bind failed, try the next device-local heap */
        {
            memoryTypeIndex += 1;
        }
    }

    /* Bind failed, try again without preferred flags */
    if (bindResult != 1) {
        memoryTypeIndex = 0;
        requiredMemoryPropertyFlags = 0;
        ignoredMemoryPropertyFlags = 0;

        if (type == VULKAN_BUFFER_TYPE_GPU) {
            if (!renderer->outOfDeviceLocalMemoryWarning) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Out of device-local memory, allocating buffers on host-local memory, expect degraded performance!");
                renderer->outOfDeviceLocalMemoryWarning = 1;
            }
        } else if (type == VULKAN_BUFFER_TYPE_UNIFORM) {
            requiredMemoryPropertyFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            if (!renderer->outofBARMemoryWarning) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Out of BAR memory, allocating uniform buffers on host-local memory, expect degraded performance!");
                renderer->outofBARMemoryWarning = 1;
            }
        } else if (type == VULKAN_BUFFER_TYPE_TRANSFER) {
            requiredMemoryPropertyFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

            if (!renderer->integratedMemoryNotification) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Integrated memory detected, allocating TransferBuffers on device-local memory!");
                renderer->integratedMemoryNotification = 1;
            }
        }

        while (VULKAN_INTERNAL_FindBufferMemoryRequirements(
            renderer,
            buffer,
            requiredMemoryPropertyFlags,
            ignoredMemoryPropertyFlags,
            &memoryRequirements,
            &memoryTypeIndex)) {
            bindResult = VULKAN_INTERNAL_BindResourceMemory(
                renderer,
                memoryTypeIndex,
                &memoryRequirements,
                size,
                buffer,
                VK_NULL_HANDLE,
                usedRegion);

            if (bindResult == 1) {
                break;
            } else /* Bind failed, try the next heap */
            {
                memoryTypeIndex += 1;
            }
        }
    }

    return bindResult;
}

/* Resource tracking */

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

static void VULKAN_INTERNAL_TrackTextureSlice(
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureSlice *textureSlice)
{
    TRACK_RESOURCE(
        textureSlice,
        VulkanTextureSlice *,
        usedTextureSlices,
        usedTextureSliceCount,
        usedTextureSliceCapacity)
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

/* Memory Barriers */

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized buffer source barrier type!");
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized buffer destination barrier type!");
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

    buffer->transitioned = SDL_TRUE;
}

static void VULKAN_INTERNAL_ImageMemoryBarrier(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode sourceUsageMode,
    VulkanTextureUsageMode destinationUsageMode,
    VulkanTextureSlice *textureSlice)
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
    memoryBarrier.image = textureSlice->parent->image;
    memoryBarrier.subresourceRange.aspectMask = textureSlice->parent->aspectFlags;
    memoryBarrier.subresourceRange.baseArrayLayer = textureSlice->layer;
    memoryBarrier.subresourceRange.layerCount = 1;
    memoryBarrier.subresourceRange.baseMipLevel = textureSlice->level;
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized texture source barrier type!");
        return;
    }

    if (!textureSlice->transitioned) {
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized texture destination barrier type!");
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

    textureSlice->transitioned = SDL_TRUE;
}

static VulkanBufferUsageMode VULKAN_INTERNAL_DefaultBufferUsageMode(
    VulkanBuffer *buffer)
{
    /* NOTE: order matters here! */

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Buffer has no default usage mode!");
        return VULKAN_BUFFER_USAGE_MODE_VERTEX_READ;
    }
}

static VulkanTextureUsageMode VULKAN_INTERNAL_DefaultTextureUsageMode(
    VulkanTexture *texture)
{
    /* NOTE: order matters here! */
    /* NOTE: graphics storage bits and sampler bit are mutually exclusive! */

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Texture has no default usage mode!");
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

static void VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode destinationUsageMode,
    VulkanTextureSlice *textureSlice)
{
    VULKAN_INTERNAL_ImageMemoryBarrier(
        renderer,
        commandBuffer,
        VULKAN_INTERNAL_DefaultTextureUsageMode(textureSlice->parent),
        destinationUsageMode,
        textureSlice);
}

static void VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureUsageMode sourceUsageMode,
    VulkanTextureSlice *textureSlice)
{
    VULKAN_INTERNAL_ImageMemoryBarrier(
        renderer,
        commandBuffer,
        sourceUsageMode,
        VULKAN_INTERNAL_DefaultTextureUsageMode(textureSlice->parent),
        textureSlice);
}

/* Resource Disposal */

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
    FramebufferHash *hash;
    Sint32 i, j;

    SDL_LockMutex(renderer->framebufferFetchLock);

    for (i = renderer->framebufferHashArray.count - 1; i >= 0; i -= 1) {
        hash = &renderer->framebufferHashArray.elements[i].key;

        for (j = 0; j < hash->colorAttachmentCount; j += 1) {
            if (hash->colorAttachmentViews[j] == view) {
                /* FIXME: do we actually need to queue this?
                 * The framebuffer should not be in use once the associated texture is being destroyed
                 */
                VULKAN_INTERNAL_ReleaseFramebuffer(
                    renderer,
                    renderer->framebufferHashArray.elements[i].value);

                FramebufferHashArray_Remove(
                    &renderer->framebufferHashArray,
                    i);

                break;
            }
        }
    }

    SDL_UnlockMutex(renderer->framebufferFetchLock);
}

static void VULKAN_INTERNAL_DestroyTexture(
    VulkanRenderer *renderer,
    VulkanTexture *texture)
{
    Uint32 sliceIndex;

    /* Clean up slices */
    for (sliceIndex = 0; sliceIndex < texture->sliceCount; sliceIndex += 1) {
        if (texture->isRenderTarget) {
            VULKAN_INTERNAL_RemoveFramebuffersContainingView(
                renderer,
                texture->slices[sliceIndex].view);

            if (texture->slices[sliceIndex].msaaTexHandle != NULL) {
                VULKAN_INTERNAL_DestroyTexture(
                    renderer,
                    texture->slices[sliceIndex].msaaTexHandle->vulkanTexture);
                SDL_free(texture->slices[sliceIndex].msaaTexHandle);
            }
        }

        renderer->vkDestroyImageView(
            renderer->logicalDevice,
            texture->slices[sliceIndex].view,
            NULL);
    }

    SDL_free(texture->slices);

    renderer->vkDestroyImageView(
        renderer->logicalDevice,
        texture->view,
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
        SDL_free(commandBuffer->usedTextureSlices);
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
        renderer->vkDestroyImageView(
            renderer->logicalDevice,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].view,
            NULL);

        SDL_free(swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices);

        renderer->vkDestroyImageView(
            renderer->logicalDevice,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->view,
            NULL);

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

/* Descriptor pool stuff */

static SDL_bool VULKAN_INTERNAL_CreateDescriptorPool(
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
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

static SDL_bool VULKAN_INTERNAL_AllocateDescriptorSets(
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
        return SDL_FALSE;
    }

    SDL_stack_free(descriptorSetLayouts);
    return SDL_TRUE;
}

static void VULKAN_INTERNAL_InitializeDescriptorSetPool(
    VulkanRenderer *renderer,
    DescriptorSetPool *descriptorSetPool)
{
    descriptorSetPool->lock = SDL_CreateMutex();

    /* Descriptor set layout and descriptor infos are already set when this function is called */

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

static SDL_bool VULKAN_INTERNAL_InitializeGraphicsPipelineResourceLayout(
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

    /* Vertex Resources */

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
        return SDL_FALSE;
    }

    /* Vertex UBOs */

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
        return SDL_FALSE;
    }

    /* Fragment resources */

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
        return SDL_FALSE;
    }

    /* Fragment UBOs */

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
        return SDL_FALSE;
    }

    /* Create the pipeline layout */

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
        return SDL_FALSE;
    }

    for (i = 0; i < 4; i += 1) {
        VULKAN_INTERNAL_InitializeDescriptorSetPool(
            renderer,
            &pipelineResourceLayout->descriptorSetPools[i]);
    }

    return SDL_TRUE;
}

static SDL_bool VULKAN_INTERNAL_InitializeComputePipelineResourceLayout(
    VulkanRenderer *renderer,
    SDL_GpuComputePipelineCreateInfo *pipelineCreateInfo,
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
    pipelineResourceLayout->readWriteStorageTextureCount = pipelineCreateInfo->readWriteStorageTextureCount;
    pipelineResourceLayout->readWriteStorageBufferCount = pipelineCreateInfo->readWriteStorageBufferCount;
    pipelineResourceLayout->uniformBufferCount = pipelineCreateInfo->uniformBufferCount;

    /* Read-only resources */

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
        return SDL_FALSE;
    }

    /* Read-write resources */

    descriptorSetLayoutCreateInfo.bindingCount =
        pipelineCreateInfo->readWriteStorageTextureCount +
        pipelineCreateInfo->readWriteStorageBufferCount;

    descriptorSetLayoutCreateInfo.pBindings = NULL;

    descriptorSetPool = &pipelineResourceLayout->descriptorSetPools[1];

    descriptorSetPool->descriptorInfoCount = descriptorSetLayoutCreateInfo.bindingCount;
    descriptorSetPool->descriptorInfos = NULL;

    if (descriptorSetLayoutCreateInfo.bindingCount > 0) {
        descriptorSetPool->descriptorInfos = SDL_malloc(
            descriptorSetPool->descriptorInfoCount * sizeof(VulkanDescriptorInfo));

        for (i = 0; i < pipelineCreateInfo->readWriteStorageTextureCount; i += 1) {
            descriptorSetLayoutBindings[i].binding = i;
            descriptorSetLayoutBindings[i].descriptorCount = 1;
            descriptorSetLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetLayoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBindings[i].pImmutableSamplers = NULL;

            descriptorSetPool->descriptorInfos[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorSetPool->descriptorInfos[i].stageFlag = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        for (i = pipelineCreateInfo->readWriteStorageTextureCount; i < descriptorSetLayoutCreateInfo.bindingCount; i += 1) {
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
        return SDL_FALSE;
    }

    /* Uniform buffers */

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
        return SDL_FALSE;
    }

    /* Create the pipeline layout */

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
        return SDL_FALSE;
    }

    for (i = 0; i < 3; i += 1) {
        VULKAN_INTERNAL_InitializeDescriptorSetPool(
            renderer,
            &pipelineResourceLayout->descriptorSetPools[i]);
    }

    return SDL_TRUE;
}

/* Data Buffer */

static VulkanBuffer *VULKAN_INTERNAL_CreateBuffer(
    VulkanRenderer *renderer,
    VkDeviceSize size,
    SDL_GpuBufferUsageFlags usageFlags,
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
        /* GPU buffers need transfer bits for defrag, transfer buffers need them for transfers */
        vulkanUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    buffer = SDL_malloc(sizeof(VulkanBuffer));

    buffer->size = size;
    buffer->usageFlags = usageFlags;
    buffer->type = type;
    buffer->markedForDestroy = 0;
    buffer->transitioned = SDL_FALSE;

    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = NULL;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = vulkanUsageFlags;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &renderer->queueFamilyIndex;

    /* Set transfer bits so we can defrag */
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

    buffer->usedRegion->vulkanBuffer = buffer; /* lol */
    buffer->handle = NULL;

    SDL_AtomicSet(&buffer->referenceCount, 0);

    return buffer;
}

/* Indirection so we can cleanly defrag buffers */
static VulkanBufferHandle *VULKAN_INTERNAL_CreateBufferHandle(
    VulkanRenderer *renderer,
    Uint32 sizeInBytes,
    SDL_GpuBufferUsageFlags usageFlags,
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create buffer!");
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
    Uint32 sizeInBytes,
    SDL_GpuBufferUsageFlags usageFlags,
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create buffer container!");
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

/* Texture Slice Utilities */

static Uint32 VULKAN_INTERNAL_GetTextureSliceIndex(
    VulkanTexture *texture,
    Uint32 layer,
    Uint32 level)
{
    return (layer * texture->levelCount) + level;
}

static VulkanTextureSlice *VULKAN_INTERNAL_FetchTextureSlice(
    VulkanTexture *texture,
    Uint32 layer,
    Uint32 level)
{
    return &texture->slices[VULKAN_INTERNAL_GetTextureSliceIndex(
        texture,
        layer,
        level)];
}

static VulkanTextureSlice *VULKAN_INTERNAL_SDLToVulkanTextureSlice(
    SDL_GpuTextureSlice *textureSlice)
{
    return VULKAN_INTERNAL_FetchTextureSlice(
        ((VulkanTextureContainer *)textureSlice->texture)->activeTextureHandle->vulkanTexture,
        textureSlice->layer,
        textureSlice->mipLevel);
}

static void VULKAN_INTERNAL_CreateSliceView(
    VulkanRenderer *renderer,
    VulkanTexture *texture,
    Uint32 layer,
    Uint32 level,
    VkComponentMapping swizzle,
    VkImageView *pView)
{
    VkResult vulkanResult;
    VkImageViewCreateInfo imageViewCreateInfo;

    /* create framebuffer compatible views for RenderTarget */
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
    imageViewCreateInfo.viewType = (texture->depth > 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;

    vulkanResult = renderer->vkCreateImageView(
        renderer->logicalDevice,
        &imageViewCreateInfo,
        NULL,
        pView);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError(
            "vkCreateImageView",
            vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create color attachment image view");
        *pView = (VkImageView)VK_NULL_HANDLE;
        return;
    }
}

/* Swapchain */

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

    /* Initialize these in case anything fails */
    outputDetails->formatsLength = 0;
    outputDetails->presentModesLength = 0;

    if (!supportsPresent) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "This surface does not support presenting!");
        return 0;
    }

    /* Run the device surface queries */
    result = renderer->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice,
        surface,
        &outputDetails->capabilities);
    VULKAN_ERROR_CHECK(result, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, 0)

    if (!(outputDetails->capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Opaque presentation unsupported! Expect weird transparency bugs!");
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

    /* Generate the arrays, if applicable */

    outputDetails->formats = NULL;
    if (outputDetails->formatsLength != 0) {
        outputDetails->formats = (VkSurfaceFormatKHR *)SDL_malloc(
            sizeof(VkSurfaceFormatKHR) * outputDetails->formatsLength);

        if (!outputDetails->formats) {
            SDL_OutOfMemory();
            return 0;
        }

        result = renderer->vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice,
            surface,
            &outputDetails->formatsLength,
            outputDetails->formats);
        if (result != VK_SUCCESS) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
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
            SDL_OutOfMemory();
            return 0;
        }

        result = renderer->vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface,
            &outputDetails->presentModesLength,
            outputDetails->presentModes);
        if (result != VK_SUCCESS) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
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

static SDL_bool VULKAN_INTERNAL_VerifySwapSurfaceFormat(
    VkFormat desiredFormat,
    VkColorSpaceKHR desiredColorSpace,
    VkSurfaceFormatKHR *availableFormats,
    Uint32 availableFormatsLength)
{
    Uint32 i;
    for (i = 0; i < availableFormatsLength; i += 1) {
        if (availableFormats[i].format == desiredFormat &&
            availableFormats[i].colorSpace == desiredColorSpace) {
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

static SDL_bool VULKAN_INTERNAL_VerifySwapPresentMode(
    VkPresentModeKHR presentMode,
    VkPresentModeKHR *availablePresentModes,
    Uint32 availablePresentModesLength)
{
    Uint32 i;

    for (i = 0; i < availablePresentModesLength; i += 1) {
        if (availablePresentModes[i] == presentMode) {
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

static SDL_bool VULKAN_INTERNAL_CreateSwapchain(
    VulkanRenderer *renderer,
    WindowData *windowData)
{
    VkResult vulkanResult;
    VulkanSwapchainData *swapchainData;
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    VkImage *swapchainImages;
    VkImageViewCreateInfo imageViewCreateInfo;
    VkSemaphoreCreateInfo semaphoreCreateInfo;
    SwapchainSupportDetails swapchainSupportDetails;
    Sint32 drawableWidth, drawableHeight;
    Uint32 i;
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    SDL_assert(_this && _this->Vulkan_CreateSurface);

    swapchainData = SDL_malloc(sizeof(VulkanSwapchainData));
    swapchainData->frameCounter = 0;

    /* Each swapchain must have its own surface. */

    if (_this->Vulkan_CreateSurface(
            _this,
            windowData->window,
            renderer->instance,
            NULL, /* FIXME: VAllocationCallbacks */
            &swapchainData->surface) < 0) {
        SDL_free(swapchainData);
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Vulkan_CreateSurface failed: %s",
            SDL_GetError());
        return SDL_FALSE;
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Device does not support swap chain creation");
        return SDL_FALSE;
    }

    if (swapchainSupportDetails.capabilities.currentExtent.width == 0 ||
        swapchainSupportDetails.capabilities.currentExtent.height == 0) {
        /* Not an error, just minimize behavior! */
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
        return SDL_FALSE;
    }

    swapchainData->format = SwapchainCompositionToFormat[windowData->swapchainComposition];
    swapchainData->colorSpace = SwapchainCompositionToColorSpace[windowData->swapchainComposition];
    swapchainData->swapchainSwizzle = SwapchainCompositionSwizzle[windowData->swapchainComposition];

    if (!VULKAN_INTERNAL_VerifySwapSurfaceFormat(
            swapchainData->format,
            swapchainData->colorSpace,
            swapchainSupportDetails.formats,
            swapchainSupportDetails.formatsLength)) {

        /* Try an RGB format instead? */
        VkFormat oldFormat = swapchainData->format;
        if (oldFormat == VK_FORMAT_B8G8R8A8_UNORM) {
            swapchainData->format = VK_FORMAT_R8G8B8A8_UNORM;
        } else if (oldFormat == VK_FORMAT_B8G8R8A8_SRGB) {
            swapchainData->format = VK_FORMAT_R8G8B8A8_SRGB;
        }

        if (oldFormat == swapchainData->format || !VULKAN_INTERNAL_VerifySwapSurfaceFormat(
                                                      swapchainData->format,
                                                      swapchainData->colorSpace,
                                                      swapchainSupportDetails.formats,
                                                      swapchainSupportDetails.formatsLength)) {
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
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Device does not support requested colorspace!");
            return SDL_FALSE;
        }
    }

    if (!VULKAN_INTERNAL_VerifySwapPresentMode(
            SDLToVK_PresentMode[windowData->presentMode],
            swapchainSupportDetails.presentModes,
            swapchainSupportDetails.presentModesLength)) {
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Device does not support requested present mode!");
        return SDL_FALSE;
    }

    swapchainData->presentMode = SDLToVK_PresentMode[windowData->presentMode];

    /* Sync now to be sure that our swapchain size is correct */
    SDL_SyncWindow(windowData->window);
    SDL_GetWindowSizeInPixels(
        windowData->window,
        &drawableWidth,
        &drawableHeight);

    if (drawableWidth < swapchainSupportDetails.capabilities.minImageExtent.width ||
        drawableWidth > swapchainSupportDetails.capabilities.maxImageExtent.width ||
        drawableHeight < swapchainSupportDetails.capabilities.minImageExtent.height ||
        drawableHeight > swapchainSupportDetails.capabilities.maxImageExtent.height) {
        if (swapchainSupportDetails.capabilities.currentExtent.width != UINT32_MAX) {
            drawableWidth = VULKAN_INTERNAL_clamp(
                drawableWidth,
                swapchainSupportDetails.capabilities.minImageExtent.width,
                swapchainSupportDetails.capabilities.maxImageExtent.width);
            drawableHeight = VULKAN_INTERNAL_clamp(
                drawableHeight,
                swapchainSupportDetails.capabilities.minImageExtent.height,
                swapchainSupportDetails.capabilities.maxImageExtent.height);
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
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No fallback swapchain size available!");
            return SDL_FALSE;
        }
    }

    swapchainData->extent.width = drawableWidth;
    swapchainData->extent.height = drawableHeight;

    swapchainData->imageCount = swapchainSupportDetails.capabilities.minImageCount + 1;

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
    swapchainCreateInfo.imageExtent = swapchainData->extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
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
        return SDL_FALSE;
    }

    renderer->vkGetSwapchainImagesKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        &swapchainData->imageCount,
        NULL);

    swapchainData->textureContainers = SDL_malloc(
        sizeof(VulkanTextureContainer) * swapchainData->imageCount);

    if (!swapchainData->textureContainers) {
        SDL_OutOfMemory();
        renderer->vkDestroySurfaceKHR(
            renderer->instance,
            swapchainData->surface,
            NULL);
        SDL_free(swapchainData);
        return SDL_FALSE;
    }

    swapchainImages = SDL_stack_alloc(VkImage, swapchainData->imageCount);

    renderer->vkGetSwapchainImagesKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        &swapchainData->imageCount,
        swapchainImages);

    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = swapchainData->format;
    imageViewCreateInfo.components = swapchainData->swapchainSwizzle;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    for (i = 0; i < swapchainData->imageCount; i += 1) {
        swapchainData->textureContainers[i].canBeCycled = 0;
        swapchainData->textureContainers[i].textureCapacity = 0;
        swapchainData->textureContainers[i].textureCount = 0;
        swapchainData->textureContainers[i].textureHandles = NULL;
        swapchainData->textureContainers[i].debugName = NULL;
        swapchainData->textureContainers[i].activeTextureHandle = SDL_malloc(sizeof(VulkanTextureHandle));

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture = SDL_malloc(sizeof(VulkanTexture));

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->image = swapchainImages[i];

        imageViewCreateInfo.image = swapchainImages[i];

        vulkanResult = renderer->vkCreateImageView(
            renderer->logicalDevice,
            &imageViewCreateInfo,
            NULL,
            &swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->view);

        if (vulkanResult != VK_SUCCESS) {
            renderer->vkDestroySurfaceKHR(
                renderer->instance,
                swapchainData->surface,
                NULL);
            SDL_stack_free(swapchainImages);
            SDL_free(swapchainData->textureContainers);
            SDL_free(swapchainData);
            LogVulkanResultAsError("vkCreateImageView", vulkanResult);
            return SDL_FALSE;
        }

        /* Swapchain memory is managed by the driver */
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->usedRegion = NULL;

        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->dimensions = swapchainData->extent;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->format = swapchainData->format;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->swizzle = swapchainData->swapchainSwizzle;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->is3D = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->isCube = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->isRenderTarget = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->depth = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->layerCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->levelCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->sampleCount = VK_SAMPLE_COUNT_1_BIT;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->usageFlags =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        swapchainData->textureContainers[i].activeTextureHandle->container = NULL;

        /* Create slice */
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->sliceCount = 1;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices = SDL_malloc(sizeof(VulkanTextureSlice));
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].parent = swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].layer = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].level = 0;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].transitioned = SDL_TRUE;
        swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].msaaTexHandle = NULL;

        VULKAN_INTERNAL_CreateSliceView(
            renderer,
            swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture,
            0,
            0,
            swapchainData->swapchainSwizzle,
            &swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].view);
        SDL_AtomicSet(&swapchainData->textureContainers[i].activeTextureHandle->vulkanTexture->slices[0].referenceCount, 0);
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
    return SDL_TRUE;
}

/* Command Buffers */

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
    SDL_GpuDevice *device)
{
    VulkanRenderer *renderer = (VulkanRenderer *)device->driverData;
    CommandPoolHashArray commandPoolHashArray;
    VulkanMemorySubAllocator *allocator;
    Sint32 i, j, k;

    VULKAN_Wait(device->driverData);

    for (i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        VULKAN_UnclaimWindow(device->driverData, renderer->claimedWindows[i]->window);
    }

    SDL_free(renderer->claimedWindows);

    VULKAN_Wait(device->driverData);

    SDL_free(renderer->submittedCommandBuffers);

    for (i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        VULKAN_INTERNAL_DestroyBuffer(
            renderer,
            renderer->uniformBufferPool[i]->bufferHandle->vulkanBuffer);
        SDL_free(renderer->uniformBufferPool[i]->bufferHandle);
        SDL_free(renderer->uniformBufferPool[i]);
    }
    SDL_free(renderer->uniformBufferPool);

    for (i = 0; i < renderer->fencePool.availableFenceCount; i += 1) {
        renderer->vkDestroyFence(
            renderer->logicalDevice,
            renderer->fencePool.availableFences[i]->fence,
            NULL);

        SDL_free(renderer->fencePool.availableFences[i]);
    }

    SDL_free(renderer->fencePool.availableFences);
    SDL_DestroyMutex(renderer->fencePool.lock);

    for (i = 0; i < NUM_COMMAND_POOL_BUCKETS; i += 1) {
        commandPoolHashArray = renderer->commandPoolHashTable.buckets[i];
        for (j = 0; j < commandPoolHashArray.count; j += 1) {
            VULKAN_INTERNAL_DestroyCommandPool(
                renderer,
                commandPoolHashArray.elements[j].value);
        }

        if (commandPoolHashArray.elements != NULL) {
            SDL_free(commandPoolHashArray.elements);
        }
    }

    for (i = 0; i < renderer->framebufferHashArray.count; i += 1) {
        VULKAN_INTERNAL_DestroyFramebuffer(
            renderer,
            renderer->framebufferHashArray.elements[i].value);
    }

    SDL_free(renderer->framebufferHashArray.elements);

    for (i = 0; i < renderer->renderPassHashArray.count; i += 1) {
        renderer->vkDestroyRenderPass(
            renderer->logicalDevice,
            renderer->renderPassHashArray.elements[i].value,
            NULL);
    }

    SDL_free(renderer->renderPassHashArray.elements);

    for (i = 0; i < VK_MAX_MEMORY_TYPES; i += 1) {
        allocator = &renderer->memoryAllocator->subAllocators[i];

        for (j = allocator->allocationCount - 1; j >= 0; j -= 1) {
            for (k = allocator->allocations[j]->usedRegionCount - 1; k >= 0; k -= 1) {
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

    /* If no inactive descriptor sets remain, create a new pool and allocate new inactive sets */

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
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor pool!");
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
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor sets!");
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
            imageInfos[imageInfoCount].imageView = commandBuffer->vertexSamplerTextures[i]->view;
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
            imageInfos[imageInfoCount].imageView = commandBuffer->vertexStorageTextureSlices[i]->view;
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

        commandBuffer->needNewVertexResourceDescriptorSet = SDL_FALSE;
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

            VULKAN_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->vertexUniformBuffers[i]);
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

        commandBuffer->needNewVertexUniformDescriptorSet = SDL_FALSE;
        commandBuffer->needNewVertexUniformOffsets = SDL_TRUE;
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

        commandBuffer->needNewVertexUniformOffsets = SDL_FALSE;
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
            imageInfos[imageInfoCount].imageView = commandBuffer->fragmentSamplerTextures[i]->view;
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
            imageInfos[imageInfoCount].imageView = commandBuffer->fragmentStorageTextureSlices[i]->view;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->fragmentStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->fragmentSamplerCount + resourceLayout->fragmentStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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

        commandBuffer->needNewFragmentResourceDescriptorSet = SDL_FALSE;
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

            VULKAN_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->fragmentUniformBuffers[i]);
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

        commandBuffer->needNewFragmentUniformDescriptorSet = SDL_FALSE;
        commandBuffer->needNewFragmentUniformOffsets = SDL_TRUE;
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

        commandBuffer->needNewFragmentUniformOffsets = SDL_FALSE;
    }
}

static void VULKAN_DrawIndexedPrimitives(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 baseVertex,
    Uint32 startIndex,
    Uint32 primitiveCount,
    Uint32 instanceCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDrawIndexed(
        vulkanCommandBuffer->commandBuffer,
        PrimitiveVerts(
            vulkanCommandBuffer->currentGraphicsPipeline->primitiveType,
            primitiveCount),
        instanceCount,
        startIndex,
        baseVertex,
        0);
}

static void VULKAN_DrawPrimitives(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 vertexStart,
    Uint32 primitiveCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDraw(
        vulkanCommandBuffer->commandBuffer,
        PrimitiveVerts(
            vulkanCommandBuffer->currentGraphicsPipeline->primitiveType,
            primitiveCount),
        1,
        vertexStart,
        0);
}

static void VULKAN_DrawPrimitivesIndirect(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)buffer)->activeBufferHandle->vulkanBuffer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDrawIndirect(
        vulkanCommandBuffer->commandBuffer,
        vulkanBuffer->buffer,
        offsetInBytes,
        drawCount,
        stride);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

static void VULKAN_DrawIndexedPrimitivesIndirect(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBuffer *vulkanBuffer = ((VulkanBufferContainer *)buffer)->activeBufferHandle->vulkanBuffer;

    VULKAN_INTERNAL_BindGraphicsDescriptorSets(renderer, vulkanCommandBuffer);

    renderer->vkCmdDrawIndexedIndirect(
        vulkanCommandBuffer->commandBuffer,
        vulkanBuffer->buffer,
        offsetInBytes,
        drawCount,
        stride);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, vulkanBuffer);
}

/* Debug Naming */

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
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *buffer,
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
    SDL_GpuRenderer *driverData,
    SDL_GpuTexture *texture,
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
    SDL_GpuCommandBuffer *commandBuffer,
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
    SDL_GpuCommandBuffer *commandBuffer,
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
    SDL_GpuCommandBuffer *commandBuffer)
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
    Uint32 isCube,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GpuTextureUsageFlags textureUsageFlags,
    SDL_bool isMSAAColorTarget)
{
    VulkanTextureHandle *textureHandle;
    VulkanTexture *texture;

    texture = VULKAN_INTERNAL_CreateTexture(
        renderer,
        width,
        height,
        depth,
        isCube,
        layerCount,
        levelCount,
        sampleCount,
        format,
        swizzle,
        aspectMask,
        textureUsageFlags,
        isMSAAColorTarget);

    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture!");
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
    Uint32 isCube,
    Uint32 layerCount,
    Uint32 levelCount,
    VkSampleCountFlagBits sampleCount,
    VkFormat format,
    VkComponentMapping swizzle,
    VkImageAspectFlags aspectMask,
    SDL_GpuTextureUsageFlags textureUsageFlags,
    SDL_bool isMSAAColorTarget)
{
    VkResult vulkanResult;
    VkImageCreateInfo imageCreateInfo;
    VkImageCreateFlags imageCreateFlags = 0;
    VkImageViewCreateInfo imageViewCreateInfo;
    Uint8 bindResult;
    Uint8 is3D = depth > 1 ? 1 : 0;
    Uint8 isRenderTarget =
        ((textureUsageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) != 0) ||
        ((textureUsageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) != 0);
    Uint32 i, j, sliceIndex;
    VkImageUsageFlags vkUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VulkanTexture *texture = SDL_malloc(sizeof(VulkanTexture));

    texture->isCube = 0;
    texture->is3D = 0;
    texture->isRenderTarget = isRenderTarget;
    texture->isMSAAColorTarget = isMSAAColorTarget;
    texture->markedForDestroy = 0;

    if (isCube) {
        imageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        texture->isCube = 1;
    } else if (is3D) {
        imageCreateFlags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        texture->is3D = 1;
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
    imageCreateInfo.imageType = is3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
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

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to bind memory for texture!");
        return NULL;
    }

    texture->usedRegion->vulkanTexture = texture; /* lol */

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

    if (isCube) {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (is3D) {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    } else if (layerCount > 1) {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else {
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    vulkanResult = renderer->vkCreateImageView(
        renderer->logicalDevice,
        &imageViewCreateInfo,
        NULL,
        &texture->view);

    if (vulkanResult != VK_SUCCESS) {
        LogVulkanResultAsError("vkCreateImageView", vulkanResult);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture image view");
        return NULL;
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

    /* Define slices */
    texture->sliceCount =
        texture->layerCount *
        texture->levelCount;

    texture->slices = SDL_malloc(
        texture->sliceCount * sizeof(VulkanTextureSlice));

    for (i = 0; i < texture->layerCount; i += 1) {
        for (j = 0; j < texture->levelCount; j += 1) {
            sliceIndex = VULKAN_INTERNAL_GetTextureSliceIndex(
                texture,
                i,
                j);

            VULKAN_INTERNAL_CreateSliceView(
                renderer,
                texture,
                i,
                j,
                swizzle,
                &texture->slices[sliceIndex].view);

            texture->slices[sliceIndex].parent = texture;
            texture->slices[sliceIndex].layer = i;
            texture->slices[sliceIndex].level = j;
            texture->slices[sliceIndex].msaaTexHandle = NULL;
            texture->slices[sliceIndex].transitioned = SDL_FALSE;
            SDL_AtomicSet(&texture->slices[sliceIndex].referenceCount, 0);

            if (
                sampleCount > VK_SAMPLE_COUNT_1_BIT &&
                isRenderTarget &&
                !isMSAAColorTarget &&
                !VULKAN_INTERNAL_IsVulkanDepthFormat(texture->format)) {
                texture->slices[sliceIndex].msaaTexHandle = VULKAN_INTERNAL_CreateTextureHandle(
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
                    SDL_TRUE);
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

    /* If a previously-cycled buffer is available, we can use that. */
    for (i = 0; i < bufferContainer->bufferCount; i += 1) {
        bufferHandle = bufferContainer->bufferHandles[i];
        if (SDL_AtomicGet(&bufferHandle->vulkanBuffer->referenceCount) == 0) {
            bufferContainer->activeBufferHandle = bufferHandle;
            return;
        }
    }

    /* No buffer handle is available, generate a new one. */
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
    VulkanTextureHandle *textureHandle;
    Uint32 i, j;
    Sint32 refCountTotal;

    /* If a previously-cycled texture is available, we can use that. */
    for (i = 0; i < textureContainer->textureCount; i += 1) {
        textureHandle = textureContainer->textureHandles[i];

        refCountTotal = 0;
        for (j = 0; j < textureHandle->vulkanTexture->sliceCount; j += 1) {
            refCountTotal += SDL_AtomicGet(&textureHandle->vulkanTexture->slices[j].referenceCount);
        }

        if (refCountTotal == 0) {
            textureContainer->activeTextureHandle = textureHandle;
            return;
        }
    }

    /* No texture handle is available, generate a new one. */
    textureContainer->activeTextureHandle = VULKAN_INTERNAL_CreateTextureHandle(
        renderer,
        textureContainer->activeTextureHandle->vulkanTexture->dimensions.width,
        textureContainer->activeTextureHandle->vulkanTexture->dimensions.height,
        textureContainer->activeTextureHandle->vulkanTexture->depth,
        textureContainer->activeTextureHandle->vulkanTexture->isCube,
        textureContainer->activeTextureHandle->vulkanTexture->layerCount,
        textureContainer->activeTextureHandle->vulkanTexture->levelCount,
        textureContainer->activeTextureHandle->vulkanTexture->sampleCount,
        textureContainer->activeTextureHandle->vulkanTexture->format,
        textureContainer->activeTextureHandle->vulkanTexture->swizzle,
        textureContainer->activeTextureHandle->vulkanTexture->aspectFlags,
        textureContainer->activeTextureHandle->vulkanTexture->usageFlags,
        SDL_FALSE);

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
    SDL_bool cycle,
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

static VulkanTextureSlice *VULKAN_INTERNAL_PrepareTextureSliceForWrite(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    VulkanTextureContainer *textureContainer,
    Uint32 layer,
    Uint32 level,
    SDL_bool cycle,
    VulkanTextureUsageMode destinationUsageMode)
{
    VulkanTextureSlice *textureSlice = VULKAN_INTERNAL_FetchTextureSlice(
        textureContainer->activeTextureHandle->vulkanTexture,
        layer,
        level);

    if (
        cycle &&
        textureContainer->canBeCycled &&
        SDL_AtomicGet(&textureSlice->referenceCount) > 0) {
        VULKAN_INTERNAL_CycleActiveTexture(
            renderer,
            textureContainer);

        textureSlice = VULKAN_INTERNAL_FetchTextureSlice(
            textureContainer->activeTextureHandle->vulkanTexture,
            layer,
            level);
    }

    /* always do barrier because of layout transitions */
    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
        renderer,
        commandBuffer,
        destinationUsageMode,
        textureSlice);

    return textureSlice;
}

static VkRenderPass VULKAN_INTERNAL_CreateRenderPass(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
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
        texture = ((VulkanTextureContainer *)colorAttachmentInfos[i].textureSlice.texture)->activeTextureHandle->vulkanTexture;

        if (texture->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
            /* Resolve attachment and multisample attachment */

            attachmentDescriptions[attachmentDescriptionCount].flags = 0;
            attachmentDescriptions[attachmentDescriptionCount].format = texture->format;
            attachmentDescriptions[attachmentDescriptionCount].samples =
                VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[attachmentDescriptionCount].loadOp = SDLToVK_LoadOp[colorAttachmentInfos[i].loadOp];
            attachmentDescriptions[attachmentDescriptionCount].storeOp =
                VK_ATTACHMENT_STORE_OP_STORE; /* Always store the resolve texture */
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
                VK_ATTACHMENT_STORE_OP_STORE; /* Always store non-MSAA textures */
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
        texture = ((VulkanTextureContainer *)depthStencilAttachmentInfo->textureSlice.texture)->activeTextureHandle->vulkanTexture;

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
    SDL_GpuGraphicsPipelineAttachmentInfo attachmentInfo,
    VkSampleCountFlagBits sampleCount)
{
    VkAttachmentDescription attachmentDescriptions[2 * MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference colorAttachmentReferences[MAX_COLOR_TARGET_BINDINGS];
    VkAttachmentReference resolveReferences[MAX_COLOR_TARGET_BINDINGS + 1];
    VkAttachmentReference depthStencilAttachmentReference;
    SDL_GpuColorAttachmentDescription attachmentDescription;
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

            /* Resolve attachment and multisample attachment */

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

static SDL_GpuGraphicsPipeline *VULKAN_CreateGraphicsPipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo)
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

    /* Find a compatible sample count to use */

    actualSampleCount = VULKAN_INTERNAL_GetMaxMultiSampleCount(
        renderer,
        SDLToVK_SampleCount[pipelineCreateInfo->multisampleState.multisampleCount]);

    /* Create a "compatible" render pass */

    VkRenderPass transientRenderPass = VULKAN_INTERNAL_CreateTransientRenderPass(
        renderer,
        pipelineCreateInfo->attachmentInfo,
        actualSampleCount);

    /* Dynamic state */

    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext = NULL;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = SDL_arraysize(dynamicStates);
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    /* Shader stages */

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

    /* Vertex input */

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
                divisorDescriptions[divisorDescriptionCount].divisor = pipelineCreateInfo->vertexInputState.vertexBindings[i].stepRate;

                divisorDescriptionCount += 1;
            }
        }

        divisorStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
        divisorStateCreateInfo.pNext = NULL;
        divisorStateCreateInfo.vertexBindingDivisorCount = divisorDescriptionCount;
        divisorStateCreateInfo.pVertexBindingDivisors = divisorDescriptions;

        vertexInputStateCreateInfo.pNext = &divisorStateCreateInfo;
    }

    /* Topology */

    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = NULL;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyStateCreateInfo.topology = SDLToVK_PrimitiveType[pipelineCreateInfo->primitiveType];

    graphicsPipeline->primitiveType = pipelineCreateInfo->primitiveType;

    /* Viewport */

    /* NOTE: viewport and scissor are dynamic, and must be set using the command buffer */

    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = NULL;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = NULL;

    /* Rasterization */

    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = SDLToVK_PolygonMode[pipelineCreateInfo->rasterizerState.fillMode];
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

    /* Multisample */

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

    /* Depth Stencil State */

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
    depthStencilStateCreateInfo.minDepthBounds = 0; /* unused */
    depthStencilStateCreateInfo.maxDepthBounds = 0; /* unused */

    /* Color Blend */

    for (i = 0; i < pipelineCreateInfo->attachmentInfo.colorAttachmentCount; i += 1) {
        SDL_GpuColorAttachmentBlendState blendState = pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].blendState;

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

    /* We don't support LogicOp, so this is easy. */
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = 0;

    /* Pipeline Layout */

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize pipeline resource layout!");
        return NULL;
    }

    /* Pipeline */

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

    /* TODO: enable pipeline caching */
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create graphics pipeline!");
        return NULL;
    }

    SDL_AtomicSet(&graphicsPipeline->referenceCount, 0);

    return (SDL_GpuGraphicsPipeline *)graphicsPipeline;
}

static SDL_GpuComputePipeline *VULKAN_CreateComputePipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuComputePipelineCreateInfo *pipelineCreateInfo)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    VkComputePipelineCreateInfo computePipelineCreateInfo;
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo;
    VkResult vulkanResult;
    Uint32 i;
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanComputePipeline *vulkanComputePipeline;

    if (pipelineCreateInfo->format != SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Incompatible shader format for Vulkan!");
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create compute pipeline!");
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create compute pipeline!");
        return NULL;
    }

    SDL_AtomicSet(&vulkanComputePipeline->referenceCount, 0);

    return (SDL_GpuComputePipeline *)vulkanComputePipeline;
}

static SDL_GpuSampler *VULKAN_CreateSampler(
    SDL_GpuRenderer *driverData,
    SDL_GpuSamplerCreateInfo *samplerCreateInfo)
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
    vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; /* arbitrary, unused */
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

    return (SDL_GpuSampler *)vulkanSampler;
}

static SDL_GpuShader *VULKAN_CreateShader(
    SDL_GpuRenderer *driverData,
    SDL_GpuShaderCreateInfo *shaderCreateInfo)
{
    VulkanShader *vulkanShader;
    VkResult vulkanResult;
    VkShaderModuleCreateInfo vkShaderModuleCreateInfo;
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    size_t entryPointNameLength;

    if (shaderCreateInfo->format != SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Incompatible shader format for Vulkan");
        return NULL;
    }

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader module!");
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

    return (SDL_GpuShader *)vulkanShader;
}

static SDL_GpuSampleCount VULKAN_GetBestSampleCount(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureFormat format,
    SDL_GpuSampleCount desiredSampleCount)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    Uint32 maxSupported;
    VkSampleCountFlagBits bits = IsDepthFormat(format) ? renderer->physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts : renderer->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts;

    if (bits & VK_SAMPLE_COUNT_8_BIT) {
        maxSupported = SDL_GPU_SAMPLECOUNT_8;
    } else if (bits & VK_SAMPLE_COUNT_4_BIT) {
        maxSupported = SDL_GPU_SAMPLECOUNT_4;
    } else if (bits & VK_SAMPLE_COUNT_2_BIT) {
        maxSupported = SDL_GPU_SAMPLECOUNT_2;
    } else {
        maxSupported = SDL_GPU_SAMPLECOUNT_1;
    }

    return (SDL_GpuSampleCount)SDL_min(maxSupported, desiredSampleCount);
}

static SDL_GpuTexture *VULKAN_CreateTexture(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureCreateInfo *textureCreateInfo)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkImageAspectFlags imageAspectFlags;
    Uint8 isDepthFormat = IsDepthFormat(textureCreateInfo->format);
    VkFormat format;
    VkComponentMapping swizzle;
    VulkanTextureContainer *container;
    VulkanTextureHandle *textureHandle;
    SDL_GpuSampleCount actualSampleCount = VULKAN_GetBestSampleCount(
        driverData,
        textureCreateInfo->format,
        textureCreateInfo->sampleCount);

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
        textureCreateInfo->depth,
        textureCreateInfo->isCube,
        textureCreateInfo->layerCount,
        textureCreateInfo->levelCount,
        SDLToVK_SampleCount[actualSampleCount],
        format,
        swizzle,
        imageAspectFlags,
        textureCreateInfo->usageFlags,
        SDL_FALSE);

    if (textureHandle == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture container!");
        return NULL;
    }

    container = SDL_malloc(sizeof(VulkanTextureContainer));
    container->canBeCycled = 1;
    container->activeTextureHandle = textureHandle;
    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textureHandles = SDL_malloc(
        container->textureCapacity * sizeof(VulkanTextureHandle *));
    container->textureHandles[0] = container->activeTextureHandle;
    container->debugName = NULL;

    textureHandle->container = container;

    return (SDL_GpuTexture *)container;
}

static SDL_GpuBuffer *VULKAN_CreateBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuBufferUsageFlags usageFlags,
    Uint32 sizeInBytes)
{
    return (SDL_GpuBuffer *)VULKAN_INTERNAL_CreateBufferContainer(
        (VulkanRenderer *)driverData,
        sizeInBytes,
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
        sizeInBytes,
        0,
        VULKAN_BUFFER_TYPE_UNIFORM);

    uniformBuffer->drawOffset = 0;
    uniformBuffer->writeOffset = 0;

    return uniformBuffer;
}

static SDL_GpuTransferBuffer *VULKAN_CreateTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBufferUsage usage, /* ignored on Vulkan */
    Uint32 sizeInBytes)
{
    return (SDL_GpuTransferBuffer *)VULKAN_INTERNAL_CreateBufferContainer(
        (VulkanRenderer *)driverData,
        sizeInBytes,
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
    SDL_GpuRenderer *driverData,
    SDL_GpuTexture *texture)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanTextureContainer *vulkanTextureContainer = (VulkanTextureContainer *)texture;
    Uint32 i;

    SDL_LockMutex(renderer->disposeLock);

    for (i = 0; i < vulkanTextureContainer->textureCount; i += 1) {
        VULKAN_INTERNAL_ReleaseTexture(renderer, vulkanTextureContainer->textureHandles[i]->vulkanTexture);
        SDL_free(vulkanTextureContainer->textureHandles[i]);
    }

    /* Containers are just client handles, so we can destroy immediately */
    if (vulkanTextureContainer->debugName != NULL) {
        SDL_free(vulkanTextureContainer->debugName);
    }
    SDL_free(vulkanTextureContainer->textureHandles);
    SDL_free(vulkanTextureContainer);

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseSampler(
    SDL_GpuRenderer *driverData,
    SDL_GpuSampler *sampler)
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

    /* Containers are just client handles, so we can free immediately */
    if (bufferContainer->debugName != NULL) {
        SDL_free(bufferContainer->debugName);
    }
    SDL_free(bufferContainer->bufferHandles);
    SDL_free(bufferContainer);

    SDL_UnlockMutex(renderer->disposeLock);
}

static void VULKAN_ReleaseBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *buffer)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *vulkanBufferContainer = (VulkanBufferContainer *)buffer;

    VULKAN_INTERNAL_ReleaseBufferContainer(
        renderer,
        vulkanBufferContainer);
}

static void VULKAN_ReleaseTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)transferBuffer;

    VULKAN_INTERNAL_ReleaseBufferContainer(
        renderer,
        transferBufferContainer);
}

static void VULKAN_ReleaseShader(
    SDL_GpuRenderer *driverData,
    SDL_GpuShader *shader)
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
    SDL_GpuRenderer *driverData,
    SDL_GpuComputePipeline *computePipeline)
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
    SDL_GpuRenderer *driverData,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
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

/* Command Buffer render state */

static VkRenderPass VULKAN_INTERNAL_FetchRenderPass(
    VulkanRenderer *renderer,
    VulkanCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    VkRenderPass renderPass;
    RenderPassHash hash;
    Uint32 i;

    SDL_LockMutex(renderer->renderPassFetchLock);

    for (i = 0; i < colorAttachmentCount; i += 1) {
        hash.colorTargetDescriptions[i].format = ((VulkanTextureContainer *)colorAttachmentInfos[i].textureSlice.texture)->activeTextureHandle->vulkanTexture->format;
        hash.colorTargetDescriptions[i].clearColor = colorAttachmentInfos[i].clearColor;
        hash.colorTargetDescriptions[i].loadOp = colorAttachmentInfos[i].loadOp;
        hash.colorTargetDescriptions[i].storeOp = colorAttachmentInfos[i].storeOp;
    }

    hash.colorAttachmentSampleCount = VK_SAMPLE_COUNT_1_BIT;
    if (colorAttachmentCount > 0) {
        hash.colorAttachmentSampleCount = ((VulkanTextureContainer *)colorAttachmentInfos[0].textureSlice.texture)->activeTextureHandle->vulkanTexture->sampleCount;
    }

    hash.colorAttachmentCount = colorAttachmentCount;

    if (depthStencilAttachmentInfo == NULL) {
        hash.depthStencilTargetDescription.format = 0;
        hash.depthStencilTargetDescription.loadOp = SDL_GPU_LOADOP_DONT_CARE;
        hash.depthStencilTargetDescription.storeOp = SDL_GPU_STOREOP_DONT_CARE;
        hash.depthStencilTargetDescription.stencilLoadOp = SDL_GPU_LOADOP_DONT_CARE;
        hash.depthStencilTargetDescription.stencilStoreOp = SDL_GPU_STOREOP_DONT_CARE;
    } else {
        hash.depthStencilTargetDescription.format = ((VulkanTextureContainer *)depthStencilAttachmentInfo->textureSlice.texture)->activeTextureHandle->vulkanTexture->format;
        hash.depthStencilTargetDescription.loadOp = depthStencilAttachmentInfo->loadOp;
        hash.depthStencilTargetDescription.storeOp = depthStencilAttachmentInfo->storeOp;
        hash.depthStencilTargetDescription.stencilLoadOp = depthStencilAttachmentInfo->stencilLoadOp;
        hash.depthStencilTargetDescription.stencilStoreOp = depthStencilAttachmentInfo->stencilStoreOp;
    }

    renderPass = RenderPassHashArray_Fetch(
        &renderer->renderPassHashArray,
        &hash);

    if (renderPass != VK_NULL_HANDLE) {
        SDL_UnlockMutex(renderer->renderPassFetchLock);
        return renderPass;
    }

    renderPass = VULKAN_INTERNAL_CreateRenderPass(
        renderer,
        commandBuffer,
        colorAttachmentInfos,
        colorAttachmentCount,
        depthStencilAttachmentInfo);

    if (renderPass != VK_NULL_HANDLE) {
        RenderPassHashArray_Insert(
            &renderer->renderPassHashArray,
            hash,
            renderPass);
    }

    SDL_UnlockMutex(renderer->renderPassFetchLock);
    return renderPass;
}

static VulkanFramebuffer *VULKAN_INTERNAL_FetchFramebuffer(
    VulkanRenderer *renderer,
    VkRenderPass renderPass,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo,
    Uint32 width,
    Uint32 height)
{
    VulkanFramebuffer *vulkanFramebuffer;
    VkFramebufferCreateInfo framebufferInfo;
    VkResult result;
    VkImageView imageViewAttachments[2 * MAX_COLOR_TARGET_BINDINGS + 1];
    FramebufferHash hash;
    VulkanTextureSlice *textureSlice;
    Uint32 attachmentCount = 0;
    Uint32 i;

    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        hash.colorAttachmentViews[i] = VK_NULL_HANDLE;
        hash.colorMultiSampleAttachmentViews[i] = VK_NULL_HANDLE;
    }

    hash.colorAttachmentCount = colorAttachmentCount;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        textureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&colorAttachmentInfos[i].textureSlice);

        hash.colorAttachmentViews[i] = textureSlice->view;

        if (textureSlice->msaaTexHandle != NULL) {
            hash.colorMultiSampleAttachmentViews[i] = textureSlice->msaaTexHandle->vulkanTexture->view;
        }
    }

    if (depthStencilAttachmentInfo == NULL) {
        hash.depthStencilAttachmentView = VK_NULL_HANDLE;
    } else {
        textureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&depthStencilAttachmentInfo->textureSlice);
        hash.depthStencilAttachmentView = textureSlice->view;
    }

    hash.width = width;
    hash.height = height;

    SDL_LockMutex(renderer->framebufferFetchLock);

    vulkanFramebuffer = FramebufferHashArray_Fetch(
        &renderer->framebufferHashArray,
        &hash);

    SDL_UnlockMutex(renderer->framebufferFetchLock);

    if (vulkanFramebuffer != NULL) {
        return vulkanFramebuffer;
    }

    vulkanFramebuffer = SDL_malloc(sizeof(VulkanFramebuffer));

    SDL_AtomicSet(&vulkanFramebuffer->referenceCount, 0);

    /* Create a new framebuffer */

    for (i = 0; i < colorAttachmentCount; i += 1) {
        textureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&colorAttachmentInfos[i].textureSlice);

        imageViewAttachments[attachmentCount] =
            textureSlice->view;

        attachmentCount += 1;

        if (textureSlice->msaaTexHandle != NULL) {
            imageViewAttachments[attachmentCount] =
                textureSlice->msaaTexHandle->vulkanTexture->view;

            attachmentCount += 1;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        textureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&depthStencilAttachmentInfo->textureSlice);

        imageViewAttachments[attachmentCount] =
            textureSlice->view;

        attachmentCount += 1;
    }

    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = NULL;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = attachmentCount;
    framebufferInfo.pAttachments = imageViewAttachments;
    framebufferInfo.width = hash.width;
    framebufferInfo.height = hash.height;
    framebufferInfo.layers = 1;

    result = renderer->vkCreateFramebuffer(
        renderer->logicalDevice,
        &framebufferInfo,
        NULL,
        &vulkanFramebuffer->framebuffer);

    if (result == VK_SUCCESS) {
        SDL_LockMutex(renderer->framebufferFetchLock);

        FramebufferHashArray_Insert(
            &renderer->framebufferHashArray,
            hash,
            vulkanFramebuffer);

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
    SDL_GpuViewport *viewport)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;

    vulkanCommandBuffer->currentViewport.x = viewport->x;
    vulkanCommandBuffer->currentViewport.width = viewport->w;
    vulkanCommandBuffer->currentViewport.minDepth = viewport->minDepth;
    vulkanCommandBuffer->currentViewport.maxDepth = viewport->maxDepth;

    /* Viewport flip for consistency with other backends */
    /* FIXME: need moltenVK hack */
    vulkanCommandBuffer->currentViewport.y = viewport->y + viewport->h;
    vulkanCommandBuffer->currentViewport.height = -viewport->h;
}

static void VULKAN_SetViewport(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuViewport *viewport)
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
    SDL_GpuRect *scissor)
{
    vulkanCommandBuffer->currentScissor.offset.x = scissor->x;
    vulkanCommandBuffer->currentScissor.offset.y = scissor->y;
    vulkanCommandBuffer->currentScissor.extent.width = scissor->w;
    vulkanCommandBuffer->currentScissor.extent.height = scissor->h;
}

static void VULKAN_SetScissor(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuRect *scissor)
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
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanTextureContainer *textureContainer;
    Uint32 i, j;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)textureSamplerBindings[i].texture;
        vulkanCommandBuffer->vertexSamplerTextures[firstSlot + i] = textureContainer->activeTextureHandle->vulkanTexture;
        vulkanCommandBuffer->vertexSamplers[firstSlot + i] = (VulkanSampler *)textureSamplerBindings[i].sampler;

        VULKAN_INTERNAL_TrackSampler(
            vulkanCommandBuffer,
            (VulkanSampler *)textureSamplerBindings[i].sampler);

        for (j = 0; j < textureContainer->activeTextureHandle->vulkanTexture->sliceCount; j += 1) {
            VULKAN_INTERNAL_TrackTextureSlice(
                vulkanCommandBuffer,
                &textureContainer->activeTextureHandle->vulkanTexture->slices[j]);
        }
    }

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindVertexStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanTextureContainer *textureContainer;
    VulkanTextureSlice *textureSlice;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)storageTextureSlices[i].texture;
        textureSlice = VULKAN_INTERNAL_FetchTextureSlice(
            textureContainer->activeTextureHandle->vulkanTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        vulkanCommandBuffer->vertexStorageTextureSlices[firstSlot + i] = textureSlice;

        VULKAN_INTERNAL_TrackTextureSlice(
            vulkanCommandBuffer,
            textureSlice);
    }

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindVertexStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
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

    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindFragmentSamplers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanTextureContainer *textureContainer;
    Uint32 i, j;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)textureSamplerBindings[i].texture;
        vulkanCommandBuffer->fragmentSamplerTextures[firstSlot + i] = textureContainer->activeTextureHandle->vulkanTexture;
        vulkanCommandBuffer->fragmentSamplers[firstSlot + i] = (VulkanSampler *)textureSamplerBindings[i].sampler;

        VULKAN_INTERNAL_TrackSampler(
            vulkanCommandBuffer,
            (VulkanSampler *)textureSamplerBindings[i].sampler);

        for (j = 0; j < textureContainer->activeTextureHandle->vulkanTexture->sliceCount; j += 1) {
            VULKAN_INTERNAL_TrackTextureSlice(
                vulkanCommandBuffer,
                &textureContainer->activeTextureHandle->vulkanTexture->slices[j]);
        }
    }

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindFragmentStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanTextureContainer *textureContainer;
    VulkanTextureSlice *textureSlice;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)storageTextureSlices[i].texture;
        textureSlice = VULKAN_INTERNAL_FetchTextureSlice(
            textureContainer->activeTextureHandle->vulkanTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        vulkanCommandBuffer->fragmentStorageTextureSlices[firstSlot + i] = textureSlice;

        VULKAN_INTERNAL_TrackTextureSlice(
            vulkanCommandBuffer,
            textureSlice);
    }

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindFragmentStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
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

    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
}

static VulkanUniformBuffer *VULKAN_INTERNAL_AcquireUniformBufferFromPool(
    VulkanRenderer *renderer)
{
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
                commandBuffer->renderer);
        }
        uniformBuffer = commandBuffer->vertexUniformBuffers[slotIndex];
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
        if (commandBuffer->fragmentUniformBuffers[slotIndex] == NULL) {
            commandBuffer->fragmentUniformBuffers[slotIndex] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                commandBuffer->renderer);
        }
        uniformBuffer = commandBuffer->fragmentUniformBuffers[slotIndex];
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
        if (commandBuffer->computeUniformBuffers[slotIndex] == NULL) {
            commandBuffer->computeUniformBuffers[slotIndex] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                commandBuffer->renderer);
        }
        uniformBuffer = commandBuffer->computeUniformBuffers[slotIndex];
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
        return;
    }

    /* If there is no more room, acquire a new uniform buffer */
    if (uniformBuffer->writeOffset + blockSize + MAX_UBO_SECTION_SIZE >= uniformBuffer->bufferHandle->vulkanBuffer->size) {
        VULKAN_INTERNAL_TrackUniformBuffer(
            commandBuffer,
            uniformBuffer);

        uniformBuffer = VULKAN_INTERNAL_AcquireUniformBufferFromPool(commandBuffer->renderer);

        uniformBuffer->drawOffset = 0;
        uniformBuffer->writeOffset = 0;

        if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_VERTEX) {
            commandBuffer->vertexUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewVertexUniformDescriptorSet = SDL_TRUE;
        } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
            commandBuffer->fragmentUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewFragmentUniformDescriptorSet = SDL_TRUE;
        } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
            commandBuffer->computeUniformBuffers[slotIndex] = uniformBuffer;
            commandBuffer->needNewComputeUniformDescriptorSet = SDL_TRUE;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
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
        commandBuffer->needNewVertexUniformOffsets = SDL_TRUE;
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_FRAGMENT) {
        commandBuffer->needNewFragmentUniformOffsets = SDL_TRUE;
    } else if (uniformBufferStage == VULKAN_UNIFORM_BUFFER_STAGE_COMPUTE) {
        commandBuffer->needNewComputeUniformOffsets = SDL_TRUE;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
        return;
    }
}

static void VULKAN_BeginRenderPass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkRenderPass renderPass;
    VulkanFramebuffer *framebuffer;

    VulkanTextureContainer *textureContainer;
    VulkanTextureSlice *textureSlice;
    Uint32 w, h;
    VkClearValue *clearValues;
    Uint32 clearCount = colorAttachmentCount;
    Uint32 multisampleAttachmentCount = 0;
    Uint32 totalColorAttachmentCount = 0;
    Uint32 i;
    SDL_GpuViewport defaultViewport;
    SDL_GpuRect defaultScissor;
    Uint32 framebufferWidth = UINT32_MAX;
    Uint32 framebufferHeight = UINT32_MAX;

    for (i = 0; i < colorAttachmentCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)colorAttachmentInfos[i].textureSlice.texture;

        w = textureContainer->activeTextureHandle->vulkanTexture->dimensions.width >> colorAttachmentInfos[i].textureSlice.mipLevel;
        h = textureContainer->activeTextureHandle->vulkanTexture->dimensions.height >> colorAttachmentInfos[i].textureSlice.mipLevel;

        /* The framebuffer cannot be larger than the smallest attachment. */

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }

        if (h < framebufferHeight) {
            framebufferHeight = h;
        }

        if (!textureContainer->activeTextureHandle->vulkanTexture->isRenderTarget) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Color attachment texture was not designated as a target!");
            return;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        textureContainer = (VulkanTextureContainer *)depthStencilAttachmentInfo->textureSlice.texture;

        w = textureContainer->activeTextureHandle->vulkanTexture->dimensions.width >> depthStencilAttachmentInfo->textureSlice.mipLevel;
        h = textureContainer->activeTextureHandle->vulkanTexture->dimensions.height >> depthStencilAttachmentInfo->textureSlice.mipLevel;

        /* The framebuffer cannot be larger than the smallest attachment. */

        if (w < framebufferWidth) {
            framebufferWidth = w;
        }

        if (h < framebufferHeight) {
            framebufferHeight = h;
        }

        if (!textureContainer->activeTextureHandle->vulkanTexture->isRenderTarget) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Depth stencil attachment texture was not designated as a target!");
            return;
        }
    }

    /* Layout transitions */

    for (i = 0; i < colorAttachmentCount; i += 1) {
        SDL_bool cycle;
        if (colorAttachmentInfos[i].loadOp) {
            cycle = SDL_FALSE;
        } else {
            cycle = colorAttachmentInfos[i].cycle;
        }

        textureContainer = (VulkanTextureContainer *)colorAttachmentInfos[i].textureSlice.texture;
        textureSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            colorAttachmentInfos[i].textureSlice.layer,
            colorAttachmentInfos[i].textureSlice.mipLevel,
            cycle,
            VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT);

        if (textureSlice->msaaTexHandle != NULL) {
            /* Transition the multisample attachment */
            VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT,
                &textureSlice->msaaTexHandle->vulkanTexture->slices[0]);

            clearCount += 1;
            multisampleAttachmentCount += 1;
        }

        vulkanCommandBuffer->colorAttachmentSlices[i] = textureSlice;

        VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, textureSlice);
        /* TODO: do we need to track the msaa texture? or is it implicitly only used when the regular texture is used? */
    }

    vulkanCommandBuffer->colorAttachmentSliceCount = colorAttachmentCount;

    if (depthStencilAttachmentInfo != NULL) {
        SDL_bool cycle;

        if (
            depthStencilAttachmentInfo->loadOp == SDL_GPU_LOADOP_LOAD ||
            depthStencilAttachmentInfo->stencilLoadOp == SDL_GPU_LOADOP_LOAD) {
            cycle = SDL_FALSE;
        } else {
            cycle = depthStencilAttachmentInfo->cycle;
        }

        textureContainer = (VulkanTextureContainer *)depthStencilAttachmentInfo->textureSlice.texture;
        textureSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            depthStencilAttachmentInfo->textureSlice.layer,
            depthStencilAttachmentInfo->textureSlice.mipLevel,
            cycle,
            VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT);

        clearCount += 1;

        vulkanCommandBuffer->depthStencilAttachmentSlice = textureSlice;

        VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, textureSlice);
    }

    /* Fetch required render objects */

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

    /* Set clear values */

    clearValues = SDL_stack_alloc(VkClearValue, clearCount);

    totalColorAttachmentCount = colorAttachmentCount + multisampleAttachmentCount;

    for (i = 0; i < totalColorAttachmentCount; i += 1) {
        clearValues[i].color.float32[0] = colorAttachmentInfos[i].clearColor.r;
        clearValues[i].color.float32[1] = colorAttachmentInfos[i].clearColor.g;
        clearValues[i].color.float32[2] = colorAttachmentInfos[i].clearColor.b;
        clearValues[i].color.float32[3] = colorAttachmentInfos[i].clearColor.a;

        textureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&colorAttachmentInfos[i].textureSlice);

        if (textureSlice->parent->sampleCount > VK_SAMPLE_COUNT_1_BIT) {
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

    /* Set sensible default viewport state */

    defaultViewport.x = 0;
    defaultViewport.y = 0;
    defaultViewport.w = framebufferWidth;
    defaultViewport.h = framebufferHeight;
    defaultViewport.minDepth = 0;
    defaultViewport.maxDepth = 1;

    VULKAN_INTERNAL_SetCurrentViewport(
        vulkanCommandBuffer,
        &defaultViewport);

    defaultScissor.x = 0;
    defaultScissor.y = 0;
    defaultScissor.w = framebufferWidth;
    defaultScissor.h = framebufferHeight;

    VULKAN_INTERNAL_SetCurrentScissor(
        vulkanCommandBuffer,
        &defaultScissor);
}

static void VULKAN_BindGraphicsPipeline(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
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

    /* Acquire uniform buffers if necessary */
    for (Uint32 i = 0; i < pipeline->resourceLayout.vertexUniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->vertexUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->vertexUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer->renderer);
        }
    }

    for (Uint32 i = 0; i < pipeline->resourceLayout.fragmentUniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->fragmentUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->fragmentUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer->renderer);
        }
    }

    /* Mark bindings as needed */
    vulkanCommandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewVertexUniformDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewFragmentUniformDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewVertexUniformOffsets = SDL_TRUE;
    vulkanCommandBuffer->needNewFragmentUniformOffsets = SDL_TRUE;
}

static void VULKAN_BindVertexBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstBinding,
    SDL_GpuBufferBinding *pBindings,
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
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferBinding *pBinding,
    SDL_GpuIndexElementSize indexElementSize)
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
    SDL_GpuCommandBuffer *commandBuffer,
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
    SDL_GpuCommandBuffer *commandBuffer,
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
    SDL_GpuCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    Uint32 i;

    renderer->vkCmdEndRenderPass(
        vulkanCommandBuffer->commandBuffer);

    for (i = 0; i < vulkanCommandBuffer->colorAttachmentSliceCount; i += 1) {
        VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_COLOR_ATTACHMENT,
            vulkanCommandBuffer->colorAttachmentSlices[i]);
    }
    vulkanCommandBuffer->colorAttachmentSliceCount = 0;

    if (vulkanCommandBuffer->depthStencilAttachmentSlice != NULL) {
        VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_DEPTH_STENCIL_ATTACHMENT,
            vulkanCommandBuffer->depthStencilAttachmentSlice);
        vulkanCommandBuffer->depthStencilAttachmentSlice = NULL;
    }

    vulkanCommandBuffer->currentGraphicsPipeline = NULL;

    vulkanCommandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;
}

static void VULKAN_BeginComputePass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuStorageTextureReadWriteBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GpuStorageBufferReadWriteBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanTextureContainer *textureContainer;
    VulkanTextureSlice *textureSlice;
    VulkanBufferContainer *bufferContainer;
    VulkanBuffer *buffer;
    Uint32 i;

    for (i = 0; i < storageTextureBindingCount; i += 1) {
        textureContainer = (VulkanTextureContainer *)storageTextureBindings[i].textureSlice.texture;
        textureSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
            renderer,
            vulkanCommandBuffer,
            textureContainer,
            storageTextureBindings[i].textureSlice.layer,
            storageTextureBindings[i].textureSlice.mipLevel,
            storageTextureBindings[i].cycle,
            VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE);

        vulkanCommandBuffer->readWriteComputeStorageTextureSlices[i] = textureSlice;

        VULKAN_INTERNAL_TrackTextureSlice(
            vulkanCommandBuffer,
            textureSlice);
    }

    for (i = 0; i < storageBufferBindingCount; i += 1) {
        bufferContainer = (VulkanBufferContainer *)storageBufferBindings[i].buffer;
        buffer = VULKAN_INTERNAL_PrepareBufferForWrite(
            renderer,
            vulkanCommandBuffer,
            bufferContainer,
            storageBufferBindings[i].cycle,
            VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ);

        vulkanCommandBuffer->readWriteComputeStorageBuffers[i] = buffer;

        VULKAN_INTERNAL_TrackBuffer(
            vulkanCommandBuffer,
            buffer);
    }
}

static void VULKAN_BindComputePipeline(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuComputePipeline *computePipeline)
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

    /* Acquire uniform buffers if necessary */
    for (Uint32 i = 0; i < vulkanComputePipeline->resourceLayout.uniformBufferCount; i += 1) {
        if (vulkanCommandBuffer->computeUniformBuffers[i] == NULL) {
            vulkanCommandBuffer->computeUniformBuffers[i] = VULKAN_INTERNAL_AcquireUniformBufferFromPool(
                vulkanCommandBuffer->renderer);
        }
    }

    /* Mark binding as needed */
    vulkanCommandBuffer->needNewComputeReadWriteDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewComputeUniformDescriptorSet = SDL_TRUE;
    vulkanCommandBuffer->needNewComputeUniformOffsets = SDL_TRUE;
}

static void VULKAN_BindComputeStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanTextureContainer *textureContainer;
    VulkanTextureSlice *textureSlice;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[firstSlot + i] != NULL) {
            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[firstSlot + i]);
        }

        textureContainer = (VulkanTextureContainer *)storageTextureSlices[i].texture;
        textureSlice = VULKAN_INTERNAL_FetchTextureSlice(
            textureContainer->activeTextureHandle->vulkanTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[firstSlot + i] = textureSlice;

        VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
            textureSlice);

        VULKAN_INTERNAL_TrackTextureSlice(
            vulkanCommandBuffer,
            textureSlice);
    }

    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_TRUE;
}

static void VULKAN_BindComputeStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
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

    vulkanCommandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_TRUE;
}

static void VULKAN_PushComputeUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
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
    VkDescriptorBufferInfo bufferInfos[MAX_STORAGE_BUFFERS_PER_STAGE];
    VkDescriptorImageInfo imageInfos[MAX_STORAGE_TEXTURES_PER_STAGE];
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
            imageInfos[imageInfoCount].imageView = commandBuffer->readOnlyComputeStorageTextureSlices[i]->view;
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

        commandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_FALSE;
    }

    if (commandBuffer->needNewComputeReadWriteDescriptorSet) {
        descriptorSetPool = &resourceLayout->descriptorSetPools[1];

        commandBuffer->computeReadWriteDescriptorSet = VULKAN_INTERNAL_FetchDescriptorSet(
            renderer,
            commandBuffer,
            descriptorSetPool);

        writeDescriptorSets = SDL_stack_alloc(
            VkWriteDescriptorSet,
            resourceLayout->readWriteStorageTextureCount +
                resourceLayout->readWriteStorageBufferCount);

        for (i = 0; i < resourceLayout->readWriteStorageTextureCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeReadWriteDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pBufferInfo = NULL;

            imageInfos[imageInfoCount].sampler = VK_NULL_HANDLE;
            imageInfos[imageInfoCount].imageView = commandBuffer->readWriteComputeStorageTextureSlices[i]->view;
            imageInfos[imageInfoCount].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            currentWriteDescriptorSet->pImageInfo = &imageInfos[imageInfoCount];

            imageInfoCount += 1;
        }

        for (i = 0; i < resourceLayout->readWriteStorageBufferCount; i += 1) {
            currentWriteDescriptorSet = &writeDescriptorSets[resourceLayout->readWriteStorageTextureCount + i];

            currentWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            currentWriteDescriptorSet->pNext = NULL;
            currentWriteDescriptorSet->descriptorCount = 1;
            currentWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            currentWriteDescriptorSet->dstArrayElement = 0;
            currentWriteDescriptorSet->dstBinding = resourceLayout->readWriteStorageTextureCount + i;
            currentWriteDescriptorSet->dstSet = commandBuffer->computeReadWriteDescriptorSet;
            currentWriteDescriptorSet->pTexelBufferView = NULL;
            currentWriteDescriptorSet->pImageInfo = NULL;

            bufferInfos[bufferInfoCount].buffer = commandBuffer->readWriteComputeStorageBuffers[i]->buffer;
            bufferInfos[bufferInfoCount].offset = 0;
            bufferInfos[bufferInfoCount].range = VK_WHOLE_SIZE;

            currentWriteDescriptorSet->pBufferInfo = &bufferInfos[bufferInfoCount];

            bufferInfoCount += 1;
        }

        renderer->vkUpdateDescriptorSets(
            renderer->logicalDevice,
            resourceLayout->readWriteStorageTextureCount + resourceLayout->readWriteStorageBufferCount,
            writeDescriptorSets,
            0,
            NULL);

        renderer->vkCmdBindDescriptorSets(
            commandBuffer->commandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            resourceLayout->pipelineLayout,
            1,
            1,
            &commandBuffer->computeReadWriteDescriptorSet,
            0,
            NULL);

        SDL_stack_free(writeDescriptorSets);
        bufferInfoCount = 0;
        imageInfoCount = 0;

        commandBuffer->needNewComputeReadWriteDescriptorSet = SDL_FALSE;
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

            VULKAN_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->computeUniformBuffers[i]);
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

        commandBuffer->needNewComputeUniformDescriptorSet = SDL_FALSE;
        commandBuffer->needNewComputeUniformOffsets = SDL_TRUE;
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

        commandBuffer->needNewComputeUniformOffsets = SDL_FALSE;
    }
}

static void VULKAN_DispatchCompute(
    SDL_GpuCommandBuffer *commandBuffer,
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

static void VULKAN_EndComputePass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    Uint32 i;

    for (i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
        if (vulkanCommandBuffer->readWriteComputeStorageTextureSlices[i] != NULL) {
            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
                vulkanCommandBuffer->readWriteComputeStorageTextureSlices[i]);

            vulkanCommandBuffer->readWriteComputeStorageTextureSlices[i] = NULL;
        }
    }

    for (i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i += 1) {
        if (vulkanCommandBuffer->readWriteComputeStorageBuffers[i] != NULL) {
            VULKAN_INTERNAL_BufferTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE,
                vulkanCommandBuffer->readWriteComputeStorageBuffers[i]);

            vulkanCommandBuffer->readWriteComputeStorageBuffers[i] = NULL;
        }
    }

    for (i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i += 1) {
        if (vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[i] != NULL) {
            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                vulkanCommandBuffer->renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COMPUTE_STORAGE_READ,
                vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[i]);

            vulkanCommandBuffer->readOnlyComputeStorageTextureSlices[i] = NULL;
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
    vulkanCommandBuffer->computeReadWriteDescriptorSet = VK_NULL_HANDLE;
    vulkanCommandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;
}

static void VULKAN_MapTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_bool cycle,
    void **ppData)
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

    *ppData = bufferPointer;
}

static void VULKAN_UnmapTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer)
{
    /* no-op because transfer buffers are persistently mapped */
    (void)driverData;
    (void)transferBuffer;
}

static void VULKAN_SetTransferData(
    SDL_GpuRenderer *driverData,
    const void *source,
    SDL_GpuTransferBufferRegion *destination,
    SDL_bool cycle)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)destination->transferBuffer;

    if (
        cycle &&
        SDL_AtomicGet(&transferBufferContainer->activeBufferHandle->vulkanBuffer->referenceCount) > 0) {
        VULKAN_INTERNAL_CycleActiveBuffer(
            renderer,
            transferBufferContainer);
    }

    Uint8 *bufferPointer =
        transferBufferContainer->activeBufferHandle->vulkanBuffer->usedRegion->allocation->mapPointer +
        transferBufferContainer->activeBufferHandle->vulkanBuffer->usedRegion->resourceOffset +
        destination->offset;

    SDL_memcpy(bufferPointer, source, destination->size);
}

static void VULKAN_GetTransferData(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBufferRegion *source,
    void *destination)
{
    (void)driverData; /* used by other backends */
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)source->transferBuffer;
    VulkanBuffer *vulkanBuffer = transferBufferContainer->activeBufferHandle->vulkanBuffer;

    Uint8 *bufferPointer =
        vulkanBuffer->usedRegion->allocation->mapPointer +
        vulkanBuffer->usedRegion->resourceOffset +
        source->offset;

    SDL_memcpy(destination, bufferPointer, source->size);
}

static void VULKAN_BeginCopyPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    /* no-op */
    (void)commandBuffer;
}

static void VULKAN_UploadToTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureTransferInfo *source,
    SDL_GpuTextureRegion *destination,
    SDL_bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)source->transferBuffer;
    VulkanTextureContainer *vulkanTextureContainer = (VulkanTextureContainer *)destination->textureSlice.texture;
    VulkanTextureSlice *vulkanTextureSlice;
    VkBufferImageCopy imageCopy;

    /* Note that the transfer buffer does not need a barrier, as it is synced by the client */

    vulkanTextureSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
        renderer,
        vulkanCommandBuffer,
        vulkanTextureContainer,
        destination->textureSlice.layer,
        destination->textureSlice.mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    imageCopy.imageExtent.width = destination->w;
    imageCopy.imageExtent.height = destination->h;
    imageCopy.imageExtent.depth = destination->d;
    imageCopy.imageOffset.x = destination->x;
    imageCopy.imageOffset.y = destination->y;
    imageCopy.imageOffset.z = destination->z;
    imageCopy.imageSubresource.aspectMask = vulkanTextureSlice->parent->aspectFlags;
    imageCopy.imageSubresource.baseArrayLayer = destination->textureSlice.layer;
    imageCopy.imageSubresource.layerCount = 1;
    imageCopy.imageSubresource.mipLevel = destination->textureSlice.mipLevel;
    imageCopy.bufferOffset = source->offset;
    imageCopy.bufferRowLength = source->imagePitch;
    imageCopy.bufferImageHeight = source->imageHeight;

    renderer->vkCmdCopyBufferToImage(
        vulkanCommandBuffer->commandBuffer,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        vulkanTextureSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        vulkanTextureSlice);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, vulkanTextureSlice);
}

static void VULKAN_UploadToBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTransferBufferLocation *source,
    SDL_GpuBufferRegion *destination,
    SDL_bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)source->transferBuffer;
    VulkanBufferContainer *bufferContainer = (VulkanBufferContainer *)destination->buffer;
    VkBufferCopy bufferCopy;

    /* Note that the transfer buffer does not need a barrier, as it is synced by the client */

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

/* Readback */

static void VULKAN_DownloadFromTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureTransferInfo *destination)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanTextureSlice *vulkanTextureSlice;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)destination->transferBuffer;
    VkBufferImageCopy imageCopy;
    vulkanTextureSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&source->textureSlice);

    /* Note that the transfer buffer does not need a barrier, as it is synced by the client */

    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        vulkanTextureSlice);

    imageCopy.imageExtent.width = source->w;
    imageCopy.imageExtent.height = source->h;
    imageCopy.imageExtent.depth = source->d;
    imageCopy.imageOffset.x = source->x;
    imageCopy.imageOffset.y = source->y;
    imageCopy.imageOffset.z = source->z;
    imageCopy.imageSubresource.aspectMask = vulkanTextureSlice->parent->aspectFlags;
    imageCopy.imageSubresource.baseArrayLayer = source->textureSlice.layer;
    imageCopy.imageSubresource.layerCount = 1;
    imageCopy.imageSubresource.mipLevel = source->textureSlice.mipLevel;
    imageCopy.bufferOffset = destination->offset;
    imageCopy.bufferRowLength = destination->imagePitch;
    imageCopy.bufferImageHeight = destination->imageHeight;

    renderer->vkCmdCopyImageToBuffer(
        vulkanCommandBuffer->commandBuffer,
        vulkanTextureSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        transferBufferContainer->activeBufferHandle->vulkanBuffer->buffer,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        vulkanTextureSlice);

    VULKAN_INTERNAL_TrackBuffer(vulkanCommandBuffer, transferBufferContainer->activeBufferHandle->vulkanBuffer);
    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, vulkanTextureSlice);
}

static void VULKAN_DownloadFromBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferRegion *source,
    SDL_GpuTransferBufferLocation *destination)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = vulkanCommandBuffer->renderer;
    VulkanBufferContainer *bufferContainer = (VulkanBufferContainer *)source->buffer;
    VulkanBufferContainer *transferBufferContainer = (VulkanBufferContainer *)destination->transferBuffer;
    VkBufferCopy bufferCopy;

    /* Note that transfer buffer does not need a barrier, as it is synced by the client */

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
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureLocation *source,
    SDL_GpuTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    SDL_bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanTextureContainer *dstContainer = (VulkanTextureContainer *)destination->textureSlice.texture;
    VulkanTextureSlice *srcSlice;
    VulkanTextureSlice *dstSlice;
    VkImageCopy imageCopy;

    srcSlice = VULKAN_INTERNAL_SDLToVulkanTextureSlice(&source->textureSlice);

    dstSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
        renderer,
        vulkanCommandBuffer,
        dstContainer,
        destination->textureSlice.layer,
        destination->textureSlice.mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSlice);

    imageCopy.srcOffset.x = source->x;
    imageCopy.srcOffset.y = source->y;
    imageCopy.srcOffset.z = source->z;
    imageCopy.srcSubresource.aspectMask = srcSlice->parent->aspectFlags;
    imageCopy.srcSubresource.baseArrayLayer = source->textureSlice.layer;
    imageCopy.srcSubresource.layerCount = 1;
    imageCopy.srcSubresource.mipLevel = source->textureSlice.mipLevel;
    imageCopy.dstOffset.x = destination->x;
    imageCopy.dstOffset.y = destination->y;
    imageCopy.dstOffset.z = destination->z;
    imageCopy.dstSubresource.aspectMask = dstSlice->parent->aspectFlags;
    imageCopy.dstSubresource.baseArrayLayer = destination->textureSlice.layer;
    imageCopy.dstSubresource.layerCount = 1;
    imageCopy.dstSubresource.mipLevel = destination->textureSlice.mipLevel;
    imageCopy.extent.width = w;
    imageCopy.extent.height = h;
    imageCopy.extent.depth = d;

    renderer->vkCmdCopyImage(
        vulkanCommandBuffer->commandBuffer,
        srcSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopy);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcSlice);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        dstSlice);

    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, srcSlice);
    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, dstSlice);
}

static void VULKAN_CopyBufferToBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferLocation *source,
    SDL_GpuBufferLocation *destination,
    Uint32 size,
    SDL_bool cycle)
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
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTexture *texture)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanTexture *vulkanTexture = ((VulkanTextureContainer *)texture)->activeTextureHandle->vulkanTexture;
    VulkanTextureSlice *srcTextureSlice;
    VulkanTextureSlice *dstTextureSlice;
    VkImageBlit blit;
    Uint32 layer, level;

    if (vulkanTexture->levelCount <= 1) {
        return;
    }

    /* Blit each slice sequentially. Barriers, barriers everywhere! */
    for (layer = 0; layer < vulkanTexture->layerCount; layer += 1)
        for (level = 1; level < vulkanTexture->levelCount; level += 1) {
            srcTextureSlice = VULKAN_INTERNAL_FetchTextureSlice(
                vulkanTexture,
                layer,
                level - 1);

            dstTextureSlice = VULKAN_INTERNAL_FetchTextureSlice(
                vulkanTexture,
                layer,
                level);

            VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                srcTextureSlice);

            VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                dstTextureSlice);

            blit.srcOffsets[0].x = 0;
            blit.srcOffsets[0].y = 0;
            blit.srcOffsets[0].z = 0;

            blit.srcOffsets[1].x = vulkanTexture->dimensions.width >> (level - 1);
            blit.srcOffsets[1].y = vulkanTexture->dimensions.height >> (level - 1);
            blit.srcOffsets[1].z = 1;

            blit.dstOffsets[0].x = 0;
            blit.dstOffsets[0].y = 0;
            blit.dstOffsets[0].z = 0;

            blit.dstOffsets[1].x = vulkanTexture->dimensions.width >> level;
            blit.dstOffsets[1].y = vulkanTexture->dimensions.height >> level;
            blit.dstOffsets[1].z = 1;

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

            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                srcTextureSlice);

            VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                renderer,
                vulkanCommandBuffer,
                VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                dstTextureSlice);

            VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, srcTextureSlice);
            VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, dstTextureSlice);
        }
}

static void VULKAN_EndCopyPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    /* no-op */
    (void)commandBuffer;
}

static void VULKAN_Blit(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureRegion *destination,
    SDL_GpuFilter filterMode,
    SDL_bool cycle)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VulkanTextureContainer *sourceTextureContainer = (VulkanTextureContainer *)source->textureSlice.texture;
    VkImageBlit region;

    VulkanTextureSlice *srcTextureSlice = VULKAN_INTERNAL_FetchTextureSlice(
        sourceTextureContainer->activeTextureHandle->vulkanTexture,
        source->textureSlice.layer,
        source->textureSlice.mipLevel);

    VulkanTextureSlice *dstTextureSlice = VULKAN_INTERNAL_PrepareTextureSliceForWrite(
        renderer,
        vulkanCommandBuffer,
        (VulkanTextureContainer *)destination->textureSlice.texture,
        destination->textureSlice.layer,
        destination->textureSlice.mipLevel,
        cycle,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION);

    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcTextureSlice);

    region.srcSubresource.aspectMask = srcTextureSlice->parent->aspectFlags;
    region.srcSubresource.baseArrayLayer = srcTextureSlice->layer;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.mipLevel = srcTextureSlice->level;
    region.srcOffsets[0].x = source->x;
    region.srcOffsets[0].y = source->y;
    region.srcOffsets[0].z = source->z;
    region.srcOffsets[1].x = source->x + source->w;
    region.srcOffsets[1].y = source->y + source->h;
    region.srcOffsets[1].z = source->z + source->d;

    region.dstSubresource.aspectMask = dstTextureSlice->parent->aspectFlags;
    region.dstSubresource.baseArrayLayer = dstTextureSlice->layer;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.mipLevel = dstTextureSlice->level;
    region.dstOffsets[0].x = destination->x;
    region.dstOffsets[0].y = destination->y;
    region.dstOffsets[0].z = destination->z;
    region.dstOffsets[1].x = destination->x + destination->w;
    region.dstOffsets[1].y = destination->y + destination->h;
    region.dstOffsets[1].z = destination->z + destination->d;

    renderer->vkCmdBlitImage(
        vulkanCommandBuffer->commandBuffer,
        srcTextureSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstTextureSlice->parent->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region,
        SDLToVK_Filter[filterMode]);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
        srcTextureSlice);

    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
        renderer,
        vulkanCommandBuffer,
        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
        dstTextureSlice);

    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, srcTextureSlice);
    VULKAN_INTERNAL_TrackTextureSlice(vulkanCommandBuffer, dstTextureSlice);
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

        /* Presentation tracking */

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

        /* Descriptor set tracking */

        commandBuffer->boundDescriptorSetDataCapacity = 16;
        commandBuffer->boundDescriptorSetDataCount = 0;
        commandBuffer->boundDescriptorSetDatas = SDL_malloc(
            commandBuffer->boundDescriptorSetDataCapacity * sizeof(DescriptorSetData));

        /* Resource bind tracking */

        commandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
        commandBuffer->needNewVertexUniformDescriptorSet = SDL_TRUE;
        commandBuffer->needNewVertexUniformOffsets = SDL_TRUE;
        commandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
        commandBuffer->needNewFragmentUniformDescriptorSet = SDL_TRUE;
        commandBuffer->needNewFragmentUniformOffsets = SDL_TRUE;

        commandBuffer->needNewComputeReadWriteDescriptorSet = SDL_TRUE;
        commandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_TRUE;
        commandBuffer->needNewComputeUniformDescriptorSet = SDL_TRUE;
        commandBuffer->needNewComputeUniformOffsets = SDL_TRUE;

        commandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;

        commandBuffer->computeReadOnlyDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->computeReadWriteDescriptorSet = VK_NULL_HANDLE;
        commandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;

        /* Resource tracking */

        commandBuffer->usedBufferCapacity = 4;
        commandBuffer->usedBufferCount = 0;
        commandBuffer->usedBuffers = SDL_malloc(
            commandBuffer->usedBufferCapacity * sizeof(VulkanBuffer *));

        commandBuffer->usedTextureSliceCapacity = 4;
        commandBuffer->usedTextureSliceCount = 0;
        commandBuffer->usedTextureSlices = SDL_malloc(
            commandBuffer->usedTextureSliceCapacity * sizeof(VulkanTextureSlice *));

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

        /* Pool it! */

        vulkanCommandPool->inactiveCommandBuffers[vulkanCommandPool->inactiveCommandBufferCount] = commandBuffer;
        vulkanCommandPool->inactiveCommandBufferCount += 1;
    }

    SDL_stack_free(commandBuffers);
}

static VulkanCommandPool *VULKAN_INTERNAL_FetchCommandPool(
    VulkanRenderer *renderer,
    SDL_ThreadID threadID)
{
    VulkanCommandPool *vulkanCommandPool;
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    VkResult vulkanResult;
    CommandPoolHash commandPoolHash;

    commandPoolHash.threadID = threadID;

    vulkanCommandPool = CommandPoolHashTable_Fetch(
        &renderer->commandPoolHashTable,
        commandPoolHash);

    if (vulkanCommandPool != NULL) {
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create command pool!");
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

    CommandPoolHashTable_Insert(
        &renderer->commandPoolHashTable,
        commandPoolHash,
        vulkanCommandPool);

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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to fetch command pool!");
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

static SDL_GpuCommandBuffer *VULKAN_AcquireCommandBuffer(
    SDL_GpuRenderer *driverData)
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire command buffer!");
        return NULL;
    }

    /* Reset state */

    commandBuffer->currentComputePipeline = NULL;
    commandBuffer->currentGraphicsPipeline = NULL;

    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        commandBuffer->colorAttachmentSlices[i] = NULL;
    }

    for (i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
        commandBuffer->vertexUniformBuffers[i] = NULL;
        commandBuffer->fragmentUniformBuffers[i] = NULL;
        commandBuffer->computeUniformBuffers[i] = NULL;
    }

    commandBuffer->depthStencilAttachmentSlice = NULL;

    commandBuffer->needNewVertexResourceDescriptorSet = SDL_TRUE;
    commandBuffer->needNewVertexUniformDescriptorSet = SDL_TRUE;
    commandBuffer->needNewVertexUniformOffsets = SDL_TRUE;
    commandBuffer->needNewFragmentResourceDescriptorSet = SDL_TRUE;
    commandBuffer->needNewFragmentUniformDescriptorSet = SDL_TRUE;
    commandBuffer->needNewFragmentUniformOffsets = SDL_TRUE;

    commandBuffer->needNewComputeReadOnlyDescriptorSet = SDL_TRUE;
    commandBuffer->needNewComputeUniformDescriptorSet = SDL_TRUE;
    commandBuffer->needNewComputeUniformOffsets = SDL_TRUE;

    commandBuffer->vertexResourceDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->vertexUniformDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->fragmentResourceDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->fragmentUniformDescriptorSet = VK_NULL_HANDLE;

    commandBuffer->computeReadOnlyDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->computeReadWriteDescriptorSet = VK_NULL_HANDLE;
    commandBuffer->computeUniformDescriptorSet = VK_NULL_HANDLE;

    SDL_zeroa(commandBuffer->vertexSamplerTextures);
    SDL_zeroa(commandBuffer->vertexSamplers);
    SDL_zeroa(commandBuffer->vertexStorageTextureSlices);
    SDL_zeroa(commandBuffer->vertexStorageBuffers);

    SDL_zeroa(commandBuffer->fragmentSamplerTextures);
    SDL_zeroa(commandBuffer->fragmentSamplers);
    SDL_zeroa(commandBuffer->fragmentStorageTextureSlices);
    SDL_zeroa(commandBuffer->fragmentStorageBuffers);

    SDL_zeroa(commandBuffer->readWriteComputeStorageTextureSlices);
    SDL_zeroa(commandBuffer->readWriteComputeStorageBuffers);
    SDL_zeroa(commandBuffer->readOnlyComputeStorageTextureSlices);
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

    return (SDL_GpuCommandBuffer *)commandBuffer;
}

static SDL_bool VULKAN_QueryFence(
    SDL_GpuRenderer *driverData,
    SDL_GpuFence *fence)
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
    SDL_GpuRenderer *driverData,
    SDL_GpuFence *fence)
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
    return (WindowData *)SDL_GetProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static SDL_bool VULKAN_SupportsSwapchainComposition(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    SDL_VideoDevice *_this;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);
    VkSurfaceKHR surface;
    SwapchainSupportDetails supportDetails;
    SDL_bool destroySurface = SDL_FALSE;
    SDL_bool result = SDL_FALSE;
    Uint32 i;

    if (windowData == NULL || windowData->swapchainData == NULL) {
        /* Create a dummy surface is the window is not claimed */
        destroySurface = SDL_TRUE;
        _this = SDL_GetVideoDevice();
        if (_this->Vulkan_CreateSurface(
                _this,
                window,
                renderer->instance,
                NULL,
                &surface) < 0) {
            return SDL_FALSE;
        }
    } else {
        surface = windowData->swapchainData->surface;
    }

    if (VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            surface,
            &supportDetails)) {
        for (i = 0; i < supportDetails.formatsLength; i += 1) {
            if (supportDetails.formats[i].format == SwapchainCompositionToFormat[swapchainComposition] &&
                supportDetails.formats[i].colorSpace == SwapchainCompositionToColorSpace[swapchainComposition]) {
                result = SDL_TRUE;
                break;
            }
        }

        SDL_free(supportDetails.formats);
        SDL_free(supportDetails.presentModes);
    }

    if (destroySurface) {
        SDL_Vulkan_DestroySurface(
            renderer->instance,
            surface,
            NULL);
    }

    return result;
}

static SDL_bool VULKAN_SupportsPresentMode(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuPresentMode presentMode)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    SDL_VideoDevice *_this;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);
    VkSurfaceKHR surface;
    SwapchainSupportDetails supportDetails;
    SDL_bool destroySurface = SDL_FALSE;
    SDL_bool result = SDL_FALSE;
    Uint32 i;

    if (windowData == NULL || windowData->swapchainData == NULL) {
        /* Create a dummy surface is the window is not claimed */
        destroySurface = SDL_TRUE;
        _this = SDL_GetVideoDevice();
        if (_this->Vulkan_CreateSurface(
                _this,
                window,
                renderer->instance,
                NULL,
                &surface) < 0) {
            return SDL_FALSE;
        }
    } else {
        surface = windowData->swapchainData->surface;
    }

    if (VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            surface,
            &supportDetails)) {

        for (i = 0; i < supportDetails.presentModesLength; i += 1) {
            if (supportDetails.presentModes[i] == SDLToVK_PresentMode[presentMode]) {
                result = SDL_TRUE;
                break;
            }
        }

        SDL_free(supportDetails.formats);
        SDL_free(supportDetails.presentModes);
    }

    if (destroySurface) {
        SDL_Vulkan_DestroySurface(
            renderer->instance,
            surface,
            NULL);
    }

    return result;
}

static SDL_bool VULKAN_ClaimWindow(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = SDL_malloc(sizeof(WindowData));
        windowData->window = window;
        windowData->presentMode = presentMode;
        windowData->swapchainComposition = swapchainComposition;

        if (VULKAN_INTERNAL_CreateSwapchain(renderer, windowData)) {
            SDL_SetProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(WindowData *));
            }

            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            return 1;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create swapchain, failed to claim window!");
            SDL_free(windowData);
            return 0;
        }
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Window already claimed!");
        return 0;
    }
}

static void VULKAN_UnclaimWindow(
    SDL_GpuRenderer *driverData,
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
                    (SDL_GpuFence *)windowData->swapchainData->inFlightFences[i]);
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
}

static SDL_bool VULKAN_INTERNAL_RecreateSwapchain(
    VulkanRenderer *renderer,
    WindowData *windowData)
{
    Uint32 i;

    if (windowData->swapchainData != NULL) {
        VULKAN_Wait((SDL_GpuRenderer *)renderer);

        for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
            if (windowData->swapchainData->inFlightFences[i] != NULL) {
                VULKAN_ReleaseFence(
                    (SDL_GpuRenderer *)renderer,
                    (SDL_GpuFence *)windowData->swapchainData->inFlightFences[i]);
            }
        }
    }

    VULKAN_INTERNAL_DestroySwapchain(renderer, windowData);
    return VULKAN_INTERNAL_CreateSwapchain(renderer, windowData);
}

static SDL_GpuTexture *VULKAN_AcquireSwapchainTexture(
    SDL_GpuCommandBuffer *commandBuffer,
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

    /* Window is claimed but swapchain is invalid! */
    if (swapchainData == NULL) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            /* Window is minimized, don't bother */
            return NULL;
        }

        /* Let's try to recreate */
        VULKAN_INTERNAL_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain!");
            return NULL;
        }
    }

    if (swapchainData->inFlightFences[swapchainData->frameCounter] != NULL) {
        if (swapchainData->presentMode == VK_PRESENT_MODE_FIFO_KHR) {
            /* In VSYNC mode, block until the least recent presented frame is done */
            VULKAN_WaitForFences(
                (SDL_GpuRenderer *)renderer,
                SDL_TRUE,
                (SDL_GpuFence **)&swapchainData->inFlightFences[swapchainData->frameCounter],
                1);
        } else {
            if (!VULKAN_QueryFence(
                    (SDL_GpuRenderer *)renderer,
                    (SDL_GpuFence *)swapchainData->inFlightFences[swapchainData->frameCounter])) {
                /*
                 * In MAILBOX or IMMEDIATE mode, if the least recent fence is not signaled,
                 * return NULL to indicate that rendering should be skipped
                 */
                return NULL;
            }
        }

        VULKAN_ReleaseFence(
            (SDL_GpuRenderer *)renderer,
            (SDL_GpuFence *)swapchainData->inFlightFences[swapchainData->frameCounter]);

        swapchainData->inFlightFences[swapchainData->frameCounter] = NULL;
    }

    acquireResult = renderer->vkAcquireNextImageKHR(
        renderer->logicalDevice,
        swapchainData->swapchain,
        UINT64_MAX,
        swapchainData->imageAvailableSemaphore[swapchainData->frameCounter],
        VK_NULL_HANDLE,
        &swapchainImageIndex);

    /* Acquisition is invalid, let's try to recreate */
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        VULKAN_INTERNAL_RecreateSwapchain(renderer, windowData);
        swapchainData = windowData->swapchainData;

        if (swapchainData == NULL) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain!");
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
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire swapchain texture!");
            return NULL;
        }
    }

    swapchainTextureContainer = &swapchainData->textureContainers[swapchainImageIndex];

    /* We need a special execution dependency with pWaitDstStageMask or image transition can start before acquire finishes */

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

    /* Set up present struct */

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

    /* Set up present semaphores */

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

    *pWidth = swapchainData->extent.width;
    *pHeight = swapchainData->extent.height;

    return (SDL_GpuTexture *)swapchainTextureContainer;
}

static SDL_GpuTextureFormat VULKAN_GetSwapchainTextureFormat(
    SDL_GpuRenderer *driverData,
    SDL_Window *window)
{
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot get swapchain format, window has not been claimed!");
        return 0;
    }

    if (windowData->swapchainData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot get swapchain format, swapchain is currently invalid!");
        return 0;
    }

    switch (windowData->swapchainData->format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8;

    case VK_FORMAT_B8G8R8A8_UNORM:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8;

    case VK_FORMAT_B8G8R8A8_SRGB:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_SRGB;

    case VK_FORMAT_R8G8B8A8_SRGB:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SRGB;

    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT;

    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2;

    default:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized swapchain format!");
        return 0;
    }
}

static SDL_bool VULKAN_SetSwapchainParameters(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    WindowData *windowData = VULKAN_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot set swapchain parameters on unclaimed window!");
        return SDL_FALSE;
    }

    if (!VULKAN_SupportsSwapchainComposition(driverData, window, swapchainComposition)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Swapchain composition not supported!");
        return SDL_FALSE;
    }

    if (!VULKAN_SupportsPresentMode(driverData, window, presentMode)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Present mode not supported!");
        return SDL_FALSE;
    }

    return VULKAN_INTERNAL_RecreateSwapchain(
        (VulkanRenderer *)driverData,
        windowData);
}

/* Submission structure */

static VulkanFenceHandle *VULKAN_INTERNAL_AcquireFenceFromPool(
    VulkanRenderer *renderer)
{
    VulkanFenceHandle *handle;
    VkFenceCreateInfo fenceCreateInfo;
    VkFence fence;
    VkResult vulkanResult;

    if (renderer->fencePool.availableFenceCount == 0) {
        /* Create fence */
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
    Sint32 i, sliceIndex;
    Sint32 refCountTotal;

    SDL_LockMutex(renderer->disposeLock);

    for (i = renderer->texturesToDestroyCount - 1; i >= 0; i -= 1) {
        refCountTotal = 0;
        for (sliceIndex = 0; sliceIndex < renderer->texturesToDestroy[i]->sliceCount; sliceIndex += 1) {
            refCountTotal += SDL_AtomicGet(&renderer->texturesToDestroy[i]->slices[sliceIndex].referenceCount);
        }

        if (refCountTotal == 0) {
            VULKAN_INTERNAL_DestroyTexture(
                renderer,
                renderer->texturesToDestroy[i]);

            renderer->texturesToDestroy[i] = renderer->texturesToDestroy[renderer->texturesToDestroyCount - 1];
            renderer->texturesToDestroyCount -= 1;
        }
    }

    for (i = renderer->buffersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->buffersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyBuffer(
                renderer,
                renderer->buffersToDestroy[i]);

            renderer->buffersToDestroy[i] = renderer->buffersToDestroy[renderer->buffersToDestroyCount - 1];
            renderer->buffersToDestroyCount -= 1;
        }
    }

    for (i = renderer->graphicsPipelinesToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->graphicsPipelinesToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyGraphicsPipeline(
                renderer,
                renderer->graphicsPipelinesToDestroy[i]);

            renderer->graphicsPipelinesToDestroy[i] = renderer->graphicsPipelinesToDestroy[renderer->graphicsPipelinesToDestroyCount - 1];
            renderer->graphicsPipelinesToDestroyCount -= 1;
        }
    }

    for (i = renderer->computePipelinesToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->computePipelinesToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyComputePipeline(
                renderer,
                renderer->computePipelinesToDestroy[i]);

            renderer->computePipelinesToDestroy[i] = renderer->computePipelinesToDestroy[renderer->computePipelinesToDestroyCount - 1];
            renderer->computePipelinesToDestroyCount -= 1;
        }
    }

    for (i = renderer->shadersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->shadersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroyShader(
                renderer,
                renderer->shadersToDestroy[i]);

            renderer->shadersToDestroy[i] = renderer->shadersToDestroy[renderer->shadersToDestroyCount - 1];
            renderer->shadersToDestroyCount -= 1;
        }
    }

    for (i = renderer->samplersToDestroyCount - 1; i >= 0; i -= 1) {
        if (SDL_AtomicGet(&renderer->samplersToDestroy[i]->referenceCount) == 0) {
            VULKAN_INTERNAL_DestroySampler(
                renderer,
                renderer->samplersToDestroy[i]);

            renderer->samplersToDestroy[i] = renderer->samplersToDestroy[renderer->samplersToDestroyCount - 1];
            renderer->samplersToDestroyCount -= 1;
        }
    }

    for (i = renderer->framebuffersToDestroyCount - 1; i >= 0; i -= 1) {
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
            (SDL_GpuRenderer *)renderer,
            (SDL_GpuFence *)commandBuffer->inFlightFence);

        commandBuffer->inFlightFence = NULL;
    }

    /* Bound descriptor sets are now available */

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

    /* Uniform buffers are now available */

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        VULKAN_INTERNAL_ReturnUniformBufferToPool(
            renderer,
            commandBuffer->usedUniformBuffers[i]);
    }
    commandBuffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    /* Decrement reference counts */

    for (i = 0; i < commandBuffer->usedBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedBuffers[i]->referenceCount);
    }
    commandBuffer->usedBufferCount = 0;

    for (i = 0; i < commandBuffer->usedTextureSliceCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedTextureSlices[i]->referenceCount);
    }
    commandBuffer->usedTextureSliceCount = 0;

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

    /* Reset presentation data */

    commandBuffer->presentDataCount = 0;
    commandBuffer->waitSemaphoreCount = 0;
    commandBuffer->signalSemaphoreCount = 0;

    /* Reset defrag state */

    if (commandBuffer->isDefrag) {
        renderer->defragInProgress = 0;
    }

    /* Return command buffer to pool */

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

    /* Remove this command buffer from the submitted list */
    for (i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        if (renderer->submittedCommandBuffers[i] == commandBuffer) {
            renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
            renderer->submittedCommandBufferCount -= 1;
        }
    }
}

static void VULKAN_WaitForFences(
    SDL_GpuRenderer *driverData,
    SDL_bool waitAll,
    SDL_GpuFence **pFences,
    Uint32 fenceCount)
{
    VulkanRenderer *renderer = (VulkanRenderer *)driverData;
    VkFence *fences = SDL_stack_alloc(VkFence, fenceCount);
    VkResult result;
    Sint32 i;

    for (i = 0; i < fenceCount; i += 1) {
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

    for (i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
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
    SDL_GpuRenderer *driverData)
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

static SDL_GpuFence *VULKAN_SubmitAndAcquireFence(
    SDL_GpuCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer;

    vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    vulkanCommandBuffer->autoReleaseFence = 0;

    VULKAN_Submit(commandBuffer);

    return (SDL_GpuFence *)vulkanCommandBuffer->inFlightFence;
}

static void VULKAN_Submit(
    SDL_GpuCommandBuffer *commandBuffer)
{
    VulkanCommandBuffer *vulkanCommandBuffer = (VulkanCommandBuffer *)commandBuffer;
    VulkanRenderer *renderer = (VulkanRenderer *)vulkanCommandBuffer->renderer;
    VkSubmitInfo submitInfo;
    VkPresentInfoKHR presentInfo;
    VulkanPresentData *presentData;
    VkResult vulkanResult, presentResult = VK_SUCCESS;
    VkPipelineStageFlags waitStages[MAX_PRESENT_COUNT];
    Uint32 swapchainImageIndex;
    VulkanTextureSlice *swapchainTextureSlice;
    Uint8 commandBufferCleaned = 0;
    VulkanMemorySubAllocator *allocator;
    SDL_bool presenting = SDL_FALSE;
    Sint32 i, j;

    SDL_LockMutex(renderer->submitLock);

    /* FIXME: Can this just be permanent? */
    for (i = 0; i < MAX_PRESENT_COUNT; i += 1) {
        waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    for (j = 0; j < vulkanCommandBuffer->presentDataCount; j += 1) {
        swapchainImageIndex = vulkanCommandBuffer->presentDatas[j].swapchainImageIndex;
        swapchainTextureSlice = VULKAN_INTERNAL_FetchTextureSlice(
            vulkanCommandBuffer->presentDatas[j].windowData->swapchainData->textureContainers[swapchainImageIndex].activeTextureHandle->vulkanTexture,
            0,
            0);

        VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
            renderer,
            vulkanCommandBuffer,
            VULKAN_TEXTURE_USAGE_MODE_PRESENT,
            swapchainTextureSlice);
    }

    VULKAN_INTERNAL_EndCommandBuffer(renderer, vulkanCommandBuffer);

    vulkanCommandBuffer->inFlightFence = VULKAN_INTERNAL_AcquireFenceFromPool(renderer);

    /* Command buffer has a reference to the in-flight fence */
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

    /* Mark command buffers as submitted */

    if (renderer->submittedCommandBufferCount + 1 >= renderer->submittedCommandBufferCapacity) {
        renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

        renderer->submittedCommandBuffers = SDL_realloc(
            renderer->submittedCommandBuffers,
            sizeof(VulkanCommandBuffer *) * renderer->submittedCommandBufferCapacity);
    }

    renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = vulkanCommandBuffer;
    renderer->submittedCommandBufferCount += 1;

    /* Present, if applicable */

    for (j = 0; j < vulkanCommandBuffer->presentDataCount; j += 1) {
        presenting = SDL_TRUE;

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
            /* If presenting, the swapchain is using the in-flight fence */
            presentData->windowData->swapchainData->inFlightFences[presentData->windowData->swapchainData->frameCounter] = vulkanCommandBuffer->inFlightFence;

            (void)SDL_AtomicIncRef(&vulkanCommandBuffer->inFlightFence->referenceCount);
        }
    }

    /* Check if we can perform any cleanups */

    for (i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
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

        for (i = 0; i < VK_MAX_MEMORY_TYPES; i += 1) {
            allocator = &renderer->memoryAllocator->subAllocators[i];

            for (j = allocator->allocationCount - 1; j >= 0; j -= 1) {
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

    /* Check pending destroys */
    VULKAN_INTERNAL_PerformPendingDestroys(renderer);

    /* Defrag! */
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
    VulkanTextureSlice *srcSlice;
    VulkanTextureSlice *dstSlice;
    Uint32 i, sliceIndex;

    SDL_LockMutex(renderer->allocatorLock);

    renderer->defragInProgress = 1;

    commandBuffer = (VulkanCommandBuffer *)VULKAN_AcquireCommandBuffer((SDL_GpuRenderer *)renderer);
    if (commandBuffer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create defrag command buffer!");
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
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create defrag buffer!");
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

            /* Copy buffer contents if necessary */
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

            /* re-point original container to new buffer */
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
                currentRegion->vulkanTexture->isCube,
                currentRegion->vulkanTexture->layerCount,
                currentRegion->vulkanTexture->levelCount,
                currentRegion->vulkanTexture->sampleCount,
                currentRegion->vulkanTexture->format,
                currentRegion->vulkanTexture->swizzle,
                currentRegion->vulkanTexture->aspectFlags,
                currentRegion->vulkanTexture->usageFlags,
                currentRegion->vulkanTexture->isMSAAColorTarget);

            if (newTexture == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create defrag texture!");
                return 0;
            }

            for (sliceIndex = 0; sliceIndex < currentRegion->vulkanTexture->sliceCount; sliceIndex += 1) {
                /* copy slice if necessary */
                srcSlice = &currentRegion->vulkanTexture->slices[sliceIndex];
                dstSlice = &newTexture->slices[sliceIndex];

                /* Set debug name if it exists */
                if (
                    renderer->debugMode &&
                    renderer->supportsDebugUtils &&
                    srcSlice->parent->handle != NULL &&
                    srcSlice->parent->handle->container != NULL &&
                    srcSlice->parent->handle->container->debugName != NULL) {
                    VULKAN_INTERNAL_SetTextureName(
                        renderer,
                        currentRegion->vulkanTexture,
                        srcSlice->parent->handle->container->debugName);
                }

                if (srcSlice->transitioned) {
                    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_SOURCE,
                        srcSlice);

                    VULKAN_INTERNAL_TextureTransitionFromDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                        dstSlice);

                    imageCopy.srcOffset.x = 0;
                    imageCopy.srcOffset.y = 0;
                    imageCopy.srcOffset.z = 0;
                    imageCopy.srcSubresource.aspectMask = srcSlice->parent->aspectFlags;
                    imageCopy.srcSubresource.baseArrayLayer = srcSlice->layer;
                    imageCopy.srcSubresource.layerCount = 1;
                    imageCopy.srcSubresource.mipLevel = srcSlice->level;
                    imageCopy.extent.width = SDL_max(1, srcSlice->parent->dimensions.width >> srcSlice->level);
                    imageCopy.extent.height = SDL_max(1, srcSlice->parent->dimensions.height >> srcSlice->level);
                    imageCopy.extent.depth = srcSlice->parent->depth;
                    imageCopy.dstOffset.x = 0;
                    imageCopy.dstOffset.y = 0;
                    imageCopy.dstOffset.z = 0;
                    imageCopy.dstSubresource.aspectMask = dstSlice->parent->aspectFlags;
                    imageCopy.dstSubresource.baseArrayLayer = dstSlice->layer;
                    imageCopy.dstSubresource.layerCount = 1;
                    imageCopy.dstSubresource.mipLevel = dstSlice->level;

                    renderer->vkCmdCopyImage(
                        commandBuffer->commandBuffer,
                        currentRegion->vulkanTexture->image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        newTexture->image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &imageCopy);

                    VULKAN_INTERNAL_TextureTransitionToDefaultUsage(
                        renderer,
                        commandBuffer,
                        VULKAN_TEXTURE_USAGE_MODE_COPY_DESTINATION,
                        dstSlice);

                    VULKAN_INTERNAL_TrackTextureSlice(commandBuffer, srcSlice);
                    VULKAN_INTERNAL_TrackTextureSlice(commandBuffer, dstSlice);
                }
            }

            /* re-point original container to new texture */
            newTexture->handle = currentRegion->vulkanTexture->handle;
            newTexture->handle->vulkanTexture = newTexture;
            currentRegion->vulkanTexture->handle = NULL;

            VULKAN_INTERNAL_ReleaseTexture(renderer, currentRegion->vulkanTexture);
        }
    }

    SDL_UnlockMutex(renderer->allocatorLock);

    VULKAN_Submit(
        (SDL_GpuCommandBuffer *)commandBuffer);

    return 1;
}

/* Format Info */

static SDL_bool VULKAN_IsTextureFormatSupported(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureFormat format,
    SDL_GpuTextureType type,
    SDL_GpuTextureUsageFlags usage)
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

/* Device instantiation */

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
        else CHECK(KHR_maintenance1) else CHECK(KHR_get_memory_requirements2) else CHECK(KHR_driver_properties) else CHECK(EXT_vertex_attribute_divisor) else CHECK(KHR_portability_subset)
#undef CHECK
    }

    return (supports->KHR_swapchain &&
            supports->KHR_maintenance1 &&
            supports->KHR_get_memory_requirements2);
}

static inline Uint32 GetDeviceExtensionCount(VulkanExtensions *supports)
{
    return (
        supports->KHR_swapchain +
        supports->KHR_maintenance1 +
        supports->KHR_get_memory_requirements2 +
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
    CHECK(KHR_get_memory_requirements2)
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
    SDL_bool *supportsDebugUtils,
    SDL_bool *supportsColorspace)
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

    /* This is optional, but nice to have! */
    *supportsDebugUtils = SupportsInstanceExtension(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        availableExtensions,
        extensionCount);

    /* Also optional and nice to have! */
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

static Uint8 VULKAN_INTERNAL_CreateInstance(
    VulkanRenderer *renderer,
    SDL_Window *deviceWindowHandle)
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
            SDL_LOG_CATEGORY_APPLICATION,
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

    /* Core since 1.1 */
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
            SDL_LOG_CATEGORY_APPLICATION,
            "Required Vulkan instance extensions not supported");

        SDL_stack_free((char *)instanceExtensionNames);
        return 0;
    }

    if (renderer->supportsDebugUtils) {
        /* Append the debug extension */
        instanceExtensionNames[instanceExtensionCount++] =
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    } else {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "%s is not supported!",
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (renderer->supportsColorspace) {
        /* Append colorspace extension */
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
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Validation layers not found, continuing without validation");
            createInfo.enabledLayerCount = 0;
        } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Validation layers enabled, expect debug level performance!");
        }
    } else {
        createInfo.enabledLayerCount = 0;
    }

    vulkanResult = vkCreateInstance(&createInfo, NULL, &renderer->instance);
    if (vulkanResult != VK_SUCCESS) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
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
    VkSurfaceKHR surface,
    Uint32 *queueFamilyIndex,
    Uint8 *deviceRank)
{
    Uint32 queueFamilyCount, queueFamilyRank, queueFamilyBest;
    SwapchainSupportDetails swapchainSupportDetails;
    VkQueueFamilyProperties *queueProps;
    VkBool32 supportsPresent;
    Uint8 querySuccess;
    VkPhysicalDeviceProperties deviceProperties;
    Uint32 i;

    /* Get the device rank before doing any checks, in case one fails.
     * Note: If no dedicated device exists, one that supports our features
     * would be fine
     */
    renderer->vkGetPhysicalDeviceProperties(
        physicalDevice,
        &deviceProperties);
    if (*deviceRank < DEVICE_PRIORITY[deviceProperties.deviceType]) {
        /* This device outranks the best device we've found so far!
         * This includes a dedicated GPU that has less features than an
         * integrated GPU, because this is a freak case that is almost
         * never intentionally desired by the end user
         */
        *deviceRank = DEVICE_PRIORITY[deviceProperties.deviceType];
    } else if (*deviceRank > DEVICE_PRIORITY[deviceProperties.deviceType]) {
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
        renderer->vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            i,
            surface,
            &supportsPresent);
        if (!supportsPresent ||
            !(queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            /* Not a graphics family, ignore. */
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
                /* Has all attribs! */
                queueFamilyRank = 3;
            } else {
                /* Probably has a DMA transfer queue family */
                queueFamilyRank = 2;
            }
        } else {
            /* Just a graphics family, probably has something better */
            queueFamilyRank = 1;
        }
        if (queueFamilyRank > queueFamilyBest) {
            *queueFamilyIndex = i;
            queueFamilyBest = queueFamilyRank;
        }
    }

    SDL_stack_free(queueProps);

    if (*queueFamilyIndex == UINT32_MAX) {
        /* Somehow no graphics queues existed. Compute-only device? */
        return 0;
    }

    /* FIXME: Need better structure for checking vs storing support details */
    querySuccess = VULKAN_INTERNAL_QuerySwapchainSupport(
        renderer,
        physicalDevice,
        surface,
        &swapchainSupportDetails);
    if (querySuccess && swapchainSupportDetails.formatsLength > 0) {
        SDL_free(swapchainSupportDetails.formats);
    }
    if (querySuccess && swapchainSupportDetails.presentModesLength > 0) {
        SDL_free(swapchainSupportDetails.presentModes);
    }

    return (querySuccess &&
            swapchainSupportDetails.formatsLength > 0 &&
            swapchainSupportDetails.presentModesLength > 0);
}

static Uint8 VULKAN_INTERNAL_DeterminePhysicalDevice(
    VulkanRenderer *renderer,
    VkSurfaceKHR surface)
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
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to find any GPUs with Vulkan support");
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
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "vkEnumeratePhysicalDevices returned VK_INCOMPLETE, will keep trying anyway...");
        vulkanResult = VK_SUCCESS;
    }

    if (vulkanResult != VK_SUCCESS) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "vkEnumeratePhysicalDevices failed: %s",
            VkErrorMessages(vulkanResult));
        SDL_stack_free(physicalDevices);
        SDL_stack_free(physicalDeviceExtensions);
        return 0;
    }

    /* Any suitable device will do, but we'd like the best */
    suitableIndex = -1;
    suitableQueueFamilyIndex = 0;
    highestRank = 0;
    for (i = 0; i < physicalDeviceCount; i += 1) {
        deviceRank = highestRank;
        if (VULKAN_INTERNAL_IsDeviceSuitable(
                renderer,
                physicalDevices[i],
                &physicalDeviceExtensions[i],
                surface,
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
    } else {
        renderer->physicalDeviceProperties.pNext = NULL;
    }

    renderer->vkGetPhysicalDeviceProperties2KHR(
        renderer->physicalDevice,
        &renderer->physicalDeviceProperties);

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
    VkPhysicalDeviceFeatures deviceFeatures;
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

    /* specifying used device features */

    SDL_zero(deviceFeatures);
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.independentBlend = VK_TRUE;

    /* creating the logical device */

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
        portabilityFeatures.samplerMipLodBias = VK_FALSE; /* Technically should be true, but eh */
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
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    vulkanResult = renderer->vkCreateDevice(
        renderer->physicalDevice,
        &deviceCreateInfo,
        NULL,
        &renderer->logicalDevice);
    SDL_stack_free(deviceExtensions);
    VULKAN_ERROR_CHECK(vulkanResult, vkCreateDevice, 0)

    /* Load vkDevice entry points */

#define VULKAN_DEVICE_FUNCTION(ext, ret, func, params)  \
    renderer->func = (vkfntype_##func)                  \
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
    /* Required for MoltenVK support */
    SDL_setenv("MVK_CONFIG_FULL_IMAGE_VIEW_SWIZZLE", "1", 1);

    /* Load Vulkan entry points */
    if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: SDL_Vulkan_LoadLibrary failed!");
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
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_Vulkan_GetVkGetInstanceProcAddr(): %s",
            SDL_GetError());
        return;
    }

#define VULKAN_GLOBAL_FUNCTION(name)                                                                              \
    name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);                                              \
    if (name == NULL) {                                                                                           \
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "vkGetInstanceProcAddr(VK_NULL_HANDLE, \"" #name "\") failed"); \
        return;                                                                                                   \
    }
#include "SDL_gpu_vulkan_vkfuncs.h"
}

static SDL_bool VULKAN_INTERNAL_PrepareVulkan(
    VulkanRenderer *renderer)
{
    SDL_Window *dummyWindowHandle;
    VkSurfaceKHR surface;
    SwapchainSupportDetails swapchainSupportDetails;

    VULKAN_INTERNAL_LoadEntryPoints();

    dummyWindowHandle = SDL_CreateWindow(
        "SDL_Gpu Vulkan",
        128,
        128,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);

    if (dummyWindowHandle == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: Could not create dummy window");
        return SDL_FALSE;
    }

    if (!VULKAN_INTERNAL_CreateInstance(renderer, dummyWindowHandle)) {
        SDL_DestroyWindow(dummyWindowHandle);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: Could not create Vulkan instance");
        return SDL_FALSE;
    }

    if (SDL_Vulkan_CreateSurface(
            dummyWindowHandle,
            renderer->instance,
            NULL, /* FIXME: VAllocationCallbacks */
            &surface) < 0) {
        SDL_DestroyWindow(dummyWindowHandle);
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "SDL_Vulkan_CreateSurface failed: %s",
            SDL_GetError());
        return SDL_FALSE;
    }

#define VULKAN_INSTANCE_FUNCTION(ext, ret, func, params) \
    renderer->func = (vkfntype_##func)vkGetInstanceProcAddr(renderer->instance, #func);
#include "SDL_gpu_vulkan_vkfuncs.h"

    if (!VULKAN_INTERNAL_DeterminePhysicalDevice(renderer, surface)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: Failed to determine a suitable physical device");

        SDL_Vulkan_DestroySurface(
            renderer->instance,
            surface,
            NULL);

        SDL_DestroyWindow(dummyWindowHandle);
        return SDL_FALSE;
    }

    if (!VULKAN_INTERNAL_QuerySwapchainSupport(
            renderer,
            renderer->physicalDevice,
            surface,
            &swapchainSupportDetails)) {
        SDL_Vulkan_DestroySurface(
            renderer->instance,
            surface,
            NULL);

        SDL_DestroyWindow(dummyWindowHandle);
        return SDL_FALSE;
    }

    SDL_free(swapchainSupportDetails.formats);
    SDL_free(swapchainSupportDetails.presentModes);

    SDL_Vulkan_DestroySurface(
        renderer->instance,
        surface,
        NULL);

    SDL_DestroyWindow(dummyWindowHandle);
    return SDL_TRUE;
}

static SDL_bool VULKAN_PrepareDriver(SDL_VideoDevice *_this)
{
    /* Set up dummy VulkanRenderer */
    VulkanRenderer *renderer;
    Uint8 result;

    if (_this->Vulkan_CreateSurface == NULL) {
        return SDL_FALSE;
    }

    if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
        return SDL_FALSE;
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

static SDL_GpuDevice *VULKAN_CreateDevice(SDL_bool debugMode)
{
    VulkanRenderer *renderer;

    SDL_GpuDevice *result;
    VkResult vulkanResult;
    Uint32 i;

    /* Variables: Image Format Detection */
    VkImageFormatProperties imageFormatProperties;

    /* Variables: Device Feature Checks */
    VkPhysicalDeviceFeatures physicalDeviceFeatures;

    if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
        SDL_assert(!"This should have failed in PrepareDevice first!");
        return NULL;
    }

    renderer = (VulkanRenderer *)SDL_malloc(sizeof(VulkanRenderer));
    SDL_memset(renderer, '\0', sizeof(VulkanRenderer));
    renderer->debugMode = debugMode;

    if (!VULKAN_INTERNAL_PrepareVulkan(renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Vulkan!");
        SDL_free(renderer);
        SDL_Vulkan_UnloadLibrary();
        return NULL;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_Gpu Driver: Vulkan");
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION,
        "Vulkan Device: %s",
        renderer->physicalDeviceProperties.properties.deviceName);
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION,
        "Vulkan Driver: %s %s",
        renderer->physicalDeviceDriverProperties.driverName,
        renderer->physicalDeviceDriverProperties.driverInfo);
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION,
        "Vulkan Conformance: %u.%u.%u",
        renderer->physicalDeviceDriverProperties.conformanceVersion.major,
        renderer->physicalDeviceDriverProperties.conformanceVersion.minor,
        renderer->physicalDeviceDriverProperties.conformanceVersion.patch);

    if (!VULKAN_INTERNAL_CreateLogicalDevice(
            renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create logical device");
        SDL_free(renderer);
        SDL_Vulkan_UnloadLibrary();
        return NULL;
    }

    /* FIXME: just move this into this function */
    result = (SDL_GpuDevice *)SDL_malloc(sizeof(SDL_GpuDevice));
    ASSIGN_DRIVER(VULKAN)

    result->driverData = (SDL_GpuRenderer *)renderer;

    /*
     * Create initial swapchain array
     */

    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindowCount = 0;
    renderer->claimedWindows = SDL_malloc(
        renderer->claimedWindowCapacity * sizeof(WindowData *));

    /* Threading */

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

    /* Memory Allocator */

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

    /* Create uniform buffer pool */

    renderer->uniformBufferPoolCount = 32;
    renderer->uniformBufferPoolCapacity = 32;
    renderer->uniformBufferPool = SDL_malloc(
        renderer->uniformBufferPoolCapacity * sizeof(VulkanUniformBuffer *));

    for (i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i] = VULKAN_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    /* Device limits */

    renderer->minUBOAlignment = (Uint32)renderer->physicalDeviceProperties.properties.limits.minUniformBufferOffsetAlignment;

    /* Initialize caches */

    for (i = 0; i < NUM_COMMAND_POOL_BUCKETS; i += 1) {
        renderer->commandPoolHashTable.buckets[i].elements = NULL;
        renderer->commandPoolHashTable.buckets[i].count = 0;
        renderer->commandPoolHashTable.buckets[i].capacity = 0;
    }

    renderer->renderPassHashArray.elements = NULL;
    renderer->renderPassHashArray.count = 0;
    renderer->renderPassHashArray.capacity = 0;

    renderer->framebufferHashArray.elements = NULL;
    renderer->framebufferHashArray.count = 0;
    renderer->framebufferHashArray.capacity = 0;

    /* Initialize fence pool */

    renderer->fencePool.lock = SDL_CreateMutex();

    renderer->fencePool.availableFenceCapacity = 4;
    renderer->fencePool.availableFenceCount = 0;
    renderer->fencePool.availableFences = SDL_malloc(
        renderer->fencePool.availableFenceCapacity * sizeof(VulkanFenceHandle *));

    /* Some drivers don't support D16, so we have to fall back to D32. */

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

    /* Deferred destroy storage */

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

    /* Defrag state */

    renderer->defragInProgress = 0;

    renderer->allocationsToDefragCount = 0;
    renderer->allocationsToDefragCapacity = 4;
    renderer->allocationsToDefrag = SDL_malloc(
        renderer->allocationsToDefragCapacity * sizeof(VulkanMemoryAllocation *));

    /* Support checks */

    renderer->vkGetPhysicalDeviceFeatures(
        renderer->physicalDevice,
        &physicalDeviceFeatures);

    return result;
}

SDL_GpuDriver VulkanDriver = {
    "Vulkan",
    SDL_GPU_BACKEND_VULKAN,
    VULKAN_PrepareDriver,
    VULKAN_CreateDevice
};

#endif /* SDL_GPU_VULKAN */
