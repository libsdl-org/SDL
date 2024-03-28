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
typedef struct SDL_GpuShaderModule SDL_GpuShaderModule;
typedef struct SDL_GpuComputePipeline SDL_GpuComputePipeline;
typedef struct SDL_GpuGraphicsPipeline SDL_GpuGraphicsPipeline;
typedef struct SDL_GpuCommandBuffer SDL_GpuCommandBuffer;
typedef struct SDL_GpuFence SDL_GpuFence;

typedef enum SDL_GpuPresentMode
{
	SDL_GPU_PRESENTMODE_IMMEDIATE,
	SDL_GPU_PRESENTMODE_MAILBOX,
	SDL_GPU_PRESENTMODE_FIFO,
	SDL_GPU_PRESENTMODE_FIFO_RELAXED
} SDL_GpuPresentMode;

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
	SDL_GPU_TEXTUREFORMAT_R16G16,
	SDL_GPU_TEXTUREFORMAT_R16G16B16A16,
	SDL_GPU_TEXTUREFORMAT_R8,
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
	/* Depth Formats */
	SDL_GPU_TEXTUREFORMAT_D16_UNORM,
	SDL_GPU_TEXTUREFORMAT_D32_SFLOAT,
	SDL_GPU_TEXTUREFORMAT_D16_UNORM_S8_UINT,
	SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT
} SDL_GpuTextureFormat;

typedef enum SDL_GpuTextureUsageFlagBits
{
	SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT              = 0x00000001,
	SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT         = 0x00000002,
	SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT = 0x00000004,
	SDL_GPU_TEXTUREUSAGE_COMPUTE_BIT              = 0X00000008
} SDL_GpuTextureUsageFlagBits;

typedef Uint32 SDL_GpuTextureUsageFlags;

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
	SDL_GPU_BUFFERUSAGE_VERTEX_BIT 	 = 0x00000001,
	SDL_GPU_BUFFERUSAGE_INDEX_BIT  	 = 0x00000002,
	SDL_GPU_BUFFERUSAGE_COMPUTE_BIT  = 0x00000004,
	SDL_GPU_BUFFERUSAGE_INDIRECT_BIT = 0x00000008
} SDL_GpuBufferUsageFlagBits;

typedef Uint32 SDL_GpuBufferUsageFlags;

typedef enum SDL_GpuShaderType
{
	SDL_GPU_SHADERTYPE_VERTEX,
	SDL_GPU_SHADERTYPE_FRAGMENT,
	SDL_GPU_SHADERTYPE_COMPUTE
} SDL_GpuShaderType;

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
	SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_BORDER
} SDL_GpuSamplerAddressMode;

/* FIXME: we should probably make a library-level decision about color types */
typedef enum SDL_GpuBorderColor
{
	SDL_GPU_BORDERCOLOR_FLOAT_TRANSPARENT_BLACK,
	SDL_GPU_BORDERCOLOR_INT_TRANSPARENT_BLACK,
	SDL_GPU_BORDERCOLOR_FLOAT_OPAQUE_BLACK,
	SDL_GPU_BORDERCOLOR_INT_OPAQUE_BLACK,
	SDL_GPU_BORDERCOLOR_FLOAT_OPAQUE_WHITE,
	SDL_GPU_BORDERCOLOR_INT_OPAQUE_WHITE
} SDL_GpuBorderColor;

typedef enum SDL_GpuTransferUsage
{
	SDL_GPU_TRANSFERUSAGE_BUFFER,
	SDL_GPU_TRANSFERUSAGE_TEXTURE
} SDL_GpuTransferUsage;

typedef enum SDL_GpuTransferOptions
{
	SDL_GPU_TRANSFEROPTIONS_CYCLE,
	SDL_GPU_TRANSFEROPTIONS_UNSAFE
} SDL_GpuTransferOptions;

typedef enum SDL_GpuBufferWriteOptions
{
	SDL_GPU_BUFFERWRITEOPTIONS_CYCLE,
	SDL_GPU_BUFFERWRITEOPTIONS_UNSAFE
} SDL_GpuBufferWriteOptions;

typedef enum SDL_GpuTextureWriteOptions
{
	SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE,
	SDL_GPU_TEXTUREWRITEOPTIONS_SAFE
} SDL_GpuTextureWriteOptions;

typedef enum SDL_GpuComputeBindOptions
{
	SDL_GPU_COMPUTEBINDOPTIONS_CYCLE,
	SDL_GPU_COMPUTEBINDOPTIONS_SAFE
} SDL_GpuComputeBindOptions;

typedef enum SDL_GpuBackend
{
	SDL_GPU_BACKEND_VULKAN,
	SDL_GPU_BACKEND_D3D11,
	SDL_GPU_BACKEND_METAL,
	SDL_GPU_BACKEND_INVALID
} SDL_GpuBackend;

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

typedef struct SDL_GpuVec4
{
	float x;
	float y;
	float z;
	float w;
} SDL_GpuVec4;

typedef struct SDL_GpuViewport
{
	float x;
	float y;
	float w;
	float h;
	float minDepth;
	float maxDepth;
} SDL_GpuViewport;

typedef struct SDL_GpuTextureSlice
{
	SDL_GpuTexture *texture;
	Uint32 mipLevel;
	Uint32 layer;
} SDL_GpuTextureSlice;

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

typedef struct SDL_GpuBufferImageCopy
{
	Uint32 bufferOffset;
	Uint32 bufferStride;
	Uint32 bufferImageHeight;
} SDL_GpuBufferImageCopy;

typedef struct SDL_GpuBufferCopy
{
	Uint32 srcOffset;
	Uint32 dstOffset;
	Uint32 size;
} SDL_GpuBufferCopy;

typedef struct SDL_GpuIndirectDrawCommand
{
	Uint32 vertexCount;
	Uint32 instanceCount;
	Uint32 firstVertex;
	Uint32 firstInstance;
} SDL_GpuIndirectDrawCommand;

/* State structures */

typedef struct SDL_GpuSamplerStateCreateInfo
{
	SDL_GpuFilter minFilter;
	SDL_GpuFilter magFilter;
	SDL_GpuSamplerMipmapMode mipmapMode;
	SDL_GpuSamplerAddressMode addressModeU;
	SDL_GpuSamplerAddressMode addressModeV;
	SDL_GpuSamplerAddressMode addressModeW;
	float mipLodBias;
	Uint8 anisotropyEnable;
	float maxAnisotropy;
	Uint8 compareEnable;
	SDL_GpuCompareOp compareOp;
	float minLod;
	float maxLod;
	SDL_GpuBorderColor borderColor;
} SDL_GpuSamplerStateCreateInfo;

typedef struct SDL_GpuVertexBinding
{
	Uint32 binding;
	Uint32 stride;
	SDL_GpuVertexInputRate inputRate;
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
	Uint8 blendEnable;
	SDL_GpuBlendFactor srcColorBlendFactor;
	SDL_GpuBlendFactor dstColorBlendFactor;
	SDL_GpuBlendOp colorBlendOp;
	SDL_GpuBlendFactor srcAlphaBlendFactor;
	SDL_GpuBlendFactor dstAlphaBlendFactor;
	SDL_GpuBlendOp alphaBlendOp;
	SDL_GpuColorComponentFlags colorWriteMask;
} SDL_GpuColorAttachmentBlendState;

typedef struct SDL_GpuShaderModuleCreateInfo
{
	size_t codeSize;
	const Uint8 *code;
	SDL_GpuShaderType type;
} SDL_GpuShaderModuleCreateInfo;

typedef struct SDL_GpuTextureCreateInfo
{
	Uint32 width;
	Uint32 height;
	Uint32 depth;
	Uint8 isCube;
	Uint32 layerCount;
	Uint32 levelCount;
	SDL_GpuSampleCount sampleCount;
	SDL_GpuTextureFormat format;
	SDL_GpuTextureUsageFlags usageFlags;
} SDL_GpuTextureCreateInfo;

/* Pipeline state structures */

typedef struct SDL_GpuGraphicsShaderInfo
{
	SDL_GpuShaderModule *shaderModule;
	const char* entryPointName;
	Uint32 uniformBufferSize;
	Uint32 samplerBindingCount;
} SDL_GpuGraphicsShaderInfo;

typedef struct SDL_GpuComputeShaderInfo
{
	SDL_GpuShaderModule* shaderModule;
	const char* entryPointName;
	Uint32 uniformBufferSize;
	Uint32 bufferBindingCount;
	Uint32 imageBindingCount;
} SDL_GpuComputeShaderInfo;

typedef struct SDL_GpuRasterizerState
{
	SDL_GpuFillMode fillMode;
	SDL_GpuCullMode cullMode;
	SDL_GpuFrontFace frontFace;
	Uint8 depthBiasEnable;
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
	Uint8 depthTestEnable;
	Uint8 depthWriteEnable;
	SDL_GpuCompareOp compareOp;
	Uint8 depthBoundsTestEnable;
	Uint8 stencilTestEnable;
	SDL_GpuStencilOpState backStencilState;
	SDL_GpuStencilOpState frontStencilState;
	Uint32 compareMask;
	Uint32 writeMask;
	Uint32 reference;
	float minDepthBounds;
	float maxDepthBounds;
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
	Uint8 hasDepthStencilAttachment;
	SDL_GpuTextureFormat depthStencilFormat;
} SDL_GpuGraphicsPipelineAttachmentInfo;

typedef struct SDL_GpuGraphicsPipelineCreateInfo
{
	SDL_GpuGraphicsShaderInfo vertexShaderInfo;
	SDL_GpuGraphicsShaderInfo fragmentShaderInfo;
	SDL_GpuVertexInputState vertexInputState;
	SDL_GpuPrimitiveType primitiveType;
	SDL_GpuRasterizerState rasterizerState;
	SDL_GpuMultisampleState multisampleState;
	SDL_GpuDepthStencilState depthStencilState;
	SDL_GpuGraphicsPipelineAttachmentInfo attachmentInfo;
	float blendConstants[4];
} SDL_GpuGraphicsPipelineCreateInfo;

/* Render pass structures */

/* These structures define how textures will be read/written in a render pass.
 *
 * loadOp: Determines what is done with the texture slice at the beginning of the render pass.
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
 *
 * storeOp: Determines what is done with the texture slice at the end of the render pass.
 *
 *   STORE:
 *     Stores the results of the render pass in the texture slice.
 *
 *   DONT_CARE:
 *     The driver will do whatever it wants with the texture slice memory.
 *     This is often a good option for depth/stencil textures.
 *
 *
 * writeOption is ignored if loadOp is LOAD and is implicitly assumed to be SAFE.
 * Interleaving LOAD and CYCLE successively on the same texture (not slice!) is undefined behavior.
 *
 * writeOption:
 *  CYCLE:
 *    If this texture slice has been used in commands that have not completed,
 *    the implementation may choose to prevent a data dependency at the cost of increased memory usage.
 *    You may NOT assume that any of the previous texture data is retained.
 *    This may prevent stalls when frequently reusing a texture slice in rendering.
 *
 *  UNSAFE:
 *    Overwrites the data unsafely. You must ensure that data used by earlier draw calls
 *    is not affected or visual corruption can occur.
 *
 *  SAFE:
 *    Overwrites the data safely. Earlier draw calls will not be affected.
 *    This is usually the slowest option.
 */

typedef struct SDL_GpuColorAttachmentInfo
{
	SDL_GpuTextureSlice textureSlice;
	SDL_GpuVec4 clearColor; /* Can be ignored by RenderPass if CLEAR is not used */
	SDL_GpuLoadOp loadOp;
	SDL_GpuStoreOp storeOp;
	SDL_GpuTextureWriteOptions writeOption;
} SDL_GpuColorAttachmentInfo;

typedef struct SDL_GpuDepthStencilAttachmentInfo
{
	SDL_GpuTextureSlice textureSlice;
	SDL_GpuDepthStencilValue depthStencilClearValue; /* Can be ignored by RenderPass if CLEAR is not used */
	SDL_GpuLoadOp loadOp;
	SDL_GpuStoreOp storeOp;
	SDL_GpuLoadOp stencilLoadOp;
	SDL_GpuStoreOp stencilStoreOp;
	SDL_GpuTextureWriteOptions writeOption;
} SDL_GpuDepthStencilAttachmentInfo;

/* Binding structs */

typedef struct SDL_GpuBufferBinding
{
	SDL_GpuBuffer *gpuBuffer;
	Uint32 offset;
} SDL_GpuBufferBinding;

typedef struct SDL_GpuTextureSamplerBinding
{
	SDL_GpuTexture *texture;
	SDL_GpuSampler *sampler;
} SDL_GpuTextureSamplerBinding;

typedef struct SDL_GpuComputeBufferBinding
{
	SDL_GpuBuffer *gpuBuffer;
	SDL_GpuComputeBindOptions writeOption;
} SDL_GpuComputeBufferBinding;

typedef struct SDL_GpuComputeTextureBinding
{
	SDL_GpuTextureSlice textureSlice;
	SDL_GpuComputeBindOptions writeOption;
} SDL_GpuComputeTextureBinding;

/* Functions */

/* Backend selection */

/* Select the graphics API backend that SDL should use.
 *
 * You must provide a pointer to an array of SDL_GpuBackend enums in order of desired selection.
 * If a backend fails to prepare, SDL will attempt to select the next one in the array.
 *
 * Returns the backend that will actually be used, and fills in a window flag bitmask.
 * This bitmask should be used to create all windows that the device claims.
 *
 * If all requested backends fail to prepare, this function returns SDL_GPU_BACKEND_INVALID.
 *
 * flags: A pointer to a bitflag value that will be filled in with required SDL_WindowFlags masks.
 */
extern DECLSPEC SDL_GpuBackend SDLCALL SDL_GpuSelectBackend(
	SDL_GpuBackend *preferredBackends,
	Uint32 preferredBackendCount,
	Uint32 *flags
);

/* Device */

/* Create a rendering context for use on the calling thread.
 * You MUST have called SDL_GpuSelectBackend prior to calling this function.
 *
 * debugMode: Enable debug mode properties.
 */
extern DECLSPEC SDL_GpuDevice *SDLCALL SDL_GpuCreateDevice(
	Uint8 debugMode
);

/* Destroys a rendering context previously returned by SDL_GpuCreateDevice. */
extern DECLSPEC void SDLCALL SDL_GpuDestroyDevice(SDL_GpuDevice *device);

/* State Creation */

/* Returns an allocated ComputePipeline* object. */
extern DECLSPEC SDL_GpuComputePipeline *SDLCALL SDL_GpuCreateComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuComputeShaderInfo *computeShaderInfo
);

/* Returns an allocated GraphicsPipeline* object. */
extern DECLSPEC SDL_GpuGraphicsPipeline *SDLCALL SDL_GpuCreateGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo
);

/* Returns an allocated Sampler* object. */
extern DECLSPEC SDL_GpuSampler *SDLCALL SDL_GpuCreateSampler(
	SDL_GpuDevice *device,
	SDL_GpuSamplerStateCreateInfo *samplerStateCreateInfo
);

/* Returns an allocated ShaderModule* object. */
extern DECLSPEC SDL_GpuShaderModule *SDLCALL SDL_GpuCreateShaderModule(
	SDL_GpuDevice *device,
	SDL_GpuShaderModuleCreateInfo *shaderModuleCreateInfo
);

/* Returns an allocated SDL_GpuTexture* object. Note that the contents of
 * the texture are undefined until SetData is called.
 */
extern DECLSPEC SDL_GpuTexture *SDLCALL SDL_GpuCreateTexture(
	SDL_GpuDevice *device,
	SDL_GpuTextureCreateInfo *textureCreateInfo
);

/* Creates a GpuBuffer.
 *
 * usageFlags:	Specifies how the buffer will be used.
 * sizeInBytes:	The length of the buffer.
 */
extern DECLSPEC SDL_GpuBuffer *SDLCALL SDL_GpuCreateGpuBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBufferUsageFlags usageFlags,
	Uint32 sizeInBytes
);

/* Creates a TransferBuffer.
 *
 * usage:
 *   Determines what kind of resource the buffer will transfer.
 *   D3D11's UpdateSubresource is busted when uploading buffers on some drivers,
 *   so this helps the implementation take an efficient path.
 * sizeInBytes: The length of the buffer.
 */
extern DECLSPEC SDL_GpuTransferBuffer *SDLCALL SDL_GpuCreateTransferBuffer(
	SDL_GpuDevice *device,
	SDL_GpuTransferUsage usage,
	Uint32 sizeInBytes
);

/* Debug Naming */

/* Sets an arbitrary string constant to be stored in a rendering API trace,
 * useful for labeling buffers for debugging purposes.
 *
 * buffer: The buffer to attach the name to.
 * text: The UTF-8 string constant to mark as the name of the buffer.
 */
extern DECLSPEC void SDLCALL SDL_GpuSetGpuBufferName(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *buffer,
	const char *text
);

/* Sets an arbitrary string constant to be stored in a rendering API trace,
 * useful for labeling textures for debugging purposes.
 *
 * texture: The texture to attach the name to.
 * text: The UTF-8 string constant to mark as the name of the texture.
 */
extern DECLSPEC void SDLCALL SDL_GpuSetTextureName(
	SDL_GpuDevice *device,
	SDL_GpuTexture *texture,
	const char *text
);

/* Disposal */

/* Sends a texture to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer if
 * this is not called from the main thread (for example, if a garbage collector
 * deletes the resource instead of the programmer).
 *
 * texture: The SDL_GpuTexture to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyTexture(
	SDL_GpuDevice *device,
	SDL_GpuTexture *texture
);

/* Sends a sampler to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer if
 * this is not called from the main thread (for example, if a garbage collector
 * deletes the resource instead of the programmer).
 *
 * texture: The SDL_GpuSampler to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroySampler(
	SDL_GpuDevice *device,
	SDL_GpuSampler *sampler
);

/* Sends a buffer to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer.
 *
 * buffer: The SDL_GpuBuffer to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyGpuBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *gpuBuffer
);

/* Sends a buffer to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer.
 *
 * buffer: The SDL_GpuTransferBuffer to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyTransferBuffer(
	SDL_GpuDevice *device,
	SDL_GpuTransferBuffer *transferBuffer
);

/* Sends a shader module to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer.
 *
 * shaderModule: The SDL_GpuShaderModule to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyShaderModule(
	SDL_GpuDevice *device,
	SDL_GpuShaderModule *shaderModule
);

/* Sends a compute pipeline to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer.
 *
 * computePipeline: The SDL_GpuComputePipeline to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuComputePipeline *computePipeline
);

/* Sends a graphics pipeline to be destroyed by the renderer. Note that we call it
 * "QueueDestroy" because it may not be immediately destroyed by the renderer.
 *
 * graphicsPipeline: The SDL_GpuGraphicsPipeline to be destroyed.
 */
extern DECLSPEC void SDLCALL SDL_GpuQueueDestroyGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuGraphicsPipeline *graphicsPipeline
);

/* Graphics State */

/* Begins a render pass.
 * This will also set a default viewport and scissor state.
 *
 * colorAttachmentInfos:
 * 		A pointer to an array of SDL_GpuColorAttachmentInfo structures
 * 		that contains render targets and clear values. May be NULL.
 * colorAttachmentCount: The amount of structs in the above array.
 * depthStencilAttachmentInfo: The depth/stencil render target and clear value. May be NULL.
 */
extern DECLSPEC void SDLCALL SDL_GpuBeginRenderPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
	Uint32 colorAttachmentCount,
	SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo
);

/* Binds a graphics pipeline to the graphics bind point. */
extern DECLSPEC void SDLCALL SDL_GpuBindGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuGraphicsPipeline *graphicsPipeline
);

/* Sets the current viewport state. */
extern DECLSPEC void SDLCALL SDL_GpuSetViewport(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuViewport *viewport
);

/* Sets the current scissor state. */
extern DECLSPEC void SDLCALL SDL_GpuSetScissor(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuRect *scissor
);

/* Binds vertex buffers for use with subsequent draw calls.
 * Note that this may only be called after binding a graphics pipeline.
 */
extern DECLSPEC void SDLCALL SDL_GpuBindVertexBuffers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 firstBinding,
	Uint32 bindingCount,
	SDL_GpuBufferBinding *pBindings
);

/* Binds an index buffer for use with subsequent draw calls. */
extern DECLSPEC void SDLCALL SDL_GpuBindIndexBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBufferBinding *pBinding,
	SDL_GpuIndexElementSize indexElementSize
);

/* Sets textures/samplers for use with the currently bound vertex shader.
 *
 * NOTE:
 * 		The length of the bindings array must be equal to the number
 * 		of sampler bindings specified by the pipeline.
 *
 * pBindings:  A pointer to an array of TextureSamplerBindings.
 */
extern DECLSPEC void SDLCALL SDL_GpuBindVertexSamplers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
);

/* Sets textures/samplers for use with the currently bound fragment shader.
 *
 * NOTE:
 *		The length of the bindings array must be equal to the number
 * 		of sampler bindings specified by the pipeline.
 *
 * pBindings:  A pointer to an array of TextureSamplerBindings.
 */
extern DECLSPEC void SDLCALL SDL_GpuBindFragmentSamplers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
);

/* Pushes vertex shader uniforms to the device.
 * This uniform data will be used with subsequent draw calls.
 *
 * NOTE:
 * 		A graphics pipeline must be bound.
 * 		Will use the block size of the currently bound vertex shader.
 *
 * data: 				The client data to write into the buffer.
 * dataLengthInBytes: 	The length of the data to write.
 */
extern DECLSPEC void SDLCALL SDL_GpuPushVertexShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer * commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
);

/* Pushes fragment shader params to the device.
 * This uniform data will be used with subsequent draw calls.
 *
 * NOTE:
 * 		A graphics pipeline must be bound.
 * 		Will use the block size of the currently bound fragment shader.
 *
 * data: 				The client data to write into the buffer.
 * dataLengthInBytes: 	The length of the data to write.
 */
extern DECLSPEC void SDLCALL SDL_GpuPushFragmentShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
);

/* Drawing */

/* Draws data from vertex/index buffers with instancing enabled.
 *
 * baseVertex:          The starting offset to read from the vertex buffer.
 * startIndex:          The starting offset to read from the index buffer.
 * primitiveCount:      The number of primitives to draw.
 * instanceCount:       The number of instances that will be drawn.
 */
extern DECLSPEC void SDLCALL SDL_GpuDrawInstancedPrimitives(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 baseVertex,
	Uint32 startIndex,
	Uint32 primitiveCount,
	Uint32 instanceCount
);

/* Draws data from vertex buffers.
 *
 * vertexStart:			The starting offset to read from the vertex buffer.
 * primitiveCount:		The number of primitives to draw.
 */
extern DECLSPEC void SDLCALL SDL_GpuDrawPrimitives(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 vertexStart,
	Uint32 primitiveCount
);

/* Similar to SDL_GpuDrawPrimitives, but draw parameters are set from a buffer.
 * The buffer layout should match the layout of SDL_GpuIndirectDrawCommand.
 *
 * buffer:              A buffer containing draw parameters.
 * offsetInBytes:       The offset to start reading from the draw buffer.
 * drawCount:           The number of draw parameter sets that should be read from the draw buffer.
 * stride:              The byte stride between sets of draw parameters.
 */
extern DECLSPEC void SDLCALL SDL_GpuDrawPrimitivesIndirect(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *gpuBuffer,
	Uint32 offsetInBytes,
	Uint32 drawCount,
	Uint32 stride
);

/* Ends the current render pass. */
extern DECLSPEC void SDLCALL SDL_GpuEndRenderPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* Compute Pass */

/* Begins a compute pass. */
extern DECLSPEC void SDLCALL SDL_GpuBeginComputePass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* Binds a compute pipeline to the compute bind point. */
extern DECLSPEC void SDLCALL SDL_GpuBindComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputePipeline *computePipeline
);

/* Binds buffers for use with the currently bound compute pipeline.
 *
 * pBindings:
 *   An array of ComputeBufferBinding structs.
 *   Length must be equal to the number of buffers
 *   specified by the compute pipeline.
 */
extern DECLSPEC void SDLCALL SDL_GpuBindComputeBuffers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeBufferBinding *pBindings
);

/* Binds textures for use with the currently bound compute pipeline.
 *
 * pBindings:
 *   An array of ComputeTextureBinding structs.
 *   Length must be equal to the number of textures
 *   specified by the compute pipeline.
 */
extern DECLSPEC void SDLCALL SDL_GpuBindComputeTextures(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeTextureBinding *pBindings
);

/* Pushes compute shader params to the device.
 * This uniform data will be used with subsequent dispatch calls.
 *
 * NOTE:
 * 	A compute pipeline must be bound.
 * 	Will use the block size of the currently bound compute shader.
 *
 * data:				The client data to write into the buffer.
 * dataLengthInBytes:	The length of the data to write.
 */
extern DECLSPEC void SDLCALL SDL_GpuPushComputeShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer * commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
);

/* Dispatches work compute items.
 *
 * groupCountX:			Number of local workgroups to dispatch in the X dimension.
 * groupCountY:			Number of local workgroups to dispatch in the Y dimension.
 * groupCountZ:			Number of local workgroups to dispatch in the Z dimension.
 */
extern DECLSPEC void SDLCALL SDL_GpuDispatchCompute(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 groupCountX,
	Uint32 groupCountY,
	Uint32 groupCountZ
);

/* Ends the current compute pass. */
extern DECLSPEC void SDLCALL SDL_GpuEndComputePass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* TransferBuffer Set/Get */

/* Immediately copies data from a pointer into a TransferBuffer.
 *
 * transferOption:
 *  CYCLE:
 *    If this TransferBuffer has been used in commands that have not completed,
 *    the issued commands will still be valid at the cost of increased memory usage.
 *    You may NOT assume that any of the previous data is retained.
 *    If the TransferBuffer was not in use, this option is equivalent to UNSAFE.
 *    This may prevent stalls when frequently updating data.
 *    It is not recommended to use this option with large TransferBuffers.
 *
 *  UNSAFE:
 *    Overwrites the data regardless of whether a command has been issued.
 *    Use this option with great care, as it can cause data races to occur!
 */
extern DECLSPEC void SDLCALL SDL_GpuSetTransferData(
	SDL_GpuDevice *device,
	void* data,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
);

/* Immediately copies data from a TransferBuffer into a pointer. */
extern DECLSPEC void SDLCALL SDL_GpuGetTransferData(
	SDL_GpuDevice *device,
	SDL_GpuTransferBuffer *transferBuffer,
	void* data,
	SDL_GpuBufferCopy *copyParams
);

/* Copy Pass */

/* Begins a copy pass. */
extern DECLSPEC void SDLCALL SDL_GpuBeginCopyPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* CPU-to-GPU copies occur on the GPU timeline.
 *
 * You MAY assume that the copy has finished for subsequent commands.
 */

/*
 * writeOption:
 *  CYCLE:
 *    If the destination resource has been used in commands that have not completed,
 *    the implementation may choose to prevent a data dependency at the cost of increased memory usage.
 *    You may NOT assume that any of the previous data is retained.
 *    This may prevent stalls on resources with frequent updates.
 *    It is not recommended to use this option with large resources.
 *
 *  UNSAFE:
 *    Overwrites the data unsafely. You must ensure that data used by earlier draw calls
 *    is not affected or visual corruption can occur.
 *
 *  SAFE:
 *    Overwrites the data safely. Earlier draw calls will not be affected.
 *    This is usually the slowest option.
 */

extern DECLSPEC void SDLCALL SDL_GpuUploadToTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTextureWriteOptions writeOption
);

/* Uploads data from a TransferBuffer to a GpuBuffer. */
extern DECLSPEC void SDLCALL SDL_GpuUploadToBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions uploadOption
);

/* GPU-to-GPU copies occur on the GPU timeline,
 * and you may assume the copy has finished in subsequent commands.
 */

/*
 * writeOption:
 *  CYCLE:
 *    If the destination resource has been used in commands that have not completed,
 *    the implementation may choose to prevent a data dependency at the cost of increased memory usage.
 *    You may NOT assume that any of the previous data is retained.
 *    This may prevent stalls on resources with frequent updates.
 *    It is not recommended to use this option with large resources.
 *
 *  UNSAFE:
 *    Overwrites the data unsafely. You must ensure that data used by earlier draw calls
 *    is not affected or visual corruption can occur.
 *
 *  SAFE:
 *    Overwrites the data safely. Earlier draw calls will not be affected.
 *    This is usually the slowest option.
 */

/* Performs a texture-to-texture copy. */
extern DECLSPEC void SDLCALL SDL_GpuCopyTextureToTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureRegion *source,
	SDL_GpuTextureRegion *destination,
	SDL_GpuTextureWriteOptions writeOption
);

/* Copies data from a buffer to a buffer. */
extern DECLSPEC void SDLCALL SDL_GpuCopyBufferToBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *source,
	SDL_GpuBuffer *destination,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
);

/* Generate mipmaps for the given texture. */
extern DECLSPEC void SDLCALL SDL_GpuGenerateMipmaps(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTexture *texture
);

extern DECLSPEC void SDLCALL SDL_GpuEndCopyPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* Ends a copy pass. */

/* Submission/Presentation */

/* Claims a window, creating a swapchain structure for it.
 * This function MUST be called before any swapchain functions
 * are called using the window.
 *
 * Returns 0 on swapchain creation failure.
 */
extern DECLSPEC Uint8 SDLCALL SDL_GpuClaimWindow(
	SDL_GpuDevice *device,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
);

/* Unclaims a window, destroying the swapchain structure for it.
 * It is good practice to call this when a window is closed to
 * prevent memory bloat, but windows are automatically unclaimed
 * by DestroyDevice.
 */
extern DECLSPEC void SDLCALL SDL_GpuUnclaimWindow(
	SDL_GpuDevice *device,
	void *windowHandle
);

/* Changes the present mode of the swapchain for the given window. */
extern DECLSPEC void SDLCALL SDL_GpuSetSwapchainPresentMode(
	SDL_GpuDevice *device,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
);

/* Returns the format of the swapchain for the given window. */
extern DECLSPEC SDL_GpuTextureFormat SDLCALL SDL_GpuGetSwapchainFormat(
	SDL_GpuDevice *device,
	void *windowHandle
);

/* Returns an allocated SDL_GpuCommandBuffer* object.
 * This command buffer is managed by the implementation and
 * should NOT be freed by the user.
 *
 * NOTE:
 * 	A command buffer may only be used on the thread that
 * 	it was acquired on. Using it on any other thread is an error.
 *
 */
extern DECLSPEC SDL_GpuCommandBuffer *SDLCALL SDL_GpuAcquireCommandBuffer(
	SDL_GpuDevice *device
);

/* Acquires a texture to use for presentation.
 * May return NULL under certain conditions.
 * If NULL, the user must ensure to not use the texture.
 * Once a swapchain texture is acquired,
 * it will automatically be presented on command buffer submission.
 *
 * NOTE:
 * 	It is not recommended to hold a reference to this texture long term.
 *
 * pWidth: A pointer to a uint32 that will be filled with the texture width.
 * pHeight: A pointer to a uint32 that will be filled with the texture height.
 */
extern DECLSPEC SDL_GpuTexture *SDLCALL SDL_GpuAcquireSwapchainTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *windowHandle,
	Uint32 *pWidth,
	Uint32 *pHeight
);

/* Submits all of the enqueued commands. */
extern DECLSPEC void SDLCALL SDL_GpuSubmit(
	SDL_GpuDevice* device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* Submits a command buffer and acquires a fence.
 * You can use the fence to check if or wait until the command buffer has finished processing.
 * You are responsible for releasing this fence when you are done using it.
 */
extern DECLSPEC SDL_GpuFence *SDLCALL SDL_GpuSubmitAndAcquireFence(
	SDL_GpuDevice* device,
	SDL_GpuCommandBuffer *commandBuffer
);

/* Waits for the device to become idle. */
extern DECLSPEC void SDLCALL SDL_GpuWait(
	SDL_GpuDevice *device
);

/* Waits for given fences to be signaled.
 *
 * waitAll: If 0, waits for any fence to be signaled. If 1, waits for all fences to be signaled.
 * fenceCount: The number of fences being submitted.
 * pFences: An array of fences to be waited on.
 */
extern DECLSPEC void SDLCALL SDL_GpuWaitForFences(
	SDL_GpuDevice *device,
	Uint8 waitAll,
	Uint32 fenceCount,
	SDL_GpuFence **pFences
);

/* Check the status of a fence. 1 means the fence is signaled. */
extern DECLSPEC int SDLCALL SDL_GpuQueryFence(
	SDL_GpuDevice *device,
	SDL_GpuFence *fence
);

/* Allows the fence to be reused by future command buffer submissions.
 * If you do not release fences after acquiring them, you will cause unbounded resource growth.
 */
extern DECLSPEC void SDLCALL SDL_GpuReleaseFence(
	SDL_GpuDevice *device,
	SDL_GpuFence *fence
);

/* Readback */

/* GPU-to-CPU copies occur immediately on the CPU timeline.
 *
 * If you modify data on the GPU and then call these functions without calling Wait or WaitForFences first,
 * the data will be undefined!
 *
 * Readback forces a sync point and is generally a bad thing to do.
 * Only use these functions if you have exhausted all other options.
 */

/*
 * transferOption:
 *  CYCLE:
 *    If this TransferBuffer has been used in commands that have not completed,
 *    the issued commands will still be valid at the cost of increased memory usage.
 *    You may NOT assume that any of the previous data is retained.
 *    If the TransferBuffer was not in use, this option is equivalent to UNSAFE.
 *    It is not recommended to use this option with large TransferBuffers.
 *
 *  UNSAFE:
 *    Overwrites the data regardless of whether a command has been issued.
 *    Use this option with great care, as it can cause data races to occur!
 */

/* Downloads data from a texture to a TransferBuffer. */
extern DECLSPEC void SDLCALL SDL_GpuDownloadFromTexture(
	SDL_GpuDevice *device,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTransferOptions transferOption
);

/* Downloads data from a GpuBuffer object. */
extern DECLSPEC void SDLCALL SDL_GpuDownloadFromBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SDL_GPU_H */
