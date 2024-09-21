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

/**
 * An opaque handle representing the SDL_GPU context.
 *
 * \since This struct is available since SDL 3.0.0
 */
typedef struct SDL_GPUDevice SDL_GPUDevice;

/**
 * An opaque handle representing a buffer.
 *
 * Used for vertices, indices, indirect draw commands, and general compute
 * data.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUBuffer
 * \sa SDL_SetGPUBufferName
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_DownloadFromGPUBuffer
 * \sa SDL_CopyGPUBufferToBuffer
 * \sa SDL_BindGPUVertexBuffers
 * \sa SDL_BindGPUIndexBuffer
 * \sa SDL_BindGPUVertexStorageBuffers
 * \sa SDL_BindGPUFragmentStorageBuffers
 * \sa SDL_DrawGPUPrimitivesIndirect
 * \sa SDL_DrawGPUIndexedPrimitivesIndirect
 * \sa SDL_BindGPUComputeStorageBuffers
 * \sa SDL_DispatchGPUComputeIndirect
 * \sa SDL_ReleaseGPUBuffer
 */
typedef struct SDL_GPUBuffer SDL_GPUBuffer;

/**
 * An opaque handle representing a transfer buffer.
 *
 * Used for transferring data to and from the device.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTransferBuffer
 * \sa SDL_MapGPUTransferBuffer
 * \sa SDL_UnmapGPUTransferBuffer
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUBuffer
 * \sa SDL_DownloadFromGPUTexture
 * \sa SDL_ReleaseGPUTransferBuffer
 */
typedef struct SDL_GPUTransferBuffer SDL_GPUTransferBuffer;

/**
 * An opaque handle representing a texture.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 * \sa SDL_SetGPUTextureName
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUTexture
 * \sa SDL_CopyGPUTextureToTexture
 * \sa SDL_BindGPUVertexSamplers
 * \sa SDL_BindGPUVertexStorageTextures
 * \sa SDL_BindGPUFragmentSamplers
 * \sa SDL_BindGPUFragmentStorageTextures
 * \sa SDL_BindGPUComputeStorageTextures
 * \sa SDL_GenerateMipmapsForGPUTexture
 * \sa SDL_BlitGPUTexture
 * \sa SDL_ReleaseGPUTexture
 */
typedef struct SDL_GPUTexture SDL_GPUTexture;

/**
 * An opaque handle representing a sampler.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUSampler
 * \sa SDL_BindGPUVertexSamplers
 * \sa SDL_BindGPUFragmentSamplers
 * \sa SDL_ReleaseGPUSampler
 */
typedef struct SDL_GPUSampler SDL_GPUSampler;

/**
 * An opaque handle representing a compiled shader object.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUShader
 * \sa SDL_CreateGPUGraphicsPipeline
 * \sa SDL_ReleaseGPUShader
 */
typedef struct SDL_GPUShader SDL_GPUShader;

/**
 * An opaque handle representing a compute pipeline.
 *
 * Used during compute passes.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUComputePipeline
 * \sa SDL_BindGPUComputePipeline
 * \sa SDL_ReleaseGPUComputePipeline
 */
typedef struct SDL_GPUComputePipeline SDL_GPUComputePipeline;

/**
 * An opaque handle representing a graphics pipeline.
 *
 * Used during render passes.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 * \sa SDL_BindGPUGraphicsPipeline
 * \sa SDL_ReleaseGPUGraphicsPipeline
 */
typedef struct SDL_GPUGraphicsPipeline SDL_GPUGraphicsPipeline;

/**
 * An opaque handle representing a command buffer.
 *
 * Most state is managed via command buffers. When setting state using a
 * command buffer, that state is local to the command buffer.
 *
 * Commands only begin execution on the GPU once SDL_SubmitGPUCommandBuffer is
 * called. Once the command buffer is submitted, it is no longer valid to use
 * it.
 *
 * Command buffers are executed in submission order. If you submit command
 * buffer A and then command buffer B all commands in A will begin executing
 * before any command in B begins executing.
 *
 * In multi-threading scenarios, you should acquire and submit a command
 * buffer on the same thread. As long as you satisfy this requirement, all
 * functionality related to command buffers is thread-safe.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_AcquireGPUCommandBuffer
 * \sa SDL_SubmitGPUCommandBuffer
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
typedef struct SDL_GPUCommandBuffer SDL_GPUCommandBuffer;

/**
 * An opaque handle representing a render pass.
 *
 * This handle is transient and should not be held or referenced after
 * SDL_EndGPURenderPass is called.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPURenderPass
 * \sa SDL_EndGPURenderPass
 */
typedef struct SDL_GPURenderPass SDL_GPURenderPass;

/**
 * An opaque handle representing a compute pass.
 *
 * This handle is transient and should not be held or referenced after
 * SDL_EndGPUComputePass is called.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPUComputePass
 * \sa SDL_EndGPUComputePass
 */
typedef struct SDL_GPUComputePass SDL_GPUComputePass;

/**
 * An opaque handle representing a copy pass.
 *
 * This handle is transient and should not be held or referenced after
 * SDL_EndGPUCopyPass is called.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPUCopyPass
 * \sa SDL_EndGPUCopyPass
 */
typedef struct SDL_GPUCopyPass SDL_GPUCopyPass;

/**
 * An opaque handle representing a fence.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 * \sa SDL_QueryGPUFence
 * \sa SDL_WaitForGPUFences
 * \sa SDL_ReleaseGPUFence
 */
typedef struct SDL_GPUFence SDL_GPUFence;

/**
 * Specifies the primitive topology of a graphics pipeline.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUPrimitiveType
{
    SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,  /**< A series of separate triangles. */
    SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, /**< A series of connected triangles. */
    SDL_GPU_PRIMITIVETYPE_LINELIST,      /**< A series of separate lines. */
    SDL_GPU_PRIMITIVETYPE_LINESTRIP,     /**< A series of connected lines. */
    SDL_GPU_PRIMITIVETYPE_POINTLIST      /**< A series of separate points. */
} SDL_GPUPrimitiveType;

/**
 * Specifies how the contents of a texture attached to a render pass are
 * treated at the beginning of the render pass.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPURenderPass
 */
typedef enum SDL_GPULoadOp
{
    SDL_GPU_LOADOP_LOAD,      /**< The previous contents of the texture will be preserved. */
    SDL_GPU_LOADOP_CLEAR,     /**< The contents of the texture will be cleared to a color. */
    SDL_GPU_LOADOP_DONT_CARE  /**< The previous contents of the texture need not be preserved. The contents will be undefined. */
} SDL_GPULoadOp;

/**
 * Specifies how the contents of a texture attached to a render pass are
 * treated at the end of the render pass.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPURenderPass
 */
typedef enum SDL_GPUStoreOp
{
    SDL_GPU_STOREOP_STORE,             /**< The contents generated during the render pass will be written to memory. */
    SDL_GPU_STOREOP_DONT_CARE,         /**< The contents generated during the render pass are not needed and may be discarded. The contents will be undefined. */
    SDL_GPU_STOREOP_RESOLVE,           /**< The multisample contents generated during the render pass will be resolved to a non-multisample texture. The contents in the multisample texture may then be discarded and will be undefined. */
    SDL_GPU_STOREOP_RESOLVE_AND_STORE  /**< The multisample contents generated during the render pass will be resolved to a non-multisample texture. The contents in the multisample texture will be written to memory. */
} SDL_GPUStoreOp;

/**
 * Specifies the size of elements in an index buffer.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUIndexElementSize
{
    SDL_GPU_INDEXELEMENTSIZE_16BIT, /**< The index elements are 16-bit. */
    SDL_GPU_INDEXELEMENTSIZE_32BIT  /**< The index elements are 32-bit. */
} SDL_GPUIndexElementSize;

/**
 * Specifies the pixel format of a texture.
 *
 * Texture format support varies depending on driver, hardware, and usage
 * flags. In general, you should use SDL_GPUTextureSupportsFormat to query if
 * a format is supported before using it. However, there are a few guaranteed
 * formats.
 *
 * For SAMPLER usage, the following formats are universally supported:
 *
 * - R8G8B8A8_UNORM
 * - B8G8R8A8_UNORM
 * - R8_UNORM
 * - R8_SNORM
 * - R8G8_UNORM
 * - R8G8_SNORM
 * - R8G8B8A8_SNORM
 * - R16_FLOAT
 * - R16G16_FLOAT
 * - R16G16B16A16_FLOAT
 * - R32_FLOAT
 * - R32G32_FLOAT
 * - R32G32B32A32_FLOAT
 * - R11G11B10_UFLOAT
 * - R8G8B8A8_UNORM_SRGB
 * - B8G8R8A8_UNORM_SRGB
 * - D16_UNORM
 *
 * For COLOR_TARGET usage, the following formats are universally supported:
 *
 * - R8G8B8A8_UNORM
 * - B8G8R8A8_UNORM
 * - R8_UNORM
 * - R16_FLOAT
 * - R16G16_FLOAT
 * - R16G16B16A16_FLOAT
 * - R32_FLOAT
 * - R32G32_FLOAT
 * - R32G32B32A32_FLOAT
 * - R8_UINT
 * - R8G8_UINT
 * - R8G8B8A8_UINT
 * - R16_UINT
 * - R16G16_UINT
 * - R16G16B16A16_UINT
 * - R8_INT
 * - R8G8_INT
 * - R8G8B8A8_INT
 * - R16_INT
 * - R16G16_INT
 * - R16G16B16A16_INT
 * - R8G8B8A8_UNORM_SRGB
 * - B8G8R8A8_UNORM_SRGB
 *
 * For STORAGE usages, the following formats are universally supported:
 *
 * - R8G8B8A8_UNORM
 * - R8G8B8A8_SNORM
 * - R16G16B16A16_FLOAT
 * - R32_FLOAT
 * - R32G32_FLOAT
 * - R32G32B32A32_FLOAT
 * - R8G8B8A8_UINT
 * - R16G16B16A16_UINT
 * - R8G8B8A8_INT
 * - R16G16B16A16_INT
 *
 * For DEPTH_STENCIL_TARGET usage, the following formats are universally
 * supported:
 *
 * - D16_UNORM
 * - Either (but not necessarily both!) D24_UNORM or D32_SFLOAT
 * - Either (but not necessarily both!) D24_UNORM_S8_UINT or
 *   D32_SFLOAT_S8_UINT
 *
 * Unless D16_UNORM is sufficient for your purposes, always check which of
 * D24/D32 is supported before creating a depth-stencil texture!
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 * \sa SDL_GPUTextureSupportsFormat
 */
typedef enum SDL_GPUTextureFormat
{
    SDL_GPU_TEXTUREFORMAT_INVALID,

    /* Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM,
    SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
    /* Compressed Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,
    /* Compressed Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT,
    /* Compressed Unsigned Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT,
    /* Signed Normalized Float Color Formats  */
    SDL_GPU_TEXTUREFORMAT_R8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM,
    /* Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
    /* Unsigned Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
    /* Unsigned Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
    SDL_GPU_TEXTUREFORMAT_R16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
    /* Signed Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_INT,
    SDL_GPU_TEXTUREFORMAT_R8G8_INT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT,
    SDL_GPU_TEXTUREFORMAT_R16_INT,
    SDL_GPU_TEXTUREFORMAT_R16G16_INT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT,
    /* SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
    /* Compressed SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,
    /* Depth Formats */
    SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT
} SDL_GPUTextureFormat;

/**
 * Specifies how a texture is intended to be used by the client.
 *
 * A texture must have at least one usage flag. Note that some usage flag
 * combinations are invalid.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 */
typedef Uint32 SDL_GPUTextureUsageFlags;

#define SDL_GPU_TEXTUREUSAGE_SAMPLER               (1u << 0) /**< Texture supports sampling. */
#define SDL_GPU_TEXTUREUSAGE_COLOR_TARGET          (1u << 1) /**< Texture is a color render target. */
#define SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET  (1u << 2) /**< Texture is a depth stencil target. */
#define SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ (1u << 3) /**< Texture supports storage reads in graphics stages. */
#define SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ  (1u << 4) /**< Texture supports storage reads in the compute stage. */
#define SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE (1u << 5) /**< Texture supports storage writes in the compute stage. */

/**
 * Specifies the type of a texture.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 */
typedef enum SDL_GPUTextureType
{
    SDL_GPU_TEXTURETYPE_2D,         /**< The texture is a 2-dimensional image. */
    SDL_GPU_TEXTURETYPE_2D_ARRAY,   /**< The texture is a 2-dimensional array image. */
    SDL_GPU_TEXTURETYPE_3D,         /**< The texture is a 3-dimensional image. */
    SDL_GPU_TEXTURETYPE_CUBE,       /**< The texture is a cube image. */
    SDL_GPU_TEXTURETYPE_CUBE_ARRAY  /**< The texture is a cube array image. */
} SDL_GPUTextureType;

/**
 * Specifies the sample count of a texture.
 *
 * Used in multisampling. Note that this value only applies when the texture
 * is used as a render target.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 * \sa SDL_GPUTextureSupportsSampleCount
 */
typedef enum SDL_GPUSampleCount
{
    SDL_GPU_SAMPLECOUNT_1,  /**< No multisampling. */
    SDL_GPU_SAMPLECOUNT_2,  /**< MSAA 2x */
    SDL_GPU_SAMPLECOUNT_4,  /**< MSAA 4x */
    SDL_GPU_SAMPLECOUNT_8   /**< MSAA 8x */
} SDL_GPUSampleCount;


/**
 * Specifies the face of a cube map.
 *
 * Can be passed in as the layer field in texture-related structs.
 *
 * \since This enum is available since SDL 3.0.0
 */
typedef enum SDL_GPUCubeMapFace
{
    SDL_GPU_CUBEMAPFACE_POSITIVEX,
    SDL_GPU_CUBEMAPFACE_NEGATIVEX,
    SDL_GPU_CUBEMAPFACE_POSITIVEY,
    SDL_GPU_CUBEMAPFACE_NEGATIVEY,
    SDL_GPU_CUBEMAPFACE_POSITIVEZ,
    SDL_GPU_CUBEMAPFACE_NEGATIVEZ
} SDL_GPUCubeMapFace;

/**
 * Specifies how a buffer is intended to be used by the client.
 *
 * A buffer must have at least one usage flag. Note that some usage flag
 * combinations are invalid.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUBuffer
 */
typedef Uint32 SDL_GPUBufferUsageFlags;

#define SDL_GPU_BUFFERUSAGE_VERTEX                (1u << 0) /**< Buffer is a vertex buffer. */
#define SDL_GPU_BUFFERUSAGE_INDEX                 (1u << 1) /**< Buffer is an index buffer. */
#define SDL_GPU_BUFFERUSAGE_INDIRECT              (1u << 2) /**< Buffer is an indirect buffer. */
#define SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ (1u << 3) /**< Buffer supports storage reads in graphics stages. */
#define SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ  (1u << 4) /**< Buffer supports storage reads in the compute stage. */
#define SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE (1u << 5) /**< Buffer supports storage writes in the compute stage. */

/**
 * Specifies how a transfer buffer is intended to be used by the client.
 *
 * Note that mapping and copying FROM an upload transfer buffer or TO a
 * download transfer buffer is undefined behavior.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTransferBuffer
 */
typedef enum SDL_GPUTransferBufferUsage
{
    SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD
} SDL_GPUTransferBufferUsage;

/**
 * Specifies which stage a shader program corresponds to.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUShader
 */
typedef enum SDL_GPUShaderStage
{
    SDL_GPU_SHADERSTAGE_VERTEX,
    SDL_GPU_SHADERSTAGE_FRAGMENT
} SDL_GPUShaderStage;

/**
 * Specifies the format of shader code.
 *
 * Each format corresponds to a specific backend that accepts it.
 *
 * \since This datatype is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUShader
 */
typedef Uint32 SDL_GPUShaderFormat;

#define SDL_GPU_SHADERFORMAT_INVALID  0
#define SDL_GPU_SHADERFORMAT_PRIVATE  (1u << 0) /**< Shaders for NDA'd platforms. */
#define SDL_GPU_SHADERFORMAT_SPIRV    (1u << 1) /**< SPIR-V shaders for Vulkan. */
#define SDL_GPU_SHADERFORMAT_DXBC     (1u << 2) /**< DXBC SM5_0 shaders for D3D11. */
#define SDL_GPU_SHADERFORMAT_DXIL     (1u << 3) /**< DXIL shaders for D3D12. */
#define SDL_GPU_SHADERFORMAT_MSL      (1u << 4) /**< MSL shaders for Metal. */
#define SDL_GPU_SHADERFORMAT_METALLIB (1u << 5) /**< Precompiled metallib shaders for Metal. */

/**
 * Specifies the format of a vertex attribute.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUVertexElementFormat
{
    SDL_GPU_VERTEXELEMENTFORMAT_INVALID,

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

/**
 * Specifies the rate at which vertex attributes are pulled from buffers.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUVertexInputRate
{
    SDL_GPU_VERTEXINPUTRATE_VERTEX,   /**< Attribute addressing is a function of the vertex index. */
    SDL_GPU_VERTEXINPUTRATE_INSTANCE  /**< Attribute addressing is a function of the instance index. */
} SDL_GPUVertexInputRate;

/**
 * Specifies the fill mode of the graphics pipeline.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUFillMode
{
    SDL_GPU_FILLMODE_FILL,  /**< Polygons will be rendered via rasterization. */
    SDL_GPU_FILLMODE_LINE   /**< Polygon edges will be drawn as line segments. */
} SDL_GPUFillMode;

/**
 * Specifies the facing direction in which triangle faces will be culled.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUCullMode
{
    SDL_GPU_CULLMODE_NONE,   /**< No triangles are culled. */
    SDL_GPU_CULLMODE_FRONT,  /**< Front-facing triangles are culled. */
    SDL_GPU_CULLMODE_BACK    /**< Back-facing triangles are culled. */
} SDL_GPUCullMode;

/**
 * Specifies the vertex winding that will cause a triangle to be determined to
 * be front-facing.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUFrontFace
{
    SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,  /**< A triangle with counter-clockwise vertex winding will be considered front-facing. */
    SDL_GPU_FRONTFACE_CLOCKWISE           /**< A triangle with clockwise vertex winding will be considered front-facing. */
} SDL_GPUFrontFace;

/**
 * Specifies a comparison operator for depth, stencil and sampler operations.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUCompareOp
{
    SDL_GPU_COMPAREOP_INVALID,
    SDL_GPU_COMPAREOP_NEVER,             /**< The comparison always evaluates false. */
    SDL_GPU_COMPAREOP_LESS,              /**< The comparison evaluates reference < test. */
    SDL_GPU_COMPAREOP_EQUAL,             /**< The comparison evaluates reference == test. */
    SDL_GPU_COMPAREOP_LESS_OR_EQUAL,     /**< The comparison evaluates reference <= test. */
    SDL_GPU_COMPAREOP_GREATER,           /**< The comparison evaluates reference > test. */
    SDL_GPU_COMPAREOP_NOT_EQUAL,         /**< The comparison evaluates reference != test. */
    SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,  /**< The comparison evalutes reference >= test. */
    SDL_GPU_COMPAREOP_ALWAYS             /**< The comparison always evaluates true. */
} SDL_GPUCompareOp;

/**
 * Specifies what happens to a stored stencil value if stencil tests fail or
 * pass.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUStencilOp
{
    SDL_GPU_STENCILOP_INVALID,
    SDL_GPU_STENCILOP_KEEP,                 /**< Keeps the current value. */
    SDL_GPU_STENCILOP_ZERO,                 /**< Sets the value to 0. */
    SDL_GPU_STENCILOP_REPLACE,              /**< Sets the value to reference. */
    SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP,  /**< Increments the current value and clamps to the maximum value. */
    SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP,  /**< Decrements the current value and clamps to 0. */
    SDL_GPU_STENCILOP_INVERT,               /**< Bitwise-inverts the current value. */
    SDL_GPU_STENCILOP_INCREMENT_AND_WRAP,   /**< Increments the current value and wraps back to 0. */
    SDL_GPU_STENCILOP_DECREMENT_AND_WRAP    /**< Decrements the current value and wraps to the maximum value. */
} SDL_GPUStencilOp;

/**
 * Specifies the operator to be used when pixels in a render target are
 * blended with existing pixels in the texture.
 *
 * The source color is the value written by the fragment shader. The
 * destination color is the value currently existing in the texture.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUBlendOp
{
    SDL_GPU_BLENDOP_INVALID,
    SDL_GPU_BLENDOP_ADD,               /**< (source * source_factor) + (destination * destination_factor) */
    SDL_GPU_BLENDOP_SUBTRACT,          /**< (source * source_factor) - (destination * destination_factor) */
    SDL_GPU_BLENDOP_REVERSE_SUBTRACT,  /**< (destination * destination_factor) - (source * source_factor) */
    SDL_GPU_BLENDOP_MIN,               /**< min(source, destination) */
    SDL_GPU_BLENDOP_MAX                /**< max(source, destination) */
} SDL_GPUBlendOp;

/**
 * Specifies a blending factor to be used when pixels in a render target are
 * blended with existing pixels in the texture.
 *
 * The source color is the value written by the fragment shader. The
 * destination color is the value currently existing in the texture.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef enum SDL_GPUBlendFactor
{
    SDL_GPU_BLENDFACTOR_INVALID,
    SDL_GPU_BLENDFACTOR_ZERO,                      /**< 0 */
    SDL_GPU_BLENDFACTOR_ONE,                       /**< 1 */
    SDL_GPU_BLENDFACTOR_SRC_COLOR,                 /**< source color */
    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,       /**< 1 - source color */
    SDL_GPU_BLENDFACTOR_DST_COLOR,                 /**< destination color */
    SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,       /**< 1 - destination color */
    SDL_GPU_BLENDFACTOR_SRC_ALPHA,                 /**< source alpha */
    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,       /**< 1 - source alpha */
    SDL_GPU_BLENDFACTOR_DST_ALPHA,                 /**< destination alpha */
    SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,       /**< 1 - destination alpha */
    SDL_GPU_BLENDFACTOR_CONSTANT_COLOR,            /**< blend constant */
    SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR,  /**< 1 - blend constant */
    SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE         /**< min(source alpha, 1 - destination alpha) */
} SDL_GPUBlendFactor;

/**
 * Specifies which color components are written in a graphics pipeline.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef Uint8 SDL_GPUColorComponentFlags;

#define SDL_GPU_COLORCOMPONENT_R (1u << 0) /**< the red component */
#define SDL_GPU_COLORCOMPONENT_G (1u << 1) /**< the green component */
#define SDL_GPU_COLORCOMPONENT_B (1u << 2) /**< the blue component */
#define SDL_GPU_COLORCOMPONENT_A (1u << 3) /**< the alpha component */

/**
 * Specifies a filter operation used by a sampler.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUSampler
 */
typedef enum SDL_GPUFilter
{
    SDL_GPU_FILTER_NEAREST,  /**< Point filtering. */
    SDL_GPU_FILTER_LINEAR    /**< Linear filtering. */
} SDL_GPUFilter;

/**
 * Specifies a mipmap mode used by a sampler.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUSampler
 */
typedef enum SDL_GPUSamplerMipmapMode
{
    SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,  /**< Point filtering. */
    SDL_GPU_SAMPLERMIPMAPMODE_LINEAR    /**< Linear filtering. */
} SDL_GPUSamplerMipmapMode;

/**
 * Specifies behavior of texture sampling when the coordinates exceed the 0-1
 * range.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUSampler
 */
typedef enum SDL_GPUSamplerAddressMode
{
    SDL_GPU_SAMPLERADDRESSMODE_REPEAT,           /**< Specifies that the coordinates will wrap around. */
    SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,  /**< Specifies that the coordinates will wrap around mirrored. */
    SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE     /**< Specifies that the coordinates will clamp to the 0-1 range. */
} SDL_GPUSamplerAddressMode;

/**
 * Specifies the timing that will be used to present swapchain textures to the
 * OS.
 *
 * Note that this value affects the behavior of
 * SDL_AcquireGPUSwapchainTexture. VSYNC mode will always be supported.
 * IMMEDIATE and MAILBOX modes may not be supported on certain systems.
 *
 * It is recommended to query SDL_WindowSupportsGPUPresentMode after claiming
 * the window if you wish to change the present mode to IMMEDIATE or MAILBOX.
 *
 * - VSYNC: Waits for vblank before presenting. No tearing is possible. If
 *   there is a pending image to present, the new image is enqueued for
 *   presentation. Disallows tearing at the cost of visual latency. When using
 *   this present mode, AcquireSwapchainTexture will block if too many frames
 *   are in flight.
 * - IMMEDIATE: Immediately presents. Lowest latency option, but tearing may
 *   occur. When using this mode, AcquireSwapchainTexture will return NULL if
 *   too many frames are in flight.
 * - MAILBOX: Waits for vblank before presenting. No tearing is possible. If
 *   there is a pending image to present, the pending image is replaced by the
 *   new image. Similar to VSYNC, but with reduced visual latency. When using
 *   this mode, AcquireSwapchainTexture will return NULL if too many frames
 *   are in flight.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_SetGPUSwapchainParameters
 * \sa SDL_WindowSupportsGPUPresentMode
 * \sa SDL_AcquireGPUSwapchainTexture
 */
typedef enum SDL_GPUPresentMode
{
    SDL_GPU_PRESENTMODE_VSYNC,
    SDL_GPU_PRESENTMODE_IMMEDIATE,
    SDL_GPU_PRESENTMODE_MAILBOX
} SDL_GPUPresentMode;

/**
 * Specifies the texture format and colorspace of the swapchain textures.
 *
 * SDR will always be supported. Other compositions may not be supported on
 * certain systems.
 *
 * It is recommended to query SDL_WindowSupportsGPUSwapchainComposition after
 * claiming the window if you wish to change the swapchain composition from
 * SDR.
 *
 * - SDR: B8G8R8A8 or R8G8B8A8 swapchain. Pixel values are in nonlinear sRGB
 *   encoding.
 * - SDR_LINEAR: B8G8R8A8_SRGB or R8G8B8A8_SRGB swapchain. Pixel values are in
 *   nonlinear sRGB encoding.
 * - HDR_EXTENDED_LINEAR: R16G16B16A16_SFLOAT swapchain. Pixel values are in
 *   extended linear encoding.
 * - HDR10_ST2048: A2R10G10B10 or A2B10G10R10 swapchain. Pixel values are in
 *   PQ ST2048 encoding.
 *
 * \since This enum is available since SDL 3.0.0
 *
 * \sa SDL_SetGPUSwapchainParameters
 * \sa SDL_WindowSupportsGPUSwapchainComposition
 * \sa SDL_AcquireGPUSwapchainTexture
 */
typedef enum SDL_GPUSwapchainComposition
{
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
    SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
    SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048
} SDL_GPUSwapchainComposition;

/* Structures */

/**
 * A structure specifying a viewport.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_SetGPUViewport
 */
typedef struct SDL_GPUViewport
{
    float x;          /**< The left offset of the viewport. */
    float y;          /**< The top offset of the viewport. */
    float w;          /**< The width of the viewport. */
    float h;          /**< The height of the viewport. */
    float min_depth;  /**< The minimum depth of the viewport. */
    float max_depth;  /**< The maximum depth of the viewport. */
} SDL_GPUViewport;

/**
 * A structure specifying parameters related to transferring data to or from a
 * texture.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUTexture
 */
typedef struct SDL_GPUTextureTransferInfo
{
    SDL_GPUTransferBuffer *transfer_buffer;  /**< The transfer buffer used in the transfer operation. */
    Uint32 offset;                           /**< The starting byte of the image data in the transfer buffer. */
    Uint32 pixels_per_row;                   /**< The number of pixels from one row to the next. */
    Uint32 rows_per_layer;                   /**< The number of rows from one layer/depth-slice to the next. */
} SDL_GPUTextureTransferInfo;

/**
 * A structure specifying a location in a transfer buffer.
 *
 * Used when transferring buffer data to or from a transfer buffer.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_DownloadFromGPUBuffer
 */
typedef struct SDL_GPUTransferBufferLocation
{
    SDL_GPUTransferBuffer *transfer_buffer;  /**< The transfer buffer used in the transfer operation. */
    Uint32 offset;                           /**< The starting byte of the buffer data in the transfer buffer. */
} SDL_GPUTransferBufferLocation;

/**
 * A structure specifying a location in a texture.
 *
 * Used when copying data from one texture to another.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CopyGPUTextureToTexture
 */
typedef struct SDL_GPUTextureLocation
{
    SDL_GPUTexture *texture;  /**< The texture used in the copy operation. */
    Uint32 mip_level;         /**< The mip level index of the location. */
    Uint32 layer;             /**< The layer index of the location. */
    Uint32 x;                 /**< The left offset of the location. */
    Uint32 y;                 /**< The top offset of the location. */
    Uint32 z;                 /**< The front offset of the location. */
} SDL_GPUTextureLocation;

/**
 * A structure specifying a region of a texture.
 *
 * Used when transferring data to or from a texture.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_UploadToGPUTexture
 * \sa SDL_DownloadFromGPUTexture
 */
typedef struct SDL_GPUTextureRegion
{
    SDL_GPUTexture *texture;  /**< The texture used in the copy operation. */
    Uint32 mip_level;         /**< The mip level index to transfer. */
    Uint32 layer;             /**< The layer index to transfer. */
    Uint32 x;                 /**< The left offset of the region. */
    Uint32 y;                 /**< The top offset of the region. */
    Uint32 z;                 /**< The front offset of the region. */
    Uint32 w;                 /**< The width of the region. */
    Uint32 h;                 /**< The height of the region. */
    Uint32 d;                 /**< The depth of the region. */
} SDL_GPUTextureRegion;

/**
 * A structure specifying a region of a texture used in the blit operation.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BlitGPUTexture
 */
typedef struct SDL_GPUBlitRegion
{
    SDL_GPUTexture *texture;  /**< The texture. */
    Uint32 mip_level;             /**< The mip level index of the region. */
    Uint32 layer_or_depth_plane;  /**< The layer index or depth plane of the region. This value is treated as a layer index on 2D array and cube textures, and as a depth plane on 3D textures. */
    Uint32 x;                     /**< The left offset of the region. */
    Uint32 y;                     /**< The top offset of the region.  */
    Uint32 w;                     /**< The width of the region. */
    Uint32 h;                     /**< The height of the region. */
} SDL_GPUBlitRegion;

/**
 * A structure specifying a location in a buffer.
 *
 * Used when copying data between buffers.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CopyGPUBufferToBuffer
 */
typedef struct SDL_GPUBufferLocation
{
    SDL_GPUBuffer *buffer;  /**< The buffer. */
    Uint32 offset;          /**< The starting byte within the buffer. */
} SDL_GPUBufferLocation;

/**
 * A structure specifying a region of a buffer.
 *
 * Used when transferring data to or from buffers.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_DownloadFromGPUBuffer
 */
typedef struct SDL_GPUBufferRegion
{
    SDL_GPUBuffer *buffer;  /**< The buffer. */
    Uint32 offset;          /**< The starting byte within the buffer. */
    Uint32 size;            /**< The size in bytes of the region. */
} SDL_GPUBufferRegion;

/**
 * A structure specifying the parameters of an indirect draw command.
 *
 * Note that the `first_vertex` and `first_instance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_DrawGPUPrimitivesIndirect
 */
typedef struct SDL_GPUIndirectDrawCommand
{
    Uint32 num_vertices;   /**< The number of vertices to draw. */
    Uint32 num_instances;  /**< The number of instances to draw. */
    Uint32 first_vertex;   /**< The index of the first vertex to draw. */
    Uint32 first_instance; /**< The ID of the first instance to draw. */
} SDL_GPUIndirectDrawCommand;

/**
 * A structure specifying the parameters of an indexed indirect draw command.
 *
 * Note that the `first_vertex` and `first_instance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_DrawGPUIndexedPrimitivesIndirect
 */
typedef struct SDL_GPUIndexedIndirectDrawCommand
{
    Uint32 num_indices;    /**< The number of indices to draw per instance. */
    Uint32 num_instances;  /**< The number of instances to draw. */
    Uint32 first_index;    /**< The base index within the index buffer. */
    Sint32 vertex_offset;  /**< The value added to the vertex index before indexing into the vertex buffer. */
    Uint32 first_instance; /**< The ID of the first instance to draw. */
} SDL_GPUIndexedIndirectDrawCommand;

/**
 * A structure specifying the parameters of an indexed dispatch command.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_DispatchGPUComputeIndirect
 */
typedef struct SDL_GPUIndirectDispatchCommand
{
    Uint32 groupcount_x;  /**< The number of local workgroups to dispatch in the X dimension. */
    Uint32 groupcount_y;  /**< The number of local workgroups to dispatch in the Y dimension. */
    Uint32 groupcount_z;  /**< The number of local workgroups to dispatch in the Z dimension. */
} SDL_GPUIndirectDispatchCommand;

/* State structures */

/**
 * A structure specifying the parameters of a sampler.
 *
 * \since This function is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUSampler
 */
typedef struct SDL_GPUSamplerCreateInfo
{
    SDL_GPUFilter min_filter;                  /**< The minification filter to apply to lookups. */
    SDL_GPUFilter mag_filter;                  /**< The magnification filter to apply to lookups. */
    SDL_GPUSamplerMipmapMode mipmap_mode;      /**< The mipmap filter to apply to lookups. */
    SDL_GPUSamplerAddressMode address_mode_u;  /**< The addressing mode for U coordinates outside [0, 1). */
    SDL_GPUSamplerAddressMode address_mode_v;  /**< The addressing mode for V coordinates outside [0, 1). */
    SDL_GPUSamplerAddressMode address_mode_w;  /**< The addressing mode for W coordinates outside [0, 1). */
    float mip_lod_bias;                        /**< The bias to be added to mipmap LOD calculation. */
    float max_anisotropy;                      /**< The anisotropy value clamp used by the sampler. If enable_anisotropy is false, this is ignored. */
    SDL_GPUCompareOp compare_op;               /**< The comparison operator to apply to fetched data before filtering. */
    float min_lod;                             /**< Clamps the minimum of the computed LOD value. */
    float max_lod;                             /**< Clamps the maximum of the computed LOD value. */
    bool enable_anisotropy;                /**< true to enable anisotropic filtering. */
    bool enable_compare;                   /**< true to enable comparison against a reference value during lookups. */
    Uint8 padding1;
    Uint8 padding2;

    SDL_PropertiesID props;                    /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUSamplerCreateInfo;

/**
 * A structure specifying the parameters of vertex buffers used in a graphics
 * pipeline.
 *
 * When you call SDL_BindGPUVertexBuffers, you specify the binding slots of
 * the vertex buffers. For example if you called SDL_BindGPUVertexBuffers with
 * a first_slot of 2 and num_bindings of 3, the binding slots 2, 3, 4 would be
 * used by the vertex buffers you pass in.
 *
 * Vertex attributes are linked to buffers via the buffer_slot field of
 * SDL_GPUVertexAttribute. For example, if an attribute has a buffer_slot of
 * 0, then that attribute belongs to the vertex buffer bound at slot 0.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUVertexAttribute
 * \sa SDL_GPUVertexInputState
 */
typedef struct SDL_GPUVertexBufferDescription
{
    Uint32 slot;                        /**< The binding slot of the vertex buffer. */
    Uint32 pitch;                       /**< The byte pitch between consecutive elements of the vertex buffer. */
    SDL_GPUVertexInputRate input_rate;  /**< Whether attribute addressing is a function of the vertex index or instance index. */
    Uint32 instance_step_rate;          /**< The number of instances to draw using the same per-instance data before advancing in the instance buffer by one element. Ignored unless input_rate is SDL_GPU_VERTEXINPUTRATE_INSTANCE */
} SDL_GPUVertexBufferDescription;

/**
 * A structure specifying a vertex attribute.
 *
 * All vertex attribute locations provided to an SDL_GPUVertexInputState must
 * be unique.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUVertexBufferDescription
 * \sa SDL_GPUVertexInputState
 */
typedef struct SDL_GPUVertexAttribute
{
    Uint32 location;                    /**< The shader input location index. */
    Uint32 buffer_slot;                 /**< The binding slot of the associated vertex buffer. */
    SDL_GPUVertexElementFormat format;  /**< The size and type of the attribute data. */
    Uint32 offset;                      /**< The byte offset of this attribute relative to the start of the vertex element. */
} SDL_GPUVertexAttribute;

/**
 * A structure specifying the parameters of a graphics pipeline vertex input
 * state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineCreateInfo
 */
typedef struct SDL_GPUVertexInputState
{
    const SDL_GPUVertexBufferDescription *vertex_buffer_descriptions; /**< A pointer to an array of vertex buffer descriptions. */
    Uint32 num_vertex_buffers;                                        /**< The number of vertex buffer descriptions in the above array. */
    const SDL_GPUVertexAttribute *vertex_attributes;                  /**< A pointer to an array of vertex attribute descriptions. */
    Uint32 num_vertex_attributes;                                     /**< The number of vertex attribute descriptions in the above array. */
} SDL_GPUVertexInputState;

/**
 * A structure specifying the stencil operation state of a graphics pipeline.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUDepthStencilState
 */
typedef struct SDL_GPUStencilOpState
{
    SDL_GPUStencilOp fail_op;        /**< The action performed on samples that fail the stencil test. */
    SDL_GPUStencilOp pass_op;        /**< The action performed on samples that pass the depth and stencil tests. */
    SDL_GPUStencilOp depth_fail_op;  /**< The action performed on samples that pass the stencil test and fail the depth test. */
    SDL_GPUCompareOp compare_op;     /**< The comparison operator used in the stencil test. */
} SDL_GPUStencilOpState;

/**
 * A structure specifying the blend state of a color target.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUColorTargetDescription
 */
typedef struct SDL_GPUColorTargetBlendState
{
    SDL_GPUBlendFactor src_color_blendfactor;     /**< The value to be multiplied by the source RGB value. */
    SDL_GPUBlendFactor dst_color_blendfactor;     /**< The value to be multiplied by the destination RGB value. */
    SDL_GPUBlendOp color_blend_op;                /**< The blend operation for the RGB components. */
    SDL_GPUBlendFactor src_alpha_blendfactor;     /**< The value to be multiplied by the source alpha. */
    SDL_GPUBlendFactor dst_alpha_blendfactor;     /**< The value to be multiplied by the destination alpha. */
    SDL_GPUBlendOp alpha_blend_op;                /**< The blend operation for the alpha component. */
    SDL_GPUColorComponentFlags color_write_mask;  /**< A bitmask specifying which of the RGBA components are enabled for writing. Writes to all channels if enable_color_write_mask is false. */
    bool enable_blend;                        /**< Whether blending is enabled for the color target. */
    bool enable_color_write_mask;             /**< Whether the color write mask is enabled. */
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUColorTargetBlendState;


/**
 * A structure specifying code and metadata for creating a shader object.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUShader
 */
typedef struct SDL_GPUShaderCreateInfo
{
    size_t code_size;             /**< The size in bytes of the code pointed to. */
    const Uint8 *code;            /**< A pointer to shader code. */
    const char *entrypoint;       /**< A pointer to a null-terminated UTF-8 string specifying the entry point function name for the shader. */
    SDL_GPUShaderFormat format;   /**< The format of the shader code. */
    SDL_GPUShaderStage stage;     /**< The stage the shader program corresponds to. */
    Uint32 num_samplers;          /**< The number of samplers defined in the shader. */
    Uint32 num_storage_textures;  /**< The number of storage textures defined in the shader. */
    Uint32 num_storage_buffers;   /**< The number of storage buffers defined in the shader. */
    Uint32 num_uniform_buffers;   /**< The number of uniform buffers defined in the shader. */

    SDL_PropertiesID props;       /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUShaderCreateInfo;

/**
 * A structure specifying the parameters of a texture.
 *
 * Usage flags can be bitwise OR'd together for combinations of usages. Note
 * that certain usage combinations are invalid, for example SAMPLER and
 * GRAPHICS_STORAGE.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTexture
 */
typedef struct SDL_GPUTextureCreateInfo
{
    SDL_GPUTextureType type;          /**< The base dimensionality of the texture. */
    SDL_GPUTextureFormat format;      /**< The pixel format of the texture. */
    SDL_GPUTextureUsageFlags usage;   /**< How the texture is intended to be used by the client. */
    Uint32 width;                     /**< The width of the texture. */
    Uint32 height;                    /**< The height of the texture. */
    Uint32 layer_count_or_depth;      /**< The layer count or depth of the texture. This value is treated as a layer count on 2D array textures, and as a depth value on 3D textures. */
    Uint32 num_levels;                /**< The number of mip levels in the texture. */
    SDL_GPUSampleCount sample_count;  /**< The number of samples per texel. Only applies if the texture is used as a render target. */

    SDL_PropertiesID props;           /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUTextureCreateInfo;

#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_R_FLOAT       "SDL.gpu.createtexture.d3d12.clear.r"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_G_FLOAT       "SDL.gpu.createtexture.d3d12.clear.g"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_B_FLOAT       "SDL.gpu.createtexture.d3d12.clear.b"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_A_FLOAT       "SDL.gpu.createtexture.d3d12.clear.a"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_DEPTH_FLOAT   "SDL.gpu.createtexture.d3d12.clear.depth"
#define SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_STENCIL_UINT8 "SDL.gpu.createtexture.d3d12.clear.stencil"

/**
 * A structure specifying the parameters of a buffer.
 *
 * Usage flags can be bitwise OR'd together for combinations of usages. Note
 * that certain combinations are invalid, for example VERTEX and INDEX.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUBuffer
 */
typedef struct SDL_GPUBufferCreateInfo
{
    SDL_GPUBufferUsageFlags usage;  /**< How the buffer is intended to be used by the client. */
    Uint32 size;                    /**< The size in bytes of the buffer. */

    SDL_PropertiesID props;         /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUBufferCreateInfo;

/**
 * A structure specifying the parameters of a transfer buffer.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUTransferBuffer
 */
typedef struct SDL_GPUTransferBufferCreateInfo
{
    SDL_GPUTransferBufferUsage usage;  /**< How the transfer buffer is intended to be used by the client. */
    Uint32 size;                       /**< The size in bytes of the transfer buffer. */

    SDL_PropertiesID props;            /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUTransferBufferCreateInfo;

/* Pipeline state structures */

/**
 * A structure specifying the parameters of the graphics pipeline rasterizer
 * state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineCreateInfo
 */
typedef struct SDL_GPURasterizerState
{
    SDL_GPUFillMode fill_mode;         /**< Whether polygons will be filled in or drawn as lines. */
    SDL_GPUCullMode cull_mode;         /**< The facing direction in which triangles will be culled. */
    SDL_GPUFrontFace front_face;       /**< The vertex winding that will cause a triangle to be determined as front-facing. */
    float depth_bias_constant_factor;  /**< A scalar factor controlling the depth value added to each fragment. */
    float depth_bias_clamp;            /**< The maximum depth bias of a fragment. */
    float depth_bias_slope_factor;     /**< A scalar factor applied to a fragment's slope in depth calculations. */
    bool enable_depth_bias;        /**< true to bias fragment depth values. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPURasterizerState;

/**
 * A structure specifying the parameters of the graphics pipeline multisample
 * state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineCreateInfo
 */
typedef struct SDL_GPUMultisampleState
{
    SDL_GPUSampleCount sample_count;  /**< The number of samples to be used in rasterization. */
    Uint32 sample_mask;               /**< Determines which samples get updated in the render targets. Treated as 0xFFFFFFFF if enable_mask is false. */
    bool enable_mask;             /**< Enables sample masking. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUMultisampleState;

/**
 * A structure specifying the parameters of the graphics pipeline depth
 * stencil state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineCreateInfo
 */
typedef struct SDL_GPUDepthStencilState
{
    SDL_GPUCompareOp compare_op;                /**< The comparison operator used for depth testing. */
    SDL_GPUStencilOpState back_stencil_state;   /**< The stencil op state for back-facing triangles. */
    SDL_GPUStencilOpState front_stencil_state;  /**< The stencil op state for front-facing triangles. */
    Uint8 compare_mask;                         /**< Selects the bits of the stencil values participating in the stencil test. */
    Uint8 write_mask;                           /**< Selects the bits of the stencil values updated by the stencil test. */
    bool enable_depth_test;                 /**< true enables the depth test. */
    bool enable_depth_write;                /**< true enables depth writes. Depth writes are always disabled when enable_depth_test is false. */
    bool enable_stencil_test;               /**< true enables the stencil test. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUDepthStencilState;

/**
 * A structure specifying the parameters of color targets used in a graphics
 * pipeline.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineTargetInfo
 */
typedef struct SDL_GPUColorTargetDescription
{
    SDL_GPUTextureFormat format;               /**< The pixel format of the texture to be used as a color target. */
    SDL_GPUColorTargetBlendState blend_state;  /**< The blend state to be used for the color target. */
} SDL_GPUColorTargetDescription;

/**
 * A structure specifying the descriptions of render targets used in a
 * graphics pipeline.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_GPUGraphicsPipelineCreateInfo
 */
typedef struct SDL_GPUGraphicsPipelineTargetInfo
{
    const SDL_GPUColorTargetDescription *color_target_descriptions;  /**< A pointer to an array of color target descriptions. */
    Uint32 num_color_targets;                                        /**< The number of color target descriptions in the above array. */
    SDL_GPUTextureFormat depth_stencil_format;                       /**< The pixel format of the depth-stencil target. Ignored if has_depth_stencil_target is false. */
    bool has_depth_stencil_target;                               /**< true specifies that the pipeline uses a depth-stencil target. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUGraphicsPipelineTargetInfo;

/**
 * A structure specifying the parameters of a graphics pipeline state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 */
typedef struct SDL_GPUGraphicsPipelineCreateInfo
{
    SDL_GPUShader *vertex_shader;                   /**< The vertex shader used by the graphics pipeline. */
    SDL_GPUShader *fragment_shader;                 /**< The fragment shader used by the graphics pipeline. */
    SDL_GPUVertexInputState vertex_input_state;     /**< The vertex layout of the graphics pipeline. */
    SDL_GPUPrimitiveType primitive_type;            /**< The primitive topology of the graphics pipeline. */
    SDL_GPURasterizerState rasterizer_state;        /**< The rasterizer state of the graphics pipeline. */
    SDL_GPUMultisampleState multisample_state;      /**< The multisample state of the graphics pipeline. */
    SDL_GPUDepthStencilState depth_stencil_state;   /**< The depth-stencil state of the graphics pipeline. */
    SDL_GPUGraphicsPipelineTargetInfo target_info;  /**< Formats and blend modes for the render targets of the graphics pipeline. */

    SDL_PropertiesID props;                         /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUGraphicsPipelineCreateInfo;

/**
 * A structure specifying the parameters of a compute pipeline state.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_CreateGPUComputePipeline
 */
typedef struct SDL_GPUComputePipelineCreateInfo
{
    size_t code_size;                       /**< The size in bytes of the compute shader code pointed to. */
    const Uint8 *code;                      /**< A pointer to compute shader code. */
    const char *entrypoint;                 /**< A pointer to a null-terminated UTF-8 string specifying the entry point function name for the shader. */
    SDL_GPUShaderFormat format;             /**< The format of the compute shader code. */
    Uint32 num_samplers;                    /**< The number of samplers defined in the shader. */
    Uint32 num_readonly_storage_textures;   /**< The number of readonly storage textures defined in the shader. */
    Uint32 num_readonly_storage_buffers;    /**< The number of readonly storage buffers defined in the shader. */
    Uint32 num_writeonly_storage_textures;  /**< The number of writeonly storage textures defined in the shader. */
    Uint32 num_writeonly_storage_buffers;   /**< The number of writeonly storage buffers defined in the shader. */
    Uint32 num_uniform_buffers;             /**< The number of uniform buffers defined in the shader. */
    Uint32 threadcount_x;                   /**< The number of threads in the X dimension. This should match the value in the shader. */
    Uint32 threadcount_y;                   /**< The number of threads in the Y dimension. This should match the value in the shader. */
    Uint32 threadcount_z;                   /**< The number of threads in the Z dimension. This should match the value in the shader. */

    SDL_PropertiesID props;                 /**< A properties ID for extensions. Should be 0 if no extensions are needed. */
} SDL_GPUComputePipelineCreateInfo;

/**
 * A structure specifying the parameters of a color target used by a render
 * pass.
 *
 * The load_op field determines what is done with the texture at the beginning
 * of the render pass.
 *
 * - LOAD: Loads the data currently in the texture. Not recommended for
 *   multisample textures as it requires significant memory bandwidth.
 * - CLEAR: Clears the texture to a single color.
 * - DONT_CARE: The driver will do whatever it wants with the texture memory.
 *   This is a good option if you know that every single pixel will be touched
 *   in the render pass.
 *
 * The store_op field determines what is done with the color results of the
 * render pass.
 *
 * - STORE: Stores the results of the render pass in the texture. Not
 *   recommended for multisample textures as it requires significant memory
 *   bandwidth.
 * - DONT_CARE: The driver will do whatever it wants with the texture memory.
 *   This is often a good option for depth/stencil textures.
 * - RESOLVE: Resolves a multisample texture into resolve_texture, which must
 *   have a sample count of 1. Then the driver may discard the multisample
 *   texture memory. This is the most performant method of resolving a
 *   multisample target.
 * - RESOLVE_AND_STORE: Resolves a multisample texture into the
 *   resolve_texture, which must have a sample count of 1. Then the driver
 *   stores the multisample texture's contents. Not recommended as it requires
 *   significant memory bandwidth.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPURenderPass
 */
typedef struct SDL_GPUColorTargetInfo
{
    SDL_GPUTexture *texture;         /**< The texture that will be used as a color target by a render pass. */
    Uint32 mip_level;                /**< The mip level to use as a color target. */
    Uint32 layer_or_depth_plane;     /**< The layer index or depth plane to use as a color target. This value is treated as a layer index on 2D array and cube textures, and as a depth plane on 3D textures. */
    SDL_FColor clear_color;          /**< The color to clear the color target to at the start of the render pass. Ignored if SDL_GPU_LOADOP_CLEAR is not used. */
    SDL_GPULoadOp load_op;           /**< What is done with the contents of the color target at the beginning of the render pass. */
    SDL_GPUStoreOp store_op;         /**< What is done with the results of the render pass. */
    SDL_GPUTexture *resolve_texture; /**< The texture that will receive the results of a multisample resolve operation. Ignored if a RESOLVE* store_op is not used. */
    Uint32 resolve_mip_level;        /**< The mip level of the resolve texture to use for the resolve operation. Ignored if a RESOLVE* store_op is not used. */
    Uint32 resolve_layer;            /**< The layer index of the resolve texture to use for the resolve operation. Ignored if a RESOLVE* store_op is not used. */
    bool cycle;                  /**< true cycles the texture if the texture is bound and load_op is not LOAD */
    bool cycle_resolve_texture;  /**< true cycles the resolve texture if the resolve texture is bound. Ignored if a RESOLVE* store_op is not used. */
    Uint8 padding1;
    Uint8 padding2;
} SDL_GPUColorTargetInfo;

/**
 * A structure specifying the parameters of a depth-stencil target used by a
 * render pass.
 *
 * The load_op field determines what is done with the depth contents of the
 * texture at the beginning of the render pass.
 *
 * - LOAD: Loads the depth values currently in the texture.
 * - CLEAR: Clears the texture to a single depth.
 * - DONT_CARE: The driver will do whatever it wants with the memory. This is
 *   a good option if you know that every single pixel will be touched in the
 *   render pass.
 *
 * The store_op field determines what is done with the depth results of the
 * render pass.
 *
 * - STORE: Stores the depth results in the texture.
 * - DONT_CARE: The driver will do whatever it wants with the depth results.
 *   This is often a good option for depth/stencil textures that don't need to
 *   be reused again.
 *
 * The stencil_load_op field determines what is done with the stencil contents
 * of the texture at the beginning of the render pass.
 *
 * - LOAD: Loads the stencil values currently in the texture.
 * - CLEAR: Clears the stencil values to a single value.
 * - DONT_CARE: The driver will do whatever it wants with the memory. This is
 *   a good option if you know that every single pixel will be touched in the
 *   render pass.
 *
 * The stencil_store_op field determines what is done with the stencil results
 * of the render pass.
 *
 * - STORE: Stores the stencil results in the texture.
 * - DONT_CARE: The driver will do whatever it wants with the stencil results.
 *   This is often a good option for depth/stencil textures that don't need to
 *   be reused again.
 *
 * Note that depth/stencil targets do not support multisample resolves.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPURenderPass
 */
typedef struct SDL_GPUDepthStencilTargetInfo
{
    SDL_GPUTexture *texture;               /**< The texture that will be used as the depth stencil target by the render pass. */
    float clear_depth;                     /**< The value to clear the depth component to at the beginning of the render pass. Ignored if SDL_GPU_LOADOP_CLEAR is not used. */
    SDL_GPULoadOp load_op;                 /**< What is done with the depth contents at the beginning of the render pass. */
    SDL_GPUStoreOp store_op;               /**< What is done with the depth results of the render pass. */
    SDL_GPULoadOp stencil_load_op;         /**< What is done with the stencil contents at the beginning of the render pass. */
    SDL_GPUStoreOp stencil_store_op;       /**< What is done with the stencil results of the render pass. */
    bool cycle;                        /**< true cycles the texture if the texture is bound and any load ops are not LOAD */
    Uint8 clear_stencil;                   /**< The value to clear the stencil component to at the beginning of the render pass. Ignored if SDL_GPU_LOADOP_CLEAR is not used. */
    Uint8 padding1;
    Uint8 padding2;
} SDL_GPUDepthStencilTargetInfo;

/**
 * A structure containing parameters for a blit command.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BlitGPUTexture
 */
typedef struct SDL_GPUBlitInfo {
    SDL_GPUBlitRegion source;       /**< The source region for the blit. */
    SDL_GPUBlitRegion destination;  /**< The destination region for the blit. */
    SDL_GPULoadOp load_op;          /**< What is done with the contents of the destination before the blit. */
    SDL_FColor clear_color;         /**< The color to clear the destination region to before the blit. Ignored if load_op is not SDL_GPU_LOADOP_CLEAR. */
    SDL_FlipMode flip_mode;         /**< The flip mode for the source region. */
    SDL_GPUFilter filter;           /**< The filter mode used when blitting. */
    bool cycle;                 /**< true cycles the destination texture if it is already bound. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUBlitInfo;
/* Binding structs */

/**
 * A structure specifying parameters in a buffer binding call.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BindGPUVertexBuffers
 * \sa SDL_BindGPUIndexBuffers
 */
typedef struct SDL_GPUBufferBinding
{
    SDL_GPUBuffer *buffer;  /**< The buffer to bind. Must have been created with SDL_GPU_BUFFERUSAGE_VERTEX for SDL_BindGPUVertexBuffers, or SDL_GPU_BUFFERUSAGE_INDEX for SDL_BindGPUIndexBuffers. */
    Uint32 offset;          /**< The starting byte of the data to bind in the buffer. */
} SDL_GPUBufferBinding;

/**
 * A structure specifying parameters in a sampler binding call.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BindGPUVertexSamplers
 * \sa SDL_BindGPUFragmentSamplers
 */
typedef struct SDL_GPUTextureSamplerBinding
{
    SDL_GPUTexture *texture;  /**< The texture to bind. Must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER. */
    SDL_GPUSampler *sampler;  /**< The sampler to bind. */
} SDL_GPUTextureSamplerBinding;

/**
 * A structure specifying parameters related to binding buffers in a compute
 * pass.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPUComputePass
 */
typedef struct SDL_GPUStorageBufferWriteOnlyBinding
{
    SDL_GPUBuffer *buffer;  /**< The buffer to bind. Must have been created with SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE. */
    bool cycle;         /**< true cycles the buffer if it is already bound. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUStorageBufferWriteOnlyBinding;

/**
 * A structure specifying parameters related to binding textures in a compute
 * pass.
 *
 * \since This struct is available since SDL 3.0.0
 *
 * \sa SDL_BeginGPUComputePass
 */
typedef struct SDL_GPUStorageTextureWriteOnlyBinding
{
    SDL_GPUTexture *texture;  /**< The texture to bind. Must have been created with SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE. */
    Uint32 mip_level;         /**< The mip level index to bind. */
    Uint32 layer;             /**< The layer index to bind. */
    bool cycle;           /**< true cycles the texture if it is already bound. */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_GPUStorageTextureWriteOnlyBinding;

/* Functions */

/* Device */

/**
 * Checks for GPU runtime support.
 *
 * \param format_flags a bitflag indicating which shader formats the app is
 *                     able to provide.
 * \param name the preferred GPU driver, or NULL to let SDL pick the optimal
 *             driver.
 * \returns true if supported, false otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUDevice
 */
extern SDL_DECLSPEC bool SDLCALL SDL_GPUSupportsShaderFormats(
    SDL_GPUShaderFormat format_flags,
    const char *name);

/**
 * Checks for GPU runtime support.
 *
 * \param props the properties to use.
 * \returns true if supported, false otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUDeviceWithProperties
 */
extern SDL_DECLSPEC bool SDLCALL SDL_GPUSupportsProperties(
    SDL_PropertiesID props);

/**
 * Creates a GPU context.
 *
 * \param format_flags a bitflag indicating which shader formats the app is
 *                     able to provide.
 * \param debug_mode enable debug mode properties and validations.
 * \param name the preferred GPU driver, or NULL to let SDL pick the optimal
 *             driver.
 * \returns a GPU context on success or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGPUShaderFormats
 * \sa SDL_GetGPUDeviceDriver
 * \sa SDL_DestroyGPUDevice
 * \sa SDL_GPUSupportsShaderFormats
 */
extern SDL_DECLSPEC SDL_GPUDevice *SDLCALL SDL_CreateGPUDevice(
    SDL_GPUShaderFormat format_flags,
    bool debug_mode,
    const char *name);

/**
 * Creates a GPU context.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOL`: enable debug mode properties
 *   and validations, defaults to true.
 * - `SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOL`: enable to prefer energy
 *   efficiency over maximum GPU performance, defaults to false.
 * - `SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING`: the name of the GPU driver to
 *   use, if a specific one is desired.
 *
 * These are the current shader format properties:
 *
 * - `SDL_PROP_GPU_DEVICE_CREATE_SHADERS_PRIVATE_BOOL`: The app is able to
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
 * \sa SDL_GetGPUShaderFormats
 * \sa SDL_GetGPUDeviceDriver
 * \sa SDL_DestroyGPUDevice
 * \sa SDL_GPUSupportsProperties
 */
extern SDL_DECLSPEC SDL_GPUDevice *SDLCALL SDL_CreateGPUDeviceWithProperties(
    SDL_PropertiesID props);

#define SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOL             "SDL.gpu.device.create.debugmode"
#define SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOL        "SDL.gpu.device.create.preferlowpower"
#define SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING                "SDL.gpu.device.create.name"
#define SDL_PROP_GPU_DEVICE_CREATE_SHADERS_PRIVATE_BOOL       "SDL.gpu.device.create.shaders.private"
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
 * Get the number of GPU drivers compiled into SDL.
 *
 * \returns the number of built in GPU drivers.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetGPUDriver
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetNumGPUDrivers(void);

/**
 * Get the name of a built in GPU driver.
 *
 * The GPU drivers are presented in the order in which they are normally
 * checked during initialization.
 *
 * The names of drivers are all simple, low-ASCII identifiers, like "vulkan",
 * "metal" or "direct3d12". These never have Unicode characters, and are not
 * meant to be proper names.
 *
 * \param index the index of a GPU driver.
 * \returns the name of the GPU driver with the given **index**.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumGPUDrivers
 */
extern SDL_DECLSPEC const char * SDLCALL SDL_GetGPUDriver(int index);

/**
 * Returns the name of the backend used to create this GPU context.
 *
 * \param device a GPU context to query.
 * \returns the name of the device's driver, or NULL on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC const char * SDLCALL SDL_GetGPUDeviceDriver(SDL_GPUDevice *device);

/**
 * Returns the supported shader formats for this GPU context.
 *
 * \param device a GPU context to query.
 * \returns a bitflag indicating which shader formats the driver is able to
 *          consume.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_GPUShaderFormat SDLCALL SDL_GetGPUShaderFormats(SDL_GPUDevice *device);

/* State Creation */

/**
 * Creates a pipeline object to be used in a compute workflow.
 *
 * Shader resource bindings must be authored to follow a particular order
 * depending on the shader format.
 *
 * For SPIR-V shaders, use the following resource sets:
 *
 * - 0: Sampled textures, followed by read-only storage textures, followed by
 *   read-only storage buffers
 * - 1: Write-only storage textures, followed by write-only storage buffers
 * - 2: Uniform buffers
 *
 * For DXBC Shader Model 5_0 shaders, use the following register order:
 *
 * - t registers: Sampled textures, followed by read-only storage textures,
 *   followed by read-only storage buffers
 * - u registers: Write-only storage textures, followed by write-only storage
 *   buffers
 * - b registers: Uniform buffers
 *
 * For DXIL shaders, use the following register order:
 *
 * - (t[n], space0): Sampled textures, followed by read-only storage textures,
 *   followed by read-only storage buffers
 * - (u[n], space1): Write-only storage textures, followed by write-only
 *   storage buffers
 * - (b[n], space2): Uniform buffers
 *
 * For MSL/metallib, use the following order:
 *
 * - [[buffer]]: Uniform buffers, followed by write-only storage buffers,
 *   followed by write-only storage buffers
 * - [[texture]]: Sampled textures, followed by read-only storage textures,
 *   followed by write-only storage textures
 *
 * \param device a GPU Context.
 * \param createinfo a struct describing the state of the compute pipeline to
 *                   create.
 * \returns a compute pipeline object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BindGPUComputePipeline
 * \sa SDL_ReleaseGPUComputePipeline
 */
extern SDL_DECLSPEC SDL_GPUComputePipeline *SDLCALL SDL_CreateGPUComputePipeline(
    SDL_GPUDevice *device,
    const SDL_GPUComputePipelineCreateInfo *createinfo);

/**
 * Creates a pipeline object to be used in a graphics workflow.
 *
 * \param device a GPU Context.
 * \param createinfo a struct describing the state of the graphics pipeline to
 *                   create.
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
    const SDL_GPUGraphicsPipelineCreateInfo *createinfo);

/**
 * Creates a sampler object to be used when binding textures in a graphics
 * workflow.
 *
 * \param device a GPU Context.
 * \param createinfo a struct describing the state of the sampler to create.
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
    const SDL_GPUSamplerCreateInfo *createinfo);

/**
 * Creates a shader to be used when creating a graphics pipeline.
 *
 * Shader resource bindings must be authored to follow a particular order
 * depending on the shader format.
 *
 * For SPIR-V shaders, use the following resource sets:
 *
 * For vertex shaders:
 *
 * - 0: Sampled textures, followed by storage textures, followed by storage
 *   buffers
 * - 1: Uniform buffers
 *
 * For fragment shaders:
 *
 * - 2: Sampled textures, followed by storage textures, followed by storage
 *   buffers
 * - 3: Uniform buffers
 *
 * For DXBC Shader Model 5_0 shaders, use the following register order:
 *
 * - t registers: Sampled textures, followed by storage textures, followed by
 *   storage buffers
 * - s registers: Samplers with indices corresponding to the sampled textures
 * - b registers: Uniform buffers
 *
 * For DXIL shaders, use the following register order:
 *
 * For vertex shaders:
 *
 * - (t[n], space0): Sampled textures, followed by storage textures, followed
 *   by storage buffers
 * - (s[n], space0): Samplers with indices corresponding to the sampled
 *   textures
 * - (b[n], space1): Uniform buffers
 *
 * For pixel shaders:
 *
 * - (t[n], space2): Sampled textures, followed by storage textures, followed
 *   by storage buffers
 * - (s[n], space2): Samplers with indices corresponding to the sampled
 *   textures
 * - (b[n], space3): Uniform buffers
 *
 * For MSL/metallib, use the following order:
 *
 * - [[texture]]: Sampled textures, followed by storage textures
 * - [[sampler]]: Samplers with indices corresponding to the sampled textures
 * - [[buffer]]: Uniform buffers, followed by storage buffers. Vertex buffer 0
 *   is bound at [[buffer(14)]], vertex buffer 1 at [[buffer(15)]], and so on.
 *   Rather than manually authoring vertex buffer indices, use the
 *   [[stage_in]] attribute which will automatically use the vertex input
 *   information from the SDL_GPUPipeline.
 *
 * \param device a GPU Context.
 * \param createinfo a struct describing the state of the shader to create.
 * \returns a shader object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateGPUGraphicsPipeline
 * \sa SDL_ReleaseGPUShader
 */
extern SDL_DECLSPEC SDL_GPUShader *SDLCALL SDL_CreateGPUShader(
    SDL_GPUDevice *device,
    const SDL_GPUShaderCreateInfo *createinfo);

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
 * \param createinfo a struct describing the state of the texture to create.
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
    const SDL_GPUTextureCreateInfo *createinfo);

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
 * \param createinfo a struct describing the state of the buffer to create.
 * \returns a buffer object on success, or NULL on failure.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetGPUBufferName
 * \sa SDL_UploadToGPUBuffer
 * \sa SDL_DownloadFromGPUBuffer
 * \sa SDL_CopyGPUBufferToBuffer
 * \sa SDL_BindGPUVertexBuffers
 * \sa SDL_BindGPUIndexBuffer
 * \sa SDL_BindGPUVertexStorageBuffers
 * \sa SDL_BindGPUFragmentStorageBuffers
 * \sa SDL_DrawGPUPrimitivesIndirect
 * \sa SDL_DrawGPUIndexedPrimitivesIndirect
 * \sa SDL_BindGPUComputeStorageBuffers
 * \sa SDL_DispatchGPUComputeIndirect
 * \sa SDL_ReleaseGPUBuffer
 */
extern SDL_DECLSPEC SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(
    SDL_GPUDevice *device,
    const SDL_GPUBufferCreateInfo *createinfo);

/**
 * Creates a transfer buffer to be used when uploading to or downloading from
 * graphics resources.
 *
 * \param device a GPU Context.
 * \param createinfo a struct describing the state of the transfer buffer to
 *                   create.
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
    const SDL_GPUTransferBufferCreateInfo *createinfo);

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
 * \param command_buffer a command buffer.
 * \param text a UTF-8 string constant to insert as the label.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_InsertGPUDebugLabel(
    SDL_GPUCommandBuffer *command_buffer,
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
 * \param command_buffer a command buffer.
 * \param name a UTF-8 string constant that names the group.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_PopGPUDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUDebugGroup(
    SDL_GPUCommandBuffer *command_buffer,
    const char *name);

/**
 * Ends the most-recently pushed debug group.
 *
 * \param command_buffer a command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_PushGPUDebugGroup
 */
extern SDL_DECLSPEC void SDLCALL SDL_PopGPUDebugGroup(
    SDL_GPUCommandBuffer *command_buffer);

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
 * You must not reference the sampler after calling this function.
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
 * \param transfer_buffer a transfer buffer to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transfer_buffer);

/**
 * Frees the given compute pipeline as soon as it is safe to do so.
 *
 * You must not reference the compute pipeline after calling this function.
 *
 * \param device a GPU context.
 * \param compute_pipeline a compute pipeline to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUComputePipeline(
    SDL_GPUDevice *device,
    SDL_GPUComputePipeline *compute_pipeline);

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
 * \param graphics_pipeline a graphics pipeline to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_ReleaseGPUGraphicsPipeline(
    SDL_GPUDevice *device,
    SDL_GPUGraphicsPipeline *graphics_pipeline);

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
 * \param command_buffer a command buffer.
 * \param slot_index the vertex uniform slot to push data to.
 * \param data client data to write.
 * \param length the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUVertexUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length);

/**
 * Pushes data to a fragment uniform slot on the command buffer.
 *
 * Subsequent draw calls will use this uniform data.
 *
 * \param command_buffer a command buffer.
 * \param slot_index the fragment uniform slot to push data to.
 * \param data client data to write.
 * \param length the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUFragmentUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length);

/**
 * Pushes data to a uniform slot on the command buffer.
 *
 * Subsequent draw calls will use this uniform data.
 *
 * \param command_buffer a command buffer.
 * \param slot_index the uniform slot to push data to.
 * \param data client data to write.
 * \param length the length of the data to write.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_PushGPUComputeUniformData(
    SDL_GPUCommandBuffer *command_buffer,
    Uint32 slot_index,
    const void *data,
    Uint32 length);

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
 * When cycle is true, if the resource is bound, the cycle rotates to the next unbound internal resource,
 * or if none are available, a new one is created.
 * This means you don't have to worry about complex state tracking and synchronization as long as cycling is correctly employed.
 *
 * For example: you can call MapTransferBuffer, write texture data, UnmapTransferBuffer, and then UploadToTexture.
 * The next time you write texture data to the transfer buffer, if you set the cycle param to true, you don't have
 * to worry about overwriting any data that is not yet uploaded.
 *
 * Another example: If you are using a texture in a render pass every frame, this can cause a data dependency between frames.
 * If you set cycle to true in the SDL_GPUColorTargetInfo struct, you can prevent this data dependency.
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
 * \param command_buffer a command buffer.
 * \param color_target_infos an array of texture subresources with
 *                           corresponding clear values and load/store ops.
 * \param num_color_targets the number of color targets in the
 *                          color_target_infos array.
 * \param depth_stencil_target_info a texture subresource with corresponding
 *                                  clear value and load/store ops, may be
 *                                  NULL.
 * \returns a render pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_EndGPURenderPass
 */
extern SDL_DECLSPEC SDL_GPURenderPass *SDLCALL SDL_BeginGPURenderPass(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUColorTargetInfo *color_target_infos,
    Uint32 num_color_targets,
    const SDL_GPUDepthStencilTargetInfo *depth_stencil_target_info);

/**
 * Binds a graphics pipeline on a render pass to be used in rendering.
 *
 * A graphics pipeline must be bound before making any draw calls.
 *
 * \param render_pass a render pass handle.
 * \param graphics_pipeline the graphics pipeline to bind.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUGraphicsPipeline(
    SDL_GPURenderPass *render_pass,
    SDL_GPUGraphicsPipeline *graphics_pipeline);

/**
 * Sets the current viewport state on a command buffer.
 *
 * \param render_pass a render pass handle.
 * \param viewport the viewport to set.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUViewport(
    SDL_GPURenderPass *render_pass,
    const SDL_GPUViewport *viewport);

/**
 * Sets the current scissor state on a command buffer.
 *
 * \param render_pass a render pass handle.
 * \param scissor the scissor area to set.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUScissor(
    SDL_GPURenderPass *render_pass,
    const SDL_Rect *scissor);

/**
 * Sets the current blend constants on a command buffer.
 *
 * \param render_pass a render pass handle.
 * \param blend_constants the blend constant color.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GPU_BLENDFACTOR_CONSTANT_COLOR
 * \sa SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUBlendConstants(
    SDL_GPURenderPass *render_pass,
    SDL_FColor blend_constants);

/**
 * Sets the current stencil reference value on a command buffer.
 *
 * \param render_pass a render pass handle.
 * \param reference the stencil reference value to set.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetGPUStencilReference(
    SDL_GPURenderPass *render_pass,
    Uint8 reference);

/**
 * Binds vertex buffers on a command buffer for use with subsequent draw
 * calls.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the vertex buffer slot to begin binding from.
 * \param bindings an array of SDL_GPUBufferBinding structs containing vertex
 *                 buffers and offset values.
 * \param num_bindings the number of bindings in the bindings array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexBuffers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    const SDL_GPUBufferBinding *bindings,
    Uint32 num_bindings);

/**
 * Binds an index buffer on a command buffer for use with subsequent draw
 * calls.
 *
 * \param render_pass a render pass handle.
 * \param binding a pointer to a struct containing an index buffer and offset.
 * \param index_element_size whether the index values in the buffer are 16- or
 *                           32-bit.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUIndexBuffer(
    SDL_GPURenderPass *render_pass,
    const SDL_GPUBufferBinding *binding,
    SDL_GPUIndexElementSize index_element_size);

/**
 * Binds texture-sampler pairs for use on the vertex shader.
 *
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the vertex sampler slot to begin binding from.
 * \param texture_sampler_bindings an array of texture-sampler binding
 *                                 structs.
 * \param num_bindings the number of texture-sampler pairs to bind from the
 *                     array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexSamplers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings);

/**
 * Binds storage textures for use on the vertex shader.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the vertex storage texture slot to begin binding from.
 * \param storage_textures an array of storage textures.
 * \param num_bindings the number of storage texture to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexStorageTextures(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings);

/**
 * Binds storage buffers for use on the vertex shader.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the vertex storage buffer slot to begin binding from.
 * \param storage_buffers an array of buffers.
 * \param num_bindings the number of buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUVertexStorageBuffers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings);

/**
 * Binds texture-sampler pairs for use on the fragment shader.
 *
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the fragment sampler slot to begin binding from.
 * \param texture_sampler_bindings an array of texture-sampler binding
 *                                 structs.
 * \param num_bindings the number of texture-sampler pairs to bind from the
 *                     array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentSamplers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings);

/**
 * Binds storage textures for use on the fragment shader.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the fragment storage texture slot to begin binding from.
 * \param storage_textures an array of storage textures.
 * \param num_bindings the number of storage textures to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentStorageTextures(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings);

/**
 * Binds storage buffers for use on the fragment shader.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ.
 *
 * \param render_pass a render pass handle.
 * \param first_slot the fragment storage buffer slot to begin binding from.
 * \param storage_buffers an array of storage buffers.
 * \param num_bindings the number of storage buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUFragmentStorageBuffers(
    SDL_GPURenderPass *render_pass,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings);

/* Drawing */

/**
 * Draws data using bound graphics state with an index buffer and instancing
 * enabled.
 *
 * You must not call this function before binding a graphics pipeline.
 *
 * Note that the `first_vertex` and `first_instance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \param render_pass a render pass handle.
 * \param num_indices the number of indices to draw per instance.
 * \param num_instances the number of instances to draw.
 * \param first_index the starting index within the index buffer.
 * \param vertex_offset value added to vertex index before indexing into the
 *                      vertex buffer.
 * \param first_instance the ID of the first instance to draw.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUIndexedPrimitives(
    SDL_GPURenderPass *render_pass,
    Uint32 num_indices,
    Uint32 num_instances,
    Uint32 first_index,
    Sint32 vertex_offset,
    Uint32 first_instance);

/**
 * Draws data using bound graphics state.
 *
 * You must not call this function before binding a graphics pipeline.
 *
 * Note that the `first_vertex` and `first_instance` parameters are NOT
 * compatible with built-in vertex/instance ID variables in shaders (for
 * example, SV_VertexID). If your shader depends on these variables, the
 * correlating draw call parameter MUST be 0.
 *
 * \param render_pass a render pass handle.
 * \param num_vertices the number of vertices to draw.
 * \param num_instances the number of instances that will be drawn.
 * \param first_vertex the index of the first vertex to draw.
 * \param first_instance the ID of the first instance to draw.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUPrimitives(
    SDL_GPURenderPass *render_pass,
    Uint32 num_vertices,
    Uint32 num_instances,
    Uint32 first_vertex,
    Uint32 first_instance);

/**
 * Draws data using bound graphics state and with draw parameters set from a
 * buffer.
 *
 * The buffer must consist of tightly-packed draw parameter sets that each
 * match the layout of SDL_GPUIndirectDrawCommand. You must not call this
 * function before binding a graphics pipeline.
 *
 * \param render_pass a render pass handle.
 * \param buffer a buffer containing draw parameters.
 * \param offset the offset to start reading from the draw buffer.
 * \param draw_count the number of draw parameter sets that should be read
 *                   from the draw buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUPrimitivesIndirect(
    SDL_GPURenderPass *render_pass,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 draw_count);

/**
 * Draws data using bound graphics state with an index buffer enabled and with
 * draw parameters set from a buffer.
 *
 * The buffer must consist of tightly-packed draw parameter sets that each
 * match the layout of SDL_GPUIndexedIndirectDrawCommand. You must not call
 * this function before binding a graphics pipeline.
 *
 * \param render_pass a render pass handle.
 * \param buffer a buffer containing draw parameters.
 * \param offset the offset to start reading from the draw buffer.
 * \param draw_count the number of draw parameter sets that should be read
 *                   from the draw buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DrawGPUIndexedPrimitivesIndirect(
    SDL_GPURenderPass *render_pass,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 draw_count);

/**
 * Ends the given render pass.
 *
 * All bound graphics state on the render pass command buffer is unset. The
 * render pass handle is now invalid.
 *
 * \param render_pass a render pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPURenderPass(
    SDL_GPURenderPass *render_pass);

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
 * \param command_buffer a command buffer.
 * \param storage_texture_bindings an array of writeable storage texture
 *                                 binding structs.
 * \param num_storage_texture_bindings the number of storage textures to bind
 *                                     from the array.
 * \param storage_buffer_bindings an array of writeable storage buffer binding
 *                                structs.
 * \param num_storage_buffer_bindings the number of storage buffers to bind
 *                                    from the array.
 * \returns a compute pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_EndGPUComputePass
 */
extern SDL_DECLSPEC SDL_GPUComputePass *SDLCALL SDL_BeginGPUComputePass(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUStorageTextureWriteOnlyBinding *storage_texture_bindings,
    Uint32 num_storage_texture_bindings,
    const SDL_GPUStorageBufferWriteOnlyBinding *storage_buffer_bindings,
    Uint32 num_storage_buffer_bindings);

/**
 * Binds a compute pipeline on a command buffer for use in compute dispatch.
 *
 * \param compute_pass a compute pass handle.
 * \param compute_pipeline a compute pipeline to bind.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputePipeline(
    SDL_GPUComputePass *compute_pass,
    SDL_GPUComputePipeline *compute_pipeline);

/**
 * Binds texture-sampler pairs for use on the compute shader.
 *
 * The textures must have been created with SDL_GPU_TEXTUREUSAGE_SAMPLER.
 *
 * \param compute_pass a compute pass handle.
 * \param first_slot the compute sampler slot to begin binding from.
 * \param texture_sampler_bindings an array of texture-sampler binding
 *                                 structs.
 * \param num_bindings the number of texture-sampler bindings to bind from the
 *                     array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputeSamplers(
    SDL_GPUComputePass *compute_pass,
    Uint32 first_slot,
    const SDL_GPUTextureSamplerBinding *texture_sampler_bindings,
    Uint32 num_bindings);

/**
 * Binds storage textures as readonly for use on the compute pipeline.
 *
 * These textures must have been created with
 * SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ.
 *
 * \param compute_pass a compute pass handle.
 * \param first_slot the compute storage texture slot to begin binding from.
 * \param storage_textures an array of storage textures.
 * \param num_bindings the number of storage textures to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputeStorageTextures(
    SDL_GPUComputePass *compute_pass,
    Uint32 first_slot,
    SDL_GPUTexture *const *storage_textures,
    Uint32 num_bindings);

/**
 * Binds storage buffers as readonly for use on the compute pipeline.
 *
 * These buffers must have been created with
 * SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ.
 *
 * \param compute_pass a compute pass handle.
 * \param first_slot the compute storage buffer slot to begin binding from.
 * \param storage_buffers an array of storage buffer binding structs.
 * \param num_bindings the number of storage buffers to bind from the array.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BindGPUComputeStorageBuffers(
    SDL_GPUComputePass *compute_pass,
    Uint32 first_slot,
    SDL_GPUBuffer *const *storage_buffers,
    Uint32 num_bindings);

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
 * \param compute_pass a compute pass handle.
 * \param groupcount_x number of local workgroups to dispatch in the X
 *                     dimension.
 * \param groupcount_y number of local workgroups to dispatch in the Y
 *                     dimension.
 * \param groupcount_z number of local workgroups to dispatch in the Z
 *                     dimension.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DispatchGPUCompute(
    SDL_GPUComputePass *compute_pass,
    Uint32 groupcount_x,
    Uint32 groupcount_y,
    Uint32 groupcount_z);

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
 * \param compute_pass a compute pass handle.
 * \param buffer a buffer containing dispatch parameters.
 * \param offset the offset to start reading from the dispatch buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DispatchGPUComputeIndirect(
    SDL_GPUComputePass *compute_pass,
    SDL_GPUBuffer *buffer,
    Uint32 offset);

/**
 * Ends the current compute pass.
 *
 * All bound compute state on the command buffer is unset. The compute pass
 * handle is now invalid.
 *
 * \param compute_pass a compute pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPUComputePass(
    SDL_GPUComputePass *compute_pass);

/* TransferBuffer Data */

/**
 * Maps a transfer buffer into application address space.
 *
 * You must unmap the transfer buffer before encoding upload commands.
 *
 * \param device a GPU context.
 * \param transfer_buffer a transfer buffer.
 * \param cycle if true, cycles the transfer buffer if it is already bound.
 * \returns the address of the mapped transfer buffer memory.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void *SDLCALL SDL_MapGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transfer_buffer,
    bool cycle);

/**
 * Unmaps a previously mapped transfer buffer.
 *
 * \param device a GPU context.
 * \param transfer_buffer a previously mapped transfer buffer.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UnmapGPUTransferBuffer(
    SDL_GPUDevice *device,
    SDL_GPUTransferBuffer *transfer_buffer);

/* Copy Pass */

/**
 * Begins a copy pass on a command buffer.
 *
 * All operations related to copying to or from buffers or textures take place
 * inside a copy pass. You must not begin another copy pass, or a render pass
 * or compute pass before ending the copy pass.
 *
 * \param command_buffer a command buffer.
 * \returns a copy pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_GPUCopyPass *SDLCALL SDL_BeginGPUCopyPass(
    SDL_GPUCommandBuffer *command_buffer);

/**
 * Uploads data from a transfer buffer to a texture.
 *
 * The upload occurs on the GPU timeline. You may assume that the upload has
 * finished in subsequent commands.
 *
 * You must align the data in the transfer buffer to a multiple of the texel
 * size of the texture format.
 *
 * \param copy_pass a copy pass handle.
 * \param source the source transfer buffer with image layout information.
 * \param destination the destination texture region.
 * \param cycle if true, cycles the texture if the texture is bound, otherwise
 *              overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UploadToGPUTexture(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUTextureTransferInfo *source,
    const SDL_GPUTextureRegion *destination,
    bool cycle);

/* Uploads data from a TransferBuffer to a Buffer. */

/**
 * Uploads data from a transfer buffer to a buffer.
 *
 * The upload occurs on the GPU timeline. You may assume that the upload has
 * finished in subsequent commands.
 *
 * \param copy_pass a copy pass handle.
 * \param source the source transfer buffer with offset.
 * \param destination the destination buffer with offset and size.
 * \param cycle if true, cycles the buffer if it is already bound, otherwise
 *              overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_UploadToGPUBuffer(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUTransferBufferLocation *source,
    const SDL_GPUBufferRegion *destination,
    bool cycle);

/**
 * Performs a texture-to-texture copy.
 *
 * This copy occurs on the GPU timeline. You may assume the copy has finished
 * in subsequent commands.
 *
 * \param copy_pass a copy pass handle.
 * \param source a source texture region.
 * \param destination a destination texture region.
 * \param w the width of the region to copy.
 * \param h the height of the region to copy.
 * \param d the depth of the region to copy.
 * \param cycle if true, cycles the destination texture if the destination
 *              texture is bound, otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_CopyGPUTextureToTexture(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUTextureLocation *source,
    const SDL_GPUTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    bool cycle);

/* Copies data from a buffer to a buffer. */

/**
 * Performs a buffer-to-buffer copy.
 *
 * This copy occurs on the GPU timeline. You may assume the copy has finished
 * in subsequent commands.
 *
 * \param copy_pass a copy pass handle.
 * \param source the buffer and offset to copy from.
 * \param destination the buffer and offset to copy to.
 * \param size the length of the buffer to copy.
 * \param cycle if true, cycles the destination buffer if it is already bound,
 *              otherwise overwrites the data.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_CopyGPUBufferToBuffer(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUBufferLocation *source,
    const SDL_GPUBufferLocation *destination,
    Uint32 size,
    bool cycle);

/**
 * Copies data from a texture to a transfer buffer on the GPU timeline.
 *
 * This data is not guaranteed to be copied until the command buffer fence is
 * signaled.
 *
 * \param copy_pass a copy pass handle.
 * \param source the source texture region.
 * \param destination the destination transfer buffer with image layout
 *                    information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DownloadFromGPUTexture(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUTextureRegion *source,
    const SDL_GPUTextureTransferInfo *destination);

/**
 * Copies data from a buffer to a transfer buffer on the GPU timeline.
 *
 * This data is not guaranteed to be copied until the command buffer fence is
 * signaled.
 *
 * \param copy_pass a copy pass handle.
 * \param source the source buffer with offset and size.
 * \param destination the destination transfer buffer with offset.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DownloadFromGPUBuffer(
    SDL_GPUCopyPass *copy_pass,
    const SDL_GPUBufferRegion *source,
    const SDL_GPUTransferBufferLocation *destination);

/**
 * Ends the current copy pass.
 *
 * \param copy_pass a copy pass handle.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_EndGPUCopyPass(
    SDL_GPUCopyPass *copy_pass);

/**
 * Generates mipmaps for the given texture.
 *
 * This function must not be called inside of any pass.
 *
 * \param command_buffer a command_buffer.
 * \param texture a texture with more than 1 mip level.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_GenerateMipmapsForGPUTexture(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_GPUTexture *texture);

/**
 * Blits from a source texture region to a destination texture region.
 *
 * This function must not be called inside of any pass.
 *
 * \param command_buffer a command buffer.
 * \param info the blit info struct containing the blit parameters.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_BlitGPUTexture(
    SDL_GPUCommandBuffer *command_buffer,
    const SDL_GPUBlitInfo *info);

/* Submission/Presentation */

/**
 * Determines whether a swapchain composition is supported by the window.
 *
 * The window must be claimed before calling this function.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \param swapchain_composition the swapchain composition to check.
 * \returns true if supported, false if unsupported (or on error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 */
extern SDL_DECLSPEC bool SDLCALL SDL_WindowSupportsGPUSwapchainComposition(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchain_composition);

/**
 * Determines whether a presentation mode is supported by the window.
 *
 * The window must be claimed before calling this function.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \param present_mode the presentation mode to check.
 * \returns true if supported, false if unsupported (or on error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 */
extern SDL_DECLSPEC bool SDLCALL SDL_WindowSupportsGPUPresentMode(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUPresentMode present_mode);

/**
 * Claims a window, creating a swapchain structure for it.
 *
 * This must be called before SDL_AcquireGPUSwapchainTexture is called using
 * the window.
 *
 * The swapchain will be created with SDL_GPU_SWAPCHAINCOMPOSITION_SDR and
 * SDL_GPU_PRESENTMODE_VSYNC. If you want to have different swapchain
 * parameters, you must call SDL_SetGPUSwapchainParameters after claiming the
 * window.
 *
 * \param device a GPU context.
 * \param window an SDL_Window.
 * \returns true on success, otherwise false.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireGPUSwapchainTexture
 * \sa SDL_ReleaseWindowFromGPUDevice
 * \sa SDL_WindowSupportsGPUPresentMode
 * \sa SDL_WindowSupportsGPUSwapchainComposition
 */
extern SDL_DECLSPEC bool SDLCALL SDL_ClaimWindowForGPUDevice(
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
 * \param swapchain_composition the desired composition of the swapchain.
 * \param present_mode the desired present mode for the swapchain.
 * \returns true if successful, false on error.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_WindowSupportsGPUPresentMode
 * \sa SDL_WindowSupportsGPUSwapchainComposition
 */
extern SDL_DECLSPEC bool SDLCALL SDL_SetGPUSwapchainParameters(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUSwapchainComposition swapchain_composition,
    SDL_GPUPresentMode present_mode);

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
 * \param command_buffer a command buffer.
 * \param window a window that has been claimed.
 * \param w a pointer filled in with the swapchain width.
 * \param h a pointer filled in with the swapchain height.
 * \returns a swapchain texture.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClaimWindowForGPUDevice
 * \sa SDL_SubmitGPUCommandBuffer
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC SDL_GPUTexture *SDLCALL SDL_AcquireGPUSwapchainTexture(
    SDL_GPUCommandBuffer *command_buffer,
    SDL_Window *window,
    Uint32 *w,
    Uint32 *h);

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
 * \param command_buffer a command buffer.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AcquireGPUCommandBuffer
 * \sa SDL_AcquireGPUSwapchainTexture
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC void SDLCALL SDL_SubmitGPUCommandBuffer(
    SDL_GPUCommandBuffer *command_buffer);

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
 * \param command_buffer a command buffer.
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
    SDL_GPUCommandBuffer *command_buffer);

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
 * \param wait_all if 0, wait for any fence to be signaled, if 1, wait for all
 *                 fences to be signaled.
 * \param fences an array of fences to wait on.
 * \param num_fences the number of fences in the fences array.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 * \sa SDL_WaitForGPUIdle
 */
extern SDL_DECLSPEC void SDLCALL SDL_WaitForGPUFences(
    SDL_GPUDevice *device,
    bool wait_all,
    SDL_GPUFence *const *fences,
    Uint32 num_fences);

/**
 * Checks the status of a fence.
 *
 * \param device a GPU context.
 * \param fence a fence.
 * \returns true if the fence is signaled, false if it is not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SubmitGPUCommandBufferAndAcquireFence
 */
extern SDL_DECLSPEC bool SDLCALL SDL_QueryGPUFence(
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
 * \param format the texture format you want to know the texel size of.
 * \returns the texel block size of the texture format.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UploadToGPUTexture
 */
extern SDL_DECLSPEC Uint32 SDLCALL SDL_GPUTextureFormatTexelBlockSize(
    SDL_GPUTextureFormat format);

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
extern SDL_DECLSPEC bool SDLCALL SDL_GPUTextureSupportsFormat(
    SDL_GPUDevice *device,
    SDL_GPUTextureFormat format,
    SDL_GPUTextureType type,
    SDL_GPUTextureUsageFlags usage);

/**
 * Determines if a sample count for a texture format is supported.
 *
 * \param device a GPU context.
 * \param format the texture format to check.
 * \param sample_count the sample count to check.
 * \returns a hardware-specific version of min(preferred, possible).
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC bool SDLCALL SDL_GPUTextureSupportsSampleCount(
    SDL_GPUDevice *device,
    SDL_GPUTextureFormat format,
    SDL_GPUSampleCount sample_count);

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
