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

/**
 * \file SDL_gpu.h
 *
 * Include file for SDL GPU API functions
 */

#ifndef SDL_GPU_H
#define SDL_GPU_H

#include <SDL3/SDL_stdinc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type Declarations */

typedef struct SDL_GpuDevice SDL_GpuDevice;
typedef struct SDL_GpuBuffer SDL_GpuBuffer;
typedef struct SDL_GpuTransferBuffer SDL_GpuTransferBuffer;
typedef struct SDL_GpuTexture SDL_GpuTexture;
typedef struct SDL_GpuSampler SDL_GpuSampler;
typedef struct SDL_GpuShader SDL_GpuShader;
typedef struct SDL_GpuComputePipeline SDL_GpuComputePipeline;
typedef struct SDL_GpuGraphicsPipeline SDL_GpuGraphicsPipeline;
typedef struct SDL_GpuCommandBuffer SDL_GpuCommandBuffer;
typedef struct SDL_GpuRenderPass SDL_GpuRenderPass;
typedef struct SDL_GpuComputePass SDL_GpuComputePass;
typedef struct SDL_GpuCopyPass SDL_GpuCopyPass;
typedef struct SDL_GpuFence SDL_GpuFence;

typedef enum SDL_GpuPrimitiveType
{
    SDL_GPU_PRIMITIVETYPE_POINTLIST,
    SDL_GPU_PRIMITIVETYPE_LINELIST,
    SDL_GPU_PRIMITIVETYPE_LINESTRIP,
    SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP
} SDL_GpuPrimitiveType;

typedef enum SDL_GpuLoadOp
{
    SDL_GPU_LOADOP_LOAD,
    SDL_GPU_LOADOP_CLEAR,
    SDL_GPU_LOADOP_DONT_CARE
} SDL_GpuLoadOp;

typedef enum SDL_GpuStoreOp
{
    SDL_GPU_STOREOP_STORE,
    SDL_GPU_STOREOP_DONT_CARE
} SDL_GpuStoreOp;

typedef enum SDL_GpuIndexElementSize
{
    SDL_GPU_INDEXELEMENTSIZE_16BIT,
    SDL_GPU_INDEXELEMENTSIZE_32BIT
} SDL_GpuIndexElementSize;

typedef enum SDL_GpuTextureFormat
{
    /* Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8,
    SDL_GPU_TEXTUREFORMAT_R5G6B5,
    SDL_GPU_TEXTUREFORMAT_A1R5G5B5,
    SDL_GPU_TEXTUREFORMAT_B4G4R4A4,
    SDL_GPU_TEXTUREFORMAT_A2R10G10B10,
    SDL_GPU_TEXTUREFORMAT_A2B10G10R10,
    SDL_GPU_TEXTUREFORMAT_R16G16,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16,
    SDL_GPU_TEXTUREFORMAT_R8,
    SDL_GPU_TEXTUREFORMAT_A8,
    /* Compressed Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1,
    SDL_GPU_TEXTUREFORMAT_BC2,
    SDL_GPU_TEXTUREFORMAT_BC3,
    SDL_GPU_TEXTUREFORMAT_BC7,
    /* Signed Normalized Float Color Formats  */
    SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
    /* Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R16_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_R32_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
    /* Unsigned Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
    SDL_GPU_TEXTUREFORMAT_R16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
    /* SRGB Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SRGB,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_SRGB,
    /* Compressed SRGB Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC3_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC7_SRGB,
    /* Depth Formats */
    SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM,
    SDL_GPU_TEXTUREFORMAT_D32_SFLOAT,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
    SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT
} SDL_GpuTextureFormat;

typedef enum SDL_GpuTextureUsageFlagBits
{
    SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT = 0x00000001,
    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT = 0x00000002,
    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT = 0x00000004,
    SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT = 0x00000008,
    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT = 0x00000020,
    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT = 0x00000040
} SDL_GpuTextureUsageFlagBits;

typedef Uint32 SDL_GpuTextureUsageFlags;

typedef enum SDL_GpuTextureType
{
    SDL_GPU_TEXTURETYPE_2D,
    SDL_GPU_TEXTURETYPE_3D,
    SDL_GPU_TEXTURETYPE_CUBE,
} SDL_GpuTextureType;

typedef enum SDL_GpuSampleCount
{
    SDL_GPU_SAMPLECOUNT_1,
    SDL_GPU_SAMPLECOUNT_2,
    SDL_GPU_SAMPLECOUNT_4,
    SDL_GPU_SAMPLECOUNT_8
} SDL_GpuSampleCount;

typedef enum SDL_GpuCubeMapFace
{
    SDL_GPU_CUBEMAPFACE_POSITIVEX,
    SDL_GPU_CUBEMAPFACE_NEGATIVEX,
    SDL_GPU_CUBEMAPFACE_POSITIVEY,
    SDL_GPU_CUBEMAPFACE_NEGATIVEY,
    SDL_GPU_CUBEMAPFACE_POSITIVEZ,
    SDL_GPU_CUBEMAPFACE_NEGATIVEZ
} SDL_GpuCubeMapFace;

typedef enum SDL_GpuBufferUsageFlagBits
{
    SDL_GPU_BUFFERUSAGE_VERTEX_BIT = 0x00000001,
    SDL_GPU_BUFFERUSAGE_INDEX_BIT = 0x00000002,
    SDL_GPU_BUFFERUSAGE_INDIRECT_BIT = 0x00000004,
    SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT = 0x00000008,
    SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT = 0x00000020,
    SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT = 0x00000040
} SDL_GpuBufferUsageFlagBits;

typedef Uint32 SDL_GpuBufferUsageFlags;

typedef enum SDL_GpuTransferBufferUsage
{
    SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD
} SDL_GpuTransferBufferUsage;

typedef enum SDL_GpuShaderStage
{
    SDL_GPU_SHADERSTAGE_VERTEX,
    SDL_GPU_SHADERSTAGE_FRAGMENT
} SDL_GpuShaderStage;

typedef enum SDL_GpuShaderFormat
{
    SDL_GPU_SHADERFORMAT_INVALID,
    SDL_GPU_SHADERFORMAT_SPIRV,    /* Vulkan */
    SDL_GPU_SHADERFORMAT_HLSL,     /* D3D11, D3D12 */
    SDL_GPU_SHADERFORMAT_DXBC,     /* D3D11, D3D12 */
    SDL_GPU_SHADERFORMAT_DXIL,     /* D3D12 */
    SDL_GPU_SHADERFORMAT_MSL,      /* Metal */
    SDL_GPU_SHADERFORMAT_METALLIB, /* Metal */
    SDL_GPU_SHADERFORMAT_SECRET    /* NDA'd platforms */
} SDL_GpuShaderFormat;

typedef enum SDL_GpuVertexElementFormat
{
    SDL_GPU_VERTEXELEMENTFORMAT_UINT,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
    SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2,
    SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
    SDL_GPU_VERTEXELEMENTFORMAT_VECTOR4,
    SDL_GPU_VERTEXELEMENTFORMAT_COLOR,
    SDL_GPU_VERTEXELEMENTFORMAT_BYTE4,
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
    SDL_GPU_VERTEXELEMENTFORMAT_SHORT4,
    SDL_GPU_VERTEXELEMENTFORMAT_NORMALIZEDSHORT2,
    SDL_GPU_VERTEXELEMENTFORMAT_NORMALIZEDSHORT4,
    SDL_GPU_VERTEXELEMENTFORMAT_HALFVECTOR2,
    SDL_GPU_VERTEXELEMENTFORMAT_HALFVECTOR4
} SDL_GpuVertexElementFormat;

typedef enum SDL_GpuVertexInputRate
{
    SDL_GPU_VERTEXINPUTRATE_VERTEX = 0,
    SDL_GPU_VERTEXINPUTRATE_INSTANCE = 1
} SDL_GpuVertexInputRate;

typedef enum SDL_GpuFillMode
{
    SDL_GPU_FILLMODE_FILL,
    SDL_GPU_FILLMODE_LINE
} SDL_GpuFillMode;

typedef enum SDL_GpuCullMode
{
    SDL_GPU_CULLMODE_NONE,
    SDL_GPU_CULLMODE_FRONT,
    SDL_GPU_CULLMODE_BACK
} SDL_GpuCullMode;

typedef enum SDL_GpuFrontFace
{
    SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    SDL_GPU_FRONTFACE_CLOCKWISE
} SDL_GpuFrontFace;

typedef enum SDL_GpuCompareOp
{
    SDL_GPU_COMPAREOP_NEVER,
    SDL_GPU_COMPAREOP_LESS,
    SDL_GPU_COMPAREOP_EQUAL,
    SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    SDL_GPU_COMPAREOP_GREATER,
    SDL_GPU_COMPAREOP_NOT_EQUAL,
    SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
    SDL_GPU_COMPAREOP_ALWAYS
} SDL_GpuCompareOp;

typedef enum SDL_GpuStencilOp
{
    SDL_GPU_STENCILOP_KEEP,
    SDL_GPU_STENCILOP_ZERO,
    SDL_GPU_STENCILOP_REPLACE,
    SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP,
    SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP,
    SDL_GPU_STENCILOP_INVERT,
    SDL_GPU_STENCILOP_INCREMENT_AND_WRAP,
    SDL_GPU_STENCILOP_DECREMENT_AND_WRAP
} SDL_GpuStencilOp;

typedef enum SDL_GpuBlendOp
{
    SDL_GPU_BLENDOP_ADD,
    SDL_GPU_BLENDOP_SUBTRACT,
    SDL_GPU_BLENDOP_REVERSE_SUBTRACT,
    SDL_GPU_BLENDOP_MIN,
    SDL_GPU_BLENDOP_MAX
} SDL_GpuBlendOp;

typedef enum SDL_GpuBlendFactor
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
} SDL_GpuBlendFactor;

typedef enum SDL_GpuColorComponentFlagBits
{
    SDL_GPU_COLORCOMPONENT_R_BIT = 0x00000001,
    SDL_GPU_COLORCOMPONENT_G_BIT = 0x00000002,
    SDL_GPU_COLORCOMPONENT_B_BIT = 0x00000004,
    SDL_GPU_COLORCOMPONENT_A_BIT = 0x00000008
} SDL_GpuColorComponentFlagBits;

typedef Uint32 SDL_GpuColorComponentFlags;

typedef enum SDL_GpuFilter
{
    SDL_GPU_FILTER_NEAREST,
    SDL_GPU_FILTER_LINEAR
} SDL_GpuFilter;

typedef enum SDL_GpuSamplerMipmapMode
{
    SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
    SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
} SDL_GpuSamplerMipmapMode;

typedef enum SDL_GpuSamplerAddressMode
{
    SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
    SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
} SDL_GpuSamplerAddressMode;

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
typedef enum SDL_GpuPresentMode
{
    SDL_GPU_PRESENTMODE_VSYNC,
    SDL_GPU_PRESENTMODE_IMMEDIATE,
    SDL_GPU_PRESENTMODE_MAILBOX
} SDL_GpuPresentMode;

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
typedef enum SDL_GpuSwapchainComposition
{
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048
} SDL_GpuSwapchainComposition;

typedef enum SDL_GpuBackendBits
{
    SDL_GPU_BACKEND_INVALID = 0,
    SDL_GPU_BACKEND_VULKAN = 0x0000000000000001,
    SDL_GPU_BACKEND_D3D11 = 0x0000000000000002,
    SDL_GPU_BACKEND_METAL = 0x0000000000000004,
    SDL_GPU_BACKEND_ALL = (SDL_GPU_BACKEND_VULKAN | SDL_GPU_BACKEND_D3D11 | SDL_GPU_BACKEND_METAL)
} SDL_GpuBackendBits;

typedef Uint64 SDL_GpuBackend;

/* Structures */

typedef struct SDL_GpuDepthStencilValue
{
    float depth;
    Uint32 stencil;
} SDL_GpuDepthStencilValue;

typedef struct SDL_GpuRect
{
    Sint32 x;
    Sint32 y;
    Sint32 w;
    Sint32 h;
} SDL_GpuRect;

typedef struct SDL_GpuColor
{
    float r;
    float g;
    float b;
    float a;
} SDL_GpuColor;

typedef struct SDL_GpuViewport
{
    float x;
    float y;
    float w;
    float h;
    float minDepth;
    float maxDepth;
} SDL_GpuViewport;

typedef struct SDL_GpuTextureTransferInfo
{
    SDL_GpuTransferBuffer *transferBuffer;
    Uint32 offset;      /* starting location of the image data */
    Uint32 imagePitch;  /* number of pixels from one row to the next */
    Uint32 imageHeight; /* number of rows from one layer/depth-slice to the next */
} SDL_GpuTextureTransferInfo;

typedef struct SDL_GpuTransferBufferLocation
{
    SDL_GpuTransferBuffer *transferBuffer;
    Uint32 offset;
} SDL_GpuTransferBufferLocation;

typedef struct SDL_GpuTransferBufferRegion
{
    SDL_GpuTransferBuffer *transferBuffer;
    Uint32 offset;
    Uint32 size;
} SDL_GpuTransferBufferRegion;

typedef struct SDL_GpuTextureSlice
{
    SDL_GpuTexture *texture;
    Uint32 mipLevel;
    Uint32 layer;
} SDL_GpuTextureSlice;

typedef struct SDL_GpuTextureLocation
{
    SDL_GpuTextureSlice textureSlice;
    Uint32 x;
    Uint32 y;
    Uint32 z;
} SDL_GpuTextureLocation;

typedef struct SDL_GpuTextureRegion
{
    SDL_GpuTextureSlice textureSlice;
    Uint32 x;
    Uint32 y;
    Uint32 z;
    Uint32 w;
    Uint32 h;
    Uint32 d;
} SDL_GpuTextureRegion;

typedef struct SDL_GpuBufferLocation
{
    SDL_GpuBuffer *buffer;
    Uint32 offset;
} SDL_GpuBufferLocation;

typedef struct SDL_GpuBufferRegion
{
    SDL_GpuBuffer *buffer;
    Uint32 offset;
    Uint32 size;
} SDL_GpuBufferRegion;

typedef struct SDL_GpuIndirectDrawCommand
{
    Uint32 vertexCount;   /* number of vertices to draw */
    Uint32 instanceCount; /* number of instances to draw */
    Uint32 firstVertex;   /* index of the first vertex to draw */
    Uint32 firstInstance; /* ID of the first instance to draw */
} SDL_GpuIndirectDrawCommand;

typedef struct SDL_GpuIndexedIndirectDrawCommand
{
    Uint32 indexCount;    /* number of vertices to draw */
    Uint32 instanceCount; /* number of instances to draw */
    Uint32 firstIndex;    /* base index within the index buffer */
    Uint32 vertexOffset;  /* value added to vertex index before indexing into the vertex buffer */
    Uint32 firstInstance; /* ID of the first instance to draw */
} SDL_GpuIndexedIndirectDrawCommand;

/* State structures */

typedef struct SDL_GpuSamplerCreateInfo
{
    SDL_GpuFilter minFilter;
    SDL_GpuFilter magFilter;
    SDL_GpuSamplerMipmapMode mipmapMode;
    SDL_GpuSamplerAddressMode addressModeU;
    SDL_GpuSamplerAddressMode addressModeV;
    SDL_GpuSamplerAddressMode addressModeW;
    float mipLodBias;
    SDL_bool anisotropyEnable;
    float maxAnisotropy;
    SDL_bool compareEnable;
    SDL_GpuCompareOp compareOp;
    float minLod;
    float maxLod;
} SDL_GpuSamplerCreateInfo;

typedef struct SDL_GpuVertexBinding
{
    Uint32 binding;
    Uint32 stride;
    SDL_GpuVertexInputRate inputRate;
    Uint32 stepRate;
} SDL_GpuVertexBinding;

typedef struct SDL_GpuVertexAttribute
{
    Uint32 location;
    Uint32 binding;
    SDL_GpuVertexElementFormat format;
    Uint32 offset;
} SDL_GpuVertexAttribute;

typedef struct SDL_GpuVertexInputState
{
    const SDL_GpuVertexBinding *vertexBindings;
    Uint32 vertexBindingCount;
    const SDL_GpuVertexAttribute *vertexAttributes;
    Uint32 vertexAttributeCount;
} SDL_GpuVertexInputState;

typedef struct SDL_GpuStencilOpState
{
    SDL_GpuStencilOp failOp;
    SDL_GpuStencilOp passOp;
    SDL_GpuStencilOp depthFailOp;
    SDL_GpuCompareOp compareOp;
} SDL_GpuStencilOpState;

typedef struct SDL_GpuColorAttachmentBlendState
{
    SDL_bool blendEnable;
    SDL_GpuBlendFactor srcColorBlendFactor;
    SDL_GpuBlendFactor dstColorBlendFactor;
    SDL_GpuBlendOp colorBlendOp;
    SDL_GpuBlendFactor srcAlphaBlendFactor;
    SDL_GpuBlendFactor dstAlphaBlendFactor;
    SDL_GpuBlendOp alphaBlendOp;
    SDL_GpuColorComponentFlags colorWriteMask;
} SDL_GpuColorAttachmentBlendState;

typedef struct SDL_GpuShaderCreateInfo
{
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GpuShaderFormat format;
    SDL_GpuShaderStage stage;
    Uint32 samplerCount;
    Uint32 storageTextureCount;
    Uint32 storageBufferCount;
    Uint32 uniformBufferCount;
} SDL_GpuShaderCreateInfo;

typedef struct SDL_GpuTextureCreateInfo
{
    Uint32 width;
    Uint32 height;
    Uint32 depth;
    SDL_bool isCube;
    Uint32 layerCount;
    Uint32 levelCount;
    SDL_GpuSampleCount sampleCount;
    SDL_GpuTextureFormat format;
    SDL_GpuTextureUsageFlags usageFlags;
} SDL_GpuTextureCreateInfo;

/* Pipeline state structures */

typedef struct SDL_GpuRasterizerState
{
    SDL_GpuFillMode fillMode;
    SDL_GpuCullMode cullMode;
    SDL_GpuFrontFace frontFace;
    SDL_bool depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
} SDL_GpuRasterizerState;

typedef struct SDL_GpuMultisampleState
{
    SDL_GpuSampleCount multisampleCount;
    Uint32 sampleMask;
} SDL_GpuMultisampleState;

typedef struct SDL_GpuDepthStencilState
{
    SDL_bool depthTestEnable;
    SDL_bool depthWriteEnable;
    SDL_GpuCompareOp compareOp;
    SDL_bool stencilTestEnable;
    SDL_GpuStencilOpState backStencilState;
    SDL_GpuStencilOpState frontStencilState;
    Uint32 compareMask;
    Uint32 writeMask;
    Uint32 reference;
} SDL_GpuDepthStencilState;

typedef struct SDL_GpuColorAttachmentDescription
{
    SDL_GpuTextureFormat format;
    SDL_GpuColorAttachmentBlendState blendState;
} SDL_GpuColorAttachmentDescription;

typedef struct SDL_GpuGraphicsPipelineAttachmentInfo
{
    SDL_GpuColorAttachmentDescription *colorAttachmentDescriptions;
    Uint32 colorAttachmentCount;
    SDL_bool hasDepthStencilAttachment;
    SDL_GpuTextureFormat depthStencilFormat;
} SDL_GpuGraphicsPipelineAttachmentInfo;

typedef struct SDL_GpuGraphicsPipelineCreateInfo
{
    SDL_GpuShader *vertexShader;
    SDL_GpuShader *fragmentShader;
    SDL_GpuVertexInputState vertexInputState;
    SDL_GpuPrimitiveType primitiveType;
    SDL_GpuRasterizerState rasterizerState;
    SDL_GpuMultisampleState multisampleState;
    SDL_GpuDepthStencilState depthStencilState;
    SDL_GpuGraphicsPipelineAttachmentInfo attachmentInfo;
    float blendConstants[4];
} SDL_GpuGraphicsPipelineCreateInfo;

typedef struct SDL_GpuComputePipelineCreateInfo
{
    size_t codeSize;
    const Uint8 *code;
    const char *entryPointName;
    SDL_GpuShaderFormat format;
    Uint32 readOnlyStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 readWriteStorageTextureCount;
    Uint32 readWriteStorageBufferCount;
    Uint32 uniformBufferCount;
    Uint32 threadCountX;
    Uint32 threadCountY;
    Uint32 threadCountZ;
} SDL_GpuComputePipelineCreateInfo;

typedef struct SDL_GpuColorAttachmentInfo
{
    /* The texture slice that will be used as a color attachment by a render pass. */
    SDL_GpuTextureSlice textureSlice;

    /* Can be ignored by RenderPass if CLEAR is not used */
    SDL_GpuColor clearColor;

    /* Determines what is done with the texture slice at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the data currently in the texture slice.
     *
     *   CLEAR:
     *     Clears the texture slice to a single color.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture slice memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GpuLoadOp loadOp;

    /* Determines what is done with the texture slice at the end of the render pass.
     *
     *   STORE:
     *     Stores the results of the render pass in the texture slice.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture slice memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GpuStoreOp storeOp;

    /* if SDL_TRUE, cycles the texture if the texture slice is bound and loadOp is not LOAD */
    SDL_bool cycle;
} SDL_GpuColorAttachmentInfo;

typedef struct SDL_GpuDepthStencilAttachmentInfo
{
    /* The texture slice that will be used as the depth stencil attachment by a render pass. */
    SDL_GpuTextureSlice textureSlice;

    /* Can be ignored by the render pass if CLEAR is not used */
    SDL_GpuDepthStencilValue depthStencilClearValue;

    /* Determines what is done with the depth values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the depth values currently in the texture slice.
     *
     *   CLEAR:
     *     Clears the texture slice to a single depth.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GpuLoadOp loadOp;

    /* Determines what is done with the depth values at the end of the render pass.
     *
     *   STORE:
     *     Stores the depth results in the texture slice.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture slice memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GpuStoreOp storeOp;

    /* Determines what is done with the stencil values at the beginning of the render pass.
     *
     *   LOAD:
     *     Loads the stencil values currently in the texture slice.
     *
     *   CLEAR:
     *     Clears the texture slice to a single stencil value.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the memory.
     *     This is a good option if you know that every single pixel will be touched in the render pass.
     */
    SDL_GpuLoadOp stencilLoadOp;

    /* Determines what is done with the stencil values at the end of the render pass.
     *
     *   STORE:
     *     Stores the stencil results in the texture slice.
     *
     *   DONT_CARE:
     *     The driver will do whatever it wants with the texture slice memory.
     *     This is often a good option for depth/stencil textures.
     */
    SDL_GpuStoreOp stencilStoreOp;

    /* if SDL_TRUE, cycles the texture if the texture slice is bound and any load ops are not LOAD */
    SDL_bool cycle;
} SDL_GpuDepthStencilAttachmentInfo;

/* Binding structs */

typedef struct SDL_GpuBufferBinding
{
    SDL_GpuBuffer *buffer;
    Uint32 offset;
} SDL_GpuBufferBinding;

typedef struct SDL_GpuTextureSamplerBinding
{
    SDL_GpuTexture *texture;
    SDL_GpuSampler *sampler;
} SDL_GpuTextureSamplerBinding;

typedef struct SDL_GpuStorageBufferReadWriteBinding
{
    SDL_GpuBuffer *buffer;

    /* if SDL_TRUE, cycles the buffer if it is bound. */
    SDL_bool cycle;
} SDL_GpuStorageBufferReadWriteBinding;

typedef struct SDL_GpuStorageTextureReadWriteBinding
{
    SDL_GpuTextureSlice textureSlice;

    /* if SDL_TRUE, cycles the texture if the texture slice is bound. */
    SDL_bool cycle;
} SDL_GpuStorageTextureReadWriteBinding;

/* Functions */

/* Device */

/**
 * Creates a GPU context.
 *
 * Backends will first be checked for availability in order of bitflags passed using preferredBackends. If none of the backends are available, the remaining backends are checked as fallback renderers.
 *
 * Think of "preferred" backends as those that have pre-built shaders readily available - for example, you would set the SDL_GPU_BACKEND_VULKAN bit if your game includes SPIR-V shaders. If you generate shaders at runtime (i.e. via SDL_shader) and the library does _not_ provide you with a preferredBackends value, you should pass SDL_GPU_BACKEND_ALL so that updated versions of SDL can be aware of which backends the application was aware of at compile time. SDL_GPU_BACKEND_INVALID is an accepted value but is not recommended.
 *
 * \param preferredBackends a bitflag containing the renderers most recognized by the application
 * \param debugMode enable debug mode properties and validations
 * \returns a GPU context on success or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSelectBackend
 * \sa SDL_GpuDestroyDevice
 */
extern SDL_DECLSPEC SDL_GpuDevice *SDLCALL SDL_GpuCreateDevice(
    SDL_GpuBackend preferredBackends,
    SDL_bool debugMode);

/**
 * Destroys a GPU context previously returned by SDL_GpuCreateDevice.
 *
 * \param device a GPU Context to destroy
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuCreateDevice
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDestroyDevice(SDL_GpuDevice *device);

/**
 * Returns the backend used to create this GPU context.
 *
 * \param device a GPU context to query
 * \returns an SDL_GpuBackend value, or SDL_GPU_BACKEND_INVALID on error
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSelectBackend
 */
extern SDL_DECLSPEC SDL_GpuBackend SDLCALL SDL_GpuGetBackend(SDL_GpuDevice *device);

/* State Creation */

/**
 * Creates a pipeline object to be used in a compute workflow.
 *
 * \param device a GPU Context
 * \param computePipelineCreateInfo a struct describing the state of the requested compute pipeline
 * \returns a compute pipeline object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuBindComputePipeline
 * \sa SDL_GpuReleaseComputePipeline
 */
extern SDL_DECLSPEC SDL_GpuComputePipeline *SDLCALL SDL_GpuCreateComputePipeline(
    SDL_GpuDevice *device,
    SDL_GpuComputePipelineCreateInfo *computePipelineCreateInfo);

/**
 * Creates a pipeline object to be used in a graphics workflow.
 *
 * \param device a GPU Context
 * \param pipelineCreateInfo a struct describing the state of the desired graphics pipeline
 * \returns a graphics pipeline object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuCreateShader
 * \sa SDL_GpuBindGraphicsPipeline
 * \sa SDL_GpuReleaseGraphicsPipeline
 */
extern SDL_DECLSPEC SDL_GpuGraphicsPipeline *SDLCALL SDL_GpuCreateGraphicsPipeline(
    SDL_GpuDevice *device,
    SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo);

/**
 * Creates a sampler object to be used when binding textures in a graphics workflow.
 *
 * \param device a GPU Context
 * \param samplerCreateInfo a struct describing the state of the desired sampler
 * \returns a sampler object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuBindVertexSamplers
 * \sa SDL_GpuBindFragmentSamplers
 * \sa SDL_ReleaseSampler
 */
extern SDL_DECLSPEC SDL_GpuSampler *SDLCALL SDL_GpuCreateSampler(
    SDL_GpuDevice *device,
    SDL_GpuSamplerCreateInfo *samplerCreateInfo);

/**
 * Creates a shader to be used when creating a graphics pipeline.
 *
 * \param device a GPU Context
 * \param shaderCreateInfo a struct describing the state of the desired shader
 * \returns a shader object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuCreateGraphicsPipeline
 * \sa SDL_GpuReleaseShader
 */
extern SDL_DECLSPEC SDL_GpuShader *SDLCALL SDL_GpuCreateShader(
    SDL_GpuDevice *device,
    SDL_GpuShaderCreateInfo *shaderCreateInfo);

/**
 * Creates a texture object to be used in graphics or compute workflows.
 * The contents of this texture are undefined until data is written to the texture.
 *
 * Note that certain combinations of usage flags are invalid.
 * For example, a texture cannot have both the SAMPLER and GRAPHICS_STORAGE_READ flags.
 *
 * If you request a sample count higher than the hardware supports,
 * the implementation will automatically fall back to the highest available sample count.
 *
 * \param device a GPU Context
 * \param textureCreateInfo a struct describing the state of the texture to create
 * \returns a texture object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuUploadToTexture
 * \sa SDL_GpuDownloadFromTexture
 * \sa SDL_GpuBindVertexSamplers
 * \sa SDL_GpuBindVertexStorageTextures
 * \sa SDL_GpuBindFragmentSamplers
 * \sa SDL_GpuBindFragmentStorageTextures
 * \sa SDL_GpuBindComputeStorageTextures
 * \sa SDL_GpuBlit
 * \sa SDL_GpuReleaseTexture
 */
extern SDL_DECLSPEC SDL_GpuTexture *SDLCALL SDL_GpuCreateTexture(
    SDL_GpuDevice *device,
    SDL_GpuTextureCreateInfo *textureCreateInfo);

/**
 * Creates a buffer object to be used in graphics or compute workflows.
 * The contents of this buffer are undefined until data is written to the buffer.
 *
 * Note that certain combinations of usage flags are invalid.
 * For example, a buffer cannot have both the VERTEX and INDEX flags.
 *
 * \param device a GPU Context
 * \param usageFlags bitflag mask hinting at how the buffer will be used
 * \param sizeInBytes the size of the buffer
 * \returns a buffer object on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuUploadToBuffer
 * \sa SDL_GpuBindVertexBuffers
 * \sa SDL_GpuBindIndexBuffer
 * \sa SDL_GpuBindVertexStorageBuffers
 * \sa SDL_GpuBindFragmentStorageBuffers
 * \sa SDL_GpuBindComputeStorageBuffers
 * \sa SDL_GpuReleaseBuffer
 */
extern SDL_DECLSPEC SDL_GpuBuffer *SDLCALL SDL_GpuCreateBuffer(
    SDL_GpuDevice *device,
    SDL_GpuBufferUsageFlags usageFlags,
    Uint32 sizeInBytes);

/**
 * Creates a transfer buffer to be used when uploading to or downloading from graphics resources.
 *
 * \param device a GPU Context
 * \param usage whether the transfer buffer will be used for uploads or downloads
 * \param sizeInBytes the size of the transfer buffer
 * \returns a transfer buffer on success, or NULL on failure
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuUploadToBuffer
 * \sa SDL_GpuDownloadFromBuffer
 * \sa SDL_GpuUploadToTexture
 * \sa SDL_GpuDownloadFromTexture
 * \sa SDL_GpuReleaseTransferBuffer
 */
extern SDL_DECLSPEC SDL_GpuTransferBuffer *SDLCALL SDL_GpuCreateTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBufferUsage usage,
    Uint32 sizeInBytes);

/* Debug Naming */

/**
 * Sets an arbitrary string constant to label a buffer. Useful for debugging.
 *
 * \param device a GPU Context
 * \param buffer a buffer to attach the name to
 * \param text a UTF-8 string constant to mark as the name of the buffer
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetBufferName(
    SDL_GpuDevice *device,
    SDL_GpuBuffer *buffer,
    const char *text);

/**
 * Sets an arbitrary string constant to label a texture. Useful for debugging.
 *
 * \param device a GPU Context
 * \param texture a texture to attach the name to
 * \param text a UTF-8 string constant to mark as the name of the texture
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetTextureName(
    SDL_GpuDevice *device,
    SDL_GpuTexture *texture,
    const char *text);

/**
 * Inserts an arbitrary string label into the command buffer callstream.
 * Useful for debugging.
 *
 * \param commandBuffer a command buffer
 * \param text a UTF-8 string constant to insert as the label
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuInsertDebugLabel(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *text);

/**
 * Begins a debug group with an arbitary name.
 * Used for denoting groups of calls when viewing the command buffer callstream
 * in a graphics debugging tool.
 *
 * Each call to SDL_GpuPushDebugGroup must have a corresponding call to SDL_GpuPopDebugGroup.
 *
 * On some backends (e.g. Metal), pushing a debug group during a render/blit/compute pass
 * will create a group that is scoped to the native pass rather than the command buffer.
 * For best results, if you push a debug group during a pass, always pop it in the same pass.
 *
 * \param commandBuffer a command buffer
 * \param name a UTF-8 string constant that names the group
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuPopDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuPushDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *name);

/**
 * Ends the most-recently pushed debug group.
 *
 * \param commandBuffer a command buffer
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuPushDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuPopDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer);

/* Disposal */

/**
 * Frees the given texture as soon as it is safe to do so.
 * You must not reference the texture after calling this function.
 *
 * \param device a GPU context
 * \param texture a texture to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseTexture(
    SDL_GpuDevice *device,
    SDL_GpuTexture *texture);

/**
 * Frees the given sampler as soon as it is safe to do so.
 * You must not reference the texture after calling this function.
 *
 * \param device a GPU context
 * \param sampler a sampler to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseSampler(
    SDL_GpuDevice *device,
    SDL_GpuSampler *sampler);

/**
 * Frees the given buffer as soon as it is safe to do so.
 * You must not reference the buffer after calling this function.
 *
 * \param device a GPU context
 * \param buffer a buffer to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseBuffer(
    SDL_GpuDevice *device,
    SDL_GpuBuffer *buffer);

/**
 * Frees the given transfer buffer as soon as it is safe to do so.
 * You must not reference the transfer buffer after calling this function.
 *
 * \param device a GPU context
 * \param transferBuffer a transfer buffer to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer);

/**
 * Frees the given compute pipeline as soon as it is safe to do so.
 * You must not reference the compute pipeline after calling this function.
 *
 * \param device a GPU context
 * \param computePipeline a compute pipeline to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseComputePipeline(
    SDL_GpuDevice *device,
    SDL_GpuComputePipeline *computePipeline);

/**
 * Frees the given shader as soon as it is safe to do so.
 * You must not reference the shader after calling this function.
 *
 * \param device a GPU context
 * \param shader a shader to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseShader(
    SDL_GpuDevice *device,
    SDL_GpuShader *shader);

/**
 * Frees the given graphics pipeline as soon as it is safe to do so.
 * You must not reference the graphics pipeline after calling this function.
 *
 * \param device a GPU context
 * \param graphicsPipeline a graphics pipeline to be destroyed
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseGraphicsPipeline(
    SDL_GpuDevice *device,
    SDL_GpuGraphicsPipeline *graphicsPipeline);

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
 * GpuTransferBuffer, GpuBuffer, and GpuTexture all effectively function as ring buffers on internal resources.
 * When cycle is SDL_TRUE, if the resource is bound, the cycle rotates to the next unbound internal resource,
 * or if none are available, a new one is created.
 * This means you don't have to worry about complex state tracking and synchronization as long as cycling is correctly employed.
 *
 * For example: you can call SetTransferData and then UploadToTexture. The next time you call SetTransferData,
 * if you set the cycle param to SDL_TRUE, you don't have to worry about overwriting any data that is not yet uploaded.
 *
 * Another example: If you are using a texture in a render pass every frame, this can cause a data dependency between frames.
 * If you set cycle to SDL_TRUE in the ColorAttachmentInfo struct, you can prevent this data dependency.
 *
 * Note that all functions which write to a texture specifically write to a GpuTextureSlice,
 * and these slices themselves are tracked for binding.
 * The GpuTexture will only cycle if the specific GpuTextureSlice being written to is bound.
 *
 * Cycling will never undefine already bound data.
 * When cycling, all data in the resource is considered to be undefined for subsequent commands until that data is written again.
 * You must take care not to read undefined data.
 *
 * You must also take care not to overwrite a section of data that has been referenced in a command without cycling first.
 * It is OK to overwrite unreferenced data in a bound resource without cycling,
 * but overwriting a section of data that has already been referenced will produce unexpected results.
 */

/* Graphics State */

/**
 * Begins a render pass on a command buffer.
 * A render pass consists of a set of texture slices, clear values, and load/store operations
 * which will be rendered to during the render pass.
 * All operations related to graphics pipelines must take place inside of a render pass.
 * A default viewport and scissor state are automatically set when this is called.
 * You cannot begin another render pass, or begin a compute pass or copy pass
 * until you have ended the render pass.
 *
 * \param commandBuffer a command buffer
 * \param colorAttachmentInfos an array of SDL_GpuColorAttachmentInfo structs
 * \param colorAttachmentCount the number of color attachments in the colorAttachmentInfos array
 * \param depthStencilAttachmentInfo the depth-stencil target and clear value, may be NULL
 * \returns a render pass handle
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuEndRenderPass
 */
extern SDL_DECLSPEC SDL_GpuRenderPass *SDLCALL SDL_GpuBeginRenderPass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo);

/**
 * Binds a graphics pipeline on a render pass to be used in rendering.
 * A graphics pipeline must be bound before making any draw calls.
 *
 * \param renderPass a render pass handle
 * \param graphicsPipeline the graphics pipeline to bind
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindGraphicsPipeline(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuGraphicsPipeline *graphicsPipeline);

/**
 * Sets the current viewport state on a command buffer.
 *
 * \param renderPass a render pass handle
 * \param viewport the viewport to set
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetViewport(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuViewport *viewport);

/**
 * Sets the current scissor state on a command buffer.
 *
 * \param renderPass a render pass handle
 * \param scissor the scissor area to set
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetScissor(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuRect *scissor);

/**
 * Binds vertex buffers on a command buffer for use with subsequent draw calls.
 *
 * \param renderPass a render pass handle
 * \param firstBinding the starting bind point for the vertex buffers
 * \param pBindings an array of SDL_GpuBufferBinding structs containing vertex buffers and offset values
 * \param bindingCount the number of bindings in the pBindings array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindVertexBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstBinding,
    SDL_GpuBufferBinding *pBindings,
    Uint32 bindingCount);

/**
 * Binds an index buffer on a command buffer for use with subsequent draw calls.
 *
 * \param renderPass a render pass handle
 * \param pBinding a pointer to a struct containing an index buffer and offset
 * \param indexElementSize whether the index values in the buffer are 16- or 32-bit
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindIndexBuffer(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBufferBinding *pBinding,
    SDL_GpuIndexElementSize indexElementSize);

/**
 * Binds texture-sampler pairs for use on the vertex shader.
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the vertex sampler slot to begin binding from
 * \param textureSamplerBindings an array of texture-sampler binding structs
 * \param bindingCount the number of texture-sampler pairs to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindVertexSamplers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount);

/**
 * Binds storage textures for use on the vertex shader.
 * These textures must have been created with SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the vertex storage texture slot to begin binding from
 * \param storageTextureSlices an array of storage texture slices
 * \param bindingCount the number of storage texture slices to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindVertexStorageTextures(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount);

/**
 * Binds storage buffers for use on the vertex shader.
 * These buffers must have been created with SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the vertex storage buffer slot to begin binding from
 * \param storageBuffers an array of buffers
 * \param bindingCount the number of buffers to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindVertexStorageBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount);

/**
 * Binds texture-sampler pairs for use on the fragment shader.
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the fragment sampler slot to begin binding from
 * \param textureSamplerBindings an array of texture-sampler binding structs
 * \param bindingCount the number of texture-sampler pairs to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindFragmentSamplers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount);

/**
 * Binds storage textures for use on the fragment shader.
 * These textures must have been created with SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the fragment storage texture slot to begin binding from
 * \param storageTextureSlices an array of storage texture slices
 * \param bindingCount the number of storage texture slices to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindFragmentStorageTextures(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount);

/**
 * Binds storage buffers for use on the fragment shader.
 * These buffers must have been created with SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT.
 *
 * \param renderPass a render pass handle
 * \param firstSlot the fragment storage buffer slot to begin binding from
 * \param storageBuffers an array of storage buffers
 * \param bindingCount the number of storage buffers to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindFragmentStorageBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount);

/**
 * Pushes data to a vertex uniform slot on the bound graphics pipeline.
 * Subsequent draw calls will use this uniform data.
 *
 * \param renderPass a render pass handle
 * \param slotIndex the vertex uniform slot to push data to
 * \param data client data to write
 * \param dataLengthInBytes the length of the data to write
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuPushVertexUniformData(
    SDL_GpuRenderPass *renderPass,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/**
 * Pushes data to a fragment uniform slot on the bound graphics pipeline.
 * Subsequent draw calls will use this uniform data.
 *
 * \param renderPass a render pass handle
 * \param slotIndex the fragment uniform slot to push data to
 * \param data client data to write
 * \param dataLengthInBytes the length of the data to write
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuPushFragmentUniformData(
    SDL_GpuRenderPass *renderPass,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/* Drawing */

/**
 * Draws data using bound graphics state with an index buffer and instancing enabled.
 * You must not call this function before binding a graphics pipeline.
 *
 * \param renderPass a render pass handle
 * \param baseVertex the starting offset to read from the vertex buffer
 * \param startIndex the starting offset to read from the index buffer
 * \param primitiveCount the number of primitives to draw
 * \param instanceCount the number of instances that will be drawn
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDrawIndexedPrimitives(
    SDL_GpuRenderPass *renderPass,
    Uint32 baseVertex,
    Uint32 startIndex,
    Uint32 primitiveCount,
    Uint32 instanceCount);

/**
 * Draws data using bound graphics state.
 * You must not call this function before binding a graphics pipeline.
 *
 * \param renderPass a render pass handle
 * \param vertexStart The starting offset to read from the vertex buffer
 * \param primitiveCount The number of primitives to draw
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDrawPrimitives(
    SDL_GpuRenderPass *renderPass,
    Uint32 vertexStart,
    Uint32 primitiveCount);

/**
 * Draws data using bound graphics state and with draw parameters set from a buffer.
 * The buffer layout should match the layout of SDL_GpuIndirectDrawCommand.
 * You must not call this function before binding a graphics pipeline.
 *
 * \param renderPass a render pass handle
 * \param buffer a buffer containing draw parameters
 * \param offsetInBytes the offset to start reading from the draw buffer
 * \param drawCount the number of draw parameter sets that should be read from the draw buffer
 * \param stride the byte stride between sets of draw parameters
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDrawPrimitivesIndirect(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride);

/**
 * Draws data using bound graphics state with an index buffer enabled
 * and with draw parameters set from a buffer.
 * The buffer layout should match the layout of SDL_GpuIndexedIndirectDrawCommand.
 * You must not call this function before binding a graphics pipeline.
 *
 * \param renderPass a render pass handle
 * \param buffer a buffer containing draw parameters
 * \param offsetInBytes the offset to start reading from the draw buffer
 * \param drawCount the number of draw parameter sets that should be read from the draw buffer
 * \param stride the byte stride between sets of draw parameters
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDrawIndexedPrimitivesIndirect(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride);

/**
 * Ends the given render pass.
 * All bound graphics state on the render pass command buffer is unset.
 * The render pass handle is now invalid.
 *
 * \param renderPass a render pass handle
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuEndRenderPass(
    SDL_GpuRenderPass *renderPass);

/* Compute Pass */

/**
 * Begins a compute pass on a command buffer.
 * A compute pass is defined by a set of texture slices and buffers that
 * will be written to by compute pipelines.
 * These textures and buffers must have been created with the COMPUTE_STORAGE_WRITE bit.
 * If these resources will also be read during the pass, they must be created with the COMPUTE_STORAGE_READ bit.
 * All operations related to compute pipelines must take place inside of a compute pass.
 * You must not begin another compute pass, or a render pass or copy pass
 * before ending the compute pass.
 *
 * \param commandBuffer a command buffer
 * \param storageTextureBindings an array of writeable storage texture binding structs
 * \param storageTextureBindingCount the number of storage textures to bind from the array
 * \param storageBufferBindings an array of writeable storage buffer binding structs
 * \param storageBufferBindingCount an array of read-write storage buffer binding structs
 *
 * \returns a compute pass handle
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuEndComputePass
 */
extern SDL_DECLSPEC SDL_GpuComputePass *SDLCALL SDL_GpuBeginComputePass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuStorageTextureReadWriteBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GpuStorageBufferReadWriteBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount);

/**
 * Binds a compute pipeline on a command buffer for use in compute dispatch.
 *
 * \param computePass a compute pass handle
 * \param computePipeline a compute pipeline to bind
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindComputePipeline(
    SDL_GpuComputePass *computePass,
    SDL_GpuComputePipeline *computePipeline);

/**
 * Binds storage textures as readonly for use on the compute pipeline.
 * These textures must have been created with SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT.
 *
 * \param computePass a compute pass handle
 * \param firstSlot the compute storage texture slot to begin binding from
 * \param storageTextureSlices an array of storage texture binding structs
 * \param bindingCount the number of storage textures to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindComputeStorageTextures(
    SDL_GpuComputePass *computePass,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount);

/**
 * Binds storage buffers as readonly for use on the compute pipeline.
 * These buffers must have been created with SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT.
 *
 * \param computePass a compute pass handle
 * \param firstSlot the compute storage buffer slot to begin binding from
 * \param storageBuffers an array of storage buffer binding structs
 * \param bindingCount the number of storage buffers to bind from the array
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBindComputeStorageBuffers(
    SDL_GpuComputePass *computePass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount);

/**
 * Pushes data to a uniform slot on the bound compute pipeline.
 * Subsequent draw calls will use this uniform data.
 *
 * \param computePass a compute pass handle
 * \param slotIndex the uniform slot to push data to
 * \param data client data to write
 * \param dataLengthInBytes the length of the data to write
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuPushComputeUniformData(
    SDL_GpuComputePass *computePass,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes);

/**
 * Dispatches compute work.
 * You must not call this function before binding a compute pipeline.
 *
 * A VERY IMPORTANT NOTE
 * If you dispatch multiple times in a compute pass,
 * and the dispatches write to the same resource region as each other,
 * there is no guarantee of which order the writes will occur.
 * If the write order matters, you MUST end the compute pass and begin another one.
 *
 * \param computePass a compute pass handle
 * \param groupCountX number of local workgroups to dispatch in the X dimension
 * \param groupCountY number of local workgroups to dispatch in the Y dimension
 * \param groupCountZ number of local workgroups to dispatch in the Z dimension
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDispatchCompute(
    SDL_GpuComputePass *computePass,
    Uint32 groupCountX,
    Uint32 groupCountY,
    Uint32 groupCountZ);

/**
 * Ends the current compute pass.
 * All bound compute state on the command buffer is unset.
 * The compute pass handle is now invalid.
 *
 * \param computePass a compute pass handle
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuEndComputePass(
    SDL_GpuComputePass *computePass);

/* TransferBuffer Data */

/**
 * Maps a transfer buffer into application address space.
 * You must unmap the transfer buffer before encoding upload commands.
 *
 * \param device a GPU context
 * \param transferBuffer a transfer buffer
 * \param cycle if SDL_TRUE, cycles the transfer buffer if it is bound
 * \param ppData where to store the address of the mapped transfer buffer memory
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuMapTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_bool cycle,
    void **ppData);

/**
 * Unmaps a previously mapped transfer buffer.
 *
 * \param device a GPU context
 * \param transferBuffer a previously mapped transfer buffer
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuUnmapTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer);

/**
 * Immediately copies data from a pointer to a transfer buffer.
 *
 * \param device a GPU context
 * \param source a pointer to data to copy into the transfer buffer
 * \param destination a transfer buffer with offset and size
 * \param cycle if SDL_TRUE, cycles the transfer buffer if it is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetTransferData(
    SDL_GpuDevice *device,
    const void *source,
    SDL_GpuTransferBufferRegion *destination,
    SDL_bool cycle);

/**
 * Immediately copies data from a transfer buffer to a pointer.
 *
 * \param device a GPU context
 * \param source a transfer buffer with offset and size
 * \param destination a data pointer
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuGetTransferData(
    SDL_GpuDevice *device,
    SDL_GpuTransferBufferRegion *source,
    void *destination);

/* Copy Pass */

/**
 * Begins a copy pass on a command buffer.
 * All operations related to copying to or from buffers or textures take place inside a copy pass.
 * You must not begin another copy pass, or a render pass or compute pass
 * before ending the copy pass.
 *
 * \param commandBuffer a command buffer
 * \returns a copy pass handle
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_GpuCopyPass *SDLCALL SDL_GpuBeginCopyPass(
    SDL_GpuCommandBuffer *commandBuffer);

/**
 * Uploads data from a transfer buffer to a texture.
 * The upload occurs on the GPU timeline.
 * You may assume that the upload has finished in subsequent commands.
 *
 * You must align the data in the transfer buffer to a multiple of
 * the texel size of the texture format.
 *
 * \param copyPass a copy pass handle
 * \param source the source transfer buffer with image layout information
 * \param destination the destination texture region
 * \param cycle if SDL_TRUE, cycles the texture if the texture slice is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuUploadToTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureTransferInfo *source,
    SDL_GpuTextureRegion *destination,
    SDL_bool cycle);

/* Uploads data from a TransferBuffer to a Buffer. */

/**
 * Uploads data from a transfer buffer to a buffer.
 * The upload occurs on the GPU timeline.
 * You may assume that the upload has finished in subsequent commands.
 *
 * \param copyPass a copy pass handle
 * \param source the source transfer buffer with offset
 * \param destination the destination buffer with offset and size
 * \param cycle if SDL_TRUE, cycles the buffer if it is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuUploadToBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTransferBufferLocation *source,
    SDL_GpuBufferRegion *destination,
    SDL_bool cycle);

/**
 * Performs a texture-to-texture copy.
 * This copy occurs on the GPU timeline.
 * You may assume the copy has finished in subsequent commands.
 *
 * \param copyPass a copy pass handle
 * \param source a source texture region
 * \param destination a destination texture region
 * \param w the width of the region to copy
 * \param h the height of the region to copy
 * \param d the depth of the region to copy
 * \param cycle if SDL_TRUE, cycles the destination texture if the destination texture slice is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuCopyTextureToTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureLocation *source,
    SDL_GpuTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    SDL_bool cycle);

/* Copies data from a buffer to a buffer. */

/**
 * Performs a buffer-to-buffer copy.
 * This copy occurs on the GPU timeline.
 * You may assume the copy has finished in subsequent commands.
 *
 * \param copyPass a copy pass handle
 * \param source the buffer and offset to copy from
 * \param destination the buffer and offset to copy to
 * \param size the length of the buffer to copy
 * \param cycle if SDL_TRUE, cycles the destination buffer if it is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuCopyBufferToBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuBufferLocation *source,
    SDL_GpuBufferLocation *destination,
    Uint32 size,
    SDL_bool cycle);

/**
 * Generates mipmaps for the given texture.
 *
 * \param copyPass a copy pass handle
 * \param texture a texture with more than 1 mip level
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuGenerateMipmaps(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTexture *texture);

/**
 * Copies data from a texture to a transfer buffer on the GPU timeline.
 * This data is not guaranteed to be copied until the command buffer fence is signaled.
 *
 * \param copyPass a copy pass handle
 * \param source the source texture region
 * \param destination the destination transfer buffer with image layout information
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDownloadFromTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureTransferInfo *destination);

/**
 * Copies data from a buffer to a transfer buffer on the GPU timeline.
 * This data is not guaranteed to be copied until the command buffer fence is signaled.
 *
 * \param copyPass a copy pass handle
 * \param source the source buffer with offset and size
 * \param destination the destination transfer buffer with offset
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuDownloadFromBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuBufferRegion *source,
    SDL_GpuTransferBufferLocation *destination);

/**
 * Ends the current copy pass.
 *
 * \param copyPass a copy pass handle
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuEndCopyPass(
    SDL_GpuCopyPass *copyPass);

/**
 * Blits from a source texture region to a destination texture region.
 * This function must not be called inside of any render, compute, or copy pass.
 *
 * \param commandBuffer a command buffer
 * \param source the texture region to copy from
 * \param destination the texture region to copy to
 * \param filterMode the filter mode that will be used when blitting
 * \param cycle if SDL_TRUE, cycles the destination texture if the destination texture slice is bound, otherwise overwrites the data.
 *
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuBlit(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureRegion *destination,
    SDL_GpuFilter filterMode,
    SDL_bool cycle);

/* Submission/Presentation */

/**
 * Obtains whether or not a swapchain composition is supported by the GPU backend.
 *
 * \param device a GPU context
 * \param window an SDL_Window
 * \param swapchainComposition the swapchain composition to check
 *
 * \returns SDL_TRUE if supported, SDL_FALSE if unsupported (or on error)
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GpuSupportsSwapchainComposition(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition);

/**
 * Obtains whether or not a presentation mode is supported by the GPU backend.
 *
 * \param device a GPU context
 * \param window an SDL_Window
 * \param presentMode the presentation mode to check
 *
 * \returns SDL_TRUE if supported, SDL_FALSE if unsupported (or on error)
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GpuSupportsPresentMode(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuPresentMode presentMode);

/**
 * Claims a window, creating a swapchain structure for it.
 * This must be called before SDL_GpuAcquireSwapchainTexture is called using the window.
 *
 * \param device a GPU context
 * \param window an SDL_Window
 * \param swapchainComposition the desired composition of the swapchain
 * \param presentMode the desired present mode for the swapchain
 *
 * \returns SDL_TRUE on success, otherwise SDL_FALSE.
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuAcquireSwapchainTexture
 * \sa SDL_GpuUnclaimWindow
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GpuClaimWindow(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode);

/**
 * Unclaims a window, destroying its swapchain structure.
 *
 * \param device a GPU context
 * \param window an SDL_Window that has been claimed
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuClaimWindow
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuUnclaimWindow(
    SDL_GpuDevice *device,
    SDL_Window *window);

/**
 * Changes the swapchain parameters for the given claimed window.
 *
 * \param device a GPU context
 * \param window an SDL_Window that has been claimed
 * \param swapchainComposition the desired composition of the swapchain
 * \param presentMode the desired present mode for the swapchain
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSetSwapchainParameters(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode);

/**
 * Obtains the texture format of the swapchain for the given window.
 *
 * \param device a GPU context
 * \param window an SDL_Window that has been claimed
 *
 * \returns the texture format of the swapchain
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_GpuTextureFormat SDLCALL SDL_GpuGetSwapchainTextureFormat(
    SDL_GpuDevice *device,
    SDL_Window *window);

/**
 * Acquire a command buffer.
 * This command buffer is managed by the implementation and should not be freed by the user.
 * The command buffer may only be used on the thread it was acquired on.
 * The command buffer should be submitted on the thread it was acquired on.
 *
 * \param device a GPU context
 * \returns a command buffer
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSubmit
 * \sa SDL_GpuSubmitAndAcquireFence
 */
extern SDL_DECLSPEC SDL_GpuCommandBuffer *SDLCALL SDL_GpuAcquireCommandBuffer(
    SDL_GpuDevice *device);

/**
 * Acquire a texture to use in presentation.
 * When a swapchain texture is acquired on a command buffer,
 * it will automatically be submitted for presentation when the command buffer is submitted.
 * The swapchain texture should only be referenced by the command buffer used to acquire it.
 * May return NULL under certain conditions. This is not necessarily an error.
 * This texture is managed by the implementation and must not be freed by the user.
 * You MUST NOT call this function from any thread other than the one that created the window.
 *
 * \param commandBuffer a command buffer
 * \param window a window that has been claimed
 * \param pWidth a pointer filled in with the swapchain width
 * \param pHeight a pointer filled in with the swapchain height
 * \returns a swapchain texture
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuClaimWindow
 * \sa SDL_GpuSubmit
 * \sa SDL_GpuSubmitAndAcquireFence
 */
extern SDL_DECLSPEC SDL_GpuTexture *SDLCALL SDL_GpuAcquireSwapchainTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight);

/**
 * Submits a command buffer so its commands can be processed on the GPU.
 * It is invalid to use the command buffer after this is called.
 *
 * \param commandBuffer a command buffer
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuAcquireCommandBuffer
 * \sa SDL_GpuAcquireSwapchainTexture
 * \sa SDL_GpuSubmitAndAcquireFence
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuSubmit(
    SDL_GpuCommandBuffer *commandBuffer);

/**
 * Submits a command buffer so its commands can be processed on the GPU,
 * and acquires a fence associated with the command buffer.
 * You must release this fence when it is no longer needed or it will cause a leak.
 * It is invalid to use the command buffer after this is called.
 *
 * \param commandBuffer a command buffer
 * \returns a fence associated with the command buffer
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_AcquireCommandBuffer
 * \sa SDL_GpuAcquireSwapchainTexture
 * \sa SDL_GpuSubmit
 * \sa SDL_GpuReleaseFence
 */
extern SDL_DECLSPEC SDL_GpuFence *SDLCALL SDL_GpuSubmitAndAcquireFence(
    SDL_GpuCommandBuffer *commandBuffer);

/**
 * Blocks the thread until the GPU is completely idle.
 *
 * \param device a GPU context
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuWaitForFences
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuWait(
    SDL_GpuDevice *device);

/**
 * Blocks the thread until the given fences are signaled.
 *
 * \param device a GPU context
 * \param waitAll if 0, wait for any fence to be signaled, if 1, wait for all fences to be signaled
 * \param pFences an array of fences to wait on
 * \param fenceCount the number of fences in the pFences array
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSubmitAndAcquireFence
 * \sa SDL_GpuWait
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuWaitForFences(
    SDL_GpuDevice *device,
    SDL_bool waitAll,
    SDL_GpuFence **pFences,
    Uint32 fenceCount);

/**
 * Checks the status of a fence.
 *
 * \param device a GPU context
 * \param fence a fence
 * \returns SDL_TRUE if the fence is signaled, SDL_FALSE if it is not
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSubmitAndAcquireFence
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GpuQueryFence(
    SDL_GpuDevice *device,
    SDL_GpuFence *fence);

/**
 * Releases a fence obtained from SDL_GpuSubmitAndAcquireFence.
 *
 * \param device a GPU context
 * \param fence a fence
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSubmitAndAcquireFence
 */
extern SDL_DECLSPEC void SDLCALL SDL_GpuReleaseFence(
    SDL_GpuDevice *device,
    SDL_GpuFence *fence);

/* Format Info */

/**
 * Obtains the texel block size for a texture format.
 *
 * \param textureFormat the texture format you want to know the texel size of
 * \returns the texel block size of the texture format
 *
 * \since This function is available since SDL 3.x.x
 *
 * \sa SDL_GpuSetTransferData
 * \sa SDL_GpuUploadToTexture
 */
extern SDL_DECLSPEC Uint32 SDLCALL SDL_GpuTextureFormatTexelBlockSize(
    SDL_GpuTextureFormat textureFormat);

/**
 * Determines whether a texture format is supported for a given type and usage.
 *
 * \param device a GPU context
 * \param format the texture format to check
 * \param type the type of texture (2D, 3D, Cube)
 * \param usage a bitmask of all usage scenarios to check
 * \returns whether the texture format is supported for this type and usage
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GpuIsTextureFormatSupported(
    SDL_GpuDevice *device,
    SDL_GpuTextureFormat format,
    SDL_GpuTextureType type,
    SDL_GpuTextureUsageFlags usage);

/**
 * Determines the "best" sample count for a texture format, i.e.
 * the highest supported sample count that is <= the desired sample count.
 *
 * \param device a GPU context
 * \param format the texture format to check
 * \param desiredSampleCount the sample count you want
 * \returns a hardware-specific version of min(preferred, possible)
 *
 * \since This function is available since SDL 3.x.x
 */
extern SDL_DECLSPEC SDL_GpuSampleCount SDLCALL SDL_GpuGetBestSampleCount(
    SDL_GpuDevice *device,
    SDL_GpuTextureFormat format,
    SDL_GpuSampleCount desiredSampleCount);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SDL_GPU_H */
