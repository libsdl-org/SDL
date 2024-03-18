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
#include "../video/SDL_sysvideo.h"

#ifndef SDL_GPU_DRIVER_H
#define SDL_GPU_DRIVER_H

/* Common Struct */

typedef struct Pass
{
    SDL_GpuCommandBuffer *commandBuffer;
    SDL_bool inProgress;
} Pass;

typedef struct CommandBufferCommonHeader
{
    SDL_GpuDevice *device;
    Pass renderPass;
    SDL_bool graphicsPipelineBound;
    Pass computePass;
    SDL_bool computePipelineBound;
    Pass copyPass;
    SDL_bool submitted;
} CommandBufferCommonHeader;

/* Internal Helper Utilities */

static inline Sint32 Texture_GetBlockSize(
	SDL_GpuTextureFormat format
) {
	switch (format)
	{
		case SDL_GPU_TEXTUREFORMAT_BC1:
		case SDL_GPU_TEXTUREFORMAT_BC2:
		case SDL_GPU_TEXTUREFORMAT_BC3:
		case SDL_GPU_TEXTUREFORMAT_BC7:
		case SDL_GPU_TEXTUREFORMAT_BC3_SRGB:
		case SDL_GPU_TEXTUREFORMAT_BC7_SRGB:
			return 4;
		case SDL_GPU_TEXTUREFORMAT_R8:
        case SDL_GPU_TEXTUREFORMAT_A8:
		case SDL_GPU_TEXTUREFORMAT_R8_UINT:
		case SDL_GPU_TEXTUREFORMAT_R5G6B5:
		case SDL_GPU_TEXTUREFORMAT_B4G4R4A4:
		case SDL_GPU_TEXTUREFORMAT_A1R5G5B5:
		case SDL_GPU_TEXTUREFORMAT_R16_SFLOAT:
		case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
		case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
		case SDL_GPU_TEXTUREFORMAT_R16_UINT:
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8:
		case SDL_GPU_TEXTUREFORMAT_R32_SFLOAT:
		case SDL_GPU_TEXTUREFORMAT_R16G16_SFLOAT:
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
        case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SRGB:
        case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_SRGB:
		case SDL_GPU_TEXTUREFORMAT_A2R10G10B10:
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
		case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT:
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16:
		case SDL_GPU_TEXTUREFORMAT_R32G32_SFLOAT:
		case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
		case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT:
			return 1;
		default:
			SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
				"Unrecognized TextureFormat!"
			);
			return 0;
	}
}

static inline SDL_bool IsDepthFormat(
    SDL_GpuTextureFormat format
) {
    switch (format)
    {
        case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
        case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
        case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT:
        case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT:
            return SDL_TRUE;

        default:
            return SDL_FALSE;
    }
}

static inline SDL_bool IsStencilFormat(
    SDL_GpuTextureFormat format
) {
    switch (format)
    {
        case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT:
            return SDL_TRUE;

        default:
            return SDL_FALSE;
    }
}

static inline Uint32 PrimitiveVerts(
	SDL_GpuPrimitiveType primitiveType,
	Uint32 primitiveCount
) {
	switch (primitiveType)
	{
		case SDL_GPU_PRIMITIVETYPE_TRIANGLELIST:
			return primitiveCount * 3;
		case SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP:
			return primitiveCount + 2;
		case SDL_GPU_PRIMITIVETYPE_LINELIST:
			return primitiveCount * 2;
		case SDL_GPU_PRIMITIVETYPE_LINESTRIP:
			return primitiveCount + 1;
		case SDL_GPU_PRIMITIVETYPE_POINTLIST:
			return primitiveCount;
		default:
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
				"Unrecognized primitive type!"
			);
			return 0;
	}
}

static inline Uint32 IndexSize(SDL_GpuIndexElementSize size)
{
	return (size == SDL_GPU_INDEXELEMENTSIZE_16BIT) ? 2 : 4;
}

static inline Uint32 BytesPerRow(
	Sint32 width,
	SDL_GpuTextureFormat format
) {
	Uint32 blocksPerRow = width;

	if (	format == SDL_GPU_TEXTUREFORMAT_BC1 ||
		format == SDL_GPU_TEXTUREFORMAT_BC2 ||
		format == SDL_GPU_TEXTUREFORMAT_BC3 ||
		format == SDL_GPU_TEXTUREFORMAT_BC7	)
	{
		blocksPerRow = (width + 3) / 4;
	}

	return blocksPerRow * SDL_GpuTextureFormatTexelBlockSize(format);
}

static inline Sint32 BytesPerImage(
	Uint32 width,
	Uint32 height,
	SDL_GpuTextureFormat format
) {
	Uint32 blocksPerRow = width;
	Uint32 blocksPerColumn = height;

	if (	format == SDL_GPU_TEXTUREFORMAT_BC1 ||
		format == SDL_GPU_TEXTUREFORMAT_BC2 ||
		format == SDL_GPU_TEXTUREFORMAT_BC3 ||
		format == SDL_GPU_TEXTUREFORMAT_BC7 )
	{
		blocksPerRow = (width + 3) / 4;
		blocksPerColumn = (height + 3) / 4;
	}

	return blocksPerRow * blocksPerColumn * SDL_GpuTextureFormatTexelBlockSize(format);
}

/* GraphicsDevice Limits */

#define MAX_TEXTURE_SAMPLERS_PER_STAGE  16
#define MAX_STORAGE_TEXTURES_PER_STAGE  8
#define MAX_STORAGE_BUFFERS_PER_STAGE   8
#define MAX_UNIFORM_BUFFERS_PER_STAGE   14
#define MAX_BUFFER_BINDINGS			    16
#define MAX_COLOR_TARGET_BINDINGS	    4
#define MAX_PRESENT_COUNT               16
#define MAX_FRAMES_IN_FLIGHT            3

/* SDL_GpuDevice Definition */

typedef struct SDL_GpuRenderer SDL_GpuRenderer;

struct SDL_GpuDevice
{
	/* Quit */

	void (*DestroyDevice)(SDL_GpuDevice *device);

	/* State Creation */

	SDL_GpuComputePipeline* (*CreateComputePipeline)(
		SDL_GpuRenderer *driverData,
		SDL_GpuComputePipelineCreateInfo *pipelineCreateInfo
	);

	SDL_GpuGraphicsPipeline* (*CreateGraphicsPipeline)(
		SDL_GpuRenderer *driverData,
		SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo
	);

	SDL_GpuSampler* (*CreateSampler)(
		SDL_GpuRenderer *driverData,
		SDL_GpuSamplerStateCreateInfo *samplerStateCreateInfo
	);

	SDL_GpuShader* (*CreateShader)(
		SDL_GpuRenderer *driverData,
		SDL_GpuShaderCreateInfo *shaderCreateInfo
	);

	SDL_GpuTexture* (*CreateTexture)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTextureCreateInfo *textureCreateInfo
	);

	SDL_GpuBuffer* (*CreateGpuBuffer)(
		SDL_GpuRenderer *driverData,
		SDL_GpuBufferUsageFlags usageFlags,
		Uint32 sizeInBytes
	);

	SDL_GpuTransferBuffer* (*CreateTransferBuffer)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTransferUsage usage,
        SDL_GpuTransferBufferMapFlags mapFlags,
		Uint32 sizeInBytes
	);

    SDL_GpuOcclusionQuery* (*CreateOcclusionQuery)(
        SDL_GpuRenderer *driverData
    );

	/* Debug Naming */

	void (*SetGpuBufferName)(
		SDL_GpuRenderer *driverData,
		SDL_GpuBuffer *buffer,
		const char *text
	);

	void (*SetTextureName)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTexture *texture,
		const char *text
	);

    void (*SetStringMarker)(
        SDL_GpuCommandBuffer *commandBuffer,
        const char *text
    );

	/* Disposal */

	void (*QueueDestroyTexture)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTexture *texture
	);

	void (*QueueDestroySampler)(
		SDL_GpuRenderer *driverData,
		SDL_GpuSampler *sampler
	);

	void (*QueueDestroyGpuBuffer)(
		SDL_GpuRenderer *driverData,
		SDL_GpuBuffer *gpuBuffer
	);

	void (*QueueDestroyTransferBuffer)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTransferBuffer *transferBuffer
	);

	void (*QueueDestroyShader)(
		SDL_GpuRenderer *driverData,
		SDL_GpuShader *shader
	);

	void (*QueueDestroyComputePipeline)(
		SDL_GpuRenderer *driverData,
		SDL_GpuComputePipeline *computePipeline
	);

	void (*QueueDestroyGraphicsPipeline)(
		SDL_GpuRenderer *driverData,
		SDL_GpuGraphicsPipeline *graphicsPipeline
	);

    void (*QueueDestroyOcclusionQuery)(
        SDL_GpuRenderer *driverData,
        SDL_GpuOcclusionQuery *query
    );

	/* Render Pass */

	void (*BeginRenderPass)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
		Uint32 colorAttachmentCount,
		SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo
	);

	void (*BindGraphicsPipeline)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuGraphicsPipeline *graphicsPipeline
	);

	void (*SetViewport)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuViewport *viewport
	);

	void (*SetScissor)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuRect *scissor
	);

	void (*BindVertexBuffers)(
		SDL_GpuCommandBuffer *commandBuffer,
		Uint32 firstBinding,
		Uint32 bindingCount,
		SDL_GpuBufferBinding *pBindings
	);

	void (*BindIndexBuffer)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuBufferBinding *pBinding,
		SDL_GpuIndexElementSize indexElementSize
	);

    void (*BindVertexSamplers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount
    );

    void (*BindVertexStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount
    );

    void (*BindVertexStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount
    );

    void (*BindFragmentSamplers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount
    );

    void (*BindFragmentStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount
    );

    void (*BindFragmentStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount
    );

	void (*PushVertexUniformData)(
		SDL_GpuCommandBuffer *commandBuffer,
        Uint32 slotIndex,
		void *data,
		Uint32 dataLengthInBytes
	);

    void (*PushFragmentUniformData)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        void *data,
        Uint32 dataLengthInBytes
    );

	void (*DrawInstancedPrimitives)(
		SDL_GpuCommandBuffer *commandBuffer,
		Uint32 baseVertex,
		Uint32 startIndex,
		Uint32 primitiveCount,
		Uint32 instanceCount
	);

	void (*DrawPrimitives)(
		SDL_GpuCommandBuffer *commandBuffer,
		Uint32 vertexStart,
		Uint32 primitiveCount
	);

	void (*DrawPrimitivesIndirect)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuBuffer *gpuBuffer,
		Uint32 offsetInBytes,
		Uint32 drawCount,
		Uint32 stride
	);

	void (*EndRenderPass)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	/* Compute Pass */

	void (*BeginComputePass)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	void (*BindComputePipeline)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuComputePipeline *computePipeline
	);

    void (*BindComputeStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount
    );

    void (*BindComputeRWStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuStorageTextureReadWriteBinding *storageTextureBindings,
        Uint32 bindingCount
    );

    void (*BindComputeStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount
    );

    void (*BindComputeRWStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuStorageBufferReadWriteBinding *storageBufferBindings,
        Uint32 bindingCount
    );

	void (*PushComputeUniformData)(
		SDL_GpuCommandBuffer *commandBuffer,
		Uint32 slotIndex,
		void *data,
		Uint32 dataLengthInBytes
	);

	void (*DispatchCompute)(
		SDL_GpuCommandBuffer *commandBuffer,
		Uint32 groupCountX,
		Uint32 groupCountY,
		Uint32 groupCountZ
	);

	void (*EndComputePass)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	/* TransferBuffer Data */

    void (*MapTransferBuffer)(
        SDL_GpuRenderer *device,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_bool cycle,
        void **ppData
    );

    void (*UnmapTransferBuffer)(
        SDL_GpuRenderer *device,
        SDL_GpuTransferBuffer *transferBuffer
    );

	void (*SetTransferData)(
		SDL_GpuRenderer *driverData,
		void* data,
		SDL_GpuTransferBuffer *transferBuffer,
		SDL_GpuBufferCopy *copyParams,
		SDL_bool cycle
	);

	void (*GetTransferData)(
		SDL_GpuRenderer *driverData,
		SDL_GpuTransferBuffer *transferBuffer,
		void* data,
		SDL_GpuBufferCopy *copyParams
	);

	/* Copy Pass */

	void (*BeginCopyPass)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	void (*UploadToTexture)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuTransferBuffer *transferBuffer,
		SDL_GpuTextureRegion *textureSlice,
		SDL_GpuBufferImageCopy *copyParams,
		SDL_bool cycle
	);

	void (*UploadToBuffer)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuTransferBuffer *transferBuffer,
		SDL_GpuBuffer *gpuBuffer,
		SDL_GpuBufferCopy *copyParams,
		SDL_bool cycle
	);

	void (*CopyTextureToTexture)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuTextureRegion *source,
		SDL_GpuTextureRegion *destination,
		SDL_bool cycle
	);

	void (*CopyBufferToBuffer)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuBuffer *source,
		SDL_GpuBuffer *destination,
		SDL_GpuBufferCopy *copyParams,
		SDL_bool cycle
	);

	void (*GenerateMipmaps)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuTexture *texture
	);

	void (*DownloadFromTexture)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuTextureRegion *textureSlice,
		SDL_GpuTransferBuffer *transferBuffer,
		SDL_GpuBufferImageCopy *copyParams
	);

	void (*DownloadFromBuffer)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_GpuBuffer *gpuBuffer,
		SDL_GpuTransferBuffer *transferBuffer,
		SDL_GpuBufferCopy *copyParams
	);

	void (*EndCopyPass)(
		SDL_GpuCommandBuffer *commandBuffer
	);

    void (*Blit)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTextureRegion *source,
        SDL_GpuTextureRegion *destination,
        SDL_GpuFilter filterMode,
		SDL_bool cycle
    );

	/* Submission/Presentation */

	SDL_bool (*ClaimWindow)(
		SDL_GpuRenderer *driverData,
		SDL_Window *windowHandle,
        SDL_GpuColorSpace colorSpace,
        SDL_bool preferVerticalSync
	);

	void (*UnclaimWindow)(
		SDL_GpuRenderer *driverData,
		SDL_Window *windowHandle
	);

	void (*SetSwapchainParameters)(
		SDL_GpuRenderer *driverData,
		SDL_Window *windowHandle,
        SDL_GpuColorSpace colorSpace,
        SDL_bool preferVerticalSync
	);

	SDL_GpuTextureFormat (*GetSwapchainFormat)(
		SDL_GpuRenderer *driverData,
		SDL_Window *windowHandle
	);

	SDL_GpuCommandBuffer* (*AcquireCommandBuffer)(
		SDL_GpuRenderer *driverData
	);

	SDL_GpuTexture* (*AcquireSwapchainTexture)(
		SDL_GpuCommandBuffer *commandBuffer,
		SDL_Window *windowHandle,
		Uint32 *pWidth,
		Uint32 *pHeight
	);

	void (*Submit)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	SDL_GpuFence* (*SubmitAndAcquireFence)(
		SDL_GpuCommandBuffer *commandBuffer
	);

	void (*Wait)(
		SDL_GpuRenderer *driverData
	);

	void (*WaitForFences)(
		SDL_GpuRenderer *driverData,
		Uint8 waitAll,
		Uint32 fenceCount,
		SDL_GpuFence **pFences
	);

	SDL_bool (*QueryFence)(
		SDL_GpuRenderer *driverData,
		SDL_GpuFence *fence
	);

	void (*ReleaseFence)(
		SDL_GpuRenderer *driverData,
		SDL_GpuFence *fence
	);

    /* Queries */

    void (*OcclusionQueryBegin)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuOcclusionQuery *query
    );

    void (*OcclusionQueryEnd)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuOcclusionQuery *query
    );

    SDL_bool (*OcclusionQueryPixelCount)(
        SDL_GpuRenderer *driverData,
        SDL_GpuOcclusionQuery *query,
        Uint32 *pixelCount
    );

    /* Feature Queries */

    SDL_bool (*IsTextureFormatSupported)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTextureFormat format,
        SDL_GpuTextureType type,
        SDL_GpuTextureUsageFlags usage
    );

    SDL_GpuSampleCount (*GetBestSampleCount)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTextureFormat format,
        SDL_GpuSampleCount desiredSampleCount
    );

    /* SPIR-V Cross Interop */

    SDL_GpuShader* (*CompileFromSPIRVCross)(
        SDL_GpuRenderer *driverData,
        SDL_GpuShaderStageFlagBits shader_stage,
        const char *entryPointName,
        const char *source
    );

	/* Opaque pointer for the Driver */
	SDL_GpuRenderer *driverData;

	/* Store this for SDL_GpuGetBackend() */
	SDL_GpuBackend backend;
};

#define ASSIGN_DRIVER_FUNC(func, name) \
	result->func = name##_##func;
#define ASSIGN_DRIVER(name) \
	ASSIGN_DRIVER_FUNC(DestroyDevice, name) \
	ASSIGN_DRIVER_FUNC(CreateComputePipeline, name) \
	ASSIGN_DRIVER_FUNC(CreateGraphicsPipeline, name) \
	ASSIGN_DRIVER_FUNC(CreateSampler, name) \
	ASSIGN_DRIVER_FUNC(CreateShader, name) \
	ASSIGN_DRIVER_FUNC(CreateTexture, name) \
	ASSIGN_DRIVER_FUNC(CreateGpuBuffer, name) \
	ASSIGN_DRIVER_FUNC(CreateTransferBuffer, name) \
    ASSIGN_DRIVER_FUNC(CreateOcclusionQuery, name) \
	ASSIGN_DRIVER_FUNC(SetGpuBufferName, name) \
	ASSIGN_DRIVER_FUNC(SetTextureName, name) \
    ASSIGN_DRIVER_FUNC(SetStringMarker, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyTexture, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroySampler, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyGpuBuffer, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyTransferBuffer, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyShader, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyComputePipeline, name) \
	ASSIGN_DRIVER_FUNC(QueueDestroyGraphicsPipeline, name) \
    ASSIGN_DRIVER_FUNC(QueueDestroyOcclusionQuery, name) \
	ASSIGN_DRIVER_FUNC(BeginRenderPass, name) \
	ASSIGN_DRIVER_FUNC(BindGraphicsPipeline, name) \
	ASSIGN_DRIVER_FUNC(SetViewport, name) \
	ASSIGN_DRIVER_FUNC(SetScissor, name) \
	ASSIGN_DRIVER_FUNC(BindVertexBuffers, name) \
	ASSIGN_DRIVER_FUNC(BindIndexBuffer, name) \
    ASSIGN_DRIVER_FUNC(BindVertexSamplers, name) \
    ASSIGN_DRIVER_FUNC(BindVertexStorageTextures, name) \
    ASSIGN_DRIVER_FUNC(BindVertexStorageBuffers, name) \
    ASSIGN_DRIVER_FUNC(BindFragmentSamplers, name) \
    ASSIGN_DRIVER_FUNC(BindFragmentStorageTextures, name) \
    ASSIGN_DRIVER_FUNC(BindFragmentStorageBuffers, name) \
	ASSIGN_DRIVER_FUNC(PushVertexUniformData, name) \
    ASSIGN_DRIVER_FUNC(PushFragmentUniformData, name) \
	ASSIGN_DRIVER_FUNC(DrawInstancedPrimitives, name) \
	ASSIGN_DRIVER_FUNC(DrawPrimitives, name) \
	ASSIGN_DRIVER_FUNC(DrawPrimitivesIndirect, name) \
	ASSIGN_DRIVER_FUNC(EndRenderPass, name) \
	ASSIGN_DRIVER_FUNC(BeginComputePass, name) \
    ASSIGN_DRIVER_FUNC(BindComputePipeline, name) \
    ASSIGN_DRIVER_FUNC(BindComputeStorageTextures, name) \
    ASSIGN_DRIVER_FUNC(BindComputeRWStorageTextures, name) \
    ASSIGN_DRIVER_FUNC(BindComputeStorageBuffers, name) \
    ASSIGN_DRIVER_FUNC(BindComputeRWStorageBuffers, name) \
	ASSIGN_DRIVER_FUNC(PushComputeUniformData, name) \
	ASSIGN_DRIVER_FUNC(DispatchCompute, name) \
	ASSIGN_DRIVER_FUNC(EndComputePass, name) \
    ASSIGN_DRIVER_FUNC(MapTransferBuffer, name) \
    ASSIGN_DRIVER_FUNC(UnmapTransferBuffer, name) \
	ASSIGN_DRIVER_FUNC(SetTransferData, name) \
	ASSIGN_DRIVER_FUNC(GetTransferData, name) \
	ASSIGN_DRIVER_FUNC(BeginCopyPass, name) \
	ASSIGN_DRIVER_FUNC(UploadToTexture, name) \
	ASSIGN_DRIVER_FUNC(UploadToBuffer, name) \
	ASSIGN_DRIVER_FUNC(DownloadFromTexture, name) \
	ASSIGN_DRIVER_FUNC(DownloadFromBuffer, name) \
	ASSIGN_DRIVER_FUNC(CopyTextureToTexture, name) \
	ASSIGN_DRIVER_FUNC(CopyBufferToBuffer, name) \
	ASSIGN_DRIVER_FUNC(GenerateMipmaps, name) \
	ASSIGN_DRIVER_FUNC(EndCopyPass, name) \
    ASSIGN_DRIVER_FUNC(Blit, name) \
	ASSIGN_DRIVER_FUNC(ClaimWindow, name) \
	ASSIGN_DRIVER_FUNC(UnclaimWindow, name) \
	ASSIGN_DRIVER_FUNC(SetSwapchainParameters, name) \
	ASSIGN_DRIVER_FUNC(GetSwapchainFormat, name) \
	ASSIGN_DRIVER_FUNC(AcquireCommandBuffer, name) \
	ASSIGN_DRIVER_FUNC(AcquireSwapchainTexture, name) \
	ASSIGN_DRIVER_FUNC(Submit, name) \
	ASSIGN_DRIVER_FUNC(SubmitAndAcquireFence, name) \
	ASSIGN_DRIVER_FUNC(Wait, name) \
	ASSIGN_DRIVER_FUNC(WaitForFences, name) \
	ASSIGN_DRIVER_FUNC(QueryFence, name) \
	ASSIGN_DRIVER_FUNC(ReleaseFence, name) \
    ASSIGN_DRIVER_FUNC(OcclusionQueryBegin, name) \
    ASSIGN_DRIVER_FUNC(OcclusionQueryEnd, name) \
    ASSIGN_DRIVER_FUNC(OcclusionQueryPixelCount, name) \
    ASSIGN_DRIVER_FUNC(IsTextureFormatSupported, name) \
    ASSIGN_DRIVER_FUNC(GetBestSampleCount, name) \
	ASSIGN_DRIVER_FUNC(CompileFromSPIRVCross, name)

typedef struct SDL_GpuDriver
{
	const char *Name;
	const SDL_GpuBackend backendflag;
	Uint8 (*PrepareDriver)(SDL_VideoDevice *_this);
	SDL_GpuDevice* (*CreateDevice)(SDL_bool debugMode);
} SDL_GpuDriver;

extern SDL_GpuDriver VulkanDriver;
extern SDL_GpuDriver D3D11Driver;
extern SDL_GpuDriver MetalDriver;
extern SDL_GpuDriver PS5Driver;

#endif /* SDL_GPU_DRIVER_H */
