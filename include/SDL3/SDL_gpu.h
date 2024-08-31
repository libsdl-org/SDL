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

/* WIKI CATEGORY: GPU */

/**
 * # CategoryGPU
 *
 * Include file for SDL GPU API functions
 */

#ifndef SDL_gpu_h_
#define SDL_gpu_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type Declarations */

typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct SDL_GPUBuffer SDL_GPUBuffer;
typedef struct SDL_GPUTransferBuffer SDL_GPUTransferBuffer;
typedef struct SDL_GPUTexture SDL_GPUTexture;
typedef struct SDL_GPUSampler SDL_GPUSampler;
typedef struct SDL_GPUShader SDL_GPUShader;
typedef struct SDL_GPUComputePipeline SDL_GPUComputePipeline;
typedef struct SDL_GPUGraphicsPipeline SDL_GPUGraphicsPipeline;
typedef struct SDL_GPUCommandBuffer SDL_GPUCommandBuffer;
typedef struct SDL_GPURenderPass SDL_GPURenderPass;
typedef struct SDL_GPUComputePass SDL_GPUComputePass;
typedef struct SDL_GPUCopyPass SDL_GPUCopyPass;
typedef struct SDL_GPUFence SDL_GPUFence;

typedef enum SDL_GPUPrimitiveType
{
    SDL_GPU_PRIMITIVETYPE_POINTLIST,
    SDL_GPU_PRIMITIVETYPE_LINELIST,
    SDL_GPU_PRIMITIVETYPE_LINESTRIP,
    SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP
} SDL_GPUPrimitiveType;

typedef enum SDL_GPULoadOp
{
    SDL_GPU_LOADOP_LOAD,
    SDL_GPU_LOADOP_CLEAR,
    SDL_GPU_LOADOP_DONT_CARE
} SDL_GPULoadOp;

typedef enum SDL_GPUStoreOp
{
    SDL_GPU_STOREOP_STORE,
    SDL_GPU_STOREOP_DONT_CARE
} SDL_GPUStoreOp;

typedef enum SDL_GPUIndexElementSize
{
    SDL_GPU_INDEXELEMENTSIZE_16BIT,
    SDL_GPU_INDEXELEMENTSIZE_32BIT
} SDL_GPUIndexElementSize;

/* Texture format support varies depending on driver, hardware, and usage flags.
 * In general, you should use SDL_GPUTextureSupportsFormat to query if a format
 * is supported before using it. However, there are a few guaranteed formats.
 *
 * For SAMPLER usage, the following formats are universally supported:
 *  - R8G8B8A8_UNORM
 *  - B8G8R8A8_UNORM
 *  - R8_UNORM
 *  - R8G8_SNORM
 *  - R8G8B8A8_SNORM
 *  - R16_FLOAT
 *  - R16G16_FLOAT
 *  - R16G16B16A16_FLOAT
 *  - R32_FLOAT
 *  - R32G32_FLOAT
 *  - R32G32B32A32_FLOAT
 *  - R8G8B8A8_UNORM_SRGB
 *  - B8G8R8A8_UNORM_SRGB
 *  - D16_UNORM
 *
 * For COLOR_TARGET usage, the following formats are universally supported:
 *  - R8G8B8A8_UNORM
 *  - B8G8R8A8_UNORM
 *  - R8_UNORM
 *  - R16_FLOAT
 *  - R16G16_FLOAT
 *  - R16G16B16A16_FLOAT
 *  - R32_FLOAT
 *  - R32G32_FLOAT
 *  - R32G32B32A32_FLOAT
 *  - R8_UINT
 *  - R8G8_UINT
 *  - R8G8B8A8_UINT
 *  - R16_UINT
 *  - R16G16_UINT
 *  - R16G16B16A16_UINT
 *  - R8G8B8A8_UNORM_SRGB
 *  - B8G8R8A8_UNORM_SRGB
 *
 * For STORAGE usages, the following formats are universally supported:
 *  - R8G8B8A8_UNORM
 *  - R8G8B8A8_SNORM
 *  - R16G16B16A16_FLOAT
 *  - R32_FLOAT
 *  - R32G32_FLOAT
 *  - R32G32B32A32_FLOAT
 *  - R8_UINT
 *  - R8G8_UINT
 *  - R8G8B8A8_UINT
 *  - R16_UINT
 *  - R16G16_UINT
 *  - R16G16B16A16_UINT
 *
 * For DEPTH_STENCIL_TARGET usage, the following formats are universally supported:
 *  - D16_UNORM
 *  - Either (but not necessarily both!) D24_UNORM or D32_SFLOAT
 *  - Either (but not necessarily both!) D24_UNORM_S8_UINT or D32_SFLOAT_S8_UINT
 *
 * Unless D16_UNORM is sufficient for your purposes, always check which
 * of D24/D32 is supported before creating a depth-stencil texture!
 */
typedef enum SDL_GPUTextureFormat
{
    SDL_GPU_TEXTUREFORMAT_INVALID = -1,

    /* Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM,
    SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM,
    SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8_UNORM,
    SDL_GPU_TEXTUREFORMAT_A8_UNORM,
    /* Compressed Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC2_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC3_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC7_UNORM,
    /* Signed Normalized Float Color Formats  */
    SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
    /* Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
    /* Unsigned Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
    SDL_GPU_TEXTUREFORMAT_R16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
    /* SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
    /* Compressed SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC3_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC7_UNORM_SRGB,
    /* Depth Formats */
    SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT
} SDL_GPUTextureFormat;

typedef enum SDL_GPUTextureUsageFlagBits
{
    SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT = 0x00000001,
    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT = 0x00000002,
    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT = 0x00000004,
    SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT = 0x00000008,
    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT = 0x00000020,
    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT = 0x00000040
} SDL_GPUTextureUsageFlagBits;

typedef Uint32 SDL_GPUTextureUsageFlags;

typedef enum SDL_GPUTextureType
{
    SDL_GPU_TEXTURETYPE_2D,
    SDL_GPU_TEXTURETYPE_2D_ARRAY,
    SDL_GPU_TEXTURETYPE_3D,
    SDL_GPU_TEXTURETYPE_CUBE
} SDL_GPUTextureType;

typedef enum SDL_GPUSampleCount
{
    SDL_GPU_SAMPLECOUNT_1,
    SDL_GPU_SAMPLECOUNT_2,
    SDL_GPU_SAMPLECOUNT_4,
    SDL_GPU_SAMPLECOUNT_8
} SDL_GPUSampleCount;

typedef enum SDL_GPUCubeMapFace
{
    SDL_GPU_CUBEMAPFACE_POSITIVEX,
    SDL_GPU_CUBEMAPFACE_NEGATIVEX,
    SDL_GPU_CUBEMAPFACE_POSITIVEY,
    SDL_GPU_CUBEMAPFACE_NEGATIVEY,
    SDL_GPU_CUBEMAPFACE_POSITIVEZ,
    SDL_GPU_CUBEMAPFACE_NEGATIVEZ
} SDL_GPUCubeMapFace;

typedef enum SDL_GPUBufferUsageFlagBits
{
    SDL_GPU_BUFFERUSAGE_VERTEX_BIT = 0x00000001,
    SDL_GPU_BUFFERUSAGE_INDEX_BIT = 0x00000002,
    SDL_GPU_BUFFERUSAGE_INDIRECT_BIT = 0x00000004,
    SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT = 0x00000008,
    SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT = 0x00000020,
    SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT = 0x00000040
} SDL_GPUBufferUsageFlagBits;

typedef Uint32 SDL_GPUBufferUsageFlags;

typedef enum SDL_GPUTransferBufferUsage
{
    SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD
} SDL_GPUTransferBufferUsage;

typedef enum SDL_GPUShaderStage
{
    SDL_GPU_SHADERSTAGE_VERTEX,
    SDL_GPU_SHADERSTAGE_FRAGMENT
} SDL_GPUShaderStage;

typedef enum SDL_GPUShaderFormatFlagBits
{
    SDL_GPU_SHADERFORMAT_INVALID = 0x00000000,
    SDL_GPU_SHADERFORMAT_SECRET = 0x00000001,   /* NDA'd platforms */
    SDL_GPU_SHADERFORMAT_SPIRV = 0x00000002,    /* Vulkan */
    SDL_GPU_SHADERFORMAT_DXBC = 0x00000004,     /* D3D11 (Shader Model 5_0) */
    SDL_GPU_SHADERFORMAT_DXIL = 0x00000008,     /* D3D12 */
    SDL_GPU_SHADERFORMAT_MSL = 0x00000010,      /* Metal */
    SDL_GPU_SHADERFORMAT_METALLIB = 0x00000020, /* Metal */
} SDL_GPUShaderFormatFlagBits;

typedef Uint32 SDL_GPUShaderFormat;

typedef enum SDL_GPUVertexElementFormat
{
    /* 32-bit Signed Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_INT,
    SDL_GPU_VERTEXELEMENTFORMAT_INT2,
    SDL_GPU_VERTEXELEMENTFORMAT_INT3,
    SDL_GPU_VERTEXELEMENTFORMAT_INT4,

    /* 32-bit Unsigned Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_UINT,
    SDL_GPU_VERTEXELEMENTFORMAT_UINT2,
    SDL_GPU_VERTEXELEMENTFORMAT_UINT3,
    SDL_GPU_VERTEXELEMENTFORMAT_UINT4,

    /* 32-bit Floats */
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,

    /* 8-bit Signed Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_BYTE2,
    SDL_GPU_VERTEXELEMENTFORMAT_BYTE4,

    /* 8-bit Unsigned Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2,
    SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4,

    /* 8-bit Signed Normalized */
    SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM,
    SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,

    /* 8-bit Unsigned Normalized */
    SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM,
    SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,

    /* 16-bit Signed Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT4,

    /* 16-bit Unsigned Integers */
    SDL_GPU_VERTEXELEMENTFORMAT_USHORT2,
    SDL_GPU_VERTEXELEMENTFORMAT_USHORT4,

    /* 16-bit Signed Normalized */
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM,
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM,

    /* 16-bit Unsigned Normalized */
    SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM,
    SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM,

    /* 16-bit Floats */
    SDL_GPU_VERTEXELEMENTFORMAT_HALF2,
    SDL_GPU_VERTEXELEMENTFORMAT_HALF4
} SDL_GPUVertexElementFormat;

typedef enum SDL_GPUVertexInputRate
{
    SDL_GPU_VERTEXINPUTRATE_VERTEX = 0,
    SDL_GPU_VERTEXINPUTRATE_INSTANCE = 1
} SDL_GPUVertexInputRate;

typedef enum SDL_GPUFillMode
{
    SDL_GPU_FILLMODE_FILL,
    SDL_GPU_FILLMODE_LINE
} SDL_GPUFillMode;

typedef enum SDL_GPUCullMode
{
    SDL_GPU_CULLMODE_NONE,
    SDL_GPU_CULLMODE_FRONT,
    SDL_GPU_CULLMODE_BACK
} SDL_GPUCullMode;

typedef enum SDL_GPUFrontFace
{
    SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    SDL_GPU_FRONTFACE_CLOCKWISE
} SDL_GPUFrontFace;

typedef enum SDL_GPUCompareOp
{
    SDL_GPU_COMPAREOP_NEVER,
    SDL_GPU_COMPAREOP_LESS,
    SDL_GPU_COMPAREOP_EQUAL,
    SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    SDL_GPU_COMPAREOP_GREATER,
    SDL_GPU_COMPAREOP_NOT_EQUAL,
    SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
    SDL_GPU_COMPAREOP_ALWAYS
} SDL_GPUCompareOp;

typedef enum SDL_GPUStencilOp
{
    SDL_GPU_STENCILOP_KEEP,
    SDL_GPU_STENCILOP_ZERO,
    SDL_GPU_STENCILOP_REPLACE,
    SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP,
    SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP,
    SDL_GPU_STENCILOP_INVERT,
    SDL_GPU_STENCILOP_INCREMENT_AND_WRAP,
    SDL_GPU_STENCILOP_DECREMENT_AND_WRAP
} SDL_GPUStencilOp;

typedef enum SDL_GPUBlendOp
{
    SDL_GPU_BLENDOP_ADD,
    SDL_GPU_BLENDOP_SUBTRACT,
    SDL_GPU_BLENDOP_REVERSE_SUBTRACT,
    SDL_GPU_BLENDOP_MIN,
    SDL_GPU_BLENDOP_MAX
} SDL_GPUBlendOp;

typedef enum SDL_GPUBlendFactor
{
    SDL_GPU_BLENDFACTOR_ZERO,
    SDL_GPU_BLENDFACTOR_ONE,
    SDL_GPU_BLENDFACTOR_SRC_COLOR,
    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
    SDL_GPU_BLENDFACTOR_DST_COLOR,
    SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,
    SDL_GPU_BLENDFACTOR_SRC_ALPHA,
    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    SDL_GPU_BLENDFACTOR_DST_ALPHA,
    SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
    SDL_GPU_BLENDFACTOR_CONSTANT_COLOR,
    SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR,
    SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE
} SDL_GPUBlendFactor;

typedef enum SDL_GPUColorComponentFlagBits
{
    SDL_GPU_COLORCOMPONENT_R_BIT = 0x00000001,
    SDL_GPU_COLORCOMPONENT_G_BIT = 0x00000002,
    SDL_GPU_COLORCOMPONENT_B_BIT = 0x00000004,
    SDL_GPU_COLORCOMPONENT_A_BIT = 0x00000008
} SDL_GPUColorComponentFlagBits;

typedef Uint8 SDL_GPUColorComponentFlags;

typedef enum SDL_GPUFilter
{
    SDL_GPU_FILTER_NEAREST,
    SDL_GPU_FILTER_LINEAR
} SDL_GPUFilter;

typedef enum SDL_GPUSamplerMipmapMode
{
    SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
} SDL_GPUSamplerMipmapMode;

typedef enum SDL_GPUSamplerAddressMode
{
    SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
    SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
} SDL_GPUSamplerAddressMode;

/*
 * VSYNC:
 *   Waits for vblank before presenting.
 *   If there is a pending image to present, the new image is enqueued for presentation.
 *   Disallows tearing at the cost of visual latency.
 *   When using this present mode, AcquireSwapchainTexture will block if too many frames are in flight.
 * IMMEDIATE:
 *   Immediately presents.
 *   Lowest latency option, but tearing may occur.
 *   When using this mode, AcquireSwapchainTexture will return NULL if too many frames are in flight.
 * MAILBOX:
 *   Waits for vblank before presenting. No tearing is possible.
 *   If there is a pending image to present, the pending image is replaced by the new image.
 *   Similar to VSYNC, but with reduced visual latency.
 *   When using this mode, AcquireSwapchainTexture will return NULL if too many frames are in flight.
 */
typedef enum SDL_GPUPresentMode
{
    SDL_GPU_PRESENTMODE_VSYNC,
    SDL_GPU_PRESENTMODE_IMMEDIATE,
    SDL_GPU_PRESENTMODE_MAILBOX
} SDL_GPUPresentMode;

/*
 * SDR:
 *   B8G8R8A8 or R8G8B8A8 swapchain. Pixel values are in nonlinear sRGB encoding. Blends raw pixel values.
 * SDR_LINEAR:
 *   B8G8R8A8_SRGB or R8G8B8A8_SRGB swapchain. Pixel values are in nonlinear sRGB encoding. Blends in linear space.
 * HDR_EXTENDED_LINEAR:
 *   R16G16B16A16_SFLOAT swapchain. Pixel values are in extended linear encoding. Blends in linear space.
 * HDR10_ST2048:
 *   A2R10G10B10 or A2B10G10R10 swapchain. Pixel values are in PQ ST2048 encoding. Blends raw pixel values. (TODO: verify this)
 */
typedef enum SDL_GPUSwapchainComposition
{
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048
} SDL_GPUSwapchainComposition;

typedef enum SDL_GPUDriver
{
    SDL_GPU_DRIVER_INVALID = -1,
    SDL_GPU_DRIVER_SECRET, /* NDA'd platforms */
    SDL_GPU_DRIVER_VULKAN,
    SDL_GPU_DRIVER_D3D11,
    SDL_GPU_DRIVER_D3D12,
    SDL_GPU_DRIVER_METAL
} SDL_GPUDriver;

/* Structures */

typedef struct SDL_GPUDepthStencilValue
{
    float depth;
    Uint8 stencil;
} SDL_GPUDepthStencilValue;

typedef struct SDL_GPUViewport
{
    float x;
    float y;
    float w;
    float h;
    float minDepth;
    float maxDepth;
} SDL_GPUViewport;

typedef struct SDL_GPUTextureTransferInfo
{
    SDL_GPUTransferBuffer *transferBuffer;
    Uint32 offset;      /* starting location of the image data */
    Uint32 imagePitch;  /* number of pixels from one row to the next */
    Uint32 imageHeight; /* number of rows from one layer/depth-slice to the next */
} SDL_GPUTextureTransferInfo;

typedef struct SDL_GPUTransferBufferLocation
{
    SDL_GPUTransferBuffer *transferBuffer;
    Uint32 offset;
} SDL_GPUTransferBufferLocation;

typedef struct SDL_GPUTextureLocation
{
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;
    Uint32 x;
    Uint32 y;
    Uint32 z;
} SDL_GPUTextureLocation;

typedef struct SDL_GPUTextureRegion
{
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;
    Uint32 x;
    Uint32 y;
    Uint32 z;
    Uint32 w;
    Uint32 h;
    Uint32 d;
} SDL_GPUTextureRegion;

typedef struct SDL_GPUBlitRegion
{
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layerOrDepthPlane;
    Uint32 x;
    Uint32 y;
    Uint32 w;
    Uint32 h;
} SDL_GPUBlitRegion;

typedef struct SDL_GPUBufferLocation
{
    SDL_GPUBuffer *buffer;
    Uint32 offset;
} SDL_GPUBufferLocation;

typedef struct SDL_GPUBufferRegion
{
    SDL_GPUBuffer *buffer;
    Uint32 offset;
    Uint32 size;
} SDL_GPUBufferRegion;

/* Note that the `firstVertex` and `firstInstance` parameters are NOT compatible with
 * built-in vertex/instance ID variables in shaders (for example, SV_VertexID). If
 * your shader depends on these variables, the correlating draw call parameter MUST
 * be 0.
 */
typedef struct SDL_GPUIndirectDrawCommand
{
    Uint32 vertexCount;   /* number of vertices to draw */
    Uint32 instanceCount; /* number of instances to draw */
    Uint32 firstVertex;   /* index of the first vertex to draw */
    Uint32 firstInstance; /* ID of the first instance to draw */
} SDL_GPUIndirectDrawCommand;

typedef struct SDL_GPUIndexedIndirectDrawCommand
{
    Uint32 indexCount;    /* number of vertices to draw per instance */
    Uint32 instanceCount; /* number of instances to draw */
    Uint32 firstIndex;    /* base index within the index buffer */
    Sint32 vertexOffset;  /* value added to vertex index before indexing into the vertex buffer */
    Uint32 firstInstance; /* ID of the first instance to draw */
} SDL_GPUIndexedIndirectDrawCommand;

typedef struct SDL_GPUIndirectDispatchCommand
{
    Uint32 groupCountX;
    Uint32 groupCountY;
    Uint32 groupCountZ;
} SDL_GPUIndirectDispatchCommand;

/* State structures */

typedef struct SDL_GPUSamplerCreateInfo
{
    SDL_GPUFilter minFilter;
    SDL_GPUFilter magFilter;
    SDL_GPUSamplerMipmapMode mipmapMode;
    SDL_GPUSamplerAddressMode addressModeU;
    SDL_GPUSamplerAddressMode addressModeV;
    SDL_GPUSamplerAddressMode addressModeW;
    float mipLodBias;
    SDL_bool anisotropyEnable;
    float maxAnisotropy;
    SDL_bool compareEnable;
    SDL_GPUCompareOp compareOp;
    float minLod;
    float maxLod;

    SDL_PropertiesID props;
} SDL_GPUSamplerCreateInfo;

typedef struct SDL_GPUVertexBinding
{
    Uint32 binding;
    Uint32 stride;
    SDL_GPUVertexInputRate inputRate;
    Uint32 instanceStepRate; /* ignored unless inputRate is INSTANCE */
} SDL_GPUVertexBinding;

typedef struct SDL_GPUVertexAttribute
{
    Uint32 location;
    Uint32 binding;
    SDL_GPUVertexElementFormat format;
    Uint32 offset;
} SDL_GPUVertexAttribute;

typedef struct SDL_GPUVertexInputState
{
    const SDL_GPUVertexBinding *vertexBindings;
    Uint32 vertexBindingCount;
    const SDL_GPUVertexAttribute *vertexAttributes;
    Uint32 vertexAttributeCount;
} SDL_GPUVertexInputState;

typedef struct SDL_GPUStencilOpState
{
    SDL_GPUStencilOp failOp;
    SDL_GPUStencilOp passOp;
    SDL_GPUStencilOp depthFailOp;
    SDL_GPUCompareOp compareOp;
} SDL_GPUStencilOpState;

typedef struct SDL_GPUColorAttachmentBlendState
{
    SDL_bool blendEnable;
    SDL_GPUBlendFactor srcColorBlendFactor;
    SDL_GPUBlendFactor dstColorBlendFactor;
    SDL_GPUBlendOp colorBlendOp;
    SDL_GPUBlendFactor srcAlphaBlendFactor;
    SDL_GPUBlendFactor dstAlphaBlendFactor;
    SDL_GPUBlendOp alphaBlendOp;
    SDL_GPUColorComponentFlags colorWriteMask;
} SDL_GPUColorAttachmentBlendState;

typedef struct SDL_GPUShaderCreateInfo
{
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GPUShaderFormat format;
    SDL_GPUShaderStage stage;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;

    SDL_PropertiesID props;
} SDL_GPUShaderCreateInfo;

typedef struct SDL_GPUTextureCreateInfo
{
    SDL_GPUTextureType type;
    SDL_GPUTextureFormat format;
    SDL_GPUTextureUsageFlags usageFlags;
    Uint32 width;
    Uint32 height;
    Uint32 layerCountOrDepth;
    Uint32 levelCount;
    SDL_GPUSampleCount sampleCount;

    SDL_PropertiesID props;
} SDL_GPUTextureCreateInfo;

#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_R_FLOAT       "SDL.gpu.createtexture.d3d12.clear.r"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_G_FLOAT       "SDL.gpu.createtexture.d3d12.clear.g"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_B_FLOAT       "SDL.gpu.createtexture.d3d12.clear.b"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_A_FLOAT       "SDL.gpu.createtexture.d3d12.clear.a"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_DEPTH_FLOAT   "SDL.gpu.createtexture.d3d12.clear.depth"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_STENCIL_UINT8 "SDL.gpu.createtexture.d3d12.clear.stencil"

typedef struct SDL_GPUBufferCreateInfo
{
    SDL_GPUBufferUsageFlags usageFlags;
    Uint32 sizeInBytes;

    SDL_PropertiesID props;
} SDL_GPUBufferCreateInfo;

typedef struct SDL_GPUTransferBufferCreateInfo
{
    SDL_GPUTransferBufferUsage usage;
    Uint32 sizeInBytes;

    SDL_PropertiesID props;
} SDL_GPUTransferBufferCreateInfo;

/* Pipeline state structures */

typedef struct SDL_GPURasterizerState
{
    SDL_GPUFillMode fillMode;
    SDL_GPUCullMode cullMode;
    SDL_GPUFrontFace frontFace;
    SDL_bool depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
} SDL_GPURasterizerState;

typedef struct SDL_GPUMultisampleState
{
    SDL_GPUSampleCount sampleCount;
    Uint32 sampleMask;
} SDL_GPUMultisampleState;

typedef struct SDL_GPUDepthStencilState
{
    SDL_bool depthTestEnable;
    SDL_bool depthWriteEnable;
    SDL_GPUCompareOp compareOp;
    SDL_bool stencilTestEnable;
    SDL_GPUStencilOpState backStencilState;
    SDL_GPUStencilOpState frontStencilState;
    Uint8 compareMask;
    Uint8 writeMask;
    Uint8 reference;
} SDL_GPUDepthStencilState;

typedef struct SDL_GPUColorAttachmentDescription
{
    SDL_GPUTextureFormat format;
    SDL_GPUColorAttachmentBlendState blendState;
} SDL_GPUColorAttachmentDescription;

typedef struct SDL_GPUGraphicsPipelineAttachmentInfo
{
    SDL_GPUColorAttachmentDescription *colorAttachmentDescriptions;
    Uint32 colorAttachmentCount;
    SDL_bool hasDepthStencilAttachment;
    SDL_GPUTextureFormat depthStencilFormat;
} SDL_GPUGraphicsPipelineAttachmentInfo;

typedef struct SDL_GPUGraphicsPipelineCreateInfo
{
    SDL_GPUShader *vertexShader;
    SDL_GPUShader *fragmentShader;
    SDL_GPUVertexInputState vertexInputState;
    SDL_GPUPrimitiveType primitiveType;
    SDL_GPURasterizerState rasterizerState;
    SDL_GPUMultisampleState multisampleState;
    SDL_GPUDepthStencilState depthStencilState;
    SDL_GPUGraphicsPipelineAttachmentInfo attachmentInfo;
    float blendConstants[4];

    SDL_PropertiesID props;
} SDL_GPUGraphicsPipelineCreateInfo;

typedef struct SDL_GPUComputePipelineCreateInfo
{
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GPUShaderFormat format;
    Uint32 readOnlyStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 writeOnlyStorageTextureCount;
    Uint32 writeOnlyStorageBufferCount;
    Uint32 uniformBufferCount;
    Uint32 threadCountX;
    Uint32 threadCountY;
    Uint32 threadCountZ;

    SDL_PropertiesID props;
} SDL_GPUComputePipelineCreateInfo;

typedef struct SDL_GPUColorAttachmentInfo
{
    /* The texture that will be used as a color attachment by a render pass. */
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layerOrDepthPlane; /* For 3D textures, you can bind an individual depth plane as an attachment. */

    /* Can be ignored by RenderPass if CLEAR is not used */
    SDL_FColor clearColor;

    /* Determines what is done with the texture at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the data currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single color.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp loadOp;

    /* Determines what is done with the texture at the end of the render pass.
     *
     *   STORE:
     *     Stores the results of the render pass in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp storeOp;

    /* if SDL_TRUE, cycles the texture if the texture is bound and loadOp is not LOAD */
    SDL_bool cycle;
} SDL_GPUColorAttachmentInfo;

typedef struct SDL_GPUDepthStencilAttachmentInfo
{
    /* The texture that will be used as the depth stencil attachment by a render pass. */
    SDL_GPUTexture *texture;

    /* Can be ignored by the render pass if CLEAR is not used */
    SDL_GPUDepthStencilValue depthStencilClearValue;

    /* Determines what is done with the depth values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the depth values currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single depth.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp loadOp;

    /* Determines what is done with the depth values at the end of the render pass.
     *
     *   STORE:
     *     Stores the depth results in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp storeOp;

    /* Determines what is done with the stencil values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the stencil values currently in the texture.
     *
     *   CLEAR:
     *     Clears the texture to a single stencil value.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GPULoadOp stencilLoadOp;

    /* Determines what is done with the stencil values at the end of the render pass.
     *
     *   STORE:
     *     Stores the stencil results in the texture.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GPUStoreOp stencilStoreOp;

    /* if SDL_TRUE, cycles the texture if the texture is bound and any load ops are not LOAD */
    SDL_bool cycle;
} SDL_GPUDepthStencilAttachmentInfo;

/* Binding structs */

typedef struct SDL_GPUBufferBinding
{
    SDL_GPUBuffer *buffer;
    Uint32 offset;
} SDL_GPUBufferBinding;

typedef struct SDL_GPUTextureSamplerBinding
{
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
} SDL_GPUTextureSamplerBinding;

typedef struct SDL_GPUStorageBufferWriteOnlyBinding
{
    SDL_GPUBuffer *buffer;

    /* if SDL_TRUE, cycles the buffer if it is bound. */
    SDL_bool cycle;
} SDL_GPUStorageBufferWriteOnlyBinding;

typedef struct SDL_GPUStorageTextureWriteOnlyBinding
{
    SDL_GPUTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;

    /* if SDL_TRUE, cycles the texture if the texture is bound. */
    SDL_bool cycle;
} SDL_GPUStorageTextureWriteOnlyBinding;

/* Functions */

/* Device */

/**
 * Creates a GPU context.
 *
 * \param formatFlags a bitflag indicating which shader formats the app is
 *                    able to provide.
 * \param debugMode enable debug mode properties and validations.
 * \param name the preferred GPU driver, or NULL to let SDL pick the optimal
 *             driver.
 * \returns a GPU context on success or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGPUDriver
 * \sa SDL_DestroyGPUDevice
 */
extern SDL_DECLSPEC SDL_GPUDevice *SDLCALL SDL_CreateGPUDevice(
    SDL_GPUShaderFormat formatFlags,
    SDL_bool debugMode,
    const char *name);

/**
 * Creates a GPU context.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOL`: enable debug mode properties
 *   and validations, defaults to SDL_TRUE.
 * - `SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOL`: enable to prefer energy
 *   efficiency over maximum GPU performance, defaults to SDL_FALSE.
 * - `SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING`: the name of the GPU driver to
 *   use, if a specific one is desired.
 *
 * These are the current shader format properties:
 *
 * - `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SECRET_BOOL`: The app is able to
 *   provide shaders for an NDA platform.
 * - `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOL`: The app is able to
 *   provide SPIR-V shaders if applicable.
 * - SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOL`: The app is able to provide
 *   DXBC shaders if applicable
 *   `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOL`: The app is able to
 *   provide DXIL shaders if applicable.
 * - `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOL`: The app is able to provide
 *   MSL shaders if applicable.
 * - `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOL`: The app is able to
 *   provide Metal shader libraries if applicable.
 *
 * With the D3D12 renderer:
 *
 * - `SDL_PROP_GPU_DEVICE_CREATE_D3D12_SEMANTIC_NAME_STRING`: the prefix to
 *   use for all vertex semantics, default is "TEXCOORD".
 *
 * \param props the properties to use.
 * \returns a GPU context on success or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGPUDriver
 * \sa SDL_DestroyGPUDevice
 */
extern SDL_DECLSPEC SDL_GPUDevice *SDLCALL SDL_CreateGPUDeviceWithProperties(
    SDL_PropertiesID props);

#define SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOL             "SDL.gpu.device.create.debugmode"
#define SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOL        "SDL.gpu.device.create.preferlowpower"
#define SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING                "SDL.gpu.device.create.name"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SECRET_BOOL        "SDL.gpu.device.create.shaders.secret"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOL         "SDL.gpu.device.create.shaders.spirv"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOL          "SDL.gpu.device.create.shaders.dxbc"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOL          "SDL.gpu.device.create.shaders.dxil"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOL           "SDL.gpu.device.create.shaders.msl"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOL      "SDL.gpu.device.create.shaders.metallib"
#define SDL_PROP_GPU_DEVICE_CREATE_D3D12_SEMANTIC_NAME_STRING "SDL.gpu.device.create.d3d12.semantic"

/**
 * Destroys a GPU context previously returned by SDL_CreateGPUDevice.
 *
 * \param device a GPU Context to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUDevice
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyGPUDevice(SDL_GPUDevice *device);

/**
 * Returns the backend used to create this GPU context.
 *
 * \param device a GPU context to query.
 * \returns an SDL_GPUDriver value, or SDL_GPU_DRIVER_INVALID on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_GPUDriver SDLCALL SDL_GetGPUDriver(SDL_GPUDevice *device);

/* State Creation */

/**
 * Creates a pipeline object to be used in a compute workflow.
 *
 * Shader resource bindings must be authored to follow a particular order. For
 * SPIR-V shaders, use the following resource sets: 0: Read-only storage
 * textures, followed by read-only storage buffers 1: Write-only storage
 * textures, followed by write-only storage buffers 2: Uniform buffers
 *
 * For DXBC Shader Model 5_0 shaders, use the following register order: For t
 * registers: Read-only storage textures, followed by read-only storage
 * buffers For u registers: Write-only storage textures, followed by
 * write-only storage buffers For b registers: Uniform buffers
 *
 * For DXIL shaders, use the following register order: (t[n], space0):
 * Read-only storage textures, followed by read-only storage buffers (u[n],
 * space1): Write-only storage textures, followed by write-only storage
 * buffers (b[n], space2): Uniform buffers
 *
 * For MSL/metallib, use the following order: For [[buffer]]: Uniform buffers,
 * followed by write-only storage buffers, followed by write-only storage
 * buffers For [[texture]]: Read-only storage textures, followed by write-only
 * storage textures
 *
 * \param device a GPU Context.
 * \param computePipelineCreateInfo a struct describing the state of the
 *                                  requested compute pipeline.
 * \returns a compute pipeline object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BindGPUComputePipeline
 * \sa SDL_ReleaseGPUComputePipeline
 */
extern SDL_DECLSPEC SDL_GPUComputePipeline *SDLCALL SDL_CreateGPUComputePipeline(
    SDL_GPUDevice *device,
    SDL_GPUComputePipelineCreateInfo *computePipelineCreateInfo);

/**
 * Creates a pipeline object to be used in a graphics workflow.
 *
 * \param device a GPU Context.
 * \param pipelineCreateInfo a struct describing the state of the desired
 *                           graphics pipeline.
 * \returns a graphics pipeline object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUShader
 * \sa SDL_BindGPUGraphicsPipeline
 * \sa SDL_ReleaseGPUGraphicsPipeline
 */
extern SDL_DECLSPEC SDL_GPUGraphicsPipeline *SDLCALL SDL_CreateGPUGraphicsPipeline(
    SDL_GPUDevice *device,
    SDL_GPUGraphicsPipelineCreateInfo *pipelineCreateInfo);

/**
 * Creates a sampler object to be used when binding textures in a graphics
 * workflow.
 *
 * \param device a GPU Context.
 * \param samplerCreateInfo a struct describing the state of the desired
 *                          sampler.
 * \returns a sampler object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BindGPUVertexSamplers
 * \sa SDL_BindGPUFragmentSamplers
 * \sa SDL_ReleaseSampler
 */
extern SDL_DECLSPEC SDL_GPUSampler *SDLCALL SDL_CreateGPUSampler(
    SDL_GPUDevice *device,
    SDL_GPUSamplerCreateInfo *samplerCreateInfo);

/**
 * Creates a shader to be used when creating a graphics pipeline.
 *
 * Shader resource bindings must be authored to follow a particular order
 * depending on the shader format.
 *
 * For SPIR-V shaders, use the following resource sets: For vertex shaders: 0:
 * Sampled textures, followed by storage textures, followed by storage buffers
 * 1: Uniform buffers For fragment shaders: 2: Sampled textures, followed by
 * storage textures, followed by storage buffers 3: Uniform buffers
 *
 * For DXBC Shader Model 5_0 shaders, use the following register order: For t
 * registers: Sampled textures, followed by storage textures, followed by
 * storage buffers For s registers: Samplers with indices corresponding to the
 * sampled textures For b registers: Uniform buffers
 *
 * For DXIL shaders, use the following register order: For vertex shaders:
 * (t[n], space0): Sampled textures, followed by storage textures, followed by
 * storage buffers (s[n], space0): Samplers with indices corresponding to the
 * sampled textures (b[n], space1): Uniform buffers For pixel shaders: (t[n],
 * space2): Sampled textures, followed by storage textures, followed by
 * storage buffers (s[n], space2): Samplers with indices corresponding to the
 * sampled textures (b[n], space3): Uniform buffers
 *
 * For MSL/metallib, use the following order: For [[texture]]: Sampled
 * textures, followed by storage textures For [[sampler]]: Samplers with
 * indices corresponding to the sampled textures For [[buffer]]: Uniform
 * buffers, followed by storage buffers. Vertex buffer 0 is bound at
 * [[buffer(30)]], vertex buffer 1 at [[buffer(29)]], and so on. Rather than
 * manually authoring vertex buffer indices, use the [[stage_in]] attribute
 * which will automatically use the vertex input information from the
 * SDL_GPUPipeline.
 *
 * \param device a GPU Context.
 * \param shaderCreateInfo a struct describing the state of the desired
 *                         shader.
 * \returns a shader object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 * \sa SDL_ReleaseGPUShader
 */
extern SDL_DECLSPEC SDL_GPUShader *SDLCALL SDL_CreateGPUShader(
    SDL_GPUDevice *device,
    SDL_GPUShaderCreateInfo *shaderCreateInfo);

/**
 * Creates a texture object to be used in graphics or compute workflows.
 *
 * The contents of this texture are undefined until data is written to the
 * texture.
 *
 * Note that certain combinations of usage flags are invalid. For example, a
 * texture cannot have both the SAMPLER and GRAPHICS_STORAGE_READ flags.
 *
 * If you request a sample count higher than the hardware supports, the
 * implementation will automatically fall back to the highest available sample
 * count.
 *
 * \param device a GPU Context.
 * \param textureCreateInfo a struct describing the state of the texture to
 *                          create.
 * \returns a texture object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUTexture
 * \sa SDL_BindGPUVertexSamplers
 * \sa SDL_BindGPUVertexStorageTextures
 * \sa SDL_BindGPUFragmentSamplers
 * \sa SDL_BindGPUFragmentStorageTextures
 * \sa SDL_BindGPUComputeStorageTextures
 * \sa SDL_BlitGPUTexture
 * \sa SDL_ReleaseGPUTexture
 * \sa SDL_GPUTextureSupportsFormat
 */
extern SDL_DECLSPEC SDL_GPUTexture *SDLCALL SDL_CreateGPUTexture(
    SDL_GPUDevice *device,
    SDL_GPUTextureCreateInfo *textureCreateInfo);

/**
 * Creates a buffer object to be used in graphics or compute workflows.
 *
 * The contents of this buffer are undefined until data is written to the
 * buffer.
 *
 * Note that certain combinations of usage flags are invalid. For example, a
 * buffer cannot have both the VERTEX and INDEX flags.
 *
 * \param device a GPU Context.
 * \param bufferCreateInfo a struct describing the state of the buffer to
 *                         create.
 * \returns a buffer object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_BindGPUVertexBuffers
 * \sa SDL_BindGPUIndexBuffer
 * \sa SDL_BindGPUVertexStorageBuffers
 * \sa SDL_BindGPUFragmentStorageBuffers
 * \sa SDL_BindGPUComputeStorageBuffers
 * \sa SDL_ReleaseGPUBuffer
 */
extern SDL_DECLSPEC SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(
    SDL_GPUDevice *device,
    SDL_GPUBufferCreateInfo *bufferCreateInfo);

/**
 * Creates a transfer buffer to be used when uploading to or downloading from
 * graphics resources.
 *
 * \param device a GPU Context.
 * \param transferBufferCreateInfo a struct describing the state of the
 *                                 transfer buffer to create.
 * \returns a transfer buffer on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_DownloadFromGPUBuffer
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUTexture
 * \sa SDL_ReleaseGPUTransferBuffer
 */
extern SDL_DECLSPEC SDL_GPUTransferBuffer *SDLCALL SDL_CreateGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBufferCreateInfo *transferBufferCreateInfo);

/* Debug Naming */

/**
 * Sets an arbitrary string constant to label a buffer.
 *
 * Useful for debugging.
 *
 * \param device a GPU Context.
 * \param buffer a buffer to attach the name to.
 * \param text a UTF-8 string constant to mark as the name of the buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUBufferName(
    SDL_GPUDevice *device,
    SDL_GPUBuffer *buffer,
    const char *text);

/**
 * Sets an arbitrary string constant to label a texture.
 *
 * Useful for debugging.
 *
 * \param device a GPU Context.
 * \param texture a texture to attach the name to.
 * \param text a UTF-8 string constant to mark as the name of the texture.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUTextureName(
    SDL_GPUDevice *device,
    SDL_GPUTexture *texture,
    const char *text);

/**
 * Inserts an arbitrary string label into the command buffer callstream.
 *
 * Useful for debugging.
 *
 * \param commandBuffer a command buffer.
 * \param text a UTF-8 string constant to insert as the label.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_InsertGPUDebugLabel(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *text);

/**
 * Begins a debug group with an arbitary name.
 *
 * Used for denoting groups of calls when viewing the command buffer
 * callstream in a graphics debugging tool.
 *
 * Each call to SDL_PushGPUDebugGroup must have a corresponding call to
 * SDL_PopGPUDebugGroup.
 *
 * On some backends (e.g. Metal), pushing a debug group during a
 * render/blit/compute pass will create a group that is scoped to the native
 * pass rather than the command buffer. For best results, if you push a debug
 * group during a pass, always pop it in the same pass.
 *
 * \param commandBuffer a command buffer.
 * \param name a UTF-8 string constant that names the group.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_PopGPUDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer,
    const char *name);

/**
 * Ends the most-recently pushed debug group.
 *
 * \param commandBuffer a command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_PushGPUDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_PopGPUDebugGroup(
    SDL_GPUCommandBuffer *commandBuffer);

/* Disposal */

/**
 * Frees the given texture as soon as it is safe to do so.
 *
 * You must not reference the texture after calling this function.
 *
 * \param device a GPU context.
 * \param texture a texture to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUTexture(
    SDL_GPUDevice *device,
    SDL_GPUTexture *texture);

/**
 * Frees the given sampler as soon as it is safe to do so.
 *
 * You must not reference the texture after calling this function.
 *
 * \param device a GPU context.
 * \param sampler a sampler to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUSampler(
    SDL_GPUDevice *device,
    SDL_GPUSampler *sampler);

/**
 * Frees the given buffer as soon as it is safe to do so.
 *
 * You must not reference the buffer after calling this function.
 *
 * \param device a GPU context.
 * \param buffer a buffer to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUBuffer(
    SDL_GPUDevice *device,
    SDL_GPUBuffer *buffer);

/**
 * Frees the given transfer buffer as soon as it is safe to do so.
 *
 * You must not reference the transfer buffer after calling this function.
 *
 * \param device a GPU context.
 * \param transferBuffer a transfer buffer to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transferBuffer);

/**
 * Frees the given compute pipeline as soon as it is safe to do so.
 *
 * You must not reference the compute pipeline after calling this function.
 *
 * \param device a GPU context.
 * \param computePipeline a compute pipeline to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUComputePipeline(
    SDL_GPUDevice *device,
    SDL_GPUComputePipeline *computePipeline);

/**
 * Frees the given shader as soon as it is safe to do so.
 *
 * You must not reference the shader after calling this function.
 *
 * \param device a GPU context.
 * \param shader a shader to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUShader(
    SDL_GPUDevice *device,
    SDL_GPUShader *shader);

/**
 * Frees the given graphics pipeline as soon as it is safe to do so.
 *
 * You must not reference the graphics pipeline after calling this function.
 *
 * \param device a GPU context.
 * \param graphicsPipeline a graphics pipeline to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUGraphicsPipeline(
    SDL_GPUDevice *device,
    SDL_GPUGraphicsPipeline *graphicsPipeline);

/*
 * COMMAND BUFFERS
 *
 * Render state is managed via command buffers.
 * When setting render state, that state is always local to the command buffer.
 *
 * Commands only begin execution on the GPU once Submit is called.
 * Once the command buffer is submitted, it is no longer valid to use it.
 *
 * Command buffers are executed in submission order. If you submit command buffer A and then command buffer B
 * all commands in A will begin executing before any command in B begins executing.
 *
 * In multi-threading scenarios, you should acquire and submit a command buffer on the same thread.
 * As long as you satisfy this requirement, all functionality related to command buffers is thread-safe.
 */

/**
 * Acquire a command buffer.
 *
 * This command buffer is managed by the implementation and should not be
 * freed by the user. The command buffer may only be used on the thread it was
 * acquired on. The command buffer should be submitted on the thread it was
 * acquired on.
 *
 * \param device a GPU context.
 * \returns a command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBuffer
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC SDL_GPUCommandBuffer *SDLCALL SDL_AcquireGPUCommandBuffer(
    SDL_GPUDevice *device);

/*
 * UNIFORM DATA
 *
 * Uniforms are for passing data to shaders.
 * The uniform data will be constant across all executions of the shader.
 *
 * There are 4 available uniform slots per shader stage (vertex, fragment, compute).
 * Uniform data pushed to a slot on a stage keeps its value throughout the command buffer
 * until you call the relevant Push function on that slot again.
 *
 * For example, you could write your vertex shaders to read a camera matrix from uniform binding slot 0,
 * push the camera matrix at the start of the command buffer, and that data will be used for every
 * subsequent draw call.
 *
 * It is valid to push uniform data during a render or compute pass.
 *
 * Uniforms are best for pushing small amounts of data.
 * If you are pushing more than a matrix or two per call you should consider using a storage buffer instead.
 */

/**
 * Pushes data to a vertex uniform slot on the command buffer.
 *
 * Subsequent draw calls will use this uniform data.
 *
 * \param commandBuffer a command buffer.
 * \param slotIndex the vertex uniform slot to push data to.
 * \param data client data to write.
 * \param dataLengthInBytes the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUVertexUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/**
 * Pushes data to a fragment uniform slot on the command buffer.
 *
 * Subsequent draw calls will use this uniform data.
 *
 * \param commandBuffer a command buffer.
 * \param slotIndex the fragment uniform slot to push data to.
 * \param data client data to write.
 * \param dataLengthInBytes the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUFragmentUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/**
 * Pushes data to a uniform slot on the command buffer.
 *
 * Subsequent draw calls will use this uniform data.
 *
 * \param commandBuffer a command buffer.
 * \param slotIndex the uniform slot to push data to.
 * \param data client data to write.
 * \param dataLengthInBytes the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUComputeUniformData(
    SDL_GPUCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/*
 * A NOTE ON CYCLING
 *
 * When using a command buffer, operations do not occur immediately -
 * they occur some time after the command buffer is submitted.
 *
 * When a resource is used in a pending or active command buffer, it is considered to be "bound".
 * When a resource is no longer used in any pending or active command buffers, it is considered to be "unbound".
 *
 * If data resources are bound, it is unspecified when that data will be unbound
 * unless you acquire a fence when submitting the command buffer and wait on it.
 * However, this doesn't mean you need to track resource usage manually.
 *
 * All of the functions and structs that involve writing to a resource have a "cycle" bool.
 * GPUTransferBuffer, GPUBuffer, and GPUTexture all effectively function as ring buffers on internal resources.
 * When cycle is SDL_TRUE, if the resource is bound, the cycle rotates to the next unbound internal resource,
 * or if none are available, a new one is created.
 * This means you don't have to worry about complex state tracking and synchronization as long as cycling is correctly employed.
 *
 * For example: you can call MapTransferBuffer, write texture data, UnmapTransferBuffer, and then UploadToTexture.
 * The next time you write texture data to the transfer buffer, if you set the cycle param to SDL_TRUE, you don't have
 * to worry about overwriting any data that is not yet uploaded.
 *
 * Another example: If you are using a texture in a render pass every frame, this can cause a data dependency between frames.
 * If you set cycle to SDL_TRUE in the ColorAttachmentInfo struct, you can prevent this data dependency.
 *
 * Cycling will never undefine already bound data.
 * When cycling, all data in the resource is considered to be undefined for subsequent commands until that data is written again.
 * You must take care not to read undefined data.
 *
 * Note that when cycling a texture, the entire texture will be cycled,
 * even if only part of the texture is used in the call,
 * so you must consider the entire texture to contain undefined data after cycling.
 *
 * You must also take care not to overwrite a section of data that has been referenced in a command without cycling first.
 * It is OK to overwrite unreferenced data in a bound resource without cycling,
 * but overwriting a section of data that has already been referenced will produce unexpected results.
 */

/* Graphics State */

/**
 * Begins a render pass on a command buffer.
 *
 * A render pass consists of a set of texture subresources (or depth slices in
 * the 3D texture case) which will be rendered to during the render pass,
 * along with corresponding clear values and load/store operations. All
 * operations related to graphics pipelines must take place inside of a render
 * pass. A default viewport and scissor state are automatically set when this
 * is called. You cannot begin another render pass, or begin a compute pass or
 * copy pass until you have ended the render pass.
 *
 * \param commandBuffer a command buffer.
 * \param colorAttachmentInfos an array of texture subresources with
 *                             corresponding clear values and load/store ops.
 * \param colorAttachmentCount the number of color attachments in the
 *                             colorAttachmentInfos array.
 * \param depthStencilAttachmentInfo a texture subresource with corresponding
 *                                   clear value and load/store ops, may be
 *                                   NULL.
 * \returns a render pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_EndGPURenderPass
 */
extern SDL_DECLSPEC SDL_GPURenderPass *SDLCALL SDL_BeginGPURenderPass(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo);

/**
 * Binds a graphics pipeline on a render pass to be used in rendering.
 *
 * A graphics pipeline must be bound before making any draw calls.
 *
 * \param renderPass a render pass handle.
 * \param graphicsPipeline the graphics pipeline to bind.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUGraphicsPipeline(
    SDL_GPURenderPass *renderPass,
    SDL_GPUGraphicsPipeline *graphicsPipeline);

/**
 * Sets the current viewport state on a command buffer.
 *
 * \param renderPass a render pass handle.
 * \param viewport the viewport to set.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUViewport(
    SDL_GPURenderPass *renderPass,
    SDL_GPUViewport *viewport);

/**
 * Sets the current scissor state on a command buffer.
 *
 * \param renderPass a render pass handle.
 * \param scissor the scissor area to set.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUScissor(
    SDL_GPURenderPass *renderPass,
    SDL_Rect *scissor);

/**
 * Binds vertex buffers on a command buffer for use with subsequent draw
 * calls.
 *
 * \param renderPass a render pass handle.
 * \param firstBinding the starting bind point for the vertex buffers.
 * \param pBindings an array of SDL_GPUBufferBinding structs containing vertex
 *                  buffers and offset values.
 * \param bindingCount the number of bindings in the pBindings array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexBuffers(
    SDL_GPURenderPass *renderPass,
    Uint32 firstBinding,
    SDL_GPUBufferBinding *pBindings,
    Uint32 bindingCount);

/**
 * Binds an index buffer on a command buffer for use with subsequent draw
 * calls.
 *
 * \param renderPass a render pass handle.
 * \param pBinding a pointer to a struct containing an index buffer and
 *                 offset.
 * \param indexElementSize whether the index values in the buffer are 16- or
 *                         32-bit.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUIndexBuffer(
    SDL_GPURenderPass *renderPass,
    SDL_GPUBufferBinding *pBinding,
    SDL_GPUIndexElementSize indexElementSize);

/**
 * Binds texture-sampler pairs for use on the vertex shader.
 *
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the vertex sampler slot to begin binding from.
 * \param textureSamplerBindings an array of texture-sampler binding structs.
 * \param bindingCount the number of texture-sampler pairs to bind from the
 *                     array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexSamplers(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount);

/**
 * Binds storage textures for use on the vertex shader.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the vertex storage texture slot to begin binding from.
 * \param storageTextures an array of storage textures.
 * \param bindingCount the number of storage texture to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexStorageTextures(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount);

/**
 * Binds storage buffers for use on the vertex shader.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the vertex storage buffer slot to begin binding from.
 * \param storageBuffers an array of buffers.
 * \param bindingCount the number of buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexStorageBuffers(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount);

/**
 * Binds texture-sampler pairs for use on the fragment shader.
 *
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the fragment sampler slot to begin binding from.
 * \param textureSamplerBindings an array of texture-sampler binding structs.
 * \param bindingCount the number of texture-sampler pairs to bind from the
 *                     array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentSamplers(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount);

/**
 * Binds storage textures for use on the fragment shader.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the fragment storage texture slot to begin binding from.
 * \param storageTextures an array of storage textures.
 * \param bindingCount the number of storage textures to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentStorageTextures(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount);

/**
 * Binds storage buffers for use on the fragment shader.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle.
 * \param firstSlot the fragment storage buffer slot to begin binding from.
 * \param storageBuffers an array of storage buffers.
 * \param bindingCount the number of storage buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentStorageBuffers(
    SDL_GPURenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount);

/* Drawing */

/**
 * Draws data using bound graphics state with an index buffer and instancing
 * enabled.
 *
 * You must not call this function before binding a graphics pipeline.
 *
 * Note that the `firstVertex` and `firstInstance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \param renderPass a render pass handle.
 * \param indexCount the number of vertices to draw per instance.
 * \param instanceCount the number of instances to draw.
 * \param firstIndex the starting index within the index buffer.
 * \param vertexOffset value added to vertex index before indexing into the
 *                     vertex buffer.
 * \param firstInstance the ID of the first instance to draw.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUIndexedPrimitives(
    SDL_GPURenderPass *renderPass,
    Uint32 indexCount,
    Uint32 instanceCount,
    Uint32 firstIndex,
    Sint32 vertexOffset,
    Uint32 firstInstance);

/**
 * Draws data using bound graphics state.
 *
 * You must not call this function before binding a graphics pipeline.
 *
 * Note that the `firstVertex` and `firstInstance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \param renderPass a render pass handle.
 * \param vertexCount the number of vertices to draw.
 * \param instanceCount the number of instances that will be drawn.
 * \param firstVertex the index of the first vertex to draw.
 * \param firstInstance the ID of the first instance to draw.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUPrimitives(
    SDL_GPURenderPass *renderPass,
    Uint32 vertexCount,
    Uint32 instanceCount,
    Uint32 firstVertex,
    Uint32 firstInstance);

/**
 * Draws data using bound graphics state and with draw parameters set from a
 * buffer.
 *
 * The buffer layout should match the layout of SDL_GPUIndirectDrawCommand.
 * You must not call this function before binding a graphics pipeline.
 *
 * \param renderPass a render pass handle.
 * \param buffer a buffer containing draw parameters.
 * \param offsetInBytes the offset to start reading from the draw buffer.
 * \param drawCount the number of draw parameter sets that should be read from
 *                  the draw buffer.
 * \param stride the byte stride between sets of draw parameters.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUPrimitivesIndirect(
    SDL_GPURenderPass *renderPass,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride);

/**
 * Draws data using bound graphics state with an index buffer enabled and with
 * draw parameters set from a buffer.
 *
 * The buffer layout should match the layout of
 * SDL_GPUIndexedIndirectDrawCommand. You must not call this function before
 * binding a graphics pipeline.
 *
 * \param renderPass a render pass handle.
 * \param buffer a buffer containing draw parameters.
 * \param offsetInBytes the offset to start reading from the draw buffer.
 * \param drawCount the number of draw parameter sets that should be read from
 *                  the draw buffer.
 * \param stride the byte stride between sets of draw parameters.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUIndexedPrimitivesIndirect(
    SDL_GPURenderPass *renderPass,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride);

/**
 * Ends the given render pass.
 *
 * All bound graphics state on the render pass command buffer is unset. The
 * render pass handle is now invalid.
 *
 * \param renderPass a render pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPURenderPass(
    SDL_GPURenderPass *renderPass);

/* Compute Pass */

/**
 * Begins a compute pass on a command buffer.
 *
 * A compute pass is defined by a set of texture subresources and buffers that
 * will be written to by compute pipelines. These textures and buffers must
 * have been created with the COMPUTE_STORAGE_WRITE bit. All operations
 * related to compute pipelines must take place inside of a compute pass. You
 * must not begin another compute pass, or a render pass or copy pass before
 * ending the compute pass.
 *
 * A VERY IMPORTANT NOTE Textures and buffers bound as write-only MUST NOT be
 * read from during the compute pass. Doing so will result in undefined
 * behavior. If your compute work requires reading the output from a previous
 * dispatch, you MUST end the current compute pass and begin a new one before
 * you can safely access the data.
 *
 * \param commandBuffer a command buffer.
 * \param storageTextureBindings an array of writeable storage texture binding
 *                               structs.
 * \param storageTextureBindingCount the number of storage textures to bind
 *                                   from the array.
 * \param storageBufferBindings an array of writeable storage buffer binding
 *                              structs.
 * \param storageBufferBindingCount the number of storage buffers to bind from
 *                                  the array.
 * \returns a compute pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_EndGPUComputePass
 */
extern SDL_DECLSPEC SDL_GPUComputePass *SDLCALL SDL_BeginGPUComputePass(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUStorageTextureWriteOnlyBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GPUStorageBufferWriteOnlyBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount);

/**
 * Binds a compute pipeline on a command buffer for use in compute dispatch.
 *
 * \param computePass a compute pass handle.
 * \param computePipeline a compute pipeline to bind.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputePipeline(
    SDL_GPUComputePass *computePass,
    SDL_GPUComputePipeline *computePipeline);

/**
 * Binds storage textures as readonly for use on the compute pipeline.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT.
 *
 * \param computePass a compute pass handle.
 * \param firstSlot the compute storage texture slot to begin binding from.
 * \param storageTextures an array of storage textures.
 * \param bindingCount the number of storage textures to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputeStorageTextures(
    SDL_GPUComputePass *computePass,
    Uint32 firstSlot,
    SDL_GPUTexture **storageTextures,
    Uint32 bindingCount);

/**
 * Binds storage buffers as readonly for use on the compute pipeline.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT.
 *
 * \param computePass a compute pass handle.
 * \param firstSlot the compute storage buffer slot to begin binding from.
 * \param storageBuffers an array of storage buffer binding structs.
 * \param bindingCount the number of storage buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputeStorageBuffers(
    SDL_GPUComputePass *computePass,
    Uint32 firstSlot,
    SDL_GPUBuffer **storageBuffers,
    Uint32 bindingCount);

/**
 * Dispatches compute work.
 *
 * You must not call this function before binding a compute pipeline.
 *
 * A VERY IMPORTANT NOTE If you dispatch multiple times in a compute pass, and
 * the dispatches write to the same resource region as each other, there is no
 * guarantee of which order the writes will occur. If the write order matters,
 * you MUST end the compute pass and begin another one.
 *
 * \param computePass a compute pass handle.
 * \param groupCountX number of local workgroups to dispatch in the X
 *                    dimension.
 * \param groupCountY number of local workgroups to dispatch in the Y
 *                    dimension.
 * \param groupCountZ number of local workgroups to dispatch in the Z
 *                    dimension.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DispatchGPUCompute(
    SDL_GPUComputePass *computePass,
    Uint32 groupCountX,
    Uint32 groupCountY,
    Uint32 groupCountZ);

/**
 * Dispatches compute work with parameters set from a buffer.
 *
 * The buffer layout should match the layout of
 * SDL_GPUIndirectDispatchCommand. You must not call this function before
 * binding a compute pipeline.
 *
 * A VERY IMPORTANT NOTE If you dispatch multiple times in a compute pass, and
 * the dispatches write to the same resource region as each other, there is no
 * guarantee of which order the writes will occur. If the write order matters,
 * you MUST end the compute pass and begin another one.
 *
 * \param computePass a compute pass handle.
 * \param buffer a buffer containing dispatch parameters.
 * \param offsetInBytes the offset to start reading from the dispatch buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DispatchGPUComputeIndirect(
    SDL_GPUComputePass *computePass,
    SDL_GPUBuffer *buffer,
    Uint32 offsetInBytes);

/**
 * Ends the current compute pass.
 *
 * All bound compute state on the command buffer is unset. The compute pass
 * handle is now invalid.
 *
 * \param computePass a compute pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPUComputePass(
    SDL_GPUComputePass *computePass);

/* TransferBuffer Data */

/**
 * Maps a transfer buffer into application address space.
 *
 * You must unmap the transfer buffer before encoding upload commands.
 *
 * \param device a GPU context.
 * \param transferBuffer a transfer buffer.
 * \param cycle if SDL_TRUE, cycles the transfer buffer if it is bound.
 * \returns the address of the mapped transfer buffer memory.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void *SDLCALL SDL_MapGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transferBuffer,
    SDL_bool cycle);

/**
 * Unmaps a previously mapped transfer buffer.
 *
 * \param device a GPU context.
 * \param transferBuffer a previously mapped transfer buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UnmapGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transferBuffer);

/* Copy Pass */

/**
 * Begins a copy pass on a command buffer.
 *
 * All operations related to copying to or from buffers or textures take place
 * inside a copy pass. You must not begin another copy pass, or a render pass
 * or compute pass before ending the copy pass.
 *
 * \param commandBuffer a command buffer.
 * \returns a copy pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_GPUCopyPass *SDLCALL SDL_BeginGPUCopyPass(
    SDL_GPUCommandBuffer *commandBuffer);

/**
 * Uploads data from a transfer buffer to a texture.
 *
 * The upload occurs on the GPU timeline. You may assume that the upload has
 * finished in subsequent commands.
 *
 * You must align the data in the transfer buffer to a multiple of the texel
 * size of the texture format.
 *
 * \param copyPass a copy pass handle.
 * \param source the source transfer buffer with image layout information.
 * \param destination the destination texture region.
 * \param cycle if SDL_TRUE, cycles the texture if the texture is bound,
 *              otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UploadToGPUTexture(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUTextureTransferInfo *source,
    SDL_GPUTextureRegion *destination,
    SDL_bool cycle);

/* Uploads data from a TransferBuffer to a Buffer. */

/**
 * Uploads data from a transfer buffer to a buffer.
 *
 * The upload occurs on the GPU timeline. You may assume that the upload has
 * finished in subsequent commands.
 *
 * \param copyPass a copy pass handle.
 * \param source the source transfer buffer with offset.
 * \param destination the destination buffer with offset and size.
 * \param cycle if SDL_TRUE, cycles the buffer if it is bound, otherwise
 *              overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UploadToGPUBuffer(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUTransferBufferLocation *source,
    SDL_GPUBufferRegion *destination,
    SDL_bool cycle);

/**
 * Performs a texture-to-texture copy.
 *
 * This copy occurs on the GPU timeline. You may assume the copy has finished
 * in subsequent commands.
 *
 * \param copyPass a copy pass handle.
 * \param source a source texture region.
 * \param destination a destination texture region.
 * \param w the width of the region to copy.
 * \param h the height of the region to copy.
 * \param d the depth of the region to copy.
 * \param cycle if SDL_TRUE, cycles the destination texture if the destination
 *              texture is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_CopyGPUTextureToTexture(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUTextureLocation *source,
    SDL_GPUTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    SDL_bool cycle);

/* Copies data from a buffer to a buffer. */

/**
 * Performs a buffer-to-buffer copy.
 *
 * This copy occurs on the GPU timeline. You may assume the copy has finished
 * in subsequent commands.
 *
 * \param copyPass a copy pass handle.
 * \param source the buffer and offset to copy from.
 * \param destination the buffer and offset to copy to.
 * \param size the length of the buffer to copy.
 * \param cycle if SDL_TRUE, cycles the destination buffer if it is bound,
 *              otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_CopyGPUBufferToBuffer(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUBufferLocation *source,
    SDL_GPUBufferLocation *destination,
    Uint32 size,
    SDL_bool cycle);

/**
 * Copies data from a texture to a transfer buffer on the GPU timeline.
 *
 * This data is not guaranteed to be copied until the command buffer fence is
 * signaled.
 *
 * \param copyPass a copy pass handle.
 * \param source the source texture region.
 * \param destination the destination transfer buffer with image layout
 *                    information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DownloadFromGPUTexture(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUTextureRegion *source,
    SDL_GPUTextureTransferInfo *destination);

/**
 * Copies data from a buffer to a transfer buffer on the GPU timeline.
 *
 * This data is not guaranteed to be copied until the command buffer fence is
 * signaled.
 *
 * \param copyPass a copy pass handle.
 * \param source the source buffer with offset and size.
 * \param destination the destination transfer buffer with offset.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DownloadFromGPUBuffer(
    SDL_GPUCopyPass *copyPass,
    SDL_GPUBufferRegion *source,
    SDL_GPUTransferBufferLocation *destination);

/**
 * Ends the current copy pass.
 *
 * \param copyPass a copy pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPUCopyPass(
    SDL_GPUCopyPass *copyPass);

/**
 * Generates mipmaps for the given texture.
 *
 * This function must not be called inside of any pass.
 *
 * \param commandBuffer a commandBuffer.
 * \param texture a texture with more than 1 mip level.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_GenerateMipmapsForGPUTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUTexture *texture);

/**
 * Blits from a source texture region to a destination texture region.
 *
 * This function must not be called inside of any pass.
 *
 * \param commandBuffer a command buffer.
 * \param source the texture region to copy from.
 * \param destination the texture region to copy to.
 * \param flipMode the flip mode for the source texture region.
 * \param filterMode the filter mode that will be used when blitting.
 * \param cycle if SDL_TRUE, cycles the destination texture if the destination
 *              texture is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BlitGPUTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBlitRegion *source,
    SDL_GPUBlitRegion *destination,
    SDL_FlipMode flipMode,
    SDL_GPUFilter filterMode,
    SDL_bool cycle);

/* Submission/Presentation */

/**
 * Determines whether a swapchain composition is supported by the window.
 *
 * The window must be claimed before calling this function.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \param swapchainComposition the swapchain composition to check.
 * \returns SDL_TRUE if supported, SDL_FALSE if unsupported (or on error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_WindowSupportsGPUSwapchainComposition(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition);

/**
 * Determines whether a presentation mode is supported by the window.
 *
 * The window must be claimed before calling this function.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \param presentMode the presentation mode to check.
 * \returns SDL_TRUE if supported, SDL_FALSE if unsupported (or on error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_WindowSupportsGPUPresentMode(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUPresentMode presentMode);

/**
 * Claims a window, creating a swapchain structure for it.
 *
 * This must be called before SDL_AcquireGPUSwapchainTexture is called using
 * the window.
 *
 * The swapchain will be created with SDL_GPU_SWAPCHAINCOMPOSITION_SDR and
 * SDL_GPU_PRESENTMODE_VSYNC. If you want to have different swapchain
 * parameters, you must call SetSwapchainParameters after claiming the window.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \returns SDL_TRUE on success, otherwise SDL_FALSE.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireGPUSwapchainTexture
 * \sa SDL_ReleaseWindowFromGPUDevice
 * \sa SDL_WindowSupportsGPUPresentMode
 * \sa SDL_WindowSupportsGPUSwapchainComposition
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_ClaimWindowForGPUDevice(
    SDL_GPUDevice *device,
    SDL_Window *window);

/**
 * Unclaims a window, destroying its swapchain structure.
 *
 * \param device a GPU context.
 * \param window an SDL_Window that has been claimed.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseWindowFromGPUDevice(
    SDL_GPUDevice *device,
    SDL_Window *window);

/**
 * Changes the swapchain parameters for the given claimed window.
 *
 * This function will fail if the requested present mode or swapchain
 * composition are unsupported by the device. Check if the parameters are
 * supported via SDL_WindowSupportsGPUPresentMode /
 * SDL_WindowSupportsGPUSwapchainComposition prior to calling this function.
 *
 * SDL_GPU_PRESENTMODE_VSYNC and SDL_GPU_SWAPCHAINCOMPOSITION_SDR are always
 * supported.
 *
 * \param device a GPU context.
 * \param window an SDL_Window that has been claimed.
 * \param swapchainComposition the desired composition of the swapchain.
 * \param presentMode the desired present mode for the swapchain.
 * \returns SDL_TRUE if successful, SDL_FALSE on error.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_WindowSupportsGPUPresentMode
 * \sa SDL_WindowSupportsGPUSwapchainComposition
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_SetGPUSwapchainParameters(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchainComposition,
    SDL_GPUPresentMode presentMode);

/**
 * Obtains the texture format of the swapchain for the given window.
 *
 * \param device a GPU context.
 * \param window an SDL_Window that has been claimed.
 * \returns the texture format of the swapchain.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_GPUTextureFormat SDLCALL SDL_GetGPUSwapchainTextureFormat(
    SDL_GPUDevice *device,
    SDL_Window *window);

/**
 * Acquire a texture to use in presentation.
 *
 * When a swapchain texture is acquired on a command buffer, it will
 * automatically be submitted for presentation when the command buffer is
 * submitted. The swapchain texture should only be referenced by the command
 * buffer used to acquire it. May return NULL under certain conditions. This
 * is not necessarily an error. This texture is managed by the implementation
 * and must not be freed by the user. You MUST NOT call this function from any
 * thread other than the one that created the window.
 *
 * \param commandBuffer a command buffer.
 * \param window a window that has been claimed.
 * \param pWidth a pointer filled in with the swapchain width.
 * \param pHeight a pointer filled in with the swapchain height.
 * \returns a swapchain texture.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 * \sa SDL_SubmitGPUCommandBuffer
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC SDL_GPUTexture *SDLCALL SDL_AcquireGPUSwapchainTexture(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight);

/**
 * Submits a command buffer so its commands can be processed on the GPU.
 *
 * It is invalid to use the command buffer after this is called.
 *
 * This must be called from the thread the command buffer was acquired on.
 *
 * All commands in the submission are guaranteed to begin executing before any
 * command in a subsequent submission begins executing.
 *
 * \param commandBuffer a command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireGPUCommandBuffer
 * \sa SDL_AcquireGPUSwapchainTexture
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC void SDLCALL SDL_SubmitGPUCommandBuffer(
    SDL_GPUCommandBuffer *commandBuffer);

/**
 * Submits a command buffer so its commands can be processed on the GPU, and
 * acquires a fence associated with the command buffer.
 *
 * You must release this fence when it is no longer needed or it will cause a
 * leak. It is invalid to use the command buffer after this is called.
 *
 * This must be called from the thread the command buffer was acquired on.
 *
 * All commands in the submission are guaranteed to begin executing before any
 * command in a subsequent submission begins executing.
 *
 * \param commandBuffer a command buffer.
 * \returns a fence associated with the command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireGPUCommandBuffer
 * \sa SDL_AcquireGPUSwapchainTexture
 * \sa SDL_SubmitGPUCommandBuffer
 * \sa SDL_ReleaseGPUFence
 */
extern SDL_DECLSPEC SDL_GPUFence *SDLCALL SDL_SubmitGPUCommandBufferAndAcquireFence(
    SDL_GPUCommandBuffer *commandBuffer);

/**
 * Blocks the thread until the GPU is completely idle.
 *
 * \param device a GPU context.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_WaitForGPUFences
 */
extern SDL_DECLSPEC void SDLCALL SDL_WaitForGPUIdle(
    SDL_GPUDevice *device);

/**
 * Blocks the thread until the given fences are signaled.
 *
 * \param device a GPU context.
 * \param waitAll if 0, wait for any fence to be signaled, if 1, wait for all
 *                fences to be signaled.
 * \param pFences an array of fences to wait on.
 * \param fenceCount the number of fences in the pFences array.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 * \sa SDL_WaitForGPUIdle
 */
extern SDL_DECLSPEC void SDLCALL SDL_WaitForGPUFences(
    SDL_GPUDevice *device,
    SDL_bool waitAll,
    SDL_GPUFence **pFences,
    Uint32 fenceCount);

/**
 * Checks the status of a fence.
 *
 * \param device a GPU context.
 * \param fence a fence.
 * \returns SDL_TRUE if the fence is signaled, SDL_FALSE if it is not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_QueryGPUFence(
    SDL_GPUDevice *device,
    SDL_GPUFence *fence);

/**
 * Releases a fence obtained from SDL_SubmitGPUCommandBufferAndAcquireFence.
 *
 * \param device a GPU context.
 * \param fence a fence.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUFence(
    SDL_GPUDevice *device,
    SDL_GPUFence *fence);

/* Format Info */

/**
 * Obtains the texel block size for a texture format.
 *
 * \param textureFormat the texture format you want to know the texel size of.
 * \returns the texel block size of the texture format.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UploadToGPUTexture
 */
extern SDL_DECLSPEC Uint32 SDLCALL SDL_GPUTextureFormatTexelBlockSize(
    SDL_GPUTextureFormat textureFormat);

/**
 * Determines whether a texture format is supported for a given type and
 * usage.
 *
 * \param device a GPU context.
 * \param format the texture format to check.
 * \param type the type of texture (2D, 3D, Cube).
 * \param usage a bitmask of all usage scenarios to check.
 * \returns whether the texture format is supported for this type and usage.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GPUTextureSupportsFormat(
    SDL_GPUDevice *device,
    SDL_GPUTextureFormat format,
    SDL_GPUTextureType type,
    SDL_GPUTextureUsageFlags usage);

/**
 * Determines if a sample count for a texture format is supported.
 *
 * \param device a GPU context.
 * \param format the texture format to check.
 * \param sampleCount the sample count to check.
 * \returns a hardware-specific version of min(preferred, possible).
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GPUTextureSupportsSampleCount(
    SDL_GPUDevice *device,
    SDL_GPUTextureFormat format,
    SDL_GPUSampleCount sampleCount);

#ifdef SDL_PLATFORM_GDK

/**
 * Call this to suspend GPU operation on Xbox when you receive the
 * SDL_EVENT_DID_ENTER_BACKGROUND event.
 *
 * Do NOT call any SDL_GPU functions after calling this function! This must
 * also be called before calling SDL_GDKSuspendComplete.
 *
 * \param device a GPU context.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AddEventWatch
 */
extern SDL_DECLSPEC void SDLCALL SDL_GDKSuspendGPU(SDL_GPUDevice *device);

/**
 * Call this to resume GPU operation on Xbox when you receive the
 * SDL_EVENT_WILL_ENTER_FOREGROUND event.
 *
 * When resuming, this function MUST be called before calling any other
 * SDL_GPU functions.
 *
 * \param device a GPU context.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AddEventWatch
 */
extern SDL_DECLSPEC void SDLCALL SDL_GDKResumeGPU(SDL_GPUDevice *device);

#endif /* SDL_PLATFORM_GDK */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#include <SDL3/SDL_close_code.h>

#endif /* SDL_gpu_h_ */
